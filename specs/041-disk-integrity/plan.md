# Implementation Plan: Disk integrity

**Branch**: `041-disk-integrity` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/041-disk-integrity/spec.md`

## Summary

Make the emulation thread the single owner of the disk store, its images and the
Disk II drive, give every other thread a published, immutable copy of drive
status instead, route the window's few disk writes through posted commands,
and fix the confirmed write-path, mount and drive-emulation defects on top of
that. Every fix lands with a regression test that fails first.

The design comes from six research decisions ([research.md](research.md)) and
the per-defect evidence in [audit.md](audit.md). It follows 035's own model for
emulation-thread state: build a copy on the owning thread, publish it under a
leaf mutex, read only the copy ([threading-035.md](threading-035.md) §5).

## Technical Context

**Language/Version**: C++ (`/std:c++latest`), MSVC toolset v145 (Visual Studio 2026)

**Primary Dependencies**: Win32 (file I/O behind `IDiskFileIo`), the project's EHM macros, Dxui; no new external dependency

**Storage**: Disk image files (`.woz`, `.dsk`, `.do`, `.po`, `.nib`, `.nb2`) and `UserPrefs.json`, all through existing seams

**Testing**: Microsoft C++ Unit Test Framework (`UnitTest`), the guest-visible `ScenarioTests` suite, and a new opt-in AddressSanitizer build of Debug x64

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: Desktop emulator plus command-line tool, all logic in `CassoEmuCore`

**Performance Goals**: Publishing drive status costs under 1% of a frame at idle (SC-005); no locked read-modify-write on the per-nibble path

**Constraints**: Must merge after 035 (spec, Merge order); must keep 035's reverse execution, flush hold and media retention correct; no user-facing text changes without the owner's approval

**Scale/Scope**: 25 defects plus one found in research, about 40 production files and 25 test files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | How this plan meets it |
|---|---|---|
| I. Code Quality | Pass | Every new function uses EHM with a single exit; no calls inside macro arguments (the ownership assertion takes a member reference, not a call); new helpers are class statics; banners inserted ahead of the `////` block, per CLAUDE.md. `scripts/CheckStyle.ps1 -Mode Tree` runs before the merge |
| II. Testing Discipline | Pass | Every defect gets a failing-first regression test (SC-001). `DurableCommit` is tested through a fake `IDiskFileIo`, so no test touches a real file; the ownership and stress tests use threads, which is process state the test owns, not system state |
| III. User Experience Consistency | Pass, with an approval gate | Three user-facing texts change (the flush-loss message, a new "disk not changed: unsaved writes" mount failure, the eject question). Their wording is drafted in [contracts/user-messages.md](contracts/user-messages.md) and needs the owner's approval before it ships |
| IV. Performance | Pass | The bay table is rebuilt only when a store generation moves; activity is four integers per drive per frame; the engine counters stay plain members because only their owner reads them |
| V. Simplicity | Pass, justified below | Four new types (`ThreadOwnership`, `DriveStatus`, `DriveStatusPublisher`, `DurableCommit`), each replacing scattered ad hoc access; see Complexity Tracking |
| VI. Thin Executable | Pass | Everything is in `CassoEmuCore`; `Casso` and `CassoCli` stay code-free |

Post-design re-check: no new violations. The AddressSanitizer build is a build
property, not a new solution configuration, so the four-configuration gate is
unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/041-disk-integrity/
├── spec.md
├── plan.md              # this file
├── research.md          # Phase 0: six design decisions
├── audit.md             # the confirmed defects, re-checked against 035
├── threading-035.md     # 035's threading model, as the design depends on it
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── internal-interfaces.md
│   └── user-messages.md
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Core/
│   └── ThreadOwnership.h/.cpp            # NEW: owner token, Release/Claim, ASSERT_THREAD_OWNERSHIP
├── Devices/Disk/
│   ├── DriveStatus.h/.cpp                # NEW: BayStatus, DriveActivity, DriveStatus (immutable)
│   ├── DurableCommit.h/.cpp              # NEW: temp, metadata, FlushFileBuffers, replace
│   ├── IDiskFileIo.h                     # + CopyFileMetadata, FlushToStorage, RenameWithoutReplacing
│   ├── DiskImageStore.h/.cpp             # ownership, generation, load-first mount, failed-save invariant,
│   │                                     #   external-change inbox, SetUserWriteProtect, watch on replace,
│   │                                     #   recovery record, salvage on the owner, BuildBayTable
│   ├── DiskImage.h/.cpp                  # raw source refresh after commit, reserved empty slots,
│   │                                     #   TryCreateTrack, LoadState slot-count compatibility
│   ├── MountedImageState.h/.cpp          # per-mount recovery and report record
│   ├── MountDiagnosis.h/.cpp             # MountFailure::UnsavedWrites
│   └── Win32ImageWatcher.cpp             # callback reaches the inbox only
├── Machines/Apple2/Common/
│   ├── Disk2Controller.h/.cpp            # kMaxQuarterTrack 158, ownership token, SoftReset via the store
│   ├── Disk2NibbleEngine.h/.cpp          # create a track on the first written bit over a reserved slot
│   └── WozLoader.cpp                     # reserve empty slots at load
├── Seams/Win32DiskFileIo.h/.cpp          # the three new IDiskFileIo operations
├── Shell/
│   ├── DriveStatusPublisher.h/.cpp       # NEW: publish, take, wake the window
│   ├── MachineChromeFacts.h/.cpp         # NEW: layout facts captured under the lifetime lock
│   ├── DiskManager.h/.cpp                # UpdateDriveWidgets reads the published status; absolute paths
│   ├── EmulatorShellDisks.cpp            # salvage split into posted commands; menu queries
│   ├── EmulatorShellCpuThread.cpp        # publish points, ownership claim and release, new commands
│   ├── EmulatorShellPresent.cpp          # drive-label and scene reads from the status
│   ├── EmulatorShellScene.cpp            # scene drive labels from the status
│   ├── EmulatorShell.cpp                 # shutdown claim; initial publish
│   ├── CpuCommandDispatcher.h/.cpp       # IDM_DISK_SET_USER_WP, IDM_DISK_SALVAGE_*, IDM_DEBUG_ATTACH_SINKS
│   ├── MachineBuilder.cpp                # wire controllers to the store's token
│   ├── ScratchHeatReplayer.cpp, ScratchMachineRenderer.cpp   # claim and release around a run
│   ├── WindowCommandManager.cpp          # mounted-path and occupancy reads from the status
│   └── Window/EmulatorWindow.cpp, EmulatorWindowInput.cpp    # menu queries, tooltips from the status
├── Ui/DriveWidgetController.h/.cpp       # sync-event queue under a mutex
└── Settings/UserConfigStore.h/.cpp       # internal mutex (path per the audit)

UnitTest/
├── EmuTests/   ThreadOwnershipTests, DriveStatusPublisherTests, DriveStatusStressTests,
│               DurableCommitTests, DiskStoreOwnershipTests, plus regressions added to
│               DiskImageStoreTests, DiskWritePathTests, DiskFlushHoldTests, WozLoaderTests,
│               Disk2Tests, NibblizationTests, SharedImageTests
├── Devices/    Disk2StateTests, Disk2ControllerAudioTests (head stop)
└── UiTests/    DriveWidgetHitTests (head range)

ScenarioTests/  a 40-track WOZ read at track 39; a guest format of a WOZ with blank tracks

scripts/        Build.ps1 and RunTests.ps1 gain -Sanitize
Directory.Build.props   CassoSanitize=Address property
```

**Structure Decision**: The existing layout, with new types placed beside the
code they serve: the ownership token in `Core/` (it is not disk-specific),
the published data and the commit in `Devices/Disk/`, and the shell-side
publisher and layout facts in `Shell/`.

## Design

### 1. Ownership (R5)

One `ThreadOwnership` token per `DiskImageStore`, shared with every
`Disk2Controller` wired into that machine. The constructing thread owns it.
Handover is two steps on two threads: the holder calls `Release`, the taker
calls `Claim`. The phases:

- **Startup**: the window thread owns; `Initialize` mounts command-line disks.
- **Handover**: the window releases just before `m_cpuManager.Start`; the first
  statement of `OnCpuThreadStart` claims.
- **Running, paused or not, and machine switch**: the emulation thread.
- **Stop**: the last statement of `OnCpuThreadStop` releases; `~EmulatorShell`
  claims right after `m_cpuManager.Stop()` for the shutdown flush.
- **Scratch machines**: `ScratchHeatReplayer::Run` and
  `ScratchMachineRenderer::Render` claim at entry and release at exit.
- **Tests**: the constructing thread, never handed over.

`ASSERT_THREAD_OWNERSHIP` is the first statement of every guarded entry point
(the list is in R5), expands to EHM `ASSERT` in Debug and to nothing in Release.
It detects; it does not lock. `NoteExternalChange`, the status reader, statics,
constructors and destructors are exempt.

### 2. Published drive status (R1)

`DriveStatus` holds a shared, immutable `BayTable` (16 `BayStatus` records:
mounted, path, format, `WriteProtectInfo` with its causes and damaged quarter
tracks, salvage verdict) and per-drive `DriveActivity` for slot 6 (motor, head
quarter track, read and write nibble counters), plus whether slot 6 has a
controller. No field points into the store, an image or the controller.

- **Generation**: `DiskImageStore::GetStatusGeneration` moves wherever a bay
  field or an image's write-protect cause is written. The bay table is rebuilt
  only when it moves; Debug builds rebuild every time and `ASSERT` the result
  matches, so a missed bump fails the suite.
- **Publish points** (all on the owner): the end of every service pass (P1,
  which covers drained commands and the paused machine), the end of
  `RunCpuThreadFrame` (P2), just before `MountDiskInSlot6` reports completion
  (P3), and once at the end of `Initialize` (P4).
- **Wake**: a publish that changed a bay sets `m_frameReadyEvent`; an
  activity-only publish does not.
- **Readers**: the window takes the newest status once per window frame at the
  top of `UpdateDriveWidgets`, and every window-thread reader uses that copy
  (the list is in R1). The mount-completion handler takes the newest.
- `DriveWidgetController`'s sync queue gets its own leaf mutex (FR-008).

### 3. Window writes become commands (FR-004, FR-005)

- **User write protection**: `IDM_DISK_SET_USER_WP` posts to the owner, which
  calls the new `DiskImageStore::SetUserWriteProtect`: save first when
  protecting, and on a failed save leave the disk writable (FR-015).
- **Salvage**: `RunSalvageFlow` becomes three steps. The window posts
  `IDM_DISK_SALVAGE_ASSESS`; the owner assesses and posts the counts back; the
  window asks the user; on yes it posts `IDM_DISK_SALVAGE_WRITE`, and the owner
  writes the copy and mounts it through the ordinary insert path.
- **Debug sinks**: `IDM_DEBUG_ATTACH_SINKS` makes the owner the only writer of
  device sinks and panel pointers.
- **External changes**: `NoteExternalChange` appends `(path, intent, time)` to an
  inbox under `m_pendingMutex` and touches no `Entry`. The owner drains the
  inbox and matches paths to bays at the start of `ApplyPendingReload`.
- **Layout facts**: `MachineChromeFacts::TryCapture` takes the lifetime lock
  shared and copies the facts layout needs (FR-006).
- **Settings**: `UserConfigStore` serializes its public operations with an
  internal mutex; the window's `GlobalUserPrefs` is touched only by the window
  thread, with emulation-thread saves posted (FR-007).

### 4. Mount, eject and reload (R2)

The invariant: a disk leaves its bay only when every guest write is in a file,
either its own (the save succeeded) or a lossless recovery copy of exactly the
current content when the change goes elsewhere. Otherwise the change is
declined or waits for an answer, and declining calls none of `RetireBay`,
`EmitBayChange` or `NotifyMediaChanged`, so history records nothing.

- **Different file**: read and load the new image into a local first; on
  success, save the old disk, then retire it and swap the new one in (FR-009).
- **Same file**: save the old disk first, then read the file and load, so the
  image loaded is the one just written (FR-009, SC-006).
- **Per action**: the table in R2 (insert, re-insert, reset, power cycle,
  machine switch, eject, reload, salvage insert, shutdown, load state).
- **Watch** (FR-011): replacing a bay's disk ends the old watch when no other
  bay uses that folder.
- **Flux burst** (FR-012): the reload commits the open flux write against the
  outgoing image before its dirty test.
- **Paths** (FR-013): command-line disk paths are made absolute before mounting.

### 5. Write path (R3)

- **Durable commit** (FR-018): `DurableCommit` runs write-temporary, copy
  metadata from the target, `FlushToStorage`, `ReplaceAtomically` (or
  `RenameWithoutReplacing` for copies that must not overwrite) through
  `IDiskFileIo`. The store, the CLI and Cassque share it.
- **One recovery copy** (FR-016): `MountedImageState` keeps the recovery path,
  identity, image id and track generations written, and which reports were
  shown. A later failure rewrites that copy (or nothing, if unchanged) and
  shows nothing more. Cleared on mount, eject, a successful save, and an
  external reload.
- **`.nib` re-save** (FR-014): after a successful commit the image's raw
  source bytes are replaced with the bytes just written.
- **Reset** (FR-017): `Disk2Controller::SoftReset` stops calling `DiskImage::Flush`
  on store-owned images; the store's own reset flush covers them.

### 6. Drive emulation (R4)

- **Head stop** (FR-020): `Disk2Controller::kMaxQuarterTrack` becomes 158; every
  reader follows the constant, including the end-stop sound.
- **Guest-created tracks** (FR-021): `WozLoader::Load` reserves an empty bit slot
  for every whole track 0..39 whose position has no data, mapped at qt/4
  placement; the first bit the sequencer writes over such a slot sizes it to
  51,200 bits. The slot count is fixed at mount, so 035's keyframes and
  `LoadState` are unaffected; `LoadState` accepts a state with fewer slots when
  every surplus live slot is an empty bit slot.

### 7. Proof (R6)

The ownership assertion, a drive-status stress test run in Debug, Release and
the AddressSanitizer build, and ASan itself. `scripts/Build.ps1 -Sanitize` and
`scripts/RunTests.ps1 -Build -Sanitize` drive the ASan build into
`x64\DebugAsan\`.

### 8. After 035 reaches `master`

Merge `master`, then apply FR-019: an acknowledged park in `CpuManager`
(`Park`/`Unpark`/`IsParked`) so the update installer's final save runs with the
emulation thread stopped, then re-run every gate (spec, Merge order).

## Order of work

1. Ownership token and assertion, wired but with every current off-thread
   caller still in place (the assertion fires; the suite shows where).
2. Published status and the window readers moved to it; the drive widget queue.
3. The window's writes as commands; the external-change inbox; layout facts;
   the settings lock. The assertion now passes.
4. Mount, eject and reload correctness.
5. The write path.
6. Drive emulation.
7. Stress test, ASan build, canary.
8. Documentation (FR-022), then the gates.
9. After 035 lands: `master` merge, FR-019, gates again.

Each defect's regression test is written first and seen to fail.

## Complexity Tracking

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| `ThreadOwnership` | SC-003 needs a check at every entry point, and the owner changes hands at runtime | A thread id compared against `GetCurrentThreadId` in place cannot follow the handovers |
| `DriveStatus` + `DriveStatusPublisher` | FR-002 requires a copy consistent within itself across many fields | Per-field atomics and locks are safe per read but not across reads |
| `DurableCommit` | FR-018 for the store, and the CLI already has half the sequence | Patching `WriteFileAtomically` alone leaves two different commit sequences |
| AddressSanitizer build property | SC-002 needs a detector for use-after-free and torn strings | A new solution configuration would grow the four-configuration gate |
