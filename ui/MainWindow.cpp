#include "ui/MainWindow.h"

#include "ui/Icons.h"

#include "core/PlaylistIO.h"
#include "core/TimeFormat.h"
#include "platform/win/WindowStyle.h"
#include "ui/DiagnosticsDialog.h"
#include "ui/PlaylistModel.h"
#include "ui/PlaylistPanel.h"
#include "ui/SeekSlider.h"
#include "ui/SettingsDialog.h"
#include "ui/ShortcutDialog.h"
#include "ui/SubtitleStyleDialog.h"
#include "ui/VideoWidget.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
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
#include <QUrl>
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
      store_(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat),
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
        mute->setIcon(glyphIcon(muted ? Glyph::Mute : Glyph::Volume));
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
    prefs_.resume = false;
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
    resumedAt_ = prefs_.resume ? resume_.positionFor(path) : 0.0;
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
    if (!prefs_.resume || row < 0 || engine_->duration() <= 0.0 ||
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
    toggle->setIcon(glyphIcon(playing ? Glyph::Pause : Glyph::Play));
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
            if (prefs_.autoplayNext || playlist_.repeat() == Playlist::Repeat::One) playNext(false);
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

void MainWindow::seekBy(double seconds) {
    engine_->seek(engine_->position() + seconds, prefs_.exactSeek ? SeekMode::Exact : SeekMode::Keyframe);
}

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
    prefs_.onTop = on;
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
    SubtitleStyleDialog dialog(prefs_.subtitles, this);
    connect(&dialog, &SubtitleStyleDialog::styleChanged, engine_.get(), &IPlaybackEngine::setSubtitleStyle);
    if (dialog.exec() == QDialog::Accepted) {
        prefs_.subtitles = dialog.subtitleStyle();
        saveSettings();
    }
    engine_->setSubtitleStyle(prefs_.subtitles);  // re-apply: undoes the live preview if cancelled
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
    seekSlider_->setPageStep(toMilliseconds(prefs_.seekShort));
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

// ------------------------------------------------------------------ settings, profiles, bookmarks

void MainWindow::applyPrefs() {
    engine_->setVolume(prefs_.volume);
    engine_->setMuted(prefs_.muted);
    engine_->setAudioChannels(prefs_.audioChannels);
    for (QAction* a : channelsMenu_->actions()) a->setChecked(a->data().toString() == prefs_.audioChannels);
    engine_->setSubtitleStyle(prefs_.subtitles);
    engine_->applySettings(prefs_.engine);
    action(QStringLiteral("playback.resume"))->setChecked(prefs_.resume);
    setAlwaysOnTop(prefs_.onTop);
    const auto secs = [](double v) { return QString::number(v, 'g', 4); };
    action(QStringLiteral("playback.seekForward"))->setText(tr("Seek &Forward %1 s").arg(secs(prefs_.seekShort)));
    action(QStringLiteral("playback.seekBackward"))->setText(tr("Seek &Backward %1 s").arg(secs(prefs_.seekShort)));
    action(QStringLiteral("playback.seekForwardLong"))->setText(tr("Seek Forward %1 s").arg(secs(prefs_.seekLong)));
    action(QStringLiteral("playback.seekBackwardLong"))->setText(tr("Seek Backward %1 s").arg(secs(prefs_.seekLong)));
    seekSlider_->setPageStep(toMilliseconds(prefs_.seekShort));
}

void MainWindow::applySettingsFromDialog(const PlayerSettings& settings) {
    prefs_ = settings;
    prefs_.volume = engine_->volume();  // the dialog does not edit what is playing right now
    prefs_.muted = engine_->isMuted();
    applyPrefs();
    saveSettings();
}

void MainWindow::showSettingsDialog() {
    prefs_.volume = engine_->volume();
    prefs_.muted = engine_->isMuted();
    SettingsDialog dialog(prefs_, this);
    connect(&dialog, &SettingsDialog::applied, this, &MainWindow::applySettingsFromDialog);
    connect(&dialog, &SettingsDialog::editShortcutsRequested, this, &MainWindow::showShortcutDialog);
    connect(&dialog, &SettingsDialog::showDiagnosticsRequested, this, &MainWindow::showDiagnostics);
    connect(&dialog, &SettingsDialog::openLogFolderRequested, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir_ + QStringLiteral("/logs"))); });
    connect(&dialog, &SettingsDialog::exportRequested, this, &MainWindow::exportSettings);
    connect(&dialog, &SettingsDialog::importRequested, this, [this, &dialog] {
        if (importSettings()) dialog.setSettings(prefs_);
    });
    dialog.exec();
}

void MainWindow::showDiagnostics() {
    if (diagnostics_ == nullptr) diagnostics_ = new DiagnosticsDialog(*engine_, this);
    diagnostics_->show();
    diagnostics_->raise();
    diagnostics_->activateWindow();
}

namespace {
// Settings-file groups that are preferences (exported/imported). Recent files, the playlist and
// window state stay private to this machine.
bool portableKey(const QString& key) {
    static const QStringList groups{QStringLiteral("engine"),   QStringLiteral("subtitles"), QStringLiteral("audio"),
                                    QStringLiteral("playback"), QStringLiteral("window"),    QStringLiteral("shortcuts"),
                                    QStringLiteral("profiles")};
    return groups.contains(key.section(QLatin1Char('/'), 0, 0));
}
}  // namespace

void MainWindow::exportSettings() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Settings"),
                                                      QDir::homePath() + QStringLiteral("/Y-Player settings.ini"),
                                                      tr("Settings (*.ini)"));
    if (path.isEmpty()) return;
    saveSettings();
    QSettings out(path, QSettings::IniFormat);
    out.clear();
    for (const QString& key : store_.allKeys()) {
        if (portableKey(key)) out.setValue(key, store_.value(key));
    }
    out.sync();
    if (out.status() == QSettings::NoError) {
        notify(tr("Settings exported"));
    } else {
        showError(tr("Cannot write %1").arg(QDir::toNativeSeparators(path)));
    }
}

bool MainWindow::importSettings() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Import Settings"), QDir::homePath(), tr("Settings (*.ini)"));
    if (path.isEmpty()) return false;
    QSettings in(path, QSettings::IniFormat);
    if (in.status() != QSettings::NoError || in.allKeys().isEmpty()) {
        showError(tr("\"%1\" is not a Y-Player settings file").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    for (const QString& key : in.allKeys()) {
        if (portableKey(key)) store_.setValue(key, in.value(key));  // validated by loadPlayerSettings below
    }
    store_.sync();
    prefs_ = loadPlayerSettings(store_);
    shortcuts_.resetAll();
    shortcuts_.applyOverrides(store_.value(QStringLiteral("shortcuts/overrides")).toMap());
    applyShortcuts();
    applyPrefs();
    notify(tr("Settings imported"));
    return true;
}

QList<NamedProfile> MainWindow::userProfiles() {
    QList<NamedProfile> profiles;
    store_.beginGroup(QStringLiteral("profiles"));
    for (const QString& group : store_.childGroups()) {
        store_.beginGroup(group);
        profiles.append({QUrl::fromPercentEncoding(group.toUtf8()), loadEngineSettings(store_)});
        store_.endGroup();
    }
    store_.endGroup();
    return profiles;
}

void MainWindow::applyProfile(const QString& name, const EngineSettings& settings) {
    prefs_.engine = settings;
    engine_->applySettings(prefs_.engine);
    saveSettings();
    notify(tr("Profile: %1").arg(name));
}

void MainWindow::fillProfilesMenu(QMenu* menu) {
    menu->clear();
    auto* group = new QActionGroup(menu);
    const auto addProfile = [&](const NamedProfile& p) {
        QAction* a = menu->addAction(p.name, this, [this, p] { applyProfile(p.name, p.settings); });
        a->setCheckable(true);
        a->setChecked(p.settings == prefs_.engine);
        group->addAction(a);
    };
    for (const NamedProfile& p : builtInProfiles()) addProfile(p);
    const QList<NamedProfile> mine = userProfiles();
    if (!mine.isEmpty()) menu->addSeparator();
    for (const NamedProfile& p : mine) addProfile(p);
    menu->addSeparator();
    menu->addAction(tr("Save Current Settings as Profile..."), this, [this] {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Save Profile"), tr("Profile name:"), QLineEdit::Normal, QString(), &ok)
                .trimmed();
        if (!ok || name.isEmpty()) return;
        // Percent-encoded so any name is a valid settings group (no '/' or '\' surprises).
        store_.beginGroup(QStringLiteral("profiles/") + QString::fromUtf8(QUrl::toPercentEncoding(name)));
        saveEngineSettings(store_, prefs_.engine);
        store_.endGroup();
        store_.sync();
        notify(tr("Profile \"%1\" saved").arg(name));
    });
    if (!mine.isEmpty()) {
        QMenu* remove = menu->addMenu(tr("Delete Profile"));
        for (const NamedProfile& p : mine) {
            remove->addAction(p.name, this, [this, name = p.name] {
                store_.remove(QStringLiteral("profiles/") + QString::fromUtf8(QUrl::toPercentEncoding(name)));
                store_.sync();
            });
        }
    }
}

void MainWindow::addBookmark() {
    const int row = playlist_.current();
    if (row < 0 || engine_->state() == PlaybackState::Idle || engine_->state() == PlaybackState::Error) {
        notify(tr("Open a file first"));
        return;
    }
    const double t = engine_->position();
    bookmarks_.add(playlist_.at(row), t, formatTime(t, engine_->duration()));
    saveSettings();
    notify(tr("Bookmark added at %1").arg(formatTime(t, engine_->duration())));
}

void MainWindow::fillBookmarksMenu(QMenu* menu) {
    menu->clear();
    menu->addAction(action(QStringLiteral("playback.bookmarkAdd")));
    menu->addSeparator();
    const int row = playlist_.current();
    const QList<Bookmark> list = row >= 0 ? bookmarks_.list(playlist_.at(row)) : QList<Bookmark>{};
    if (list.isEmpty()) {
        menu->addAction(tr("(no bookmarks for this file)"))->setEnabled(false);
        return;
    }
    for (const Bookmark& b : list) {
        menu->addAction(b.name, this, [this, t = b.time] { engine_->seek(t, SeekMode::Exact); });
    }
    menu->addSeparator();
    const QString path = playlist_.at(row);
    QMenu* remove = menu->addMenu(tr("Delete Bookmark"));
    for (int i = 0; i < list.size(); ++i) {
        remove->addAction(list.at(i).name, this, [this, path, i] {
            bookmarks_.remove(path, i);
            saveSettings();
        });
    }
    menu->addAction(tr("Delete All Bookmarks for This File"), this, [this, path] {
        bookmarks_.clear(path);
        saveSettings();
    });
}

// ------------------------------------------------------------------ persistence

void MainWindow::loadSettings() {
    prefs_ = loadPlayerSettings(store_);
    if (!persistent_) prefs_.resume = false;  // benchmark mode
    applyPrefs();

    playlist_.setRepeat(
        static_cast<Playlist::Repeat>(std::clamp(store_.value(QStringLiteral("playback/repeat"), 0).toInt(), 0, 2)));
    for (QAction* a : repeatMenu_->actions()) {
        if (a->data().isValid()) a->setChecked(a->data().toInt() == static_cast<int>(playlist_.repeat()));
    }
    const bool shuffle = store_.value(QStringLiteral("playback/shuffle"), false).toBool();
    playlist_.setShuffle(shuffle);
    action(QStringLiteral("playback.shuffle"))->setChecked(shuffle);

    recent_.setItems(store_.value(QStringLiteral("recent/files")).toStringList());

    if (const int skipped = shortcuts_.applyOverrides(store_.value(QStringLiteral("shortcuts/overrides")).toMap());
        skipped > 0) {
        showError(tr("%n saved shortcut(s) conflicted or were unknown and were reset.", "", skipped));
    }
    applyShortcuts();

    QFile resume(dataDir_ + QStringLiteral("/resume.json"));
    if (resume.open(QIODevice::ReadOnly)) resume_.loadJson(resume.readAll());
    QFile marks(dataDir_ + QStringLiteral("/bookmarks.json"));
    if (marks.open(QIODevice::ReadOnly)) bookmarks_.loadJson(marks.readAll());

    playlistio::ParseResult session;
    if (prefs_.restoreSession && playlistio::load(dataDir_ + QStringLiteral("/session.m3u8"), session)) {
        model_->insertPaths(-1, session.paths);
        model_->setCurrent(store_.value(QStringLiteral("playlist/current"), -1).toInt());
    }
}

void MainWindow::saveSettings() {
    if (!persistent_) return;
    prefs_.volume = engine_->volume();
    prefs_.muted = engine_->isMuted();
    savePlayerSettings(store_, prefs_);
    store_.setValue(QStringLiteral("playback/repeat"), static_cast<int>(playlist_.repeat()));
    store_.setValue(QStringLiteral("playback/shuffle"), playlist_.shuffle());
    store_.setValue(QStringLiteral("recent/files"), recent_.items());
    store_.setValue(QStringLiteral("shortcuts/overrides"), shortcuts_.overrides());
    store_.setValue(QStringLiteral("playlist/current"), playlist_.current());
    store_.sync();

    const auto writeFile = [this](const QString& name, const QByteArray& contents) {
        QSaveFile f(dataDir_ + QLatin1Char('/') + name);
        if (!f.open(QIODevice::WriteOnly) || f.write(contents) < 0 || !f.commit()) {
            qWarning("could not save %s", qPrintable(name));
        }
    };
    writeFile(QStringLiteral("resume.json"), resume_.toJson());
    writeFile(QStringLiteral("bookmarks.json"), bookmarks_.toJson());
    QString error;
    if (!playlistio::save(dataDir_ + QStringLiteral("/session.m3u8"), playlist_.items(), &error)) {
        qWarning("could not save the session playlist: %s", qPrintable(error));
    }
}

}  // namespace yp::ui
