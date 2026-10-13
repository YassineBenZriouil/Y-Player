// Integration tests: the real libmpv engine presenting into a small native window, muted.
// Media: tests/media (regenerate with tests/media/generate.sh).

#include "engine/mpv/MpvEngine.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

#include <windows.h>
#include <objbase.h>
#include <tlhelp32.h>

#include <memory>

using yp::PlaybackState;
using yp::SeekMode;
using yp::mpv::MpvEngine;

namespace {

constexpr int kLoadTimeoutMs = 15000;

QString media(const char* name) { return QDir(QStringLiteral(YP_TEST_MEDIA_DIR)).filePath(QString::fromLatin1(name)); }

DWORD handleCount() {
    DWORD n = 0;
    GetProcessHandleCount(GetCurrentProcess(), &n);
    return n;
}

int threadCount() {
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te{};
    te.dwSize = sizeof te;
    int n = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID == GetCurrentProcessId()) ++n;
    }
    CloseHandle(snap);
    return n;
}

}  // namespace

class TestMpvEngine : public QObject {
    Q_OBJECT

    // Declared before the engine so it is destroyed after it (the engine presents into it).
    std::unique_ptr<QWidget> window_;
    std::unique_ptr<MpvEngine> engine_;

    bool createEngine() {
        if (!window_) {
            window_ = std::make_unique<QWidget>();
            window_->setAttribute(Qt::WA_NativeWindow);
            window_->resize(320, 240);
            window_->show();
        }
        engine_ = std::make_unique<MpvEngine>();
        if (!engine_->attachVideoOutput(static_cast<quintptr>(window_->winId()))) return false;
        engine_->setMuted(true);
        return true;
    }

    [[nodiscard]] int countTracks(yp::TrackType type) const {
        int n = 0;
        for (const yp::Track& t : engine_->tracks()) n += t.type == type ? 1 : 0;
        return n;
    }

    template <typename Pred>
    [[nodiscard]] int findTrack(yp::TrackType type, Pred pred) const {
        for (const yp::Track& t : engine_->tracks()) {
            if (t.type == type && pred(t)) return t.id;
        }
        return 0;
    }

    [[nodiscard]] int selectedTrack(yp::TrackType type) const {
        return findTrack(type, [](const yp::Track& t) { return t.selected; });
    }

    bool openAndWaitPlaying(const QString& path) {
        engine_->open(path);
        return QTest::qWaitFor([&] { return engine_->state() == PlaybackState::Playing; }, kLoadTimeoutMs);
    }

private Q_SLOTS:
    void init() {
        if (!createEngine()) QSKIP("no D3D11 video output in this environment (not an engine failure)");
    }

    void cleanup() { engine_.reset(); }

    void cleanupTestCase() { window_.reset(); }

    void playsEachCodec_data() {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("video");
        QTest::addColumn<QString>("audio");
        QTest::newRow("H.264/AAC in MP4") << media("h264_aac.mp4") << "h264" << "aac";
        QTest::newRow("HEVC 10-bit/AAC in MKV") << media("hevc10_aac.mkv") << "hevc" << "aac";
        QTest::newRow("VP9/Opus in WebM") << media("vp9_opus.webm") << "vp9" << "opus";
        QTest::newRow("AV1 10-bit/Opus in MKV") << media("av1_opus.mkv") << "av1" << "opus";
    }

    void playsEachCodec() {
        QFETCH(QString, file);
        QFETCH(QString, video);
        QFETCH(QString, audio);
        QVERIFY(QFile::exists(file));
        QVERIFY(openAndWaitPlaying(file));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->position() > 0.5, 5000);  // time actually advances
        QTRY_COMPARE_WITH_TIMEOUT(engine_->mediaInfo().videoCodec, video, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine_->mediaInfo().audioCodec, audio, 5000);
        QCOMPARE(engine_->mediaInfo().width, 320);
        QCOMPARE(engine_->mediaInfo().height, 240);
        QVERIFY2(qAbs(engine_->duration() - 3.0) < 0.1, qPrintable(QString::number(engine_->duration())));
    }

    void opensUnicodePath() {
        QTemporaryDir dir;
        const QString path = QDir::toNativeSeparators(dir.filePath(QStringLiteral("тест 視頻 ✓ (1).mp4")));
        QVERIFY(QFile::copy(media("h264_aac.mp4"), path));
        QVERIFY(openAndWaitPlaying(path));
        QTRY_COMPARE_WITH_TIMEOUT(engine_->mediaInfo().path, path, 5000);
        engine_->stop();
        QTRY_COMPARE(engine_->state(), PlaybackState::Idle);  // release the file before dir cleanup
    }

    void reportsUnplayableFiles_data() {
        QTest::addColumn<QString>("file");
        QTest::newRow("not media") << media("not_media.mp4");
        QTest::newRow("missing") << media("does_not_exist.mp4");
    }

    void reportsUnplayableFiles() {
        QFETCH(QString, file);
        QSignalSpy errors(engine_.get(), &yp::IPlaybackEngine::errorOccurred);
        engine_->open(file);
        QTRY_COMPARE_WITH_TIMEOUT(engine_->state(), PlaybackState::Error, kLoadTimeoutMs);
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.first().first().toString().contains(QFileInfo(file).fileName()));
        // The engine stays usable after an error.
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));
    }

    void pauseSeekEndStopReplay() {
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));
        engine_->pause();
        QTRY_COMPARE(engine_->state(), PlaybackState::Paused);
        engine_->seek(2.0, SeekMode::Exact);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(engine_->position() - 2.0) < 0.05, 5000);
        engine_->play();
        QTRY_COMPARE_WITH_TIMEOUT(engine_->state(), PlaybackState::Ended, 5000);  // keep-open holds the end
        engine_->play();                                                          // restarts from 0
        QTRY_COMPARE(engine_->state(), PlaybackState::Playing);
        QTRY_VERIFY(engine_->position() < 1.0);
        engine_->stop();
        QTRY_COMPARE(engine_->state(), PlaybackState::Idle);
        engine_->play();  // reopens the last file
        QTRY_COMPARE_WITH_TIMEOUT(engine_->state(), PlaybackState::Playing, kLoadTimeoutMs);
    }

    void volumeAndMute() {
        QSignalSpy volume(engine_.get(), &yp::IPlaybackEngine::volumeChanged);
        engine_->setVolume(0);  // silent while the mute flag is toggled below
        QTRY_COMPARE(engine_->volume(), 0);
        engine_->setVolume(250);  // clamped to 100
        QTRY_COMPARE(engine_->volume(), 100);
        engine_->setVolume(0);
        QTRY_COMPARE(engine_->volume(), 0);
        QVERIFY(volume.count() >= 3);
        QTRY_VERIFY(engine_->isMuted());
        engine_->setMuted(false);
        QTRY_VERIFY(!engine_->isMuted());
        engine_->setMuted(true);
        QTRY_VERIFY(engine_->isMuted());
    }

    // Phase 2 exit criterion: 100 open/close cycles without crashes or resource growth.
    void repeatedOpenStopDoesNotLeak() {
        const QString file = media("h264_aac.mp4");
        const auto cycle = [&] {
            if (!openAndWaitPlaying(file)) return false;
            engine_->stop();
            return QTest::qWaitFor([&] { return engine_->state() == PlaybackState::Idle; }, kLoadTimeoutMs);
        };
        for (int i = 0; i < 5; ++i) QVERIFY(cycle());  // warm-up: decoder/driver pools settle
        const DWORD handles = handleCount();
        const int threads = threadCount();
        for (int i = 0; i < 100; ++i) QVERIFY2(cycle(), qPrintable(QStringLiteral("cycle %1").arg(i)));
        const auto handleGrowth = static_cast<long>(handleCount()) - static_cast<long>(handles);
        const int threadGrowth = threadCount() - threads;
        qInfo("100 cycles: handle growth %ld, thread growth %d", handleGrowth, threadGrowth);
        QVERIFY2(handleGrowth < 64, qPrintable(QString::number(handleGrowth)));
        QVERIFY2(threadGrowth <= 4, qPrintable(QString::number(threadGrowth)));
    }

    void repeatedEngineLifecycleDoesNotLeak() {
        engine_.reset();
        const auto lifecycle = [&] {
            if (!createEngine() || !openAndWaitPlaying(media("h264_aac.mp4"))) return false;
            engine_.reset();
            return true;
        };
        for (int i = 0; i < 3; ++i) QVERIFY(lifecycle());
        const DWORD handles = handleCount();
        const int threads = threadCount();
        for (int i = 0; i < 20; ++i) QVERIFY(lifecycle());
        const auto handleGrowth = static_cast<long>(handleCount()) - static_cast<long>(handles);
        const int threadGrowth = threadCount() - threads;
        qInfo("20 engine lifecycles: handle growth %ld, thread growth %d", handleGrowth, threadGrowth);
        QVERIFY2(handleGrowth < 64, qPrintable(QString::number(handleGrowth)));
        QVERIFY2(threadGrowth <= 4, qPrintable(QString::number(threadGrowth)));
        QVERIFY(createEngine());  // leave an engine for cleanup()
    }

    // ---- Phase 5: tracks, subtitles, chapters, adjustments, screenshots, devices

    void listsAndSwitchesTracks() {
        QVERIFY(openAndWaitPlaying(media("multitrack.mkv")));
        QTRY_COMPARE_WITH_TIMEOUT(countTracks(yp::TrackType::Audio), 2, 5000);
        QCOMPARE(countTracks(yp::TrackType::Subtitle), 2);
        QCOMPARE(countTracks(yp::TrackType::Video), 1);
        const int commentary = findTrack(yp::TrackType::Audio, [](const yp::Track& t) { return t.title == "Commentary"; });
        QVERIFY(commentary > 0);
        engine_->selectTrack(yp::TrackType::Audio, commentary);
        QTRY_COMPARE(selectedTrack(yp::TrackType::Audio), commentary);
        engine_->selectTrack(yp::TrackType::Subtitle, 0);
        QTRY_COMPARE(selectedTrack(yp::TrackType::Subtitle), 0);
        const int french = findTrack(yp::TrackType::Subtitle, [](const yp::Track& t) { return t.language == "fre"; });
        engine_->selectTrack(yp::TrackType::Subtitle, french);
        QTRY_COMPARE(selectedTrack(yp::TrackType::Subtitle), french);
    }

    void loadsExternalSubtitlesAndSurvivesMalformedOnes() {
        QVERIFY(openAndWaitPlaying(media("multitrack.mkv")));
        engine_->addSubtitleFile(media("external_rtl.srt"));
        QTRY_VERIFY_WITH_TIMEOUT(findTrack(yp::TrackType::Subtitle, [](const yp::Track& t) { return t.external && t.selected; }) > 0,
                                 5000);
        QSignalSpy errors(engine_.get(), &yp::IPlaybackEngine::errorOccurred);
        const int before = countTracks(yp::TrackType::Subtitle);
        engine_->addSubtitleFile(media("malformed.srt"));
        // Either rejected with a message or loaded as an (empty) track -- never a crash or a stop.
        QTRY_VERIFY_WITH_TIMEOUT(errors.count() > 0 || countTracks(yp::TrackType::Subtitle) > before, 5000);
        QCOMPARE(engine_->state(), PlaybackState::Playing);
        engine_->addSubtitleFile(media("missing.srt"));
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty() && errors.last().first().toString().contains("missing.srt"), 5000);
    }

    void chaptersAndChapterSeek() {
        QVERIFY(openAndWaitPlaying(media("multitrack.mkv")));
        QTRY_COMPARE_WITH_TIMEOUT(engine_->chapters().size(), 3, 5000);
        QCOMPARE(engine_->chapters().at(1).title, QStringLiteral("Middle"));
        QCOMPARE(engine_->chapters().at(2).time, 4.0);
        engine_->seekChapter(2);
        QTRY_COMPARE_WITH_TIMEOUT(engine_->currentChapter(), 2, 5000);
        QVERIFY(engine_->position() >= 3.9);
    }

    void startPositionSpeedAndDelays() {
        engine_->open(media("multitrack.mkv"), 2.5);  // resume-style start
        QTRY_COMPARE_WITH_TIMEOUT(engine_->state(), PlaybackState::Playing, kLoadTimeoutMs);
        QTRY_VERIFY_WITH_TIMEOUT(engine_->position() >= 2.4, 5000);
        engine_->setSpeed(1.5);
        QTRY_COMPARE(engine_->speed(), 1.5);
        engine_->setSpeed(50.0);  // clamped
        QTRY_COMPARE(engine_->speed(), 4.0);
        engine_->setAudioDelay(0.25);
        QTRY_COMPARE(engine_->audioDelay(), 0.25);
        engine_->setSubtitleDelay(-0.3);
        QTRY_COMPARE(engine_->subtitleDelay(), -0.3);
        engine_->setSpeed(1.0);
        engine_->setAudioDelay(0.0);
        engine_->setSubtitleDelay(0.0);
        QTRY_COMPARE(engine_->speed(), 1.0);
    }

    void frameStepMovesExactlyOneFrame() {
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));  // 30 fps
        engine_->pause();
        engine_->seek(1.0, SeekMode::Exact);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(engine_->position() - 1.0) < 0.02, 5000);
        engine_->frameStep(false);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(engine_->position() - (1.0 + 1.0 / 30.0)) < 0.01, 5000);
        QCOMPARE(engine_->state(), PlaybackState::Paused);
        engine_->frameStep(true);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(engine_->position() - 1.0) < 0.01, 5000);
    }

    void abLoopRepeatsTheSection() {
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));
        engine_->setAbLoop(0.5, 1.0);
        QTest::qWait(2500);  // would reach the 3 s end without the loop
        QVERIFY2(engine_->position() >= 0.4 && engine_->position() <= 1.1, qPrintable(QString::number(engine_->position())));
        QCOMPARE(engine_->state(), PlaybackState::Playing);
        engine_->setAbLoop(-1.0, -1.0);
        QTRY_COMPARE_WITH_TIMEOUT(engine_->state(), PlaybackState::Ended, 5000);
    }

    void screenshotWritesPng() {
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));
        QTemporaryDir dir;
        const QString path = QDir::toNativeSeparators(dir.filePath(QStringLiteral("shot ✓.png")));
        QSignalSpy saved(engine_.get(), &yp::IPlaybackEngine::screenshotSaved);
        engine_->screenshot(path, true);
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 5000);
        QCOMPARE(saved.first().first().toString(), path);
        QFile png(path);
        QVERIFY(png.open(QIODevice::ReadOnly));
        QVERIFY(png.read(8).startsWith("\x89PNG"));
    }

    void videoTransformsKeepPlaying() {
        QSignalSpy errors(engine_.get(), &yp::IPlaybackEngine::errorOccurred);
        QVERIFY(openAndWaitPlaying(media("hevc10_aac.mkv")));
        engine_->setAspectOverride(2.35);
        engine_->setRotation(90);
        engine_->setFlip(true, true);  // CPU filter on hardware-decoded frames
        QTest::qWait(700);
        QCOMPARE(engine_->state(), PlaybackState::Playing);
        const double pos = engine_->position();
        QTRY_VERIFY_WITH_TIMEOUT(engine_->position() > pos, 3000);  // still advancing
        engine_->setFlip(false, false);
        engine_->setRotation(0);
        engine_->setAspectOverride(0.0);
        QCOMPARE(errors.count(), 0);
    }

    void audioDevicesAreListed() {
        QTRY_VERIFY_WITH_TIMEOUT(!engine_->audioDevices().isEmpty(), 5000);
        QCOMPARE(engine_->audioDevices().first().name, QStringLiteral("auto"));
        engine_->setAudioDevice(QStringLiteral("auto"));
        QTRY_COMPARE(engine_->audioDevice(), QStringLiteral("auto"));
        engine_->setAudioChannels(QStringLiteral("stereo"));
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));  // plays with a forced layout
        engine_->setAudioChannels(QStringLiteral("auto"));
    }

    // Phase 2 exit criterion (closing while playing < 1 s) and the COM regression guard
    // documented in MpvEngine's constructor.
    void teardownIsFastAndKeepsComApartment() {
        QVERIFY(openAndWaitPlaying(media("h264_aac.mp4")));
        QElapsedTimer timer;
        timer.start();
        engine_.reset();
        const qint64 ms = timer.elapsed();
        qInfo("teardown while playing: %lld ms", ms);
        QVERIFY2(ms < 1000, qPrintable(QString::number(ms)));
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        QCOMPARE(hr, S_FALSE);  // S_OK would mean mpv tore down the UI thread's apartment
        CoUninitialize();
        QVERIFY(createEngine());
    }
};

QTEST_MAIN(TestMpvEngine)
#include "tst_mpvengine.moc"
