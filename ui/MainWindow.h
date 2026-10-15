#pragma once

#include "core/Adjustments.h"
#include "core/Bookmarks.h"
#include "core/PlaybackEngine.h"
#include "core/PlayerSettings.h"
#include "core/Playlist.h"
#include "core/RecentFiles.h"
#include "core/ResumeStore.h"
#include "ui/ShortcutMap.h"

#include <QHash>
#include <QMainWindow>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

class QAction;
class QLabel;
class QMenu;
class QSlider;
class QToolButton;
class QVBoxLayout;

namespace yp::ui {

class DiagnosticsDialog;
class PlaylistModel;
class PlaylistPanel;
class SeekSlider;
class VideoWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Takes ownership of the engine and destroys it before any child window, because the
    // engine presents into the video widget's native window. The UI only sees the interface.
    // aboutText: version details for Help > About. dataDir: where settings, the session
    // playlist and resume points are stored (%LOCALAPPDATA%\Y-Player).
    MainWindow(std::unique_ptr<IPlaybackEngine> engine, QString aboutText, const QString& dataDir,
               QWidget* parent = nullptr);
    ~MainWindow() override;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Validates the paths (media files or .m3u/.m3u8/.pls playlists), puts them in the playlist
    // (replacing it, or appending) and plays the first new entry.
    void openFiles(const QStringList& paths, bool replace = true);
    void openFile(const QString& path) { openFiles({path}); }

    // Benchmark runs must not resume old positions or overwrite the user's saved session.
    void setBenchmarkMode();

    [[nodiscard]] IPlaybackEngine& engine() { return *engine_; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // MainWindowMenus.cpp
    void buildActions();
    void buildMenus();
    void applyShortcuts();
    [[nodiscard]] QAction* action(const QString& id) const { return actions_.value(id); }
    void fillRecentMenu(QMenu* menu);
    void fillSpeedMenu(QMenu* menu);
    void fillChapterMenu(QMenu* menu);
    void fillTrackMenu(QMenu* menu, TrackType type);
    void fillDeviceMenu(QMenu* menu);
    void showContextMenu(const QPoint& globalPos);
    void fillProfilesMenu(QMenu* menu);
    void fillBookmarksMenu(QMenu* menu);

    // MainWindow.cpp: playback and playlist
    QWidget* buildControls();
    QStringList expandPaths(const QStringList& paths);
    void addToPlaylist(const QStringList& paths, int row);
    void playIndex(int row);
    void playNext(bool userRequested);
    void playPrevious();
    void removeFromPlaylist(const QList<int>& rows);
    void recordResumePoint();
    void onStateChanged(PlaybackState state);
    void togglePlayPause();
    void seekBy(double seconds);
    void changeVolumeBy(int delta);
    void setSpeed(double speed);
    void setAudioDelay(double seconds);
    void setSubtitleDelay(double seconds);
    void cycleTrack(TrackType type);
    void cycleAbLoop();
    void setRepeat(Playlist::Repeat repeat);
    void setShuffle(bool on);
    void setAspect(double ratio);
    void setRotation(int degrees);
    void setFlip(bool horizontal, bool vertical);
    void takeScreenshot(bool withSubtitles);
    void setAlwaysOnTop(bool on);
    void setBorderless(bool on);
    void setFullscreen(bool on);
    void setPlaylistVisible(bool on);

    // MainWindow.cpp: dialogs
    void showOpenDialog(bool replace);
    void showSubtitleFileDialog();
    void showLoadPlaylistDialog();
    void showSavePlaylistDialog();
    void showShortcutDialog();
    void showSubtitleStyleDialog();

    // MainWindow.cpp: engine -> UI
    void updatePosition(double seconds);
    void updateDuration(double seconds);
    void updateTimeLabel();
    void updateInfo(const MediaInfo& info);
    void showError(const QString& message);
    void notify(const QString& message);  // OSD + status bar

    // MainWindow.cpp: settings, profiles, bookmarks (Phase 6)
    void applyPrefs();
    void applyCurrentTheme();
    void refreshIcons();
    void showFullscreenControls();
    void hideFullscreenControls();
    void showSeekPreview(int milliseconds, const QPoint& globalPos);
    void applySettingsFromDialog(const PlayerSettings& settings);
    void showSettingsDialog();
    void showDiagnostics();
    void exportSettings();
    bool importSettings();
    QList<NamedProfile> userProfiles();
    void applyProfile(const QString& name, const EngineSettings& settings);
    void addBookmark();

    // MainWindow.cpp: persistence
    void loadSettings();
    void saveSettings();

    std::unique_ptr<IPlaybackEngine> engine_;
    QString aboutText_;
    QString dataDir_;
    QSettings store_;
    PlayerSettings prefs_;
    bool persistent_ = true;

    Playlist playlist_;
    PlaylistModel* model_ = nullptr;
    PlaylistPanel* playlistPanel_ = nullptr;
    RecentFiles recent_;
    ResumeStore resume_;
    Bookmarks bookmarks_;
    double resumedAt_ = 0.0;
    int consecutiveFailures_ = 0;

    ShortcutMap shortcuts_;
    QHash<QString, QAction*> actions_;
    QHash<QString, std::function<void()>> handlers_;
    QMenu* repeatMenu_ = nullptr;
    QMenu* aspectMenu_ = nullptr;
    QMenu* rotateMenu_ = nullptr;
    QMenu* channelsMenu_ = nullptr;
    DiagnosticsDialog* diagnostics_ = nullptr;

    AbLoop abLoop_;
    double aspect_ = 0.0;
    int rotation_ = 0;
    bool flipH_ = false;
    bool flipV_ = false;
    bool borderless_ = false;
    bool wasMaximized_ = false;
    QString lastDirectory_;

    QString appliedTheme_;
    QVBoxLayout* centralLayout_ = nullptr;
    QWidget* overlay_ = nullptr;
    QTimer hideControlsTimer_;
    VideoWidget* video_ = nullptr;
    QWidget* controls_ = nullptr;
    QToolButton* playPauseButton_ = nullptr;
    SeekSlider* seekSlider_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* infoLabel_ = nullptr;
};

}  // namespace yp::ui
