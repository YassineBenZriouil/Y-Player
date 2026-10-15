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
| 6 | Advanced features & settings | ✅ Done (2026-10-15, owner-tested) |
| 7 | UI/UX & identity | 🧪 Implemented (2026-10-15), **awaiting owner testing** |
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

## Phase 7: implemented, not yet tested

Nothing was run by the implementer. It compiles cleanly (`/W4 /WX` and the clang-tidy CI preset).

**What to check:**

1. **Identity:**
   - The teal "Y" icon shows on the exe in Explorer, the taskbar, Alt+Tab and the title bar.
   - About shows the version, a description and the licence.
   - The UI uses Segoe UI Variable on Windows 11.
2. **Themes** (Settings › General › Theme):
   - Dark and Light both look consistent: menus, dialogs, playlist, sliders in teal.
   - "Follow Windows" switches live when you change Windows' app mode, and the control icons recolour too.
3. **High contrast:** turn on a Windows contrast theme and restart. The player should use the system's high-contrast colours, not the brand palette.
4. **Fullscreen controls:**
   - In fullscreen, moving the mouse shows the control bar at the bottom over the video. It hides after 2.5 s together with the cursor.
   - It stays visible while hovered or while dragging the seek bar.
   - Shortcuts (Space, arrows, Esc) still work after clicking a button in that bar.
   - Leaving fullscreen puts the bar back under the video.
5. **Seek preview:** hovering the seek bar shows a tooltip with the time, plus the chapter name on `tests/media/multitrack.mkv`.
6. **On-screen messages:** light text with a dark outline, readable on bright and dark frames.
7. **Window position:**
   - Size and position are restored after a restart, on the same monitor.
   - With that monitor unplugged, the window should come back on a visible screen.
   - It works at 100/150/200 % display scaling.
8. **French** (Settings › General › Language › Français, then restart): menus, dialogs, settings, on-screen messages and the playlist all appear in French. "Follow Windows" picks French on a French Windows.

**Accessibility checklist** (the exit criterion; use Narrator with Ctrl+Win+Enter and a keyboard only):
- [ ] Tab reaches every control-bar button, the seek bar and the volume slider, with a visible focus ring.
- [ ] Seek bar: Left/Right seek, Home/End jump to the ends. Volume: arrow keys.
- [ ] Narrator reads names: "Play", "Seek", "Volume", "Position", "Video", "Status", every menu item, settings fields, the playlist.
- [ ] Every menu is reachable with Alt plus the underlined letter, and every action has a keyboard shortcut (Ctrl+K lists them).
- [ ] Dialogs (Settings, Shortcuts, Subtitle style, Diagnostics) work with Tab, Space and Enter, and Esc closes them.
- [ ] High contrast: see check 3.

**Exit criteria status:**
- [ ] UI stays responsive (input lag under 50 ms) during 4K playback. For the owner to test.
- [ ] Accessibility checklist passes. For the owner to test.
- [x] No PotPlayer assets or branding. The icon is drawn by `scripts/make_icon.py`, and the icons come from the Windows icon font.

## Phase 6: implemented (owner-tested)

From now on the owner does all testing; nothing in Phase 6 was run by the implementer. It compiles cleanly (`/W4 /WX` and the clang-tidy CI preset), and that is all that has been verified.

**What to check:**

1. **Settings window** (Tools › Settings, Ctrl+,):
   - All 13 categories open, and the search box filters rows and hides empty categories.
   - Apply, OK, Cancel and Restore defaults behave as expected.
   - Values survive a restart.
2. **Import/export** (General page): export, change something, import, and confirm the change is undone. Recent files and the playlist are never exported.
3. **Video picture:** brightness, contrast, saturation, gamma and hue. Deinterlacing on an interlaced file.
4. **Hardware decoding modes:** Diagnostics should show the matching decoder for each mode, and "Off" should show none.
5. **Rendering:**
   - each upscaler
   - tone mapping on an HDR file (`bench/media/hevc10_2160p60_hdr10.mkv`)
   - HDR peak detection, HDR passthrough (needs Windows HDR on), smooth motion
6. **Filters:**
   - sharpen (a GPU shader)
   - denoise (a CPU filter; watch CPU at 4K)
   - equalizer and its presets
   - normalization, downmix normalization
7. **Languages:** preferred audio/subtitle languages on `tests/media/multitrack.mkv`, and auto-loading an `.srt` next to a video.
8. **Performance and logging:** cache size, decoder threads (with hardware decoding Off), and the log level (the log grows at "Verbose").
9. **Profiles** (Video › Profiles): built-ins, save the current settings, delete. The checkmark should follow the active profile.
10. **Bookmarks** (Playback › Bookmarks, key B): add, jump, delete, delete all, and confirm they survive a restart.
11. **Diagnostics panel** (Tools › Diagnostics, Ctrl+I): values update while playing, and Copy works.
12. **File associations:**
    - Check some types, apply, and Y-Player should appear in Explorer's "Open with".
    - "Open Windows default apps" should open the Windows page.
    - Unchecking and applying should remove the entries.
13. **Playlist options:** "Remember playlist" off means an empty playlist after a restart. "Continue through the playlist" off means it stops at the end of a file.

**Exit criteria status:**
- [ ] With filters off, performance is unchanged. Unverifiable: there's no baseline, because Phases 3 and 4 were skipped.
- [ ] Settings persist, and import/export round-trips. For the owner to test.
- [ ] Diagnostics shows real values. For the owner to test.

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
