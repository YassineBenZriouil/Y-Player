#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace yp {

struct ResumePolicy {
    double minDuration = 60.0;  // shorter media always starts from the beginning
    double minPosition = 10.0;  // stopped in the first seconds: start over next time
    double endMargin = 20.0;    // stopped in the last seconds (credits): treat as finished
    int maxEntries = 500;       // least recently used entries are dropped beyond this
};

// Remembers where playback stopped, per file. Pure data + JSON (de)serialization; the caller
// decides where the JSON lives. Paths are keyed case-insensitively (Windows).
class ResumeStore {
public:
    explicit ResumeStore(ResumePolicy policy = {}) : policy_(policy) {}

    // Records (or forgets, per policy) the stop position of `path`.
    void record(const QString& path, double position, double duration);
    [[nodiscard]] double positionFor(const QString& path) const;  // 0 = start from the beginning
    void forget(const QString& path);
    [[nodiscard]] int size() const { return static_cast<int>(entries_.size()); }

    [[nodiscard]] QByteArray toJson() const;
    void loadJson(const QByteArray& json);  // malformed input leaves the store empty

private:
    struct Entry {
        QString key;
        QString path;
        double position;
    };
    [[nodiscard]] static QString keyFor(const QString& path);

    ResumePolicy policy_;
    QList<Entry> entries_;  // most recent first
};

}  // namespace yp
