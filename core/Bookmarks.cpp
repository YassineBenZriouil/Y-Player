#include "core/Bookmarks.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace yp {

QString Bookmarks::keyFor(const QString& path) { return QDir::toNativeSeparators(path).toLower(); }

void Bookmarks::add(const QString& path, double time, const QString& name) {
    if (path.isEmpty() || !std::isfinite(time) || time < 0.0) return;
    const QString key = keyFor(path);
    displayPath_.insert(key, QDir::toNativeSeparators(path));
    QList<Bookmark>& list = byFile_[key];
    const auto at = std::ranges::upper_bound(list, time, {}, &Bookmark::time);
    list.insert(at, Bookmark{time, name});
}

void Bookmarks::remove(const QString& path, int index) {
    const auto it = byFile_.find(keyFor(path));
    if (it == byFile_.end() || index < 0 || index >= it->size()) return;
    it->removeAt(index);
    if (it->isEmpty()) clear(path);
}

void Bookmarks::clear(const QString& path) {
    byFile_.remove(keyFor(path));
    displayPath_.remove(keyFor(path));
}

QList<Bookmark> Bookmarks::list(const QString& path) const { return byFile_.value(keyFor(path)); }

QByteArray Bookmarks::toJson() const {
    QJsonArray files;
    for (auto it = byFile_.cbegin(); it != byFile_.cend(); ++it) {
        QJsonArray marks;
        for (const Bookmark& b : it.value()) {
            marks.append(QJsonObject{{QStringLiteral("time"), b.time}, {QStringLiteral("name"), b.name}});
        }
        files.append(QJsonObject{{QStringLiteral("path"), displayPath_.value(it.key())},
                                 {QStringLiteral("bookmarks"), marks}});
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("files"), files}}).toJson();
}

void Bookmarks::loadJson(const QByteArray& json) {
    byFile_.clear();
    displayPath_.clear();
    const QJsonArray files = QJsonDocument::fromJson(json).object().value(QStringLiteral("files")).toArray();
    for (const auto& f : files) {
        const QString path = f[QStringLiteral("path")].toString();
        for (const auto& b : f[QStringLiteral("bookmarks")].toArray()) {
            add(path, b[QStringLiteral("time")].toDouble(-1.0), b[QStringLiteral("name")].toString());
        }
    }
}

}  // namespace yp
