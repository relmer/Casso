# Tasks: Blank Disk Creation & Mounting

**Input**: Design documents from `/specs/017-blank-disk-creation/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — constitution Testing Discipline is non-negotiable; every pure
component ships its unit suite in the same phase. End-to-end guest-visible gates
reuse the real-CPU DOS-boot harness (`CatalogReproductionTest` pattern).

**Organization**: Phases map to spec user stories in priority order
(US1 P1 → US2 P2 → US4 P2 → US3 P3). Constitution commit discipline: commit +
push after each completed phase.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

- [X] T001 Add new-file skeletons to the projects: `CassoEmuCore/Devices/Disk/BlankDiskBuilder.h/.cpp`, `Dos33Skeleton.h/.cpp`, `ProDosSkeleton.h/.cpp`, `CassoEmuCore/Ui/FileBrowseModel.h/.cpp`, `Casso/Ui/Dialogs/CreateDiskDialog.h/.cpp`, `UnitTest/EmuTests/BlankDiskBuilderTests.cpp`, `ProDosVolumeTests.cpp`, `FileBrowseModelTests.cpp` — EHM-conformant stubs wired into `CassoEmuCore.vcxproj(.filters)`, `Casso.vcxproj(.filters)`, `UnitTest.vcxproj(.filters)`; x64 Debug compiles clean

## Phase 2: Foundational (blocking prerequisites)

- [X] T002 [P] Extend `IFileSystem` (+ the real and mock implementations) with folder enumeration (name/isFolder/size/modifiedUnix) and read-only-attribute get/set — needed by FileBrowseModel (US1/US3) and the DSK/PO write-protect toggle (US4)
- [X] T003 [P] Add `DiskImageStore::MountedSourcePaths()` (path + slot/drive pairs for every mounted entry) in `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp` + unit coverage in existing `DiskImageStoreTests` — feeds FR-018 refusal
- [X] T004 [P] Define `BlankDiskContents` + `BlankDiskSpec` (+ `ValidateSpec` combination matrix per FR-010) in `BlankDiskBuilder.h/.cpp` with `BlankDiskBuilderTests` cases for every legal/illegal pairing

## Phase 3: User Story 1 — Create a ready-to-use disk and save to it (P1) — MVP

**Goal**: `<Create new disk...>` first in the picker → dialog with defaults →
WOZ+DOS 3.3 data disk created, mounted, guest `SAVE`/`LOAD` round-trips.

**Independent test**: quickstart §1 — create with defaults, `SAVE TEST` →
`LOAD TEST` → `LIST` clean, survives remount.

- [X] T005 [P] [US1] Implement `Dos33Skeleton::Write` (VTOC T17 S0, catalog chain S15→S1, INIT-compatible bitmap per R-004) in `CassoEmuCore/Devices/Disk/Dos33Skeleton.cpp`; structural unit tests (VTOC fields, chain links, bitmap bits) in `UnitTest/EmuTests/BlankDiskBuilderTests.cpp`
- [X] T006 [US1] Implement `BlankDiskBuilder::Build` for `Woz`+`Dos33`(data-only) and `Woz`+`Unformatted`: skeleton buffer → `NibblizationLayer::Nibblize` → `WozLoader::Serialize`; determinism + INFO-WP-byte-0 (FR-012) tests; mount-level test: built bytes → `DiskImageStore::MountFromBytes` → image loads, `CATALOG` scrape lists empty (KeystrokeInjector/TextScreenScraper harness)
- [X] T007 [US1] End-to-end write gate: extend `UnitTest/EmuTests/DiskWritePathTests.cpp` (or `CatalogReproductionTest.cpp`) — boot DOS master in drive 1, mount a built blank WOZ in drive 2, real-CPU `SAVE TEST,D2` → `LOAD TEST,D2` → `LIST` recovers the program; then serialize the written image, reload it fresh, and re-read `TEST` to prove SC-002's remount survival. Uses the cached master asset with graceful SKIP-if-missing (sanctioned exception, plan.md Constitution Check)
- [X] T008 [P] [US1] Implement `FileBrowseModel` v1 in `CassoEmuCore/Ui/FileBrowseModel.cpp`: `SetFolder`/`Refresh`/`Entries`, `SetExtensionFilter`, `UniqueDefaultName`, `SetMountedPaths` + `ValidateTarget` (Ok/Exists/InvalidName/FolderNotWritable/MountedInDrive ordering per contract) — navigation (`NavigateInto`/`Up`, `..` row) deferred to US3; mock-`IFileSystem` tests in `FileBrowseModelTests.cpp` incl. FR-018 refusal outranking Exists
- [X] T009 [US1] Build `CreateDiskDialog` v1 in `Casso/Ui/Dialogs/CreateDiskDialog.h/.cpp` (`DxuiDialogWindow`, PickerDialog idiom): current-folder label, file list (read-only display this phase), `DxuiTextInput` name seeded from `UniqueDefaultName`, Create/Cancel buttons, verdict messaging (exists→confirm, mounted→refuse naming drive, invalid/unwritable→error); format/contents/bootable controls arrive in US2
- [X] T010 [US1] Pin the sentinel `<Create new disk...>` row in `DiskMruPickerSession` (`Casso/AssetBootstrap.cpp`): sentinel `ModelRow` + `sortLess` first-always + `rowPasses` filter-immune (R-009); decode new result in `AssetBootstrap::PromptInsertDiskMru` → new out-signal
- [X] T011 [US1] Orchestrate the create flow in `Casso/Shell/WindowCommandManager.cpp` (contract §2): default-folder resolution (`Documents\Casso Disks`, created on demand), atomic write (temp + rename), occupied-drive confirm (FR-009), `m_shell.Mount` into the picker's drive (FR-008 — MRU/persistence/door for free), all inside the existing `BeginModalKeepAlive` span; failure paths create nothing partial (FR-011)
- [X] T012 [US1] Runtime validation pass: quickstart §1 + §5.2/5.3 by hand (or scripted PostMessage where possible), plus the controller-absent edge: on a machine config without a Disk ][ controller the create path is unreachable/explained (spec edge case); fix what it finds

**Checkpoint**: US1 alone = the feature's MVP; every later phase extends it.
(FR-006 is deliberately partial here — fixed default folder only; the
last-created-folder persistence completes in US3/T028.)

## Phase 4: User Story 2 — Choose format, filesystem, bootable (P2)

**Goal**: format WOZ/DSK/PO × contents DOS 3.3/ProDOS 1.1.1/Unformatted with
FR-010 gating; bootable toggle with catalog-downloaded OS payloads.

**Independent test**: quickstart §2 + §3 — every offered combination yields a
clean guest volume; bootable disks boot to their OS prompt.

- [X] T013 [P] [US2] Implement `ProDosSkeleton::Write` (key block 2, dir chain 2–5, bitmap block 6, `NEWDISK`, 280 blocks per R-005) in `CassoEmuCore/Devices/Disk/ProDosSkeleton.cpp`; structural tests in `UnitTest/EmuTests/ProDosVolumeTests.cpp`
- [X] T014 [US2] Extend `BlankDiskBuilder::Build` to the full matrix: `Dsk` (DOS-order buffer identity), `Po` (ProDOS-order), ProDOS contents on WOZ (block→sector mapping via existing interleave tables), unformatted variants; golden-size + determinism tests; mount-level `CAT`/`CATALOG` cleanliness for each (SC-003)
- [X] T015 [US2] Add format/contents dropdowns to `CreateDiskDialog` with live FR-010 constraint gating (illegal pairings never listed for the chosen format), extension follows format in the name field
- [X] T016 [US2] Implement `ProDosReader::ExtractFile` (walk volume dir, seedling/sapling/tree data blocks) in `ProDosSkeleton.cpp` (or sibling file) with tests against a synthetic volume built by T017's `ProDosFileWriter` (round-trip; runs after T017)
- [X] T017 [US2] Implement `ProDosFileWriter::WriteFile` (dir entry, bitmap allocation, seedling/sapling layouts) with invariant tests: every allocated block marked used exactly once, file count/entry chains coherent, `CAT` shows the files
- [X] T018 [US2] Implement boot-payload installers: `Dos33Skeleton::InstallDos` (tracks 0–2 from System Master) + `Dos33FileWriter::WriteHello` (catalog entry + TS list + data sector, VTOC-honest) per R-006; `ProDosSkeleton::InstallBoot` (blocks 0–1 + `PRODOS` + `BASIC.SYSTEM` via T016/T017) per R-007; unit tests feed SYNTHETIC payload bytes (fabricated tracks 0–2 / synthetic PRODOS+BASIC.SYSTEM files — no host reads; only T020's boot gates touch the real masters) and assert payload placement + skeleton invariants still hold
- [X] T019 [US2] Promote `DownloadStockBootDisk` to a public `AssetBootstrap` static (R-008); wire bootable availability + disabled-reason + on-demand download affordance into `CreateDiskDialog` (FR-017); shell loads payload bytes and re-checks on Create
- [X] T020 [US2] Boot gates (SC-006): real-CPU tests — built bootable DOS 3.3 disk boots to the Applesoft prompt (HELLO runs, screen scrape shows no `FILE NOT FOUND`); built bootable ProDOS disk reaches the BASIC.SYSTEM prompt; place beside the existing boot tests in `UnitTest/EmuTests/`
- [ ] T021 [US2] Runtime validation pass: quickstart §2 + §3 by hand; fix what it finds

## Phase 5: User Story 4 — Write-protect toggle (P2)

**Goal**: Disk-menu per-drive toggle; WOZ flag travels with the image, DSK/PO
via host read-only attribute; indication always truthful.

**Independent test**: quickstart §6.

- [ ] T022 [US4] Implement `DiskManager::ToggleImageWriteProtect (drive)` in `Casso/Shell/DiskManager.cpp` per contract §4: WOZ = flush-dirty-first → `SetImageWriteProtected` → serializer-level flush (bypasses the guest write gate); DSK/PO = `IFileSystem` read-only attribute set/clear; both re-probe (`ProbeFileWritability`) + `ApplyExternalWriteProtect`; failure reports + re-reads truth (FR-016)
- [ ] T023 [P] [US4] Core coverage: WOZ WP flag round-trips through `WozLoader::Serialize`/`Load` after toggle; flush-ordering test proves no sector loss when toggling ON with dirty content; DSK/PO attribute path tested through mock `IFileSystem` — in `UnitTest/EmuTests/` beside `DiskWritePathTests`
- [ ] T024 [US4] Menu surface: `IDM_DISK_WP1/2` checkable items in `Casso/Ui/MainMenu.cpp` (enabled iff mounted; check = `imageFlag ‖ readOnlyFile`), routed in `WindowCommandManager::OnDiskCommand`; update `MainMenuDropdownTests` row expectations
- [ ] T025 [US4] Guest-visible gate: real-CPU test — toggle ON → `SAVE` yields `WRITE PROTECTED`; toggle OFF → `SAVE` succeeds (SC-005); plus quickstart §6 hand pass (padlock, tooltip causes, Explorer attribute visible)

## Phase 6: User Story 3 — Name and locate in-dialog (P3)

**Goal**: real save-dialog navigation + persisted default folder.

**Independent test**: quickstart §4.

- [ ] T026 [P] [US3] Complete `FileBrowseModel`: `NavigateInto`/`NavigateUp`, synthetic `..`, folders-first case-insensitive ordering, cached-listing refilter on `SetExtensionFilter`; extend `FileBrowseModelTests.cpp` (nav round-trips, root behavior, hidden/system exclusion)
- [ ] T027 [US3] Wire navigation into `CreateDiskDialog`: folder rows + `..` activate-to-navigate, up-button + current-path label, selecting a file seeds the name field, full keyboard pass (tab order, Enter-in-name creates, list `SetKeyboardColumnNav`)
- [ ] T028 [US3] Add `GlobalUserPrefs::lastDiskCreateFolder` (4-edit recipe: member, `s_kKnownTopLevel`, `ToJson`, `FromJson`) + RoundTrip prefs test; write on successful create, read at dialog open, fall back to `Documents\Casso Disks` when empty/missing (R-012)
- [ ] T029 [US3] Runtime validation pass: quickstart §4 by hand; fix what it finds

## Phase 7: Polish & cross-cutting

- [ ] T030 [P] Fix the `.nib` lie in `WindowCommandManager::PromptForDiskImage`'s `COMDLG_FILTERSPEC` (advertises `*.nib`; `DetectFormatByExtension` rejects it) — drop the extension (sanctioned adjacent cleanup surfaced by the research code survey; NIB support stays out of scope per spec Assumptions)
- [ ] T031 [P] `CHANGELOG.md` under `[Unreleased]`: feature prose for create-a-disk + write-protect toggle (features get prose; keep fixes terse)
- [ ] T032 Full gates: x64 Debug + Release build clean, full `UnitTest.dll` suite green both configs, ARM64 builds, Code Analysis zero warnings (constitution Quality Gate 5), `scripts/CheckStyle.ps1 -Mode Tree` clean, quickstart end-to-end pass — then user validation before merge to master

## Dependencies

- Phase 2 blocks everything: T002 → T008/T022; T003 → T008; T004 → T005/T006.
- US1 (T005→T006→T007; T008/T009→T011; T010→T011) is the MVP and blocks the
  dialog-extension tasks in US2 (T015, T019) and US3 (T027).
- US2's ProDOS chain: T013 → T017 → T016 → T018 → T020 (T016 tests against
  T017's writer). DOS payload chain: T005 → T018.
- US4 depends only on Phase 2 (+ any mounted image) — parallel to US2/US3.
- US3 completes the dialog started in US1; T028 is independent of T026/T027.
- Polish last; T030/T031 anytime after their subjects stabilize.

## Parallel opportunities

- Phase 2: T002 ∥ T003 ∥ T004.
- US1: T005 ∥ T008 (core, different files) while T009 (dialog shell) builds.
- US2: T013 ∥ T015 ∥ (T017 → T016) chains; T018 once installers' inputs settle.
- US4 can proceed in parallel with US2/US3 after Phase 2.

## Implementation strategy

MVP first (US1): a user can create-with-defaults and save — ship-worthy alone.
Then US2 (the matrix + bootable, the bulk of new core code), US4 (small,
independent), US3 (dialog completion + pref), polish. Commit + push after each
phase; runtime-validation tasks (T012/T021/T025/T029) gate each phase's close.
