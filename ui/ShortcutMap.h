#pragma once

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace yp::ui {

struct ActionDef {
    QString id;    // stable key used in settings, e.g. "playback.toggle"
    QString text;  // menu text (may contain & mnemonics)
    QList<QKeySequence> defaults;
};

// Every user-visible action with its default keys (INIT.md §12), in menu order.
QList<ActionDef> playerActions();

// Current key bindings for a fixed set of actions. A key can belong to only one action:
// set() refuses duplicates and names the action that already owns the key.
class ShortcutMap {
public:
    explicit ShortcutMap(QList<ActionDef> definitions);

    [[nodiscard]] const QList<ActionDef>& definitions() const { return definitions_; }
    [[nodiscard]] QList<QKeySequence> shortcuts(const QString& id) const { return current_.value(id); }
    [[nodiscard]] QString owner(const QKeySequence& key, const QString& except = {}) const;

    // Replaces the keys of `id`. Returns false (and the owning id in *conflict) if any key is
    // already bound to another action; nothing changes then. Empty list = unbound.
    bool set(const QString& id, const QList<QKeySequence>& keys, QString* conflict = nullptr);
    void reset(const QString& id);
    void resetAll();

    // Only bindings that differ from the defaults, as {id: ["Ctrl+O", ...]} (portable text).
    [[nodiscard]] QVariantMap overrides() const;
    // Applies saved overrides; unknown ids and entries that would create duplicates are skipped.
    // Returns how many entries were skipped.
    int applyOverrides(const QVariantMap& overrides);

private:
    [[nodiscard]] QList<QKeySequence> defaultsFor(const QString& id) const;

    QList<ActionDef> definitions_;
    QHash<QString, QList<QKeySequence>> current_;
};

}  // namespace yp::ui
