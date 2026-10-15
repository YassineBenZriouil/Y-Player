# Architecture Decision Records

Each ADR records the context, the decision, and the evidence behind it. Revisit an ADR when its evidence changes; do not silently drift from it.

---

## ADR-001 — Media engine: libmpv

**Status:** Accepted (2026-10-11)

**Context:** The engine needs broad codec and container support, hardware decoding (D3D11VA), a high-quality renderer, subtitles (libass), and A/V sync, without rewriting a media stack.

**Options**
- **libmpv**: a mature player core on top of FFmpeg. It includes the gpu-next renderer (libplacebo), libass, hwdec selection and fallback, a property/command API, and an event loop.
- **Custom FFmpeg pipeline**: full control, but we would have to reimplement A/V sync, the renderer, subtitles, hwdec interop, and frame scheduling. That is months of work just to reach parity, with high risk.

**Decision:** Use libmpv (client API 2.5, mpv `v0.41.0-1122-gaccb7b282`, shinchiro Windows build). The app talks to it only through an `IPlaybackEngine` interface (Phase 1), so another backend stays possible.

**Evidence:** The spike (`spikes/mpv-embed/`) plays a local MP4 with video and audio inside a Qt window on this machine, as confirmed by the owner. It also plays a synthetic 1080p60 source in both embed modes (ADR-003).

**Revisit if:** Phase 3/4 benchmarks show an mpv bottleneck that cannot be fixed through configuration.

---

## ADR-002 — UI framework: Qt 6 Widgets (6.9.3)

**Status:** Accepted

**Context:** We need a native, low-overhead desktop UI with menus, settings dialogs, shortcuts, DPI scaling, accessibility, and i18n.

**Options**
- **Qt 6 Widgets**: mature, native-feeling, low overhead, and provides `winId()` for embedding. LGPL.
- **Qt Quick/QML**: smoother animations, but heavier, with a scene graph competing for the GPU. Embedding a native swapchain is more awkward.
- **Raw Win32 / WinUI 3**: Win32 is very costly for settings UIs. WinUI 3 has a large runtime and makes embedding a native child swapchain awkward.

**Decision:** Use Qt 6 Widgets with the MSVC build.

**Why 6.9.3 and not 6.11/6.12:** `aqtinstall` 3.3.0 (the latest on PyPI) cannot read the repository layout Qt uses from 6.10 onward. 6.9.3 installs headlessly without a Qt account. Moving to a newer Qt (through the official installer or a fixed aqt) is a Phase 1+ task and does not affect the architecture.

---

## ADR-003 — Render path: native child window (`wid`) + `vo=gpu-next`, `gpu-api=d3d11`

**Status:** Accepted

**Context:** `Init.md` §5.2–5.3 asks for D3D11 presentation, zero-copy hardware frames, and display-synced timing.

**Finding (verified in `third_party/mpv/include/mpv/render.h`):** The libmpv render API supports only **OpenGL** and **software** rendering. It has no D3D11 option. So:
- **Render API (`vo=libmpv`) + QOpenGLWidget**: mpv draws into a Qt OpenGL FBO, and Qt composites it. That means OpenGL on Windows (D3D11VA frames need a WGL/DX interop hop), an extra composition copy, and Qt's paint loop in the timing path.
- **`wid` embedding**: mpv owns a D3D11 swapchain on a native child window. It uses d3d11va surfaces directly (the zero-copy path), plus mpv's own display-sync and frame timing.

**Spike measurements** (`spikes/mpv-embed/results.jsonl`). Setup: synthetic `testsrc2` 1080p60 clips encoded with FFmpeg, 15 s measured after a 3 s warm-up, a 1280×720 window, one sample per cell, on a Ryzen 5 5500 + RTX 3050 + 1080p 180 Hz display. CPU is % of one core.

| Clip | Mode | hwdec active | CPU | VO drops | Working set | Private | Time to first frame |
|------|------|-------------|-----|----------|-------------|---------|------|
| H.264 8-bit | `wid` | d3d11va | **6.1 %** | 0 | 157 MB | 200 MB | 414 ms |
| H.264 8-bit | `wid` | no (SW) | 35.0 % | 0 | 220 MB | 273 MB | 407 ms |
| H.264 8-bit | `gl` | nvdec | 21.5 % | 2 | 242 MB | 682 MB | 96 ms |
| HEVC 10-bit | `wid` | d3d11va | **7.1 %** | 0 | 147 MB | 200 MB | 450 ms |
| HEVC 10-bit | `wid` | no (SW) | 47.6 % | 0 | 293 MB | 352 MB | 442 ms |
| AV1 10-bit | `wid` | d3d11va | **9.6 %** | 0 | 145 MB | 183 MB | 476 ms |
| AV1 10-bit | `wid` | no (SW) | 41.9 % | 0 | 279 MB | 357 MB | 421 ms |
| AV1 10-bit | `gl` | nvdec | 16.1 % | 5 | 273 MB | 1010 MB | 176 ms |
| AV1 10-bit | `gl` | no (SW) | 54.9 % | 4 | 421 MB | 667 MB | 122 ms |

The owner stopped the run partway through, so the VP9 and 4K rows and some `gl` cells were never measured. The owner also confirmed separately that a real MP4 plays with picture and sound.

**Reading:** `wid` with d3d11va uses about 2–3× less CPU, has 0 dropped frames versus 2–5 for `gl`, and has 3–5× less private memory. With `gl`, mpv falls back to nvdec + GL interop rather than d3d11va, which confirms the extra hop. `gl` reaches its first frame faster (100–180 ms versus 400–480 ms for `wid`). The cost on the `wid` side is most likely D3D11 device and swapchain creation per window, and Phase 3 should investigate it (for example by reusing the swapchain across files). These are single samples and only show direction; real benchmarks are Phase 3.

**Decision:** Use `wid` embedding with `vo=gpu-next`, `gpu-api=d3d11`, and `hwdec=auto-safe`.

**Consequences**
- **Airspace:** Qt widgets cannot be drawn semi-transparently on top of the mpv child window. Overlays (seek bar, OSD) must be one of:
  (a) mpv's own OSD/overlay commands,
  (b) separate layered top-level windows that track the video, or
  (c) opaque docked controls.
  Phase 2 uses opaque controls and mpv OSD. Phase 7 evaluates (b).
- Phase 3 must check the cause of the slower time to first frame.

---

## ADR-004 — Toolchain & build: MSVC (VS 2026 Build Tools) + CMake + Ninja, prebuilt dependencies

**Status:** Accepted

**Decision**
- **Compiler:** MSVC 19.51 (toolset 14.51, VS 2026 Build Tools), C++20, `/W4 /permissive-`. It is ABI-compatible with the Qt `msvc2022_64` binaries (the spike links and runs).
- **Build:** CMake 4.3 (bundled with Build Tools) and Ninja 1.13.
- **Dependencies:** prebuilt binaries, not vcpkg/Conan:
  - Qt comes from the official repository via aqtinstall.
  - libmpv comes from shinchiro's `mpv-winbuild-cmake` releases.
  - Building Qt and mpv/FFmpeg from source through vcpkg takes hours and adds no value for now.
- **libmpv import library:** the package ships a MinGW `.dll.a`. CMake generates an MSVC `mpv.lib` from the DLL's exports (`dumpbin` + `lib /def`).
- **x64 only for now.** ARM64 is post-1.0 (`context/PHASES.md`).

**Phase 1 follow-up:** a fetch script with pinned versions and SHA-256 checks instead of manual downloads.

---

## ADR-005 — Product license: GPL-3.0-or-later

**Status:** Accepted as default. **Needs owner confirmation** before any public or commercial distribution.

**Context:** The libmpv build we use is GPL. Its configuration does not set `-Dgpl=false`, and mpv's `gpl` option defaults to `true`. FFmpeg inside it is also a GPL build. Qt is LGPL-3.0.

**Decision:** License Y-Player under GPL-3.0-or-later. That is compatible with all current dependencies.

**Alternative:** if closed-source distribution is needed later, self-build an LGPL libmpv/FFmpeg. That costs some GPL-only filters and encoders. See [`LICENSES.md`](LICENSES.md).

---

## ADR-006 — Security defaults for the embedded engine

**Status:** Accepted

**Context:** The libmpv build includes Lua and JavaScript scripting, libarchive, and user-config loading. Media files are untrusted input (`Init.md` §17).

**Decision:** The product always sets `config=no` and `load-scripts=no`. It never passes user-controlled strings into mpv commands that run shell or scripts. Archive and URL protocol handling stays limited to local files until network playback is designed. The spike already uses `config=no`.

---

## ADR-007 — Product identity

**Status:** Accepted (direction; final assets in Phase 7)

- **Name:** **Y-Player** (the project's working name, unrelated to PotPlayer branding).
- **Visual direction:** dark-first neutral UI (near-black #0E0F12 background, #E6E8EC text) with a single accent (electric teal #19C3B4), and an optional light theme. Thin, precise typography (Segoe UI Variable, the system font, with nothing to bundle).
- **Icon:** placeholder: a stylized "Y" fused with a play triangle. Final icon in Phase 7.

---

## ADR-008 — Unit tests: Qt Test + CTest

**Status:** Accepted (Phase 1)

**Context:** `PHASES.md` suggested GoogleTest or Catch2.

**Decision:** Use Qt Test, which ships with qtbase (already a dependency). It runs through CTest (`ctest --preset <name>`), and each test is one executable, `tests/tst_<area>.cpp`. This adds no dependency or download, and it handles Qt types and event loops natively. GoogleTest's main advantages, parameterized and death tests, are not needed yet.

---

## ADR-009 — Logging: Qt message handler + rotating file

**Status:** Accepted (Phase 1)

**Decision:** Do not add a logging library (spdlog etc.). `core/Logging.*` installs a Qt message handler, so all Qt-internal and Y-Player logging goes through one path. It writes to a size-rotated file in `%LOCALAPPDATA%\Y-Player\logs`. Categories (`yp.<module>`) give per-module filtering through `QT_LOGGING_RULES`. Each record is flushed as it is written, so the last lines survive a crash. Logging volume during playback must stay low (`INIT.md` §5.6). The flush cost is acceptable at that volume and gets measured in Phase 3.

---

## ADR-010 — Dependencies: pinned fetch script into `deps/`

**Status:** Accepted (Phase 1). This replaces the manual steps from Phase 0.

**Decision:** `scripts/fetch_deps.py` installs the following into the gitignored `deps/`:
- **Qt 6.9.3**, `qtbase` only. It is installed through aqtinstall 3.3.0 in a local venv, and aqt verifies Qt's checksums.
- **libmpv**, a pinned release URL plus a SHA-256 check.

The script is idempotent: stamp files skip work that is already done, and a partial Qt install is never mistaken for a complete one. Qt downloads run one at a time with retries, because download.qt.io drops TLS connections under parallel load (seen twice).

`scripts\build.cmd [preset] [run]` is the single entry point used locally and in CI:

1. find MSVC
2. fetch dependencies
3. configure
4. build
5. run the tests

**Static analysis:** clang-tidy (LLVM 23.1.3) runs in the `ci` preset with warnings as errors (`.clang-tidy`). The MSVC `/W4 /WX` setting applies to every preset.

---

## ADR-011 — Run libmpv teardown off the UI thread (unmatched `CoUninitialize`)

**Status:** Accepted (Phase 2). Revised in Phase 5.

**Context:**
- Closing the app after using fullscreen crashed in about 1 of 3 runs with an access violation in `~QApplication`. The symbolized stack was `QWindowsContext::~QWindowsContext` → `winrt::clear_factory_cache` → a call into the already-unloaded `C:\Windows\System32\Windows.UI.dll`.
- A probe showed the UI thread's COM apartment present before `mpv_terminate_destroy` (`CoInitializeEx` returned `S_FALSE`) and **gone** after it (`S_OK`). libmpv's teardown calls `CoUninitialize` on its calling thread without matching it. That tears down Qt's OLE apartment, which unloads the WinRT DLL whose factory Qt cached (for example when entering fullscreen).

**First fix, Phase 2:** hold one extra COM reference on the UI thread.
- This passed the Phase 2 tests.
- In Phase 5 it **failed**: with more engine lifecycles and features in play, the apartment was still destroyed, and the test process crashed in `~QApplication` the same way. A single reference can't absorb an unknown number of stray calls.

**Decision:** `~MpvEngine` runs `mpv_terminate_destroy` on a throwaway `std::thread` and joins it. mpv's stray `CoUninitialize` hits a thread with no COM state, where it does nothing. Teardown stays synchronous, so the video window still outlives mpv. The extra-reference workaround is removed.

**Evidence:**
- `tst_mpvengine`: 23 of 23 pass, including `teardownIsFastAndKeepsComApartment` (UI-thread COM intact after teardown; teardown took 63 ms). The process exits cleanly after 20+ engine lifecycles.
- Before the change, the same test run failed that check and crashed on exit.

**Revisit:** report upstream to mpv.

---

## ADR-012 — Benchmark method: external sampler + in-process report

**Status:** Accepted (Phase 3)

**Context:** PotPlayer exposes no internals, and comparisons must use identical methods (`INIT.md` §6).

**Decision:**
- **External metrics, the same for every player** (`bench/run_bench.py`):
  - CPU as process CPU time over wall time
  - working set and private bytes
  - per-process GPU engine utilisation, split by engine type (3D, VideoDecode, VideoProcessing, Copy)
  - GPU dedicated and shared memory, via Windows PDH counters
  - time from launch to window shown
  - same window size (1280×760), same warm-up and sampling period, players interleaved per clip to spread thermal drift
- **Y-Player-only metrics** come from `--bench-report`: startup, first frame, frame drops, A/V offset, seek latency.
- **Audio** is muted at the default device endpoint for the run and then restored, so both players keep a real audio pipeline while the user hears nothing.
- **Corpus:** synthetic clips from `bench/make_corpus.py`, recorded with SHA-256 hashes in `bench/corpus-manifest.json`. The media itself is not committed.

**Limits:**
- "First frame" is mpv's `playback-restart` (the first frame is ready to present), not a photon measurement.
- Audio underruns are not exposed by libmpv and are not measured.
- PotPlayer's first-frame and seek times can't be observed from outside.

---

## ADR-013 — Small test clips are committed

**Status:** Accepted (Phase 2)

`tests/media/` holds four 3-second 320×240 clips (H.264/AAC MP4, HEVC 10-bit/AAC MKV, VP9/Opus WebM, AV1 10-bit/Opus MKV), about 350 KB in total, plus a fake media file. They're committed so the integration test runs on a fresh clone and in CI, with no download. `tests/media/generate.sh` regenerates them.

---

## ADR-014 — Local persistence: one INI, a session playlist, a resume file

**Status:** Accepted (Phase 5)

**Decision:** Everything Y-Player stores lives in `%LOCALAPPDATA%\Y-Player` (`INIT.md` §17: local only, never uploaded):
- `settings.ini` (`QSettings` INI): volume, mute, audio channels, resume on/off, repeat and shuffle, recent files, subtitle style, shortcut overrides (only bindings that differ from the defaults), always-on-top, current playlist index.
- `session.m3u8`: the playlist, restored on start without autoplay.
- `resume.json`: stop positions keyed by path. Policy: media shorter than 60 s, stops before 10 s and stops in the last 20 s are not kept, and at most 500 entries are stored, evicting the least recently used.
- `logs/`: see ADR-009.

These files are saved on close and when the shortcut or subtitle-style dialogs are accepted. `--bench-report` runs never write them, so benchmarks can't resume old positions or overwrite the user's session.

Phase 6's settings window will edit the same keys, so no migration is needed.

---

## ADR-015 — Window style changes go through Win32, not Qt window flags

**Status:** Accepted (Phase 5)

**Context:** `QWidget::setWindowFlags` (for `WindowStaysOnTopHint`, `FramelessWindowHint`) recreates the top-level native window, and with it the native video child window that libmpv presents into.

**Decision:** Always-on-top uses `SetWindowPos(HWND_TOPMOST)`. Borderless removes `WS_CAPTION` with `SetWindowLongPtr`, keeping the resizable frame (`platform/win/WindowStyle`). In borderless mode a drag on the video moves the window (`QWindow::startSystemMove` after the drag distance, so double-click still toggles fullscreen). Leaving fullscreen re-applies borderless, because Qt restores the caption.

---

## ADR-016 — Settings model: one validated struct, applied to the engine as a diff

**Status:** Accepted (Phase 6)

**Decision:**
- `core/EngineSettings` holds everything the backend applies: hardware decoding, scaler, HDR, smooth motion, picture, filters, equalizer, normalization, languages, cache, threads, log level. It uses backend-neutral values.
- `core/PlayerSettings` adds UI preferences: resume, session restore, autoplay, seek steps, exact seek, always-on-top, subtitle style, channels, volume.
- **Loading validates:** unknown choices fall back to defaults, numbers are clamped, language lists are restricted to letters, and colours must be `#RRGGBB`. So a hand-edited or imported file can never push arbitrary strings into mpv options.
- `IPlaybackEngine::applySettings()` sends only what changed since the last call. Filter chains (`vf`, `af`, shaders) are only rebuilt when their resulting string changes, so an unrelated change causes no playback glitch.

**Filter costs, shown in the UI next to each option:**
- **No measurable cost:** picture controls (renderer).
- **Small GPU cost:** sharpen (a luma unsharp-mask shader, written to `%TEMP%` and tuned through `glsl-shader-opts`, so strength changes don't reload it).
- **GPU cost per frame or refresh:** EWA scaler, smooth motion.
- **CPU filter:** denoise (`hqdn3d`), which forces a GPU→CPU copy per frame with hardware decoding. Flip is also a CPU filter; flip and denoise share one `vf` chain.
- **Negligible CPU:** equalizer (10 × FFmpeg `equalizer`) and `dynaudnorm`.

**Profiles:**
- Built-in profiles are Balanced, High quality and Power saver.
- User profiles are stored as `profiles/<percent-encoded name>/…` in `settings.ini`.
- A profile is an `EngineSettings` snapshot.

**Export/import:**
- Exported: only portable groups (engine, subtitles, audio, playback, window, shortcuts, profiles).
- Never exported: recent files, the playlist, resume points.

**Bookmarks:** stored in `bookmarks.json`, per file.

---

## ADR-017 — File associations: per-user "Open with", the user picks the default

**Status:** Accepted (Phase 6)

**Decision:**
- Under `HKCU\Software\Classes`, register the ProgID `YPlayer.Media` (open command and icon), and add `.ext\OpenWithProgids\YPlayer.Media` for each checked type. No admin rights are needed. Unchecking removes the entry, and the ProgID is removed with the last one.
- Windows 10/11 block programs from setting themselves as the default (UserChoice is hash-protected), so the page offers a button that opens Settings › Default apps instead of trying to bypass that.

---

## ADR-018 — Visual identity and theming

**Status:** Accepted (Phase 7). Finalizes ADR-007.

**Decision:**
- **Icon:** a white "Y" monogram on a teal (#23D3C3 → #12A99C) rounded square. It is generated by `scripts/make_icon.py` (pure Python) into `app/resources/y-player.ico` (16–256 px, for the exe resource) and `y-player.png` (the window icon). It is original: no PotPlayer or third-party art.
- **Theme:**
  - Qt's Fusion style with brand palettes. Dark is #0E0F12/#17191E with #E6E8EC text. Light is #F3F4F6 with #16181C text.
  - The accent is teal: #19C3B4 on dark, and a darker #0E8F84 on light so white text on it stays readable.
  - "Follow Windows" tracks `QStyleHints::colorScheme` live.
  - The only stylesheet is for the slider groove and handle, with a focus border.
- **High contrast:** when Windows contrast themes are on, the native style and system palette are used untouched. Accessibility beats branding.
- **Font:** Segoe UI Variable Text on Windows 11, Segoe UI otherwise. Both ship with Windows, so nothing is bundled.
- **Control icons:** Segoe Fluent Icons or Segoe MDL2 glyphs in palette colours, re-created on every theme change.
- **On-screen messages:** mpv OSD with #E6E8EC text, a #0E0F12 outline and size 30.

---

## ADR-019 — Fullscreen controls, keyboard focus, translations

**Status:** Accepted (Phase 7)

**Fullscreen controls:**
- A Qt child widget can't be drawn over the native mpv window (ADR-003). So in fullscreen the control bar is moved into a frameless `Qt::Tool` window owned by the main window. It's flagged `WindowDoesNotAcceptFocus`, so clicking it never takes keyboard focus from the main window's shortcuts.
- Mouse movement over the video (mouse tracking on `VideoWidget`) shows it. A 2.5 s timer hides it along with the cursor, except while the bar is hovered or the seek bar is being dragged.

**Keyboard focus:**
- Controls use `Qt::TabFocus`: reachable with Tab for keyboard and screen-reader users, while a mouse click doesn't take focus, so Space and the arrows keep driving playback.
- The focused seek bar handles Left/Right, Home and End itself.

**Seek preview:** a time tooltip (plus the chapter name) on hover. Frame thumbnails are deferred: they need a second decoder instance, and `INIT.md` §11 only wants previews "if implemented efficiently".

**Translations:**
- Every UI string goes through `tr()` or `QCoreApplication::translate`.
- `qt_add_translations` compiles `i18n/*.ts` into `.qm` files embedded at `:/i18n`.
- French is the proof language: all 399 strings are translated, maintained by `i18n/fill_fr.py`.
- The language comes from Settings › General (System, English, Français) and applies at start.
- Qt's own built-in dialog texts (standard buttons, file dialogs) stay English until `qttranslations` is added.

**Dependency:** `qttools` (lupdate/lrelease) is added to `scripts/fetch_deps.py`. It's a build tool and isn't shipped.
