# Y-Player

A high-performance Windows video player built on libmpv and Qt 6. Goals and constraints are in [`context/INIT.md`](context/INIT.md), the phase plan is in [`context/PHASES.md`](context/PHASES.md), and progress is in [`context/ROADMAP.md`](context/ROADMAP.md).

**Status:** Phase 1 is complete. The app opens an empty player window, and there is no playback yet (that's Phase 2).

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
bench/          benchmark runner (Phase 3)
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
