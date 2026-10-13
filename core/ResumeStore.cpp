#include "core/ResumeStore.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace yp {

QString ResumeStore::keyFor(const QString& path) { return QDir::toNativeSeparators(path).toLower(); }

void ResumeStore::record(const QString& path, double position, double duration) {
    forget(path);
    const bool worthKeeping = std::isfinite(position) && std::isfinite(duration) &&
                              duration >= policy_.minDuration && position >= policy_.minPosition &&
                              position <= duration - policy_.endMargin;
    if (!worthKeeping) return;
    entries_.prepend({.key = keyFor(path), .path = QDir::toNativeSeparators(path), .position = position});
    while (entries_.size() > policy_.maxEntries) entries_.removeLast();
}

double ResumeStore::positionFor(const QString& path) const {
    const QString key = keyFor(path);
    for (const Entry& e : entries_) {
        if (e.key == key) return e.position;
    }
    return 0.0;
}

void ResumeStore::forget(const QString& path) {
    const QString key = keyFor(path);
    entries_.removeIf([&](const Entry& e) { return e.key == key; });
}

QByteArray ResumeStore::toJson() const {
    QJsonArray list;
    for (const Entry& e : entries_) {
        list.append(QJsonObject{{QStringLiteral("path"), e.path}, {QStringLiteral("position"), e.position}});
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("entries"), list}}).toJson();
}

void ResumeStore::loadJson(const QByteArray& json) {
    entries_.clear();
    const QJsonArray list = QJsonDocument::fromJson(json).object().value(QStringLiteral("entries")).toArray();
    for (const auto& v : list) {
        const QString path = v[QStringLiteral("path")].toString();
        const double position = v[QStringLiteral("position")].toDouble(-1.0);
        if (path.isEmpty() || !(position > 0.0) || entries_.size() >= policy_.maxEntries) continue;
        entries_.append({.key = keyFor(path), .path = path, .position = position});
    }
}

}  // namespace yp
