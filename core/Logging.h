#pragma once

#include <QString>
#include <QtGlobal>

// Process-wide logging: routes every qDebug/qCInfo/... call to a size-rotated file.
// One record per line: "<UTC ISO time> <level> [<category>] tid=<id> <message>".
namespace yp::log {

struct Options {
    QString directory;
    QString baseName = QStringLiteral("y-player.log");
    qint64 maxBytes = qint64{5} * 1024 * 1024;  // rotate when the active file reaches this size
    int keepFiles = 3;                  // rotated files kept: <base>.1 ... <base>.<keepFiles>
    bool forwardToPrevious = true;      // also pass records to the previously installed handler
};

// Returns false if already installed or the log file cannot be opened.
bool install(const Options& options);
void shutdown();
QString activeFilePath();

}  // namespace yp::log
