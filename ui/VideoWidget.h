#pragma once

#include <QWidget>

namespace yp::ui {

// Native child window the engine presents into (ADR-003). Qt never paints it; the
// backend owns every pixel, including the black idle background.
class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    [[nodiscard]] QPaintEngine* paintEngine() const override { return nullptr; }
    // Borderless windows have no title bar; let a left-drag on the video move the window.
    void setDragMovesWindow(bool on) { dragMovesWindow_ = on; }

Q_SIGNALS:
    void doubleClicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    bool dragMovesWindow_ = false;
    QPoint pressPos_{-1, -1};
};

}  // namespace yp::ui
