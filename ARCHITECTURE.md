# Architecture

Status: Phase 1 skeleton. Each section says what exists today and what the module is for. Update this file whenever a boundary moves.

## Modules and dependencies

```
app/  (Y-Player.exe: composition root, startup, logging setup)
 ├──> ui/            yp_ui            Qt Widgets: MainWindow (menus, video area)
 ├──> engine/mpv/    yp_engine_mpv    libmpv backend (Mpv::Mpv imported target)
 ├──> platform/win/  yp_platform_win  Windows-only queries (DXGI adapters, OS/CPU)
 └──> core/          yp_core          Qt Core only: IPlaybackEngine, logging
          ^
          └── ui/ and engine/mpv/ depend on core/; nothing depends on app/ or ui/
```

**Rules**
- `ui/` never includes `engine/` or libmpv headers. It talks to playback only through `yp::IPlaybackEngine` (`core/PlaybackEngine.h`). The app layer connects the two, and is the only place that knows a concrete engine.
- `core/` depends on Qt Core only, with no Widgets or Gui, so it stays unit-testable without a display.
- Windows API calls live in `platform/win/`, with one exception: `engine/mpv/` passes an HWND to mpv.
- Each module is a static library linked into the single exe. There are no plugins, services, or extra processes.

## Threading (contract defined in `IPlaybackEngine`)

- All `IPlaybackEngine` methods and signals live on the UI thread.
- libmpv runs its own demux, decode, render, and audio threads (ADR-001/003). Its wakeup callback only posts a queued call to the UI thread, which then drains events. The spike already demonstrates this pattern (`spikes/mpv-embed/main.cpp`).
- The logger is thread-safe: one mutex, and each record is written and flushed while holding it.

## Video presentation (ADR-003)

`MainWindow`'s central widget becomes a native child window, and mpv renders into it with `vo=gpu-next` and `gpu-api=d3d11` (Phase 2). Qt never paints over it. Overlays use mpv OSD or separate windows, never translucent child widgets.

## Logging

`yp::log::install()` (`core/Logging.*`) captures every `qDebug`/`qCInfo`/... call:

- **File:** `%LOCALAPPDATA%\Y-Player\logs\y-player.log`, rotated at 5 MB, keeping 3 old files.
- **Format:** `<UTC ISO-8601 ms> <level> [<category>] tid=<thread> <message>`, with exactly one line per record.
- **Categories:** `yp.<module>`, for example `yp.app`. Release builds turn off `yp.*.debug`. The `QT_LOGGING_RULES` environment variable overrides this when you need diagnostics.

## Build layout

- `CMakeLists.txt` (root) adds every module. The presets in `CMakePresets.json` are `debug`, `release`, `relwithdebinfo`, and `ci` (Release + clang-tidy).
- `cmake/YpTargetDefaults.cmake` applies the same settings to every target: C++20, `/W4 /WX /permissive- /utf-8`, root-relative includes, and optional clang-tidy.
- `cmake/Libmpv.cmake` defines the `Mpv::Mpv` target and generates the MSVC import lib from the DLL.
- All executables (the app and the tests) are output to `build/<preset>/bin/`. libmpv is copied there, and `windeployqt` deploys Qt next to `Y-Player.exe`.
- `deps/` (gitignored) holds the pinned Qt and libmpv, fetched by `scripts/fetch_deps.py`.

## Planned modules (from `context/INIT.md` §4, added in their phases)

- Media session and playlist (Phase 5)
- Settings and profiles (Phase 6)
- Input and shortcut mapping (Phase 5)
- Diagnostics and crash handling (Phases 6/8)
- Benchmark runner in `bench/` (Phase 3)
