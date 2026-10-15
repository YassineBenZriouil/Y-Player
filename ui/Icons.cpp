#include "ui/Icons.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

namespace yp::ui {

QIcon glyphIcon(Glyph glyph) {
    static const QString family = QFontDatabase::families().contains(QStringLiteral("Segoe Fluent Icons"))
                                      ? QStringLiteral("Segoe Fluent Icons")
                                      : QStringLiteral("Segoe MDL2 Assets");
    const QPalette palette = QApplication::palette();
    const QString text(QChar(static_cast<char16_t>(glyph)));
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
        const QColor color = palette.color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active,
                                           QPalette::ButtonText);
        for (const int size : {16, 20, 24, 32, 48}) {  // sizes cover 100-200 % display scaling
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::TextAntialiasing);
            QFont font(family);
            font.setPixelSize(size * 3 / 4);
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
            painter.end();
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}

}  // namespace yp::ui
