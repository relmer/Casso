---

description: "Task list for 030-screenshot-capture"
---

# Tasks: Screenshot capture modes, file output, and metadata

**Input**: Design documents from `/specs/030-screenshot-capture/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/screenshot-metadata.md](contracts/screenshot-metadata.md), [quickstart.md](quickstart.md)

**Tests**: Included. Constitution Principle II (Testing Discipline) is NON-NEGOTIABLE and
the spec carries an explicit testing requirement. Every task that adds a decision to core
has a paired test task.

**Organization**: Grouped by user story so each is independently implementable and
testable. Closes GH #132.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: US1-US4, matching the user stories in spec.md
- Exact file paths are given in every task

## Path Conventions

Four projects, per [plan.md](plan.md) Structure Decision:

- `CassoEmuCore/` -- static library, linked by `UnitTest`. All new **decisions** go here.
- `Casso/` -- the executable. Device objects, `HWND`, paint pump. **Executes**, decides nothing.
- `Dxui/` -- widget library.
- `UnitTest/` -- test project. New `.cpp` files must be registered in `UnitTest.vcxproj`.

**Line numbers below are hints, not anchors.** Every one was verified against the tree on
2026-09-05, but CLAUDE.md's standing hazard is that master lands sweeping renames into
long-lived branches. Navigate by the symbol names, which are given alongside.

---

## Phase 1: Setup

**Purpose**: Make room for the new code before writing any of it.

- [X] T001 Verify a clean baseline: `pwsh scripts/Build.ps1 -Configuration Debug` then `pwsh scripts/RunTests.ps1 -Configuration Debug` both green, and `UnitTest.dll` newer than the build, before changing anything
- [X] T002 Create directory `CassoEmuCore/Capture/` and register it in `CassoEmuCore/CassoEmuCore.vcxproj` (plus `.filters`)
- [X] T003 [P] Create directory `UnitTest/CaptureTests/` and register it in `UnitTest/UnitTest.vcxproj` (plus `.filters`)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The shared decision layer, the generalized components, and the readback
primitive. Every user story depends on this phase.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

**⚠️ HIGHEST RISK**: T018 is new machinery. Per [research.md](research.md) R-004 there is
no GPU-to-CPU readback anywhere in the tree -- every `Map` in `Casso/` and `Dxui/` is
`WRITE_DISCARD`. Do T018 early and prove it in isolation before building on it.

### Capture mode token

- [X] T004 Define `ScreenshotMode` enum with `ParseToken` / `FormatToken` in `CassoEmuCore/Capture/ScreenshotMode.h`; unrecognized and empty tokens resolve to `Scene`, per data-model.md
- [X] T005 [P] Test token round-trip for all three modes, unknown-token fallback to `Scene`, and empty-string fallback, in `UnitTest/CaptureTests/ScreenshotModeTests.cpp`

### Generalize the filename policy

- [X] T006 Replace `ComposePngPath` with `ComposeTimestampedPath (folder, baseName, extension, when, taken)` in `CassoEmuCore/Devices/Printer/PrintFileNaming.h/.cpp`, keeping the bare-name-then-` (n)` deduplication unchanged
- [X] T007 Update the print call site `WindowCommandManager::SavePrintoutAs` in `Casso/Shell/WindowCommandManager.cpp` to pass `"Casso Print"` and `".png"`
- [X] T008 [P] Extend `UnitTest/PrinterTests/PrintFileNamingTests.cpp` to the new signature, adding cases for the `"Casso"` screenshot base name, a non-`.png` extension, and a collision run that exercises the suffix

### PNG text metadata mechanism

- [X] T009 Define `MetadataEntry` (keyword + value, with the PNG keyword rules from data-model.md) in `CassoEmuCore/Devices/Printer/PngMetadata.h` -- beside its codec consumer, **not** under `Capture/`, so the printer never depends on the screenshot directory
- [X] T010 Add an optional `const vector<MetadataEntry> &` parameter to `PngCodec::EncodeRgba` in `CassoEmuCore/Devices/Printer/PngCodec.h/.cpp`, writing each as `/[<n>]tEXt/{str=<Keyword>}` with `VT_LPSTR` through `IWICBitmapFrameEncode::GetMetadataQueryWriter`, before `WriteSource` (both the index and the ordering are required; WIC silently drops the data otherwise); the codec writes what it is handed and decides nothing
- [X] T011 [P] Extend `UnitTest/PrinterTests/PngCodecTests.cpp` with a `tEXt` write-then-read round trip, including the empty-chunks case and a keyword at the 79-character limit

### Preferences

- [X] T012 Add `screenshotMode` (string token), `screenshotSaveFile` (bool, default true) and `screenshotFolder` (string, empty means default) to `Casso/Config/GlobalUserPrefs.h/.cpp`, including `ToJson`/`FromJson`; a known key with an unrecognized value normalizes to `"scene"` on save, unlike an unknown key
- [X] T013 [P] Extend `UnitTest/UiTests/GlobalUserPrefsTests.cpp` with defaults, full round-trip, unrecognized `screenshotMode` normalizing to `"scene"`, and unknown keys still surviving via `unknownPassthrough`

### The decision layer

- [X] T014 Implement `ScreenshotPlan::Resolve` in `CassoEmuCore/Capture/ScreenshotPlan.h/.cpp` per data-model.md, including `folderMustBeCreated`; the three `CaptureSource` conditions are mutually exclusive and exhaustive, and `Scene` is always `BackBufferRegion`. Resolution is total, with refusals as `CaptureRefusal` rather than a failure return; the Pictures folder and the clock are injected, never discovered
- [X] T015 [P] Test `ScreenshotPlan` in `UnitTest/CaptureTests/ScreenshotPlanTests.cpp`: source and rectangle per mode, `Scene` resolving to `BackBufferRegion` with `deskSceneActive` both true and false, `Crt` splitting on `deskSceneActive`, `Raw` never refused when minimized, `Scene`/`Crt` refused when minimized, `hideOverlays` false for `Raw`, empty folder falling back to `<pictures>/Casso Screenshots`, `folderMustBeCreated` when the destination is absent, and `writeFile` false when saving is off
- [X] T016 Implement `CaptureOutcome` and the pure `CaptureOutcome::DescribeResult` in `CassoEmuCore/Capture/CaptureOutcome.h/.cpp`, returning the user-facing notice text for every reachable state; the shell displays the string it is handed and chooses no wording
- [X] T017 [P] Test all eight reachable outcome states in `UnitTest/CaptureTests/CaptureOutcomeTests.cpp`: refused, both sinks succeeded, clipboard-only by preference, clipboard-only by write failure, file-only after a clipboard failure, and neither

### Readback and the shell seam

- [X] T018 Add a readback entry point to `Casso/D3DRenderer.h/.cpp`: create a `D3D11_USAGE_STAGING` texture with `D3D11_CPU_ACCESS_READ` sized to the requested rectangle, `CopySubresourceRegion` the source into it, `Map` for read, unpack rows honoring `D3D11_MAPPED_SUBRESOURCE::RowPitch` into a tightly packed top-down RGBA buffer, `Unmap`. Resources are created per capture and released, never retained. Single exit via `Error:` so the unmap cannot be skipped
- [X] T019 Change `ClipboardManager::CopyScreenshot` in `Casso/Shell/ClipboardManager.h/.cpp` to accept a captured image rather than reading `m_uiFramebuffer` directly, keeping the CF_DIB form and the reversed-row emission
- [X] T020 Add `Casso/Shell/ScreenshotCapture.h/.cpp` to execute a resolved `ScreenshotPlan`: readback or framebuffer read, PNG encode, file write, clipboard copy, populating a `CaptureOutcome`

**Checkpoint**: The decision layer is fully unit-tested and the readback works. User story
work can begin.

---

## Phase 3: User Story 1 - Capture what is on screen, and keep it (Priority: P1) 🎯 MVP

**Goal**: Pressing Screenshot writes a PNG of the scene, with CRT effects, excluding
chrome and the app-describing overlays, and puts the same image on the clipboard. No
dialog, no pause.

**Independent test**: Take one screenshot at default settings. The file exists, opens,
shows the CRT-processed scene without chrome or overlays, and matches the clipboard.

- [X] T021 [US1] Add capture-frame overlay suppression to `Casso/EmulatorShell.h/.cpp`: hide the scene compass (`m_sceneCompass`), the frame-rate readout (`m_fpsReadout`), the scene-pose readout (`m_sceneViewReadout`) and the **mouse-capture** banner (`m_captureBanner`) for the capture paint, restoring them immediately after, per research.md R-013
- [X] T022 [US1] Add the synchronous capture paint to `Casso/EmulatorShell.h/.cpp`: set a capture-pending request, drive `InvalidateRect` + `UpdateWindow` through the existing `WM_PAINT` entry point, and service it at the frame position data-model.md's Frame ordering table gives for the plan's source, reading back before Present
- [X] T023 [US1] Route `IDM_EDIT_COPY_SCREENSHOT` in `Casso/Shell/WindowCommandManager.cpp` through `ScreenshotCapture` with a plan resolved from preferences, replacing the direct `ClipboardManager::CopyScreenshot` call (near line 595)
- [X] T024 [US1] In `Casso/Shell/ScreenshotCapture.cpp`, obtain the Pictures folder via `SHGetKnownFolderPath (FOLDERID_Pictures, …)` and **pass it into** `ScreenshotPlan::Resolve`; perform `create_directories` only when the returned plan sets `folderMustBeCreated`. The exe supplies the folder and does the syscall; it composes no path and decides no policy
- [ ] T025 [US1] Display `CaptureOutcome::DescribeResult`'s text through `DxuiHudNotice` in `Casso/EmulatorShell.cpp`, covering success, **refusal** (the minimized-window edge case, FR-008 and spec Edge Cases) and failure alike -- one notice path, no wording chosen at the call site
- [X] T026 [US1] Run the two sinks independently in `Casso/Shell/ScreenshotCapture.cpp` so neither failure prevents the other (FR-018), recording both results in the `CaptureOutcome`
- [ ] T027 [US1] Validate manually per [quickstart.md](quickstart.md) Scenario 1, with the frame-rate and pose readouts switched on so their absence from the capture is proven, and watch the emulated machine across a single capture and a ten-capture burst for any stall (SC-004)

**Checkpoint**: GH #132's core complaint is fixed and screenshots are files. This is a
shippable increment on its own.

---

## Phase 4: User Story 2 - Choose what the screenshot contains (Priority: P2)

**Goal**: The user picks `scene`, `crt` or `raw` in Settings, and the command follows it.

**Independent test**: Capture once in each mode; three visibly different images with the
documented content and dimensions. Resize and repeat: `raw` alone is unchanged.

- [X] T028 [P] [US2] Add an optional per-option description line to `DxuiRadioOption` and lay it out under the label in `Dxui/Widgets/DxuiRadio.h/.cpp` (research.md R-015); the page must not own the widget's internal geometry
- [X] T029 [P] [US2] Extend `UnitTest/UiTests/DxuiRadioGroupTests.cpp` for the description line: measurement, layout, and options without a description still laying out as before
- [X] T030 [US2] Implement the `Crt` readback points in `Casso/EmulatorShell.cpp` and `Casso/D3DRenderer.cpp`: the back-buffer sub-rect at `m_targetBoundsPx` read **before** the chrome panel walk under flat themes, and `GetSceneContentSrv()` at `pictureRect` under scene themes
- [X] T031 [US2] Implement the `Raw` path in `Casso/Shell/ScreenshotCapture.cpp`: read `m_uiFramebuffer` under its mutex and feed it through the same encode-and-write pipeline, with no capture paint and no overlay suppression
- [X] T032 [US2] Retitle `PrintingPage` and add the Screenshots section with the described radio group in `Casso/Ui/Settings/PrintingPage.h/.cpp`, keeping the Printing section first (FR-031)
- [X] T033 [US2] Change the tab title to `Printing and Screenshots` where `CreatePage<PrintingPage>` is called in `Casso/Ui/Settings/SettingsSheet.cpp` (near line 70)
- [X] T034 [P] [US2] Add `UnitTest/UiTests/PrintingPageTests.cpp` covering the Screenshots section: radio reflects the stored mode, selection writes the token back, and Printing controls are unaffected -- following the existing `DisplayPageTests.cpp` pattern
- [X] T035 [US2] Validate manually per [quickstart.md](quickstart.md) Scenarios 2 and 3, including the persistence-trail check that ruled out a one-shot offscreen render

**Checkpoint**: All three modes work and persist.

---

## Phase 5: User Story 3 - A shared screenshot explains itself (Priority: P3)

**Goal**: Every written file carries the metadata contract, varying correctly by mode.

**Independent test**: Capture in each mode and read the entries back with `exiftool`;
seven entries for `scene`, six for `crt`, five for `raw`, and nothing excluded.

- [X] T036 [US3] Implement `ScreenshotMetadata::Compose (facts)` in `CassoEmuCore/Capture/ScreenshotMetadata.h/.cpp`, emitting the entries of [contracts/screenshot-metadata.md](contracts/screenshot-metadata.md) in the contract's order, per mode; this is the single authority for what a screenshot says
- [X] T037 [P] [US3] Test the composer in `UnitTest/CaptureTests/ScreenshotMetadataTests.cpp`: exact entry count and order per mode, `Casso Scene Pose` absent for `crt` and `raw`, `Casso CRT` absent for `raw`, keyword rules honored, and guards asserting no emitted value contains a path separator, a drive letter, or the image dimensions (FR-025, FR-026)
- [X] T038 [US3] Extract the scene-pose format string from `EmulatorShell::UpdateSceneViewReadout` in `Casso/EmulatorShell.cpp` (near line 2426) into one formatter used by both the on-screen readout and `ScreenshotFacts`, so a pose read from a file and one read from a picture are the same text (FR-024)
- [X] T039 [P] [US3] Implement RFC 1123 `Creation Time` formatting with UTC offset in `CassoEmuCore/Capture/ScreenshotMetadata.cpp`, with tests for a positive offset, a negative offset, and UTC
- [X] T040 [US3] Assemble `ScreenshotFacts` in `Casso/Shell/ScreenshotCapture.cpp`: `"Casso " VERSION_STRING` reusing the `kCassoCreator` construction from `CassoEmuCore/Devices/Disk/WozLoader.cpp`, the machine JSON `name` field, `CrtResolver::MakeKey` for the monitor, the pose, and the resolved `CrtParams`
- [X] T041 [US3] Pass the composed entries into `PngCodec::EncodeRgba` from `Casso/Shell/ScreenshotCapture.cpp`, and pass dpi 0 so no `pHYs` is written (FR-019, research.md R-007)
- [X] T042 [US3] Validate manually per [quickstart.md](quickstart.md) Scenario 4, including the pose round trip and grepping the `exiftool` output for the Windows account name

**Checkpoint**: Screenshots are self-describing.

---

## Phase 6: User Story 4 - Control where screenshots land (Priority: P4)

**Goal**: File saving can be turned off, and the destination can be changed and opened.

**Independent test**: Turn saving off and confirm clipboard-only; turn it on with a custom
folder and confirm the file lands there.

- [X] T043 [US4] Add the "save a file as well as copying" toggle to the Screenshots section in `Casso/Ui/Settings/PrintingPage.h/.cpp`, bound to `screenshotSaveFile`
- [X] T044 [US4] Add the folder path display with Browse and Open actions as children of the toggle in `Casso/Ui/Settings/PrintingPage.h/.cpp`, disabled and dimmed when the toggle is off (FR-034)
- [X] T045 [US4] Implement the folder picker in `Casso/Shell/WindowCommandManager.cpp` using `IFileOpenDialog` with `FOS_PICKFOLDERS` -- new to this tree; the existing `CLSID_FileOpenDialog` use near line 1004 is a file picker (research.md R-016)
- [X] T046 [P] [US4] Implement the Open-folder action via `ShellExecuteW`, following the existing call in `Casso/Ui/Dialogs/DialogBodyContent.cpp` (near line 73)
- [X] T047 [US4] Honor `folderMustBeCreated` for a configured folder that has since been deleted, and surface an unrecoverable write failure through `CaptureOutcome`, in `Casso/Shell/ScreenshotCapture.cpp`
- [X] T048 [P] [US4] Extend `UnitTest/UiTests/PrintingPageTests.cpp` for the toggle gating its children, and `UnitTest/CaptureTests/ScreenshotPlanTests.cpp` for a custom folder overriding the default
- [X] T049 [US4] Validate manually per [quickstart.md](quickstart.md) Scenario 5

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T050 Work the full edge-case table in [quickstart.md](quickstart.md) Scenario 6, and confirm FR-035: the feature's only exposure is the three settings plus the single toolbar button, single menu item and single shortcut, all following the selected mode -- no second command was added anywhere
- [ ] T051 [P] Add the CHANGELOG entry under `[Unreleased]` in `CHANGELOG.md`, stating the net user-visible effect and referencing `Closes #132`; no entry for the spec check-in itself
- [ ] T052 [P] Add a README headline entry for screenshots in `README.md` -- user-visible news only, no detail
- [ ] T053 Run `pwsh scripts/CheckStyle.ps1 -Mode Tree` and clear every hit; CI runs this on every master push
- [ ] T054 Build clean with zero warnings in Debug and Release on x64, and confirm ARM64 builds; ARM64 is build-only, there is no device to run on
- [ ] T055 Run the full suite in both configurations; confirm `UnitTest.dll` is newer than the build before trusting the result
- [ ] T056 Push and watch CI to completion; Code Analysis is not reproducible locally, so a local `-RunCodeAnalysis` pass is not evidence this gate passed

---

## Dependencies

```text
Phase 1 Setup
      |
Phase 2 Foundational  ......  T018 readback is the risk; prove it first
      |
      +--> Phase 3  US1 (P1)  MVP -- closes #132
      |         |
      |         +--> Phase 4  US2 (P2)   needs US1's capture frame
      |         |
      |         +--> Phase 5  US3 (P3)   needs US1's write path
      |         |
      |         +--> Phase 6  US4 (P4)   needs US1's write path
      |
Phase 7 Polish
```

**Story dependencies**: US2, US3 and US4 all build on US1's capture-and-write path but
not on each other. Once US1 is done they can proceed in any order, or concurrently by
different people -- US3 is almost entirely core-side, US4 is almost entirely UI-side, and
US2 straddles both.

**Within Phase 2**: T004/T005, T006-T008, T009-T011, T012/T013 and T016/T017 are five
independent tracks. T014 needs T004. T018-T020 need nothing from the others and can start
immediately, which is what makes proving the readback early possible.

## Parallel execution examples

**Phase 2, five tracks at once**:

```text
T004 + T005   ScreenshotMode           (CassoEmuCore/Capture/)
T006 + T008   PrintFileNaming          (CassoEmuCore/Devices/Printer/)
T009 - T011   PngMetadata + PngCodec   (CassoEmuCore/Devices/Printer/)
T012 + T013   GlobalUserPrefs          (Casso/Config/)
T016 + T017   CaptureOutcome           (CassoEmuCore/Capture/)
T018          D3DRenderer readback     (Casso/)
```

**Phase 4**: T028 + T029 (Dxui widget) run alongside T030 + T031 (capture paths); they
share no files.

**Phase 5**: T037 and T039 are independent test-side work parallel to T036's
implementation.

## Implementation strategy

**MVP is Phase 1 + Phase 2 + Phase 3 (US1)** -- T001 through T027. That alone closes
GH #132 and delivers screenshots as files. Everything after it is refinement, and each
subsequent phase is independently shippable.

**Commit per phase**, per the constitution's Commit Discipline: do not accumulate all
phases into one commit. Use `type(scope): description` with a scope, and reference the
issue (`Closes #132` on the commit that completes US1).

**The one thing to do out of order**: start T018 first. It is the only task in the
feature with no local precedent, and if the readback turns out harder than expected --
row pitch, format conversion, a `FLIP_DISCARD` interaction not anticipated -- that
discovery should happen before four projects have been edited around it.
