#include "core/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QThread>

namespace yp::log {
namespace {

struct State {
    QMutex mutex;
    Options options;
    QFile file;
    qint64 bytes = 0;
    QtMessageHandler previous = nullptr;
    bool installed = false;
};

State& state() {
    static State s;
    return s;
}

const char* levelName(QtMsgType type) {
    switch (type) {
        case QtDebugMsg: return "debug";
        case QtInfoMsg: return "info ";
        case QtWarningMsg: return "warn ";
        case QtCriticalMsg: return "error";
        case QtFatalMsg: return "fatal";
    }
    return "?    ";
}

QString filePath(const Options& o, int index) {
    const QString base = QDir(o.directory).filePath(o.baseName);
    return index == 0 ? base : base + QLatin1Char('.') + QString::number(index);
}

bool openActive(State& s) {
    s.file.setFileName(filePath(s.options, 0));
    if (!s.file.open(QIODevice::WriteOnly | QIODevice::Append)) return false;
    s.bytes = s.file.size();
    return true;
}

void rotateLocked(State& s) {
    s.file.close();
    QFile::remove(filePath(s.options, s.options.keepFiles));
    for (int i = s.options.keepFiles - 1; i >= 0; --i) {
        QFile::rename(filePath(s.options, i), filePath(s.options, i + 1));
    }
    openActive(s);  // on failure logging stops silently; nothing better to report to
}

QByteArray formatLine(QtMsgType type, const char* category, const QString& msg) {
    QString text = msg;
    text.replace(QLatin1Char('\n'), QLatin1String("\\n"));  // keep one record per line
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8() + ' ' + levelName(type) +
           " [" + (category ? category : "default") + "] tid=" +
           QByteArray::number(reinterpret_cast<quintptr>(QThread::currentThreadId())) + ' ' + text.toUtf8() + '\n';
}

void handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    State& s = state();
    QtMessageHandler forward = nullptr;
    {
        const QMutexLocker lock(&s.mutex);
        if (s.file.isOpen()) {
            const QByteArray line = formatLine(type, ctx.category, msg);
            s.file.write(line);
            s.file.flush();
            s.bytes += line.size();
            if (s.bytes >= s.options.maxBytes) rotateLocked(s);
        }
        if (s.options.forwardToPrevious) forward = s.previous;
    }
    if (forward) forward(type, ctx, msg);
}

}  // namespace

bool install(const Options& options) {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    if (s.installed || !QDir().mkpath(options.directory)) return false;
    s.options = options;
    if (!openActive(s)) return false;
    s.previous = qInstallMessageHandler(handler);
    s.installed = true;
    return true;
}

void shutdown() {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    if (!s.installed) return;
    qInstallMessageHandler(s.previous);
    s.previous = nullptr;
    s.file.close();
    s.installed = false;
}

QString activeFilePath() {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    return s.installed ? s.file.fileName() : QString();
}

}  // namespace yp::log
