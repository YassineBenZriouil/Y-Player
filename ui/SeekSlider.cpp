#include "ui/SeekSlider.h"

#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

namespace yp::ui {

SeekSlider::SeekSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
    setRange(0, 0);
    setFocusPolicy(Qt::NoFocus);  // arrow keys belong to the window's seek shortcuts
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

}  // namespace yp::ui
