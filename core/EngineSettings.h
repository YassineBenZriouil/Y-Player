#pragma once

#include <QString>

#include <array>

namespace yp {

inline constexpr int kEqualizerBands = 10;
inline constexpr std::array<int, kEqualizerBands> kEqualizerFrequencies{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

// Everything the playback backend applies as a whole (Phase 6: Settings > Hardware acceleration,
// Rendering, Video, Filters, Audio, Subtitles, Performance, Advanced). Values are backend-neutral
// names; the engine maps them. Defaults cost nothing extra: every filter starts off.
struct EngineSettings {
    // Hardware acceleration
    QString hwdec = QStringLiteral("auto");  // auto | d3d11va | d3d11va-copy | nvdec | no

    // Rendering
    QString scaler = QStringLiteral("spline36");     // bilinear | spline36 | lanczos | ewa_lanczossharp
    QString toneMapping = QStringLiteral("auto");    // auto | bt.2390 | spline | hable | mobius | reinhard | clip
    bool hdrComputePeak = true;   // per-scene HDR peak detection (small GPU cost)
    bool hdrPassthrough = false;  // send HDR untouched to an HDR-enabled display
    bool smoothMotion = false;    // display-resample + interpolation (extra GPU work every refresh)

    // Picture (-100..100, applied in the GPU renderer: no measurable cost)
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    int gamma = 0;
    int hue = 0;
    QString deinterlace = QStringLiteral("auto");  // auto | yes | no

    // Filters
    int sharpen = 0;  // 0..100, GPU shader
    int denoise = 0;  // 0 off, 1 low, 2 medium, 3 high; CPU filter (copies hardware frames to the CPU)
    bool equalizerOn = false;
    std::array<int, kEqualizerBands> equalizer{};  // dB, -12..12 per band
    bool normalize = false;        // dynamic loudness normalization
    bool normalizeDownmix = true;  // avoid clipping when downmixing surround to stereo

    // Languages and subtitles
    QString audioLanguages;     // preferred, comma separated ISO codes, e.g. "jpn,eng"
    QString subtitleLanguages;
    QString subtitleAutoLoad = QStringLiteral("fuzzy");  // no | exact | fuzzy (external files next to the video)

    // Performance and diagnostics
    int cacheMb = 150;       // demuxer read-ahead cap
    int decoderThreads = 0;  // 0 = automatic
    QString logLevel = QStringLiteral("warn");  // warn | info | v (engine messages copied to the log)

    bool operator==(const EngineSettings&) const = default;
};

// Backend filter-graph strings (FFmpeg lavfi syntax), empty when nothing is enabled.
QString videoFilterChain(bool flipHorizontal, bool flipVertical, int denoise);
QString audioFilterChain(const EngineSettings& s);

}  // namespace yp
