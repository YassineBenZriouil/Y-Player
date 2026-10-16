# Porting Y-Player to macOS

Y-Player is currently **Windows + MSVC only**. This document lists everything that
blocks a macOS build and what to change, area by area. It is a porting plan, not a
claim that the port exists.

Work top to bottom: the build system and the platform layer must exist before the
app will link, and the mpv render-path changes are needed before video appears.

## 0. Decide the strategy first

Two viable routes:

1. **Homebrew Qt + Homebrew libmpv** (fastest). `brew install qt mpv`, use
   pkg-config/find_library for libmpv. Good for getting something running locally.
2. **Pinned fetch like Windows** (reproducible). Extend `scripts/fetch_deps.py`
   with a macOS branch that installs Qt via `aqt` (`mac`/`desktop`) and a pinned
   libmpv build. More work; matches the repo's ADR-010 "pinned deps" philosophy.

This guide assumes route 1 for the first pass and notes where route 2 differs.

## 1. Build system

| File | Windows assumption | macOS change |
|---|---|---|
| `CMakeLists.txt:4-6` | `if(NOT MSVC) FATAL_ERROR` | Allow `MSVC` **or** `APPLE`/Clang |
| `cmake/YpTargetDefaults.cmake:13` | `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus` | Use `-Wall -Wextra -Werror -fPIC` (Clang) |
| `cmake/YpTargetDefaults.cmake:14-15` | defines `UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN` | Drop the Win32 defines for non-Windows targets |
| `cmake/Libmpv.cmake` | needs `deps/mpv/libmpv-2.dll`; builds a `.lib` import lib via MSVC `dumpbin`/`lib` | On macOS link `libmpv.dylib` from Homebrew; the whole `.def`/import-lib block must be skipped |
| `app/CMakeLists.txt:1` | `qt_add_executable(y-player WIN32 ...)` | Drop `WIN32`; add `MACOSX_BUNDLE` and an `Info.plist` |
| `app/CMakeLists.txt:1` | `y-player.rc` (Windows resource) | Remove on macOS; see §6 for `.icns` |
| `app/CMakeLists.txt:7-12` | `windeployqt.exe` post-build | Replace with `macdeployqt` (bundle Qt frameworks + cocoa plugin) |
| `cmake/Libmpv.cmake:last` | copies `libmpv-2.dll` next to the exe | Copy `libmpv*.dylib` (or fix rpath with `install_name_tool`) into the bundle |
| `CMakePresets.json:10` | `CMAKE_PREFIX_PATH=deps/qt/6.9.3/msvc2022_64` | Add a `mac` preset pointing at the Homebrew Qt prefix |
| `scripts/build.cmd`, `build.cmd`, `run.cmd`, `build-run.cmd` | cmd.exe batch | Add `.sh` equivalents (`build`, `run`, `build-run`) |
| `scripts/fetch_deps.py:108-112` | exits if `sys.platform != "win32"` | Add a `darwin` branch (or document the Homebrew manual step) |
| `.github/workflows/ci.yml:9` | `runs-on: windows-2025` | Add a `macos-*` job (or skip CI for now) |
| `.clang-tidy` | header filter uses `[\\/]` | Already portable; ensure `clang-tidy` is on `PATH` |

`scripts/fetch_deps.py` also hardcodes `win64_msvc2022_64` Qt archives and a
Windows libmpv `.7z`; those constants are Windows-only.

## 2. Platform layer (`platform/win/` → `platform/mac/`)

The whole `platform/win/` target (`yp_platform_win`, links `dxgi shell32`) is
Win32. Create a parallel target `yp_platform_mac` implementing the same headers,
and select it in CMake by platform. Call sites to satisfy:

- **`SystemInfo.h/.cpp`** — `querySystemInfo()` uses DXGI to enumerate GPUs and
  `QSettings` on `HKLM\HARDWARE\...` for the CPU name; `processAgeMs()` uses
  `GetProcessTimes`. On macOS: GPU via Metal (`MTLCopyAllDevices`) or
  `system_profiler SPDisplaysDataType`; CPU via `sysctlbyname("machdep.cpu.brand_string")`;
  process age via `proc_pidinfo`/`PROC_PIDTASKINFO` or `sysctl KERN_PROC_PID`.
  This is used by `app/main.cpp:28-33`, `app/BenchMode.cpp`, `tests/tst_systeminfo.cpp`.
- **`WindowStyle.h/.cpp`** — `setAlwaysOnTop`, `setBorderless`, `highContrastEnabled`.
  On macOS use the NSWindow (`setLevel:`/`setStyleMask:`) through a small
  Objective-C++ (`.mm`) file. **Do not** switch to `Qt::WindowStaysOnTopHint`/
  `setWindowFlags`: ADR-015 explains that recreating the native window destroys
  the child window mpv presents into. `highContrastEnabled()` →
  `NSWorkspace.accessibilityDisplayShouldIncreaseContrast`.
- **`FileAssociations.h/.cpp`** — registry + `SHChangeNotify` + `ms-settings:defaultapps`.
  On macOS use `LSSetDefaultRoleHandlerForContentType`/UTIs plus a
  `CFBundleDocumentTypes` entry in `Info.plist`; `openDefaultAppsSettings()` is
  not needed (macOS has no equivalent "default apps" pane). Consumed by
  `ui/SettingsDialog.cpp:3,396,405-406`.

## 3. Engine (`engine/mpv/`)

- **`MpvEngine.cpp:84-108` (options)** — `gpu-api=d3d11` is Windows-specific. On
  macOS set `gpu-api=metal` (or `auto`). `osd-font=Segoe UI` is not installed on
  macOS; use `.AppleSystemUIFont` or leave default.
- **`MpvEngine.cpp:110-... hwdec`** — the valid hwdec values are `d3d11va`/
  `d3d11va-copy` (`core/EngineSettings.h:17`, `core/PlayerSettings.cpp:76`,
  `ui/SettingsDialog.cpp:139-140`). On macOS these become `videotoolbox` /
  `videotoolbox-copy`. Make the valid set platform-aware.
- **`MpvEngine.cpp:186-196` (`attachVideoOutput`)** — `wid` is the native window
  handle. On macOS mpv's `wid` expects an **`NSView*`**, not an integer. See §4.
- **`MpvEngine.cpp:176-184` (teardown)** — the throwaway-thread join exists to
  dodge an unmatched `CoUninitialize()` on the UI thread (ADR-011). That bug is
  Win32/OLE-specific; the workaround is harmless but the comment/rationale no
  longer applies. Keep the off-thread teardown if it still measures faster.
- **`MpvInfo.cpp`** — loads `libmpv-2.dll` by name; change the library name to
  `libmpv.dylib` / the bundled path on macOS.

### Render path note
Windows embeds mpv into a native child window via `--wid`. On macOS `--wid`
(embedding into an `NSView`) works but is less supported than on Windows. If it
proves flaky, the robust alternative is mpv's **render API**
(`vo=libmpv` + `mpv_render_context`) drawing into a `QOpenGLWidget`/Metal layer.
That is a larger change to `MpvEngine`, `IPlaybackEngine`
(`core/PlaybackEngine.h`), and `VideoWidget`. Try `--wid` first.

## 4. Window handle (`WId`) portability

`ui/MainWindow.cpp:165` and `tests/tst_mpvengine.cpp:65` do
`static_cast<quintptr>(video_->winId())` / `window_->winId()`. On Windows `WId`
is integer-like, but on macOS `WId` is an `NSView*` (`void*`), so that cast may
not compile. Introduce one helper (e.g. `nativeViewHandle(QWidget*)`) that
returns the platform native handle and passes the `NSView*` to mpv's `wid`.
`ui/VideoWidget.cpp` already sets `WA_NativeWindow`/`WA_PaintOnScreen`, which is
the right setup on both platforms.

## 5. UI bits

- **`ui/Theme.cpp:75-80`** — picks `Segoe UI Variable Text`/`Segoe UI`. Add a macOS
  fallback (`.AppleSystemUIFont` / `Helvetica Neue`).
- **`ui/Theme.cpp:82`** — calls `platform::highContrastEnabled()` (see §2).
- **`ui/MainWindow.cpp:489-497,523`** — always-on-top / borderless via the
  platform layer (see §2); keep the ADR-015 approach on macOS too.
- **Shortcuts** (`ui/ShortcutMap.cpp`, `context/INIT.md` §12) use `Ctrl+...`
  portable sequences. Qt maps `Ctrl` to Command on macOS automatically, but the
  `tst_shortcutmap` default must still match `INIT.md`. Decide whether the docs
  should say `Cmd` on macOS, and whether Ctrl-based bindings (e.g. `Ctrl+Space`)
  collide with macOS system shortcuts (Spotlight etc.).

## 6. App bundle, icon, packaging

- Remove the `.rc` from the macOS build; supply an `.icns`. `scripts/make_icon.py`
  generates the Windows `.ico` — add `.icns` generation (or `iconutil` from a set).
- Add an `Info.plist` declaring `CFBundleDocumentTypes` (for open-with),
  `NSHighResolutionCapable`, and the app `CFBundleIdentifier`.
- `ui/MainWindow.cpp:474` writes screenshots to `PicturesLocation` — fine on macOS.
- Runtime data already uses `QStandardPaths::AppLocalDataLocation`
  (`app/main.cpp:65`), which resolves to
  `~/Library/Application Support/Y-Player` — no change needed (ADR-014 wording is
  Windows-specific but the behavior is portable).

## 7. Tests

- `tests/tst_mpvengine.cpp:95` skips when no D3D11 output exists — that skip logic
  is Windows-specific. Replace with a macOS-appropriate capability check (or a
  generic "engine could not initialize a video output" skip).
- `tests/tst_mpvengine.cpp:358` calls `CoUninitialize()` (OLE) — Windows-only.
- `tests/tst_systeminfo.cpp` assumes GPU/CPU strings; adjust expectations for
  macOS output.
- Test media (`tests/media/`) is committed and codec-agnostic; `generate.sh`
  needs `ffmpeg` (Homebrew) but is not required to run tests that use committed media.
- ctest environment injection in `tests/CMakeLists.txt:12-13` sets Windows-style
  `PATH`; on macOS the equivalent is `DYLD_FRAMEWORK_PATH`/`DYLD_LIBRARY_PATH`.

## 8. Licensing / distribution

GPL-3.0-or-later is unchanged. Distribution needs notarization + code signing for
Gatekeeper, and bundling `libmpv.dylib` plus its FFmpeg dylib dependencies
(`macdeployqt` does the Qt half only; use `dylibbundler` or fix rpaths for mpv).

## 9. Suggested order of work

1. CMake: allow Clang/APPLE, add a `mac` preset, stub `platform/mac/` so `y-player` links.
2. Replace `Libmpv.cmake` for macOS (link Homebrew `libmpv`).
3. Implement `platform/mac/` (SystemInfo, WindowStyle, FileAssociations).
4. Fix `attachVideoOutput` + `WId` helper; set `gpu-api=metal` and `videotoolbox`.
5. Get a window + video on screen, then run `ctest` and fix the test skips.
6. Icon, Info.plist, `macdeployqt`, then CI.

## Reference: files that mention Windows APIs

```
CMakeLists.txt, cmake/YpTargetDefaults.cmake, cmake/Libmpv.cmake
CMakePresets.json, app/CMakeLists.txt, app/y-player.rc
scripts/build.cmd, build.cmd, run.cmd, build-run.cmd, scripts/fetch_deps.py
platform/win/{FileAssociations,SystemInfo,WindowStyle}.{h,cpp}
engine/mpv/MpvEngine.{h,cpp}, engine/mpv/MpvInfo.cpp
ui/MainWindow.cpp, ui/VideoWidget.{h,cpp}, ui/Theme.{h,cpp}, ui/SettingsDialog.cpp
core/EngineSettings.h, core/PlayerSettings.cpp
app/main.cpp, app/BenchMode.cpp
tests/tst_mpvengine.cpp, tests/tst_systeminfo.cpp
.github/workflows/ci.yml, .clang-tidy
```
