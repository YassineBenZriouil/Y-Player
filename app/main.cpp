#include "app/BenchMode.h"
#include "core/Logging.h"
#include "engine/mpv/MpvEngine.h"
#include "engine/mpv/MpvInfo.h"
#include "platform/win/SystemInfo.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcApp, "yp.app")

namespace {

void logStartup() {
    qCInfo(lcApp).noquote() << "Y-Player" << YP_VERSION << "starting; Qt runtime" << qVersion() << "built with"
                            << QT_VERSION_STR << "; libmpv client API" << yp::mpv::loadedApiVersion();
    const yp::platform::SystemInfo sys = yp::platform::querySystemInfo();
    qCInfo(lcApp).noquote() << "OS:" << sys.os << "| CPU:" << sys.cpu << "|" << sys.logicalCores << "logical cores";
    for (const yp::platform::GpuInfo& gpu : sys.gpus) {
        qCInfo(lcApp).noquote() << "GPU:" << gpu.name << "driver" << gpu.driverVersion << "|"
                                << gpu.dedicatedVideoMemoryMb << "MB dedicated";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Y-Player"));
    QApplication::setApplicationVersion(QStringLiteral(YP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/y-player.png")));
#ifdef NDEBUG
    QLoggingCategory::setFilterRules(QStringLiteral("yp.*.debug=false"));  // QT_LOGGING_RULES still overrides
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Y-Player video player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Media file to play."));
    const QCommandLineOption benchReport(QStringLiteral("bench-report"),
                                         QStringLiteral("Benchmark FILE, write JSON metrics here, then exit."),
                                         QStringLiteral("path"));
    const QCommandLineOption benchPlay(QStringLiteral("bench-play"), QStringLiteral("Measured playback seconds."),
                                       QStringLiteral("seconds"), QStringLiteral("12"));
    const QCommandLineOption benchWarmup(QStringLiteral("bench-warmup"),
                                         QStringLiteral("Seconds after the first frame before measuring."),
                                         QStringLiteral("seconds"), QStringLiteral("3"));
    parser.addOptions({benchReport, benchPlay, benchWarmup});
    parser.process(app);
    const QStringList files = parser.positionalArguments();

    // Everything Y-Player stores stays on this machine: %LOCALAPPDATA%\Y-Player
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    const QString logDir = dataDir + QStringLiteral("/logs");
    const bool fileLogging = yp::log::install({.directory = logDir});
    logStartup();
    if (!fileLogging) qCWarning(lcApp).noquote() << "file logging unavailable in" << logDir;
    if (!yp::mpv::loadedApiMatchesHeaders()) {
        qCCritical(lcApp).noquote() << "libmpv API mismatch: loaded" << yp::mpv::loadedApiVersion();
    }

    // Interface language (Settings > General); "system" follows Windows. Untranslated strings
    // fall back to English. Read before any window exists so every string is translated.
    QTranslator translator;
    {
        const QSettings stored(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
        const QString language = stored.value(QStringLiteral("general/language"), QStringLiteral("system")).toString();
        const QLocale locale = language == QLatin1String("system") ? QLocale::system() : QLocale(language);
        if (translator.load(locale, QStringLiteral("yplayer"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
            QApplication::installTranslator(&translator);
        }
    }

    const QString about =
        QApplication::translate("About",
                                "<h3>Y-Player %1</h3><p>A fast, hardware-accelerated video player for Windows.</p>"
                                "<p>Built with Qt %2 and libmpv (client API %3).<br>"
                                "Licensed under the GNU GPL, version 3 or later.</p>")
            .arg(QStringLiteral(YP_VERSION), QString::fromLatin1(qVersion()), yp::mpv::loadedApiVersion());
    yp::ui::MainWindow window(std::make_unique<yp::mpv::MpvEngine>(), about, dataDir);
    // Created before show() so the bench sees the window's first expose (startup time).
    yp::app::BenchRun* bench = nullptr;
    if (parser.isSet(benchReport)) {
        window.setBenchmarkMode();
        bench = new yp::app::BenchRun(window, {.reportPath = parser.value(benchReport),
                                               .file = files.value(0),
                                               .warmupSeconds = parser.value(benchWarmup).toInt(),
                                               .playSeconds = parser.value(benchPlay).toInt()});  // owned by window
    }
    window.show();

    if (bench != nullptr) {
        QTimer::singleShot(0, bench, &yp::app::BenchRun::start);
    } else if (!files.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, files] { window.openFiles(files); });
    }

    const int rc = QApplication::exec();
    qCInfo(lcApp) << "exiting with code" << rc;
    yp::log::shutdown();
    return rc;
}
