#include "ui/SubtitleStyleDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include <utility>

namespace yp::ui {
namespace {

void paintSwatch(QPushButton* button, const QString& color) {
    button->setText(color.toUpper());
    button->setStyleSheet(QStringLiteral("QPushButton { background: %1; color: %2; }")
                              .arg(color, QColor(color).lightness() > 128 ? QStringLiteral("#000") : QStringLiteral("#fff")));
}

}  // namespace

SubtitleStyleDialog::SubtitleStyleDialog(SubtitleStyle style, QWidget* parent)
    : QDialog(parent), style_(std::move(style)) {
    setWindowTitle(tr("Subtitle Style"));
    auto* form = new QFormLayout(this);

    font_ = new QFontComboBox(this);
    font_->setCurrentFont(QFont(style_.font));
    form->addRow(tr("Font:"), font_);

    size_ = new QSpinBox(this);
    size_->setRange(8, 200);
    size_->setValue(style_.size);
    form->addRow(tr("Size:"), size_);

    color_ = new QPushButton(this);
    paintSwatch(color_, style_.color);
    form->addRow(tr("Text color:"), color_);

    outlineColor_ = new QPushButton(this);
    paintSwatch(outlineColor_, style_.outlineColor);
    form->addRow(tr("Outline color:"), outlineColor_);

    outlineSize_ = new QDoubleSpinBox(this);
    outlineSize_->setRange(0.0, 20.0);
    outlineSize_->setSingleStep(0.5);
    outlineSize_->setValue(style_.outlineSize);
    form->addRow(tr("Outline size:"), outlineSize_);

    position_ = new QSlider(Qt::Horizontal, this);
    position_->setRange(0, 150);
    position_->setValue(style_.position);
    position_->setToolTip(tr("0 = top of the video, 100 = bottom edge, above 100 = below the video"));
    form->addRow(tr("Vertical position:"), position_);

    override_ = new QCheckBox(tr("Also apply to styled (ASS/SSA) subtitles"), this);
    override_->setChecked(style_.overrideStyled);
    form->addRow(override_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    form->addRow(buttons);

    connect(font_, &QFontComboBox::currentFontChanged, this, &SubtitleStyleDialog::update);
    connect(size_, &QSpinBox::valueChanged, this, &SubtitleStyleDialog::update);
    connect(outlineSize_, &QDoubleSpinBox::valueChanged, this, &SubtitleStyleDialog::update);
    connect(position_, &QSlider::valueChanged, this, &SubtitleStyleDialog::update);
    connect(override_, &QCheckBox::toggled, this, &SubtitleStyleDialog::update);
    connect(color_, &QPushButton::clicked, this, [this] { pickColor(style_.color, color_); });
    connect(outlineColor_, &QPushButton::clicked, this, [this] { pickColor(style_.outlineColor, outlineColor_); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        const SubtitleStyle d;
        style_.color = d.color;
        style_.outlineColor = d.outlineColor;
        paintSwatch(color_, d.color);
        paintSwatch(outlineColor_, d.outlineColor);
        font_->setCurrentFont(QFont(d.font));
        size_->setValue(d.size);
        outlineSize_->setValue(d.outlineSize);
        position_->setValue(d.position);
        override_->setChecked(d.overrideStyled);
        update();
    });
}

void SubtitleStyleDialog::pickColor(QString& target, QPushButton* button) {
    const QColor chosen = QColorDialog::getColor(QColor(target), this, tr("Choose Color"));
    if (!chosen.isValid()) return;
    target = chosen.name();
    paintSwatch(button, target);
    update();
}

void SubtitleStyleDialog::update() {
    style_.font = font_->currentFont().family();
    style_.size = size_->value();
    style_.outlineSize = outlineSize_->value();
    style_.position = position_->value();
    style_.overrideStyled = override_->isChecked();
    Q_EMIT styleChanged(style_);
}

}  // namespace yp::ui
