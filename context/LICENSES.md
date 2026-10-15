# Third-Party Licenses & Distribution Notes

Status: Phase 0 assessment. Check this again before every release (Phase 9).

## Components

| Component | Version in use | License | How we use it | Obligations |
|-----------|----------------|---------|---------------|-------------|
| **libmpv** | git `accb7b2821` (2026-10-11), client API 2.5 | **GPL-2.0-or-later** in the shinchiro build we use. mpv can be built LGPL-2.1+ with `-Dgpl=false` | Dynamic link (`libmpv-2.dll`) | GPL build: the whole app must be distributed under a GPL-compatible license, with source |
| **FFmpeg** (statically inside libmpv-2.dll) | bundled with the libmpv build | GPL (build uses `--enable-gpl` components) | Indirect, through libmpv | Same as above; list its configure flags in the release notices |
| **Qt 6** (Core, Gui, Widgets; Test for tests only) | 6.9.3 | LGPL-3.0 (open-source edition) | Dynamic link | Ship Qt as replaceable DLLs, include the license text, and provide or offer the Qt source |
| **MSVC runtime** | VS 2026 Build Tools (14.51) | Microsoft redistributable license | Redistributable DLLs | Ship the VC++ redist or app-local DLLs as Microsoft allows |
| **Qt Linguist tools** (lupdate/lrelease, `qttools`, dev tool only) | 6.9.3 | GPL-3.0 tools | Build-time translation compiler; **not shipped** (only the generated .qm data is) | None for the product |
| **Segoe UI Variable / Segoe Fluent Icons / Segoe MDL2 Assets** | system fonts | Microsoft, part of Windows | Used at runtime from the OS; **not bundled** | Don't redistribute the font files |
| **LLVM / clang-tidy** (dev tool only) | 23.1.3 | Apache-2.0 with LLVM exception | Static analysis in CI; **not shipped** | None for the product |
| **FFmpeg CLI** (dev tool only) | BtbN master build | GPL | Generates test media; **not shipped** | None for the product |

## Product license decision

See **ADR-005** in [`DECISIONS.md`](DECISIONS.md). The short version: Y-Player is licensed **GPL-3.0-or-later**. That is compatible with GPL-2.0+ libmpv/FFmpeg and with LGPL-3.0 Qt, and it lets us use prebuilt GPL libmpv. If closed-source distribution is ever needed, the way out is our own LGPL libmpv build (`-Dgpl=false`, LGPL FFmpeg). That loses some filters and decoders, so it is a deliberate decision, not a default.

## Codec patents

Distributing decoders for H.264, HEVC, AAC, AC-3/E-AC-3 and others may require patent licenses in some jurisdictions, regardless of the software license. Before any commercial distribution:
- Prefer OS-provided hardware decoders (D3D11VA) where possible.
- Get legal review of the bundled software decoders.

## Assets

No PotPlayer code, icons, or branding are used. All product assets will be original (Phase 7).
