#include "platform/win/SystemInfo.h"

#include <QTest>

class TestSystemInfo : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reportsOsAndCpu() {
        const yp::platform::SystemInfo info = yp::platform::querySystemInfo();
        QVERIFY(info.os.contains(QStringLiteral("Windows")));
        QVERIFY(!info.cpu.isEmpty());
        QVERIFY(info.logicalCores > 0);
        // GPUs are not asserted: CI VMs may only expose a software adapter, which is skipped.
        for (const auto& gpu : info.gpus) QVERIFY(!gpu.name.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSystemInfo)
#include "tst_systeminfo.moc"
