#pragma once

#include <QIcon>

namespace yp::ui {

// Segoe Fluent Icons (Windows 11) / Segoe MDL2 Assets (Windows 10) code points.
enum class Glyph : char16_t {
    Play = 0xE768,
    Pause = 0xE769,
    Stop = 0xE71A,
    Previous = 0xE892,
    Next = 0xE893,
    Volume = 0xE767,
    Mute = 0xE74F,
    Fullscreen = 0xE740,
    Playlist = 0xE8FD,
};

// Icon drawn from the Windows icon font in the palette's text colours (normal + disabled), so it
// stays visible in dark and light mode; Qt's built-in media icons are dark and vanish on dark.
// Colours are taken when the icon is made: MainWindow::refreshIcons() re-creates them after a theme change.
QIcon glyphIcon(Glyph glyph);

}  // namespace yp::ui
