#pragma once

#include <QObject>
#include <QString>

namespace yp {
Q_NAMESPACE

enum class PlaybackState { Idle, Loading, Playing, Paused, Ended, Error };
Q_ENUM_NS(PlaybackState)

enum class SeekMode { Keyframe, Exact };
Q_ENUM_NS(SeekMode)

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

    virtual void open(const QString& path) = 0;  // starts playing when loaded
    virtual void play() = 0;                     // also restarts from Ended / reopens after stop()
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(double seconds, SeekMode mode) = 0;
    virtual void setVolume(int percent) = 0;  // 0..100
    virtual void setMuted(bool muted) = 0;

    [[nodiscard]] virtual PlaybackState state() const = 0;
    [[nodiscard]] virtual double position() const = 0;  // seconds
    [[nodiscard]] virtual double duration() const = 0;  // seconds, 0 when unknown
    [[nodiscard]] virtual int volume() const = 0;
    [[nodiscard]] virtual bool isMuted() const = 0;
    [[nodiscard]] virtual MediaInfo mediaInfo() const = 0;

Q_SIGNALS:
    void stateChanged(yp::PlaybackState state);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void volumeChanged(int percent);
    void mutedChanged(bool muted);
    void mediaInfoChanged(const yp::MediaInfo& info);
    void errorOccurred(const QString& message);
};

}  // namespace yp
