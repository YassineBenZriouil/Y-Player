#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace yp::playlistio {

// Entries are returned as absolute native paths. Relative entries resolve against `baseDir`
// (the playlist's folder). Network URLs are skipped: Y-Player plays local files only for now.
struct ParseResult {
    QStringList paths;
    int skipped = 0;  // comments are not counted; unusable or network entries are
};

ParseResult parseM3u(const QByteArray& data, const QString& baseDir);
ParseResult parsePls(const QByteArray& data, const QString& baseDir);
QByteArray writeM3u(const QStringList& paths);  // UTF-8 (.m3u8)
QByteArray writePls(const QStringList& paths);

// Picks the format from the extension (.m3u, .m3u8, .pls). Returns false on I/O errors or an
// unknown extension; `error` explains why.
bool load(const QString& file, ParseResult& out, QString* error = nullptr);
bool save(const QString& file, const QStringList& paths, QString* error = nullptr);

}  // namespace yp::playlistio
