#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace yp {
Q_NAMESPACE

enum class PlaybackState { Idle, Loading, Playing, Paused, Ended, Error };
Q_ENUM_NS(PlaybackState)

enum class SeekMode { Keyframe, Exact };
Q_ENUM_NS(SeekMode)

enum class TrackType { Video, Audio, Subtitle };
Q_ENUM_NS(TrackType)

// What is actually playing, as reported by the backend (not what was requested).
struct MediaInfo {
    QString path;
    QString title;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    QString videoCodec;
    QString audioCodec;
    QString hwdec;  // active hardware decoder, empty when decoding in software
};

struct Track {
    int id = 0;  // backend id, unique per type; 0 is never a valid id ("off")
    TrackType type = TrackType::Video;
    QString title;
    QString language;
    QString codec;
    bool external = false;  // loaded from a separate file
    bool selected = false;
    bool isDefault = false;
};

struct Chapter {
    QString title;
    double time = 0.0;  // seconds
};

struct AudioDevice {
    QString name;  // backend id; "auto" follows the system default device
    QString description;
};

// Subtitle look. Colors are "#RRGGBB". Styled (ASS/SSA) subtitles keep their own styles unless
// overrideStyled is set.
struct SubtitleStyle {
    QString font = QStringLiteral("Segoe UI");
    int size = 46;               // scaled to a 720-line reference height
    QString color = QStringLiteral("#FFFFFF");
    QString outlineColor = QStringLiteral("#000000");
    double outlineSize = 2.5;
    int position = 100;          // 0 = top, 100 = bottom edge of the video
    bool overrideStyled = false;
};

// Live counters for diagnostics and benchmarks. -1 / negative = not reported by the backend
// in the current mode (e.g. jitter and mistimed frames need display-synced presentation).
struct PlaybackStats {
    qint64 droppedFrames = -1;         // dropped by the video output (too late to present)
    qint64 decoderDroppedFrames = -1;  // dropped by the decoder to catch up
    qint64 delayedFrames = -1;         // presented later than scheduled
    qint64 mistimedFrames = -1;
    double avsync = 0.0;  // current audio/video offset, seconds
    double vsyncJitter = -1.0;
    double displayFps = 0.0;
    double estimatedVideoFps = 0.0;
};

// The only way the rest of the app talks to a media backend (ADR-001).
// Threading contract: call every method on the thread that owns the engine (the UI thread);
// implementations marshal backend callbacks onto that thread before emitting signals.
class IPlaybackEngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    // Native window the backend presents into (an HWND on Windows, ADR-003). Call once,
    // before open(); the window must outlive the engine.
    virtual bool attachVideoOutput(quintptr nativeWindow) = 0;

    // Starts playing when loaded, at `startSeconds` if > 0.
    virtual void open(const QString& path, double startSeconds = 0.0) = 0;
    virtual void play() = 0;  // also restarts from Ended / reopens after stop()
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(double seconds, SeekMode mode) = 0;
    virtual void frameStep(bool backward) = 0;  // pauses, then shows the next/previous frame
    virtual void setSpeed(double speed) = 0;
    virtual void setAbLoop(double a, double b) = 0;  // a/b < 0: unset

    virtual void setVolume(int percent) = 0;  // 0..100
    virtual void setMuted(bool muted) = 0;
    virtual void setAudioDelay(double seconds) = 0;
    virtual void setAudioDevice(const QString& name) = 0;
    virtual void setAudioChannels(const QString& layout) = 0;  // "auto", "stereo", "5.1", "7.1", "mono"

    virtual void selectTrack(TrackType type, int id) = 0;  // id 0 = off (subtitles, audio)
    virtual void addSubtitleFile(const QString& path) = 0;  // loads and selects it
    virtual void setSubtitleDelay(double seconds) = 0;
    virtual void setSubtitleStyle(const SubtitleStyle& style) = 0;

    virtual void setAspectOverride(double ratio) = 0;  // width/height; <= 0 = use the file's
    virtual void setRotation(int degrees) = 0;        // 0, 90, 180, 270 (clockwise)
    // Mirroring runs as a CPU filter: with hardware decoding it forces a GPU->CPU copy per frame.
    virtual void setFlip(bool horizontal, bool vertical) = 0;

    virtual void seekChapter(int index) = 0;
    // Saves the current frame as PNG; result arrives via screenshotSaved() or errorOccurred().
    virtual void screenshot(const QString& path, bool withSubtitles) = 0;
    virtual void showMessage(const QString& text, int milliseconds = 1500) = 0;  // on-video OSD

    [[nodiscard]] virtual PlaybackState state() const = 0;
    [[nodiscard]] virtual double position() const = 0;  // seconds
    [[nodiscard]] virtual double duration() const = 0;  // seconds, 0 when unknown
    [[nodiscard]] virtual double speed() const = 0;
    [[nodiscard]] virtual int volume() const = 0;
    [[nodiscard]] virtual bool isMuted() const = 0;
    [[nodiscard]] virtual double audioDelay() const = 0;
    [[nodiscard]] virtual double subtitleDelay() const = 0;
    [[nodiscard]] virtual QString audioDevice() const = 0;
    [[nodiscard]] virtual QList<AudioDevice> audioDevices() const = 0;
    [[nodiscard]] virtual QList<Track> tracks() const = 0;
    [[nodiscard]] virtual QList<Chapter> chapters() const = 0;
    [[nodiscard]] virtual int currentChapter() const = 0;  // -1 = none
    [[nodiscard]] virtual MediaInfo mediaInfo() const = 0;
    [[nodiscard]] virtual PlaybackStats stats() const = 0;  // queried on demand; not free, don't poll per frame

Q_SIGNALS:
    void stateChanged(yp::PlaybackState state);
    // Playback (re)started at a new position: the first frame after loading, or after a seek.
    void playbackRestarted();
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void speedChanged(double speed);
    void volumeChanged(int percent);
    void mutedChanged(bool muted);
    void audioDelayChanged(double seconds);
    void subtitleDelayChanged(double seconds);
    void tracksChanged();
    void chaptersChanged();
    void audioDevicesChanged();
    void mediaInfoChanged(const yp::MediaInfo& info);
    void screenshotSaved(const QString& path);
    void notice(const QString& message);  // non-fatal information worth showing the user
    void errorOccurred(const QString& message);
};

}  // namespace yp
