#include "ui/MainWindow.h"

#include <QMenuBar>
#include <QMessageBox>

#include <utility>

namespace yp::ui {

MainWindow::MainWindow(QString aboutText, QWidget* parent) : QMainWindow(parent), aboutText_(std::move(aboutText)) {
    setWindowTitle(QStringLiteral("Y-Player"));
    resize(1280, 720);

    // Video area; becomes the native surface libmpv renders into in Phase 2.
    auto* video = new QWidget(this);
    video->setAutoFillBackground(true);
    QPalette palette = video->palette();
    palette.setColor(QPalette::Window, Qt::black);
    video->setPalette(palette);
    setCentralWidget(video);

    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("E&xit"), QKeySequence(Qt::CTRL | Qt::Key_Q), this, &QWidget::close);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About Y-Player"), this,
                    [this] { QMessageBox::about(this, tr("About Y-Player"), aboutText_); });
}

}  // namespace yp::ui
