# Testing

## Running

```bat
scripts\build.cmd            :: Debug build + all tests
scripts\build.cmd ci         :: Release + clang-tidy + all tests (same as GitHub Actions)
ctest --preset debug -L integration   :: only the playback integration test
ctest --preset debug -LE integration  :: only the fast unit tests
```

Run a single test executable directly with `build\<preset>\bin\tst_<name>.exe`. It needs `deps\qt\6.9.3\msvc2022_64\bin` on `PATH` and `QT_PLUGIN_PATH` pointing at that install's `plugins`; ctest sets both.

## Suites

| Test | Kind | Covers |
|------|------|--------|
| `tst_logging` | unit | record format, one line per record, size rotation, bounded file count, install/shutdown |
| `tst_playbackstate` | unit | every `PlaybackState` transition rule and a typical session |
| `tst_timeformat` | unit | time labels, hour layout, NaN/inf/negative input, ms clamping, seek-target clamping |
| `tst_playlist` | unit | next/previous across repeat modes, shuffle visits every entry once and retraces, index fix-ups on insert/remove/move, 100 000-entry list speed |
| `tst_playlistio` | unit | M3U/M3U8/PLS parsing (comments, BOM, relative paths, `file://`, skipped network URLs, legacy code pages, malformed input), save/load round trips |
| `tst_library` | unit | recent files (dedupe, cap, order), resume policy/LRU/JSON, speed and delay steps, A-B cycling, aspect parsing |
| `tst_shortcutmap` | unit | default keys are unique and match `INIT.md` §12, duplicate rejection, override save/restore (including swaps and bad entries), reset |
| `tst_mpvinfo` | unit | the loaded libmpv DLL matches the headers |
| `tst_systeminfo` | unit | OS/CPU/GPU queries |
| `tst_mpvengine` | integration | real libmpv playback in a 320×240 window, muted. Covers the 4 codecs/containers, a Unicode path, unplayable files, pause/seek/end/stop/replay, volume/mute, **100 open/stop cycles** and **20 engine lifecycles** with handle/thread growth checks, teardown time < 1 s, the COM-apartment regression (ADR-011). **Phase 5:** track listing/switching, external (UTF-8/RTL), malformed and missing subtitles, chapters, start position, speed/delay clamping, exact frame stepping, A-B loop, PNG screenshots, aspect/rotate/flip while playing, audio devices and channel layouts |

The integration test media lives in `tests/media/` (committed, ADR-013), and `tests/media/generate.sh` regenerates it. It includes `multitrack.mkv` (2 audio tracks, 2 subtitle tracks, 3 chapters), `external_rtl.srt` and `malformed.srt`.

## Environments

- **Developer machine (Windows 11, RTX 3050):** every suite passes, and hardware decoding (`d3d11va`) is active for all 4 test codecs.
- **GitHub Actions `windows-2025`:** no GPU and no audio device. Phase 2/3 CI runs are still to come. Whether mpv gets a D3D11 (WARP) output there is unconfirmed. If it can't create one, `tst_mpvengine` **skips** with "no D3D11 video output in this environment (not an engine failure)", so an unsupported environment is never reported as an engine bug. Software decoding and the null audio fallback are expected there.

## Manual checks that are not automated yet

- **Drag-and-drop** from Explorer: implemented (`MainWindow::dropEvent`, with mpv's own drop handling disabled) but **not verified yet**. Simulating OLE drag-and-drop reliably needs UI automation (Phase 8). Until then, check it by hand.
- **Fullscreen and double-click input:** verified with simulated input (`SendInput`) during Phase 2, with no test in CI yet.
- **Phase 5 UI, not exercised interactively yet:** the playlist dock (drag-reorder, drop files, Delete), the shortcut and subtitle-style dialogs, borderless and always-on-top, the right-click context menu, recent files, and restoring the session playlist. The logic behind them is unit-tested and the engine side is integration-tested, but the widgets themselves were only checked by building and launching the app (it starts, every action has a handler, and it exits cleanly).

## Benchmarks

Performance testing is separate from the test suites. It is done with `bench/make_corpus.py` and `bench/run_bench.py` (ADR-012). `PERFORMANCE.md` will hold the results once the skipped Phase 3 baseline is run (see `ROADMAP.md`).
