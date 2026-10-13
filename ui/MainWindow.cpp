#include "ui/MainWindow.h"

#include "core/PlaylistIO.h"
#include "core/TimeFormat.h"
#include "platform/win/WindowStyle.h"
#include "ui/PlaylistModel.h"
#include "ui/PlaylistPanel.h"
#include "ui/SeekSlider.h"
#include "ui/ShortcutDialog.h"
#include "ui/SubtitleStyleDialog.h"
#include "ui/VideoWidget.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMimeData>
#include <QSaveFile>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace yp::ui {
namespace {

constexpr int kVolumeStep = 5;
constexpr int kSkipAfterErrorMs = 800;

const QString kMediaFilter = QStringLiteral(
    "Media files (*.mp4 *.m4v *.mkv *.webm *.mov *.avi *.wmv *.flv *.ts *.m2ts *.mts *.mpg *.mpeg *.vob *.ogv "
    "*.3gp *.mp3 *.flac *.m4a *.aac *.opus *.ogg *.wav *.wma);;Playlists (*.m3u *.m3u8 *.pls);;All files (*)");
const QString kSubtitleFilter =
    QStringLiteral("Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx *.sup *.smi *.txt);;All files (*)");
const QString kPlaylistFilter = QStringLiteral("Playlists (*.m3u8 *.m3u *.pls)");

bool isPlaylistFile(const QString& path) {
    const QString s = QFileInfo(path).suffix().toLower();
    return s == QLatin1String("m3u") || s == QLatin1String("m3u8") || s == QLatin1String("pls");
}

QToolButton* makeButton(QWidget* parent, QAction* action) {
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);  // Space/arrows always go to the window shortcuts
    return button;
}

QString signedMs(double seconds) {
    const long long ms = std::llround(seconds * 1000.0);
    return (ms > 0 ? QStringLiteral("+") : QString()) + QString::number(ms) + QStringLiteral(" ms");
}

}  // namespace

MainWindow::MainWindow(std::unique_ptr<IPlaybackEngine> engine, QString aboutText, const QString& dataDir,
                       QWidget* parent)
    : QMainWindow(parent),
      engine_(std::move(engine)),
      aboutText_(std::move(aboutText)),
      dataDir_(dataDir),
      settings_(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat),
      shortcuts_(playerActions()) {
    setWindowTitle(QStringLiteral("Y-Player"));
    setAcceptDrops(true);
    resize(1280, 760);

    model_ = new PlaylistModel(playlist_, this);
    buildActions();

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    video_ = new VideoWidget(central);
    layout->addWidget(video_, 1);
    controls_ = buildControls();
    layout->addWidget(controls_);
    setCentralWidget(central);

    playlistPanel_ = new PlaylistPanel(*model_, this);
    addDockWidget(Qt::RightDockWidgetArea, playlistPanel_);
    playlistPanel_->hide();
    connect(playlistPanel_, &QDockWidget::visibilityChanged, this,
            [this](bool visible) { action(QStringLiteral("view.playlist"))->setChecked(visible); });
    connect(playlistPanel_, &PlaylistPanel::playRequested, this, &MainWindow::playIndex);
    connect(playlistPanel_, &PlaylistPanel::removeRequested, this, &MainWindow::removeFromPlaylist);
    connect(playlistPanel_, &PlaylistPanel::addRequested, this, [this] { showOpenDialog(false); });
    connect(playlistPanel_, &PlaylistPanel::clearRequested, this, [this] { model_->clearAll(); });
    connect(model_, &PlaylistModel::filesDropped, this,
            [this](int row, const QStringList& paths) { addToPlaylist(expandPaths(paths), row); });

    buildMenus();

    infoLabel_ = new QLabel(tr("No file loaded"), this);
    statusBar()->addWidget(infoLabel_, 1);

    connect(video_, &VideoWidget::doubleClicked, this, [this] { setFullscreen(!isFullScreen()); });
    connect(video_, &VideoWidget::contextMenuRequested, this, &MainWindow::showContextMenu);
    connect(engine_.get(), &IPlaybackEngine::stateChanged, this, &MainWindow::onStateChanged);
    connect(engine_.get(), &IPlaybackEngine::positionChanged, this, &MainWindow::updatePosition);
    connect(engine_.get(), &IPlaybackEngine::durationChanged, this, &MainWindow::updateDuration);
    connect(engine_.get(), &IPlaybackEngine::mediaInfoChanged, this, &MainWindow::updateInfo);
    connect(engine_.get(), &IPlaybackEngine::errorOccurred, this, &MainWindow::showError);
    connect(engine_.get(), &IPlaybackEngine::notice, this, &MainWindow::notify);
    connect(engine_.get(), &IPlaybackEngine::screenshotSaved, this, [this](const QString& path) {
        notify(tr("Screenshot saved: %1").arg(QDir::toNativeSeparators(path)));
    });
    connect(engine_.get(), &IPlaybackEngine::volumeChanged, this, [this](int v) {
        const QSignalBlocker block(volumeSlider_);
        volumeSlider_->setValue(v);
    });
    connect(engine_.get(), &IPlaybackEngine::mutedChanged, this, [this](bool muted) {
        QAction* mute = action(QStringLiteral("audio.mute"));
        mute->setChecked(muted);
        mute->setIcon(style()->standardIcon(muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    });

    // winId() creates the native window now, so mpv can attach before the first show.
    if (!engine_->attachVideoOutput(static_cast<quintptr>(video_->winId()))) {
        showError(tr("Video output could not be initialized; see the log for details."));
    }
    loadSettings();
    onStateChanged(engine_->state());
}

MainWindow::~MainWindow() {
    engine_.reset();  // must stop presenting before ~QWidget destroys the video window
}

void MainWindow::setBenchmarkMode() {
    persistent_ = false;
    resumeEnabled_ = false;
}

QWidget* MainWindow::buildControls() {
    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(6, 4, 6, 4);

    playPauseButton_ = makeButton(bar, action(QStringLiteral("playback.toggle")));
    row->addWidget(playPauseButton_);
    row->addWidget(makeButton(bar, action(QStringLiteral("playback.stop"))));
    row->addWidget(makeButton(bar, action(QStringLiteral("playback.previous"))));
    row->addWidget(makeButton(bar, action(QStringLiteral("playback.next"))));

    seekSlider_ = new SeekSlider(bar);
    seekSlider_->setAccessibleName(tr("Seek"));
    connect(seekSlider_, &SeekSlider::seekRequested, this, [this](int ms, bool exact) {
        engine_->seek(ms / 1000.0, exact ? SeekMode::Exact : SeekMode::Keyframe);
        updateTimeLabel();
    });
    row->addWidget(seekSlider_, 1);

    timeLabel_ = new QLabel(bar);
    timeLabel_->setMinimumWidth(timeLabel_->fontMetrics().horizontalAdvance(QStringLiteral("00:00:00 / 00:00:00")));
    timeLabel_->setAlignment(Qt::AlignCenter);
    row->addWidget(timeLabel_);

    row->addWidget(makeButton(bar, action(QStringLiteral("audio.mute"))));
    volumeSlider_ = new QSlider(Qt::Horizontal, bar);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(engine_->volume());
    volumeSlider_->setFixedWidth(100);
    volumeSlider_->setFocusPolicy(Qt::NoFocus);
    volumeSlider_->setAccessibleName(tr("Volume"));
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int v) { engine_->setVolume(v); });
    row->addWidget(volumeSlider_);

    row->addWidget(makeButton(bar, action(QStringLiteral("view.playlist"))));
    row->addWidget(makeButton(bar, action(QStringLiteral("video.fullscreen"))));
    updateTimeLabel();
    return bar;
}

// ------------------------------------------------------------------ playlist

QStringList MainWindow::expandPaths(const QStringList& paths) {
    QStringList files;
    QStringList problems;
    for (const QString& path : paths) {
        const QFileInfo info(path);
        if (!info.isFile() || !info.isReadable()) {
            problems << QDir::toNativeSeparators(path);
        } else if (isPlaylistFile(path)) {
            playlistio::ParseResult parsed;
            QString error;
            if (playlistio::load(info.absoluteFilePath(), parsed, &error)) {
                files << parsed.paths;
                if (parsed.skipped > 0) notify(tr("%n playlist entries skipped (network or unreadable).", "", parsed.skipped));
            } else {
                problems << QStringLiteral("%1 (%2)").arg(QDir::toNativeSeparators(path), error);
            }
        } else {
            // Absolute native path: mpv must never interpret a file name as a protocol or option.
            files << QDir::toNativeSeparators(info.absoluteFilePath());
        }
    }
    if (!problems.isEmpty()) showError(tr("Cannot open: %1").arg(problems.join(QStringLiteral(", "))));
    return files;
}

void MainWindow::openFiles(const QStringList& paths, bool replace) {
    const QStringList files = expandPaths(paths);
    if (files.isEmpty()) return;
    lastDirectory_ = QFileInfo(files.first()).absolutePath();
    if (replace) {
        recordResumePoint();
        model_->clearAll();
    }
    const int firstNew = playlist_.size();
    model_->insertPaths(-1, files);
    const bool idle = engine_->state() == PlaybackState::Idle || engine_->state() == PlaybackState::Error;
    if (replace || idle) playIndex(firstNew);
}

void MainWindow::addToPlaylist(const QStringList& paths, int row) {
    if (paths.isEmpty()) return;
    model_->insertPaths(row, paths);
    notify(tr("%n file(s) added to the playlist", "", static_cast<int>(paths.size())));
}

void MainWindow::removeFromPlaylist(const QList<int>& rows) {
    model_->removeRowsAt(rows);  // removing the playing entry keeps it playing, just unlisted
}

void MainWindow::playIndex(int row) {
    if (row < 0 || row >= playlist_.size()) return;
    recordResumePoint();
    model_->setCurrent(row);
    playlistPanel_->scrollToRow(row);
    const QString path = playlist_.at(row);
    abLoop_.clear();
    engine_->setAbLoop(-1.0, -1.0);  // mpv keeps A-B points across files
    resumedAt_ = resumeEnabled_ ? resume_.positionFor(path) : 0.0;
    engine_->open(path, resumedAt_);
    recent_.add(path);
}

void MainWindow::playNext(bool userRequested) {
    const int next = playlist_.nextIndex(userRequested);
    if (next >= 0) playIndex(next);
}

void MainWindow::playPrevious() {
    // Like most players: more than 3 s in, "previous" restarts the current file first.
    if (engine_->position() > 3.0 && engine_->state() != PlaybackState::Idle) {
        engine_->seek(0.0, SeekMode::Exact);
        return;
    }
    const int prev = playlist_.previousIndex();
    if (prev >= 0) playIndex(prev);
}

void MainWindow::recordResumePoint() {
    const int row = playlist_.current();
    const PlaybackState s = engine_->state();
    if (!resumeEnabled_ || row < 0 || engine_->duration() <= 0.0 ||
        (s != PlaybackState::Playing && s != PlaybackState::Paused && s != PlaybackState::Ended)) {
        return;
    }
    const double position = s == PlaybackState::Ended ? engine_->duration() : engine_->position();
    resume_.record(playlist_.at(row), position, engine_->duration());
}

void MainWindow::onStateChanged(PlaybackState state) {
    const bool playing = state == PlaybackState::Playing;
    QAction* toggle = action(QStringLiteral("playback.toggle"));
    toggle->setText(playing ? tr("&Pause") : tr("&Play"));
    toggle->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    action(QStringLiteral("playback.stop"))->setEnabled(state != PlaybackState::Idle && state != PlaybackState::Error);
    seekSlider_->setEnabled(state == PlaybackState::Playing || state == PlaybackState::Paused ||
                            state == PlaybackState::Ended);
    switch (state) {
        case PlaybackState::Idle:
            seekSlider_->setRange(0, 0);
            updateTimeLabel();
            setWindowTitle(QStringLiteral("Y-Player"));
            break;
        case PlaybackState::Playing:
            consecutiveFailures_ = 0;
            if (resumedAt_ > 0.0) {
                engine_->showMessage(tr("Resumed at %1").arg(formatTime(resumedAt_, engine_->duration())), 2500);
                resumedAt_ = 0.0;
            }
            break;
        case PlaybackState::Ended:
            recordResumePoint();  // finished: forget the resume point
            playNext(false);
            break;
        case PlaybackState::Error:
            // Skip unplayable entries, but never loop forever over a list of broken files.
            if (++consecutiveFailures_ < playlist_.size()) {
                QTimer::singleShot(kSkipAfterErrorMs, this, [this] { playNext(true); });
            }
            break;
        default: break;
    }
}

// ------------------------------------------------------------------ playback actions

void MainWindow::togglePlayPause() {
    if (engine_->state() == PlaybackState::Playing) {
        engine_->pause();
    } else if (engine_->state() == PlaybackState::Idle && playlist_.current() < 0 && !playlist_.isEmpty()) {
        playIndex(0);
    } else {
        engine_->play();
    }
}

void MainWindow::seekBy(double seconds) { engine_->seek(engine_->position() + seconds, SeekMode::Exact); }

void MainWindow::changeVolumeBy(int delta) {
    const int v = std::clamp(engine_->volume() + delta, 0, 100);
    engine_->setVolume(v);
    engine_->showMessage(tr("Volume %1%").arg(v));
}

void MainWindow::setSpeed(double speed) {
    const double s = clampSpeed(speed);
    engine_->setSpeed(s);
    engine_->showMessage(tr("Speed %1×").arg(s, 0, 'g', 3));
}

void MainWindow::setAudioDelay(double seconds) {
    engine_->setAudioDelay(seconds);
    engine_->showMessage(tr("Audio delay %1").arg(signedMs(seconds)));
}

void MainWindow::setSubtitleDelay(double seconds) {
    engine_->setSubtitleDelay(seconds);
    engine_->showMessage(tr("Subtitle delay %1").arg(signedMs(seconds)));
}

void MainWindow::cycleTrack(TrackType type) {
    QList<Track> list;
    for (const Track& t : engine_->tracks()) {
        if (t.type == type) list << t;
    }
    const QString kind = type == TrackType::Audio ? tr("Audio") : tr("Subtitles");
    if (list.isEmpty()) {
        engine_->showMessage(tr("%1: no tracks").arg(kind));
        return;
    }
    int selected = -1;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).selected) selected = i;
    }
    // Subtitles cycle through "off" after the last track; audio just wraps.
    const int next = selected + 1;
    if (type == TrackType::Subtitle && next >= list.size()) {
        engine_->selectTrack(type, 0);
        engine_->showMessage(tr("Subtitles: off"));
        return;
    }
    const Track& t = list.at(next % static_cast<int>(list.size()));
    engine_->selectTrack(type, t.id);
    const QString name = t.title.isEmpty() ? (t.language.isEmpty() ? tr("Track %1").arg(t.id) : t.language) : t.title;
    engine_->showMessage(QStringLiteral("%1: %2").arg(kind, name));
}

void MainWindow::cycleAbLoop() {
    abLoop_.cycle(engine_->position());
    engine_->setAbLoop(abLoop_.a, abLoop_.active() ? abLoop_.b : -1.0);
    if (abLoop_.active()) {
        engine_->showMessage(tr("A-B repeat: %1 – %2").arg(formatTime(abLoop_.a), formatTime(abLoop_.b)));
    } else if (abLoop_.hasA()) {
        engine_->showMessage(tr("A-B repeat: A = %1 (press again to set B)").arg(formatTime(abLoop_.a)));
    } else {
        engine_->showMessage(tr("A-B repeat off"));
    }
}

void MainWindow::setRepeat(Playlist::Repeat repeat) {
    playlist_.setRepeat(repeat);
    const QStringList names{tr("Repeat off"), tr("Repeat one"), tr("Repeat all")};
    const int i = static_cast<int>(repeat);
    for (QAction* a : repeatMenu_->actions()) {
        if (a->data().isValid()) a->setChecked(a->data().toInt() == i);
    }
    engine_->showMessage(names.at(i));
}

void MainWindow::setShuffle(bool on) {
    playlist_.setShuffle(on);
    action(QStringLiteral("playback.shuffle"))->setChecked(on);
    engine_->showMessage(on ? tr("Shuffle on") : tr("Shuffle off"));
}

void MainWindow::setAspect(double ratio) {
    aspect_ = ratio;
    engine_->setAspectOverride(ratio);
    for (QAction* a : aspectMenu_->actions()) {
        if (a->data().isValid()) a->setChecked(qFuzzyCompare(1.0 + a->data().toDouble(), 1.0 + ratio));
    }
    engine_->showMessage(ratio > 0.0 ? tr("Aspect ratio %1:1").arg(ratio, 0, 'f', 2) : tr("Aspect ratio: default"));
}

void MainWindow::setRotation(int degrees) {
    rotation_ = ((degrees % 360) + 360) % 360;
    engine_->setRotation(rotation_);
    for (QAction* a : rotateMenu_->actions()) {
        if (a->data().isValid()) a->setChecked(a->data().toInt() == rotation_);
    }
    engine_->showMessage(tr("Rotation %1°").arg(rotation_));
}

void MainWindow::setFlip(bool horizontal, bool vertical) {
    flipH_ = horizontal;
    flipV_ = vertical;
    engine_->setFlip(horizontal, vertical);
    action(QStringLiteral("video.flipH"))->setChecked(horizontal);
    action(QStringLiteral("video.flipV"))->setChecked(vertical);
}

void MainWindow::takeScreenshot(bool withSubtitles) {
    if (playlist_.current() < 0 || engine_->state() == PlaybackState::Idle) {
        notify(tr("Nothing to capture"));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Y-Player");
    if (!QDir().mkpath(dir)) {
        showError(tr("Cannot create the screenshot folder %1").arg(QDir::toNativeSeparators(dir)));
        return;
    }
    const auto ms = static_cast<long long>(engine_->position() * 1000.0);
    const QString stamp = formatTime(engine_->position(), engine_->duration()).replace(QLatin1Char(':'), QLatin1Char('-')) +
                          QStringLiteral(".%1").arg(ms % 1000, 3, 10, QLatin1Char('0'));
    const QString base = dir + QLatin1Char('/') + QFileInfo(playlist_.at(playlist_.current())).completeBaseName() +
                         QLatin1Char('_') + stamp;
    QString path = base + QStringLiteral(".png");
    for (int n = 2; QFileInfo::exists(path); ++n) path = base + QStringLiteral(" (%1).png").arg(n);
    engine_->screenshot(path, withSubtitles);
}

void MainWindow::setAlwaysOnTop(bool on) {
    onTop_ = on;
    platform::setAlwaysOnTop(static_cast<quintptr>(winId()), on);
    action(QStringLiteral("video.onTop"))->setChecked(on);
}

void MainWindow::setBorderless(bool on) {
    borderless_ = on;
    if (!isFullScreen()) platform::setBorderless(static_cast<quintptr>(winId()), on);
    video_->setDragMovesWindow(on);
    action(QStringLiteral("video.borderless"))->setChecked(on);
}

void MainWindow::setFullscreen(bool on) {
    if (on == isFullScreen()) return;
    menuBar()->setVisible(!on);
    controls_->setVisible(!on);
    statusBar()->setVisible(!on);
    if (on) {
        wasMaximized_ = isMaximized();
        showFullScreen();
    } else {
        if (wasMaximized_) {
            showMaximized();
        } else {
            showNormal();
        }
        if (borderless_) platform::setBorderless(static_cast<quintptr>(winId()), true);  // Qt restored the caption
    }
    action(QStringLiteral("video.fullscreen"))->setChecked(on);
}

void MainWindow::setPlaylistVisible(bool on) { playlistPanel_->setVisible(on); }

// ------------------------------------------------------------------ dialogs

void MainWindow::showOpenDialog(bool replace) {
    const QStringList paths = QFileDialog::getOpenFileNames(this, replace ? tr("Open Files") : tr("Add Files"),
                                                            lastDirectory_, kMediaFilter);
    if (!paths.isEmpty()) openFiles(paths, replace);
}

void MainWindow::showSubtitleFileDialog() {
    if (engine_->state() == PlaybackState::Idle) {
        notify(tr("Open a video first, then load its subtitles"));
        return;
    }
    const QString start = playlist_.current() >= 0 ? QFileInfo(playlist_.at(playlist_.current())).absolutePath() : lastDirectory_;
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Subtitle File"), start, kSubtitleFilter);
    if (!path.isEmpty()) engine_->addSubtitleFile(QDir::toNativeSeparators(path));
}

void MainWindow::showLoadPlaylistDialog() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Playlist"), lastDirectory_, kPlaylistFilter);
    if (!path.isEmpty()) openFiles({path}, true);
}

void MainWindow::showSavePlaylistDialog() {
    if (playlist_.isEmpty()) {
        notify(tr("The playlist is empty"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Playlist"), lastDirectory_ + QStringLiteral("/playlist.m3u8"),
                                                      kPlaylistFilter);
    if (path.isEmpty()) return;
    QString error;
    if (playlistio::save(path, playlist_.items(), &error)) {
        notify(tr("Playlist saved"));
    } else {
        showError(tr("Cannot save the playlist: %1").arg(error));
    }
}

void MainWindow::showShortcutDialog() {
    ShortcutDialog dialog(shortcuts_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    shortcuts_ = dialog.map();
    applyShortcuts();
    saveSettings();
}

void MainWindow::showSubtitleStyleDialog() {
    SubtitleStyleDialog dialog(subtitleStyle_, this);
    connect(&dialog, &SubtitleStyleDialog::styleChanged, engine_.get(), &IPlaybackEngine::setSubtitleStyle);
    if (dialog.exec() == QDialog::Accepted) {
        subtitleStyle_ = dialog.subtitleStyle();
        saveSettings();
    }
    engine_->setSubtitleStyle(subtitleStyle_);  // re-apply: undoes the live preview if cancelled
}

// ------------------------------------------------------------------ engine -> UI

void MainWindow::updatePosition(double seconds) {
    if (!seekSlider_->isDragging()) {
        const QSignalBlocker block(seekSlider_);
        seekSlider_->setValue(toMilliseconds(seconds));
    }
    updateTimeLabel();
}

void MainWindow::updateDuration(double seconds) {
    seekSlider_->setRange(0, toMilliseconds(seconds));
    seekSlider_->setPageStep(toMilliseconds(5.0));
    updateTimeLabel();
}

void MainWindow::updateTimeLabel() {
    const double duration = engine_->duration();
    const double position = seekSlider_->isDragging() ? seekSlider_->value() / 1000.0 : engine_->position();
    timeLabel_->setText(formatTime(position, duration) + QStringLiteral(" / ") + formatTime(duration, duration));
}

void MainWindow::updateInfo(const MediaInfo& info) {
    if (info.path.isEmpty()) return;
    const QString name = info.title.isEmpty() ? QFileInfo(info.path).fileName() : info.title;
    setWindowTitle(name + QStringLiteral(" — Y-Player"));
    QStringList parts{name};
    if (info.width > 0) parts << QStringLiteral("%1×%2").arg(info.width).arg(info.height);
    if (info.fps > 0) parts << tr("%1 fps").arg(info.fps, 0, 'f', info.fps == static_cast<int>(info.fps) ? 0 : 3);
    QStringList codecs;
    if (!info.videoCodec.isEmpty()) codecs << info.videoCodec;
    if (!info.audioCodec.isEmpty()) codecs << info.audioCodec;
    if (!codecs.isEmpty()) parts << codecs.join(QStringLiteral(" / "));
    if (!info.videoCodec.isEmpty()) {
        parts << (info.hwdec.isEmpty() ? tr("software decoding") : tr("hardware decoding (%1)").arg(info.hwdec));
    }
    infoLabel_->setStyleSheet(QString());
    infoLabel_->setText(parts.join(QStringLiteral("  ·  ")));
}

void MainWindow::showError(const QString& message) {
    infoLabel_->setStyleSheet(QStringLiteral("color: #d9534f;"));
    infoLabel_->setText(message);
}

void MainWindow::notify(const QString& message) {
    statusBar()->showMessage(message, 4000);
    engine_->showMessage(message, 2500);
}

// ------------------------------------------------------------------ drag & drop, close

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (std::ranges::any_of(urls, [](const QUrl& u) { return u.isLocalFile(); })) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    if (paths.isEmpty()) return;
    // Holding Ctrl adds to the playlist instead of replacing it.
    openFiles(paths, !(event->modifiers() & Qt::ControlModifier));
    event->acceptProposedAction();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    recordResumePoint();
    saveSettings();
    QMainWindow::closeEvent(event);
}

// ------------------------------------------------------------------ persistence

void MainWindow::loadSettings() {
    engine_->setVolume(settings_.value(QStringLiteral("audio/volume"), 100).toInt());
    engine_->setMuted(settings_.value(QStringLiteral("audio/muted"), false).toBool());
    audioChannels_ = settings_.value(QStringLiteral("audio/channels"), QStringLiteral("auto")).toString();
    engine_->setAudioChannels(audioChannels_);
    for (QAction* a : channelsMenu_->actions()) a->setChecked(a->data().toString() == audioChannels_);

    resumeEnabled_ = settings_.value(QStringLiteral("playback/resume"), true).toBool();
    action(QStringLiteral("playback.resume"))->setChecked(resumeEnabled_);
    playlist_.setRepeat(static_cast<Playlist::Repeat>(std::clamp(settings_.value(QStringLiteral("playback/repeat"), 0).toInt(), 0, 2)));
    for (QAction* a : repeatMenu_->actions()) {
        if (a->data().isValid()) a->setChecked(a->data().toInt() == static_cast<int>(playlist_.repeat()));
    }
    const bool shuffle = settings_.value(QStringLiteral("playback/shuffle"), false).toBool();
    playlist_.setShuffle(shuffle);
    action(QStringLiteral("playback.shuffle"))->setChecked(shuffle);

    recent_.setItems(settings_.value(QStringLiteral("recent/files")).toStringList());

    settings_.beginGroup(QStringLiteral("subtitles"));
    const SubtitleStyle d;
    subtitleStyle_.font = settings_.value(QStringLiteral("font"), d.font).toString();
    subtitleStyle_.size = settings_.value(QStringLiteral("size"), d.size).toInt();
    subtitleStyle_.color = settings_.value(QStringLiteral("color"), d.color).toString();
    subtitleStyle_.outlineColor = settings_.value(QStringLiteral("outlineColor"), d.outlineColor).toString();
    subtitleStyle_.outlineSize = settings_.value(QStringLiteral("outlineSize"), d.outlineSize).toDouble();
    subtitleStyle_.position = settings_.value(QStringLiteral("position"), d.position).toInt();
    subtitleStyle_.overrideStyled = settings_.value(QStringLiteral("overrideStyled"), d.overrideStyled).toBool();
    settings_.endGroup();
    engine_->setSubtitleStyle(subtitleStyle_);

    if (const int skipped = shortcuts_.applyOverrides(settings_.value(QStringLiteral("shortcuts/overrides")).toMap()); skipped > 0) {
        showError(tr("%n saved shortcut(s) conflicted or were unknown and were reset.", "", skipped));
    }
    applyShortcuts();

    if (settings_.value(QStringLiteral("window/onTop"), false).toBool()) setAlwaysOnTop(true);

    QFile resume(dataDir_ + QStringLiteral("/resume.json"));
    if (resume.open(QIODevice::ReadOnly)) resume_.loadJson(resume.readAll());

    playlistio::ParseResult session;
    if (playlistio::load(dataDir_ + QStringLiteral("/session.m3u8"), session)) {
        model_->insertPaths(-1, session.paths);
        model_->setCurrent(settings_.value(QStringLiteral("playlist/current"), -1).toInt());
    }
}

void MainWindow::saveSettings() {
    if (!persistent_) return;
    settings_.setValue(QStringLiteral("audio/volume"), engine_->volume());
    settings_.setValue(QStringLiteral("audio/muted"), engine_->isMuted());
    settings_.setValue(QStringLiteral("audio/channels"), audioChannels_);
    settings_.setValue(QStringLiteral("playback/resume"), resumeEnabled_);
    settings_.setValue(QStringLiteral("playback/repeat"), static_cast<int>(playlist_.repeat()));
    settings_.setValue(QStringLiteral("playback/shuffle"), playlist_.shuffle());
    settings_.setValue(QStringLiteral("recent/files"), recent_.items());
    settings_.beginGroup(QStringLiteral("subtitles"));
    settings_.setValue(QStringLiteral("font"), subtitleStyle_.font);
    settings_.setValue(QStringLiteral("size"), subtitleStyle_.size);
    settings_.setValue(QStringLiteral("color"), subtitleStyle_.color);
    settings_.setValue(QStringLiteral("outlineColor"), subtitleStyle_.outlineColor);
    settings_.setValue(QStringLiteral("outlineSize"), subtitleStyle_.outlineSize);
    settings_.setValue(QStringLiteral("position"), subtitleStyle_.position);
    settings_.setValue(QStringLiteral("overrideStyled"), subtitleStyle_.overrideStyled);
    settings_.endGroup();
    settings_.setValue(QStringLiteral("shortcuts/overrides"), shortcuts_.overrides());
    settings_.setValue(QStringLiteral("window/onTop"), onTop_);
    settings_.setValue(QStringLiteral("playlist/current"), playlist_.current());
    settings_.sync();

    QSaveFile resume(dataDir_ + QStringLiteral("/resume.json"));
    if (resume.open(QIODevice::WriteOnly)) {
        resume.write(resume_.toJson());
        resume.commit();
    }
    QString error;
    if (!playlistio::save(dataDir_ + QStringLiteral("/session.m3u8"), playlist_.items(), &error)) {
        qWarning("could not save the session playlist: %s", qPrintable(error));
    }
}

}  // namespace yp::ui
