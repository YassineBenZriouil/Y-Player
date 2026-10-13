#include "core/Playlist.h"

#include <algorithm>

namespace yp {

void Playlist::setCurrent(int index) {
    current_ = index >= 0 && index < size() ? index : -1;
    if (shuffle_ && current_ >= 0 && orderPosition() < 0) rebuildOrder();
}

void Playlist::append(const QStringList& paths) { insert(size(), paths); }

void Playlist::insert(int position, const QStringList& paths) {
    if (paths.isEmpty()) return;
    position = std::clamp(position, 0, size());
    for (int i = 0; i < paths.size(); ++i) items_.insert(position + i, paths.at(i));
    if (current_ >= position) current_ += static_cast<int>(paths.size());
    if (shuffle_) rebuildOrder();
}

void Playlist::removeAt(QList<int> rows) {
    std::ranges::sort(rows, std::greater<>());
    rows.erase(std::ranges::unique(rows).begin(), rows.end());
    for (const int row : rows) {
        if (row < 0 || row >= size()) continue;
        items_.removeAt(row);
        if (current_ == row) {
            current_ = -1;
        } else if (current_ > row) {
            --current_;
        }
    }
    if (shuffle_) rebuildOrder();
}

void Playlist::move(int from, int to) {
    if (from < 0 || from >= size() || to < 0 || to >= size() || from == to) return;
    items_.move(from, to);
    if (current_ == from) {
        current_ = to;
    } else if (from < current_ && to >= current_) {
        --current_;
    } else if (from > current_ && to <= current_) {
        ++current_;
    }
    if (shuffle_) rebuildOrder();
}

void Playlist::clear() {
    items_.clear();
    order_.clear();
    current_ = -1;
}

void Playlist::setShuffle(bool on, quint32 seed) {
    shuffle_ = on;
    rng_.seed(seed);
    rebuildOrder();
}

void Playlist::rebuildOrder() {
    order_.clear();
    if (!shuffle_) return;
    for (int i = 0; i < size(); ++i) {
        if (i != current_) order_.append(i);
    }
    std::shuffle(order_.begin(), order_.end(), rng_);
    if (current_ >= 0) order_.prepend(current_);  // shuffled play continues from what is playing
}

int Playlist::orderPosition() const { return static_cast<int>(order_.indexOf(current_)); }

int Playlist::nextIndex(bool userRequested) const {
    if (isEmpty()) return -1;
    if (repeat_ == Repeat::One && !userRequested && current_ >= 0) return current_;
    const bool wrap = repeat_ != Repeat::Off;
    if (shuffle_) {
        const int pos = orderPosition();
        if (pos + 1 < order_.size()) return order_.at(pos + 1);
        return wrap ? order_.first() : -1;
    }
    if (current_ + 1 < size()) return current_ + 1;
    return wrap ? 0 : -1;
}

int Playlist::previousIndex() const {
    if (isEmpty()) return -1;
    const bool wrap = repeat_ != Repeat::Off;
    if (shuffle_) {
        const int pos = orderPosition();
        if (pos > 0) return order_.at(pos - 1);
        return wrap ? order_.last() : -1;
    }
    if (current_ > 0) return current_ - 1;
    if (current_ < 0) return 0;
    return wrap ? size() - 1 : -1;
}

}  // namespace yp
