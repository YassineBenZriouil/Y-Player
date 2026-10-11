#pragma once

#include <QSlider>

namespace yp::ui {

// Horizontal millisecond slider that jumps to the clicked point. While dragging it asks for
// fast keyframe seeks; on release it asks for one exact seek.
class SeekSlider : public QSlider {
    Q_OBJECT
public:
    explicit SeekSlider(QWidget* parent = nullptr);

    [[nodiscard]] bool isDragging() const { return dragging_; }

Q_SIGNALS:
    void seekRequested(int milliseconds, bool exact);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] int valueAt(const QPointF& pos) const;

    bool dragging_ = false;
};

}  // namespace yp::ui
