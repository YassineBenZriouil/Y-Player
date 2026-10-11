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

Q_SIGNALS:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

}  // namespace yp::ui
