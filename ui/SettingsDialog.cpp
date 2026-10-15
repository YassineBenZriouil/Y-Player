#include "ui/SettingsDialog.h"

#include "platform/win/FileAssociations.h"
#include "ui/SubtitleStyleDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <utility>

namespace yp::ui {
namespace {

QComboBox* combo(QWidget* parent, const QList<std::pair<QString, QString>>& items) {  // (data, text)
    auto* c = new QComboBox(parent);
    for (const auto& [data, text] : items) c->addItem(text, data);
    return c;
}

void select(QComboBox* c, const QString& data) {
    const int i = c->findData(data);
    c->setCurrentIndex(i >= 0 ? i : 0);
}

QString selected(const QComboBox* c) { return c->currentData().toString(); }

QSpinBox* spin(QWidget* parent, int lo, int hi, const QString& suffix = {}) {
    auto* s = new QSpinBox(parent);
    s->setRange(lo, hi);
    s->setSuffix(suffix);
    return s;
}

struct EqPreset {
    const char* name;
    std::array<int, kEqualizerBands> gains;
};
constexpr EqPreset kEqPresets[] = {
    {QT_TRANSLATE_NOOP("Settings", "Flat"), {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {QT_TRANSLATE_NOOP("Settings", "Bass boost"), {6, 5, 4, 2, 0, 0, 0, 0, 0, 0}},
    {QT_TRANSLATE_NOOP("Settings", "Treble boost"), {0, 0, 0, 0, 0, 1, 2, 4, 5, 6}},
    {QT_TRANSLATE_NOOP("Settings", "Voice"), {-2, -1, 0, 2, 4, 4, 2, 0, -1, -2}},
    {QT_TRANSLATE_NOOP("Settings", "Loudness"), {4, 3, 1, 0, -1, -1, 0, 1, 3, 4}},
};

}  // namespace

SettingsDialog::SettingsDialog(PlayerSettings settings, QWidget* parent) : QDialog(parent), base_(std::move(settings)) {
    setWindowTitle(tr("Settings"));
    resize(820, 600);

    auto* root = new QVBoxLayout(this);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Search settings"));
    search_->setClearButtonEnabled(true);
    search_->setAccessibleName(tr("Search settings"));
    root->addWidget(search_);

    auto* body = new QHBoxLayout;
    categories_ = new QListWidget(this);
    categories_->setFixedWidth(200);
    categories_->setAccessibleName(tr("Categories"));
    pages_ = new QStackedWidget(this);
    body->addWidget(categories_);
    body->addWidget(pages_, 1);
    root->addLayout(body, 1);
    connect(categories_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(search_, &QLineEdit::textChanged, this, &SettingsDialog::filter);

    // --- General
    QFormLayout* general = addPage(tr("General"));
    resume_ = new QCheckBox(tr("Continue files where they were stopped"), this);
    addRow(general, tr("Resume playback"), resume_, tr("Remembers positions of files longer than one minute"));
    onTop_ = new QCheckBox(tr("Keep the player above other windows"), this);
    addRow(general, tr("Always on top"), onTop_);
    auto* transfer = new QWidget(this);
    auto* transferRow = new QHBoxLayout(transfer);
    transferRow->setContentsMargins(0, 0, 0, 0);
    auto* exportButton = new QPushButton(tr("Export..."), transfer);
    auto* importButton = new QPushButton(tr("Import..."), transfer);
    transferRow->addWidget(exportButton);
    transferRow->addWidget(importButton);
    transferRow->addStretch();
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::exportRequested);
    connect(importButton, &QPushButton::clicked, this, &SettingsDialog::importRequested);
    addRow(general, tr("Settings file"), transfer, tr("Export or import all settings, shortcuts and profiles"));

    // --- Playback
    QFormLayout* playback = addPage(tr("Playback"));
    seekShort_ = new QDoubleSpinBox(this);
    seekShort_->setRange(1.0, 600.0);
    seekShort_->setSuffix(tr(" s"));
    addRow(playback, tr("Short seek (arrow keys)"), seekShort_);
    seekLong_ = new QDoubleSpinBox(this);
    seekLong_->setRange(1.0, 3600.0);
    seekLong_->setSuffix(tr(" s"));
    addRow(playback, tr("Long seek (Shift + arrows)"), seekLong_);
    exactSeek_ = new QCheckBox(tr("Land on the exact frame"), this);
    addRow(playback, tr("Exact seeking"), exactSeek_,
           tr("Off jumps to the nearest keyframe: faster on files with long keyframe intervals"));

    // --- Video
    QFormLayout* video = addPage(tr("Video"));
    const QStringList pictureNames{tr("Brightness"), tr("Contrast"), tr("Saturation"), tr("Gamma"), tr("Hue")};
    for (int i = 0; i < 5; ++i) {
        picture_.at(i) = spin(this, -100, 100);
        addRow(video, pictureNames.at(i), picture_.at(i), tr("Applied by the GPU renderer: no measurable cost"));
    }
    deinterlace_ = combo(this, {{QStringLiteral("auto"), tr("Automatic (interlaced files only)")},
                                {QStringLiteral("yes"), tr("Always")},
                                {QStringLiteral("no"), tr("Never")}});
    addRow(video, tr("Deinterlacing"), deinterlace_, tr("Uses the GPU's video processor when hardware decoding is on"));

    // --- Hardware acceleration
    QFormLayout* hw = addPage(tr("Hardware acceleration"));
    hwdec_ = combo(this, {{QStringLiteral("auto"), tr("Automatic (recommended)")},
                          {QStringLiteral("d3d11va"), tr("D3D11 (zero-copy)")},
                          {QStringLiteral("d3d11va-copy"), tr("D3D11 with copy-back (compatible with CPU filters)")},
                          {QStringLiteral("nvdec"), tr("NVIDIA NVDEC")},
                          {QStringLiteral("no"), tr("Off (software decoding)")}});
    addRow(hw, tr("Hardware decoding"), hwdec_,
           tr("Automatic falls back to software for formats the GPU cannot decode. The diagnostics panel shows "
              "which decoder is actually active."));

    // --- Rendering
    QFormLayout* rendering = addPage(tr("Rendering"));
    scaler_ = combo(this, {{QStringLiteral("bilinear"), tr("Bilinear (fastest)")},
                           {QStringLiteral("spline36"), tr("Spline36 (balanced)")},
                           {QStringLiteral("lanczos"), tr("Lanczos (sharper)")},
                           {QStringLiteral("ewa_lanczossharp"), tr("EWA Lanczos sharp (best, highest GPU cost)")}});
    addRow(rendering, tr("Upscaler"), scaler_, tr("Higher quality scalers cost more GPU time per frame"));
    toneMapping_ = combo(this, {{QStringLiteral("auto"), tr("Automatic")},
                                {QStringLiteral("bt.2390"), tr("BT.2390")},
                                {QStringLiteral("spline"), tr("Spline")},
                                {QStringLiteral("hable"), tr("Hable (filmic)")},
                                {QStringLiteral("mobius"), tr("Mobius")},
                                {QStringLiteral("reinhard"), tr("Reinhard")},
                                {QStringLiteral("clip"), tr("Clip (no compression)")}});
    addRow(rendering, tr("HDR tone mapping"), toneMapping_, tr("How HDR video is converted for SDR displays"));
    hdrPeak_ = new QCheckBox(tr("Measure brightness per scene"), this);
    addRow(rendering, tr("HDR peak detection"), hdrPeak_, tr("Better HDR tone mapping for a small GPU cost"));
    hdrPassthrough_ = new QCheckBox(tr("Send HDR to HDR-enabled displays"), this);
    addRow(rendering, tr("HDR passthrough"), hdrPassthrough_, tr("Needs Windows HDR turned on for the display"));
    smoothMotion_ = new QCheckBox(tr("Resample to the display refresh rate"), this);
    addRow(rendering, tr("Smooth motion"), smoothMotion_,
           tr("Removes judder (e.g. 24 fps on 60 Hz); extra GPU work on every refresh"));

    // --- Audio
    QFormLayout* audio = addPage(tr("Audio"));
    channels_ = combo(this, {{QStringLiteral("auto"), tr("Automatic")},
                             {QStringLiteral("stereo"), tr("Stereo")},
                             {QStringLiteral("5.1"), tr("5.1 Surround")},
                             {QStringLiteral("7.1"), tr("7.1 Surround")},
                             {QStringLiteral("mono"), tr("Mono")}});
    addRow(audio, tr("Channels"), channels_);
    normalize_ = new QCheckBox(tr("Even out loud and quiet passages"), this);
    addRow(audio, tr("Volume normalization"), normalize_, tr("Dynamic normalization; negligible CPU"));
    normalizeDownmix_ = new QCheckBox(tr("Prevent clipping when mixing surround down to stereo"), this);
    addRow(audio, tr("Downmix normalization"), normalizeDownmix_);
    audioLanguages_ = new QLineEdit(this);
    audioLanguages_->setPlaceholderText(tr("e.g. jpn,eng"));
    addRow(audio, tr("Preferred audio languages"), audioLanguages_, tr("Comma-separated language codes, first match wins"));

    // --- Subtitles
    QFormLayout* subtitles = addPage(tr("Subtitles"));
    subtitleLanguages_ = new QLineEdit(this);
    subtitleLanguages_->setPlaceholderText(tr("e.g. eng,fre"));
    addRow(subtitles, tr("Preferred subtitle languages"), subtitleLanguages_);
    subtitleAutoLoad_ = combo(this, {{QStringLiteral("fuzzy"), tr("Files that start with the video's name")},
                                     {QStringLiteral("exact"), tr("Files with exactly the video's name")},
                                     {QStringLiteral("no"), tr("Never")}});
    addRow(subtitles, tr("Load external subtitles"), subtitleAutoLoad_, tr("Subtitle files in the video's folder"));
    auto* styleButton = new QPushButton(tr("Edit style..."), this);
    connect(styleButton, &QPushButton::clicked, this, [this] {
        SubtitleStyleDialog dialog(subtitleStyle_, this);
        if (dialog.exec() == QDialog::Accepted) subtitleStyle_ = dialog.subtitleStyle();
    });
    addRow(subtitles, tr("Font, colors and position"), styleButton);

    // --- Filters
    QFormLayout* filters = addPage(tr("Filters"));
    sharpen_ = spin(this, 0, 100, tr(" %"));
    addRow(filters, tr("Sharpen"), sharpen_, tr("GPU shader; small cost. 0 turns it off"));
    denoise_ = combo(this, {{QStringLiteral("0"), tr("Off")},
                            {QStringLiteral("1"), tr("Low")},
                            {QStringLiteral("2"), tr("Medium")},
                            {QStringLiteral("3"), tr("High")}});
    addRow(filters, tr("Denoise"), denoise_,
           tr("CPU filter: with hardware decoding every frame is copied to the CPU. High cost at 4K"));
    equalizerOn_ = new QCheckBox(tr("Enabled"), this);
    addRow(filters, tr("Equalizer"), equalizerOn_, tr("10-band audio equalizer; negligible CPU"));
    addRow(filters, tr("Equalizer bands"), buildEqualizer());

    // --- Keyboard shortcuts
    QFormLayout* keys = addPage(tr("Keyboard shortcuts"));
    auto* keysButton = new QPushButton(tr("Edit keyboard shortcuts..."), this);
    connect(keysButton, &QPushButton::clicked, this, &SettingsDialog::editShortcutsRequested);
    addRow(keys, tr("Shortcuts"), keysButton, tr("Rebind any action; duplicate keys are refused"));

    // --- Playlist
    QFormLayout* playlist = addPage(tr("Playlist"));
    restoreSession_ = new QCheckBox(tr("Reload the last playlist when Y-Player starts"), this);
    addRow(playlist, tr("Remember playlist"), restoreSession_);
    autoplayNext_ = new QCheckBox(tr("Play the next entry when a file ends"), this);
    addRow(playlist, tr("Continue through the playlist"), autoplayNext_);

    // --- File associations
    QFormLayout* assoc = addPage(tr("File associations"));
    addRow(assoc, tr("Open with Y-Player"), buildAssociations(),
           tr("Adds Y-Player to Open with for the checked types, for this Windows user only. Windows asks you to "
              "confirm the default player in Settings."));

    // --- Performance
    QFormLayout* performance = addPage(tr("Performance"));
    cacheMb_ = spin(this, 16, 4096, tr(" MB"));
    addRow(performance, tr("Read-ahead cache"), cacheMb_, tr("Memory used to buffer ahead; larger helps slow drives"));
    decoderThreads_ = spin(this, 0, 64);
    decoderThreads_->setSpecialValueText(tr("Automatic"));
    addRow(performance, tr("Software decoder threads"), decoderThreads_, tr("Only used when hardware decoding is off"));

    // --- Advanced diagnostics
    QFormLayout* advanced = addPage(tr("Advanced diagnostics"));
    logLevel_ = combo(this, {{QStringLiteral("warn"), tr("Warnings and errors")},
                             {QStringLiteral("info"), tr("Informational")},
                             {QStringLiteral("v"), tr("Verbose (large logs)")}});
    addRow(advanced, tr("Playback engine logging"), logLevel_);
    auto* diagButton = new QPushButton(tr("Show diagnostics panel"), this);
    connect(diagButton, &QPushButton::clicked, this, &SettingsDialog::showDiagnosticsRequested);
    addRow(advanced, tr("Live statistics"), diagButton);
    auto* logsButton = new QPushButton(tr("Open log folder"), this);
    connect(logsButton, &QPushButton::clicked, this, &SettingsDialog::openLogFolderRequested);
    addRow(advanced, tr("Log files"), logsButton);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply |
                                             QDialogButtonBox::RestoreDefaults,
                                         this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        Q_EMIT applied(collect());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { Q_EMIT applied(collect()); });
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        PlayerSettings defaults;
        defaults.volume = base_.volume;  // not a preference: keep what the user hears
        defaults.muted = base_.muted;
        setSettings(defaults);
    });

    setSettings(base_);
    categories_->setCurrentRow(0);
}

QFormLayout* SettingsDialog::addPage(const QString& title) {
    auto* scroll = new QScrollArea(pages_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget(scroll);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    scroll->setWidget(page);
    pages_->addWidget(scroll);
    categories_->addItem(title);
    pageTitles_ << title;
    return form;
}

void SettingsDialog::addRow(QFormLayout* form, const QString& label, QWidget* field, const QString& help) {
    if (!help.isEmpty()) field->setToolTip(help);
    field->setAccessibleName(label);
    if (help.isEmpty()) {
        form->addRow(label, field);
    } else {
        // Help under the field so costs/caveats are visible without hovering.
        auto* box = new QWidget(field->parentWidget());
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        field->setParent(box);
        v->addWidget(field);
        auto* note = new QLabel(help, box);
        note->setWordWrap(true);
        note->setEnabled(false);  // rendered in the palette's subdued text colour
        v->addWidget(note);
        form->addRow(label, box);
        field = box;
    }
    rows_.append({form, field, (label + QLatin1Char(' ') + help).toLower(), pages_->count() - 1});
}

void SettingsDialog::filter(const QString& text) {
    const QString needle = text.trimmed().toLower();
    QList<bool> pageHasMatch(pages_->count(), false);
    for (const Row& r : rows_) {
        const bool titleMatch = pageTitles_.at(r.page).toLower().contains(needle);
        const bool match = needle.isEmpty() || titleMatch || r.text.contains(needle);
        r.form->setRowVisible(r.field, match);
        if (match) pageHasMatch[r.page] = true;
    }
    int firstVisible = -1;
    for (int i = 0; i < pages_->count(); ++i) {
        categories_->item(i)->setHidden(!pageHasMatch.at(i));
        if (pageHasMatch.at(i) && firstVisible < 0) firstVisible = i;
    }
    const int current = categories_->currentRow();
    if ((current < 0 || !pageHasMatch.value(current)) && firstVisible >= 0) categories_->setCurrentRow(firstVisible);
}

QWidget* SettingsDialog::buildEqualizer() {
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    equalizerPreset_ = new QComboBox(box);
    for (const EqPreset& p : kEqPresets) equalizerPreset_->addItem(QCoreApplication::translate("Settings", p.name));
    connect(equalizerPreset_, &QComboBox::activated, this, [this](int i) {
        for (int b = 0; b < kEqualizerBands; ++b) bands_.at(b)->setValue(kEqPresets[i].gains.at(b));
    });
    v->addWidget(equalizerPreset_);
    auto* sliders = new QHBoxLayout;
    for (int b = 0; b < kEqualizerBands; ++b) {
        auto* column = new QVBoxLayout;
        auto* slider = new QSlider(Qt::Vertical, box);
        slider->setRange(-12, 12);
        slider->setTickPosition(QSlider::TicksBothSides);
        slider->setTickInterval(6);
        slider->setMinimumHeight(120);
        const int hz = kEqualizerFrequencies.at(b);
        const QString label = hz >= 1000 ? tr("%1k").arg(hz / 1000) : QString::number(hz);
        slider->setAccessibleName(tr("%1 Hz").arg(hz));
        auto* value = new QLabel(QStringLiteral("0"), box);
        value->setAlignment(Qt::AlignCenter);
        connect(slider, &QSlider::valueChanged, value, [value](int g) { value->setText(QString::number(g)); });
        column->addWidget(value);
        column->addWidget(slider, 1, Qt::AlignHCenter);
        auto* name = new QLabel(label, box);
        name->setAlignment(Qt::AlignCenter);
        column->addWidget(name);
        sliders->addLayout(column);
        bands_.at(b) = slider;
    }
    v->addLayout(sliders);
    return box;
}

QWidget* SettingsDialog::buildAssociations() {
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    associations_ = new QListWidget(box);
    associations_->setMinimumHeight(180);
    for (const QString& ext : platform::associableExtensions()) {
        auto* item = new QListWidgetItem(QStringLiteral(".%1").arg(ext), associations_);
        item->setData(Qt::UserRole, ext);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    }
    v->addWidget(associations_);
    auto* row = new QHBoxLayout;
    auto* all = new QPushButton(tr("Check all"), box);
    auto* apply = new QPushButton(tr("Apply associations"), box);
    auto* defaults = new QPushButton(tr("Open Windows default apps"), box);
    row->addWidget(all);
    row->addWidget(apply);
    row->addWidget(defaults);
    row->addStretch();
    v->addLayout(row);
    associationStatus_ = new QLabel(box);
    associationStatus_->setWordWrap(true);
    v->addWidget(associationStatus_);

    connect(all, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < associations_->count(); ++i) associations_->item(i)->setCheckState(Qt::Checked);
    });
    connect(defaults, &QPushButton::clicked, this, [] { platform::openDefaultAppsSettings(); });
    connect(apply, &QPushButton::clicked, this, [this] {
        QStringList on;
        QStringList off;
        for (int i = 0; i < associations_->count(); ++i) {
            const QListWidgetItem* item = associations_->item(i);
            (item->checkState() == Qt::Checked ? on : off) << item->data(Qt::UserRole).toString();
        }
        QString error;
        const bool ok = platform::unregisterExtensions(off, &error) &&
                        (on.isEmpty() || platform::registerExtensions(on, QCoreApplication::applicationFilePath(), &error));
        associationStatus_->setText(ok ? tr("Saved. To make Y-Player the default, choose it in Windows default apps.")
                                       : tr("Could not update associations: %1").arg(error));
        refreshAssociations();
    });
    refreshAssociations();
    return box;
}

void SettingsDialog::refreshAssociations() {
    const QStringList registered = platform::registeredExtensions();
    for (int i = 0; i < associations_->count(); ++i) {
        QListWidgetItem* item = associations_->item(i);
        item->setCheckState(registered.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
    }
}

void SettingsDialog::setSettings(const PlayerSettings& s) {
    base_ = s;
    const EngineSettings& e = s.engine;
    resume_->setChecked(s.resume);
    onTop_->setChecked(s.onTop);
    seekShort_->setValue(s.seekShort);
    seekLong_->setValue(s.seekLong);
    exactSeek_->setChecked(s.exactSeek);
    const std::array<int, 5> picture{e.brightness, e.contrast, e.saturation, e.gamma, e.hue};
    for (int i = 0; i < 5; ++i) picture_.at(i)->setValue(picture.at(i));
    select(deinterlace_, e.deinterlace);
    select(hwdec_, e.hwdec);
    select(scaler_, e.scaler);
    select(toneMapping_, e.toneMapping);
    hdrPeak_->setChecked(e.hdrComputePeak);
    hdrPassthrough_->setChecked(e.hdrPassthrough);
    smoothMotion_->setChecked(e.smoothMotion);
    select(channels_, s.audioChannels);
    normalize_->setChecked(e.normalize);
    normalizeDownmix_->setChecked(e.normalizeDownmix);
    audioLanguages_->setText(e.audioLanguages);
    subtitleLanguages_->setText(e.subtitleLanguages);
    select(subtitleAutoLoad_, e.subtitleAutoLoad);
    subtitleStyle_ = s.subtitles;
    sharpen_->setValue(e.sharpen);
    select(denoise_, QString::number(e.denoise));
    equalizerOn_->setChecked(e.equalizerOn);
    for (int b = 0; b < kEqualizerBands; ++b) bands_.at(b)->setValue(e.equalizer.at(b));
    restoreSession_->setChecked(s.restoreSession);
    autoplayNext_->setChecked(s.autoplayNext);
    cacheMb_->setValue(e.cacheMb);
    decoderThreads_->setValue(e.decoderThreads);
    select(logLevel_, e.logLevel);
}

PlayerSettings SettingsDialog::collect() const {
    PlayerSettings s = base_;
    EngineSettings& e = s.engine;
    s.resume = resume_->isChecked();
    s.onTop = onTop_->isChecked();
    s.seekShort = seekShort_->value();
    s.seekLong = seekLong_->value();
    s.exactSeek = exactSeek_->isChecked();
    e.brightness = picture_.at(0)->value();
    e.contrast = picture_.at(1)->value();
    e.saturation = picture_.at(2)->value();
    e.gamma = picture_.at(3)->value();
    e.hue = picture_.at(4)->value();
    e.deinterlace = selected(deinterlace_);
    e.hwdec = selected(hwdec_);
    e.scaler = selected(scaler_);
    e.toneMapping = selected(toneMapping_);
    e.hdrComputePeak = hdrPeak_->isChecked();
    e.hdrPassthrough = hdrPassthrough_->isChecked();
    e.smoothMotion = smoothMotion_->isChecked();
    s.audioChannels = selected(channels_);
    e.normalize = normalize_->isChecked();
    e.normalizeDownmix = normalizeDownmix_->isChecked();
    e.audioLanguages = audioLanguages_->text().trimmed();
    e.subtitleLanguages = subtitleLanguages_->text().trimmed();
    e.subtitleAutoLoad = selected(subtitleAutoLoad_);
    s.subtitles = subtitleStyle_;
    e.sharpen = sharpen_->value();
    e.denoise = selected(denoise_).toInt();
    e.equalizerOn = equalizerOn_->isChecked();
    for (int b = 0; b < kEqualizerBands; ++b) e.equalizer.at(b) = bands_.at(b)->value();
    s.restoreSession = restoreSession_->isChecked();
    s.autoplayNext = autoplayNext_->isChecked();
    e.cacheMb = cacheMb_->value();
    e.decoderThreads = decoderThreads_->value();
    e.logLevel = selected(logLevel_);
    return s;
}

}  // namespace yp::ui
