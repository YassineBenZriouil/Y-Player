#pragma once

#include <QString>

namespace yp::mpv {

// Client API version of the loaded libmpv DLL, e.g. "2.5".
QString loadedApiVersion();

// True when the loaded DLL's major API version matches the headers we compiled against.
bool loadedApiMatchesHeaders();

}  // namespace yp::mpv
