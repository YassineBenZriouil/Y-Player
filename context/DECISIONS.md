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
