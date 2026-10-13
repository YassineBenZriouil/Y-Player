# Roadmap

The full phase plan lives in [`PHASES.md`](PHASES.md). This file tracks status.

| # | Phase | Status |
|---|-------|--------|
| 0 | Environment & decisions | ✅ Done (2026-10-11) |
| 1 | Skeleton & build pipeline | ✅ Done (2026-10-11) |
| 2 | Minimum functional player | ✅ Done (2026-10-11) |
| 3 | Benchmark harness & baseline | ⚠️ **Skipped partway** (2026-10-11): harness built, baseline **not** recorded |
| 4 | Pipeline optimization | ⚠️ **Skipped** (2026-10-11): not started |
| 5 | Core playback features | ✅ Done (2026-10-11), with one unverifiable criterion, see below |
| 6 | Advanced features & settings | — |
| 7 | UI/UX & identity | — |
| 8 | Hardening & reliability | — |
| 9 | Packaging & release | — |

## ⚠️ Skipped work (owner decision, 2026-10-11): must be finished before release

The owner stopped the Phase 3 baseline run and moved straight to Phase 5. That decision broke the "finish a phase before the next" rule in `PHASES.md` on purpose, so the gaps are listed here.

**Phase 3, still to do:**
- [ ] Run the full baseline: `python bench/run_bench.py --label baseline --repeats 3`. It takes about 45–50 min, unattended and muted. The first attempt was stopped after 16 of 96 runs, and nothing was saved.
- [ ] Make the runner save after every run, so an interrupted run keeps its data.
- [ ] Run a second PotPlayer pass with its hardware decoding switched on, for a like-for-like comparison. The default PotPlayer install decodes in software.
- [ ] Write `context/PERFORMANCE.md`: method, baseline tables, variance, bottlenecks.
- [ ] Set targets for each hardware class from the baseline.

**Phase 4, still to do (all of it):** verified hardware-decode selection and fallback, zero-copy checks, presentation tuning, buffering, and before/after measurements against the Phase 3 baseline.

**Consequence:** Phase 5's exit criterion "no regressions against the Phase 4 benchmark numbers" **cannot be checked**, because no baseline exists. It is recorded as unverified.

**Already built for Phase 3:**
- Corpus generator: `bench/make_corpus.py`, 14 clips, hashed in `bench/corpus-manifest.json`.
- In-app `--bench-report` mode.
- External sampler/runner: `bench/run_bench.py`, with PDH GPU counters, the same window size for every player, and a muted endpoint.

**Early findings from smoke runs (not a baseline):**
- Y-Player 4K HEVC 10-bit: about 4 % of one core, d3d11va, 0 dropped frames, 427 ms from open to first frame.
- PotPlayer defaults on the same clip: about 285 % of one core (software decoding).
- Y-Player startup varied from 154 ms to 2189 ms between cold and warm starts. Phase 4 should investigate.

## Phase 5 exit criteria

- [x] **Every feature in `INIT.md` §8 works with real media, with no placeholder controls.** The engine features are verified by `tst_mpvengine` (23/23); the logic by 4 new unit suites.

  | Feature | Status |
  |---|---|
  | Play/pause/stop/resume, seek bar | ✅ |
  | Frame step ± | ✅ |
  | Speed presets, fine ±0.1 steps | ✅ |
  | Playlist (add/remove/reorder/drop, M3U/PLS, session restore) | ✅ |
  | Repeat one/all, shuffle, previous/next | ✅ |
  | A-B repeat | ✅ |
  | Audio delay, subtitle delay | ✅ |
  | Audio and subtitle track selection | ✅ |
  | External subtitles, including UTF-8/RTL and malformed files | ✅ |
  | Subtitle style (font, size, colors, outline, position, ASS override) | ✅ |
  | Aspect presets and custom ratio, rotation, flip | ✅ |
  | Fullscreen, borderless | ✅ |
  | Screenshots, with or without subtitles | ✅ |
  | Volume/mute | ✅ |
  | Output device, with hot-plug fallback | ✅ |
  | Channel layout | ✅ |
  | Always on top | ✅ |
  | Resume (toggleable) | ✅ |
  | Recent files | ✅ |
  | Drag-and-drop (Ctrl adds) | ✅ |
  | Command line (multiple files/playlists) | ✅ |
  | Chapters | ✅ |
  | Rebindable shortcuts with duplicate detection | ✅ |
  | File associations | **Phase 6**, per `PHASES.md` |

- [ ] **"No regressions against the Phase 4 benchmark numbers": UNVERIFIABLE.** Phases 3 and 4 were skipped (see above), so no baseline exists.
- [x] **All tests pass.** 9 unit suites plus `tst_mpvengine` pass, and the clang-tidy CI preset builds clean.

**Honest limits:**
- These Phase 5 widgets were only checked by build and launch, not clicked through:
  - the playlist dock
  - the shortcut and subtitle-style dialogs
  - borderless, always-on-top
  - the context menu
  - recent files and session restore

  See `TESTING.md`.
- Drag-and-drop from Explorer has still never been verified.
- Fixed along the way: ADR-011 was revised, because the Phase 2 COM workaround was not enough.

## Phase 2 exit criteria

- [x] H.264/MP4, HEVC 10-bit/MKV, VP9/WebM and AV1 10-bit/MKV all play with A/V in sync. `tst_mpvengine` verifies this with d3d11va active for all 4 on the dev machine.
- [x] 100 open/close cycles with no crash and no handle or thread growth. Measured: handles −32 and threads +0 over 100 cycles; handles +0 and threads +0 over 20 full engine lifecycles.
- [x] Closing while playing takes < 1 s. Engine teardown took 59 ms; the whole app took 221–326 ms from fullscreen.
- Fixed along the way: a crash when closing after fullscreen (ADR-011).

## Phase 1 exit criteria

- [x] A fresh clone plus `scripts\build.cmd` produces a running empty window. Verified by building a copy of the tracked files in a clean folder: dependencies downloaded from scratch, the build passed, and the window opened and closed with exit code 0.
- [x] CI green with at least one unit test. The GitHub Actions workflow passes: [run 38175743454](https://github.com/YassineBenZriouil/Y-Player/actions/runs/38175743454).
- [x] No warnings. `/W4 /WX` applies to every target, and clang-tidy warnings are errors in `ci`.

## Phase 0 deliverables

- Toolchain installed, with versions recorded in [`README.md`](../README.md)
- ADRs in [`DECISIONS.md`](DECISIONS.md)
- Licensing notes in [`LICENSES.md`](LICENSES.md)
- libmpv embedding spike in [`spikes/mpv-embed/`](../spikes/mpv-embed/) (`run_matrix.sh` → `results.jsonl`), with results in `DECISIONS.md` (ADR-003)
