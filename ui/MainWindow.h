#pragma once

#include "core/PlaybackEngine.h"

#include <QMainWindow>
#include <QString>

#include <memory>

class QAction;
class QLabel;
class QSlider;
class QToolButton;

namespace yp::ui {

class SeekSlider;
class VideoWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Takes ownership of the engine and destroys it before any child window, because the
    // engine presents into the video widget's native window. The UI only sees the interface.
    // aboutText: version/build details for Help > About, assembled by the app layer.
    MainWindow(std::unique_ptr<IPlaybackEngine> engine, QString aboutText, QWidget* parent = nullptr);
    ~MainWindow() override;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Opens a local file after validating it; reports problems in the status bar.
    void openFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void buildActions();
    QWidget* buildControls();
    void showOpenDialog();
    void togglePlayPause();
    void seekBy(double seconds);
    void changeVolumeBy(int delta);
    void setFullscreen(bool on);
    void updateState(PlaybackState state);
    void updatePosition(double seconds);
    void updateDuration(double seconds);
    void updateTimeLabel();
    void updateInfo(const MediaInfo& info);
    void showError(const QString& message);

    std::unique_ptr<IPlaybackEngine> engine_;
    QString aboutText_;
    QString lastDirectory_;
    bool wasMaximized_ = false;

    VideoWidget* video_ = nullptr;
    QWidget* controls_ = nullptr;
    QToolButton* playPauseButton_ = nullptr;
    QToolButton* stopButton_ = nullptr;
    SeekSlider* seekSlider_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QToolButton* muteButton_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* infoLabel_ = nullptr;

    QAction* playPauseAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QAction* muteAction_ = nullptr;
    QAction* fullscreenAction_ = nullptr;
};

}  // namespace yp::ui
