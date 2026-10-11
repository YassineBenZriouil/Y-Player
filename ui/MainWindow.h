#pragma once

#include <QMainWindow>
#include <QString>

namespace yp::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // aboutText: version/build details shown by Help > About (assembled by the app layer,
    // so the UI never talks to the engine directly).
    explicit MainWindow(QString aboutText, QWidget* parent = nullptr);

private:
    QString aboutText_;
};

}  // namespace yp::ui
