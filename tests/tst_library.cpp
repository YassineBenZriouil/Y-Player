// Recent files, resume points and playback adjustments (speed, delays, A-B, aspect parsing).

#include "core/Adjustments.h"
#include "core/RecentFiles.h"
#include "core/ResumeStore.h"

#include <QTest>

#include <limits>

class TestLibrary : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void recentFilesMostRecentFirstNoDuplicates() {
        yp::RecentFiles r(3);
        r.add(QStringLiteral("C:/a.mkv"));
        r.add(QStringLiteral("C:\\b.mkv"));
        r.add(QStringLiteral("c:\\A.MKV"));  // same file as the first, different case/separators
        QCOMPARE(r.items(), (QStringList{QStringLiteral("c:\\A.MKV"), QStringLiteral("C:\\b.mkv")}));
        r.add(QStringLiteral("C:\\c.mkv"));
        r.add(QStringLiteral("C:\\d.mkv"));
        QCOMPARE(r.items().size(), 3);  // capped
        QCOMPARE(r.items().first(), QStringLiteral("C:\\d.mkv"));
        r.remove(QStringLiteral("c:/D.mkv"));
        QCOMPARE(r.items().first(), QStringLiteral("C:\\c.mkv"));
        r.setItems({QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("x")});
        QCOMPARE(r.items(), (QStringList{QStringLiteral("x"), QStringLiteral("y")}));
    }

    void resumePolicy() {
        yp::ResumeStore s;
        const QString f = QStringLiteral("C:\\Movies\\film.mkv");
        s.record(f, 1234.5, 6000.0);
        QCOMPARE(s.positionFor(QStringLiteral("c:/movies/FILM.mkv")), 1234.5);  // case/separator-insensitive
        s.record(f, 5.0, 6000.0);  // stopped in the first seconds: start over next time
        QCOMPARE(s.positionFor(f), 0.0);
        s.record(f, 5990.0, 6000.0);  // in the credits: treat as finished
        QCOMPARE(s.positionFor(f), 0.0);
        s.record(QStringLiteral("C:\\clip.mp4"), 20.0, 45.0);  // short media never resumes
        QCOMPARE(s.positionFor(QStringLiteral("C:\\clip.mp4")), 0.0);
        s.record(f, std::numeric_limits<double>::quiet_NaN(), 6000.0);
        QCOMPARE(s.size(), 0);
    }

    void resumeEvictsLeastRecent() {
        yp::ResumeStore s({.maxEntries = 3});
        for (int i = 0; i < 4; ++i) s.record(QStringLiteral("f%1").arg(i), 100.0, 1000.0);
        QCOMPARE(s.size(), 3);
        QCOMPARE(s.positionFor(QStringLiteral("f0")), 0.0);
        s.record(QStringLiteral("f1"), 200.0, 1000.0);  // touching an entry makes it most recent
        s.record(QStringLiteral("f4"), 100.0, 1000.0);
        QCOMPARE(s.positionFor(QStringLiteral("f1")), 200.0);
        QCOMPARE(s.positionFor(QStringLiteral("f2")), 0.0);
    }

    void resumeJsonRoundTripAndBadInput() {
        yp::ResumeStore s;
        s.record(QStringLiteral("C:\\ä ✓.mkv"), 321.0, 5000.0);
        yp::ResumeStore t;
        t.loadJson(s.toJson());
        QCOMPARE(t.positionFor(QStringLiteral("C:\\ä ✓.mkv")), 321.0);
        t.loadJson("{not json");
        QCOMPARE(t.size(), 0);
        t.loadJson(R"({"entries":[{"path":"x","position":-3},{"position":5},{"path":"y","position":7}]})");
        QCOMPARE(t.size(), 1);
    }

    void speedSteps_data() {
        QTest::addColumn<double>("from");
        QTest::addColumn<int>("direction");
        QTest::addColumn<double>("expected");
        QTest::newRow("up from 1") << 1.0 << 1 << 1.1;
        QTest::newRow("down from 1") << 1.0 << -1 << 0.9;
        QTest::newRow("off-grid snaps up") << 1.25 << 1 << 1.3;
        QTest::newRow("off-grid snaps down") << 1.25 << -1 << 1.2;
        QTest::newRow("clamped at max") << 4.0 << 1 << 4.0;
        QTest::newRow("clamped at min") << 0.25 << -1 << 0.25;
        QTest::newRow("float noise 1.15") << 1.1 + 0.05 << 1 << 1.2;
    }

    void speedSteps() {
        QFETCH(double, from);
        QFETCH(int, direction);
        QFETCH(double, expected);
        QCOMPARE(yp::stepSpeed(from, direction), expected);
    }

    void delaySteps() {
        QCOMPARE(yp::stepDelay(0.0, 0.1), 0.1);
        double d = 0.0;
        for (int i = 0; i < 3; ++i) d = yp::stepDelay(d, 0.1);
        QCOMPARE(d, 0.3);  // rounded to ms, no 0.30000000000000004 drift
        QCOMPARE(yp::stepDelay(-0.05, -0.1), -0.15);
        QCOMPARE(yp::stepDelay(599.95, 0.1), 600.0);
        QCOMPARE(yp::stepDelay(std::numeric_limits<double>::quiet_NaN(), 0.1), 0.1);
    }

    void abLoopCycle() {
        yp::AbLoop ab;
        ab.cycle(10.0);
        QVERIFY(ab.hasA() && !ab.active());
        ab.cycle(5.0);  // not after A: becomes the new A
        QCOMPARE(ab.a, 5.0);
        QVERIFY(!ab.active());
        ab.cycle(8.0);
        QVERIFY(ab.active());
        QCOMPARE(ab.b, 8.0);
        ab.cycle(9.0);  // third press clears
        QVERIFY(!ab.hasA() && !ab.active());
    }

    void aspectParsing() {
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("16:9")), 16.0 / 9.0);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral(" 2.39 : 1 ")), 2.39);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("2.35")), 2.35);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("0:9")), 0.0);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("abc")), 0.0);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("1:2:3")), 0.0);
        QCOMPARE(yp::parseAspectRatio(QStringLiteral("100:1")), 0.0);  // outside 0.1..10: a typo
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
