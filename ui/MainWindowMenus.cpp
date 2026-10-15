// MainWindow: actions, shortcuts and menus (split from MainWindow.cpp to keep files small).

#include "ui/MainWindow.h"

#include "ui/Icons.h"

#include "core/TimeFormat.h"
#include "ui/PlaylistPanel.h"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStyle>

namespace yp::ui {
namespace {

struct AspectChoice {
    const char* label;
    double ratio;
};
constexpr AspectChoice kAspects[] = {{"Default", 0.0},       {"4:3", 4.0 / 3.0},   {"16:9", 16.0 / 9.0},
                                     {"1.85:1", 1.85},       {"2.35:1", 2.35},     {"2.39:1", 2.39},
                                     {"1:1", 1.0}};

QString trackLabel(const Track& t) {
    QString label = t.title.isEmpty() ? QObject::tr("Track %1").arg(t.id) : t.title;
    if (!t.language.isEmpty()) label += QStringLiteral(" [%1]").arg(t.language);
    if (!t.codec.isEmpty()) label += QStringLiteral(" (%1)").arg(t.codec);
    if (t.external) label += QObject::tr(" — external");
    return label;
}

}  // namespace

void MainWindow::buildActions() {
    const auto engine = [this] { return engine_.get(); };
    handlers_ = {
        {QStringLiteral("file.open"), [this] { showOpenDialog(true); }},
        {QStringLiteral("file.add"), [this] { showOpenDialog(false); }},
        {QStringLiteral("file.openSubtitle"), [this] { showSubtitleFileDialog(); }},
        {QStringLiteral("file.loadPlaylist"), [this] { showLoadPlaylistDialog(); }},
        {QStringLiteral("file.savePlaylist"), [this] { showSavePlaylistDialog(); }},
        {QStringLiteral("file.quit"), [this] { close(); }},
        {QStringLiteral("playback.toggle"), [this] { togglePlayPause(); }},
        {QStringLiteral("playback.stop"), [engine] { engine()->stop(); }},
        {QStringLiteral("playback.previous"), [this] { playPrevious(); }},
        {QStringLiteral("playback.next"), [this] { playNext(true); }},
        {QStringLiteral("playback.seekForward"), [this] { seekBy(5.0); }},
        {QStringLiteral("playback.seekBackward"), [this] { seekBy(-5.0); }},
        {QStringLiteral("playback.seekForwardLong"), [this] { seekBy(30.0); }},
        {QStringLiteral("playback.seekBackwardLong"), [this] { seekBy(-30.0); }},
        {QStringLiteral("playback.frameNext"), [engine] { engine()->frameStep(false); }},
        {QStringLiteral("playback.framePrevious"), [engine] { engine()->frameStep(true); }},
        {QStringLiteral("playback.faster"), [this] { setSpeed(stepSpeed(engine_->speed(), +1)); }},
        {QStringLiteral("playback.slower"), [this] { setSpeed(stepSpeed(engine_->speed(), -1)); }},
        {QStringLiteral("playback.speedReset"), [this] { setSpeed(1.0); }},
        {QStringLiteral("playback.abLoop"), [this] { cycleAbLoop(); }},
        {QStringLiteral("playback.abClear"),
         [this] {
             abLoop_.clear();
             engine_->setAbLoop(-1.0, -1.0);
             engine_->showMessage(tr("A-B repeat off"));
         }},
        {QStringLiteral("playback.chapterNext"), [this] { engine_->seekChapter(engine_->currentChapter() + 1); }},
        {QStringLiteral("playback.chapterPrevious"),
         [this] { engine_->seekChapter(std::max(0, engine_->currentChapter() - 1)); }},
        {QStringLiteral("playback.cycleRepeat"),
         [this] { setRepeat(static_cast<Playlist::Repeat>((static_cast<int>(playlist_.repeat()) + 1) % 3)); }},
        {QStringLiteral("playback.shuffle"), [this] { setShuffle(!playlist_.shuffle()); }},
        {QStringLiteral("playback.resume"),
         [this] {
             resumeEnabled_ = action(QStringLiteral("playback.resume"))->isChecked();
             notify(resumeEnabled_ ? tr("Resume playback on") : tr("Resume playback off"));
         }},
        {QStringLiteral("audio.volumeUp"), [this] { changeVolumeBy(5); }},
        {QStringLiteral("audio.volumeDown"), [this] { changeVolumeBy(-5); }},
        {QStringLiteral("audio.mute"), [engine] { engine()->setMuted(!engine()->isMuted()); }},
        {QStringLiteral("audio.cycleTrack"), [this] { cycleTrack(TrackType::Audio); }},
        {QStringLiteral("audio.delayPlus"), [this] { setAudioDelay(stepDelay(engine_->audioDelay(), kDelayStep)); }},
        {QStringLiteral("audio.delayMinus"), [this] { setAudioDelay(stepDelay(engine_->audioDelay(), -kDelayStep)); }},
        {QStringLiteral("audio.delayReset"), [this] { setAudioDelay(0.0); }},
        {QStringLiteral("subtitle.cycleTrack"), [this] { cycleTrack(TrackType::Subtitle); }},
        {QStringLiteral("subtitle.delayPlus"), [this] { setSubtitleDelay(stepDelay(engine_->subtitleDelay(), kDelayStep)); }},
        {QStringLiteral("subtitle.delayMinus"), [this] { setSubtitleDelay(stepDelay(engine_->subtitleDelay(), -kDelayStep)); }},
        {QStringLiteral("subtitle.delayReset"), [this] { setSubtitleDelay(0.0); }},
        {QStringLiteral("subtitle.style"), [this] { showSubtitleStyleDialog(); }},
        {QStringLiteral("video.fullscreen"), [this] { setFullscreen(!isFullScreen()); }},
        {QStringLiteral("video.exitFullscreen"), [this] { setFullscreen(false); }},
        {QStringLiteral("video.screenshot"), [this] { takeScreenshot(true); }},
        {QStringLiteral("video.screenshotClean"), [this] { takeScreenshot(false); }},
        {QStringLiteral("video.cycleAspect"),
         [this] {
             int i = 0;
             for (int n = 0; n < std::ssize(kAspects); ++n) {
                 if (qFuzzyCompare(1.0 + kAspects[n].ratio, 1.0 + aspect_)) i = n;
             }
             setAspect(kAspects[(i + 1) % std::ssize(kAspects)].ratio);
         }},
        {QStringLiteral("video.rotate"), [this] { setRotation(rotation_ + 90); }},
        {QStringLiteral("video.flipH"), [this] { setFlip(!flipH_, flipV_); }},
        {QStringLiteral("video.flipV"), [this] { setFlip(flipH_, !flipV_); }},
        {QStringLiteral("video.onTop"), [this] { setAlwaysOnTop(!onTop_); }},
        {QStringLiteral("video.borderless"), [this] { setBorderless(!borderless_); }},
        {QStringLiteral("view.playlist"), [this] { setPlaylistVisible(!playlistPanel_->isVisible()); }},
        {QStringLiteral("tools.shortcuts"), [this] { showShortcutDialog(); }},
        {QStringLiteral("help.about"), [this] { QMessageBox::about(this, tr("About Y-Player"), aboutText_); }},
    };

    const QStringList checkable{QStringLiteral("playback.shuffle"), QStringLiteral("playback.resume"),
                                QStringLiteral("audio.mute"),       QStringLiteral("video.flipH"),
                                QStringLiteral("video.flipV"),      QStringLiteral("video.onTop"),
                                QStringLiteral("video.borderless"), QStringLiteral("video.fullscreen"),
                                QStringLiteral("view.playlist")};
    for (const ActionDef& def : shortcuts_.definitions()) {
        auto* a = new QAction(def.text, this);
        a->setCheckable(checkable.contains(def.id));
        const auto handler = handlers_.value(def.id);
        Q_ASSERT_X(handler, "buildActions", qPrintable(def.id));
        connect(a, &QAction::triggered, this, [handler] { handler(); });
        addAction(a);  // stays active while the menu bar is hidden in fullscreen
        actions_.insert(def.id, a);
    }
    action(QStringLiteral("playback.stop"))->setIcon(glyphIcon(Glyph::Stop));
    action(QStringLiteral("playback.previous"))->setIcon(glyphIcon(Glyph::Previous));
    action(QStringLiteral("playback.next"))->setIcon(glyphIcon(Glyph::Next));
    action(QStringLiteral("audio.mute"))->setIcon(glyphIcon(Glyph::Volume));
    action(QStringLiteral("video.fullscreen"))->setIcon(glyphIcon(Glyph::Fullscreen));
    action(QStringLiteral("view.playlist"))->setIcon(glyphIcon(Glyph::Playlist));
    action(QStringLiteral("video.flipH"))->setToolTip(tr("Runs on the CPU: with hardware decoding it adds a GPU-to-CPU copy per frame"));
    action(QStringLiteral("video.flipV"))->setToolTip(action(QStringLiteral("video.flipH"))->toolTip());
    applyShortcuts();
}

void MainWindow::applyShortcuts() {
    for (auto it = actions_.cbegin(); it != actions_.cend(); ++it) it.value()->setShortcuts(shortcuts_.shortcuts(it.key()));
}

void MainWindow::buildMenus() {
    const auto add = [this](QMenu* menu, const char* id) { menu->addAction(action(QString::fromLatin1(id))); };

    QMenu* file = menuBar()->addMenu(tr("&File"));
    add(file, "file.open");
    add(file, "file.add");
    QMenu* recent = file->addMenu(tr("&Recent Files"));
    connect(recent, &QMenu::aboutToShow, this, [this, recent] { fillRecentMenu(recent); });
    file->addSeparator();
    add(file, "file.loadPlaylist");
    add(file, "file.savePlaylist");
    file->addSeparator();
    add(file, "file.quit");

    QMenu* playback = menuBar()->addMenu(tr("&Playback"));
    for (const char* id : {"playback.toggle", "playback.stop", "playback.previous", "playback.next"}) add(playback, id);
    playback->addSeparator();
    for (const char* id : {"playback.seekForward", "playback.seekBackward", "playback.seekForwardLong",
                           "playback.seekBackwardLong", "playback.frameNext", "playback.framePrevious"}) {
        add(playback, id);
    }
    playback->addSeparator();
    QMenu* speed = playback->addMenu(tr("S&peed"));
    connect(speed, &QMenu::aboutToShow, this, [this, speed] { fillSpeedMenu(speed); });
    QMenu* ab = playback->addMenu(tr("&A-B Repeat"));
    add(ab, "playback.abLoop");
    add(ab, "playback.abClear");
    QMenu* chapters = playback->addMenu(tr("&Chapters"));
    connect(chapters, &QMenu::aboutToShow, this, [this, chapters] { fillChapterMenu(chapters); });
    playback->addSeparator();
    repeatMenu_ = playback->addMenu(tr("Re&peat"));
    auto* repeatGroup = new QActionGroup(repeatMenu_);
    const QStringList repeatNames{tr("Off"), tr("Repeat One"), tr("Repeat All")};
    for (int i = 0; i < 3; ++i) {
        QAction* a = repeatMenu_->addAction(repeatNames.at(i), this, [this, i] { setRepeat(static_cast<Playlist::Repeat>(i)); });
        a->setCheckable(true);
        a->setData(i);
        repeatGroup->addAction(a);
    }
    repeatMenu_->addSeparator();
    add(repeatMenu_, "playback.cycleRepeat");
    add(playback, "playback.shuffle");
    add(playback, "playback.resume");

    QMenu* audio = menuBar()->addMenu(tr("&Audio"));
    QMenu* audioTracks = audio->addMenu(tr("&Track"));
    connect(audioTracks, &QMenu::aboutToShow, this, [this, audioTracks] { fillTrackMenu(audioTracks, TrackType::Audio); });
    add(audio, "audio.cycleTrack");
    audio->addSeparator();
    for (const char* id : {"audio.volumeUp", "audio.volumeDown", "audio.mute"}) add(audio, id);
    audio->addSeparator();
    for (const char* id : {"audio.delayPlus", "audio.delayMinus", "audio.delayReset"}) add(audio, id);
    audio->addSeparator();
    QMenu* devices = audio->addMenu(tr("Output &Device"));
    connect(devices, &QMenu::aboutToShow, this, [this, devices] { fillDeviceMenu(devices); });
    channelsMenu_ = audio->addMenu(tr("&Channels"));
    auto* channelGroup = new QActionGroup(channelsMenu_);
    const QList<std::pair<QString, QString>> layouts{{QStringLiteral("auto"), tr("Automatic")},
                                                    {QStringLiteral("stereo"), tr("Stereo")},
                                                    {QStringLiteral("5.1"), tr("5.1 Surround")},
                                                    {QStringLiteral("7.1"), tr("7.1 Surround")},
                                                    {QStringLiteral("mono"), tr("Mono")}};
    for (const auto& [id, name] : layouts) {
        QAction* a = channelsMenu_->addAction(name, this, [this, id] {
            audioChannels_ = id;
            engine_->setAudioChannels(id);
        });
        a->setCheckable(true);
        a->setData(id);
        channelGroup->addAction(a);
    }

    QMenu* subtitles = menuBar()->addMenu(tr("&Subtitles"));
    QMenu* subTracks = subtitles->addMenu(tr("&Track"));
    connect(subTracks, &QMenu::aboutToShow, this, [this, subTracks] { fillTrackMenu(subTracks, TrackType::Subtitle); });
    add(subtitles, "subtitle.cycleTrack");
    add(subtitles, "file.openSubtitle");
    subtitles->addSeparator();
    for (const char* id : {"subtitle.delayPlus", "subtitle.delayMinus", "subtitle.delayReset"}) add(subtitles, id);
    subtitles->addSeparator();
    add(subtitles, "subtitle.style");

    QMenu* video = menuBar()->addMenu(tr("&Video"));
    aspectMenu_ = video->addMenu(tr("&Aspect Ratio"));
    auto* aspectGroup = new QActionGroup(aspectMenu_);
    for (const AspectChoice& c : kAspects) {
        const double ratio = c.ratio;
        QAction* a = aspectMenu_->addAction(ratio > 0.0 ? QString::fromLatin1(c.label) : tr("Default"), this,
                                            [this, ratio] { setAspect(ratio); });
        a->setCheckable(true);
        a->setChecked(ratio == 0.0);
        a->setData(ratio);
        aspectGroup->addAction(a);
    }
    aspectMenu_->addAction(tr("Custom..."), this, [this] {
        bool ok = false;
        const QString text = QInputDialog::getText(this, tr("Custom Aspect Ratio"),
                                                   tr("Width:height or a ratio (e.g. 21:9 or 2.4):"), QLineEdit::Normal,
                                                   QString(), &ok);
        if (!ok) return;
        if (const double r = parseAspectRatio(text); r > 0.0) {
            setAspect(r);
        } else {
            showError(tr("\"%1\" is not a valid aspect ratio").arg(text));
        }
    });
    aspectMenu_->addSeparator();
    add(aspectMenu_, "video.cycleAspect");
    rotateMenu_ = video->addMenu(tr("&Rotate"));
    auto* rotateGroup = new QActionGroup(rotateMenu_);
    for (const int deg : {0, 90, 180, 270}) {
        QAction* a = rotateMenu_->addAction(tr("%1°").arg(deg), this, [this, deg] { setRotation(deg); });
        a->setCheckable(true);
        a->setChecked(deg == 0);
        a->setData(deg);
        rotateGroup->addAction(a);
    }
    rotateMenu_->addSeparator();
    add(rotateMenu_, "video.rotate");
    add(video, "video.flipH");
    add(video, "video.flipV");
    video->addSeparator();
    add(video, "video.screenshot");
    add(video, "video.screenshotClean");
    video->addSeparator();
    for (const char* id : {"video.fullscreen", "video.onTop", "video.borderless"}) add(video, id);

    QMenu* view = menuBar()->addMenu(tr("Vie&w"));
    add(view, "view.playlist");

    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    add(tools, "tools.shortcuts");

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    add(help, "help.about");
}

void MainWindow::fillRecentMenu(QMenu* menu) {
    menu->clear();
    if (recent_.items().isEmpty()) {
        menu->addAction(tr("(empty)"))->setEnabled(false);
        return;
    }
    int n = 1;
    for (const QString& path : recent_.items()) {
        const QString label = QStringLiteral("&%1 %2").arg(n++ % 10).arg(QFileInfo(path).fileName());
        QAction* a = menu->addAction(label, this, [this, path] { openFiles({path}, true); });
        a->setToolTip(path);
    }
    menu->addSeparator();
    menu->addAction(tr("Clear Recent Files"), this, [this] { recent_.clear(); });
}

void MainWindow::fillSpeedMenu(QMenu* menu) {
    menu->clear();
    for (const double s : speedPresets()) {
        QAction* a = menu->addAction(tr("%1×").arg(s, 0, 'g', 3), this, [this, s] { setSpeed(s); });
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(s, engine_->speed()));
    }
    menu->addSeparator();
    for (const char* id : {"playback.faster", "playback.slower", "playback.speedReset"}) {
        menu->addAction(action(QString::fromLatin1(id)));
    }
}

void MainWindow::fillChapterMenu(QMenu* menu) {
    menu->clear();
    const QList<Chapter> chapters = engine_->chapters();
    if (chapters.isEmpty()) menu->addAction(tr("(no chapters)"))->setEnabled(false);
    for (int i = 0; i < chapters.size(); ++i) {
        const Chapter& c = chapters.at(i);
        const QString title = c.title.isEmpty() ? tr("Chapter %1").arg(i + 1) : c.title;
        QAction* a = menu->addAction(QStringLiteral("%1  (%2)").arg(title, formatTime(c.time, engine_->duration())), this,
                                     [this, i] { engine_->seekChapter(i); });
        a->setCheckable(true);
        a->setChecked(i == engine_->currentChapter());
    }
    menu->addSeparator();
    menu->addAction(action(QStringLiteral("playback.chapterNext")));
    menu->addAction(action(QStringLiteral("playback.chapterPrevious")));
}

void MainWindow::fillTrackMenu(QMenu* menu, TrackType type) {
    menu->clear();
    auto* group = new QActionGroup(menu);
    bool anySelected = false;
    QList<Track> list;
    for (const Track& t : engine_->tracks()) {
        if (t.type == type) list << t;
    }
    if (type == TrackType::Subtitle || type == TrackType::Audio) {
        for (const Track& t : list) anySelected = anySelected || t.selected;
        QAction* off = menu->addAction(tr("Off"), this, [this, type] { engine_->selectTrack(type, 0); });
        off->setCheckable(true);
        off->setChecked(!anySelected && !list.isEmpty());
        off->setEnabled(!list.isEmpty());
        group->addAction(off);
    }
    for (const Track& t : list) {
        QAction* a = menu->addAction(trackLabel(t), this, [this, type, id = t.id] { engine_->selectTrack(type, id); });
        a->setCheckable(true);
        a->setChecked(t.selected);
        group->addAction(a);
    }
    if (list.isEmpty()) menu->addAction(tr("(no tracks)"))->setEnabled(false);
}

void MainWindow::fillDeviceMenu(QMenu* menu) {
    menu->clear();
    auto* group = new QActionGroup(menu);
    for (const AudioDevice& d : engine_->audioDevices()) {
        const QString label = d.name == QLatin1String("auto") ? tr("System Default") : d.description;
        QAction* a = menu->addAction(label, this, [this, name = d.name] { engine_->setAudioDevice(name); });
        a->setCheckable(true);
        a->setChecked(d.name == engine_->audioDevice());
        group->addAction(a);
    }
    if (menu->isEmpty()) menu->addAction(tr("(no devices)"))->setEnabled(false);
}

void MainWindow::showContextMenu(const QPoint& globalPos) {
    QMenu menu(this);
    for (QAction* top : menuBar()->actions()) menu.addAction(top);  // same menus, one right-click away
    menu.exec(globalPos);
}

}  // namespace yp::ui
