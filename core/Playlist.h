#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <random>

namespace yp {

// Ordered list of local media paths with a current entry, repeat and shuffle.
// Pure logic (no I/O, no Qt models); the UI wraps it in a list model.
class Playlist {
public:
    enum class Repeat { Off, One, All };

    [[nodiscard]] int size() const { return static_cast<int>(items_.size()); }
    [[nodiscard]] bool isEmpty() const { return items_.isEmpty(); }
    [[nodiscard]] const QString& at(int index) const { return items_.at(index); }
    [[nodiscard]] const QStringList& items() const { return items_; }

    [[nodiscard]] int current() const { return current_; }  // -1 when nothing is current
    void setCurrent(int index);

    void append(const QStringList& paths);
    void insert(int position, const QStringList& paths);
    void removeAt(QList<int> rows);  // any order, duplicates ignored
    void move(int from, int to);     // `to` is the final index of the moved entry
    void clear();

    [[nodiscard]] Repeat repeat() const { return repeat_; }
    void setRepeat(Repeat repeat) { repeat_ = repeat; }
    [[nodiscard]] bool shuffle() const { return shuffle_; }
    void setShuffle(bool on, quint32 seed = std::random_device{}());

    // Index to play after the current entry, or -1 to stop.
    // `userRequested`: the user pressed Next (Repeat::One then moves on instead of replaying).
    [[nodiscard]] int nextIndex(bool userRequested) const;
    [[nodiscard]] int previousIndex() const;

private:
    void rebuildOrder();
    [[nodiscard]] int orderPosition() const;  // position of current_ within order_

    QStringList items_;
    int current_ = -1;
    Repeat repeat_ = Repeat::Off;
    bool shuffle_ = false;
    QList<int> order_;  // play order while shuffled: a permutation starting at the current entry
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace yp
