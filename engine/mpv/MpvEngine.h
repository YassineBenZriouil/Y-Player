#pragma once

#include "core/PlaybackEngine.h"
#include "core/PlaybackStateMachine.h"

#include <QHash>

#include <atomic>
#include <optional>

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
    void open(const QString& path, double startSeconds = 0.0) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(double seconds, SeekMode mode) override;
    void frameStep(bool backward) override;
    void setSpeed(double speed) override;
    void setAbLoop(double a, double b) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;
    void setAudioDelay(double seconds) override;
    void setAudioDevice(const QString& name) override;
    void setAudioChannels(const QString& layout) override;
    void selectTrack(TrackType type, int id) override;
    void addSubtitleFile(const QString& path) override;
    void setSubtitleDelay(double seconds) override;
    void setSubtitleStyle(const SubtitleStyle& style) override;
    void setAspectOverride(double ratio) override;
    void setRotation(int degrees) override;
    void setFlip(bool horizontal, bool vertical) override;
    void seekChapter(int index) override;
    void screenshot(const QString& path, bool withSubtitles) override;
    void showMessage(const QString& text, int milliseconds = 1500) override;
    void applySettings(const EngineSettings& settings) override;

    [[nodiscard]] PlaybackState state() const override { return state_; }
    [[nodiscard]] double position() const override { return position_; }
    [[nodiscard]] double duration() const override { return duration_; }
    [[nodiscard]] double speed() const override { return speed_; }
    [[nodiscard]] int volume() const override { return volume_; }
    [[nodiscard]] bool isMuted() const override { return muted_; }
    [[nodiscard]] double audioDelay() const override { return audioDelay_; }
    [[nodiscard]] double subtitleDelay() const override { return subtitleDelay_; }
    [[nodiscard]] QString audioDevice() const override { return audioDevice_; }
    [[nodiscard]] QList<AudioDevice> audioDevices() const override { return audioDevices_; }
    [[nodiscard]] QList<Track> tracks() const override { return tracks_; }
    [[nodiscard]] QList<Chapter> chapters() const override { return chapters_; }
    [[nodiscard]] int currentChapter() const override { return currentChapter_; }
    [[nodiscard]] MediaInfo mediaInfo() const override { return info_; }
    [[nodiscard]] PlaybackStats stats() const override;
    [[nodiscard]] QList<std::pair<QString, QString>> diagnostics() const override;

private:
    enum class Request { SubtitleFile, Screenshot };
    struct Pending {
        Request kind = Request::SubtitleFile;
        QString path;
    };

    void drainEvents();
    void handlePropertyChange(const mpv_event_property& prop, quint64 id);
    void handleReply(quint64 id, int error);
    void handleDeviceList(const QList<AudioDevice>& devices);
    void apply(PlaybackEvent event);
    void fail(const QString& reason);
    void setPause(bool paused);
    void rebuildVideoFilters();
    [[nodiscard]] QString sharpenShaderPath();
    void setString(const char* name, const QByteArray& value);
    void setDouble(const char* name, double value);
    void command(std::initializer_list<QByteArray> args, quint64 replyId = 0);
    quint64 track(Request kind, const QString& path);
    [[nodiscard]] bool ready() const;

    mpv_handle* mpv_ = nullptr;
    bool initialized_ = false;
    std::atomic<bool> drainQueued_{false};
    quint64 nextReplyId_ = 1;
    QHash<quint64, Pending> pending_;

    PlaybackState state_ = PlaybackState::Idle;
    bool paused_ = false;
    double position_ = 0.0;
    double lastEmittedPosition_ = -1.0;
    double duration_ = 0.0;
    double speed_ = 1.0;
    int volume_ = 100;
    bool muted_ = false;
    double audioDelay_ = 0.0;
    double subtitleDelay_ = 0.0;
    QString audioDevice_ = QStringLiteral("auto");
    QList<AudioDevice> audioDevices_;
    QList<Track> tracks_;
    QList<Chapter> chapters_;
    int currentChapter_ = -1;
    bool flipH_ = false;
    bool flipV_ = false;
    int denoise_ = 0;
    QString vfChain_;
    QString afChain_;
    QString shaderPath_;
    std::optional<EngineSettings> applied_;
    MediaInfo info_;
    bool infoDirty_ = false;
    QString lastPath_;
};

}  // namespace yp::mpv
