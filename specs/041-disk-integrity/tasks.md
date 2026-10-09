---

description: "Task list for 041 disk integrity"
---

# Tasks: Disk integrity

**Input**: Design documents from `specs/041-disk-integrity/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [audit.md](audit.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: Required. SC-001 asks for a regression test per defect that fails
before its fix and passes after, so every fix below is preceded by its test,
and the test is run and seen to fail before the fix is written.

**Defect ids** in brackets (for example `[failed-mount-dangling-disk-image]`)
are the headings in [audit.md](audit.md); each one holds the evidence, the
035 impact, the proposed fix and the proposed regression test. `R1`..`R6` are
the decisions in [research.md](research.md).

**Merge order**: this branch merges to `master` only after 035 has (spec, Merge
order). Phase 9 is the work that waits for that.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an unfinished task)
- **[Story]**: the user story in [spec.md](spec.md)

## Phase 1: Setup

- [X] T001 Record the baseline on 035 at `811a6f727`: Debug 9,525 passing, Release 9,519 passing (`scripts/RunTests.ps1`), in this file's header and in quickstart.md
- [ ] T002 Add the opt-in AddressSanitizer build per R6: a `CassoSanitize=Address` property in `Directory.Build.props` (output to `x64\DebugAsan\`), and `-Sanitize` on `scripts/Build.ps1` and `scripts/RunTests.ps1`; document it in `docs/testing.md`

---

## Phase 2: Foundational (blocks every story)

**Purpose**: the ownership rule and the published status. The window's readers
cannot move off the live state until the status exists, and the assertion is
what proves they moved.

- [ ] T003 [P] Write `UnitTest/EmuTests/ThreadOwnershipTests.cpp` per R5's test plan: the constructing thread owns; `Release` then `Claim` on another thread hands over; `Claim` while owned and `Release` off the owner assert; `IsHeldByCurrentThread` across threads. Confirm it fails to build or fails
- [ ] T004 Implement `ThreadOwnership` and `ASSERT_THREAD_OWNERSHIP` in `CassoEmuCore/Core/ThreadOwnership.h/.cpp` per contracts/internal-interfaces.md (Debug: EHM `ASSERT` at the call site; Release: nothing); add both files to `CassoEmuCore/CassoEmuCore.vcxproj` and the test to `UnitTest/UnitTest.vcxproj`; T003 passes
- [ ] T005 Give `DiskImageStore` its token (`GetThreadOwnership`) and `Disk2Controller` a `SetThreadOwnership`, wire each controller to its machine's store token in `CassoEmuCore/Shell/MachineBuilder.cpp`, and put `ASSERT_THREAD_OWNERSHIP` first in every entry point R5 lists, in `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp` and `CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h/.cpp`; exempt the entry points R5 exempts
- [ ] T006 Add the handovers R5's phase table gives: release before `m_cpuManager.Start` in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp`; claim first in `OnCpuThreadStart` and release last in `OnCpuThreadStop` in `CassoEmuCore/Shell/EmulatorShellCpuThread.cpp`; claim after `m_cpuManager.Stop()` in `~EmulatorShell` in `CassoEmuCore/Shell/EmulatorShell.cpp`; claim and release around `ScratchHeatReplayer::Run` and `ScratchMachineRenderer::Render`
- [ ] T007 Run the full Debug suite and list every ownership assertion that fires; each is a call site Phases 3 to 5 move (record the list in this file under T007)
- [ ] T008 [P] Write `UnitTest/EmuTests/DriveStatusPublisherTests.cpp` per R1's test plan: the bay table equals the live bays after mount, eject, swap, write-protect change, salvage-verdict change and a 035 `SeatMedia`; the table is shared while the generation stands; an activity-only publish wakes nothing; a bay change wakes the window. Confirm it fails
- [ ] T009 Implement `BayStatus`, `DriveActivity`, `BayTable` and `DriveStatus` in `CassoEmuCore/Devices/Disk/DriveStatus.h/.cpp`, and `DiskImageStore::GetStatusGeneration` and `CaptureBayTable`, with the generation bumped at every bay-field and write-protect-cause write R1 lists (including 035's `SeatMedia`, `RetireBay`, `DiskImage::LoadState` and `Replayer::ApplyInput` paths) and the Debug rebuild-and-compare check
- [ ] T010 Implement `DriveStatusPublisher` in `CassoEmuCore/Shell/DriveStatusPublisher.h/.cpp` and the publish points P1 (end of every service pass), P2 (end of `RunCpuThreadFrame`), P3 (before `MountDiskInSlot6` reports completion, `CassoEmuCore/Shell/DiskManager.cpp`) and P4 (end of `Initialize`), with the bay-change wake on `m_frameReadyEvent`; T008 passes

**Checkpoint**: the token, the assertion and the status exist; the window still reads live state, which T007's list records.

---

## Phase 3: User Story 1 - What the guest saves stays saved (P1)

**Goal**: no path loses the guest's writes, and every save is durable.

**Independent test**: the write-path regressions below, each comparing the
image file byte-for-byte with what the guest wrote.

### Tests (write first, see each fail)

- [ ] T011 [P] [US1] `[nib-reflush]` regression in `UnitTest/EmuTests/DiskWritePathTests.cpp`: mount a `.nib`, write track 5, flush, write track 10, flush, read the file; track 5 must hold the first write
- [ ] T012 [P] [US1] `[remount-discards-dirty]` regressions in `UnitTest/EmuTests/DiskWritePathTests.cpp`, one per action in R2's table (insert of another file, re-insert, reset, power cycle, machine switch, eject, salvage insert), each with the save forced to fail through the fake file I/O; the writes must survive and the bay must keep the disk
- [ ] T013 [P] [US1] Same-file remount ordering regression (research.md "New defect found during research"; SC-006) in `UnitTest/EmuTests/DiskWritePathTests.cpp`: a dirty disk reset before the motor stops ends with the writes in both the file and the mounted image
- [ ] T014 [P] [US1] `[settings-wp-drops-dirty]` regression in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: unsaved writes, then user write protection; the file must hold the writes; a failed save must leave the disk writable with them
- [ ] T015 [P] [US1] `[recovery-file-per-spindown]` regression in `UnitTest/EmuTests/DiskWritePathTests.cpp`: an undecodable sector image flushed three times leaves one recovery copy with the latest content and reports once
- [ ] T016 [P] [US1] `[softreset-second-write-path]` regression in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: a reset after a failed remount writes nothing that bypasses `FlushEntry` (no write to `DiskImage::m_filePath`)
- [ ] T017 [P] [US1] `UnitTest/EmuTests/DurableCommitTests.cpp` per R3 with a fake `IDiskFileIo`: the call order (write temporary, copy metadata, flush to storage, replace), the temporary removed on every failure, `CreateNew` never replacing, metadata copied only when the target exists

### Implementation

- [ ] T018 [US1] FR-014: replace the image's raw source bytes with the committed bytes after each successful save, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp` (after `WriteFileAtomically` succeeds) and `DiskImage::Flush` in `CassoEmuCore/Devices/Disk/DiskImage.cpp`; T011 passes
- [ ] T019 [US1] FR-009 and FR-010: restructure `DiskImageStore::Mount`, `MountFromBytes` and `MountRestored` in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp` per R2 (different file: load into a local, save the old disk, then `RetireBay` and swap; same file: save first, then read and load; a failed save declines the change and calls none of `RetireBay`, `EmitBayChange`, `NotifyMediaChanged`); add `MountFailure::UnsavedWrites` in `CassoEmuCore/Devices/Disk/MountDiagnosis.h/.cpp`
- [ ] T020 [US1] FR-010 per action: apply R2's table to eject (`DiskImageStore::Eject`, the existing `EjectWhenAnswered` question), reset and power cycle (`DiskManager::RemountSlot6Disks` in `CassoEmuCore/Shell/DiskManager.cpp`), machine switch (`CassoEmuCore/Shell/MachineManager.cpp`, the store survives), salvage insert and load state (`CassoEmuCore/Shell/EmulatorShellState.cpp`); T012 and T013 pass
- [ ] T021 [US1] FR-015: add `DiskImageStore::SetUserWriteProtect` (save first when protecting; a failed save leaves it writable) in `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp`, add `IDM_DISK_SET_USER_WP` to `CassoEmuCore/resource.h` and `CassoEmuCore/Shell/CpuCommandDispatcher.h/.cpp`, and post it from `EmulatorShell::SetDriveUserWriteProtect` in `CassoEmuCore/Shell/EmulatorShellDisks.cpp` instead of touching the image; T014 passes
- [ ] T022 [US1] FR-016: the per-mount recovery and report record in `CassoEmuCore/Devices/Disk/MountedImageState.h/.cpp` per R3, used by `TryWriteRecoveryImage` and the report cadence in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, cleared on mount, eject, a successful save and an external reload; T015 passes
- [ ] T023 [US1] FR-017: stop `Disk2Controller::SoftReset` from calling `DiskImage::Flush` on store-owned images in `CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp`, relying on the store's reset flush; T016 passes
- [ ] T024 [US1] FR-018: add `CopyFileMetadata`, `FlushToStorage` and `RenameWithoutReplacing` to `CassoEmuCore/Devices/Disk/IDiskFileIo.h` and implement them in `CassoEmuCore/Seams/Win32DiskFileIo.h/.cpp`; implement `DurableCommit` in `CassoEmuCore/Devices/Disk/DurableCommit.h/.cpp`; route the store's image, recovery and preserved-copy writes and the CLI's `DiskImageSession` commit through it; update every test fake of `IDiskFileIo`; T017 passes
- [ ] T025 [US1] Implement the three new texts in contracts/user-messages.md (after the owner approves their wording) in `CassoEmuCore/Devices/Disk/MountDiagnosis.cpp`, `CassoEmuCore/Devices/Disk/ChangePrompt.cpp` and `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`, with tests of the composed text in `UnitTest/EmuTests/`

**Checkpoint**: every US1 regression passes; the full Debug suite passes.

---

## Phase 4: User Story 2 - Disk operations never crash (P1)

**Goal**: no thread but the owner touches disk state, and a failed mount keeps the old disk.

**Independent test**: the US2 regressions, and a Debug suite in which no ownership assertion fires.

### Tests (write first, see each fail)

- [ ] T026 [P] [US2] `[failed-mount-dangling-disk-image]` regressions in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: an unloadable file inserted into an occupied drive leaves the old image mounted and attached to the controller; repeated with media retention on, nothing is retired
- [ ] T027 [P] [US2] `[flux-burst-spliced-into-reloaded-image]` regression in `UnitTest/EmuTests/DiskImageStoreTests.cpp`: an open flux write, then an external reload; the reloaded image holds none of the burst
- [ ] T028 [P] [US2] `[note-external-change-reads-entry-unlocked]` regression in `UnitTest/EmuTests/SharedImageTests.cpp`: `NoteExternalChange` from another thread touches no bay (the ownership assertion stays silent) and the owner matches it on the next `ApplyPendingReload`
- [ ] T029 [P] [US2] `[drive-widget-sync-events-unlocked]` regression in `UnitTest/UiTests/`: concurrent publish and consume of `DriveWidgetController` sync events loses none
- [ ] T030 [P] [US2] `[salvage-mount-on-ui-thread]` and `[salvage-decode-on-ui-thread]` regressions: the salvage commands run assessment, write and insert on the owner, in `UnitTest/EmuTests/DiskImageStoreTests.cpp` and the shell's command tests
- [ ] T031 [P] [US2] `[debug-sink-attach-race]` regression: device sinks change only through `IDM_DEBUG_ATTACH_SINKS` on the owner, in the existing debug-panel tests under `UnitTest/`

### Implementation

- [ ] T032 [US2] T019's restructure makes T026 pass; confirm, and add the retention case to `ReverseController` tests if T026 needs a reverse fixture
- [ ] T033 [US2] FR-012: commit or discard the open flux write against the outgoing image before the dirty test in `DiskImageStore::ApplyPendingReloadToBay` and `MountExternallyModifiedDisk`, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`; T027 passes
- [ ] T034 [US2] FR-005: replace `NoteExternalChange`'s bay walk with an inbox under `m_pendingMutex`, drained and matched on the owner at the start of `ApplyPendingReload`, in `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp`; T028 passes
- [ ] T035 [US2] FR-008: guard `DriveWidgetController`'s sync events and id counter with a leaf mutex in `CassoEmuCore/Ui/DriveWidgetController.h/.cpp`; T029 passes
- [ ] T036 [US2] FR-004: split `RunSalvageFlow` into `IDM_DISK_SALVAGE_ASSESS`, `WM_APP_SALVAGE_ASSESSED` and `IDM_DISK_SALVAGE_WRITE` per contracts/internal-interfaces.md, in `CassoEmuCore/resource.h`, `CassoEmuCore/Shell/CpuCommandDispatcher.h/.cpp`, `CassoEmuCore/Shell/EmulatorShellDisks.cpp`, `CassoEmuCore/Shell/EmulatorShellCpuThread.cpp` and `CassoEmuCore/Shell/Window/EmulatorWindow.cpp`; T030 passes
- [ ] T037 [US2] FR-004: add `IDM_DEBUG_ATTACH_SINKS` per `[debug-sink-attach-race]`'s proposed fix and post it wherever a debug panel opens or closes, in `CassoEmuCore/Shell/EmulatorShellDebug.cpp`, `CassoEmuCore/Shell/CpuCommandDispatcher.h/.cpp` and `CassoEmuCore/Shell/EmulatorShellCpuThread.cpp`; T031 passes
- [ ] T038 [US2] FR-002: move `DiskManager::UpdateDriveWidgets` to the published status (take once per window frame, keep it as the shown status) in `CassoEmuCore/Shell/DiskManager.cpp`; `[engine-counters-not-atomic]` is resolved because the engine's only reader is now its owner
- [ ] T039 [P] [US2] FR-002: move the drive-label and scene reads in `CassoEmuCore/Shell/EmulatorShellPresent.cpp` and `CassoEmuCore/Shell/EmulatorShellScene.cpp` to the shown status
- [ ] T040 [P] [US2] FR-002: move the tooltip reads in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp` and the Disk-menu label and enable queries in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` and `CassoEmuCore/Shell/EmulatorShellDisks.cpp` to the shown status (`[ui-derefs-store-diskimage]`, `[entry-path-string-race]`, `[entry-flags-race]`)
- [ ] T041 [P] [US2] FR-002: move `WindowCommandManager`'s mounted-path and occupancy reads in `CassoEmuCore/Shell/WindowCommandManager.cpp` to the shown status
- [ ] T042 [US2] Run the full Debug suite; no ownership assertion fires, and T007's list is empty; record that here

**Checkpoint**: US1 and US2 regressions pass; the Debug suite runs with the ownership rule enforced.

---

## Phase 5: User Story 3 - Drive status and external changes are accurate (P2)

**Goal**: relative mounts are watched, replaced disks stop being watched, and layout reads consistent facts.

**Independent test**: the US3 regressions.

### Tests (write first, see each fail)

- [ ] T043 [P] [US3] `[relative-disk1-no-watch]` regression in the command-line mount tests under `UnitTest/EmuTests/`: a relative `--disk1` path is mounted as an absolute path and its folder is watched
- [ ] T044 [P] [US3] `[watch-leak]` regression in `UnitTest/EmuTests/SharedImageTests.cpp`: swapping a disk for one in another folder ends the old folder's watch; a folder still used by another bay stays watched
- [ ] T045 [P] [US3] `[layout-reads-machine-refs-unlocked]` regression: `MachineChromeFacts::TryCapture` leaves the facts unchanged when the lifetime lock is held exclusively, in a new `UnitTest/EmuTests/MachineChromeFactsTests.cpp`

### Implementation

- [ ] T046 [US3] FR-013: make command-line disk paths absolute before mounting, in `CassoEmuCore/Shell/DiskManager.cpp` (`MountCommandLineDisks`); T043 passes
- [ ] T047 [US3] FR-011: end the old watch when a mount replaces a bay's disk, in `CassoEmuCore/Devices/Disk/DiskImageStore.cpp`; T044 passes
- [ ] T048 [US3] FR-006: add `MachineChromeFacts` in `CassoEmuCore/Shell/MachineChromeFacts.h/.cpp` per contracts/internal-interfaces.md and switch the window layout paths `[layout-reads-machine-refs-unlocked]` lists to it; T045 passes

**Checkpoint**: US1 to US3 regressions pass.

---

## Phase 6: User Story 4 - Forty-track disks and blank tracks work (P2)

**Goal**: the head reaches track 39; the guest can format a WOZ's blank tracks.

**Independent test**: the US4 unit regressions and the two scenario tests.

### Tests (write first, see each fail)

- [ ] T049 [P] [US4] `[head-clamp-139]` regressions per R4: stepping outward stops at quarter track 158 and every stop is even, in `UnitTest/EmuTests/Disk2Tests.cpp`; the end-stop bump fires one step past 156 in `UnitTest/Devices/Disk2ControllerAudioTests.cpp`; update the assertions of 139 in `UnitTest/Devices/Disk2StateTests.cpp` and `UnitTest/UiTests/DriveWidgetHitTests.cpp`
- [ ] T050 [P] [US4] `[unmapped-qt-writes-discarded]` regressions per R4: `WozLoader::Load` reserves an empty bit slot for every unmapped whole track 0..39 with qt/4 placement (`UnitTest/EmuTests/WozLoaderTests.cpp`); the first written bit sizes the slot to 51,200 bits and only on a writable WOZ (`UnitTest/EmuTests/DiskImageTests.cpp`); `LoadState` accepts a state with fewer slots when every surplus live slot is an empty bit slot (`UnitTest/Devices/Disk2StateTests.cpp`); a save and reload reproduce the reservation
- [ ] T051 [P] [US4] Scenario tests in `ScenarioTests/`: a 40-track WOZ read at track 39; a guest format of a WOZ with blank tracks, then the saved file read back with every track mapped

### Implementation

- [ ] T052 [US4] FR-020: set `Disk2Controller::kMaxQuarterTrack` to 158 in `CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h`, as R4 describes; T049 passes
- [ ] T053 [US4] FR-021: reserve empty bit slots in `WozLoader::Load` (`CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`); add `DiskImage::TryCreateTrack` and the `LoadState` compatibility rule (`CassoEmuCore/Devices/Disk/DiskImage.h/.cpp`); create on the first written bit in `Disk2NibbleEngine::StepLss` (`CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp`), with a `static_assert` tying the length to `NibblizationLayer::kTrackBitCapacity`; T050 and T051 pass

**Checkpoint**: US1 to US4 regressions pass, including the scenario suite.

---

## Phase 7: User Story 5 - Settings are saved consistently (P3)

- [ ] T054 [P] [US5] `[user-config-store-cross-thread]` regression: concurrent `SaveDelta` calls from two threads both land and the combined file parses, in the `UserConfigStore` tests under `UnitTest/`
- [ ] T055 [US5] FR-007: add the internal mutex to `UserConfigStore` per the audit's proposed fix, and post emulation-thread changes to the window's `GlobalUserPrefs` instead of writing them, in the settings store and its callers in `CassoEmuCore/Shell/`; T054 passes

---

## Phase 8: Polish and cross-cutting

- [ ] T056 [P] SC-002: `UnitTest/EmuTests/DriveStatusStressTests.cpp` per R6 (10,000 operations on an owner thread, a concurrent reader, the four pass conditions), and `UnitTest/EmuTests/SanitizerCanaryTests.cpp` reading the live bays, expected to fail only in the AddressSanitizer build; record the canary's failing run in the commit message
- [ ] T057 Run `scripts/RunTests.ps1 -Build -Sanitize`; the stress test passes and AddressSanitizer reports nothing
- [ ] T058 [P] SC-005: measure the publish cost at idle in `UnitTest/EmuTests/PerformanceTests.cpp`; under 1% of a frame
- [ ] T059 [P] FR-022: correct `[stale-step-comments]` and `[stale-thread-ownership-comments]`, and the comments R1 to R5 found wrong (`CassoEmuCore/Devices/Disk/DiskImageStore.h`, `CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h`, and every comment in the disk and shell files that claims an ownership, atomicity or flush trigger the code does not have)
- [ ] T060 [P] FR-022: correct `docs/disk-write-integrity.md` (the stale items research.md and the audit list) and `ARCHITECTURE.md` §2 and §5; §8 lands on `master` from the `docs-refresh` branch and is updated in Phase 9
- [ ] T061 Report the two 035 findings to the 035 session, with the owner's approval: reverse execution's replay flag surviving `Stop` (threading-035.md §3) and `TogglePaused` with no acknowledgement (§1)
- [ ] T062 Gates on this branch: full Debug and Release suites (count = baseline + the tests added), `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, ARM64 build, `scripts/CheckStyle.ps1 -Mode Tree`
- [ ] T063 On-screen check per quickstart.md "On screen", launched in the background with `--title 041-disk-integrity`; screenshots for the owner

---

## Phase 9: After 035 reaches `master`

- [ ] T064 Merge `master` into this branch (it then holds 035's own resolution); re-read `.github/copilot-instructions.md` for new rules
- [ ] T065 FR-019: add `CpuManager::Park`, `Unpark` and `IsParked` per contracts/internal-interfaces.md and park before the update installer's final save in `CassoEmuCore/Shell/EmulatorShellUpdate.cpp`, with its regression test (`[update-flush-with-cpu-running]` in audit.md)
- [ ] T066 Update `ARCHITECTURE.md` §8 (Disks) for the ownership rule and the published status
- [ ] T067 CHANGELOG entry, written last and approved by the owner after testing
- [ ] T068 Re-run every T062 gate, then merge to `master` with `--no-ff` and watch CI to completion

---

## Dependencies and execution order

- **Phase 1 → Phase 2 → stories.** Phase 2 blocks everything: the status must
  exist before readers move, and the assertion proves they moved.
- **US1 before US2.** T019 (the mount restructure) serves both; T032 only
  confirms it.
- **US2 before the ownership gate.** T042 closes Phase 2's T007 list.
- **US3, US4 and US5** depend only on Phase 2 and can run in any order, or in
  parallel with each other, after US2.
- **Phase 8** after all stories. **Phase 9** only after 035 has merged to `master`.

### Parallel opportunities

- Each story's test tasks marked [P] are in different files and can be written together.
- T039, T040 and T041 touch different files once T038 settles the shown-status API.
- US3, US4 and US5 as whole phases, once US2 is done.

## Implementation strategy

There is no partial delivery: the branch ships whole, after 035 (spec, Merge
order). The order above is build order. Each phase ends with the full Debug
suite green, so a stop at any checkpoint leaves a working tree.
