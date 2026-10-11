#include "core/Logging.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

Q_LOGGING_CATEGORY(lcTest, "yp.test")

class TestLogging : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void writesOneFormattedRecordPerLine() {
        QTemporaryDir dir;
        QVERIFY(yp::log::install({.directory = dir.path(), .baseName = QStringLiteral("t.log"), .forwardToPrevious = false}));
        qCInfo(lcTest, "hello\nworld");
        qCWarning(lcTest, "careful");
        const QString path = yp::log::activeFilePath();
        yp::log::shutdown();

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        static const QRegularExpression record(
            QStringLiteral(R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z (info |warn ) \[yp\.test\] tid=\d+ (.*)$)"));
        const auto first = record.match(lines[0]);
        QVERIFY2(first.hasMatch(), qPrintable(lines[0]));
        QCOMPARE(first.captured(2), QStringLiteral(R"(hello\nworld)"));
        QVERIFY(record.match(lines[1]).hasMatch());
    }

    void rotatesAndKeepsBoundedFileCount() {
        QTemporaryDir dir;
        const yp::log::Options o{.directory = dir.path(), .baseName = QStringLiteral("r.log"), .maxBytes = 512,
                                 .keepFiles = 2, .forwardToPrevious = false};
        QVERIFY(yp::log::install(o));
        for (int i = 0; i < 200; ++i) qCInfo(lcTest, "line %d padding padding padding", i);
        yp::log::shutdown();

        const QDir d(dir.path());
        QVERIFY(d.exists(QStringLiteral("r.log")));
        QVERIFY(d.exists(QStringLiteral("r.log.1")));
        QVERIFY(d.exists(QStringLiteral("r.log.2")));
        QVERIFY(!d.exists(QStringLiteral("r.log.3")));
        for (const QString& name : {QStringLiteral("r.log"), QStringLiteral("r.log.1"), QStringLiteral("r.log.2")}) {
            QVERIFY2(QFileInfo(d.filePath(name)).size() < 512 + 200, qPrintable(name));  // max + one record
        }
        // Newest record is in the active file.
        QFile active(d.filePath(QStringLiteral("r.log")));
        QVERIFY(active.open(QIODevice::ReadOnly));
        QVERIFY(active.readAll().contains("line 199 "));
    }

    void secondInstallIsRejectedAndShutdownIsIdempotent() {
        QTemporaryDir dir;
        QVERIFY(yp::log::install({.directory = dir.path(), .forwardToPrevious = false}));
        QVERIFY(!yp::log::install({.directory = dir.path()}));
        yp::log::shutdown();
        yp::log::shutdown();
        QVERIFY(yp::log::activeFilePath().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLogging)
#include "tst_logging.moc"
