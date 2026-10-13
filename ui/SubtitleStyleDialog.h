#pragma once

#include "core/PlaybackEngine.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QFontComboBox;
class QPushButton;
class QSlider;
class QSpinBox;

namespace yp::ui {

// Edits a SubtitleStyle with live preview: styleChanged() fires on every edit so the caller can
// apply it immediately, and restore the original if the dialog is cancelled.
class SubtitleStyleDialog : public QDialog {
    Q_OBJECT
public:
    SubtitleStyleDialog(SubtitleStyle style, QWidget* parent = nullptr);

    [[nodiscard]] SubtitleStyle subtitleStyle() const { return style_; }

Q_SIGNALS:
    void styleChanged(const yp::SubtitleStyle& style);

private:
    void pickColor(QString& target, QPushButton* button);
    void update();

    SubtitleStyle style_;
    QFontComboBox* font_ = nullptr;
    QSpinBox* size_ = nullptr;
    QPushButton* color_ = nullptr;
    QPushButton* outlineColor_ = nullptr;
    QDoubleSpinBox* outlineSize_ = nullptr;
    QSlider* position_ = nullptr;
    QCheckBox* override_ = nullptr;
};

}  // namespace yp::ui
