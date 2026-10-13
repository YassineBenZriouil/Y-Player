#include "core/Playlist.h"

#include <QElapsedTimer>
#include <QSet>
#include <QTest>

using yp::Playlist;

namespace {

Playlist make(int n) {
    Playlist p;
    QStringList items;
    for (int i = 0; i < n; ++i) items << QStringLiteral("C:\\m\\%1.mkv").arg(i);
    p.append(items);
    return p;
}

}  // namespace

class TestPlaylist : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void sequentialNextAndPrevious() {
        Playlist p = make(3);
        QCOMPARE(p.current(), -1);
        QCOMPARE(p.nextIndex(false), 0);  // nothing current: start at the top
        p.setCurrent(0);
        QCOMPARE(p.nextIndex(false), 1);
        p.setCurrent(2);
        QCOMPARE(p.nextIndex(false), -1);  // end of list, repeat off: stop
        QCOMPARE(p.previousIndex(), 1);
        p.setCurrent(0);
        QCOMPARE(p.previousIndex(), -1);
    }

    void repeatModes() {
        Playlist p = make(3);
        p.setCurrent(2);
        p.setRepeat(Playlist::Repeat::All);
        QCOMPARE(p.nextIndex(false), 0);
        p.setCurrent(0);
        QCOMPARE(p.previousIndex(), 2);
        p.setRepeat(Playlist::Repeat::One);
        QCOMPARE(p.nextIndex(false), 0);  // automatic advance replays the same entry
        QCOMPARE(p.nextIndex(true), 1);   // the Next button still moves on
    }

    void insertKeepsCurrentEntry() {
        Playlist p = make(3);
        p.setCurrent(1);
        p.insert(0, {QStringLiteral("new")});
        QCOMPARE(p.current(), 2);
        QCOMPARE(p.at(p.current()), QStringLiteral("C:\\m\\1.mkv"));
        p.insert(10, {QStringLiteral("tail")});  // out of range appends
        QCOMPARE(p.at(p.size() - 1), QStringLiteral("tail"));
    }

    void removeAdjustsCurrent() {
        Playlist p = make(5);
        p.setCurrent(3);
        p.removeAt({0, 1, 1, 99});  // duplicates and out-of-range rows are ignored
        QCOMPARE(p.size(), 3);
        QCOMPARE(p.current(), 1);
        QCOMPARE(p.at(1), QStringLiteral("C:\\m\\3.mkv"));
        p.removeAt({1});
        QCOMPARE(p.current(), -1);  // the current entry itself was removed
    }

    void moveAdjustsCurrent_data() {
        QTest::addColumn<int>("current");
        QTest::addColumn<int>("from");
        QTest::addColumn<int>("to");
        QTest::addColumn<int>("expected");
        QTest::newRow("move current down") << 1 << 1 << 3 << 3;
        QTest::newRow("move other from above past current") << 2 << 0 << 3 << 1;
        QTest::newRow("move other from below above current") << 2 << 4 << 0 << 3;
        QTest::newRow("unrelated move") << 0 << 2 << 4 << 0;
    }

    void moveAdjustsCurrent() {
        QFETCH(int, current);
        QFETCH(int, from);
        QFETCH(int, to);
        QFETCH(int, expected);
        Playlist p = make(5);
        p.setCurrent(current);
        const QString playing = p.at(current);
        p.move(from, to);
        QCOMPARE(p.current(), expected);
        QCOMPARE(p.at(p.current()), playing);
    }

    void shufflePlaysEveryEntryOnce() {
        Playlist p = make(50);
        p.setCurrent(7);
        p.setShuffle(true, 1234);
        QSet<int> seen{7};
        int i = p.current();
        for (int step = 0; step < 49; ++step) {
            i = p.nextIndex(false);
            QVERIFY(i >= 0);
            QVERIFY2(!seen.contains(i), "an entry repeated before the cycle finished");
            seen.insert(i);
            p.setCurrent(i);
        }
        QCOMPARE(seen.size(), 50);
        QCOMPARE(p.nextIndex(false), -1);  // repeat off: stop after the full cycle
        p.setRepeat(Playlist::Repeat::All);
        QCOMPARE(p.nextIndex(false), 7);  // wraps to the start of the same order
    }

    void shufflePreviousRetracesOrder() {
        Playlist p = make(10);
        p.setCurrent(0);
        p.setShuffle(true, 99);
        const int a = p.nextIndex(false);
        p.setCurrent(a);
        const int b = p.nextIndex(false);
        p.setCurrent(b);
        QCOMPARE(p.previousIndex(), a);
    }

    void largeListsStayFast() {
        QElapsedTimer t;
        t.start();
        Playlist p = make(100000);
        p.setCurrent(50000);
        QCOMPARE(p.nextIndex(false), 50001);
        p.setShuffle(true, 1);
        QVERIFY(p.nextIndex(false) >= 0);
        QVERIFY2(t.elapsed() < 1000, qPrintable(QString::number(t.elapsed())));
    }

    void emptyPlaylist() {
        Playlist p;
        QCOMPARE(p.nextIndex(true), -1);
        QCOMPARE(p.previousIndex(), -1);
        p.setCurrent(3);
        QCOMPARE(p.current(), -1);
    }
};

QTEST_GUILESS_MAIN(TestPlaylist)
#include "tst_playlist.moc"
