#include "engine/mpv/MpvInfo.h"

#include <mpv/client.h>

namespace yp::mpv {
namespace {

// mpv packs versions as (major << 16) | minor.
constexpr unsigned long majorOf(unsigned long v) { return v >> 16U; }
constexpr unsigned long minorOf(unsigned long v) { return v & 0xFFFFU; }

}  // namespace

QString loadedApiVersion() {
    const unsigned long v = mpv_client_api_version();
    return QStringLiteral("%1.%2").arg(majorOf(v)).arg(minorOf(v));
}

bool loadedApiMatchesHeaders() {
    return majorOf(mpv_client_api_version()) == majorOf(MPV_CLIENT_API_VERSION);
}

}  // namespace yp::mpv
