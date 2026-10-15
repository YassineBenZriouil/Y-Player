#pragma once

#include <QDialog>
#include <QTimer>

class QTableWidget;

namespace yp {
class IPlaybackEngine;
}

namespace yp::ui {

// Live engine statistics (decoder, renderer, formats, frame drops...). Refreshes twice a second
// only while visible, so it costs nothing when closed.
class DiagnosticsDialog : public QDialog {
    Q_OBJECT
public:
    DiagnosticsDialog(const IPlaybackEngine& engine, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void refresh();

    const IPlaybackEngine& engine_;
    QTableWidget* table_ = nullptr;
    QTimer timer_;
};

}  // namespace yp::ui
