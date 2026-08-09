# Implementation Plan: Blank Disk Creation & Mounting

**Branch**: `017-blank-disk-creation` | **Date**: 2026-08-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/017-blank-disk-creation/spec.md`

## Summary

Add first-class blank-disk creation: a `<Create new disk...>` row pinned first
in the insert-disk picker opens a themed create dialog that navigates like a
real file-save dialog (folder browsing + filename editing in-dialog) with
format (WOZ/DSK/PO), contents (DOS 3.3 / ProDOS 1.1.1 / unformatted), and a
bootable toggle whose OS payload arrives via the existing stock-disk download
catalog. The new image is written to disk and mounted through the existing
mount path. Adjacent scope: an image-level write-protect toggle (Disk menu,
per drive) — WOZ via its INFO flag, DSK/PO via the host read-only attribute.

Technical approach (research.md): all generation is pure core code —
`NibblizationLayer::Nibblize` over a skeleton sector buffer already yields
formatted tracks, and `WozLoader::Serialize` already writes valid WOZ v2 — so
the new core surface is the filesystem skeleton writers (DOS 3.3 VTOC/catalog,
ProDOS directory/bitmap, boot payload installers) plus a pure
`FileBrowseModel` behind the dialog. The shell adds the dialog view, the
pinned picker row, two menu items, and one persisted pref.

## Technical Context

**Language/Version**: C++20, MSVC v145 (VS 2026); Win32 + D3D11/D2D (Dxui)

**Primary Dependencies**: None new. Reuses `NibblizationLayer`, `WozLoader`,
`DiskImageStore`/`DiskManager` mount path, `AssetBootstrap` boot-disk catalog
(`s_kDos33Disk`, `s_kProDOSDisk`), Dxui widgets (`DxuiListView`,
`DxuiTextInput`, `DxuiDropdown`, `DxuiCheckbox`, `DxuiDialogWindow`).

**Storage**: Host files — created `.woz`/`.dsk`/`.po` images (user-chosen
folder, default `Documents\Casso Disks`); one new `GlobalUserPrefs` string
(`lastDiskCreateFolder`); downloaded OS masters cached in the existing
`%LOCALAPPDATA%\Casso\Disks`.

**Testing**: VS UnitTest (MSTest C++), existing suites as pattern:
`NibblizationTests`, `DiskWritePathTests`, `CatalogReproductionTest` (real DOS
boot + `SAVE` via `KeystrokeInjector`/`TextScreenScraper` — reused for SC-001/
SC-006 end-to-end gates).

**Target Platform**: Windows 10/11 x64 (+ ARM64 build-only)

**Project Type**: Desktop app — core library (`CassoEmuCore`) + Win32 shell
(`Casso`) + UI framework (`Dxui`) + `UnitTest`

**Performance Goals**: Create-and-mount well under 1s (SC-001's 30s budget is
user time); zero impact on emulation thread.

**Constraints**: Core/shell doctrine — generation and browse logic must be
UT-reachable (no window/file/registry deps in core logic; file access only
through `IFileSystem`). OS payloads never bundled (copyright) — consent-based
download only.

**Scale/Scope**: 140K 5.25" media only; 3 formats × 3 contents + bootable;
two drives; one new dialog, one new picker row, two menu items.

## Constitution Check

*Constitution v1.8.0. GATE evaluated pre-Phase-0 and re-checked post-design.*

- **I. Code Quality (NON-NEGOTIABLE)** — PASS (design-time): new code follows
  EHM single-exit, banners, decl-at-top, no anonymous namespaces; gated by
  CheckStyle CS0001–CS0019 pre-push + CI.
- **II. Testing Discipline (NON-NEGOTIABLE)** — PASS: every pure component
  (skeleton writers, payload installers, `FileBrowseModel`, WP state logic)
  ships its unit suite in the same phase; end-to-end gates reuse the
  DOS-boot/`SAVE` harness. No system state in tests — `IFileSystem` injected.
- **VI. Thin Executable, Testable Core (NON-NEGOTIABLE)** — PASS: FR-013
  restates it. Core owns: `BlankDiskBuilder`, `Dos33Skeleton`,
  `ProDosSkeleton`, `Dos33FileWriter` (HELLO), `ProDosFileWriter`,
  `ProDosReader` (payload extraction), `FileBrowseModel`. Shell owns only:
  dialog widgets/wiring, picker row decode, menu items, known-folder
  resolution, the actual file write + mount calls, attribute flip via the new
  `IFileSystem` seam.
- **Commit discipline** — commit + push after each completed phase.

**Post-design re-check**: PASS — no violations introduced; Complexity Tracking
empty.

## Project Structure

### Documentation (this feature)

```text
specs/017-blank-disk-creation/
├── plan.md              # This file
├── research.md          # Phase 0 (R-001..R-013)
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── blank-disk-builder.md
│   ├── file-browse-model.md
│   └── shell-integration.md
└── tasks.md             # Phase 2 (/speckit-tasks — not yet created)
```

### Source Code (repository root)

```text
CassoEmuCore/Devices/Disk/
├── BlankDiskBuilder.h/.cpp      # NEW — template → sector buffer / DiskImage / file bytes
├── Dos33Skeleton.h/.cpp         # NEW — VTOC + catalog chain (+ HELLO via Dos33FileWriter)
├── ProDosSkeleton.h/.cpp        # NEW — volume dir + bitmap (+ ProDosFileWriter, ProDosReader)
├── NibblizationLayer.h/.cpp     # existing — reused unchanged
├── WozLoader.h/.cpp             # existing — Serialize reused unchanged
├── DiskImageStore.h/.cpp        # + MountedSourcePaths() query (FR-018)
└── IDiskImage.h                 # existing WriteProtectInfo — unchanged

CassoEmuCore/Host/ (or existing IFileSystem home)
└── IFileSystem.h                # + read-only-attribute get/set seam (R-011)

CassoEmuCore/Ui/ (core-side, UT-reachable)
└── FileBrowseModel.h/.cpp       # NEW — folder listing/nav/validation/unique-name

Casso/
├── AssetBootstrap.h/.cpp        # sentinel picker row (R-009); DownloadStockBootDisk promoted public (R-008)
├── Ui/Dialogs/CreateDiskDialog.h/.cpp   # NEW — the themed save-style dialog (R-010)
├── Shell/DiskManager.cpp        # WP toggle apply/refresh path (R-011)
├── Shell/WindowCommandManager.cpp  # IDM_DISK_WP1/2 routing; create-flow orchestration
├── Ui/MainMenu.cpp              # Disk menu: Write-Protect Disk 1/2 (checkable)
└── Config/GlobalUserPrefs.h/.cpp  # + lastDiskCreateFolder (R-012)

UnitTest/EmuTests/
├── BlankDiskBuilderTests.cpp    # NEW — skeleton invariants, format matrix
├── ProDosVolumeTests.cpp        # NEW — reader/writer round-trip, bitmap/dir invariants
├── FileBrowseModelTests.cpp     # NEW — nav/validation/unique-name (mock IFileSystem)
└── (extend) DiskWritePathTests / CatalogReproductionTest — created-disk SAVE round-trip, boot gates
```

**Structure Decision**: Single solution, existing four-project split. All new
logic lands in `CassoEmuCore` per constitution VI; the shell gains one dialog
class and wiring only. No new projects, no new dependencies.

## Complexity Tracking

*No constitution violations — table intentionally empty.*
