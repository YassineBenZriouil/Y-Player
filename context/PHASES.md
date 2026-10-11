# Y-Player — Phases to Production

This file turns `INIT.md` into 10 ordered phases. Each phase ends with exit criteria, and those criteria are the gate. Do not start a phase until the one before it meets every exit criterion. The app must build and run at the end of every phase.

| # | Phase | Outcome |
|---|-------|---------|
| 0 | Environment & decisions | Toolchain verified, stack chosen, ADRs written |
| 1 | Skeleton & build pipeline | Empty Qt window builds reproducibly in CI |
| 2 | Minimum functional player | Plays real local files with basic controls |
| 3 | Benchmark harness & baseline | Repeatable measurements, baseline recorded |
| 4 | Pipeline optimization | Verified HW decode / zero-copy, measured gains |
| 5 | Core playback features | Playlists, tracks, subtitles, audio, shortcuts |
| 6 | Advanced features & settings | Filters, HDR, profiles, searchable settings |
| 7 | UI/UX & identity | Original polished interface, context menus, OSD |
| 8 | Hardening & reliability | Recovery paths, stability soak, security review |
| 9 | Packaging & release | Installer, portable build, docs, clean-machine sign-off |

---

## Phase 0 — Environment & Decisions

**Goal:** Know what we have, and choose the stack based on evidence.

- Inspect the OS, the MSVC/Clang version, CMake, Ninja, vcpkg/Conan, the Qt install, and the Git setup.
- Prototype a libmpv integration: render API with D3D11 versus an embedded window (`wid`). Check the licensing of each libmpv build (LGPL vs GPL).
- Decide on Qt 6 Widgets versus an alternative. Decide on libmpv versus a custom FFmpeg pipeline (default: libmpv).
- Choose the original product name, a placeholder icon, and the color palette direction.
- Create `DECISIONS.md` (one ADR for each choice), plus `LICENSES.md` and `ROADMAP.md`.

**Exit criteria**
- Every required tool is installed and its version is recorded in `README.md`.
- ADRs exist for: media engine, UI framework, render path, build/dependency manager, and license model.
- A throwaway spike plays one file through libmpv on this machine.

---

## Phase 1 — Skeleton & Build Pipeline

**Goal:** Set up a clean, buildable foundation with module boundaries in place.

- Create the CMake project (C++20), with Debug, Release, and RelWithDebInfo presets.
- Set up the directory layout: `app/`, `core/` (playback abstraction, state), `engine/mpv/`, `ui/`, `platform/win/`, `tests/`, `bench/`.
- Define the `IPlaybackEngine` interface. The UI must never talk to libmpv directly.
- Add structured logging with levels, written to a rotating file under `%LOCALAPPDATA%`.
- Add a test framework (GoogleTest or Catch2), plus warnings-as-errors and clang-tidy.
- Set up CI (GitHub Actions or a local script) that builds and runs the tests on every commit.
- Write a skeleton `ARCHITECTURE.md`.

**Exit criteria**
- A fresh clone followed by a single documented command produces a running empty window.
- CI is green, and at least one unit test runs.
- The build has no warnings at the chosen warning level.

---

## Phase 2 — Minimum Functional Player

**Goal:** Reliably play real media files.

- Open files through the file dialog, drag-and-drop, and the command line, with full Unicode path support.
- Render video in the main window and play audio through the default device.
- Implement play, pause, stop, seek bar (accurate position/duration), volume, mute, and fullscreen.
- Keep the UI thread from blocking: engine events go through the mpv event loop and are bridged to Qt signals.
- Show basic info on screen: file name, resolution, codec, and the active decoder.
- Shut down cleanly during playback, with no hangs and no leaked threads.
- Add unit tests for the playback state machine and for time/duration math.

**Exit criteria**
- These files play correctly with A/V in sync: H.264, HEVC, VP9, AV1 (where supported), MKV, MP4, WebM.
- Repeated open/close 100 times causes no crash and no growth in handles or threads.
- Closing the app while playing exits in under 1 s.

---

## Phase 3 — Benchmark Harness & Baseline

**Goal:** Get numbers before attempting any optimization.

- Build the test media corpus in `bench/media/`: each codec × 8/10-bit × 1080p/4K/8K, plus HDR/SDR, VFR, high-FPS, high-bitrate, multi-track, and damaged files. Store a manifest with hashes rather than the files themselves.
- Write a `bench/` runner that records:
  - startup time, time to first frame, and seek latency
  - CPU, GPU, working set, and private bytes
  - dropped, late, and repeated frames, plus frame-time jitter
  - audio underruns and A/V drift
  - idle CPU and wakeups
- Run every test N times and report the mean and variance, together with the hardware/driver environment.
- Run the same corpus with the same settings through PotPlayer for comparison.
- Write `PERFORMANCE.md` covering the methodology, baseline table, and identified bottlenecks.

**Exit criteria**
- One command reruns the entire suite and writes machine-readable results (JSON or CSV).
- The baseline for the current build and for PotPlayer is committed.
- Performance targets are set for each hardware class and workload, based on the baseline.

---

## Phase 4 — Media Pipeline Optimization

**Goal:** Make measured, verified efficiency gains without losing correctness.

- Detect hardware: GPU, driver, supported decoders and profiles, display refresh rate, HDR, and audio devices.
- Choose the decoder adaptively for each file (`d3d11va` → `d3d11va-copy` → software), and report the decoder that is actually active.
- If a decoder fails mid-playback, fall back automatically while keeping the position, and never change the user's saved preference.
- Verify zero-copy for each pipeline, and remove GPU→CPU→GPU round trips.
- Tune presentation: display-sync / vsync, the swapchain depth, no redraws while paused, and no rendering stalls caused by the UI.
- Tune demuxer cache and buffer limits, and keep the queues bounded.
- Compare every change against the Phase 3 baseline. Reject changes that bring no gain or that reduce reliability.

**Exit criteria**
- Hardware decode is active and verified on every supported codec/profile on the test machine.
- CPU and power use during steady 1080p/4K playback meet the Phase 3 targets.
- Zero dropped frames on the reference files at native refresh rate (or deviations are documented with their cause).
- `PERFORMANCE.md` is updated with a before/after for each optimization.

---

## Phase 5 — Core Playback Features

**Goal:** Deliver a complete, everyday-usable player.

- **Playback:** frame step forward/back, fine speed control, A-B repeat, resume position (configurable), recent files, chapters.
- **Playlist:** add/remove/reorder, previous/next, repeat-one/all, shuffle, persistence, M3U/PLS import/export, and large lists without stalls.
- **Audio:** track selection, delay, output device selection, channel layout, and device hot-plug handling.
- **Subtitles:** embedded and external tracks, delay, font/size/color/outline/position, Unicode/RTL, and malformed-file tolerance.
- **Video:** aspect ratio and custom ratios, rotate/flip, screenshot capture, always-on-top, borderless.
- **Input:** a rebindable shortcut system with defaults from `INIT.md` §12 and duplicate detection.
- Add unit tests for the playlist, shortcut map, subtitle delay, and resume logic.
- Add integration tests for track switching and subtitle loading.

**Exit criteria**
- Every feature listed in `INIT.md` §8 works with real media. No placeholder controls remain.
- There are no regressions against the Phase 4 benchmark numbers.
- All tests pass.

---

## Phase 6 — Advanced Features & Settings

**Goal:** Reach PotPlayer-level depth, with every feature opt-in and cheap when it is off.

- Add video filters: deinterlace, brightness/contrast/saturation, sharpen, denoise, and scaler selection.
- Add HDR→SDR tone mapping and HDR passthrough where the display supports it.
- Add audio features: equalizer, normalization, and channel mixing.
- Add bookmarks, configurable seek intervals, and playback/rendering profiles.
- Build the stats/diagnostics panel: decoder, renderer, codec, dimensions, FPS, bitrate, drops, and output config.
- Build a searchable settings window with categories from `INIT.md` §11, reset to defaults, and import/export.
- Add file associations (opt-in, per extension, per user without admin).
- Show the estimated CPU/GPU cost of each filter in the UI.

**Exit criteria**
- With every filter off, the benchmarks match Phase 5 within noise.
- Settings persist across restarts, and their import/export round-trips.
- The diagnostics panel shows real values that match `mpv` stats.

---

## Phase 7 — UI/UX & Original Identity

**Goal:** Deliver a fast, polished, unobtrusive interface that is clearly not PotPlayer.

- Finalize the name, icon, typography, colors, and both dark and light themes.
- Make controls auto-hide, add an OSD for volume, seek, speed, and track changes, and add an efficient seek-bar hover preview.
- Build full context menus (playback, video, audio, subtitles, aspect, filters, speed, tracks, screenshot, window, advanced), with every entry functional and shortcuts shown.
- Handle multi-monitor setups, DPI scaling, resizing, and fullscreen transitions without flicker.
- Accessibility: full keyboard navigation, screen-reader labels, high-contrast support.
- Localization readiness: every string goes through `tr()`, with one non-English translation as proof.

**Exit criteria**
- The UI stays responsive (no input lag over 50 ms) during 4K playback.
- An accessibility checklist pass is done with Narrator plus keyboard-only use.
- No PotPlayer assets or branding appear anywhere.

---

## Phase 8 — Hardening & Reliability

**Goal:** Make sure the app never crashes, hangs, or leaks in real use.

- Recovery paths: corrupted media, unsupported codecs, decoder init failure, GPU device loss, audio device removal, missing subtitles, permission/disk errors, and unavailable network streams.
- Stress scenarios: rapid file switching, seek spam, track switching during playback, resize/minimize loops, and suspend/resume.
- Add a crash handler (minidump written locally, never uploaded without consent).
- Soak test: 12 h or more of continuous playback with flat memory and GPU resource use.
- Fuzz the subtitle and playlist parsers with malformed input.
- Security review: untrusted-input handling, path traversal, no shell construction, no admin requirement, and a dependency CVE check.
- Add performance-regression tests to CI (fail the build on regressions beyond a set threshold).
- Write `TESTING.md` covering procedures, supported environments, and how unsupported test environments are distinguished from real failures.

**Exit criteria**
- Every scenario in `INIT.md` §16 is tested, and none of them crash or deadlock.
- The 12 h soak shows no memory growth beyond a documented tolerance.
- Fuzzing runs for N hours without a crash.
- No known high or critical CVEs remain in the shipped dependencies.

---

## Phase 9 — Packaging & Release

**Goal:** Make the app installable, documented, and shippable.

- Make the release build reproducible: optimized, with PDBs archived separately, and code-signed if a certificate is available.
- Provide both an installer (MSIX or Inno Setup/WiX) and a portable ZIP.
- Produce third-party notices for FFmpeg, libmpv, Qt, and the codecs, and comply with LGPL/GPL obligations.
- Finalize the docs: `README.md`, `ARCHITECTURE.md`, `PERFORMANCE.md` (final numbers vs PotPlayer: wins, losses, and why), `TESTING.md`, `DECISIONS.md`, `LICENSES.md`, `CONTRIBUTING.md`.
- Update strategy: opt-in update check only, with signed downloads.
- Release checklist: version bump, changelog, full test plus benchmark run, clean-VM install/uninstall test on Windows 10 and 11.

**Exit criteria**
- The app installs, runs, plays the corpus, and uninstalls cleanly on a fresh Windows 10 VM and a fresh Windows 11 VM.
- Uninstalling leaves no orphaned files or registry keys (except user settings, when the user chooses to keep them).
- Every item in the `INIT.md` §22 Definition of Done holds for the whole app.
- v1.0.0 is tagged.

---

## Post-1.0 (not blocking release)

- ARM64 build
- Network streaming hardening (HLS/DASH, YouTube-style URL handling via yt-dlp, opt-in)
- Media library view
- A second backend (custom FFmpeg pipeline), only if the benchmarks justify it
