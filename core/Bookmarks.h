#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

namespace yp {

struct Bookmark {
    double time = 0.0;  // seconds
    QString name;
};

// Named positions per file (paths compare case-insensitively, as on Windows). Pure data + JSON;
// the caller decides where the JSON lives.
class Bookmarks {
public:
    void add(const QString& path, double time, const QString& name);  // kept sorted by time
    void remove(const QString& path, int index);
    void clear(const QString& path);
    [[nodiscard]] QList<Bookmark> list(const QString& path) const;

    [[nodiscard]] QByteArray toJson() const;
    void loadJson(const QByteArray& json);  // malformed input leaves the store empty

private:
    [[nodiscard]] static QString keyFor(const QString& path);

    QHash<QString, QList<Bookmark>> byFile_;
    QHash<QString, QString> displayPath_;  // key -> path as first given
};

}  // namespace yp
