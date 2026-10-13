#include "ui/ShortcutMap.h"

#include <QSet>
#include <QTest>

using yp::ui::ShortcutMap;

namespace {
QKeySequence k(const char* s) { return {QString::fromLatin1(s), QKeySequence::PortableText}; }
}  // namespace

class TestShortcutMap : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsHaveNoDuplicatesAndCoverInitKeys() {
        const ShortcutMap map(yp::ui::playerActions());
        QSet<QString> ids;
        QHash<QKeySequence, QString> owners;
        for (const auto& d : map.definitions()) {
            QVERIFY2(!ids.contains(d.id), qPrintable(d.id));
            ids.insert(d.id);
            for (const QKeySequence& key : d.defaults) {
                QVERIFY2(!owners.contains(key), qPrintable(key.toString() + " used by " + owners.value(key) + " and " + d.id));
                owners.insert(key, d.id);
            }
        }
        // INIT.md §12 defaults
        QCOMPARE(map.owner(k("Space")), QStringLiteral("playback.toggle"));
        QCOMPARE(map.owner(k("Right")), QStringLiteral("playback.seekForward"));
        QCOMPARE(map.owner(k("Up")), QStringLiteral("audio.volumeUp"));
        QCOMPARE(map.owner(k("F")), QStringLiteral("video.fullscreen"));
        QCOMPARE(map.owner(k("Return")), QStringLiteral("video.fullscreen"));
        QCOMPARE(map.owner(k("Esc")), QStringLiteral("video.exitFullscreen"));
        QCOMPARE(map.owner(k("M")), QStringLiteral("audio.mute"));
        QCOMPARE(map.owner(k("S")), QStringLiteral("subtitle.cycleTrack"));
        QCOMPARE(map.owner(k("A")), QStringLiteral("audio.cycleTrack"));
        QCOMPARE(map.owner(k("O")), QStringLiteral("file.open"));
    }

    void setRejectsDuplicates() {
        ShortcutMap map(yp::ui::playerActions());
        QString conflict;
        QVERIFY(!map.set(QStringLiteral("audio.mute"), {k("Space")}, &conflict));
        QCOMPARE(conflict, QStringLiteral("playback.toggle"));
        QCOMPARE(map.shortcuts(QStringLiteral("audio.mute")), QList<QKeySequence>{k("M")});  // unchanged
        QVERIFY(map.set(QStringLiteral("audio.mute"), {k("Ctrl+M"), k("Ctrl+M")}));
        QCOMPARE(map.shortcuts(QStringLiteral("audio.mute")), QList<QKeySequence>{k("Ctrl+M")});
        QVERIFY(map.set(QStringLiteral("audio.mute"), {}));  // unbinding is allowed
        QVERIFY(map.owner(k("M")).isEmpty());
        QVERIFY(!map.set(QStringLiteral("no.such.action"), {k("F12")}));
    }

    void overridesRoundTrip() {
        ShortcutMap map(yp::ui::playerActions());
        QVERIFY(map.set(QStringLiteral("audio.mute"), {k("Ctrl+M")}));
        QVERIFY(map.set(QStringLiteral("playback.toggle"), {k("K")}));
        const QVariantMap saved = map.overrides();
        QCOMPARE(saved.size(), 2);  // only what differs from the defaults

        ShortcutMap restored(yp::ui::playerActions());
        QCOMPARE(restored.applyOverrides(saved), 0);
        QCOMPARE(restored.shortcuts(QStringLiteral("audio.mute")), QList<QKeySequence>{k("Ctrl+M")});
        QCOMPARE(restored.shortcuts(QStringLiteral("playback.toggle")), QList<QKeySequence>{k("K")});
        QVERIFY(restored.owner(k("Space")).isEmpty());
    }

    void overridesSwapKeysAndSkipBadEntries() {
        ShortcutMap map(yp::ui::playerActions());
        const QVariantMap swapped{{QStringLiteral("audio.mute"), QStringList{QStringLiteral("Space")}},
                                  {QStringLiteral("playback.toggle"), QStringList{QStringLiteral("M")}},
                                  {QStringLiteral("bogus.id"), QStringList{QStringLiteral("F1")}},
                                  {QStringLiteral("video.screenshot"), QStringList{QStringLiteral("Right")}}};
        QCOMPARE(map.applyOverrides(swapped), 2);  // bogus id + conflict with seekForward
        QCOMPARE(map.owner(k("Space")), QStringLiteral("audio.mute"));
        QCOMPARE(map.owner(k("M")), QStringLiteral("playback.toggle"));
        QCOMPARE(map.shortcuts(QStringLiteral("video.screenshot")), QList<QKeySequence>{k("Ctrl+E")});  // default kept
    }

    void resetRestoresDefaultAndFreesKey() {
        ShortcutMap map(yp::ui::playerActions());
        QVERIFY(map.set(QStringLiteral("audio.mute"), {}));
        QVERIFY(map.set(QStringLiteral("video.onTop"), {k("M")}));
        map.reset(QStringLiteral("audio.mute"));
        QCOMPARE(map.owner(k("M")), QStringLiteral("audio.mute"));
        QVERIFY(!map.shortcuts(QStringLiteral("video.onTop")).contains(k("M")));
        map.resetAll();
        QVERIFY(map.overrides().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestShortcutMap)
#include "tst_shortcutmap.moc"
