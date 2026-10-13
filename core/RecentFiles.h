#pragma once

#include <QStringList>

namespace yp {

// Most-recently-used file list: newest first, no duplicates (paths compare case-insensitively
// and separator-insensitively, as Windows does), capped length.
class RecentFiles {
public:
    explicit RecentFiles(int capacity = 15) : capacity_(capacity) {}

    void add(const QString& path);
    void remove(const QString& path);
    void clear() { items_.clear(); }
    [[nodiscard]] const QStringList& items() const { return items_; }
    void setItems(const QStringList& items);  // e.g. restored from settings; re-applies the rules

private:
    int capacity_;
    QStringList items_;
};

}  // namespace yp
