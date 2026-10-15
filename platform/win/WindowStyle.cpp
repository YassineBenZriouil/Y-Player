#include "platform/win/WindowStyle.h"

#include <windows.h>

namespace yp::platform {

namespace {
// Qt hands out native window ids as integers; turning one back into an HWND is the point.
HWND toHwnd(quintptr id) { return reinterpret_cast<HWND>(id); }  // NOLINT(performance-no-int-to-ptr)
}  // namespace

void setAlwaysOnTop(quintptr hwnd, bool on) {
    SetWindowPos(toHwnd(hwnd), on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void setBorderless(quintptr hwnd, bool on) {
    const HWND h = toHwnd(hwnd);
    auto style = static_cast<ULONG_PTR>(GetWindowLongPtrW(h, GWL_STYLE));
    constexpr auto caption = static_cast<ULONG_PTR>(WS_CAPTION);
    style = on ? (style & ~caption) : (style | caption);
    SetWindowLongPtrW(h, GWL_STYLE, static_cast<LONG_PTR>(style));
    SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool highContrastEnabled() {
    HIGHCONTRASTW hc{};
    hc.cbSize = sizeof hc;
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0) != 0 && (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

}  // namespace yp::platform
