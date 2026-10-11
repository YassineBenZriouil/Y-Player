# MASTER PROMPT — Build the Ultimate High-Performance Windows Video Player

## 1. YOUR MISSION

Act as a principal desktop software architect, senior C++ engineer, multimedia systems engineer, GPU performance specialist, UI/UX designer, and QA engineer.

Build a production-quality Windows desktop video player inspired by PotPlayer. The goal is to deliver the playback performance, codec support, customization, precision controls, and advanced functionality of PotPlayer, with an original interface and architecture.

This must be a real, usable desktop application—not a prototype, mockup, or collection of nonfunctional controls.

**Primary objectives, in order:**
1. Maximum playback smoothness and reliability.
2. Minimum CPU, GPU, RAM, and power consumption for equivalent playback quality.
3. Broad format and codec compatibility.
4. Fast startup, seeking, track switching, and recovery.
5. A powerful, customizable user experience.
6. Long-term maintainability, extensibility, and testability.

Do not sacrifice playback correctness or stability for misleading benchmark improvements.

## 2. OPERATING PRINCIPLES

- Inspect the development environment before making architectural decisions.
- Check the existing repository before creating, replacing, or deleting files.
- Preserve useful existing code and user data.
- Make decisions independently when requirements are sufficiently clear.
- Do not stop at planning; implement, build, run, test, and debug the application.
- Work incrementally, keeping the application buildable at each milestone.
- Never pretend a feature works when it is only a placeholder.
- Never claim performance improvements without measurements.
- Document limitations, trade-offs, and unsupported features honestly.
- Avoid unnecessary dependencies, abstractions, background services, and network requests.
- Keep the application usable offline for local media playback.
- Do not copy PotPlayer's proprietary code, branding, icons, or distinctive assets.
- Use PotPlayer as a functional reference, while creating an original product identity.

## 3. TECHNOLOGY SELECTION

Evaluate technologies based on measured performance, compatibility, maturity, and development cost.

Preferred starting architecture:

- **Platform:** Windows 10/11, x64, with ARM64 support considered separately.
- **Language:** C++20 or newer where supported.
- **Media engine:** libmpv as the initial candidate.
- **Alternative media backend:** FFmpeg libraries when a custom playback pipeline is justified by measurable requirements.
- **Rendering:** D3D11/D3D12 where appropriate, or the rendering path supported efficiently by the selected media engine.
- **Hardware decoding:** Windows-supported hardware acceleration through the media backend.
- **UI:** Qt 6 Widgets or another proven desktop framework with minimal overhead.
- **Build system:** CMake.
- **Packaging:** A reproducible Windows installer and portable build where feasible.
- **Testing:** Automated unit, integration, playback, and performance tests.

Do not automatically choose every technology in this list. Evaluate the actual capabilities, licensing, integration complexity, and performance of each candidate.

Prefer a mature, highly optimized media engine over a custom decoder implementation. Do not rewrite FFmpeg, libmpv, or other established multimedia components without compelling benchmark evidence.

Keep the playback engine isolated behind a clear interface so alternative backends can be evaluated later.

## 4. ARCHITECTURE

Use a modular architecture with clearly defined ownership and communication boundaries.

Suggested modules:

- Application lifecycle and configuration.
- Playback engine abstraction.
- Media session and playback state.
- Hardware decoding and rendering configuration.
- Audio output and device selection.
- Subtitle loading, rendering, and synchronization.
- Playlist and media library.
- Input handling and shortcut mapping.
- Video presentation and playback controls.
- Settings and profiles.
- Logging, diagnostics, and crash reporting.
- Performance instrumentation.
- Update and packaging infrastructure.

Keep the UI responsive while media operations execute independently.

Establish explicit ownership and lifetime rules for media objects, decoded frames, audio buffers, and GPU resources. Avoid data races, dangling references, deadlocks, and unnecessary synchronization.

Do not split the application into microservices. A well-designed native desktop application should remain simple to launch, install, debug, and maintain.

## 5. PERFORMANCE ENGINEERING

Treat performance as a measurable engineering requirement, not a marketing claim.

### 5.1 Hardware-accelerated decoding

- Detect available decoding hardware and supported codecs.
- Prefer the most efficient compatible hardware decoder.
- Investigate D3D11VA, DXVA2, and other supported paths as appropriate.
- Account for codec profiles, bit depth, chroma subsampling, resolution, and GPU capabilities.
- Support hardware decoding for common codecs such as H.264, HEVC, AV1, and VP9 where supported by the device and backend.
- Use software decoding when hardware decoding is unavailable, unsupported, unstable, or slower for the specific workload.
- Provide configurable hardware-decoding options and a reliable fallback.
- Report the actual active decoder rather than merely displaying the requested setting.
- Handle device loss, driver failures, unsupported formats, and decoder initialization errors gracefully.

Never assume every GPU supports every codec, profile, or bit depth.

### 5.2 Zero-copy and memory transfers

- Investigate zero-copy frame presentation and GPU-native decoding surfaces.
- Avoid unnecessary GPU-to-CPU-to-GPU transfers.
- Avoid copying decoded frames when ownership transfer or shared surfaces are possible.
- Reuse frame buffers and memory pools when appropriate.
- Ensure synchronization between decoding, rendering, and GPU resource lifetimes.
- Avoid excessive texture allocation and destruction during playback.
- Measure memory bandwidth and frame-transfer overhead.
- Use fallback copies when required for compatibility or correctness.

Zero-copy must be verified for each actual pipeline; do not assume it is automatically enabled.

### 5.3 Rendering performance

- Keep rendering independent of slow UI operations.
- Avoid unnecessary frame redraws when paused.
- Minimize texture uploads, CPU-side image conversion, and redundant color-space transformations.
- Use appropriate GPU-native formats and supported conversion paths.
- Synchronize presentation with the display refresh rate where appropriate.
- Avoid unnecessary intermediate render targets.
- Handle window resizing, fullscreen transitions, multi-monitor setups, and display changes efficiently.
- Investigate HDR output, color management, and high-refresh-rate presentation.
- Avoid excessive vsync buffering and presentation queues where they introduce measurable latency.
- Prevent rendering stalls from subtitle layout, overlays, menus, or UI animations.

Do not disable synchronization blindly; prioritize correct timing and smooth presentation.

### 5.4 Multithreading

- Keep decoding, demuxing, audio output, rendering, and UI responsibilities appropriately separated.
- Use the media engine's internal threading capabilities before implementing custom parallelism.
- Avoid excessive thread counts, busy loops, spin waiting, and unnecessary context switches.
- Never block the UI thread on long-running decoding, seeking, disk operations, or network access.
- Use thread-safe queues with bounded memory usage where appropriate.
- Establish explicit cancellation and shutdown behavior.
- Avoid priority escalation that starves the operating system or other applications.
- Tune thread counts based on measured results rather than assuming more threads are faster.

### 5.5 Buffering and frame scheduling

- Maintain bounded audio and video queues.
- Prevent unbounded memory growth during slow decoding or network stalls.
- Keep audio/video synchronization accurate.
- Handle variable-frame-rate media and changing frame durations.
- Minimize avoidable startup buffering without causing excessive frame drops.
- Recover gracefully when decoding cannot keep up with playback.
- Make buffering configurable for network playback and high-bitrate files.
- Keep playback speed changes and seeking consistent with timestamps.
- Correctly handle end-of-file, discontinuities, missing timestamps, and damaged streams.

### 5.6 CPU, GPU, and memory efficiency

- Avoid unnecessary polling and repeated state queries.
- Do not continuously scan directories in the background without user consent.
- Avoid loading entire media files into RAM.
- Use streaming and bounded caches for large files.
- Reuse objects and buffers only where doing so measurably helps.
- Avoid excessive logging during normal playback.
- Release GPU resources and decoder contexts promptly when no longer needed.
- Stop unnecessary background activity when idle.
- Measure idle resource consumption separately from active playback.
- Investigate CPU wakeups and power consumption on laptops.
- Ensure long playback sessions do not cause memory growth or performance degradation.

## 6. PERFORMANCE BENCHMARKING

Create a reproducible benchmark suite before attempting aggressive optimizations.

Record a baseline using the initial functional implementation, then measure each meaningful optimization against it.

Measure:

- Application startup time.
- Time from opening a file to the first presented frame.
- Seeking latency at multiple positions.
- CPU utilization during steady-state playback.
- GPU utilization and dedicated/shared GPU memory.
- Process working set and private memory.
- Dropped, repeated, and late-presented frames.
- Audio underruns and synchronization errors.
- Frame-presentation timing and jitter.
- Time required to open large files.
- Recovery from decoder and rendering errors.
- Idle CPU use and power consumption.
- Stability during long-duration playback.

Use representative files covering:

- H.264 and HEVC.
- AV1 and VP9.
- 8-bit and 10-bit video.
- 1080p, 1440p, 4K, and supported 8K workloads.
- High-bitrate local files.
- Variable-frame-rate media.
- HDR and SDR content.
- High-frame-rate media.
- Multiple audio and subtitle tracks.
- Damaged or unusual media files.
- Network streams, if network playback is implemented.

Compare against PotPlayer using identical hardware, media files, output settings, and comparable rendering configurations wherever practical.

Repeat tests, record the environment, and report measurement variability.

Do not compare unrelated files or settings and present the results as a fair benchmark.

Define performance targets after measuring a realistic baseline. Set separate targets for different hardware classes and media workloads.

## 7. ADAPTIVE PLAYBACK CONFIGURATION

Implement an explainable playback configuration system.

At startup or when necessary, detect:

- CPU architecture and relevant capabilities.
- GPU and driver information.
- Supported hardware decoding paths.
- Display resolution and refresh rate.
- HDR capabilities when available.
- Available audio devices.
- Relevant operating-system capabilities.

Select safe, efficient defaults for the current device and media.

Prefer per-session decisions when different media files require different decoding paths.

Allow users to override automatic choices.

If a selected decoder fails, attempt a safe fallback when possible, preserving playback position and settings.

Never silently change a user's preferences permanently.

Provide a diagnostics panel showing the active decoder, renderer, codec, video dimensions, frame rate, bitrate when available, dropped-frame statistics, and relevant output configuration.

## 8. PLAYBACK FEATURES

Implement real functionality for:

- Play, pause, stop, and resume.
- Seek bar with accurate position and duration.
- Frame-by-frame forward and backward navigation where supported.
- Configurable playback speeds, including fine-grained adjustments.
- Playlist management.
- Repeat-one, repeat-all, shuffle, and sequential playback.
- Previous and next media.
- A-B repeat.
- Audio delay adjustment.
- Subtitle delay adjustment.
- Audio track selection.
- Subtitle track selection.
- External subtitle loading.
- Subtitle synchronization and styling.
- Aspect ratio controls.
- Video rotation and flipping.
- Fullscreen and borderless playback.
- Screenshot capture.
- Volume control and mute.
- Audio output device selection.
- Audio channel configuration where supported.
- Window resizing and always-on-top mode.
- Resume playback from the previous position, with user-configurable behavior.
- Recent files.
- Drag-and-drop file opening.
- Command-line file opening and file associations where appropriate.

Ensure all playback actions work with real media.

## 9. ADVANCED PLAYBACK FEATURES

Add advanced features in prioritized stages:

- Configurable video and audio filters.
- Deinterlacing.
- Color, brightness, contrast, and saturation adjustments.
- Sharpening and scaling filters.
- Denoising where appropriate.
- Audio equalization.
- Audio normalization and channel mixing where supported.
- HDR-to-SDR tone mapping when needed.
- Subtitle rendering and advanced styling.
- External audio and subtitle files.
- Chapter navigation.
- Playback statistics and technical media information.
- Bookmarks.
- Custom aspect ratios.
- Configurable seek intervals.
- Playlist persistence.
- Multiple profiles for playback and rendering preferences.
- Advanced synchronization and presentation settings.

Filters must be opt-in or use sensible defaults. Avoid expensive processing when the user has not enabled it.

Explain when filters increase GPU load, CPU usage, latency, or power consumption.

## 10. FORMAT COMPATIBILITY

Use a mature media backend to support a broad range of containers, codecs, and media formats.

Evaluate support for:

- MP4, MKV, WebM, MOV, AVI, MPEG-TS, MPEG-PS, and common legacy containers.
- H.264, HEVC, AV1, VP9, VP8, MPEG-2, and other backend-supported codecs.
- AAC, MP3, FLAC, Opus, Vorbis, PCM, AC-3, E-AC-3, and other supported audio codecs.
- Common bitmap and text subtitle formats.
- Embedded and external subtitles.
- Multiple audio and subtitle tracks.
- Chapters, metadata, and supported attachments.

Do not promise universal compatibility. Actual support depends on the media backend, build configuration, codecs, licensing, and platform capabilities.

Test representative files rather than relying exclusively on format names or file extensions.

## 11. USER INTERFACE AND DESIGN

Create an original, modern interface inspired by the functionality and configurability of PotPlayer.

The visual experience should feel fast, polished, precise, and unobtrusive.

### Main player

- Video occupies the primary viewing area.
- Playback controls can auto-hide during viewing.
- Controls appear on mouse movement or user interaction.
- Support fullscreen and windowed playback.
- Keep controls accessible without distracting from the video.
- Provide a responsive seek bar with hover previews if implemented efficiently.
- Display transient feedback for volume, seeking, playback speed, and track changes.
- Avoid unnecessary animations, heavy visual effects, and excessive overlays.

### Context menus

Provide practical right-click menus for:

- Playback.
- Video.
- Audio.
- Subtitles.
- Aspect ratio.
- Filters.
- Playback speed.
- Tracks.
- Screenshots.
- Window behavior.
- Advanced settings.

Menus must contain functional actions, not decorative entries.

### Settings

Create a searchable settings window with logical categories:

- General.
- Playback.
- Video.
- Hardware acceleration.
- Rendering.
- Audio.
- Subtitles.
- Filters.
- Keyboard shortcuts.
- Playlist.
- File associations.
- Performance.
- Advanced diagnostics.

Support sensible defaults, reset options, and import/export of configuration where practical.

Avoid overwhelming beginners while preserving access to advanced settings.

### Original identity

Develop an original application name, icon, typography, colors, layout, and visual language.

Do not reproduce PotPlayer's branding or assets.

The interface should be usable with a mouse and keyboard and should remain responsive during playback.

## 12. KEYBOARD SHORTCUTS

Provide a customizable shortcut system.

Include defaults for common operations such as:

- Space: play/pause.
- Left/right arrows: seek backward/forward.
- Up/down arrows: volume.
- F: fullscreen.
- M: mute.
- S: subtitle controls.
- A: audio track controls.
- O: open file.
- Enter: fullscreen or context-appropriate confirmation.
- Esc: exit fullscreen or close the active overlay.
- Frame-step shortcuts.
- Fine-grained playback-speed adjustment.
- Screenshot capture.
- Playlist navigation.

These are proposed defaults, not mandatory choices when conflicts arise.

Show shortcuts in menus where useful, support rebinding, and prevent accidental duplicate bindings.

Avoid intercepting system-wide shortcuts without explicit user consent.

## 13. AUDIO ENGINEERING

Treat audio as a first-class subsystem.

- Maintain accurate audio/video synchronization.
- Support common audio output devices.
- Handle device disconnection and reconnection.
- Support volume and mute controls.
- Implement audio delay correction.
- Support track selection and channel configuration where available.
- Avoid unnecessary resampling or conversion.
- Preserve high-quality output when the chosen device and backend support it.
- Make output format and resampling decisions explicit.
- Prevent audio glitches during seeking, track changes, device switches, and playback-speed changes.
- Release audio resources correctly during shutdown.

Do not claim bit-perfect output unless it has been specifically verified for the selected audio path.

## 14. SUBTITLE ENGINEERING

- Detect embedded subtitle tracks.
- Support external subtitle files.
- Support track selection, delay, and synchronization.
- Provide configurable font, size, color, outline, and position.
- Handle Unicode and international text.
- Support right-to-left text where the rendering backend permits.
- Support appropriate styling and timing for supported subtitle formats.
- Prevent subtitle parsing and rendering from blocking playback.
- Handle missing fonts and malformed subtitle files gracefully.
- Avoid excessive per-frame allocations during subtitle rendering.

Subtitle support must not create unnecessary frame drops or memory leaks.

## 15. FILE HANDLING AND PLAYLISTS

- Open files through a file picker, drag-and-drop, playlists, and supported command-line arguments.
- Validate paths and handle unusual filenames.
- Handle files being deleted, renamed, moved, or becoming inaccessible.
- Support large playlists without excessive memory usage.
- Avoid blocking the UI during metadata extraction.
- Keep metadata extraction bounded and cancellable.
- Support common playlist formats where practical.
- Persist user preferences and playlist state safely.
- Avoid recursive filesystem scanning unless explicitly requested.
- Handle Unicode paths correctly.
- Do not automatically upload local filenames or playback history.

For network streams, add configurable buffering, timeout handling, and graceful recovery separately from local playback.

## 16. RELIABILITY AND ERROR RECOVERY

The application must handle:

- Corrupted media.
- Unsupported codecs.
- Decoder initialization failures.
- Hardware decoder failures.
- GPU device loss.
- Audio-device changes.
- Missing external subtitle files.
- Unavailable network streams.
- Permission errors.
- Disk read failures.
- Rapid file switching.
- Repeated seeking.
- Window resizing during playback.
- Fullscreen transitions.
- Suspend and resume.
- Application shutdown during active playback.

Errors should be understandable to users and useful to developers.

Provide a safe software-decoding fallback when appropriate.

Never leave deadlocked threads, orphaned processes, leaked GPU resources, or indefinitely spinning background tasks after a failure.

## 17. SECURITY AND PRIVACY

Treat media files and embedded metadata as untrusted input.

- Use maintained dependencies.
- Track security advisories and update vulnerable components.
- Avoid unnecessary network access.
- Do not execute embedded scripts or arbitrary content from media files.
- Avoid unsafe shell command construction.
- Validate paths and external resource references.
- Prevent path traversal in archive or subtitle handling.
- Use safe update and installer practices.
- Do not run with administrator privileges unnecessarily.
- Do not collect analytics without clear consent.
- Keep local playback history and configuration on the device by default.
- Document the media libraries and their licenses.
- Review distribution requirements for FFmpeg, libmpv, Qt, and bundled codecs.

## 18. TESTING STRATEGY

Create automated tests covering the most failure-prone components.

### Unit tests

Test:

- Playback-state transitions.
- Time and duration calculations.
- Playlist operations.
- Configuration parsing.
- Shortcut mapping.
- Subtitle delay calculations.
- Resume-position handling.
- Error-state transitions.

### Integration tests

Test:

- Opening and closing media.
- Playback and pause.
- Seeking and frame stepping.
- Track switching.
- Hardware-decoding initialization.
- Software-decoding fallback.
- Audio-device switching.
- Subtitle loading.
- Fullscreen transitions.
- Playback recovery.
- Application shutdown.

### Performance tests

Test representative media at multiple resolutions and codec profiles.

Record baseline metrics and flag meaningful regressions.

### Stability tests

- Run extended playback sessions.
- Repeatedly open and close files.
- Perform repeated seeks.
- Switch tracks while playing.
- Resize and minimize the window.
- Exercise failure and recovery paths.
- Monitor RAM and GPU-resource usage.
- Verify that resources are released after playback ends.

Tests should distinguish unsupported test environments from genuine application failures.

## 19. DEVELOPMENT WORKFLOW

Follow this workflow:

### Phase 1 — Inspect and plan

1. Inspect the operating system, compiler, build tools, and installed dependencies.
2. Inspect the repository and identify existing code.
3. Research the selected media engine's current capabilities and integration requirements.
4. Compare relevant backend and UI alternatives.
5. Document architecture decisions and licensing constraints.
6. Define initial performance benchmarks.
7. Create a phased implementation plan.

### Phase 2 — Build the minimum functional player

1. Create the build configuration.
2. Integrate the chosen media backend.
3. Build a working application window.
4. Open a local media file.
5. Implement play, pause, stop, and seeking.
6. Render video and play audio.
7. Display basic playback information.
8. Add clean shutdown and resource management.
9. Build and test on the target Windows environment.

At the end of this phase, the application must play real files reliably.

### Phase 3 — Establish performance baselines

1. Record startup and first-frame latency.
2. Measure CPU, GPU, and memory usage.
3. Measure dropped frames and synchronization.
4. Test supported hardware-decoding paths.
5. Compare software and hardware decoding.
6. Identify the largest measured bottlenecks.
7. Record reproducible results.

### Phase 4 — Optimize the media pipeline

1. Verify the active decoding path.
2. Investigate frame-copy and conversion overhead.
3. Inspect rendering and presentation timing.
4. Tune thread counts and buffering.
5. Reduce unnecessary allocations and background activity.
6. Fix audio/video synchronization issues.
7. Test each optimization against the baseline.
8. Reject changes that reduce reliability or do not provide a meaningful benefit.

### Phase 5 — Build the complete playback experience

Implement playlists, subtitles, audio controls, advanced seeking, keyboard shortcuts, context menus, filters, screenshots, and configurable settings.

Test each feature against real media and ensure that it integrates correctly with the playback engine.

### Phase 6 — Polish and harden

Add diagnostics, crash handling, long-session testing, accessibility improvements, localization readiness, and performance-regression tests.

### Phase 7 — Package and release

Create a reproducible release build, installer, portable distribution if feasible, dependency notices, user documentation, and a release checklist.

Verify the application on a clean Windows environment.

## 20. PROJECT DOCUMENTATION

Maintain these documents in the repository:

- `README.md` — project overview, requirements, setup, build, and usage.
- `ARCHITECTURE.md` — component boundaries and data flow.
- `ROADMAP.md` — milestones and implementation status.
- `PERFORMANCE.md` — benchmark methodology, results, and known bottlenecks.
- `TESTING.md` — test procedures and supported environments.
- `DECISIONS.md` — important architecture and dependency decisions.
- `LICENSES.md` — third-party licensing and distribution notes.
- `CONTRIBUTING.md` — development conventions, if the project is intended for contributors.

Keep documentation synchronized with the actual implementation.

## 21. CODE QUALITY REQUIREMENTS

- Use clear, consistent naming.
- Prefer RAII and deterministic resource cleanup.
- Use modern C++ ownership practices.
- Avoid unnecessary raw-pointer ownership.
- Separate business logic from presentation.
- Keep public interfaces small and explicit.
- Avoid giant source files and monolithic classes.
- Avoid unnecessary template complexity.
- Use compiler warnings and static analysis.
- Keep dependencies and build steps documented.
- Handle errors explicitly.
- Use structured logging with appropriate verbosity.
- Avoid swallowing exceptions or errors silently.
- Ensure release builds are optimized and debug builds remain diagnosable.
- Keep code easy for future AI coding agents and human developers to understand.

Do not perform premature micro-optimization. First measure, then optimize the bottleneck, then benchmark again.

## 22. DEFINITION OF DONE

The application is not complete merely because the window opens.

A milestone is complete only when:

- The application builds successfully.
- The implemented feature works with real media.
- Relevant automated tests pass.
- Resource ownership and shutdown behavior are correct.
- Errors are handled without crashing the application unnecessarily.
- Performance has been measured when relevant.
- Documentation reflects the actual state.
- No fake controls or falsely advertised features remain.

The initial release should prioritize reliable playback and core functionality over an enormous but unstable feature set.

## 23. FINAL INSTRUCTIONS

Start by inspecting the repository and development environment. Then produce a concise architecture decision record and implementation roadmap, and immediately begin building the first working version.

Do not spend the entire task writing documentation or discussing possibilities. Implement the application in small, verifiable increments.

Use profiling to guide performance decisions. Compare alternative decoding and rendering paths when the results could materially affect playback performance.

Do not assume the application can outperform PotPlayer in every scenario. Treat that as a benchmark-driven goal and identify where it wins, loses, and why.

**The ultimate goal is a native Windows video player that combines PotPlayer-level depth with excellent playback smoothness, low overhead, reliable hardware acceleration, precise controls, extensive customization, and maintainable engineering.**