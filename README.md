# Y-Player

A high-performance Windows video player built on libmpv and Qt 6. Goals and constraints are in [`context/INIT.md`](context/INIT.md), the phase plan is in [`context/PHASES.md`](context/PHASES.md), and progress is in [`context/ROADMAP.md`](context/ROADMAP.md).

**Status:** Phases 0–2 and 5 are complete. Phase 3 is partly done (the benchmark harness exists, but no baseline has been recorded yet) and Phase 4 hasn't started. Both were skipped on purpose, and [`context/ROADMAP.md`](context/ROADMAP.md) lists what remains.

Y-Player plays local files with hardware decoding and has:
- playlists (M3U/PLS, repeat, shuffle)
- audio and subtitle track switching, external subtitles, delays, subtitle styling
- speed control, frame stepping, A-B repeat, chapters
- aspect, rotation and flip, screenshots
- audio device and channel selection
- resume, recent files
- rebindable shortcuts

## Everyday commands

Run these from the project folder in a Command Prompt or PowerShell:

| Command | What it does |
|---------|--------------|
| `build` | Builds `build\release\bin\Y-Player.exe` (no tests). Close the player first, because Windows locks a running exe. |
| `run` | Starts the already-built player. Files are passed through: `run "C:\path\video.mkv"` |
| `build-run` | Both: build, then start. |

`scripts\build.cmd` is still the full build: deps, all targets, tests (see Quick start below).

## Using the player

```bat
build\release\bin\Y-Player.exe "C:\path\to\video.mkv"
```

You can also start it with no arguments and use **File › Open** (Ctrl+O), or drag a file onto the window.

Default keys (rebind them in **Tools › Keyboard Shortcuts**, Ctrl+K). Right-click the video for every menu.

| Key | Action | Key | Action |
|-----|--------|-----|--------|
| Space | Play / pause | Ctrl+Space | Stop |
| ← / → | Seek 5 s (Shift: 30 s) | PgUp / PgDn | Previous / next file |
| , / . | Previous / next frame | [ / ] / Backspace | Slower / faster / normal speed |
| ↑ / ↓ | Volume | M | Mute |
| A | Next audio track | S | Next subtitle track (then off) |
| Ctrl+− / Ctrl+= / Ctrl+0 | Audio delay | Z / X / Shift+Z | Subtitle delay |
| L / Shift+L | A-B repeat / clear | Ctrl+PgUp / Ctrl+PgDn | Chapters |
| R / H | Cycle repeat / shuffle | F6 | Playlist |
| F, Enter, double-click | Fullscreen | Esc | Leave fullscreen |
| Ctrl+E (Shift: no subs) | Screenshot to Pictures\Y-Player | Ctrl+A / Ctrl+R | Cycle aspect / rotate |
| Ctrl+T / Ctrl+B | Always on top / borderless | O, Ctrl+O / Ctrl+Shift+O | Open / add to playlist |
| Alt+O | Load subtitle file | Ctrl+S / Ctrl+Q | Save playlist / quit |

## Quick start

Requirements:
- Windows 10/11 x64
- Visual Studio Build Tools with the C++ workload
- Python 3.10+
- An internet connection for the first build

Then run one command:

```bat
scripts\build.cmd
```

This finds MSVC, fetches the pinned Qt and libmpv into `deps/`, then configures, builds, and runs the tests. The app ends up at `build\debug\bin\Y-Player.exe`. Add `run` to launch it after a green build: `scripts\build.cmd run`.

| Command | What it does |
|---------|--------------|
| `scripts\build.cmd` | Debug build + tests |
| `scripts\build.cmd release` | Optimized build + tests |
| `scripts\build.cmd relwithdebinfo` | Optimized build with PDBs + tests |
| `scripts\build.cmd ci` | Release + clang-tidy (warnings are errors) + tests. Same as GitHub Actions |

You can also use the CMake presets directly from a "x64 Native Tools" prompt: `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug`.

To install the Build Tools:

```
winget install --id Microsoft.VisualStudio.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

The `ci` preset also needs LLVM for clang-tidy: `winget install LLVM.LLVM`.

## Repository layout

```
app/            Y-Player.exe entry point (composition root)
core/           Qt Core-only logic: IPlaybackEngine interface, logging
engine/mpv/     libmpv backend
platform/win/   Windows-specific queries (GPU/OS/CPU)
ui/             Qt Widgets UI
tests/          Qt Test unit tests (run with ctest)
bench/          benchmark corpus generator and runner (ADR-012)
cmake/          shared CMake modules
scripts/        build.cmd, fetch_deps.py
spikes/         throwaway experiments (not part of the product build)
context/        project docs: brief, phase plan, architecture, decisions, licenses, roadmap
deps/           fetched dependencies (gitignored)
```

The module boundaries are described in [`context/ARCHITECTURE.md`](context/ARCHITECTURE.md).

**Logs:** `%LOCALAPPDATA%\Y-Player\logs\y-player.log`. For debug output in a release build, set `QT_LOGGING_RULES=yp.*.debug=true`.

## Development environment (verified 2026-10-11)

| Item | Version / value |
|------|-----------------|
| OS | Windows 11 Pro 25H2 (10.0.26200), x64 |
| CPU / RAM | AMD Ryzen 5 5500 (6C/12T), 16 GB |
| GPU | NVIDIA GeForce RTX 3050, driver 560.94 |
| Display | 1920×1080 @ 180 Hz |
| Compiler | MSVC 19.51.36260 (VS 2026 Build Tools, toolset 14.51) |
| Windows SDK | 10.0.26100.0 |
| CMake / Ninja | 4.3.1-msvc1 / 1.13.2 (bundled with Build Tools) |
| clang-tidy | LLVM 23.1.3 |
| Qt | 6.9.3 `msvc2022_64`, `qtbase` (fetched into `deps/qt`) |
| libmpv | mpv v0.41.0-1122-gaccb7b282, client API 2.5 (fetched into `deps/mpv`) |
| Python | 3.13.15 |
| FFmpeg CLI (dev tool, not shipped) | N-127367-gb5902e69c7, at `C:\Users\Me\tools\ffmpeg` |

## Docs

- [`context/ARCHITECTURE.md`](context/ARCHITECTURE.md): modules, boundaries, threading, logging
- [`context/DECISIONS.md`](context/DECISIONS.md): architecture decision records
- [`context/LICENSES.md`](context/LICENSES.md): third-party licensing
- [`context/ROADMAP.md`](context/ROADMAP.md): phase status
- [`context/TESTING.md`](context/TESTING.md): test suites and environments
- `context/PERFORMANCE.md`: benchmark results. Not written yet, because the Phase 3 baseline was skipped (see ROADMAP)
