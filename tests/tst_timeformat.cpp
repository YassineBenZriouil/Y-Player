#include "core/TimeFormat.h"

#include <QTest>

#include <climits>
#include <limits>

class TestTimeFormat : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void formatTime_data() {
        QTest::addColumn<double>("seconds");
        QTest::addColumn<double>("reference");
        QTest::addColumn<QString>("expected");
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        QTest::newRow("zero") << 0.0 << 0.0 << QStringLiteral("0:00");
        QTest::newRow("truncates fraction") << 59.999 << 0.0 << QStringLiteral("0:59");
        QTest::newRow("one minute") << 60.0 << 0.0 << QStringLiteral("1:00");
        QTest::newRow("minutes beyond 9") << 754.2 << 0.0 << QStringLiteral("12:34");
        QTest::newRow("just under an hour") << 3599.9 << 0.0 << QStringLiteral("59:59");
        QTest::newRow("one hour") << 3600.0 << 0.0 << QStringLiteral("1:00:00");
        QTest::newRow("hours") << 3723.0 << 0.0 << QStringLiteral("1:02:03");
        QTest::newRow("long") << 100.0 * 3600 + 5 << 0.0 << QStringLiteral("100:00:05");
        QTest::newRow("hour layout from reference") << 65.0 << 7200.0 << QStringLiteral("0:01:05");
        QTest::newRow("negative") << -5.0 << 0.0 << QStringLiteral("0:00");
        QTest::newRow("nan") << nan << 0.0 << QStringLiteral("0:00");
        QTest::newRow("inf") << inf << 0.0 << QStringLiteral("0:00");
        QTest::newRow("nan reference ignored") << 65.0 << nan << QStringLiteral("1:05");
    }

    void formatTime() {
        QFETCH(double, seconds);
        QFETCH(double, reference);
        QFETCH(QString, expected);
        QCOMPARE(yp::formatTime(seconds, reference), expected);
    }

    void toMilliseconds() {
        QCOMPARE(yp::toMilliseconds(0.0), 0);
        QCOMPARE(yp::toMilliseconds(1.2345), 1235);  // rounds to nearest
        QCOMPARE(yp::toMilliseconds(-3.0), 0);
        QCOMPARE(yp::toMilliseconds(std::numeric_limits<double>::quiet_NaN()), 0);
        QCOMPARE(yp::toMilliseconds(1e12), INT_MAX);  // clamps instead of overflowing
    }

    void clampSeekTarget() {
        QCOMPARE(yp::clampSeekTarget(12.5, 100.0), 12.5);
        QCOMPARE(yp::clampSeekTarget(-4.0, 100.0), 0.0);
        QCOMPARE(yp::clampSeekTarget(150.0, 100.0), 100.0);
        QCOMPARE(yp::clampSeekTarget(150.0, 0.0), 150.0);  // unknown duration: no upper clamp
        QCOMPARE(yp::clampSeekTarget(std::numeric_limits<double>::quiet_NaN(), 100.0), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestTimeFormat)
#include "tst_timeformat.moc"
