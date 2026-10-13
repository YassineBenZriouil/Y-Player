#include "core/RecentFiles.h"

#include <QDir>

#include <ranges>

namespace yp {
namespace {

bool samePath(const QString& a, const QString& b) {
    return QDir::toNativeSeparators(a).compare(QDir::toNativeSeparators(b), Qt::CaseInsensitive) == 0;
}

}  // namespace

void RecentFiles::add(const QString& path) {
    if (path.isEmpty()) return;
    remove(path);
    items_.prepend(QDir::toNativeSeparators(path));
    while (items_.size() > capacity_) items_.removeLast();
}

void RecentFiles::remove(const QString& path) {
    items_.removeIf([&](const QString& item) { return samePath(item, path); });
}

void RecentFiles::setItems(const QStringList& items) {
    items_.clear();
    for (const QString& item : items | std::views::reverse) add(item);  // oldest first keeps order
}

}  // namespace yp
