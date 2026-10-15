#include "ui/Theme.h"

#include "platform/win/WindowStyle.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace yp::ui {
namespace {

// Brand colours (ADR-007/018).
const QColor kAccent(0x19, 0xC3, 0xB4);

QPalette darkPalette() {
    QPalette p;
    const QColor window(0x17, 0x19, 0x1E);
    const QColor base(0x0E, 0x0F, 0x12);
    const QColor text(0xE6, 0xE8, 0xEC);
    const QColor muted(0x7D, 0x83, 0x8C);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x22, 0x25, 0x2B));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, QColor(0x22, 0x25, 0x2B));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Highlight, kAccent);
    p.setColor(QPalette::HighlightedText, base);
    p.setColor(QPalette::Link, kAccent);
    p.setColor(QPalette::Light, QColor(0x33, 0x37, 0x3F));
    p.setColor(QPalette::Midlight, QColor(0x2A, 0x2D, 0x34));
    p.setColor(QPalette::Mid, QColor(0x22, 0x25, 0x2B));
    p.setColor(QPalette::Dark, QColor(0x0B, 0x0C, 0x0E));
    p.setColor(QPalette::Shadow, Qt::black);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, muted);
    }
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3A, 0x3E, 0x46));
    return p;
}

QPalette lightPalette() {
    QPalette p;
    const QColor text(0x16, 0x18, 0x1C);
    const QColor muted(0x8A, 0x90, 0x99);
    p.setColor(QPalette::Window, QColor(0xF3, 0xF4, 0xF6));
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor(0xF3, 0xF4, 0xF6));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0xE9, 0xEB, 0xEE));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Highlight, QColor(0x0E, 0x8F, 0x84));  // darker teal: readable white text on it
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x0E, 0x8F, 0x84));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, muted);
    }
    return p;
}

}  // namespace

bool applyTheme(QApplication& app, const QString& mode) {
    const QString family = QFontDatabase::families().contains(QStringLiteral("Segoe UI Variable Text"))
                               ? QStringLiteral("Segoe UI Variable Text")
                               : QStringLiteral("Segoe UI");
    QFont font = QApplication::font();
    font.setFamily(family);
    QApplication::setFont(font);

    if (platform::highContrastEnabled()) {
        // Accessibility first: keep the native style and the user's high-contrast colours.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("windows11")));
        QApplication::setPalette(QApplication::style()->standardPalette());
        app.setStyleSheet(QString());
        return QApplication::palette().color(QPalette::Window).lightness() < 128;
    }

    const bool dark = mode == QLatin1String("dark") ||
                      (mode != QLatin1String("light") && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(dark ? darkPalette() : lightPalette());
    // Accent the two sliders users touch most; everything else stays palette-driven.
    app.setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: palette(mid); }"
        "QSlider::sub-page:horizontal { border-radius: 2px; background: palette(highlight); }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px;"
        "  background: palette(highlight); }"
        "QSlider::handle:horizontal:focus { border: 2px solid palette(text); }"));
    return dark;
}

}  // namespace yp::ui
