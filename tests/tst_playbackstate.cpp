#include "core/PlaybackStateMachine.h"

#include <QTest>

using yp::PlaybackEvent;
using yp::PlaybackState;

Q_DECLARE_METATYPE(yp::PlaybackEvent)

class TestPlaybackState : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void transitions_data() {
        QTest::addColumn<PlaybackState>("from");
        QTest::addColumn<PlaybackEvent>("event");
        QTest::addColumn<bool>("paused");
        QTest::addColumn<PlaybackState>("to");
        using S = PlaybackState;
        using E = PlaybackEvent;
        QTest::newRow("start from idle") << S::Idle << E::FileStarted << false << S::Loading;
        QTest::newRow("start replaces playing") << S::Playing << E::FileStarted << false << S::Loading;
        QTest::newRow("start after error") << S::Error << E::FileStarted << false << S::Loading;
        QTest::newRow("loaded -> playing") << S::Loading << E::FileLoaded << false << S::Playing;
        QTest::newRow("loaded paused -> paused") << S::Loading << E::FileLoaded << true << S::Paused;
        QTest::newRow("late loaded ignored when idle") << S::Idle << E::FileLoaded << false << S::Idle;
        QTest::newRow("pause") << S::Playing << E::PauseChanged << true << S::Paused;
        QTest::newRow("resume") << S::Paused << E::PauseChanged << false << S::Playing;
        QTest::newRow("pause while loading stays loading") << S::Loading << E::PauseChanged << true << S::Loading;
        QTest::newRow("pause at end stays ended") << S::Ended << E::PauseChanged << true << S::Ended;
        QTest::newRow("pause when idle stays idle") << S::Idle << E::PauseChanged << true << S::Idle;
        QTest::newRow("eof while playing") << S::Playing << E::EofReached << true << S::Ended;
        QTest::newRow("eof while paused") << S::Paused << E::EofReached << true << S::Ended;
        QTest::newRow("eof while loading (empty file)") << S::Loading << E::EofReached << false << S::Ended;
        QTest::newRow("eof ignored when idle") << S::Idle << E::EofReached << false << S::Idle;
        QTest::newRow("seek back from end, paused") << S::Ended << E::EofCleared << true << S::Paused;
        QTest::newRow("seek back from end, playing") << S::Ended << E::EofCleared << false << S::Playing;
        QTest::newRow("eof cleared ignored when playing") << S::Playing << E::EofCleared << false << S::Playing;
        QTest::newRow("stop") << S::Playing << E::Stopped << false << S::Idle;
        QTest::newRow("stop after error") << S::Error << E::Stopped << false << S::Idle;
        QTest::newRow("fail while loading") << S::Loading << E::Failed << false << S::Error;
        QTest::newRow("fail while playing") << S::Playing << E::Failed << false << S::Error;
    }

    void transitions() {
        QFETCH(PlaybackState, from);
        QFETCH(PlaybackEvent, event);
        QFETCH(bool, paused);
        QFETCH(PlaybackState, to);
        QCOMPARE(yp::nextState(from, event, paused), to);
    }

    void typicalSessionSequence() {
        // open -> load -> pause -> resume -> end -> seek back -> stop
        PlaybackState s = PlaybackState::Idle;
        s = yp::nextState(s, PlaybackEvent::FileStarted, false);
        s = yp::nextState(s, PlaybackEvent::FileLoaded, false);
        QCOMPARE(s, PlaybackState::Playing);
        s = yp::nextState(s, PlaybackEvent::PauseChanged, true);
        s = yp::nextState(s, PlaybackEvent::PauseChanged, false);
        s = yp::nextState(s, PlaybackEvent::PauseChanged, true);  // keep-open pauses at EOF...
        s = yp::nextState(s, PlaybackEvent::EofReached, true);    // ...then reports EOF
        QCOMPARE(s, PlaybackState::Ended);
        s = yp::nextState(s, PlaybackEvent::EofCleared, true);
        QCOMPARE(s, PlaybackState::Paused);
        s = yp::nextState(s, PlaybackEvent::Stopped, true);
        QCOMPARE(s, PlaybackState::Idle);
    }
};

QTEST_GUILESS_MAIN(TestPlaybackState)
#include "tst_playbackstate.moc"
