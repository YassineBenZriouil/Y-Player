#pragma once

#include "core/EngineSettings.h"
#include "core/PlaybackEngine.h"

class QSettings;

namespace yp {

// Everything the Settings window edits. Session state (playlist, recent files, repeat/shuffle,
// shortcuts, resume points, bookmarks) is stored separately by the window.
struct PlayerSettings {
    EngineSettings engine;
    SubtitleStyle subtitles;
    QString audioChannels = QStringLiteral("auto");  // auto | stereo | 5.1 | 7.1 | mono
    int volume = 100;
    bool muted = false;

    bool resume = true;          // continue files from where they were stopped
    bool restoreSession = true;  // reload the last playlist at start
    bool autoplayNext = true;    // advance through the playlist when a file ends
    bool onTop = false;
    double seekShort = 5.0;   // seconds, arrow keys
    double seekLong = 30.0;   // seconds, Shift+arrow keys
    bool exactSeek = true;    // false: snap to keyframes (faster on long-GOP files)

    bool operator==(const PlayerSettings&) const = default;
};

// Keys live under the QSettings' current group. Loading validates everything: unknown choices
// fall back to the default and numbers are clamped, so a hand-edited or imported file can't
// push bad values into the engine.
void saveEngineSettings(QSettings& s, const EngineSettings& e);
EngineSettings loadEngineSettings(QSettings& s);
void savePlayerSettings(QSettings& s, const PlayerSettings& p);
PlayerSettings loadPlayerSettings(QSettings& s);

// Built-in rendering profiles (Video > Profiles); user profiles are stored next to them.
struct NamedProfile {
    QString name;
    EngineSettings settings;
};
QList<NamedProfile> builtInProfiles();

}  // namespace yp
