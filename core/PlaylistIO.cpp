#include "core/PlaylistIO.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUrl>

namespace yp::playlistio {
namespace {

// Playlists from other players are often in the ANSI code page; accept UTF-8 first.
QString decode(const QByteArray& data) {
    QByteArray bytes = data;
    if (bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0, 3);
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(bytes);
    if (!utf8.hasError()) return text;
    QStringDecoder system(QStringDecoder::System);
    return system.decode(bytes);
}

// Returns an absolute native path, or empty if the entry is not a local file reference.
QString resolve(QString entry, const QString& baseDir) {
    entry = entry.trimmed();
    if (entry.isEmpty()) return {};
    if (entry.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        entry = QUrl(entry).toLocalFile();
        if (entry.isEmpty()) return {};
    } else if (entry.contains(QRegularExpression(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]+://")))) {
        return {};  // http://, rtsp://, ... (network playback is not supported yet)
    }
    const QString absolute = QDir::isAbsolutePath(entry) ? entry : QDir(baseDir).filePath(entry);
    return QDir::toNativeSeparators(QDir::cleanPath(absolute));
}

}  // namespace

ParseResult parseM3u(const QByteArray& data, const QString& baseDir) {
    ParseResult r;
    const QStringList lines = decode(data).split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QString path = resolve(line, baseDir);
        if (path.isEmpty()) {
            ++r.skipped;
        } else {
            r.paths.append(path);
        }
    }
    return r;
}

ParseResult parsePls(const QByteArray& data, const QString& baseDir) {
    ParseResult r;
    static const QRegularExpression fileKey(QStringLiteral("^File(\\d+)\\s*=(.*)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    QMap<int, QString> ordered;  // PLS entries are numbered; honour the numbers, not line order
    const QStringList lines = decode(data).split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    for (const QString& line : lines) {
        const auto m = fileKey.match(line.trimmed());
        if (m.hasMatch()) ordered.insert(m.captured(1).toInt(), m.captured(2));
    }
    for (const QString& entry : ordered) {
        const QString path = resolve(entry, baseDir);
        if (path.isEmpty()) {
            ++r.skipped;
        } else {
            r.paths.append(path);
        }
    }
    return r;
}

QByteArray writeM3u(const QStringList& paths) {
    QByteArray out = "#EXTM3U\n";
    for (const QString& p : paths) out += QDir::toNativeSeparators(p).toUtf8() + '\n';
    return out;
}

QByteArray writePls(const QStringList& paths) {
    QByteArray out = "[playlist]\n";
    for (int i = 0; i < paths.size(); ++i) {
        out += "File" + QByteArray::number(i + 1) + '=' + QDir::toNativeSeparators(paths.at(i)).toUtf8() + '\n';
    }
    out += "NumberOfEntries=" + QByteArray::number(paths.size()) + "\nVersion=2\n";
    return out;
}

bool load(const QString& file, ParseResult& out, QString* error) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QString suffix = QFileInfo(file).suffix().toLower();
    const QString base = QFileInfo(file).absolutePath();
    if (suffix == QLatin1String("pls")) {
        out = parsePls(f.readAll(), base);
    } else if (suffix == QLatin1String("m3u") || suffix == QLatin1String("m3u8")) {
        out = parseM3u(f.readAll(), base);
    } else {
        if (error) *error = QStringLiteral("unsupported playlist type .%1").arg(suffix);
        return false;
    }
    return true;
}

bool save(const QString& file, const QStringList& paths, QString* error) {
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const bool pls = QFileInfo(file).suffix().compare(QLatin1String("pls"), Qt::CaseInsensitive) == 0;
    f.write(pls ? writePls(paths) : writeM3u(paths));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

}  // namespace yp::playlistio
