#include "core/EngineSettings.h"

#include <QStringList>

#include <algorithm>

namespace yp {

QString videoFilterChain(bool flipHorizontal, bool flipVertical, int denoise) {
    static const char* const kDenoise[] = {"", "hqdn3d=2:1.5:3:2.25", "hqdn3d=4:3:6:4.5", "hqdn3d=8:6:12:9"};
    QStringList filters;
    if (flipHorizontal) filters << QStringLiteral("hflip");
    if (flipVertical) filters << QStringLiteral("vflip");
    if (const int level = std::clamp(denoise, 0, 3); level > 0) {
        filters << QStringLiteral("lavfi=[%1]").arg(QString::fromLatin1(kDenoise[level]));
    }
    return filters.join(QLatin1Char(','));
}

QString audioFilterChain(const EngineSettings& s) {
    QStringList graph;
    if (s.equalizerOn) {
        for (int i = 0; i < kEqualizerBands; ++i) {
            const int gain = std::clamp(s.equalizer.at(i), -12, 12);
            if (gain != 0) {
                graph << QStringLiteral("equalizer=f=%1:t=o:w=1:g=%2").arg(kEqualizerFrequencies.at(i)).arg(gain);
            }
        }
    }
    if (s.normalize) graph << QStringLiteral("dynaudnorm");
    return graph.isEmpty() ? QString() : QStringLiteral("lavfi=[%1]").arg(graph.join(QLatin1Char(',')));
}

}  // namespace yp
