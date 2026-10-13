#include "core/PlaylistIO.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace io = yp::playlistio;

class TestPlaylistIO : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void parsesM3uWithCommentsRelativeAndUrls() {
        const QByteArray data =
            "\xEF\xBB\xBF#EXTM3U\r\n"
            "#EXTINF:123,Title\r\n"
            "C:\\Videos\\a.mkv\r\n"
            "\r\n"
            "sub/b.mp4\n"
            "file:///D:/c%20d.webm\n"
            "https://example.com/stream.m3u8\n"
            "   \n";
        const io::ParseResult r = io::parseM3u(data, QStringLiteral("E:/lists"));
        QCOMPARE(r.paths, (QStringList{QStringLiteral("C:\\Videos\\a.mkv"), QStringLiteral("E:\\lists\\sub\\b.mp4"),
                                       QStringLiteral("D:\\c d.webm")}));
        QCOMPARE(r.skipped, 1);  // the network URL
    }

    void parsesUtf8AndLegacyEncodings() {
        const QString name = QStringLiteral("C:\\Фильмы\\視頻.mkv");
        QCOMPARE(io::parseM3u(name.toUtf8(), {}).paths, QStringList{name});
        // Not valid UTF-8 -> decoded with the system code page instead of producing garbage U+FFFD.
        const io::ParseResult latin = io::parseM3u(QByteArray("C:\\caf\xE9.mkv\n"), {});
        QCOMPARE(latin.paths.size(), 1);
        QVERIFY(!latin.paths.first().contains(QChar(0xFFFD)));
    }

    void parsesPlsInNumberOrder() {
        const QByteArray data =
            "[playlist]\n"
            "File2=C:\\two.mkv\n"
            "Title2=Two\n"
            "file1 = C:\\one.mkv\n"
            "File3=relative.mp4\n"
            "NumberOfEntries=3\n";
        const io::ParseResult r = io::parsePls(data, QStringLiteral("D:/x"));
        QCOMPARE(r.paths, (QStringList{QStringLiteral("C:\\one.mkv"), QStringLiteral("C:\\two.mkv"),
                                       QStringLiteral("D:\\x\\relative.mp4")}));
    }

    void malformedInputDoesNotCrash() {
        QCOMPARE(io::parseM3u(QByteArray("\0\0\xff\xfe garbage", 13), {}).paths.size(), 1);
        QVERIFY(io::parsePls(QByteArray("FileX=\nFile=\n[[["), {}).paths.isEmpty());
        QVERIFY(io::parseM3u({}, {}).paths.isEmpty());
    }

    void saveAndLoadRoundTrip_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("m3u8") << QStringLiteral("list.m3u8");
        QTest::newRow("m3u") << QStringLiteral("list.m3u");
        QTest::newRow("pls") << QStringLiteral("list.pls");
    }

    void saveAndLoadRoundTrip() {
        QFETCH(QString, name);
        QTemporaryDir dir;
        const QStringList paths{QStringLiteral("C:\\a b\\c.mkv"), QStringLiteral("D:\\Ünïcødé ✓\\d.mp4")};
        const QString file = dir.filePath(name);
        QString error;
        QVERIFY2(io::save(file, paths, &error), qPrintable(error));
        io::ParseResult r;
        QVERIFY2(io::load(file, r, &error), qPrintable(error));
        QCOMPARE(r.paths, paths);
    }

    void loadRejectsUnknownTypes() {
        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("x.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        io::ParseResult r;
        QString error;
        QVERIFY(!io::load(f.fileName(), r, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!io::load(dir.filePath(QStringLiteral("missing.m3u")), r, &error));
    }
};

QTEST_GUILESS_MAIN(TestPlaylistIO)
#include "tst_playlistio.moc"
