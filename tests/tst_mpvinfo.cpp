#include "engine/mpv/MpvInfo.h"

#include <QRegularExpression>
#include <QTest>

// Guards the libmpv DLL/header pairing: a mismatched deps/mpv update fails here, not at runtime.
class TestMpvInfo : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void loadedDllMatchesHeaders() {
        QVERIFY(QRegularExpression(QStringLiteral(R"(^\d+\.\d+$)")).match(yp::mpv::loadedApiVersion()).hasMatch());
        QVERIFY2(yp::mpv::loadedApiMatchesHeaders(), qPrintable(yp::mpv::loadedApiVersion()));
    }
};

QTEST_GUILESS_MAIN(TestMpvInfo)
#include "tst_mpvinfo.moc"
