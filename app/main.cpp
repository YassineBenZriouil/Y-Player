#include "core/Logging.h"
#include "engine/mpv/MpvInfo.h"
#include "platform/win/SystemInfo.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcApp, "yp.app")

namespace {

void logStartup() {
    qCInfo(lcApp).noquote() << "Y-Player" << YP_VERSION << "starting; Qt runtime" << qVersion() << "built with" << QT_VERSION_STR
                  << "; libmpv client API" << yp::mpv::loadedApiVersion();
    const yp::platform::SystemInfo sys = yp::platform::querySystemInfo();
    qCInfo(lcApp).noquote() << "OS:" << sys.os << "| CPU:" << sys.cpu << "|" << sys.logicalCores << "logical cores";
    for (const yp::platform::GpuInfo& gpu : sys.gpus) {
        qCInfo(lcApp).noquote() << "GPU:" << gpu.name << "driver" << gpu.driverVersion << "|" << gpu.dedicatedVideoMemoryMb
                      << "MB dedicated";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Y-Player"));
    QApplication::setApplicationVersion(QStringLiteral(YP_VERSION));
#ifdef NDEBUG
    QLoggingCategory::setFilterRules(QStringLiteral("yp.*.debug=false"));  // QT_LOGGING_RULES still overrides
#endif

    // %LOCALAPPDATA%\Y-Player\logs
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
    const bool fileLogging = yp::log::install({.directory = logDir});
    logStartup();
    if (!fileLogging) qCWarning(lcApp).noquote() << "file logging unavailable in" << logDir;

    if (!yp::mpv::loadedApiMatchesHeaders()) {
        qCCritical(lcApp).noquote() << "libmpv API mismatch: loaded" << yp::mpv::loadedApiVersion();
    }

    const QString about = QStringLiteral("<b>Y-Player %1</b><br>Qt %2<br>libmpv client API %3")
                              .arg(QStringLiteral(YP_VERSION), QString::fromLatin1(qVersion()), yp::mpv::loadedApiVersion());
    yp::ui::MainWindow window(about);
    window.show();

    const int rc = QApplication::exec();
    qCInfo(lcApp).noquote() << "exiting with code" << rc;
    yp::log::shutdown();
    return rc;
}
