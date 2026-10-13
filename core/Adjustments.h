#pragma once

#include <QList>
#include <QString>

namespace yp {

inline constexpr double kMinSpeed = 0.25;
inline constexpr double kMaxSpeed = 4.0;
inline constexpr double kSpeedStep = 0.1;
inline constexpr double kDelayStep = 0.1;     // seconds per audio/subtitle delay keypress
inline constexpr double kMaxDelay = 600.0;    // +-10 minutes

QList<double> speedPresets();
double clampSpeed(double speed);
// One fine step faster (+1) or slower (-1), snapped to the 0.1 grid and clamped.
double stepSpeed(double current, int direction);
// Delay after adding `step` seconds, rounded to whole milliseconds and clamped to +-kMaxDelay.
double stepDelay(double current, double step);

// "16:9", "2.39:1", "2.35" or "1.85" -> width/height ratio; returns 0 for invalid input
// (anything outside 0.1..10 is rejected as a typo).
double parseAspectRatio(const QString& text);

// A-B repeat driven by one key: 1st press sets A, 2nd sets B (or moves A if the position is
// not after A), 3rd clears. a/b < 0 means unset.
struct AbLoop {
    double a = -1.0;
    double b = -1.0;

    [[nodiscard]] bool hasA() const { return a >= 0.0; }
    [[nodiscard]] bool active() const { return a >= 0.0 && b > a; }
    void cycle(double position);
    void clear() { a = b = -1.0; }
};

}  // namespace yp
