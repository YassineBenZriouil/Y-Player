#include "platform/win/SystemInfo.h"

#include <QSettings>
#include <QSysInfo>
#include <QThread>

#include <windows.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace yp::platform {

SystemInfo querySystemInfo() {
    SystemInfo info;
    info.os = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    const QSettings cpuKey(QStringLiteral(R"(HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0)"),
                           QSettings::NativeFormat);
    info.cpu = cpuKey.value(QStringLiteral("ProcessorNameString")).toString().trimmed();
    info.logicalCores = QThread::idealThreadCount();

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return info;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE)) != 0) continue;
        GpuInfo gpu;
        gpu.name = QString::fromWCharArray(desc.Description);
        gpu.dedicatedVideoMemoryMb = desc.DedicatedVideoMemory / (SIZE_T{1024} * 1024);
        LARGE_INTEGER umd{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
            gpu.driverVersion = QStringLiteral("%1.%2.%3.%4")
                                    .arg(HIWORD(umd.HighPart))
                                    .arg(LOWORD(umd.HighPart))
                                    .arg(HIWORD(umd.LowPart))
                                    .arg(LOWORD(umd.LowPart));
        }
        info.gpus.push_back(gpu);
    }
    return info;
}

qint64 processAgeMs() {
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    FILETIME now{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimePreciseAsFileTime(&now);
    const auto ticks = [](FILETIME t) { return (static_cast<quint64>(t.dwHighDateTime) << 32U) | t.dwLowDateTime; };
    return static_cast<qint64>((ticks(now) - ticks(created)) / 10000U);  // 100 ns ticks -> ms
}

}  // namespace yp::platform
