#include "core/TimeFormat.h"

#include <climits>
#include <cmath>

namespace yp {
namespace {

double sanitize(double seconds) { return std::isfinite(seconds) && seconds > 0.0 ? seconds : 0.0; }

}  // namespace

QString formatTime(double seconds, double reference) {
    const auto total = static_cast<long long>(std::floor(sanitize(seconds)));
    const bool hours = total >= 3600 || sanitize(reference) >= 3600.0;
    const long long h = total / 3600;
    const long long m = (total / 60) % 60;
    const long long s = total % 60;
    const QChar zero(u'0');
    if (hours) return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, zero).arg(s, 2, 10, zero);
    return QStringLiteral("%1:%2").arg(total / 60).arg(s, 2, 10, zero);
}

int toMilliseconds(double seconds) {
    const double ms = std::round(sanitize(seconds) * 1000.0);
    return ms >= static_cast<double>(INT_MAX) ? INT_MAX : static_cast<int>(ms);
}

double clampSeekTarget(double seconds, double duration) {
    const double s = sanitize(seconds);
    return duration > 0.0 && s > duration ? duration : s;
}

}  // namespace yp
