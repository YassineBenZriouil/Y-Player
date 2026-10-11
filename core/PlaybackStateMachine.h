#pragma once

#include "core/PlaybackEngine.h"

namespace yp {

// Backend-neutral events that drive PlaybackState. Engines translate their native
// notifications into these so the transition rules live (and are tested) in one place.
enum class PlaybackEvent {
    FileStarted,   // a file began loading
    FileLoaded,    // demuxer/decoders are ready
    PauseChanged,  // the backend's pause flag changed
    EofReached,    // playback hit the end and is holding the last frame
    EofCleared,    // left the end again (e.g. seek back)
    Stopped,       // file unloaded on request
    Failed,        // file could not be played
};

// `paused` is the backend's pause flag *after* the event.
PlaybackState nextState(PlaybackState current, PlaybackEvent event, bool paused);

}  // namespace yp
