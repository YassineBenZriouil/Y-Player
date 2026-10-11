#include "ui/MainWindow.h"

#include "core/TimeFormat.h"
#include "ui/SeekSlider.h"
#include "ui/VideoWidget.h"

#include <QAction>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace yp::ui {
namespace {

constexpr double kSeekStep = 5.0;  // seconds per arrow key
constexpr int kVolumeStep = 5;

const QString kMediaFilter = QStringLiteral(
    "Media files (*.mp4 *.m4v *.mkv *.webm *.mov *.avi *.wmv *.flv *.ts *.m2ts *.mts *.mpg *.mpeg *.vob *.ogv "
    "*.3gp *.mp3 *.flac *.m4a *.aac *.opus *.ogg *.wav *.wma);;All files (*)");

QToolButton* makeButton(QWidget* parent, QAction* action) {
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);  // Space/arrows always go to the window shortcuts
    return button;
}

}  // namespace

MainWindow::MainWindow(std::unique_ptr<IPlaybackEngine> engine, QString aboutText, QWidget* parent)
    : QMainWindow(parent), engine_(std::move(engine)), aboutText_(std::move(aboutText)) {
    setWindowTitle(QStringLiteral("Y-Player"));
    setAcceptDrops(true);
    resize(1280, 760);

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

    infoLabel_ = new QLabel(tr("No file loaded"), this);
    statusBar()->addWidget(infoLabel_, 1);

    connect(video_, &VideoWidget::doubleClicked, this, [this] { setFullscreen(!isFullScreen()); });
    connect(engine_.get(), &IPlaybackEngine::stateChanged, this, &MainWindow::updateState);
    connect(engine_.get(), &IPlaybackEngine::positionChanged, this, &MainWindow::updatePosition);
    connect(engine_.get(), &IPlaybackEngine::durationChanged, this, &MainWindow::updateDuration);
    connect(engine_.get(), &IPlaybackEngine::mediaInfoChanged, this, &MainWindow::updateInfo);
    connect(engine_.get(), &IPlaybackEngine::errorOccurred, this, &MainWindow::showError);
    connect(engine_.get(), &IPlaybackEngine::volumeChanged, this, [this](int v) {
        const QSignalBlocker block(volumeSlider_);
        volumeSlider_->setValue(v);
    });
    connect(engine_.get(), &IPlaybackEngine::mutedChanged, this, [this](bool muted) {
        muteAction_->setChecked(muted);
        muteAction_->setIcon(style()->standardIcon(muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    });

    // winId() creates the native window now, so mpv can attach before the first show.
    if (!engine_->attachVideoOutput(static_cast<quintptr>(video_->winId()))) {
        showError(tr("Video output could not be initialized; see the log for details."));
    }
    updateState(engine_->state());
}

MainWindow::~MainWindow() {
    engine_.reset();  // must stop presenting before ~QWidget destroys the video window
}

void MainWindow::buildActions() {
    const auto add = [this](QMenu* menu, const QString& text, const QKeySequence& key, auto slot) {
        QAction* action = menu->addAction(text);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);  // stays active while the menu bar is hidden in fullscreen
        return action;
    };

    QMenu* file = menuBar()->addMenu(tr("&File"));
    add(file, tr("&Open File..."), QKeySequence::Open, [this] { showOpenDialog(); });
    file->addSeparator();
    add(file, tr("E&xit"), QKeySequence(Qt::CTRL | Qt::Key_Q), [this] { close(); });

    QMenu* playback = menuBar()->addMenu(tr("&Playback"));
    playPauseAction_ = add(playback, tr("&Play"), QKeySequence(Qt::Key_Space), [this] { togglePlayPause(); });
    stopAction_ = add(playback, tr("&Stop"), QKeySequence(), [this] { engine_->stop(); });
    stopAction_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    playback->addSeparator();
    add(playback, tr("Seek &Forward 5 s"), QKeySequence(Qt::Key_Right), [this] { seekBy(kSeekStep); });
    add(playback, tr("Seek &Backward 5 s"), QKeySequence(Qt::Key_Left), [this] { seekBy(-kSeekStep); });

    QMenu* audio = menuBar()->addMenu(tr("&Audio"));
    add(audio, tr("Volume &Up"), QKeySequence(Qt::Key_Up), [this] { changeVolumeBy(kVolumeStep); });
    add(audio, tr("Volume &Down"), QKeySequence(Qt::Key_Down), [this] { changeVolumeBy(-kVolumeStep); });
    muteAction_ = add(audio, tr("&Mute"), QKeySequence(Qt::Key_M), [this] { engine_->setMuted(!engine_->isMuted()); });
    muteAction_->setCheckable(true);
    muteAction_->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));

    QMenu* view = menuBar()->addMenu(tr("&View"));
    fullscreenAction_ = add(view, tr("&Fullscreen"), QKeySequence(Qt::Key_F), [this] { setFullscreen(!isFullScreen()); });
    fullscreenAction_->setCheckable(true);
    fullscreenAction_->setShortcuts({QKeySequence(Qt::Key_F), QKeySequence(Qt::Key_Return)});
    fullscreenAction_->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    auto* exitFullscreen = new QAction(this);
    exitFullscreen->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(exitFullscreen, &QAction::triggered, this, [this] { setFullscreen(false); });
    addAction(exitFullscreen);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About Y-Player"), this, [this] { QMessageBox::about(this, tr("About Y-Player"), aboutText_); });
}

QWidget* MainWindow::buildControls() {
    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(6, 4, 6, 4);

    playPauseButton_ = makeButton(bar, playPauseAction_);
    stopButton_ = makeButton(bar, stopAction_);
    row->addWidget(playPauseButton_);
    row->addWidget(stopButton_);

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

    muteButton_ = makeButton(bar, muteAction_);
    row->addWidget(muteButton_);
    volumeSlider_ = new QSlider(Qt::Horizontal, bar);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(engine_->volume());
    volumeSlider_->setFixedWidth(100);
    volumeSlider_->setFocusPolicy(Qt::NoFocus);
    volumeSlider_->setAccessibleName(tr("Volume"));
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int v) { engine_->setVolume(v); });
    row->addWidget(volumeSlider_);

    row->addWidget(makeButton(bar, fullscreenAction_));
    updateTimeLabel();
    return bar;
}

void MainWindow::openFile(const QString& path) {
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        showError(tr("Cannot open \"%1\": file not found or not readable.").arg(QDir::toNativeSeparators(path)));
        return;
    }
    lastDirectory_ = info.absolutePath();
    // Absolute native path: mpv must never interpret a file name as a protocol or option.
    engine_->open(QDir::toNativeSeparators(info.absoluteFilePath()));
}

void MainWindow::showOpenDialog() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Media File"), lastDirectory_, kMediaFilter);
    if (!path.isEmpty()) openFile(path);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile()) return;
    // shortcut: only the first dropped file plays; queueing the rest needs playlists (Phase 5).
    openFile(urls.first().toLocalFile());
    event->acceptProposedAction();
    activateWindow();
}

void MainWindow::togglePlayPause() {
    if (engine_->state() == PlaybackState::Playing) {
        engine_->pause();
    } else {
        engine_->play();
    }
}

void MainWindow::seekBy(double seconds) { engine_->seek(engine_->position() + seconds, SeekMode::Exact); }

void MainWindow::changeVolumeBy(int delta) { engine_->setVolume(engine_->volume() + delta); }

void MainWindow::setFullscreen(bool on) {
    if (on == isFullScreen()) return;
    menuBar()->setVisible(!on);
    controls_->setVisible(!on);
    statusBar()->setVisible(!on);
    if (on) {
        wasMaximized_ = isMaximized();
        showFullScreen();
    } else if (wasMaximized_) {
        showMaximized();
    } else {
        showNormal();
    }
    fullscreenAction_->setChecked(on);
}

void MainWindow::updateState(PlaybackState state) {
    const bool playing = state == PlaybackState::Playing;
    playPauseAction_->setText(playing ? tr("&Pause") : tr("&Play"));
    playPauseAction_->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    stopAction_->setEnabled(state != PlaybackState::Idle && state != PlaybackState::Error);
    seekSlider_->setEnabled(state == PlaybackState::Playing || state == PlaybackState::Paused ||
                            state == PlaybackState::Ended);
    if (state == PlaybackState::Idle) {
        seekSlider_->setRange(0, 0);
        updateTimeLabel();
        setWindowTitle(QStringLiteral("Y-Player"));
    }
}

void MainWindow::updatePosition(double seconds) {
    if (!seekSlider_->isDragging()) {
        const QSignalBlocker block(seekSlider_);
        seekSlider_->setValue(toMilliseconds(seconds));
    }
    updateTimeLabel();
}

void MainWindow::updateDuration(double seconds) {
    seekSlider_->setRange(0, toMilliseconds(seconds));
    seekSlider_->setPageStep(toMilliseconds(kSeekStep));
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

}  // namespace yp::ui
