# Implementation Plan: Screenshot capture modes, file output, and metadata

**Branch**: `claude/screenshot-save-format-location-7eee56` | **Date**: 2026-09-05 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/030-screenshot-capture/spec.md`

## Summary

The Screenshot command gains three capture modes (`scene` / `crt` / `raw`), writes a PNG
alongside the clipboard copy, and stamps that PNG with metadata that makes it
self-describing. Closes GH #132.

The technical approach turns on one fact established during research: **the CRT chain
runs on the GPU and its output is never read back**. Every `Map` in the tree is
`D3D11_MAP_WRITE_DISCARD` -- the codebase uploads to the GPU and has never once brought
pixels home. So the core of this feature is a new GPU-to-CPU readback path, and
everything else (naming, metadata, prefs, settings UI) is composition around it.

The second turning point is that the swap chain is `DXGI_SWAP_EFFECT_FLIP_DISCARD`, so
there is no readable already-rendered frame. The command drives a synchronous paint
through the existing `WM_PAINT` entry point, with the app-describing overlays hidden for
that one frame, and reads back before Present.

## Technical Context

**Language/Version**: C++ `/std:c++latest`, MSVC v145+

**Primary Dependencies**: Windows SDK only -- Direct3D 11 + DXGI (readback), WIC
(PNG + tEXt), Shell (`SHGetKnownFolderPath`, folder picker). In-tree `Dxui` for the
settings widgets. **No new third-party dependency**, so no constitution amendment.

**Storage**: User preferences JSON via the existing `UserConfigStore` / `GlobalUserPrefs`
global section; PNG files under `<Pictures>\Casso Screenshots`.

**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New pure logic lands in
`CassoEmuCore`, which `UnitTest` already links.

**Target Platform**: Windows 10/11, x64 and ARM64.

**Project Type**: Desktop GUI application over static core libraries
(`CassoCore`, `CassoEmuCore`, `Dxui`).

**Performance Goals**: A capture costs at most one additional rendered frame. The
emulated machine never pauses. The readback stall is bounded by one full-window texture
copy and map, on a user action taken at human frequency.

**Constraints**: No per-frame cost may be added to the render loop -- the readback
resources are created on demand at capture time and released, never maintained. No
retained copy of the back buffer.

**Scale/Scope**: ~18 source files across 4 projects, one preferences schema addition,
nine new or extended test files.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

### VI. Thin Executable, Testable Core (NON-NEGOTIABLE) -- the governing gate

This principle was amended (1.9.0 → 1.10.0) precisely to close the exemption this feature
would otherwise claim. "It calls Win32" is explicitly not a reason to live in the exe;
the only question is whether `UnitTest` can drive it. A clipboard round-trip and an image
codec over WIC are named in the principle text as things that belong in core.

**What the principle permits to stay in `Casso.exe`**: the `HWND`, its message pump, and
the graphics device objects. The readback itself -- `ID3D11DeviceContext`,
`CopySubresourceRegion`, `Map` -- is device-object work and qualifies.

**What must not stay there**: every decision. Which source a mode reads from, which
rectangle, which overlays to hide, whether to write a file, where, under what name, and
which metadata entries that mode emits. All of it is data-in / data-out and all of it is
where the defects will be.

**Resolution**: a pure `ScreenshotPlan` resolver in `CassoEmuCore`. It takes the mode,
the preferences, the window and viewport geometry, and whether a desk scene is active;
it returns the source selection, the source rectangle, the overlay hide-list, the output
path and whether the folder needs creating. A second pure function,
`CaptureOutcome::DescribeResult`, turns what happened into the notice text. The exe
executes against D3D and the filesystem and chooses neither a path nor a word. This makes
the per-mode contract and every edge case in the spec testable without an `HWND`.

The three places this could quietly slip back across the line, and where it must not:

- The **default destination** is composed by the resolver from an injected Pictures
  folder. Only `SHGetKnownFolderPath` itself is the exe's.
- **Failure reporting** is `DescribeResult`'s output, not wording chosen at the call site.
- **Creating a folder that has been deleted** is `folderMustBeCreated` on the plan, so it
  is policy rather than a rescue path in the shell.

**PASS**, with two recorded tensions in Complexity Tracking.

### II. Testing Discipline

Test Isolation is NON-NEGOTIABLE and forbids tests touching the real filesystem or
calling `SHGetKnownFolderPath`. The resolver therefore takes the Pictures folder as an
input rather than discovering it, and collision checking stays behind the injected
`taken` predicate that `PrintFileNaming` already uses. The existing `IFileSystem` seam
covers the preferences round-trip. **PASS**.

### III. User Experience Consistency

Backward compatibility applies: renaming the Printing settings tab changes something a
user navigates by. FR-031 keeps the Printing section first on the page so the existing
route still lands on printer settings. Screenshot's toolbar button, menu item and
shortcut are unchanged in form and count. No CLI surface is touched, so no `--help`
change. **PASS**.

### IV. Performance Requirements

One extra frame per capture, no per-frame cost, readback resources created on demand and
released. The extra frame is visually free: with identical input the persistence pass
combines via `max()`. **PASS**.

### V. Simplicity & Maintainability

YAGNI is doing real work here, and the rejections are recorded in research.md so they are
not re-litigated: no JPEG, no format setting, no `pHYs` aspect hint, no per-capture mode
override, no Edit submenu, no retained back-buffer copy. Two existing components are
generalized rather than duplicated (`PrintFileNaming`, `PngCodec`). **PASS**.

### I. Code Quality

Standard: EHM on every failable path, single exit via `Error:`, declarations at top of
scope, class statics not free functions, function comments in `.cpp` only. The readback
path is COM- and resource-heavy and is exactly where the single-exit discipline earns its
keep -- a mapped staging texture must be unmapped on every path. **PASS**.

## Project Structure

### Documentation (this feature)

```text
specs/030-screenshot-capture/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── screenshot-metadata.md
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks, NOT created here)
```

### Source Code (repository root)

```text
CassoEmuCore/                        # static library; UnitTest links it
└── Devices/Printer/
    ├── PngCodec.h/.cpp              # EXTEND: optional tEXt chunks on EncodeRgba
    ├── PngMetadata.h                # NEW: MetadataEntry, beside its codec consumer
    └── PrintFileNaming.h/.cpp       # GENERALIZE: base name + extension parameters

CassoEmuCore/Capture/                # NEW directory: the pure decision layer
├── ScreenshotMode.h                 # the scene/crt/raw token + parse/format
├── ScreenshotPlan.h/.cpp            # mode + geometry + prefs -> what to capture & write
├── CaptureOutcome.h/.cpp            # what happened -> the notice text (pure)
└── ScreenshotMetadata.h/.cpp        # facts -> the contract's entry set, per mode

Casso/                               # the executable: devices, HWND, pump
├── Config/GlobalUserPrefs.h/.cpp    # EXTEND: 3 global keys + JSON round-trip
├── Shell/ClipboardManager.h/.cpp    # CHANGE: accept a captured image, not the framebuffer
├── Shell/ScreenshotCapture.h/.cpp   # NEW: executes a ScreenshotPlan against D3D + disk
├── Shell/WindowCommandManager.h/.cpp# CHANGE: command routing, print call site, folder picker
├── D3DRenderer.h/.cpp               # NEW: readback of the offscreen target / back buffer
├── EmulatorShell.h/.cpp             # CHANGE: capture frame, overlay hide/restore, notice
└── Ui/Settings/
    ├── PrintingPage.h/.cpp          # EXTEND: retitle, add the Screenshots section
    └── SettingsSheet.cpp            # CHANGE: tab title

Dxui/Widgets/DxuiRadio.h/.cpp        # EXTEND: per-option description line

UnitTest/
├── PrinterTests/PrintFileNamingTests.cpp    # EXTEND: base + extension cases
├── PrinterTests/PngCodecTests.cpp           # EXTEND: tEXt round-trip
├── CaptureTests/ScreenshotModeTests.cpp     # NEW
├── CaptureTests/ScreenshotPlanTests.cpp     # NEW
├── CaptureTests/CaptureOutcomeTests.cpp     # NEW
├── CaptureTests/ScreenshotMetadataTests.cpp # NEW
├── UiTests/GlobalUserPrefsTests.cpp         # EXTEND: 3 keys + passthrough
├── UiTests/DxuiRadioGroupTests.cpp          # EXTEND: per-option description
└── UiTests/PrintingPageTests.cpp            # NEW: the Screenshots section
```

**Layering direction**: `Capture/` consumes the generalized printer components
(`PngCodec`, `PngMetadata`, `PrintFileNaming`), never the reverse. `MetadataEntry` sits
beside `PngCodec` for that reason -- defining it under `Capture/` would make the printer
depend on the screenshot directory.

**Structure Decision**: The decision layer is new and goes in `CassoEmuCore/Capture/`, a
real static library that `UnitTest` links directly -- not into `Casso/`, whose sources
`UnitTest` can only reach by compiling individual `.cpp` files into the test DLL. That
workaround exists for `GlobalUserPrefs.cpp` and `ClipboardManager.cpp` today and is the
GH #85 debt the constitution's Principle VI tells us not to imitate. The execution layer
(`ScreenshotCapture`, the `D3DRenderer` readback) stays in `Casso/` because it is device
objects and the paint pump, which Principle VI explicitly leaves in the exe.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Three new preference fields land in `Casso/Config/GlobalUserPrefs`, inside the exe, reachable by `UnitTest` only because its `.cpp` is compiled into the test DLL | `GlobalUserPrefs` is one struct with one JSON round-trip. Splitting three fields into a core-resident sibling would give the feature a second preferences file and a second schema version | Relocating `GlobalUserPrefs` wholesale into core is the right fix and is GH #85's scope. Doing it inside this feature would multiply the diff several times over and put an unrelated, higher-risk refactor on the critical path of a screenshot change |
| `ScreenshotCapture` in the exe orchestrates readback, encode and write | It holds the D3D context and the `HWND`, which Principle VI leaves in the exe | The orchestration itself decides nothing: every choice it acts on comes from a `ScreenshotPlan` resolved in core. Moving the D3D calls into core would require abstracting `ID3D11DeviceContext` behind an interface for no testing gain -- the untestable part is the GPU, not the code around it |
