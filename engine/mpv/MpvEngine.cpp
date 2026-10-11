#include "engine/mpv/MpvEngine.h"

#include "core/TimeFormat.h"

#include <QFileInfo>
#include <QLoggingCategory>

#include <mpv/client.h>

#include <algorithm>
#include <cmath>
#include <utility>

Q_LOGGING_CATEGORY(lcMpv, "yp.mpv")

namespace yp::mpv {
namespace {

enum Property : quint64 {
    TimePos = 1,
    Duration,
    Pause,
    EofReached,
    Volume,
    Mute,
    Width,
    Height,
    Fps,
    VideoFormat,
    AudioCodec,
    Hwdec,
    Title,
    Path,
};

struct Observed {
    Property id;
    const char* name;
    mpv_format format;
};

constexpr Observed kObserved[] = {
    {TimePos, "time-pos", MPV_FORMAT_DOUBLE},
    {Duration, "duration", MPV_FORMAT_DOUBLE},
    {Pause, "pause", MPV_FORMAT_FLAG},
    {EofReached, "eof-reached", MPV_FORMAT_FLAG},
    {Volume, "volume", MPV_FORMAT_DOUBLE},
    {Mute, "mute", MPV_FORMAT_FLAG},
    {Width, "width", MPV_FORMAT_INT64},
    {Height, "height", MPV_FORMAT_INT64},
    {Fps, "container-fps", MPV_FORMAT_DOUBLE},
    {VideoFormat, "video-format", MPV_FORMAT_STRING},
    {AudioCodec, "audio-codec-name", MPV_FORMAT_STRING},
    {Hwdec, "hwdec-current", MPV_FORMAT_STRING},
    {Title, "media-title", MPV_FORMAT_STRING},
    {Path, "path", MPV_FORMAT_STRING},
};

// ADR-006: mpv must not read config, run scripts, or act on its own input/drops.
// ADR-003: present via gpu-next on D3D11 into our window, safe hardware decoding.
constexpr std::pair<const char*, const char*> kOptions[] = {
    {"config", "no"},
    {"load-scripts", "no"},
    {"ytdl", "no"},
    {"terminal", "no"},
    {"osc", "no"},
    {"input-default-bindings", "no"},
    {"input-vo-keyboard", "no"},
    {"input-cursor", "no"},
    {"drag-and-drop", "no"},
    {"vo", "gpu-next"},
    {"gpu-api", "d3d11"},
    {"hwdec", "auto-safe"},
    {"idle", "yes"},
    {"force-window", "yes"},  // keep the swapchain across files; paints black when idle
    {"keep-open", "yes"},     // hold the last frame at EOF instead of unloading
    {"volume-max", "100"},
    {"audio-client-name", "Y-Player"},
};

constexpr double kPositionStep = 0.1;  // seconds; throttles positionChanged during playback

}  // namespace

MpvEngine::MpvEngine(QObject* parent) : IPlaybackEngine(parent), mpv_(mpv_create()) {
    if (mpv_ == nullptr) {
        qCCritical(lcMpv) << "mpv_create failed";
        return;
    }
    for (const auto& [name, value] : kOptions) {
        if (const int err = mpv_set_option_string(mpv_, name, value); err < 0) {
            qCWarning(lcMpv) << "option" << name << "=" << value << "rejected:" << mpv_error_string(err);
        }
    }
    for (const Observed& p : kObserved) mpv_observe_property(mpv_, p.id, p.name, p.format);
    mpv_request_log_messages(mpv_, "warn");
    mpv_set_wakeup_callback(
        mpv_,
        [](void* ctx) {
            // Runs on an mpv thread: only queue a drain, coalescing bursts into one.
            auto* self = static_cast<MpvEngine*>(ctx);
            if (!self->drainQueued_.exchange(true)) {
                QMetaObject::invokeMethod(self, &MpvEngine::drainEvents, Qt::QueuedConnection);
            }
        },
        this);
}

MpvEngine::~MpvEngine() {
    if (mpv_ == nullptr) return;
    mpv_set_wakeup_callback(mpv_, nullptr, nullptr);  // no callback runs after this returns
    mpv_terminate_destroy(mpv_);  // joins mpv's threads; queued drains die with this QObject
}

bool MpvEngine::attachVideoOutput(quintptr nativeWindow) {
    if (mpv_ == nullptr || initialized_) return false;
    auto wid = static_cast<int64_t>(nativeWindow);
    mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid);
    if (const int err = mpv_initialize(mpv_); err < 0) {
        qCCritical(lcMpv) << "mpv_initialize failed:" << mpv_error_string(err);
        return false;
    }
    initialized_ = true;
    return true;
}

bool MpvEngine::ready() const { return mpv_ != nullptr && initialized_; }

void MpvEngine::open(const QString& path) {
    lastPath_ = path;
    if (!ready()) {
        fail(tr("the playback engine is not available"));
        return;
    }
    setPause(false);  // mpv's pause flag persists across files; a new file should play
    const QByteArray utf8 = path.toUtf8();
    const char* cmd[] = {"loadfile", utf8.constData(), "replace", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvEngine::play() {
    if (!ready()) return;
    if (state_ == PlaybackState::Idle || state_ == PlaybackState::Error) {
        if (!lastPath_.isEmpty()) open(lastPath_);
        return;
    }
    if (state_ == PlaybackState::Ended) seek(0.0, SeekMode::Exact);
    setPause(false);
}

void MpvEngine::pause() {
    if (ready()) setPause(true);
}

void MpvEngine::stop() {
    if (!ready()) return;
    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvEngine::seek(double seconds, SeekMode mode) {
    if (!ready() || state_ == PlaybackState::Idle || state_ == PlaybackState::Error) return;
    const QByteArray target = QByteArray::number(clampSeekTarget(seconds, duration_), 'f', 3);
    const char* cmd[] = {"seek", target.constData(), mode == SeekMode::Exact ? "absolute+exact" : "absolute+keyframes",
                         nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvEngine::setVolume(int percent) {
    if (!ready()) return;
    double v = std::clamp(percent, 0, 100);
    mpv_set_property_async(mpv_, 0, "volume", MPV_FORMAT_DOUBLE, &v);
}

void MpvEngine::setMuted(bool muted) {
    if (!ready()) return;
    int flag = muted ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::setPause(bool paused) {
    int flag = paused ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::drainEvents() {
    drainQueued_ = false;  // reset first: a wakeup during the loop queues another drain
    bool more = true;
    while (more) {
        const mpv_event* ev = mpv_wait_event(mpv_, 0);
        switch (ev->event_id) {
            case MPV_EVENT_NONE: more = false; break;
            case MPV_EVENT_START_FILE:
                position_ = 0.0;
                lastEmittedPosition_ = -1.0;
                duration_ = 0.0;
                info_ = MediaInfo{};
                infoDirty_ = true;
                apply(PlaybackEvent::FileStarted);
                Q_EMIT durationChanged(0.0);
                Q_EMIT positionChanged(0.0);
                break;
            case MPV_EVENT_FILE_LOADED:
                qCInfo(lcMpv).noquote() << "loaded" << lastPath_;
                apply(PlaybackEvent::FileLoaded);
                break;
            case MPV_EVENT_PLAYBACK_RESTART:  // a seek finished: report the exact landing spot
                lastEmittedPosition_ = position_;
                Q_EMIT positionChanged(position_);
                break;
            case MPV_EVENT_END_FILE: {
                const auto* end = static_cast<const mpv_event_end_file*>(ev->data);
                if (end->reason == MPV_END_FILE_REASON_ERROR) {
                    fail(QString::fromUtf8(mpv_error_string(end->error)));
                } else if (end->reason == MPV_END_FILE_REASON_EOF) {
                    apply(PlaybackEvent::EofReached);
                } else if (end->reason == MPV_END_FILE_REASON_STOP || end->reason == MPV_END_FILE_REASON_QUIT) {
                    apply(PlaybackEvent::Stopped);
                }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
                handlePropertyChange(*static_cast<const mpv_event_property*>(ev->data), ev->reply_userdata);
                break;
            case MPV_EVENT_LOG_MESSAGE: {
                const auto* msg = static_cast<const mpv_event_log_message*>(ev->data);
                const QString text = QString::fromUtf8(msg->text).trimmed();
                if (msg->log_level <= MPV_LOG_LEVEL_ERROR) {
                    qCCritical(lcMpv).noquote() << msg->prefix << text;
                } else {
                    qCWarning(lcMpv).noquote() << msg->prefix << text;
                }
                break;
            }
            case MPV_EVENT_COMMAND_REPLY:
            case MPV_EVENT_SET_PROPERTY_REPLY:
                // Expected while nothing is loaded (e.g. a seek during loading); real failures
                // surface as END_FILE errors.
                if (ev->error < 0) qCDebug(lcMpv) << "async request failed:" << mpv_error_string(ev->error);
                break;
            default: break;
        }
    }
    if (infoDirty_) {
        infoDirty_ = false;
        if (info_.width > 0) {
            qCInfo(lcMpv).noquote() << "media:" << info_.width << "x" << info_.height << info_.videoCodec << "/"
                                    << info_.audioCodec << "fps" << info_.fps << "hwdec"
                                    << (info_.hwdec.isEmpty() ? QStringLiteral("no") : info_.hwdec);
        }
        Q_EMIT mediaInfoChanged(info_);
    }
}

void MpvEngine::handlePropertyChange(const mpv_event_property& prop, quint64 id) {
    const bool has = prop.format != MPV_FORMAT_NONE && prop.data != nullptr;  // NONE: unavailable
    const auto asDouble = [&] { return has ? *static_cast<const double*>(prop.data) : 0.0; };
    const auto asFlag = [&] { return has && *static_cast<const int*>(prop.data) != 0; };
    const auto asInt = [&] { return has ? static_cast<int>(*static_cast<const int64_t*>(prop.data)) : 0; };
    const auto asString = [&] {
        return has ? QString::fromUtf8(*static_cast<char* const*>(prop.data)) : QString();
    };

    switch (static_cast<Property>(id)) {
        case TimePos:
            position_ = asDouble();
            if (paused_ || std::abs(position_ - lastEmittedPosition_) >= kPositionStep) {
                lastEmittedPosition_ = position_;
                Q_EMIT positionChanged(position_);
            }
            break;
        case Duration:
            duration_ = asDouble();
            Q_EMIT durationChanged(duration_);
            break;
        case Pause:
            paused_ = asFlag();
            apply(PlaybackEvent::PauseChanged);
            lastEmittedPosition_ = position_;
            Q_EMIT positionChanged(position_);  // throttling may have skipped the final value
            break;
        case EofReached: apply(asFlag() ? PlaybackEvent::EofReached : PlaybackEvent::EofCleared); break;
        case Volume:
            if (const int v = static_cast<int>(std::lround(asDouble())); has && v != volume_) {
                volume_ = v;
                Q_EMIT volumeChanged(v);
            }
            break;
        case Mute:
            if (has && asFlag() != muted_) {
                muted_ = asFlag();
                Q_EMIT mutedChanged(muted_);
            }
            break;
        case Width: info_.width = asInt(); infoDirty_ = true; break;
        case Height: info_.height = asInt(); infoDirty_ = true; break;
        case Fps: info_.fps = asDouble(); infoDirty_ = true; break;
        case VideoFormat: info_.videoCodec = asString(); infoDirty_ = true; break;
        case AudioCodec: info_.audioCodec = asString(); infoDirty_ = true; break;
        case Hwdec: {
            const QString hw = asString();
            info_.hwdec = hw == QLatin1String("no") ? QString() : hw;
            infoDirty_ = true;
            break;
        }
        case Title: info_.title = asString(); infoDirty_ = true; break;
        case Path: info_.path = asString(); infoDirty_ = true; break;
    }
}

void MpvEngine::apply(PlaybackEvent event) {
    const PlaybackState next = nextState(state_, event, paused_);
    if (next == state_) return;
    state_ = next;
    qCDebug(lcMpv) << "state" << next;
    Q_EMIT stateChanged(next);
}

void MpvEngine::fail(const QString& reason) {
    const QString message = tr("Cannot play \"%1\": %2").arg(QFileInfo(lastPath_).fileName(), reason);
    qCWarning(lcMpv).noquote() << message;
    apply(PlaybackEvent::Failed);
    Q_EMIT errorOccurred(message);
}

}  // namespace yp::mpv
