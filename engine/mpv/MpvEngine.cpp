#include "engine/mpv/MpvEngine.h"

#include "core/Adjustments.h"
#include "core/TimeFormat.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QVariant>

#include <mpv/client.h>

#include <algorithm>
#include <cmath>
#include <thread>
#include <utility>
#include <vector>

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
    Speed,
    AudioDelayProp,
    SubDelay,
    TrackList,
    ChapterList,
    ChapterProp,
    AudioDeviceList,
    AudioDeviceProp,
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
    {Speed, "speed", MPV_FORMAT_DOUBLE},
    {AudioDelayProp, "audio-delay", MPV_FORMAT_DOUBLE},
    {SubDelay, "sub-delay", MPV_FORMAT_DOUBLE},
    {TrackList, "track-list", MPV_FORMAT_NODE},
    {ChapterList, "chapter-list", MPV_FORMAT_NODE},
    {ChapterProp, "chapter", MPV_FORMAT_INT64},
    {AudioDeviceList, "audio-device-list", MPV_FORMAT_NODE},
    {AudioDeviceProp, "audio-device", MPV_FORMAT_STRING},
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
    {"screenshot-format", "png"},
    {"osd-font", "Segoe UI"},
    {"osd-font-size", "30"},  // brand OSD (ADR-018): light text, dark outline, readable on any frame
    {"osd-color", "#E6E8EC"},
    {"osd-border-color", "#0E0F12"},
    {"osd-border-size", "2"},
};

constexpr double kPositionStep = 0.1;  // seconds; throttles positionChanged during playback

QVariant toVariant(const mpv_node& node) {
    switch (node.format) {
        case MPV_FORMAT_STRING: return QString::fromUtf8(node.u.string);
        case MPV_FORMAT_FLAG: return node.u.flag != 0;
        case MPV_FORMAT_INT64: return static_cast<qlonglong>(node.u.int64);
        case MPV_FORMAT_DOUBLE: return node.u.double_;
        case MPV_FORMAT_NODE_ARRAY: {
            QVariantList list;
            for (int i = 0; i < node.u.list->num; ++i) list.append(toVariant(node.u.list->values[i]));
            return list;
        }
        case MPV_FORMAT_NODE_MAP: {
            QVariantMap map;
            for (int i = 0; i < node.u.list->num; ++i) {
                map.insert(QString::fromUtf8(node.u.list->keys[i]), toVariant(node.u.list->values[i]));
            }
            return map;
        }
        default: return {};
    }
}

TrackType trackType(const QString& mpvType) {
    if (mpvType == QLatin1String("audio")) return TrackType::Audio;
    if (mpvType == QLatin1String("sub")) return TrackType::Subtitle;
    return TrackType::Video;
}

const char* trackProperty(TrackType type) {
    switch (type) {
        case TrackType::Audio: return "aid";
        case TrackType::Subtitle: return "sid";
        case TrackType::Video: break;
    }
    return "vid";
}

}  // namespace

MpvEngine::MpvEngine(QObject* parent) : IPlaybackEngine(parent) {
    mpv_ = mpv_create();
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
    // mpv's teardown calls unmatched CoUninitialize() on its calling thread (measured, ADR-011).
    // On the UI thread that destroys Qt's OLE apartment and ~QApplication later crashes in
    // Windows.UI.dll. Run it on a throwaway thread instead -- still synchronous, so the video
    // window outlives mpv. Regression test: tst_mpvengine::teardownIsFastAndKeepsComApartment.
    std::thread([handle = mpv_] { mpv_terminate_destroy(handle); }).join();
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

void MpvEngine::command(std::initializer_list<QByteArray> args, quint64 replyId) {
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const QByteArray& a : args) argv.push_back(a.constData());
    argv.push_back(nullptr);
    mpv_command_async(mpv_, replyId, argv.data());  // mpv copies the arguments
}

void MpvEngine::setString(const char* name, const QByteArray& value) {
    const char* data = value.constData();
    mpv_set_property_async(mpv_, 0, name, MPV_FORMAT_STRING, static_cast<void*>(&data));
}

void MpvEngine::setDouble(const char* name, double value) {
    mpv_set_property_async(mpv_, 0, name, MPV_FORMAT_DOUBLE, &value);
}

void MpvEngine::setPause(bool paused) {
    int flag = paused ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

quint64 MpvEngine::track(Request kind, const QString& path) {
    const quint64 id = nextReplyId_++;
    pending_.insert(id, {kind, path});
    return id;
}

void MpvEngine::open(const QString& path, double startSeconds) {
    lastPath_ = path;
    if (!ready()) {
        fail(tr("the playback engine is not available"));
        return;
    }
    setPause(false);  // mpv's pause flag persists across files; a new file should play
    if (startSeconds > 0.0) {
        command({"loadfile", path.toUtf8(), "replace", "-1", "start=" + QByteArray::number(startSeconds, 'f', 3)});
    } else {
        command({"loadfile", path.toUtf8(), "replace"});
    }
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
    if (ready()) command({"stop"});
}

void MpvEngine::seek(double seconds, SeekMode mode) {
    if (!ready() || state_ == PlaybackState::Idle || state_ == PlaybackState::Error) return;
    const QByteArray target = QByteArray::number(clampSeekTarget(seconds, duration_), 'f', 3);
    const QByteArray flags = mode == SeekMode::Exact ? "absolute+exact" : "absolute+keyframes";
    qCDebug(lcMpv) << "seek" << target << flags;
    command({"seek", target, flags});
}

void MpvEngine::frameStep(bool backward) {
    if (ready()) command({backward ? "frame-back-step" : "frame-step"});
}

void MpvEngine::setSpeed(double speed) {
    if (ready()) setDouble("speed", clampSpeed(speed));
}

void MpvEngine::setAbLoop(double a, double b) {
    if (!ready()) return;
    setString("ab-loop-a", a >= 0.0 ? QByteArray::number(a, 'f', 3) : QByteArray("no"));
    setString("ab-loop-b", b >= 0.0 ? QByteArray::number(b, 'f', 3) : QByteArray("no"));
}

void MpvEngine::setVolume(int percent) {
    if (ready()) setDouble("volume", std::clamp(percent, 0, 100));
}

void MpvEngine::setMuted(bool muted) {
    if (!ready()) return;
    int flag = muted ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::setAudioDelay(double seconds) {
    if (ready()) setDouble("audio-delay", stepDelay(seconds, 0.0));
}

void MpvEngine::setAudioDevice(const QString& name) {
    if (ready()) setString("audio-device", name.isEmpty() ? QByteArray("auto") : name.toUtf8());
}

void MpvEngine::setAudioChannels(const QString& layout) {
    if (ready()) setString("audio-channels", layout == QLatin1String("auto") ? QByteArray("auto-safe") : layout.toUtf8());
}

void MpvEngine::selectTrack(TrackType type, int id) {
    if (ready()) setString(trackProperty(type), id > 0 ? QByteArray::number(id) : QByteArray("no"));
}

void MpvEngine::addSubtitleFile(const QString& path) {
    if (ready()) command({"sub-add", path.toUtf8(), "select"}, track(Request::SubtitleFile, path));
}

void MpvEngine::setSubtitleDelay(double seconds) {
    if (ready()) setDouble("sub-delay", stepDelay(seconds, 0.0));
}

void MpvEngine::setSubtitleStyle(const SubtitleStyle& style) {
    if (!ready()) return;
    setString("sub-font", style.font.toUtf8());
    setString("sub-font-size", QByteArray::number(std::clamp(style.size, 8, 200)));
    setString("sub-color", style.color.toUtf8());
    setString("sub-outline-color", style.outlineColor.toUtf8());
    setDouble("sub-outline-size", std::clamp(style.outlineSize, 0.0, 20.0));
    setString("sub-pos", QByteArray::number(std::clamp(style.position, 0, 150)));
    setString("sub-ass-override", style.overrideStyled ? "force" : "scale");
}

void MpvEngine::setAspectOverride(double ratio) {
    if (ready()) setString("video-aspect-override", ratio > 0.0 ? QByteArray::number(ratio, 'f', 6) : QByteArray("-1"));
}

void MpvEngine::setRotation(int degrees) {
    if (!ready()) return;
    const int normalized = ((degrees / 90 * 90) % 360 + 360) % 360;
    setString("video-rotate", QByteArray::number(normalized));
}

void MpvEngine::setFlip(bool horizontal, bool vertical) {
    flipH_ = horizontal;
    flipV_ = vertical;
    rebuildVideoFilters();
}

void MpvEngine::rebuildVideoFilters() {
    const QString chain = videoFilterChain(flipH_, flipV_, denoise_);
    if (!ready() || chain == vfChain_) return;  // rebuilding the chain re-inits filters: only on change
    vfChain_ = chain;
    setString("vf", chain.toUtf8());  // we own the whole filter chain
}

QString MpvEngine::sharpenShaderPath() {
    if (!shaderPath_.isEmpty()) return shaderPath_;
    // Unsharp mask on luma; strength comes from glsl-shader-opts so changing it needs no reload.
    static constexpr char kShader[] = R"(//!PARAM strength
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 2.0
0.5

//!HOOK LUMA
//!BIND HOOKED
//!DESC Y-Player sharpen
vec4 hook() {
    vec4 c = HOOKED_texOff(vec2(0.0, 0.0));
    vec4 blur = (HOOKED_texOff(vec2(1.0, 0.0)) + HOOKED_texOff(vec2(-1.0, 0.0)) +
                 HOOKED_texOff(vec2(0.0, 1.0)) + HOOKED_texOff(vec2(0.0, -1.0))) * 0.25;
    return clamp(c + (c - blur) * strength, 0.0, 1.0);
}
)";
    const QString path = QDir::temp().filePath(QStringLiteral("y-player-sharpen.glsl"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(kShader) < 0 || !file.commit()) {
        qCWarning(lcMpv).noquote() << "cannot write the sharpen shader to" << path;
        return {};
    }
    shaderPath_ = path;
    return shaderPath_;
}

void MpvEngine::applySettings(const EngineSettings& s) {
    if (!ready()) return;
    const bool all = !applied_.has_value();
    const EngineSettings old = applied_.value_or(EngineSettings{});
    const auto changed = [&](auto member) { return all || s.*member != old.*member; };
    const auto yesNo = [](bool on) { return on ? QByteArray("yes") : QByteArray("no"); };

    if (changed(&EngineSettings::hwdec)) {
        setString("hwdec", s.hwdec == QLatin1String("auto") ? QByteArray("auto-safe") : s.hwdec.toUtf8());
    }
    if (changed(&EngineSettings::scaler)) setString("scale", s.scaler.toUtf8());
    if (changed(&EngineSettings::toneMapping)) setString("tone-mapping", s.toneMapping.toUtf8());
    if (changed(&EngineSettings::hdrComputePeak)) setString("hdr-compute-peak", yesNo(s.hdrComputePeak));
    if (changed(&EngineSettings::hdrPassthrough)) setString("target-colorspace-hint", yesNo(s.hdrPassthrough));
    if (changed(&EngineSettings::smoothMotion)) {
        setString("video-sync", s.smoothMotion ? "display-resample" : "audio");
        setString("interpolation", yesNo(s.smoothMotion));
    }
    if (changed(&EngineSettings::brightness)) setString("brightness", QByteArray::number(s.brightness));
    if (changed(&EngineSettings::contrast)) setString("contrast", QByteArray::number(s.contrast));
    if (changed(&EngineSettings::saturation)) setString("saturation", QByteArray::number(s.saturation));
    if (changed(&EngineSettings::gamma)) setString("gamma", QByteArray::number(s.gamma));
    if (changed(&EngineSettings::hue)) setString("hue", QByteArray::number(s.hue));
    if (changed(&EngineSettings::deinterlace)) setString("deinterlace", s.deinterlace.toUtf8());

    if (changed(&EngineSettings::sharpen)) {
        const bool wasOn = !all && old.sharpen > 0;
        const QString shader = s.sharpen > 0 ? sharpenShaderPath() : QString();
        if ((s.sharpen > 0) != wasOn || all) setString("glsl-shaders", shader.toUtf8());
        if (s.sharpen > 0) setString("glsl-shader-opts", "strength=" + QByteArray::number(s.sharpen / 50.0, 'f', 2));
    }
    denoise_ = s.denoise;
    rebuildVideoFilters();

    if (const QString af = audioFilterChain(s); all || af != afChain_) {
        afChain_ = af;
        setString("af", af.toUtf8());
    }
    if (changed(&EngineSettings::normalizeDownmix)) setString("audio-normalize-downmix", yesNo(s.normalizeDownmix));
    if (changed(&EngineSettings::audioLanguages)) setString("alang", s.audioLanguages.toUtf8());
    if (changed(&EngineSettings::subtitleLanguages)) setString("slang", s.subtitleLanguages.toUtf8());
    if (changed(&EngineSettings::subtitleAutoLoad)) setString("sub-auto", s.subtitleAutoLoad.toUtf8());
    if (changed(&EngineSettings::cacheMb)) setString("demuxer-max-bytes", QByteArray::number(s.cacheMb) + "MiB");
    if (changed(&EngineSettings::decoderThreads)) setString("vd-lavc-threads", QByteArray::number(s.decoderThreads));
    if (changed(&EngineSettings::logLevel)) mpv_request_log_messages(mpv_, s.logLevel.toUtf8().constData());
    applied_ = s;
}

QList<std::pair<QString, QString>> MpvEngine::diagnostics() const {
    struct Row {
        const char* label;
        const char* property;
    };
    static constexpr Row kRows[] = {
        {QT_TRANSLATE_NOOP("Diagnostics", "File"), "path"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Container"), "file-format"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Video codec"), "video-codec"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Hardware decoder"), "hwdec-current"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Video output"), "current-vo"},
        {QT_TRANSLATE_NOOP("Diagnostics", "GPU context"), "current-gpu-context"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Width"), "video-params/w"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Height"), "video-params/h"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Pixel format"), "video-params/pixelformat"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Hardware surface format"), "video-params/hw-pixelformat"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Color matrix"), "video-params/colormatrix"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Primaries (source)"), "video-params/primaries"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Transfer (source)"), "video-params/gamma"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Primaries (output)"), "video-target-params/primaries"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Transfer (output)"), "video-target-params/gamma"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Container FPS"), "container-fps"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Measured FPS"), "estimated-vf-fps"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Display FPS"), "display-fps"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Video bitrate"), "video-bitrate"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Audio codec"), "audio-codec"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Sample rate"), "audio-params/samplerate"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Channels"), "audio-params/channels"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Sample format"), "audio-params/format"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Audio output"), "current-ao"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Audio device"), "audio-device"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Audio bitrate"), "audio-bitrate"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Dropped frames (output)"), "frame-drop-count"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Dropped frames (decoder)"), "decoder-frame-drop-count"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Delayed frames"), "vo-delayed-frame-count"},
        {QT_TRANSLATE_NOOP("Diagnostics", "A/V offset (s)"), "avsync"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Buffered ahead (s)"), "demuxer-cache-duration"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Upscaler"), "scale"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Tone mapping"), "tone-mapping"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Video filters"), "vf"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Audio filters"), "af"},
        {QT_TRANSLATE_NOOP("Diagnostics", "Shaders"), "glsl-shaders"},
    };
    QList<std::pair<QString, QString>> rows;
    for (const Row& r : kRows) {
        char* value = ready() ? mpv_get_property_osd_string(mpv_, r.property) : nullptr;
        const QString text = value != nullptr && *value != '\0' ? QString::fromUtf8(value) : QStringLiteral("—");
        mpv_free(value);
        rows.append({QCoreApplication::translate("Diagnostics", r.label), text});
    }
    return rows;
}

void MpvEngine::seekChapter(int index) {
    if (!ready() || index < 0 || index >= chapters_.size()) return;
    setString("chapter", QByteArray::number(index));
}

void MpvEngine::screenshot(const QString& path, bool withSubtitles) {
    if (!ready()) return;
    command({"screenshot-to-file", path.toUtf8(), withSubtitles ? "subtitles" : "video"},
            track(Request::Screenshot, path));
}

void MpvEngine::showMessage(const QString& text, int milliseconds) {
    if (ready()) command({"show-text", text.toUtf8(), QByteArray::number(milliseconds)});
}

PlaybackStats MpvEngine::stats() const {
    PlaybackStats s;
    if (!ready()) return s;
    const auto getInt = [this](const char* name, qint64& out) {
        int64_t v = 0;
        if (mpv_get_property(mpv_, name, MPV_FORMAT_INT64, &v) >= 0) out = v;
    };
    const auto getDouble = [this](const char* name, double& out) {
        double v = 0.0;
        if (mpv_get_property(mpv_, name, MPV_FORMAT_DOUBLE, &v) >= 0) out = v;
    };
    getInt("frame-drop-count", s.droppedFrames);
    getInt("decoder-frame-drop-count", s.decoderDroppedFrames);
    getInt("vo-delayed-frame-count", s.delayedFrames);
    getInt("mistimed-frame-count", s.mistimedFrames);
    getDouble("avsync", s.avsync);
    getDouble("vsync-jitter", s.vsyncJitter);
    getDouble("display-fps", s.displayFps);
    getDouble("estimated-vf-fps", s.estimatedVideoFps);
    return s;
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
            case MPV_EVENT_PLAYBACK_RESTART:  // first frame after load/seek: report the exact spot
                qCDebug(lcMpv) << "playback restart at" << position_;
                lastEmittedPosition_ = position_;
                Q_EMIT positionChanged(position_);
                Q_EMIT playbackRestarted();
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
            case MPV_EVENT_SET_PROPERTY_REPLY: handleReply(ev->reply_userdata, ev->error); break;
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

void MpvEngine::handleReply(quint64 id, int error) {
    if (!pending_.contains(id)) {
        // Expected while nothing is loaded (e.g. a seek during loading); real playback failures
        // surface as END_FILE errors.
        if (error < 0) qCDebug(lcMpv) << "async request failed:" << mpv_error_string(error);
        return;
    }
    const Pending p = pending_.take(id);
    const QString name = QFileInfo(p.path).fileName();
    if (p.kind == Request::Screenshot) {
        if (error < 0) {
            Q_EMIT errorOccurred(tr("Screenshot failed: %1").arg(QString::fromUtf8(mpv_error_string(error))));
        } else {
            Q_EMIT screenshotSaved(p.path);
        }
    } else if (error < 0) {
        Q_EMIT errorOccurred(tr("Cannot load subtitles \"%1\": %2").arg(name, QString::fromUtf8(mpv_error_string(error))));
    }
}

void MpvEngine::handleDeviceList(const QList<AudioDevice>& devices) {
    audioDevices_ = devices;
    Q_EMIT audioDevicesChanged();
    if (audioDevice_ == QLatin1String("auto")) return;
    const bool present = std::ranges::any_of(devices, [&](const AudioDevice& d) { return d.name == audioDevice_; });
    if (!present) {  // unplugged: fall back to the system default rather than going silent
        qCWarning(lcMpv).noquote() << "audio device disappeared:" << audioDevice_;
        setAudioDevice(QStringLiteral("auto"));
        Q_EMIT notice(tr("Audio device disconnected; switched to the system default."));
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
    const auto asList = [&] { return has ? toVariant(*static_cast<const mpv_node*>(prop.data)).toList() : QVariantList(); };

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
        case Speed:
            if (has) {
                speed_ = asDouble();
                Q_EMIT speedChanged(speed_);
            }
            break;
        case AudioDelayProp:
            audioDelay_ = asDouble();
            Q_EMIT audioDelayChanged(audioDelay_);
            break;
        case SubDelay:
            subtitleDelay_ = asDouble();
            Q_EMIT subtitleDelayChanged(subtitleDelay_);
            break;
        case TrackList: {
            tracks_.clear();
            for (const QVariant& v : asList()) {
                const QVariantMap m = v.toMap();
                tracks_.append({.id = m.value(QStringLiteral("id")).toInt(),
                                .type = trackType(m.value(QStringLiteral("type")).toString()),
                                .title = m.value(QStringLiteral("title")).toString(),
                                .language = m.value(QStringLiteral("lang")).toString(),
                                .codec = m.value(QStringLiteral("codec")).toString(),
                                .external = m.value(QStringLiteral("external")).toBool(),
                                .selected = m.value(QStringLiteral("selected")).toBool(),
                                .isDefault = m.value(QStringLiteral("default")).toBool()});
            }
            Q_EMIT tracksChanged();
            break;
        }
        case ChapterList:
            chapters_.clear();
            for (const QVariant& v : asList()) {
                const QVariantMap m = v.toMap();
                chapters_.append({m.value(QStringLiteral("title")).toString(), m.value(QStringLiteral("time")).toDouble()});
            }
            Q_EMIT chaptersChanged();
            break;
        case ChapterProp:
            currentChapter_ = has ? asInt() : -1;
            Q_EMIT chaptersChanged();
            break;
        case AudioDeviceList: {
            QList<AudioDevice> devices;
            for (const QVariant& v : asList()) {
                const QVariantMap m = v.toMap();
                devices.append({m.value(QStringLiteral("name")).toString(), m.value(QStringLiteral("description")).toString()});
            }
            handleDeviceList(devices);
            break;
        }
        case AudioDeviceProp:
            audioDevice_ = has ? asString() : QStringLiteral("auto");
            Q_EMIT audioDevicesChanged();
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
