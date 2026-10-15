#pragma once

#include <QtGlobal>

namespace yp::platform {

// Window-style changes made directly on the HWND. Qt's equivalents (setWindowFlags) recreate the
// native window, which would destroy the video child window libmpv presents into.
void setAlwaysOnTop(quintptr hwnd, bool on);
// Removes the caption (title bar) but keeps the resizable frame.
void setBorderless(quintptr hwnd, bool on);
// Windows "High contrast" / "Contrast themes" accessibility setting.
bool highContrastEnabled();

}  // namespace yp::platform
