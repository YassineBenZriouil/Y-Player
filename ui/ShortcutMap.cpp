#include "ui/ShortcutMap.h"

#include <QCoreApplication>

#include <utility>

namespace yp::ui {
namespace {

QString tr(const char* text) { return QCoreApplication::translate("Actions", text); }
QKeySequence k(const char* portable) { return {QString::fromLatin1(portable), QKeySequence::PortableText}; }

}  // namespace

QList<ActionDef> playerActions() {
    return {
        {QStringLiteral("file.open"), tr("&Open Files..."), {k("Ctrl+O"), k("O")}},
        {QStringLiteral("file.add"), tr("&Add Files to Playlist..."), {k("Ctrl+Shift+O")}},
        {QStringLiteral("file.openSubtitle"), tr("Load &Subtitle File..."), {k("Alt+O")}},
        {QStringLiteral("file.loadPlaylist"), tr("&Load Playlist..."), {}},
        {QStringLiteral("file.savePlaylist"), tr("Sa&ve Playlist..."), {k("Ctrl+S")}},
        {QStringLiteral("file.quit"), tr("E&xit"), {k("Ctrl+Q")}},

        {QStringLiteral("playback.toggle"), tr("&Play / Pause"), {k("Space")}},
        {QStringLiteral("playback.stop"), tr("&Stop"), {k("Ctrl+Space")}},
        {QStringLiteral("playback.previous"), tr("Pre&vious File"), {k("PgUp")}},
        {QStringLiteral("playback.next"), tr("&Next File"), {k("PgDown")}},
        {QStringLiteral("playback.seekForward"), tr("Seek &Forward 5 s"), {k("Right")}},
        {QStringLiteral("playback.seekBackward"), tr("Seek &Backward 5 s"), {k("Left")}},
        {QStringLiteral("playback.seekForwardLong"), tr("Seek Forward 30 s"), {k("Shift+Right")}},
        {QStringLiteral("playback.seekBackwardLong"), tr("Seek Backward 30 s"), {k("Shift+Left")}},
        {QStringLiteral("playback.frameNext"), tr("Next &Frame"), {k(".")}},
        {QStringLiteral("playback.framePrevious"), tr("Previous F&rame"), {k(",")}},
        {QStringLiteral("playback.faster"), tr("Faster (+0.1)"), {k("]")}},
        {QStringLiteral("playback.slower"), tr("Slower (-0.1)"), {k("[")}},
        {QStringLiteral("playback.speedReset"), tr("Normal Speed"), {k("Backspace")}},
        {QStringLiteral("playback.abLoop"), tr("Set A-B Point / Clear"), {k("L")}},
        {QStringLiteral("playback.abClear"), tr("Clear A-B Repeat"), {k("Shift+L")}},
        {QStringLiteral("playback.chapterNext"), tr("Next Chapter"), {k("Ctrl+PgDown")}},
        {QStringLiteral("playback.chapterPrevious"), tr("Previous Chapter"), {k("Ctrl+PgUp")}},
        {QStringLiteral("playback.cycleRepeat"), tr("Cycle Repeat Mode"), {k("R")}},
        {QStringLiteral("playback.shuffle"), tr("S&huffle"), {k("H")}},
        {QStringLiteral("playback.resume"), tr("&Resume From Last Position"), {}},

        {QStringLiteral("audio.volumeUp"), tr("Volume &Up"), {k("Up")}},
        {QStringLiteral("audio.volumeDown"), tr("Volume &Down"), {k("Down")}},
        {QStringLiteral("audio.mute"), tr("&Mute"), {k("M")}},
        {QStringLiteral("audio.cycleTrack"), tr("Next Audio &Track"), {k("A")}},
        {QStringLiteral("audio.delayPlus"), tr("Audio Delay +100 ms"), {k("Ctrl+=")}},
        {QStringLiteral("audio.delayMinus"), tr("Audio Delay -100 ms"), {k("Ctrl+-")}},
        {QStringLiteral("audio.delayReset"), tr("Reset Audio Delay"), {k("Ctrl+0")}},

        {QStringLiteral("subtitle.cycleTrack"), tr("Next Subtitle &Track"), {k("S")}},
        {QStringLiteral("subtitle.delayPlus"), tr("Subtitle Delay +100 ms"), {k("X")}},
        {QStringLiteral("subtitle.delayMinus"), tr("Subtitle Delay -100 ms"), {k("Z")}},
        {QStringLiteral("subtitle.delayReset"), tr("Reset Subtitle Delay"), {k("Shift+Z")}},
        {QStringLiteral("subtitle.style"), tr("Subtitle &Style..."), {}},

        {QStringLiteral("video.fullscreen"), tr("&Fullscreen"), {k("F"), k("Return")}},
        {QStringLiteral("video.exitFullscreen"), tr("Exit Fullscreen"), {k("Esc")}},
        {QStringLiteral("video.screenshot"), tr("Save &Screenshot"), {k("Ctrl+E")}},
        {QStringLiteral("video.screenshotClean"), tr("Save Screenshot (Without Subtitles)"), {k("Ctrl+Shift+E")}},
        {QStringLiteral("video.cycleAspect"), tr("Cycle Aspect Ratio"), {k("Ctrl+A")}},
        {QStringLiteral("video.rotate"), tr("Rotate 90° Clockwise"), {k("Ctrl+R")}},
        {QStringLiteral("video.flipH"), tr("Flip &Horizontally"), {}},
        {QStringLiteral("video.flipV"), tr("Flip &Vertically"), {}},
        {QStringLiteral("video.onTop"), tr("Always on &Top"), {k("Ctrl+T")}},
        {QStringLiteral("video.borderless"), tr("&Borderless Window"), {k("Ctrl+B")}},

        {QStringLiteral("view.playlist"), tr("&Playlist"), {k("F6")}},
        {QStringLiteral("tools.shortcuts"), tr("&Keyboard Shortcuts..."), {k("Ctrl+K")}},
        {QStringLiteral("help.about"), tr("&About Y-Player"), {}},
    };
}

ShortcutMap::ShortcutMap(QList<ActionDef> definitions) : definitions_(std::move(definitions)) { resetAll(); }

QList<QKeySequence> ShortcutMap::defaultsFor(const QString& id) const {
    for (const ActionDef& d : definitions_) {
        if (d.id == id) return d.defaults;
    }
    return {};
}

QString ShortcutMap::owner(const QKeySequence& key, const QString& except) const {
    if (key.isEmpty()) return {};
    for (auto it = current_.cbegin(); it != current_.cend(); ++it) {
        if (it.key() != except && it.value().contains(key)) return it.key();
    }
    return {};
}

bool ShortcutMap::set(const QString& id, const QList<QKeySequence>& keys, QString* conflict) {
    if (!current_.contains(id)) return false;
    QList<QKeySequence> unique;
    for (const QKeySequence& key : keys) {
        if (key.isEmpty() || unique.contains(key)) continue;
        if (const QString other = owner(key, id); !other.isEmpty()) {
            if (conflict) *conflict = other;
            return false;
        }
        unique.append(key);
    }
    current_[id] = std::move(unique);
    return true;
}

void ShortcutMap::reset(const QString& id) {
    // Restoring a default may collide with a key the user moved elsewhere; drop it there.
    const QList<QKeySequence> defaults = defaultsFor(id);
    for (const QKeySequence& key : defaults) {
        if (const QString other = owner(key, id); !other.isEmpty()) current_[other].removeAll(key);
    }
    current_[id] = defaults;
}

void ShortcutMap::resetAll() {
    current_.clear();
    for (const ActionDef& d : definitions_) current_.insert(d.id, d.defaults);
}

QVariantMap ShortcutMap::overrides() const {
    QVariantMap out;
    for (const ActionDef& d : definitions_) {
        const QList<QKeySequence>& keys = current_.value(d.id);
        if (keys == d.defaults) continue;
        QStringList text;
        for (const QKeySequence& key : keys) text << key.toString(QKeySequence::PortableText);
        out.insert(d.id, text);
    }
    return out;
}

int ShortcutMap::applyOverrides(const QVariantMap& overrides) {
    int skipped = 0;
    // Clear every overridden action first so swapped keys (A<->B) don't conflict mid-way.
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        if (current_.contains(it.key())) current_[it.key()].clear();
    }
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        if (!current_.contains(it.key())) {
            ++skipped;
            continue;
        }
        QList<QKeySequence> keys;
        for (const QString& text : it.value().toStringList()) {
            keys << QKeySequence(text, QKeySequence::PortableText);
        }
        if (!set(it.key(), keys)) {
            ++skipped;
            set(it.key(), defaultsFor(it.key()));  // fall back to defaults when they are free
        }
    }
    return skipped;
}

}  // namespace yp::ui
