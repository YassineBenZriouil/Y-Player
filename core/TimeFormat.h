#pragma once

#include <QString>

namespace yp {

// "m:ss", or "h:mm:ss" when the value (or `reference`, e.g. the duration) reaches an hour,
// so position and duration labels share one width. Negative/NaN/inf render as 0.
QString formatTime(double seconds, double reference = 0.0);

// Seconds -> whole milliseconds for slider positions, clamped to [0, INT_MAX].
int toMilliseconds(double seconds);

// Clamp a seek target into [0, duration]; an unknown duration (<= 0) only clamps at 0.
double clampSeekTarget(double seconds, double duration);

}  // namespace yp
