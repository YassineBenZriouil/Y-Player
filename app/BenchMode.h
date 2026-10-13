#pragma once

#include "core/PlaybackEngine.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QTimer>

namespace yp::ui {
class MainWindow;
}

namespace yp::app {

// `Y-Player --bench-report out.json [--bench-play S] [--bench-warmup S] FILE`
// Plays FILE in the real app window, measures what only the process itself can see
// (startup, first frame, frame drops, A/V sync, seek latency), writes JSON and exits.
// CPU/GPU/memory are sampled externally by bench/run_bench.py, identically for every player.
struct BenchOptions {
    QString reportPath;
    QString file;
    int warmupSeconds = 3;
    int playSeconds = 12;
};

class BenchRun : public QObject {
    Q_OBJECT
public:
    BenchRun(ui::MainWindow& window, BenchOptions options);
    void start();  // call after the window is shown

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onRestarted();
    void beginMeasureWindow();
    void endMeasureWindow();
    void nextSeek();
    void finish(const QString& error = {});

    ui::MainWindow& window_;
    IPlaybackEngine& engine_;
    BenchOptions options_;
    qint64 startupMs_ = -1;
    qint64 openAtMs_ = -1;
    qint64 firstFrameMs_ = -1;
    PlaybackStats windowStart_;
    PlaybackStats windowEnd_;
    double avsyncMaxAbs_ = 0.0;
    int avsyncSamples_ = 0;
    QTimer avsyncSampler_;
    QJsonArray seeks_;
    int seekIndex_ = -1;
    QElapsedTimer seekClock_;
    QTimer seekTimeout_;
    bool finished_ = false;
};

}  // namespace yp::app
