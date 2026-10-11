# Roadmap

The full phase plan lives in [`context/PHASES.md`](context/PHASES.md). This file tracks status.

| # | Phase | Status |
|---|-------|--------|
| 0 | Environment & decisions | ✅ Done (2026-10-11) |
| 1 | Skeleton & build pipeline | ✅ Done (2026-10-11) |
| 2 | Minimum functional player | ⏳ Next |
| 3 | Benchmark harness & baseline | — |
| 4 | Pipeline optimization | — |
| 5 | Core playback features | — |
| 6 | Advanced features & settings | — |
| 7 | UI/UX & identity | — |
| 8 | Hardening & reliability | — |
| 9 | Packaging & release | — |

## Phase 1 exit criteria

- [x] A fresh clone plus `scripts\build.cmd` produces a running empty window. Verified by building a copy of the tracked files in a clean folder: dependencies downloaded from scratch, the build passed, and the window opened and closed with exit code 0.
- [x] CI green with at least one unit test. `scripts\build.cmd ci` passes: Release, clang-tidy, and 3 Qt Test suites (logging, libmpv DLL/header match, system info). The GitHub Actions workflow (`.github/workflows/ci.yml`) runs the same command. It has **not** run on GitHub yet because the repo has no remote.
- [x] No warnings. `/W4 /WX` applies to every target, and clang-tidy warnings are errors in `ci`.

## Phase 0 deliverables

- Toolchain installed, with versions recorded in [`README.md`](README.md)
- ADRs in [`DECISIONS.md`](DECISIONS.md)
- Licensing notes in [`LICENSES.md`](LICENSES.md)
- libmpv embedding spike in [`spikes/mpv-embed/`](spikes/mpv-embed/) (`run_matrix.sh` → `results.jsonl`), with results in `DECISIONS.md` (ADR-003)

## Carried into Phase 2

- Implement `MpvEngine : yp::IPlaybackEngine` in `engine/mpv/`, and turn the central widget into the native video surface.
- First GitHub Actions run, once a remote exists.
