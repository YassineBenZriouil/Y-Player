#pragma once

#include <QList>
#include <QString>

namespace yp::platform {

struct GpuInfo {
    QString name;
    QString driverVersion;
    quint64 dedicatedVideoMemoryMb = 0;
};

struct SystemInfo {
    QString os;
    QString cpu;
    int logicalCores = 0;
    QList<GpuInfo> gpus;  // hardware adapters only (software rasterizers are skipped)
};

SystemInfo querySystemInfo();

// Milliseconds since this process was created (startup/first-frame timing).
qint64 processAgeMs();

}  // namespace yp::platform
