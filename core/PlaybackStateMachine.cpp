#include "core/PlaybackStateMachine.h"

namespace yp {

PlaybackState nextState(PlaybackState current, PlaybackEvent event, bool paused) {
    const PlaybackState running = paused ? PlaybackState::Paused : PlaybackState::Playing;
    const bool active = current == PlaybackState::Playing || current == PlaybackState::Paused;
    switch (event) {
        case PlaybackEvent::FileStarted: return PlaybackState::Loading;
        case PlaybackEvent::FileLoaded: return current == PlaybackState::Loading ? running : current;
        case PlaybackEvent::PauseChanged: return active ? running : current;
        case PlaybackEvent::EofReached:
            return active || current == PlaybackState::Loading ? PlaybackState::Ended : current;
        case PlaybackEvent::EofCleared: return current == PlaybackState::Ended ? running : current;
        case PlaybackEvent::Stopped: return PlaybackState::Idle;
        case PlaybackEvent::Failed: return PlaybackState::Error;
    }
    return current;
}

}  // namespace yp
