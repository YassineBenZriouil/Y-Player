#include "app/BenchMode.h"

#include "platform/win/SystemInfo.h"
#include "ui/MainWindow.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QSaveFile>
#include <QWindow>

#include <cmath>
#include <utility>

namespace yp::app {
namespace {

constexpr int kAvsyncSampleMs = 100;
constexpr int kSeekGapMs = 400;
constexpr int kSeekTimeoutMs = 5000;
constexpr int kWatchdogMs = 120000;

struct SeekStep {
    double fraction;
    SeekMode mode;
};
constexpr SeekStep kSeeks[] = {{0.1, SeekMode::Exact},    {0.5, SeekMode::Exact},    {0.9, SeekMode::Exact},
                               {0.1, SeekMode::Keyframe}, {0.5, SeekMode::Keyframe}, {0.9, SeekMode::Keyframe}};

QJsonValue delta(qint64 from, qint64 to) { return from < 0 || to < 0 ? QJsonValue() : QJsonValue(to - from); }
QJsonValue orNull(double v) { return v < 0 ? QJsonValue() : QJsonValue(v); }

}  // namespace

BenchRun::BenchRun(ui::MainWindow& window, BenchOptions options)
    : QObject(&window), window_(window), engine_(window.engine()), options_(std::move(options)) {
    qApp->installEventFilter(this);
    avsyncSampler_.setInterval(kAvsyncSampleMs);
    connect(&avsyncSampler_, &QTimer::timeout, this, [this] {
        avsyncMaxAbs_ = std::max(avsyncMaxAbs_, std::abs(engine_.stats().avsync));
        ++avsyncSamples_;
    });
    seekTimeout_.setSingleShot(true);
    connect(&seekTimeout_, &QTimer::timeout, this, [this] {  // latency stays null: seek never landed
        seekClock_.invalidate();
        nextSeek();
    });
    connect(&engine_, &IPlaybackEngine::playbackRestarted, this, &BenchRun::onRestarted);
    connect(&engine_, &IPlaybackEngine::errorOccurred, this, [this](const QString& message) {
        if (firstFrameMs_ < 0) finish(message);  // later errors (damaged media) are part of the result
    });
    QTimer::singleShot(kWatchdogMs, this, [this] { finish(QStringLiteral("watchdog timeout")); });
}

bool BenchRun::eventFilter(QObject* watched, QEvent* event) {
    if (startupMs_ < 0 && event->type() == QEvent::Expose && watched == window_.windowHandle() &&
        window_.windowHandle()->isExposed()) {
        startupMs_ = platform::processAgeMs();  // the window is on screen
        qApp->removeEventFilter(this);
    }
    return QObject::eventFilter(watched, event);
}

void BenchRun::start() {
    if (!QFileInfo(options_.file).isFile()) {
        finish(QStringLiteral("file not found: ") + options_.file);
        return;
    }
    openAtMs_ = platform::processAgeMs();
    window_.openFile(options_.file);
}

void BenchRun::onRestarted() {
    if (firstFrameMs_ < 0) {
        firstFrameMs_ = platform::processAgeMs();
        QTimer::singleShot(options_.warmupSeconds * 1000, this, &BenchRun::beginMeasureWindow);
        return;
    }
    if (!seekClock_.isValid()) return;  // a restart we did not ask for
    seekTimeout_.stop();
    const qsizetype last = seeks_.size() - 1;
    QJsonObject entry = seeks_.at(last).toObject();
    entry[QStringLiteral("latency_ms")] = static_cast<double>(seekClock_.nsecsElapsed()) / 1e6;
    seeks_[last] = entry;  // QJsonArray::last() returns a copy; operator[] writes through
    seekClock_.invalidate();
    nextSeek();
}

void BenchRun::beginMeasureWindow() {
    windowStart_ = engine_.stats();
    avsyncSampler_.start();
    QTimer::singleShot(options_.playSeconds * 1000, this, &BenchRun::endMeasureWindow);
}

void BenchRun::endMeasureWindow() {
    windowEnd_ = engine_.stats();
    avsyncSampler_.stop();
    nextSeek();
}

void BenchRun::nextSeek() {
    if (++seekIndex_ >= std::ssize(kSeeks)) {
        finish();
        return;
    }
    QTimer::singleShot(kSeekGapMs, this, [this] {
        const SeekStep step = kSeeks[seekIndex_];
        const double target = step.fraction * engine_.duration();
        seeks_.append(QJsonObject{{QStringLiteral("target_s"), target},
                                  {QStringLiteral("mode"), step.mode == SeekMode::Exact ? QStringLiteral("exact")
                                                                                        : QStringLiteral("keyframe")},
                                  {QStringLiteral("latency_ms"), QJsonValue()}});
        seekClock_.start();
        seekTimeout_.start(kSeekTimeoutMs);
        engine_.seek(target, step.mode);
    });
}

void BenchRun::finish(const QString& error) {
    if (finished_) return;
    finished_ = true;
    const MediaInfo info = engine_.mediaInfo();
    QJsonObject r{
        {QStringLiteral("app_version"), QStringLiteral(YP_VERSION)},
        {QStringLiteral("file"), options_.file},
        {QStringLiteral("startup_ms"), startupMs_},
        {QStringLiteral("ttff_ms"), firstFrameMs_},
        {QStringLiteral("open_to_first_frame_ms"), firstFrameMs_ < 0 ? QJsonValue() : QJsonValue(firstFrameMs_ - openAtMs_)},
        {QStringLiteral("media"), QJsonObject{{QStringLiteral("width"), info.width},
                                              {QStringLiteral("height"), info.height},
                                              {QStringLiteral("fps"), info.fps},
                                              {QStringLiteral("video_codec"), info.videoCodec},
                                              {QStringLiteral("audio_codec"), info.audioCodec},
                                              {QStringLiteral("hwdec"), info.hwdec}}},
        {QStringLiteral("window_seconds"), options_.playSeconds},
        {QStringLiteral("dropped_frames"), delta(windowStart_.droppedFrames, windowEnd_.droppedFrames)},
        {QStringLiteral("decoder_dropped_frames"),
         delta(windowStart_.decoderDroppedFrames, windowEnd_.decoderDroppedFrames)},
        {QStringLiteral("delayed_frames"), delta(windowStart_.delayedFrames, windowEnd_.delayedFrames)},
        {QStringLiteral("mistimed_frames"), delta(windowStart_.mistimedFrames, windowEnd_.mistimedFrames)},
        {QStringLiteral("avsync_max_abs_ms"), avsyncSamples_ > 0 ? QJsonValue(avsyncMaxAbs_ * 1000.0) : QJsonValue()},
        {QStringLiteral("vsync_jitter"), orNull(windowEnd_.vsyncJitter)},
        {QStringLiteral("display_fps"), windowEnd_.displayFps},
        {QStringLiteral("estimated_video_fps"), windowEnd_.estimatedVideoFps},
        {QStringLiteral("seeks"), seeks_},
        {QStringLiteral("final_state"),
         QString::fromLatin1(QMetaEnum::fromType<PlaybackState>().valueToKey(static_cast<int>(engine_.state())))},
        {QStringLiteral("error"), error.isEmpty() ? QJsonValue() : QJsonValue(error)},
    };
    QSaveFile out(options_.reportPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(r).toJson());
        out.commit();
    }
    QCoreApplication::exit(error.isEmpty() ? 0 : 2);
}

}  // namespace yp::app
