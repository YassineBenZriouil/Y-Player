#include "ui/SeekSlider.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

namespace yp::ui {

SeekSlider::SeekSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
    setRange(0, 0);
    setFocusPolicy(Qt::TabFocus);  // reachable with Tab; a click leaves the arrow keys to the window
    setMouseTracking(true);        // hover time preview
}

int SeekSlider::valueAt(const QPointF& pos) const {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    const int span = groove.width() - handle.width();
    const int x = static_cast<int>(pos.x()) - groove.x() - handle.width() / 2;
    return QStyle::sliderValueFromPosition(minimum(), maximum(), x, span);
}

void SeekSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || maximum() <= minimum()) {
        QSlider::mousePressEvent(event);
        return;
    }
    dragging_ = true;
    setSliderDown(true);
    setValue(valueAt(event->position()));
    Q_EMIT seekRequested(value(), false);
    event->accept();
}

void SeekSlider::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        if (maximum() > minimum()) Q_EMIT hovered(valueAt(event->position()), event->globalPosition().toPoint());
        QSlider::mouseMoveEvent(event);
        return;
    }
    const int v = valueAt(event->position());
    if (v != value()) {
        setValue(v);
        Q_EMIT seekRequested(v, false);
    }
    event->accept();
}

void SeekSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (!dragging_) {
        QSlider::mouseReleaseEvent(event);
        return;
    }
    dragging_ = false;
    setSliderDown(false);
    Q_EMIT seekRequested(value(), true);
    event->accept();
}

void SeekSlider::leaveEvent(QEvent* event) {
    Q_EMIT hoverEnded();
    QSlider::leaveEvent(event);
}

void SeekSlider::keyPressEvent(QKeyEvent* event) {
    if (maximum() <= minimum()) {
        QSlider::keyPressEvent(event);
        return;
    }
    int target = value();
    switch (event->key()) {
        case Qt::Key_Left: target -= pageStep(); break;
        case Qt::Key_Right: target += pageStep(); break;
        case Qt::Key_Home: target = minimum(); break;
        case Qt::Key_End: target = maximum(); break;
        default: QSlider::keyPressEvent(event); return;
    }
    setValue(target);  // clamped by QSlider
    Q_EMIT seekRequested(value(), true);
    event->accept();
}

}  // namespace yp::ui
