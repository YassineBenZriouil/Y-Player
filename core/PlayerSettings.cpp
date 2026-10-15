#include "core/PlayerSettings.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QVariantList>

#include <algorithm>

namespace yp {
namespace {

QString choice(QSettings& s, const QString& key, const QString& fallback, const QStringList& allowed) {
    const QString v = s.value(key, fallback).toString();
    return allowed.contains(v) ? v : fallback;
}

int clampedInt(QSettings& s, const QString& key, int fallback, int lo, int hi) {
    bool ok = false;
    const int v = s.value(key, fallback).toInt(&ok);
    return ok ? std::clamp(v, lo, hi) : fallback;
}

double clampedDouble(QSettings& s, const QString& key, double fallback, double lo, double hi) {
    bool ok = false;
    const double v = s.value(key, fallback).toDouble(&ok);
    return ok ? std::clamp(v, lo, hi) : fallback;
}

// "jpn, eng;fre" -> "jpn,eng,fre": letters and separators only, nothing an engine could misparse.
QString languageList(QSettings& s, const QString& key) {
    QStringList codes;
    for (const QString& part : s.value(key).toString().split(QRegularExpression(QStringLiteral("[,; ]+")), Qt::SkipEmptyParts)) {
        if (part.size() <= 8 && std::ranges::all_of(part, [](QChar c) { return c.isLetter() || c == QLatin1Char('-'); })) {
            codes << part.toLower();
        }
    }
    return codes.join(QLatin1Char(','));
}

}  // namespace

void saveEngineSettings(QSettings& s, const EngineSettings& e) {
    s.setValue(QStringLiteral("hwdec"), e.hwdec);
    s.setValue(QStringLiteral("scaler"), e.scaler);
    s.setValue(QStringLiteral("toneMapping"), e.toneMapping);
    s.setValue(QStringLiteral("hdrComputePeak"), e.hdrComputePeak);
    s.setValue(QStringLiteral("hdrPassthrough"), e.hdrPassthrough);
    s.setValue(QStringLiteral("smoothMotion"), e.smoothMotion);
    s.setValue(QStringLiteral("brightness"), e.brightness);
    s.setValue(QStringLiteral("contrast"), e.contrast);
    s.setValue(QStringLiteral("saturation"), e.saturation);
    s.setValue(QStringLiteral("gamma"), e.gamma);
    s.setValue(QStringLiteral("hue"), e.hue);
    s.setValue(QStringLiteral("deinterlace"), e.deinterlace);
    s.setValue(QStringLiteral("sharpen"), e.sharpen);
    s.setValue(QStringLiteral("denoise"), e.denoise);
    s.setValue(QStringLiteral("equalizerOn"), e.equalizerOn);
    QVariantList bands;
    for (const int g : e.equalizer) bands << g;
    s.setValue(QStringLiteral("equalizer"), bands);
    s.setValue(QStringLiteral("normalize"), e.normalize);
    s.setValue(QStringLiteral("normalizeDownmix"), e.normalizeDownmix);
    s.setValue(QStringLiteral("audioLanguages"), e.audioLanguages);
    s.setValue(QStringLiteral("subtitleLanguages"), e.subtitleLanguages);
    s.setValue(QStringLiteral("subtitleAutoLoad"), e.subtitleAutoLoad);
    s.setValue(QStringLiteral("cacheMb"), e.cacheMb);
    s.setValue(QStringLiteral("decoderThreads"), e.decoderThreads);
    s.setValue(QStringLiteral("logLevel"), e.logLevel);
}

EngineSettings loadEngineSettings(QSettings& s) {
    const EngineSettings d;
    EngineSettings e;
    e.hwdec = choice(s, QStringLiteral("hwdec"), d.hwdec,
                     {QStringLiteral("auto"), QStringLiteral("d3d11va"), QStringLiteral("d3d11va-copy"),
                      QStringLiteral("nvdec"), QStringLiteral("no")});
    e.scaler = choice(s, QStringLiteral("scaler"), d.scaler,
                      {QStringLiteral("bilinear"), QStringLiteral("spline36"), QStringLiteral("lanczos"),
                       QStringLiteral("ewa_lanczossharp")});
    e.toneMapping = choice(s, QStringLiteral("toneMapping"), d.toneMapping,
                           {QStringLiteral("auto"), QStringLiteral("bt.2390"), QStringLiteral("spline"),
                            QStringLiteral("hable"), QStringLiteral("mobius"), QStringLiteral("reinhard"),
                            QStringLiteral("clip")});
    e.hdrComputePeak = s.value(QStringLiteral("hdrComputePeak"), d.hdrComputePeak).toBool();
    e.hdrPassthrough = s.value(QStringLiteral("hdrPassthrough"), d.hdrPassthrough).toBool();
    e.smoothMotion = s.value(QStringLiteral("smoothMotion"), d.smoothMotion).toBool();
    e.brightness = clampedInt(s, QStringLiteral("brightness"), 0, -100, 100);
    e.contrast = clampedInt(s, QStringLiteral("contrast"), 0, -100, 100);
    e.saturation = clampedInt(s, QStringLiteral("saturation"), 0, -100, 100);
    e.gamma = clampedInt(s, QStringLiteral("gamma"), 0, -100, 100);
    e.hue = clampedInt(s, QStringLiteral("hue"), 0, -100, 100);
    e.deinterlace = choice(s, QStringLiteral("deinterlace"), d.deinterlace,
                           {QStringLiteral("auto"), QStringLiteral("yes"), QStringLiteral("no")});
    e.sharpen = clampedInt(s, QStringLiteral("sharpen"), 0, 0, 100);
    e.denoise = clampedInt(s, QStringLiteral("denoise"), 0, 0, 3);
    e.equalizerOn = s.value(QStringLiteral("equalizerOn"), d.equalizerOn).toBool();
    const QVariantList bands = s.value(QStringLiteral("equalizer")).toList();
    for (int i = 0; i < kEqualizerBands && i < bands.size(); ++i) e.equalizer.at(i) = std::clamp(bands.at(i).toInt(), -12, 12);
    e.normalize = s.value(QStringLiteral("normalize"), d.normalize).toBool();
    e.normalizeDownmix = s.value(QStringLiteral("normalizeDownmix"), d.normalizeDownmix).toBool();
    e.audioLanguages = languageList(s, QStringLiteral("audioLanguages"));
    e.subtitleLanguages = languageList(s, QStringLiteral("subtitleLanguages"));
    e.subtitleAutoLoad = choice(s, QStringLiteral("subtitleAutoLoad"), d.subtitleAutoLoad,
                                {QStringLiteral("no"), QStringLiteral("exact"), QStringLiteral("fuzzy")});
    e.cacheMb = clampedInt(s, QStringLiteral("cacheMb"), d.cacheMb, 16, 4096);
    e.decoderThreads = clampedInt(s, QStringLiteral("decoderThreads"), 0, 0, 64);
    e.logLevel = choice(s, QStringLiteral("logLevel"), d.logLevel,
                        {QStringLiteral("warn"), QStringLiteral("info"), QStringLiteral("v")});
    return e;
}

void savePlayerSettings(QSettings& s, const PlayerSettings& p) {
    s.beginGroup(QStringLiteral("engine"));
    saveEngineSettings(s, p.engine);
    s.endGroup();
    s.beginGroup(QStringLiteral("subtitles"));
    s.setValue(QStringLiteral("font"), p.subtitles.font);
    s.setValue(QStringLiteral("size"), p.subtitles.size);
    s.setValue(QStringLiteral("color"), p.subtitles.color);
    s.setValue(QStringLiteral("outlineColor"), p.subtitles.outlineColor);
    s.setValue(QStringLiteral("outlineSize"), p.subtitles.outlineSize);
    s.setValue(QStringLiteral("position"), p.subtitles.position);
    s.setValue(QStringLiteral("overrideStyled"), p.subtitles.overrideStyled);
    s.endGroup();
    s.setValue(QStringLiteral("audio/channels"), p.audioChannels);
    s.setValue(QStringLiteral("audio/volume"), p.volume);
    s.setValue(QStringLiteral("audio/muted"), p.muted);
    s.setValue(QStringLiteral("playback/resume"), p.resume);
    s.setValue(QStringLiteral("playback/restoreSession"), p.restoreSession);
    s.setValue(QStringLiteral("playback/autoplayNext"), p.autoplayNext);
    s.setValue(QStringLiteral("playback/seekShort"), p.seekShort);
    s.setValue(QStringLiteral("playback/seekLong"), p.seekLong);
    s.setValue(QStringLiteral("playback/exactSeek"), p.exactSeek);
    s.setValue(QStringLiteral("window/onTop"), p.onTop);
    s.setValue(QStringLiteral("general/theme"), p.theme);
    s.setValue(QStringLiteral("general/language"), p.language);
}

PlayerSettings loadPlayerSettings(QSettings& s) {
    const PlayerSettings d;
    PlayerSettings p;
    s.beginGroup(QStringLiteral("engine"));
    p.engine = loadEngineSettings(s);
    s.endGroup();
    s.beginGroup(QStringLiteral("subtitles"));
    p.subtitles.font = s.value(QStringLiteral("font"), d.subtitles.font).toString();
    p.subtitles.size = clampedInt(s, QStringLiteral("size"), d.subtitles.size, 8, 200);
    p.subtitles.color = s.value(QStringLiteral("color"), d.subtitles.color).toString();
    p.subtitles.outlineColor = s.value(QStringLiteral("outlineColor"), d.subtitles.outlineColor).toString();
    p.subtitles.outlineSize = clampedDouble(s, QStringLiteral("outlineSize"), d.subtitles.outlineSize, 0.0, 20.0);
    p.subtitles.position = clampedInt(s, QStringLiteral("position"), d.subtitles.position, 0, 150);
    p.subtitles.overrideStyled = s.value(QStringLiteral("overrideStyled"), d.subtitles.overrideStyled).toBool();
    s.endGroup();
    const QRegularExpression hexColor(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    if (!hexColor.match(p.subtitles.color).hasMatch()) p.subtitles.color = d.subtitles.color;
    if (!hexColor.match(p.subtitles.outlineColor).hasMatch()) p.subtitles.outlineColor = d.subtitles.outlineColor;
    p.audioChannels = choice(s, QStringLiteral("audio/channels"), d.audioChannels,
                             {QStringLiteral("auto"), QStringLiteral("stereo"), QStringLiteral("5.1"),
                              QStringLiteral("7.1"), QStringLiteral("mono")});
    p.volume = clampedInt(s, QStringLiteral("audio/volume"), d.volume, 0, 100);
    p.muted = s.value(QStringLiteral("audio/muted"), d.muted).toBool();
    p.resume = s.value(QStringLiteral("playback/resume"), d.resume).toBool();
    p.restoreSession = s.value(QStringLiteral("playback/restoreSession"), d.restoreSession).toBool();
    p.autoplayNext = s.value(QStringLiteral("playback/autoplayNext"), d.autoplayNext).toBool();
    p.seekShort = clampedDouble(s, QStringLiteral("playback/seekShort"), d.seekShort, 1.0, 600.0);
    p.seekLong = clampedDouble(s, QStringLiteral("playback/seekLong"), d.seekLong, 1.0, 3600.0);
    p.exactSeek = s.value(QStringLiteral("playback/exactSeek"), d.exactSeek).toBool();
    p.onTop = s.value(QStringLiteral("window/onTop"), d.onTop).toBool();
    p.theme = choice(s, QStringLiteral("general/theme"), d.theme,
                     {QStringLiteral("system"), QStringLiteral("dark"), QStringLiteral("light")});
    p.language = choice(s, QStringLiteral("general/language"), d.language,
                        {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("fr")});
    return p;
}

QList<NamedProfile> builtInProfiles() {
    EngineSettings quality;
    quality.scaler = QStringLiteral("ewa_lanczossharp");
    quality.smoothMotion = true;
    quality.sharpen = 15;
    EngineSettings saver;
    saver.scaler = QStringLiteral("bilinear");
    saver.hdrComputePeak = false;
    return {{QCoreApplication::translate("Profiles", "Balanced (default)"), EngineSettings{}},
            {QCoreApplication::translate("Profiles", "High quality"), quality},
            {QCoreApplication::translate("Profiles", "Power saver"), saver}};
}

}  // namespace yp
