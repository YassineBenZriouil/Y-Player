#pragma once

#include "core/PlaybackEngine.h"
#include "core/PlaybackStateMachine.h"

#include <atomic>

struct mpv_handle;
struct mpv_event_property;

namespace yp::mpv {

// libmpv-backed IPlaybackEngine. mpv presents into the attached native window with
// vo=gpu-next / gpu-api=d3d11 / hwdec=auto-safe (ADR-003). mpv runs its own threads; its
// wakeup callback only queues a drain of the event queue onto this object's thread.
class MpvEngine final : public IPlaybackEngine {
    Q_OBJECT
public:
    explicit MpvEngine(QObject* parent = nullptr);
    ~MpvEngine() override;  // terminates mpv; the attached window must still exist
    MpvEngine(const MpvEngine&) = delete;
    MpvEngine& operator=(const MpvEngine&) = delete;

    bool attachVideoOutput(quintptr nativeWindow) override;
    void open(const QString& path) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(double seconds, SeekMode mode) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;

    [[nodiscard]] PlaybackState state() const override { return state_; }
    [[nodiscard]] double position() const override { return position_; }
    [[nodiscard]] double duration() const override { return duration_; }
    [[nodiscard]] int volume() const override { return volume_; }
    [[nodiscard]] bool isMuted() const override { return muted_; }
    [[nodiscard]] MediaInfo mediaInfo() const override { return info_; }

private:
    void drainEvents();
    void handlePropertyChange(const mpv_event_property& prop, quint64 id);
    void apply(PlaybackEvent event);
    void fail(const QString& message);
    void setPause(bool paused);
    [[nodiscard]] bool ready() const;

    mpv_handle* mpv_ = nullptr;
    bool initialized_ = false;
    std::atomic<bool> drainQueued_{false};

    PlaybackState state_ = PlaybackState::Idle;
    bool paused_ = false;
    double position_ = 0.0;
    double lastEmittedPosition_ = -1.0;
    double duration_ = 0.0;
    int volume_ = 100;
    bool muted_ = false;
    MediaInfo info_;
    bool infoDirty_ = false;
    QString lastPath_;
};

}  // namespace yp::mpv
