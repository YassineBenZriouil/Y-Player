#include "ui/VideoWidget.h"

#include <QMouseEvent>

namespace yp::ui {

VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMinimumSize(160, 90);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) Q_EMIT doubleClicked();
    QWidget::mouseDoubleClickEvent(event);
}

}  // namespace yp::ui
