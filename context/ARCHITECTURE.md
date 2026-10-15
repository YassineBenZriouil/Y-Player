# Architecture

Status: Phase 7. Each section says what exists today and what the module is for. Update this file whenever a boundary moves.

## Modules and dependencies

```
app/  (Y-Player.exe: composition root, startup, logging, --bench-report mode)
 ├──> ui/            yp_ui            Qt Widgets: MainWindow(+Menus), VideoWidget, SeekSlider, PlaylistModel/Panel,
 │                                    ShortcutMap/Dialog, SubtitleStyleDialog
 ├──> engine/mpv/    yp_engine_mpv    MpvEngine : IPlaybackEngine (Mpv::Mpv imported target)
 ├──> platform/win/  yp_platform_win  Windows-only: DXGI/OS/CPU queries, process age, window style (on-top, borderless)
 └──> core/          yp_core          Qt Core only: IPlaybackEngine, PlaybackStateMachine, Playlist, PlaylistIO,
                                      RecentFiles, ResumeStore, Adjustments, TimeFormat, logging
          ^
          └── ui/ and engine/mpv/ depend on core/; ui/ also uses platform/win/ for window styles;
              nothing depends on app/ or ui/
```

**Rules**
- `ui/` never includes `engine/` or libmpv headers. It talks to playback only through `yp::IPlaybackEngine` (`core/PlaybackEngine.h`). `app/main.cpp` creates the concrete `MpvEngine` and hands it to `MainWindow`, which owns it.
- `core/` depends on Qt Core only, with no Widgets or Gui, so it stays unit-testable without a display. Rules that don't need a backend live here and have unit tests: `PlaybackStateMachine` (state transitions) and `TimeFormat` (labels, slider ms, seek clamping).
- Windows API calls live in `platform/win/`. The one exception is `engine/mpv/` passing an HWND to mpv.
- Rules that don't need a backend or a display live in `core/` and are unit-tested:
  - playlist order, repeat, shuffle and index fix-ups (`Playlist`)
  - M3U/PLS parsing and writing (`PlaylistIO`)
  - the MRU list (`RecentFiles`)
  - the resume policy (`ResumeStore`)
  - speed and delay steps, A-B cycling and aspect parsing (`Adjustments`)
- `ui/ShortcutMap` holds key bindings, with one owner per key. It is tested with Qt Gui types only.
- Each module is a static library linked into the single exe. There are no plugins, services, or extra processes.

## Playback engine (`engine/mpv/MpvEngine`)

- **mpv options** (ADR-003/006):
  - presentation: `vo=gpu-next`, `gpu-api=d3d11`, `hwdec=auto-safe`
  - lifetime: `force-window=yes` (keeps the swapchain across files and paints black when idle), `keep-open=yes` (Ended holds the last frame)
  - safety: `config/load-scripts/ytdl/osc/input-*/drag-and-drop = no`
- **Commands:** every command and property write is asynchronous (`mpv_command_async` / `mpv_set_property_async`), so the UI thread never waits on mpv.
- **State:** mpv notifications are translated into `PlaybackEvent`s and fed through `nextState()`. The engine only emits `stateChanged` when the result differs.
- **Position updates:** `positionChanged` is throttled to 0.1 s steps during playback. It always fires on pause and on `playbackRestarted` (first frame after a load or seek), so labels land exactly.
- **Stats:** `stats()` reads mpv's frame counters and A/V offset on demand. The bench mode uses it now, and the Phase 6 diagnostics panel will too.
- **Ownership:** `MainWindow` owns the engine and destroys it in `~MainWindow` *before* its child widgets. mpv presents into the video widget's HWND, so that window must outlive the engine. `mpv_terminate_destroy` runs on a joined worker thread (ADR-011).
- **Features (Phase 5):**
  - tracks (`track-list`), chapters, audio devices (with hot-plug fallback to the system default and a `notice()`)
  - speed, A-B loop, frame step, audio and subtitle delay, subtitle style (`sub-*`, `sub-ass-override`)
  - aspect override, rotation, flip (`vf`, a CPU filter), screenshots (`screenshot-to-file`, PNG), OSD messages (`show-text`)
  - Requests whose outcome the UI must hear about (subtitle files, screenshots) carry a reply id and report through `screenshotSaved()` or `errorOccurred()`.

## Threading

- All `IPlaybackEngine` methods and signals live on the UI thread.
- libmpv runs its own demux, decode, render and audio threads. Its wakeup callback (any thread) only queues one `drainEvents()` onto the engine's thread. An atomic flag coalesces bursts so a busy mpv can't flood the event queue. `~MpvEngine` clears the callback before `mpv_terminate_destroy`. Queued drains die with the QObject.
- The logger is thread-safe: one mutex, and each record is written and flushed while holding it.

## Video presentation and input (ADR-003)

`VideoWidget` is a native child window (`WA_NativeWindow`, `WA_PaintOnScreen`, null paint engine), and mpv creates its own child window inside it. Qt never paints there. Overlays must use mpv OSD or separate windows, never translucent child widgets.

Input behaviour, verified with simulated input in Phase 2:
- After clicking the video, keyboard shortcuts still reach Qt.
- Double-click on the video toggles fullscreen.

All window shortcuts are `QAction`s added to the window itself, so they keep working in fullscreen where the menu bar is hidden. Controls are `Qt::NoFocus`, so Space and the arrow keys never get stolen by a focused button or slider. Phase 7 has to revisit this for keyboard accessibility.

## Logging

`yp::log::install()` (`core/Logging.*`) captures every `qDebug`/`qCInfo`/... call:

- **File:** `%LOCALAPPDATA%\Y-Player\logs\y-player.log`, rotated at 5 MB, keeping 3 old files.
- **Format:** `<UTC ISO-8601 ms> <level> [<category>] tid=<thread> <message>`, with exactly one line per record.
- **Categories:** `yp.app`, `yp.mpv`. mpv's own warnings and errors are forwarded into `yp.mpv`. Release builds turn off `yp.*.debug`. `QT_LOGGING_RULES=yp.*.debug=true` turns on state, seek and restart traces.

## Benchmarking (ADR-012)

- **Internal:** `Y-Player --bench-report out.json FILE` (`app/BenchMode.*`) measures what only the process can see, then exits:
  - startup to first expose
  - first frame
  - dropped and delayed frames
  - A/V offset
  - seek latency
- **External:** `bench/run_bench.py` samples CPU, memory and GPU (via PDH) from outside, the same way for every player, and writes `bench/results/`.

## Build layout

- `CMakeLists.txt` (root) adds every module. The presets in `CMakePresets.json` are `debug`, `release`, `relwithdebinfo`, and `ci` (Release + clang-tidy).
- `cmake/YpTargetDefaults.cmake` applies the same settings to every target: C++20, `/W4 /WX /permissive- /utf-8`, root-relative includes, and optional clang-tidy.
- `cmake/Libmpv.cmake` defines the `Mpv::Mpv` target and generates the MSVC import lib from the DLL.
- All executables (the app and the tests) are output to `build/<preset>/bin/`. libmpv is copied there, and `windeployqt` deploys Qt next to `Y-Player.exe`.
- `deps/` (gitignored) holds the pinned Qt and libmpv, fetched by `scripts/fetch_deps.py`.

## UI behaviour (Phase 5)

- **Actions:** every action is defined once in `playerActions()` (id, text, default keys). `MainWindow` creates one `QAction` per id with a handler, and the menus, buttons, context menu (right-click on the video = all menus) and shortcut dialog all use those same actions.
- **Dynamic menus:** recent files, speed, chapters, tracks and devices are rebuilt in `aboutToShow`, so they always reflect engine state without signal bookkeeping.
- **Playback flow:**
  - Playback flows through the playlist. Ended → `nextIndex(false)`, honouring repeat/shuffle.
  - A failed entry is skipped after 0.8 s, stopping after `size()` consecutive failures.
  - "Previous" more than 3 s into a file restarts it.
  - The resume point is recorded whenever the file changes and on close.
- **Persistence:** ADR-014.

## Settings (Phase 6, ADR-016/017)

- **Model:** `core/EngineSettings` and `core/PlayerSettings` hold the values. `ui/SettingsDialog` edits a copy and emits `applied()`. `MainWindow::applyPrefs()` pushes the result to the engine (`applySettings`, which applies a diff) and to the UI (seek steps, always-on-top, …).
- **Profiles:** `EngineSettings` snapshots.
- **Bookmarks:** `core/Bookmarks`.
- **Diagnostics panel:** `ui/DiagnosticsDialog` polls `IPlaybackEngine::diagnostics()` twice a second, only while it is visible.
- **File associations:** `platform/win/FileAssociations`.

## Look and language (Phase 7, ADR-018/019)

- **Theme:** `ui/Theme` applies the brand palette, or the system palette under high contrast. `MainWindow::applyCurrentTheme()` re-creates the glyph icons afterwards.
- **Fullscreen controls:** a focus-less tool window (`overlay_`) that the control bar is reparented into.
- **Translations:** `.qm` files are embedded at `:/i18n`, and `main.cpp` installs the `QTranslator` before any window is created.
- **Icon:** the exe icon comes from `app/y-player.rc`; the window icon is `:/icons/y-player.png`.

## Planned modules (from `context/INIT.md` §4, added in their phases)

- Crash handling (Phase 8)
