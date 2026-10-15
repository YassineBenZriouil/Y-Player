#pragma once

#include <QString>

class QApplication;

namespace yp::ui {

// Applies the Y-Player look (ADR-018): Fusion style with the brand palette, Segoe UI Variable.
// mode: "system" (follow Windows light/dark), "dark" or "light". With Windows high contrast on,
// the system palette is kept untouched so accessibility colours always win.
// Returns true when the effective theme is dark.
bool applyTheme(QApplication& app, const QString& mode);

}  // namespace yp::ui
