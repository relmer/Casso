# Implementation Plan: Native DX Dialogs Completion

**Branch**: `011-native-dialogs-completion` | **Date**: 2026-05-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/011-native-dialogs-completion/spec.md`

## Summary

Complete the spec-007 UI overhaul by converting every remaining Win32 UI surface in
`Casso/` (asset-bootstrap modals, Help/About/Keymap/Machine-Info MessageBoxes, the
`SettingsPanel` stray MessageBox, the legacy `GetOpenFileName` disk-insert path, the
drive widget label, the `DebugConsole` EDIT control, and the full `DiskIIDebugDialog`)
into themed DX overlays that match the chrome introduced in spec 007. The only
deliberately surviving Win32 surfaces are `IFileOpenDialog` in
`WindowCommandManager::PromptForDiskImage` and the `MessageBoxW` last-resort path
inside the EHM `SetNotifyFunction` callback in `Main.cpp`.

The approach extracts the modal-overlay plumbing already proven in
`Casso/Ui/Settings/SettingsWindow.cpp` into a reusable dialog primitive under
`Casso/Ui/Dialog/`, then incrementally retargets each consumer at it. Work is
sequenced in three phases matching the spec's priority labels so P1 (unified
startup download + boot-disk MRU picker) can ship independently of P2 (Help/About
+ drive widget label + file-open dedup) and P3 (Debug Console + Disk II Debug
Dialog).

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145, VS 2026)
**Primary Dependencies**: Windows SDK, Direct3D 11, Direct2D, DirectWrite, WIC, STL
  — plus existing in-tree `DxUiPainter`, `ChromeTheme`, `SettingsWindow`,
  `SettingsPanel`, `GlobalUserPrefs`, `DriveWidget`, `DriveWidgetController`
**Storage**: JSON-backed user-prefs file managed by `GlobalUserPrefs`
  (`Casso/Shell/GlobalUserPrefs.cpp`) — new `recentDisks` array key for MRU
**Testing**: Microsoft Native C++ Unit Test Framework (`UnitTest/` project) —
  headless unit tests only; no Win32, no real file I/O
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Desktop application (Win32 GUI, native DX chrome)
**Performance Goals**: Dialog open / repaint within one frame at the active
  refresh rate; MRU prune on render MUST NOT block the UI thread on slow
  file-system calls (network paths in particular)
**Constraints**:
  - All system includes via `Pch.h` only; quoted includes for project headers
  - EHM pattern (`HRESULT hr = S_OK;` + single `Error:` exit, asserting `*A`
    variants by default; non-asserting variants only for genuinely
    user/external failure modes — e.g. missing MRU file, missing ROM
    download, `ShellExecuteW` on a system with no default browser)
  - 5 blank lines between top-level constructs; 3 blank lines between the
    variable-declaration block and the first statement; column-aligned
    declarations; cast spacing; `s_<typePrefix><Name>` for file-scope statics
  - Themes: DarkModern, Skeuomorphic, GreenScreen
  - DPI scales: 100%, 125%, 150%, 200%
**Scale/Scope**:
  - 1 new reusable primitive (`Casso/Ui/Dialog/`)
  - ~6 dialog consumers retargeted (unified-startup, boot-disk picker,
    About, Keymap, Machine Info, SettingsPanel stray)
  - 2 heavy full-window conversions (Debug Console, Disk II Debug Dialog)
  - 1 new persisted user-prefs field (MRU array, cap 16)
  - 1 `DriveWidgetState` extension (`imagePath` / `imageName`)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle                                | Status | Notes                                                                                                                          |
| ---------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------ |
| I. Code Quality (formatting, EHM, etc.)  | PASS   | All new code follows EHM (`*A` variants default), 5/3 blank-line rules, column alignment, Pch.h-only system includes.          |
| II. Testing Discipline (headless tests)  | PASS   | MRU pruning + cap, dialog layout metrics, filename truncation are all factored as pure functions taking injected predicates / metrics. No real file I/O in unit tests. |
| III. UX Consistency                       | PASS   | All converted dialogs preserve existing text and accelerators; no CLI change.                                                  |
| IV. Performance                           | PASS   | One-frame open/repaint; MRU prune uses cheap `std::filesystem::exists` only — never network-stat. Edge case explicitly punted to next launch. |
| V. Simplicity & Maintainability          | PASS   | Single new primitive directory; no new third-party dependency (no constitution amendment needed); `DiskIIDebugDialogState` UI/logic separation is preserved. |

**Approved third-party dependencies**: No additions. Implementation uses
existing Windows SDK + DX stack already on the constitution allowlist
baseline.

**Validation suites**: The dialog work touches **no** CPU, assembler, or
binary-output code paths. The Dormann (`scripts/RunDormannTest.ps1`) and
Harte (`scripts/RunHarteTests.ps1 -SkipGenerate`) suites are **not**
required for this feature. Should any FR end up touching CassoCore /
CassoEmuCore behavior (it should not), re-evaluate and run both.

**Result**: No violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/011-native-dialogs-completion/
├── plan.md              # This file (/speckit.plan command output)
├── spec.md              # Feature spec
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output — MRU + DriveWidgetState extensions
├── quickstart.md        # Phase 1 output — manual verification per phase
├── contracts/           # Phase 1 output
│   └── dialog-primitive.md   # Reusable themed dialog primitive interface
└── tasks.md             # Phase 2 output (/speckit.tasks — NOT created here)
```

### Source Code (repository root)

```text
Casso/
├── Ui/
│   ├── Dialog/                       # NEW — reusable themed dialog primitive
│   │   ├── DialogDefinition.h        # NEW — value type (title/icon/body/buttons/hyperlinks)
│   │   ├── DialogPrimitive.h/.cpp    # NEW — modal overlay window + layout + hit-testing
│   │   ├── DialogLayout.h/.cpp       # NEW — pure layout math (testable, no Win32)
│   │   ├── StartupDownloadDialog.h/.cpp  # NEW — P1 consumer (unified startup)
│   │   └── BootDiskPicker.h/.cpp     # NEW — P1 consumer (MRU + downloads)
│   ├── Settings/                     # EXISTING — model new primitive on this
│   │   ├── SettingsWindow.cpp        # Existing modal overlay (template/reference)
│   │   └── SettingsPanel.cpp         # FR-012: replace stray MessageBoxW
│   ├── Chrome/
│   │   ├── ChromeLayout.*            # EXISTING
│   │   ├── ChromeTheme.h             # EXISTING — palette
│   │   ├── NavLayer.*                # EXISTING
│   │   └── DriveWidget.*             # FR-007: paint filename label
│   ├── DxUiPainter.*                 # EXISTING — extend for inline hyperlink hit-testing
│   ├── DebugConsolePanel.h/.cpp      # NEW (P3) — themed replacement for Win32 EDIT
│   └── DiskIIDebugPanel.h/.cpp       # NEW (P3) — themed replacement for full Win32 dialog
├── Shell/
│   ├── GlobalUserPrefs.cpp           # FR-003: add MRU array to JSON schema
│   └── WindowCommandManager.cpp      # FR-005/006/011: route Help/About/Keymap/MachineInfo
│                                     # + IDM_DISK_INSERT1/2 through new primitive /
│                                     # PromptForDiskImage
├── AssetBootstrap.cpp                # FR-001: rewrite PromptUser / PromptBootDisk /
│                                     # PromptDiskAudioConsent against new primitive
├── DebugConsole.cpp                  # P3: delete after DebugConsolePanel reaches parity
├── DiskIIDebugDialog.cpp             # P3: delete after DiskIIDebugPanel reaches parity
├── DiskIIDebugDialogState.h/.cpp     # UNTOUCHED — pure logic, must stay UI-free
└── Main.cpp                          # UNTOUCHED — EHM-notify MessageBoxW path is in-scope-OUT
WindowCommandManager.cpp::PromptForDiskImage  # UNTOUCHED — IFileOpenDialog stays
UnitTest/
├── DiskMruTests.cpp                  # NEW — MRU prune + cap eviction (synthetic predicate)
├── DialogLayoutTests.cpp             # NEW — button row metrics, hyperlink hit-test, icon slot
├── DriveLabelTruncationTests.cpp     # NEW — basename derivation + ellipsis truncation
└── DiskIIDebugDialogStateTests.cpp   # EXISTING — must continue to build/pass unchanged
```

**Structure Decision**: Existing 5-project solution (`Casso.sln`). All new
runtime code lives in the `Casso` project under `Casso/Ui/Dialog/`; all new
tests live in the `UnitTest` project. No changes to `CassoCore`,
`CassoEmuCore`, or `CassoCli`.

### Explicitly Out of Scope (do not touch)

- `Casso/Shell/WindowCommandManager.cpp::PromptForDiskImage` — keep
  `IFileOpenDialog`. This is the supported modern picker and FR-015
  explicitly allows it.
- `Casso/Main.cpp` EHM `SetNotifyFunction` callback that calls
  `MessageBoxW` — this is the chicken-and-egg last-resort path used
  before the DX painter is initialized (or after it has failed). FR-015
  explicitly allows it.

### Integration Points

- **`GlobalUserPrefs` JSON schema**: add a top-level `recentDisks` array
  of absolute paths, most-recent-first, cap 16. Load/save plumbing
  follows the same pattern as the recent `preserve machines section` fix.
- **`DriveWidgetState`**: add `std::filesystem::path imagePath` (or
  `std::string imageName` — see data-model.md). Plumb from the
  disk-mount path (whichever code already updates "drive contents")
  through `DriveWidgetController` to the widget.
- **`SettingsWindow` modal-overlay plumbing**: extract the
  show / route-input / dismiss / re-layout-on-DPI / re-paint-on-theme
  logic into the new `DialogPrimitive`. `SettingsWindow` is then
  expressed in terms of `DialogPrimitive` (preferred) or co-exists
  with it during the transition (fallback if the refactor surfaces
  unexpected coupling — decide in Phase 0 research).
- **`WindowCommandManager`**: the three MessageBox-based commands
  (Help/About, Help/Keymap, machine-info) route to
  `DialogPrimitive::Show` with `DialogDefinition` values. The two
  legacy `IDM_DISK_INSERT1/2` `GetOpenFileNameW` paths route to the
  existing `PromptForDiskImage`.

### Testability Surface

All of these MUST be headless (no Win32, no real file I/O):

- **MRU**: `class DiskMru` with `void RecordMount (path)`,
  `vector<path> Prune (function<bool (path)> exists) const`, cap = 16.
  Synthetic `exists` predicate in tests.
- **Dialog layout**: free functions in `DialogLayout` that take a
  `DialogDefinition` + metrics struct (font heights, padding, dpi
  scale, max width) and return a `DialogLayoutResult` (icon rect,
  body rects, hyperlink rects, button rects, total size). Tested
  with synthetic metrics — no DirectWrite calls.
- **Drive label truncation**: pure function taking a basename, a
  max-pixel-width, and a "measure glyph-run width" callback; returns
  the truncated display string. Tests inject a deterministic measure
  callback (e.g., constant 8 px per char) so the algorithm is
  testable without DirectWrite.

### Migration Risk

- **FR-010 (Disk II Debug Dialog)** is the largest single conversion
  in the spec. Approach:
  1. Stand up `DiskIIDebugPanel` next to `DiskIIDebugDialog`; both
     bind to the same `DiskIIDebugDialogState`.
  2. Port one control family at a time (in order of risk): static
     labels → checkboxes → radio buttons → text inputs with
     validation → buttons → ListView → column-header context menu
     → tooltips.
  3. Keep `DiskIIDebugDialog.cpp` building and reachable behind a
     `#ifdef CASSO_LEGACY_DISKII_DEBUG_DIALOG` (or equivalent
     compile-time switch) until `DiskIIDebugPanel` reaches feature
     parity.
  4. Delete the Win32 version once parity is verified by manual
     control-by-control exercise against the quickstart checklist.
- `DiskIIDebugDialogState` is already pure-logic and headless-tested.
  The conversion MUST NOT add any Win32 includes or types to that
  TU; SC-010 enforces this via the existing headless tests
  continuing to build.

## Complexity Tracking

No constitution violations. Section intentionally empty.
