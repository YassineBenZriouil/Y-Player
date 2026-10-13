#include "core/Adjustments.h"

#include <algorithm>
#include <cmath>

namespace yp {

QList<double> speedPresets() { return {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0}; }

double clampSpeed(double speed) { return std::isfinite(speed) ? std::clamp(speed, kMinSpeed, kMaxSpeed) : 1.0; }

double stepSpeed(double current, int direction) {
    // Integer hundredths avoid 1.15/0.1 == 11.4999... style rounding errors. Off-grid values
    // (1.25) snap toward the step direction: 1.25 -> 1.3 up, 1.2 down.
    const long h = std::lround(clampSpeed(current) * 100.0);
    const long gridBelow = h / 10 * 10;                      // floor to 0.1 (h > 0)
    const long gridAbove = gridBelow == h ? h : gridBelow + 10;  // ceil to 0.1
    const long next = direction > 0 ? gridBelow + 10 : gridAbove - 10;
    return clampSpeed(static_cast<double>(next) / 100.0);
}

double stepDelay(double current, double step) {
    const double next = std::isfinite(current) ? current + step : step;
    return std::clamp(std::round(next * 1000.0) / 1000.0, -kMaxDelay, kMaxDelay);
}

double parseAspectRatio(const QString& text) {
    const QStringList parts = text.trimmed().split(QLatin1Char(':'));
    bool ok1 = false;
    bool ok2 = true;
    const double w = parts.value(0).trimmed().toDouble(&ok1);
    const double h = parts.size() == 2 ? parts.value(1).trimmed().toDouble(&ok2) : 1.0;
    if (parts.size() > 2 || !ok1 || !ok2 || !(w > 0.0) || !(h > 0.0)) return 0.0;
    const double ratio = w / h;
    return ratio >= 0.1 && ratio <= 10.0 ? ratio : 0.0;
}

void AbLoop::cycle(double position) {
    if (!hasA()) {
        a = position;
    } else if (b < 0.0) {
        if (position > a) {
            b = position;
        } else {
            a = position;  // B must come after A; restart from here
        }
    } else {
        clear();
    }
}

}  // namespace yp
