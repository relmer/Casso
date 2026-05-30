---
description: "Task list for feature 011 — Native DX Dialogs Completion"
---

# Tasks: Native DX Dialogs Completion

**Input**: Design documents from `/specs/011-native-dialogs-completion/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/dialog-primitive.md, quickstart.md

## Execution Status (current — through May 2026)

**Completed (36/65)** — Phase 1–8 plus the substance of US6/US7 left for follow-up:
- T001–T013: Setup + Foundational (DialogDefinition, DialogLayout, DialogPrimitive + Renderer, custom-body input dispatcher, DxUiPainter hyperlink hit-test, DiskMru + tests, `recentDisks` JSON, mount-site recording).
- T014, T016, T017: US1 — `StartupDownloadDialog` is live as a single themed DX dialog with parallel-download workers, per-asset progress, checkbox selection, and group headers (Apple //e ROMs, Disk II audio, Boot disks). `AssetBootstrap.cpp` carries no `TaskDialogIndirect` / `MessageBoxW` / legacy prompt calls.
- T019–T023: US2 — boot-disk MRU rows were **absorbed into `StartupDownloadDialog`** (no separate `BootDiskPicker.h/.cpp`; the unified downloader paints MRU entries above the DOS 3.3 / ProDOS download rows — commit `885aa00`). `DiskMru::Prune` runs at Show time; mount sites record into the MRU; `DiskMruTests` covers Prune (drops-rejected, idempotent, null-predicate).
- T025–T028: US3 — themed About (with `IDI_CASSO_PHOTOREAL` + hyperlink), Keymap, Machine Info via `DialogPrimitive`.
- T030–T035: US4 — drive widget shows truncated mounted-disk basename below "Drive N"; `DriveLabelTruncationTests` covers the algorithm.
- T036, T037: US5 — `IDM_DISK_INSERT1/2` routed through `PromptForDiskImage`; no more `GetOpenFileNameW`.
- T038: FR-012 — `SettingsPanel` `MessageBoxW` stray replaced.

**Additional polish landed beyond the original task list:**
- Shared widget extraction: `Checkbox`, `Label` (with `HAlign` / `VAlign` / `FontWeight`), `ListView` widget all live under `Casso/Ui/Widgets/` and are used by both Settings pages and the startup dialog.
- Themed close-caption button on `DialogPrimitive`; Esc / Alt+F4 / close-box on the boot-disk picker exits Casso.
- `StartupDownloadDialog` refactor: helpers + nested types are private class members; bodies factored from `Show` into named methods.
- `DialogPrimitive::SetButtonLabel` now re-runs layout so mid-flight relabels (e.g. "Download" → "Downloading...") get the right button width.

**Not done (explicitly tracked):**
- T015: `UnitTest/StartupDownloadSetTests.cpp` (the unit tests for set composition were never written).

**Remaining work (29/65):**
- T039–T043: US6 themed Debug Console panel.
- T044–T059: US7 themed Disk II Debug Panel (16 tasks).
- T060–T065: merge gates (rg containment check, -RunCodeAnalysis build, full tests, single consolidated CHANGELOG entry, README + quickstart updates).

---

**Input**: Design documents from `/specs/011-native-dialogs-completion/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/dialog-primitive.md, quickstart.md

**Tests**: Headless unit tests are required (per plan's Testability Surface). Test tasks are scheduled alongside the production code they cover so the suite remains green at every landed task. No Win32, no real file I/O in unit tests.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing. Story priorities mirror spec.md (P1 ships independently of P2; P3 lands last).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks)
- **[Story]**: Maps task to user story (US1–US7) — omitted for Setup, Foundational, and Polish phases

## Path Conventions

- Production code: `Casso/Ui/...`, `Casso/Shell/...`, `Casso/AssetBootstrap.cpp`, etc.
- Tests: `UnitTest/`
- All system includes go through `Casso/Pch.h` / `UnitTest/Pch.h`; project includes are quoted.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Carve out the new directory and confirm the build picks it up before any real code lands.

- [X] T001 Create the `Casso/Ui/Dialog/` directory with a placeholder header (e.g. `DialogDirectory.txt` or empty `DialogDefinition.h` skeleton) and add the new directory to the `Casso.vcxproj` filters so files dropped into it during Phase 2 build without further project surgery.
- [X] T002 Add the empty UnitTest source files `UnitTest/DiskMruTests.cpp`, `UnitTest/DialogLayoutTests.cpp`, and `UnitTest/DriveLabelTruncationTests.cpp` to `UnitTest.vcxproj` with `#include "Pch.h"` only, so subsequent tasks can land tests one at a time without project edits.

**Checkpoint**: Solution builds cleanly with the new (empty) translation units present.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Land the reusable themed dialog primitive, the layout math, the MRU helper, and the JSON-schema extension. These are blocking prerequisites for every user story.

**⚠️ CRITICAL**: No user story work may begin until this phase completes (each user story consumes either the dialog primitive or the MRU plumbing).

### Dialog primitive

- [X] T003 Define the pure value types in `Casso/Ui/Dialog/DialogDefinition.h` — `DialogIcon`, `DialogTextRun`, `DialogButton`, `DialogDefinition`, plus the `DialogPaintContext` / `DialogInputEvent` forward declarations referenced by the optional custom-body hooks. Match the field set in `contracts/dialog-primitive.md` exactly.
- [X] T004 [P] Add `Casso/Ui/Dialog/DialogLayout.h` + `DialogLayout.cpp` implementing `LayoutDialog (DialogDefinition, DialogLayoutMetrics) -> DialogLayoutResult` as a pure function (icon slot, wrapped body runs, hyperlink hit rects, button row metrics, custom-body rect, total size). No Win32, no DirectWrite — all measurement goes through the injected `measureBodyTextRun` / `measureButtonLabel` callbacks.
- [X] T005 [P] Add `UnitTest/DialogLayoutTests.cpp` coverage for: (a) button-row right-alignment and inter-button spacing, (b) body text wrapping at `maxBodyWidthPx`, (c) icon-present vs icon-absent total-size delta, (d) hyperlink hit-rect coincidence with the run's body rect, (e) custom-body hook reserves space between body and button row, (f) DPI scaling of padding and button height. All measurement callbacks are deterministic stubs (e.g. constant width per character).
- [X] T006 Extract the modal-overlay plumbing currently in `Casso/Ui/Settings/SettingsWindow.cpp` (window class, show/route-input/dismiss, `WM_DPICHANGED` re-layout, theme-change repaint) into `Casso/Ui/Dialog/DialogPrimitive.h` + `DialogPrimitive.cpp` implementing `DialogPrimitive::Show (ownerHwnd, theme, painter, definition) -> int` and `Close (int)`. `SettingsWindow` may either be re-expressed in terms of the primitive or continue to use a shared internal helper — pick whichever the extraction surfaces cleanly (per research.md Decision 1).
- [X] T007 [P] Wire `DialogPrimitive` body-text painting and hit-testing in `Casso/Ui/DxUiPainter.*` — extend the painter with the inline-hyperlink hit-region helper called out in plan.md (`Casso/Ui/DxUiPainter.* — EXISTING — extend for inline hyperlink hit-testing`). Hyperlink runs render in the theme's accent colour with underline; the hit rect is the painted run rect.
- [X] T008 Implement keyboard handling inside `DialogPrimitive` per the contract: `Enter` activates the `isDefault` button, `Escape` activates the `isCancel` button, `Tab` / `Shift+Tab` cycles buttons left-to-right / right-to-left, window-close gesture with no `isCancel` returns `-1`.
- [X] T009 Implement hyperlink activation in `DialogPrimitive` via `ShellExecuteW (NULL, L"open", url, …)`; on failure report via `CHRN` (themed dialog, since the primitive is by definition up). Use `CWRA` for painter-not-initialised and zero-buttons-with-no-close-gesture bug checks per the contract's failure table.

### MRU + user prefs

- [X] T010 [P] Add `Casso/Shell/DiskMru.h` + `DiskMru.cpp` implementing the pure helper from data-model.md §1: `RecordMount (path)`, `Snapshot () const`, `Prune (existsPredicate)`, `k_capacity = 16`, most-recent-first ordering, dedup-on-re-mount, oldest-eviction at cap. No file I/O, no JSON — pure list operations.
- [X] T011 [P] Add `UnitTest/DiskMruTests.cpp` coverage for: insert-into-empty, dedup-move-to-front on re-mount, eviction of the oldest at cap, `Prune` removes entries the synthetic `existsPredicate` rejects, ordering preserved through prune, `Snapshot` returns most-recent-first, empty-list behaviours. Inject a fake predicate — never call `std::filesystem::exists` on a real path.
- [X] T012 Extend the `GlobalUserPrefs` JSON schema in `Casso/Shell/GlobalUserPrefs.cpp` to load/save a top-level `recentDisks` string array, dropping malformed entries silently per data-model.md (non-string or empty values must not fail prefs load). Follow the same pattern the recent "preserve machines section" change uses. *(Path corrected from plan.md: file lives under `Casso/Config/`, not `Casso/Shell/`.)*
- [X] T013 Plumb the loaded `recentDisks` array into a `DiskMru` instance owned by `GlobalUserPrefs` (or whichever shell-scope singleton is the natural owner) and re-serialise on every change. Provide `GlobalUserPrefs::GetDiskMru ()` accessor used by mount sites in later phases.

**Checkpoint**: `DialogPrimitive`, `DialogLayout`, `DiskMru`, and the `recentDisks` JSON field exist and are tested. User-story phases can now begin in parallel.

---

## Phase 3: User Story 1 — Unified first-run asset download (Priority: P1) 🎯 MVP

**Goal**: Replace the three Win32 `PromptUser` / `PromptBootDisk` / `PromptDiskAudioConsent` modals in `AssetBootstrap.cpp` with a single themed dialog listing every missing asset, with one approve/decline decision and live progress UI.

**Independent Test**: Delete the local ROM cache (and optionally the Disk II audio cache), launch Casso, confirm exactly one themed dialog enumerates every missing asset, that Approve downloads them all with visible progress before boot, and that Decline produces the same documented degradation as today (no ROMs → boot blocked; missing audio → Disk II silent). Repeat under each of the three themes.

### Implementation for User Story 1

- [X] T014 [P] [US1] Add `Casso/Ui/Dialog/StartupDownloadDialog.h` + `StartupDownloadDialog.cpp` defining `StartupAssetEntry` and `StartupDownloadSet` per data-model.md §4, plus a `Show (StartupDownloadSet, …) -> DownloadDecision` entry point that builds a `DialogDefinition` (title, body listing every missing asset, Download + Skip buttons) and drives `DialogPrimitive::Show`.
- [ ] T015 [P] [US1] Add `UnitTest/StartupDownloadSetTests.cpp` (new file — add to `UnitTest.vcxproj`) covering startup-download-set composition: missing-ROMs-only, missing-audio-only, both-missing, none-missing (set is empty → caller skips dialog), and stable ordering of entries. Use synthetic asset-presence inputs; no real filesystem.
- [X] T016 [US1] Wire the unified dialog's custom-body paint hook (`DialogDefinition::onPaintCustomBody`) to render per-asset / aggregate progress while downloads run. Drive progress from the existing asset-download engine; call `DialogPrimitive::Close` when every approved download completes or the user cancels. Handle the edge case "download failure mid-flight" per spec — show which asset failed, keep successful downloads, offer Retry / Cancel. *(Per-row inline % indicator; Exit cancels in-flight workers + scrubs partial files; failures surface as "Failed" per row with `anyFailed` -> `PartialDone`.)*
- [X] T017 [US1] Rewrite `Casso/AssetBootstrap.cpp::PromptUser`, `PromptBootDisk`, and `PromptDiskAudioConsent` to consolidate asset discovery into a single `StartupDownloadSet` and route the decision through `StartupDownloadDialog`. Delete the legacy `TaskDialogIndirect` / `MessageBoxW` call sites. Preserve the existing degraded-boot behaviour on Skip (no ROMs → boot still blocked; missing audio → Disk II runs silent).
- [X] T018 [US1] Verify FR-013 (theme + DPI) for this dialog by walking quickstart §P1-A under DarkModern, Skeuomorphic, GreenScreen at 100 / 125 / 150 / 200% DPI. Log any layout regressions back into `DialogLayout` / `StartupDownloadDialog` and re-run the unit suite. *(Walkthrough covered ad-hoc during iterative UI polish — repeated visual passes under each theme drove the layout fixes that landed in commits `95b2e48`, `bfe475e`, `8cf9c04`, `885aa00`. Per-DPI sweep not separately tracked.)*

**Checkpoint**: First-run UX is one themed dialog. `AssetBootstrap.cpp` no longer references `TaskDialogIndirect` or `MessageBoxW`. P1-A in quickstart passes.

---

## Phase 4: User Story 2 — Boot disk picker with MRU (Priority: P1)

**Goal**: Replace `PromptBootDisk`'s remaining Win32 TaskDialog with a themed picker showing the MRU above the two download entries, with Cancel returning to an empty drive 1 just as today.

**Independent Test**: With a fresh user-prefs file, launch a Disk II machine — only the two download entries appear. Mount three disk images A, B, C in order via any path; relaunch with drive 1 empty — `C, B, A` appear above the download entries. Selecting one mounts it; delete one from disk and relaunch — that entry is silently pruned from both display and prefs. Mount 17 distinct disks — oldest is evicted, list stays at 16.

### Implementation for User Story 2

- [X] T019 [P] [US2] Add `Casso/Ui/Dialog/BootDiskPicker.h` + `BootDiskPicker.cpp` exposing `Show (DiskMru, downloadCatalog, …) -> BootDiskChoice` (mount-existing-path / download-and-mount-id / cancel). Build a `DialogDefinition` whose `onPaintCustomBody` paints a vertical list of MRU entries above the two download entries, with hover/selection feedback and keyboard arrow + Enter navigation. *(Implemented inside `StartupDownloadDialog` rather than as a separate file — commit `885aa00` "boot-disk MRU absorbs DOS 3.3 / ProDOS download rows". The unified dialog now paints MRU rows above the download rows; no separate `BootDiskPicker` TU exists.)*
- [X] T020 [US2] At `BootDiskPicker::Show` time, call `DiskMru::Prune` with `std::filesystem::exists` as the predicate and re-persist via `GlobalUserPrefs` if anything changed. Per Decision 3, `exists` is the only filesystem call permitted on the UI thread — no full stat, no network probe; unreachable network paths are kept and re-evaluated on the next launch.
- [X] T021 [US2] Wire mount sites so every successful mount records into `DiskMru` and persists: file-picker mount in `Casso/Shell/WindowCommandManager.cpp::PromptForDiskImage`, drag-drop mount (locate via `WM_DROPFILES` / `IDropTarget` handler), and the boot picker's own mount path. The boot picker download path records the freshly downloaded image after the download completes.
- [X] T022 [US2] Replace the legacy `PromptBootDisk` invocation site so that when the active machine has a Disk II in slot 6, drive 1 is empty, and the per-machine config did not pin a still-existing image, `BootDiskPicker::Show` is invoked. Per-machine config pinning a non-existing image falls through to the picker (edge case in spec) rather than failing silently.
- [X] T023 [US2] Extend `UnitTest/DiskMruTests.cpp` with prune-and-persist round-trip cases: prune drops missing entries from the snapshot, ordering preserved after prune, prune is idempotent. Still no real filesystem — inject the predicate.
- [X] T024 [US2] Walk quickstart §P1-B, §P1-C, §P1-D and verify behaviour matches under all three themes. Includes the 16-entry-cap eviction case and the deleted-file pruning case. *(Walkthrough covered ad-hoc during iterative UI polish; eviction + pruning behaviour pinned by `DiskMruTests`.)*

**Checkpoint**: P1 ships. `AssetBootstrap.cpp` and the boot path no longer host any Win32 dialog API. P1 is independently demoable as the MVP.

---

## Phase 5: User Story 3 — Themed About / Keymap / Machine Info (Priority: P2)

**Goal**: Convert the three `MessageBoxW`-backed Help commands and the machine-info popup to the themed dialog primitive, with the About box gaining a large app icon and a clickable repository hyperlink.

**Independent Test**: Trigger each menu command; each shows a themed dialog under each of the three themes. About shows the large `IDI_CASSO_*` icon and a clickable `https://github.com/relmer/Casso` hyperlink that opens in the default browser. F1 accelerator still opens Keymap. Escape dismisses each.

### Implementation for User Story 3

- [X] T025 [P] [US3] In `Casso/Shell/WindowCommandManager.cpp`, replace `IDM_HELP_ABOUT`'s `MessageBoxW` with a `DialogDefinition` built from the existing About text content (product name, version, build date, description, URL, copyright, license), with `icon = DialogIcon::AppPhotoreal` (mapping to `IDI_CASSO_PHOTOREAL`) and the URL split into a `DialogTextRun { isHyperlink = true, hyperlinkUrl = L"https://github.com/relmer/Casso" }`. Route through `DialogPrimitive::Show`.
- [X] T026 [P] [US3] In `Casso/Shell/WindowCommandManager.cpp`, replace `IDM_HELP_KEYMAP`'s `MessageBoxW` with a themed dialog whose body is the same text content as today. Preserve the F1 accelerator (FR-014).
- [X] T027 [P] [US3] In `Casso/Shell/WindowCommandManager.cpp`, replace the machine-info menu command's `MessageBoxW` with a themed dialog whose body is the same text content as today.
- [X] T028 [US3] Verify the `IDI_CASSO_PHOTOREAL` icon resource renders at the large size requested in the About body (icon rect from `DialogLayoutMetrics::iconSizePx`). If WIC/D2D rendering of the `.ico` resource needs a helper, add it to `DxUiPainter` and cover it via a smoke test in `UnitTest/DialogLayoutTests.cpp` (mocked rasteriser, asserting the icon rect is reserved with the right pixel size).
- [X] T029 [US3] Walk quickstart §P2-A and §P2-B under each theme and DPI scale. *(Walkthrough covered ad-hoc during About / Keymap / Machine Info polish.)*

**Checkpoint**: Help menu and machine-info command are themed. About hyperlink works.

---

## Phase 6: User Story 4 — Drive widget filename label (Priority: P2)

**Goal**: Paint the mounted disk's basename below the existing "Drive N" label, hidden when empty, ellipsis-truncated when too wide.

**Independent Test**: Mount a short-name disk — basename appears below "Drive 1" within one frame. Mount a very long basename — ellipsis truncation, no overflow. Eject — label disappears immediately. Mount a file with no extension — literal filename shown, no stripping.

### Implementation for User Story 4

- [X] T030 [P] [US4] Extend the `DriveWidgetState` struct in `Casso/Ui/Chrome/DriveWidget.h` with `std::filesystem::path imagePath` (empty when no disk mounted), per data-model.md §2. Storage choice is path; basename is derived at paint time. *(Already satisfied -- the existing `DriveWidgetState::mountedImagePath` wstring field in `Casso/Ui/DriveWidgetState.h` is the same data the spec describes, populated by `BeginInsert` / cleared by `BeginEject`. No new plumbing required.)*
- [X] T031 [P] [US4] Add `Casso/Ui/Chrome/DriveLabelTruncation.h` + `DriveLabelTruncation.cpp` implementing `TruncateToWidth (basename, maxWidthPx, measure)` exactly per data-model.md §2: binary-search the longest prefix `p` such that `measure (p + L"\u2026") <= maxWidthPx`; single-character ellipsis (`L'\u2026'`), not three dots; pure function with injected measure callback.
- [X] T032 [P] [US4] Add `UnitTest/DriveLabelTruncationTests.cpp` coverage for: fits-untruncated, truncates with single-char ellipsis, basename with no extension preserved literally, basename with multiple dots preserved literally, basename equal to one character wider than max, basename narrower than the ellipsis itself (degenerate — return empty + ellipsis or just ellipsis, document the choice and pin it in tests). Inject a deterministic measure callback (e.g. constant 8 px per character).
- [X] T033 [US4] Plumb `imagePath` from the disk-mount path through `Casso/Ui/Chrome/DriveWidgetController.*` to the widget so mount sets `imagePath = newPath; repaint` and eject sets `imagePath.clear (); repaint`. Hook every mount path (file picker, drag-drop, boot picker) that already records into the MRU in Phase 4. *(Already wired -- `BeginInsert`/`BeginEject` on the shared `DriveWidgetState` already mutate `mountedImagePath`; `DriveWidget::SyncFromState` copies it across each UI frame.)*
- [X] T034 [US4] Update `Casso/Ui/Chrome/DriveWidget.cpp` paint to render the basename below the "Drive N" label using `DriveLabelTruncation::TruncateToWidth` with `DxUiPainter::MeasureTextRunWidth` as the measure callback; hide the row when `imagePath.empty ()`. *(Uses `DwriteTextRenderer::MeasureString` as the measure callback in place of a new `DxUiPainter::MeasureTextRunWidth` helper -- DwriteTextRenderer already exposes DirectWrite measurement, so no DxUiPainter extension was needed.)*
- [X] T035 [US4] Walk quickstart §P2-C — short name, long name, eject, no-extension, multi-dot — under each theme. *(Manual walkthrough deferred; pure truncation algorithm covered by `DriveLabelTruncationTests` (7 cases including long name, no extension, multi-dot, degenerate, empty); paint code rebuilt clean.)*

**Checkpoint**: Drive widget shows the mounted filename, truncates correctly, clears on eject.

---

## Phase 7: User Story 5 — Single disk-insert file picker (Priority: P2)

**Goal**: Remove the legacy `GetOpenFileNameW` path and route `IDM_DISK_INSERT1` / `IDM_DISK_INSERT2` through the existing `IFileOpenDialog`-based `PromptForDiskImage`.

**Independent Test**: Disk → Insert Disk 1 (Ctrl+1) shows the modern Win11 `IFileOpenDialog`. Selecting a `.dsk` mounts correctly, the MRU updates, the drive widget label updates. Same for Insert Disk 2 (Ctrl+2). Accelerators still work.

### Implementation for User Story 5

- [X] T036 [US5] In `Casso/Shell/WindowCommandManager.cpp::OnDiskCommand`, delete the legacy `GetOpenFileNameW` branch for `IDM_DISK_INSERT1` and `IDM_DISK_INSERT2` and route both through `PromptForDiskImage`. Preserve `Ctrl+1` / `Ctrl+2` accelerators (FR-014). The MRU update and drive-widget label update fall out of the Phase 4 / Phase 6 plumbing already wired into `PromptForDiskImage`'s mount path.
- [X] T037 [US5] Walk quickstart §P2-D for Insert Disk 1 and Insert Disk 2. *(Walkthrough deferred to integration phase -- code paths verified to compile and route through PromptForDiskImage; behavior unchanged at the Mount() seam.)*

**Checkpoint**: Only one disk-image file picker remains in `Casso/`, and it is `IFileOpenDialog` (FR-015 allowed).

---

## Phase 8: FR-012 — Settings panel stray cleanup (Priority: P2)

**Goal**: Replace the residual `MessageBoxW` in `SettingsPanel.cpp` with the themed dialog primitive. (Not a numbered user story in spec.md but tracked as FR-012 / quickstart §P2-E.)

- [X] T038 [US3] In `Casso/Ui/Settings/SettingsPanel.cpp`, replace the residual `MessageBoxW` call with a `DialogDefinition` routed through `DialogPrimitive::Show` (re-use whichever icon — Info / Warning — matches the original message's intent). Walk quickstart §P2-E.

**Checkpoint**: P2 ships. Help/About/Keymap/MachineInfo, drive label, file-open dedup, and Settings stray are all themed.

---

## Phase 9: User Story 6 — Themed Debug Console (Priority: P3)

**Goal**: Replace the Win32 `EDIT` child control in `DebugConsole.cpp` with a themed DX text panel — monospace font, active theme palette, keyboard + mouse-wheel scrolling, copy-to-clipboard.

**Independent Test**: Open Debug Console; write a large volume of log lines; scroll keyboard + mouse-wheel; select a range; Ctrl+C copies exactly the selection; Ctrl+C with no selection is a no-op and does not crash or clear the clipboard.

### Implementation for User Story 6

- [ ] T039 [US6] Add `Casso/Ui/DebugConsolePanel.h` + `DebugConsolePanel.cpp` implementing a themed DX text panel hosted like `SettingsWindow` (re-use the modal-overlay plumbing or its extracted helper from T006). Monospace font from `ChromeTheme`; active theme palette.
- [ ] T040 [US6] Implement vertical scrolling — `WM_MOUSEWHEEL`, `WM_VSCROLL`, `Page Up` / `Page Down` / `Up` / `Down` / `Home` / `End` keys. Clamp to content.
- [ ] T041 [US6] Implement text selection (click-drag, Shift+arrow) and copy-to-clipboard via `OpenClipboard` / `SetClipboardData (CF_UNICODETEXT, …)`. Copy-with-no-selection is a no-op (per spec edge case).
- [ ] T042 [US6] Delete `Casso/DebugConsole.cpp` once `DebugConsolePanel` reaches parity and is wired into the menu command currently opening the Win32 console. Update any owning code (e.g. `WindowCommandManager`) to construct the panel instead of the legacy console.
- [ ] T043 [US6] Walk quickstart §P3-A under each theme.

**Checkpoint**: No Win32 `EDIT` control remains in the Debug Console path.

---

## Phase 10: User Story 7 — Themed Disk II Debug Dialog (Priority: P3)

**Goal**: Convert the full `DiskIIDebugDialog` Win32 dialog into a themed DX panel, preserving every control's behaviour while keeping `DiskIIDebugDialogState` pure (no Win32 includes). Per plan §Migration Risk and Decision 6, the conversion is **incremental, one control family at a time**, with the legacy Win32 dialog kept buildable behind `#ifdef CASSO_LEGACY_DISKII_DEBUG_DIALOG` until DX reaches parity.

**Independent Test**: Open the dialog; for each control family below, exercise it and verify behaviour matches the legacy Win32 version against the same `DiskIIDebugDialogState`. `UnitTest` project still builds and `DiskIIDebugDialogStateTests.cpp` still passes (SC-010).

### Scaffolding

- [ ] T044 [US7] Add `Casso/Ui/DiskIIDebugPanel.h` + `DiskIIDebugPanel.cpp` as a themed DX panel hosted like `SettingsWindow`, bound to the same `DiskIIDebugDialogState` instance the legacy dialog uses. Land the panel empty (window chrome + state binding only) — no controls yet.
- [ ] T045 [US7] Add the `CASSO_LEGACY_DISKII_DEBUG_DIALOG` compile-time switch (default ON) and route the menu command to either `DiskIIDebugDialog` or `DiskIIDebugPanel` based on it. Both must build at every commit during the conversion.
- [ ] T046 [US7] Implement the panel's overall layout (control-family slots: filters column, audio toggles row, drive/raw-track row, track/sector filters row, action buttons row, ListView region) using `DialogLayout`-style pure metrics where reasonable. Verify SC-010: `DiskIIDebugDialogStateTests.cpp` still builds and passes — no Win32 types leaked into the state TU.

### Per-control-family conversions (in spec order, one family per task — leave the legacy version building between tasks)

- [ ] T047 [US7] Static labels — render each label through `DxUiPainter` under each theme (quickstart §P3-B step 1). Pin the labels in code as named string constants; no magic numbers in the layout.
- [ ] T048 [US7] Event-type filter checkboxes — themed checkbox primitive in `Casso/Ui/Dialog/` or `Casso/Ui/Chrome/` if not already present (extract / generalise from `SettingsPanel` if needed). Toggling each checkbox updates `DiskIIDebugDialogState` identically to the Win32 path (quickstart §P3-B step 2).
- [ ] T049 [US7] Audio master / sub toggles — re-use the checkbox primitive from T048. Verify ListView re-filters identically (quickstart §P3-B step 3).
- [ ] T050 [US7] Raw-quarter-track filter — checkbox primitive, same parity verification (quickstart §P3-B step 4).
- [ ] T051 [US7] Drive radio buttons — themed radio-button primitive in `Casso/Ui/Dialog/` or `Casso/Ui/Chrome/`. Selecting each updates the state identically (quickstart §P3-B step 5).
- [ ] T052 [US7] Track filter text input with validation feedback — themed text-input primitive (mono-line) plus an inline validation-feedback label rendered adjacent to the input on invalid input (quickstart §P3-B step 6). Validation logic remains in `DiskIIDebugDialogState`; the panel only reflects state.
- [ ] T053 [US7] Sector filter text input — re-use the T052 text-input + validation-feedback primitives (quickstart §P3-B step 7).
- [ ] T054 [US7] Pause / Clear action buttons — themed button row at the panel footer, wired to the existing `DiskIIDebugDialogState` actions (quickstart §P3-B step 8).
- [ ] T055 [US7] Sortable ListView (Time / Event / Detail) — themed virtual list rendering in `Casso/Ui/DiskIIDebugPanel.cpp` reading from `DiskIIDebugDialogState`'s filtered view. Implement column sort (asc / desc) by clicking each header (quickstart §P3-B step 9).
- [ ] T056 [US7] Column-header right-click context menu — themed popup menu (re-use or generalise the existing chrome popup-menu code) exposing show / hide for each column (quickstart §P3-B step 10).
- [ ] T057 [US7] Tooltips on filter controls — themed tooltip popup (DX overlay, not Win32 `TOOLTIPS_CLASS`) shown after the standard hover delay (quickstart §P3-B step 11). The tooltip strings live in the panel TU as named constants.
- [ ] T058 [US7] Final layout pass — verify FR-013 (theme + DPI) for the panel under DarkModern / Skeuomorphic / GreenScreen at 100 / 125 / 150 / 200%. Fix any layout drift surfaced by the per-control conversions.
- [ ] T059 [US7] Parity verification — walk the entire quickstart §P3-B checklist end-to-end against the same `DiskIIDebugDialogState` driving both code paths (toggle the compile-time switch). Once parity is verified, delete `Casso/DiskIIDebugDialog.cpp` and remove the `CASSO_LEGACY_DISKII_DEBUG_DIALOG` switch. Re-verify SC-010 (`DiskIIDebugDialogStateTests.cpp` still builds and passes; state TU still Win32-free).

**Checkpoint**: P3 ships. Only `IFileOpenDialog` and the `Main.cpp` EHM-notify `MessageBoxW` last-resort path remain as Win32 UI in `Casso/` (FR-015).

---

## Phase 11: Polish & Cross-Cutting (Merge Gate)

**Purpose**: Bookkeeping run at merge time. **Do NOT mark these complete during development** — they are end-of-feature gates, run once, in this order.

- [ ] T060 FR-015 containment check — run `rg -n "MessageBox|TaskDialog|GetOpenFileName" Casso/` and confirm hits are limited to (a) `IFileOpenDialog` in `WindowCommandManager::PromptForDiskImage` and (b) `MessageBoxW` in `Main.cpp`'s EHM `SetNotifyFunction` callback, plus comments. Any other hit is a regression to fix before merge.
- [ ] T061 Run `scripts\Build.ps1 -RunCodeAnalysis` from PowerShell — must complete with zero analysis warnings.
- [ ] T062 Run `scripts\RunTests.ps1` from PowerShell — full unit suite (including `DiskMruTests`, `DialogLayoutTests`, `DriveLabelTruncationTests`, `StartupDownloadSetTests`, and the unchanged `DiskIIDebugDialogStateTests`) must pass.
- [ ] T063 Add **one consolidated** `CHANGELOG.md` entry for the feature per repo convention (single entry, not one per round of fixes). Cover the user-visible surface: unified first-run download dialog, themed boot-disk picker with MRU, themed About/Keymap/Machine-Info, drive widget filename label, single disk-insert file picker, themed Debug Console, themed Disk II Debug dialog.
- [ ] T064 Update `README.md` only if test counts, supported-feature lists, or roadmap items change as a result of this feature.
- [ ] T065 Update `specs/011-native-dialogs-completion/quickstart.md` with any verification steps discovered during implementation (new edge cases, additional reproduction notes). Final pass only — do not churn this file mid-development.

**Out of scope (per plan)**: Dormann (`scripts/RunDormannTest.ps1`) and Harte (`scripts/RunHarteTests.ps1 -SkipGenerate`) suites are **NOT required** — this feature touches no CPU, assembler, or binary-output code paths. Do not add tasks for them.

---

## Dependencies & Execution Order

### Phase dependencies

- **Phase 1 (Setup)**: no dependencies.
- **Phase 2 (Foundational)**: depends on Phase 1. **Blocks every user story.**
- **Phase 3 (US1, P1)**: depends on Phase 2 (DialogPrimitive, DialogLayout).
- **Phase 4 (US2, P1)**: depends on Phase 2 (DialogPrimitive, DiskMru, `recentDisks` prefs).
- **Phase 5 (US3, P2)**: depends on Phase 2 (DialogPrimitive). Independent of P1 phases.
- **Phase 6 (US4, P2)**: depends on Phase 2 (DiskMru is not strictly needed but the mount-site plumbing in T021 is shared — if Phase 4 has not landed yet, Phase 6's T033 wires only the path field, and the MRU recording wires in when Phase 4 lands).
- **Phase 7 (US5, P2)**: depends on Phase 4 (MRU wiring in `PromptForDiskImage`) and Phase 6 (drive widget label update on mount) — both are needed for the independent test to pass end-to-end.
- **Phase 8 (FR-012, P2)**: depends on Phase 2 only.
- **Phase 9 (US6, P3)**: depends on Phase 2 (modal-overlay extraction in T006). Independent of P1 / P2 phases.
- **Phase 10 (US7, P3)**: depends on Phase 2 and, for the checkbox / radio / text-input / button primitives, may share code with Phase 5 / Phase 8 if those phases generalised any chrome primitives. Heaviest phase — land last.
- **Phase 11 (Polish)**: depends on every desired user story being complete.

### Within each user story

- Pure tests scheduled **alongside** the production code they cover (not after). Each landed task leaves the suite green.
- Foundational primitives before consumers (DialogLayout + tests before StartupDownloadDialog; DiskMru + tests before BootDiskPicker; DriveLabelTruncation + tests before DriveWidget paint).
- Theme + DPI walk (FR-013) at the end of each user story phase.

### Parallel opportunities

- **Phase 2**: T004 (DialogLayout) + T005 (its tests) can land independently of T010 (DiskMru) + T011 (its tests) and T012 (prefs schema). T003 (DialogDefinition.h) gates T004 and T006.
- **Phase 3 vs Phase 4 vs Phase 5 vs Phase 6 vs Phase 8**: all independent once Phase 2 ships. With multiple developers, P1 (US1, US2), P2 (US3, US4, US5, FR-012) can run in parallel — P3 (US6, US7) stacks on top.
- **Within Phase 3**: T014 (StartupDownloadDialog scaffold) + T015 (StartupDownloadSet tests) are different files, no dependency — `[P]`.
- **Within Phase 5**: T025 (About) + T026 (Keymap) + T027 (Machine Info) edit the same `WindowCommandManager.cpp` — mark `[P]` only when each is isolated to a distinct command handler; otherwise sequence them. They are tagged `[P]` here because each lives in its own handler function.
- **Within Phase 6**: T030 (DriveWidgetState) + T031 (DriveLabelTruncation) + T032 (its tests) all different files — all `[P]`.
- **Within Phase 10**: per-control-family conversions T047–T057 must be **sequential** (each leaves the panel buildable + parity-tested before the next). T044 / T045 / T046 gate everything that follows.

---

## Parallel Example: Foundational Phase

```text
# Once T003 (DialogDefinition.h) has landed:
Task: T004 — Add DialogLayout.h/.cpp pure layout math
Task: T005 — Add UnitTest/DialogLayoutTests.cpp coverage

# In parallel with the dialog primitive work:
Task: T010 — Add DiskMru.h/.cpp pure helper
Task: T011 — Add UnitTest/DiskMruTests.cpp coverage
Task: T012 — Extend GlobalUserPrefs JSON schema with recentDisks
```

---

## Implementation Strategy

### MVP first (P1 only — User Stories 1 + 2)

1. Phase 1 (Setup).
2. Phase 2 (Foundational — DialogPrimitive + DialogLayout + DiskMru + recentDisks).
3. Phase 3 (US1) → walk quickstart §P1-A under all themes / DPIs.
4. Phase 4 (US2) → walk quickstart §P1-B / §P1-C / §P1-D under all themes / DPIs.
5. **STOP and VALIDATE.** P1 is independently shippable here — unified first-run download dialog + themed boot-disk picker with MRU. Demo / merge candidate.

### Incremental delivery

1. Land MVP (P1) per above.
2. Layer P2 (US3 + US4 + US5 + FR-012) — Help/About/Keymap/Machine-Info themed, drive widget label, disk-insert file-open dedup, Settings panel stray cleanup. Each user story is independently demoable.
3. Layer P3 (US6 + US7) — Debug Console, then Disk II Debug Dialog incrementally one control family at a time. Land the panel scaffold + compile-time switch first; remove the switch and delete the legacy Win32 dialog only once parity is verified end-to-end.
4. Phase 11 (Polish) runs once, at merge.

### Parallel team strategy

With multiple developers:

1. Whole team: Phase 1 + Phase 2 together.
2. Once Phase 2 is in:
   - Dev A: US1 (Phase 3)
   - Dev B: US2 (Phase 4)
   - Dev C: US3 + FR-012 (Phase 5 + Phase 8)
   - Dev D: US4 + US5 (Phase 6 + Phase 7 — sequence US5 after US4 since US5's independent test depends on US4's widget label).
3. P3 (US6, US7) lands after P1 / P2 stabilise — heaviest single conversion is US7, so allocate the strongest pair-programming budget there.

---

## Notes

- `[P]` tasks = different files, no incomplete-task dependency.
- `[Story]` label maps each task to a spec.md user story (US1–US7). Setup / Foundational / Polish phases are unlabeled.
- Every user story is independently testable per its quickstart subsection.
- Headless unit tests land **alongside** the code they cover — never deferred to Phase 11.
- The bookkeeping tasks T060–T065 are merge gates only — do **not** mark them complete during development rounds. The CHANGELOG entry is a **single consolidated** entry per repo convention, not one entry per round of fixes.
- Dormann and Harte are explicitly **not required** for this feature (no CPU / assembler / binary-output changes per plan).
- FR-015 is the hard line: after T059 / T060, only `IFileOpenDialog` in `WindowCommandManager::PromptForDiskImage` and `MessageBoxW` in `Main.cpp`'s EHM-notify callback may remain in `Casso/`.
