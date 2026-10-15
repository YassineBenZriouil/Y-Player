#pragma once

#include "core/PlayerSettings.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace yp::ui {

// Searchable settings window (INIT.md §11 categories). Edits a copy; applied() hands the result
// to the window on Apply/OK. File associations act immediately from their own page.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(PlayerSettings settings, QWidget* parent = nullptr);

    void setSettings(const PlayerSettings& settings);  // e.g. after an import
    [[nodiscard]] PlayerSettings collect() const;

Q_SIGNALS:
    void applied(const yp::PlayerSettings& settings);
    void editShortcutsRequested();
    void exportRequested();
    void importRequested();
    void showDiagnosticsRequested();
    void openLogFolderRequested();

private:
    QFormLayout* addPage(const QString& title);
    void addRow(QFormLayout* form, const QString& label, QWidget* field, const QString& help = {});
    void filter(const QString& text);
    QWidget* buildEqualizer();
    QWidget* buildAssociations();
    void refreshAssociations();

    struct Row {
        QFormLayout* form;
        QWidget* field;
        QString text;  // label + help, lower-cased, for search
        int page;
    };

    PlayerSettings base_;  // fields not shown here (volume, mute) pass through unchanged
    QList<Row> rows_;
    QStringList pageTitles_;
    QListWidget* categories_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLineEdit* search_ = nullptr;

    QCheckBox* resume_ = nullptr;
    QCheckBox* onTop_ = nullptr;
    QDoubleSpinBox* seekShort_ = nullptr;
    QDoubleSpinBox* seekLong_ = nullptr;
    QCheckBox* exactSeek_ = nullptr;
    std::array<QSpinBox*, 5> picture_{};  // brightness, contrast, saturation, gamma, hue
    QComboBox* deinterlace_ = nullptr;
    QComboBox* hwdec_ = nullptr;
    QComboBox* scaler_ = nullptr;
    QComboBox* toneMapping_ = nullptr;
    QCheckBox* hdrPeak_ = nullptr;
    QCheckBox* hdrPassthrough_ = nullptr;
    QCheckBox* smoothMotion_ = nullptr;
    QComboBox* channels_ = nullptr;
    QCheckBox* normalize_ = nullptr;
    QCheckBox* normalizeDownmix_ = nullptr;
    QLineEdit* audioLanguages_ = nullptr;
    QLineEdit* subtitleLanguages_ = nullptr;
    QComboBox* subtitleAutoLoad_ = nullptr;
    QSpinBox* sharpen_ = nullptr;
    QComboBox* denoise_ = nullptr;
    QCheckBox* equalizerOn_ = nullptr;
    QComboBox* equalizerPreset_ = nullptr;
    std::array<QSlider*, kEqualizerBands> bands_{};
    QCheckBox* restoreSession_ = nullptr;
    QCheckBox* autoplayNext_ = nullptr;
    QListWidget* associations_ = nullptr;
    QLabel* associationStatus_ = nullptr;
    QSpinBox* cacheMb_ = nullptr;
    QSpinBox* decoderThreads_ = nullptr;
    QComboBox* logLevel_ = nullptr;
    SubtitleStyle subtitleStyle_;
};

}  // namespace yp::ui
