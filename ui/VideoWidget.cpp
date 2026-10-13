#include "ui/VideoWidget.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QWindow>

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

void VideoWidget::mousePressEvent(QMouseEvent* event) {
    pressPos_ = event->button() == Qt::LeftButton ? event->position().toPoint() : QPoint(-1, -1);
    QWidget::mousePressEvent(event);
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event) {
    // Start the move only once the mouse really drags, so double-click still works.
    const bool dragged = pressPos_.x() >= 0 && (event->buttons() & Qt::LeftButton) &&
                         (event->position().toPoint() - pressPos_).manhattanLength() >= QApplication::startDragDistance();
    if (dragMovesWindow_ && dragged && window()->windowHandle() != nullptr) {
        pressPos_ = QPoint(-1, -1);
        window()->windowHandle()->startSystemMove();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void VideoWidget::contextMenuEvent(QContextMenuEvent* event) { Q_EMIT contextMenuRequested(event->globalPos()); }

}  // namespace yp::ui
