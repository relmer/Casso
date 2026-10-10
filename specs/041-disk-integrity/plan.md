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
leaf mutex, read only the copy ([threading-035.md](threading-035.md) §5). An
adversarial review of the plan (2026-10-08) changed several parts of it; the
corrections it did not take are listed at the end.

## Technical Context

**Language/Version**: C++ (`/std:c++latest`), MSVC toolset v145 (Visual Studio 2026)

**Primary Dependencies**: Win32 (file I/O behind `IDiskFileIo`), the project's EHM macros, Dxui; no new external dependency

**Storage**: Disk image files (`.woz`, `.dsk`, `.do`, `.po`, `.nib`, `.nb2`) and `UserPrefs.json`, all through existing seams

**Testing**: Microsoft C++ Unit Test Framework (`UnitTest`), the guest-visible `ScenarioTests` suite (part of every phase gate), and a new opt-in AddressSanitizer build of Debug x64

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: Desktop emulator plus command-line tool, all logic in `CassoEmuCore`

**Performance Goals**: Publishing drive status costs under 1% of a frame at idle (SC-005); no locked read-modify-write on the per-nibble path; the spin-down commit's added `FlushFileBuffers` is measured before the mount design depends on a synchronous save (tasks.md T062)

**Constraints**: Must merge after 035 (spec, Merge order); must keep 035's reverse execution, flush hold and media retention correct; no user-facing text changes without the owner's approval

**Scale/Scope**: 25 defects plus one found in research, about 45 production files and 30 test files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | How this plan meets it |
|---|---|---|
| I. Code Quality | Pass | Every new function uses EHM with a single exit. No calls inside macro conditions or arguments: the ownership check takes a member, a dereference or a hoisted local reference, and goes after the declaration block. The snippets in research.md and audit.md are designs, not code to paste; every macro condition in them is rewritten with its call hoisted into a descriptive `bool` local and checked against CS0011 before it is committed. Functions already over 100 lines, and `DiskManager::OnBayChange` at 99, get an extraction task, or a split as the first step of the task that first adds to them, before any new logic (tasks.md, Rules for every task, lists every one a task edits, comment-only edits included); the exceptions, a few statements at fixed points in four long startup, shutdown and load functions (three in `EmulatorShell::Initialize`, one in each of the others) and one ownership check in `DiskImageStore::ResolvePendingChange` before its split, are in Complexity Tracking. New helpers are class statics; banners are inserted ahead of the `////` block, per CLAUDE.md. `scripts/CheckStyle.ps1 -Mode Staged` runs before every commit and `-Mode Tree` before the merge |
| II. Testing Discipline | Pass | Every defect gets a failing-first regression test (SC-001), except the five items tasks.md records with the check that closes each. After 041 no unit test creates, writes, renames, deletes or watches a real file or folder, or reads a file outside the checkout, other than the Harte tests, which the owner left in the unit suite (owner decision, 2026-10-09): tasks.md T057 moves every unit test that does, 65 at the inventory of 2026-10-09 (among them `Win32DiskFileIoTests`, `Win32ImageWatcherTests`, the `WriteFileAtomically_*` and `Flush_ToRealFile_WritesThroughAtomicPath` cases of `DiskImageStoreTests.cpp:765-857` and the three real-folder cases of `SharedImageTests.cpp:2200-2302`), into the scenario suite, where copilot-instructions.md allows temp files ("only in integration tests, never in unit tests"), and the seam's real-file cases (R3's six, or those 040's cherry-picked commit lacks, tasks.md T054) go there too. Unit tests still read checked-in fixtures (the fixture ROMs `FixtureProvider` reads, as every `TestMachine` test does, golden images, docs); the Harte tests and a few probes of paths that do not exist stay in the unit suite by owner decision (2026-10-09; second revision, below). The tests this work adds touch no real file: `DurableCommit` and the store's commits are tested through `FakeDiskFileIo`, the disk tests use disks with no file behind them, the shell tests in `DiskWritePathTests` install a recording flush sink and an image reader on the shell's store and mount with `MountFromBytes` (tasks.md T092, T093), the shell's state-load test passes the state as bytes (T091), the CLI's base directory comes from a fake `IDiskFileIo` (T073), and the `DiskManager` rig reads no saved disk path and passes a synthetic base. The store tests that run through `FakeDiskFileIo` with no sink would reach the real file system against the code before their fix, so they are written with the fix and seen to fail under mutations instead (tasks.md, SC-001 exceptions). No constitution exception is needed and no approval is pending (owner decisions, 2026-10-08 and 2026-10-09). The consequences: CI runs only `UnitTest.dll` (`.github/workflows/ci.yml:73`), so it stops running the moved cases, and they run under `scripts/RunTests.ps1 -Scenario`, which every phase gate runs; the unit count falls by the moved cases in Debug and in Release. Threads in tests are process state the test owns. The tests that run a real `CpuManager` (tasks.md T012, T128, T129) use its thread, its waitable timer and its COM setup, as `CpuManagerCommandTests` already does; no other test uses a kernel object, because the drive-status wake is a callable the rig counts |
| III. User Experience Consistency | Pass, with an approval gate | About twenty user-facing texts are added or changed, and [contracts/user-messages.md](contracts/user-messages.md) lists every one. The owner approves them in Phase 1 (tasks.md T005), before any task composes or asserts one |
| IV. Performance | Pass | The bay table is rebuilt only when a store generation moves; activity is four integers per drive per frame; the engine counters stay plain members because only their owner reads them; the commit's flush cost is measured on slow media before the restructure (T062) |
| V. Simplicity | Pass, justified below | Four new types (`ThreadOwnership`, `DriveStatus`, `DriveStatusPublisher`, `DurableCommit`), each replacing scattered ad hoc access, plus `DeploySave` for the update installer's save behind live; user write protection reuses its existing command; see Complexity Tracking |
| VI. Thin Executable, Testable Core | Pass | Everything is in `CassoEmuCore`; `Casso` and `CassoCli` stay code-free. Under the Testability Litmus, new logic is reachable from `UnitTest`: FR-003's wake is a callable `DiskManager::PublishDriveStatus` calls on a bay change, which the `DiskManager` rig counts (tasks.md T030); the deploy's notice decision and its return-to-live mapping are `DeploySave` statics with table tests (T129). What only the Debug app run shows is glue with no decision of its own: `ServiceCpuThread`'s three calls, the order at the three update call sites (T139: the MSIX deploy, the zip update's `WM_CLOSE` and `OnDestroy` with an update pending, whose return to live and quit save T129's `AReturnToLiveBeforeQuitSavesTheLiveEndsDisk` covers), the one real working-directory read behind the `IDiskFileIo` seam (`Win32DiskFileIo::GetWorkingDirectory`, T073), and the window code around modal dialogs and debug panel windows, whose data the regressions pin (tasks.md, Ownership move list) |

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
│   ├── ThreadOwnership.h/.cpp            # NEW: owner token, Release/Claim, ASSERT_THREAD_OWNERSHIP
│   └── PathResolver.h/.cpp               # MakeAbsolutePath (path, baseDirectory)
├── Config/
│   ├── UserConfigStore.h/.cpp            # internal lock, GetPrefsForCaller, UpdateUiPrefs
│   └── DiskSettings.cpp                  # WriteSavedUiPrefs uses UpdateUiPrefs
├── Cli/
│   ├── DebugBatchRunner.h/.cpp           # IDiskFileIo for --write-disks, SetFileBacked, shutdown flush
│   └── ImageArtifactSink.cpp             # states the absolute path
├── Debugger/Reverse/Replayer.cpp         # the flag-only write-protect setter
├── Devices/Disk/
│   ├── DriveStatus.h/.cpp                # NEW: BayStatus, DriveActivity, DriveStatus (immutable)
│   ├── DurableCommit.h/.cpp              # NEW (040's commit, cherry-picked, T054): CommitMode; temporary,
│   │                                     #   metadata, FlushFileBuffers, replace
│   ├── CommitPlan.h/.cpp                 # CopyMetadata and FlushTemporary steps
│   ├── IDiskFileIo.h                     # + CopyFileMetadata, FlushToStorage, RenameWithoutReplacing (040's);
│   │                                     #   + GetWorkingDirectory (041's, T073)
│   ├── DiskImageStore.h/.cpp             # ownership, generation, FlushEntry split, load-first mount,
│   │                                     #   same-file save-first, FlushMoment, external-change inbox,
│   │                                     #   SetUserWriteProtect and the flag-only setters, watch on
│   │                                     #   replace, recovery record, CommitFile, salvage media id
│   ├── DiskImage.h/.cpp                  # MarkSaved, reserved empty slots, TryCreateTrack,
│   │                                     #   LoadState slot-count compatibility, GetTrackGenerations
│   ├── DiskImageSession.cpp              # the CLI's commit through DurableCommit
│   ├── DiskTrackSnapshot.cpp             # layout generation bump in Restore
│   ├── DiskCommandRunner.cpp             # states the absolute path
│   ├── ChangePrompt.h/.cpp               # imagePath, suggestedSavePath, new causes, ComposeReplacedReport
│   ├── MountedImageState.h/.cpp          # recovery record, report flag, asked cause
│   └── MountDiagnosis.h/.cpp             # UnsavedWrites, BehindLive, ShouldReport
├── Machines/Apple2/Common/
│   ├── Disk2Controller.h/.cpp            # kMaxQuarterTrack 158, ownership token, SoftReset only resets
│   ├── Disk2NibbleEngine.h/.cpp          # StepLss split; create a track on the first written bit
│   └── WozLoader.h/.cpp                  # reserve empty slots at load; the track-record rule shared with spec 040
├── Seams/Win32DiskFileIo.h/.cpp          # the three new IDiskFileIo operations (040's) and GetWorkingDirectory
├── Shell/
│   ├── DriveStatusPublisher.h/.cpp       # NEW: Publish returns DriveStatusChange; Take
│   ├── MachineChromeFacts.h/.cpp         # NEW: layout facts captured under the lifetime lock
│   ├── DiskManager.h/.cpp                # UpdateDriveWidgets and OnBayChange split; the shown status;
│   │                                     #   the status wake; P3; RestoreDoor; IsReplacePromptNeeded;
│   │                                     #   base directory; MountDiskForCommand and the behind-live declines
│   ├── EmulatorShellDisks.cpp            # salvage offer and outcome; damage report from MountCompletion;
│   │                                     #   AskAboutChange; menu queries; SetDriveUserWriteProtect
│   ├── EmulatorShellCpuThread.cpp        # P2, ownership claim and release, new command overrides;
│   │                                     #   MountDisk calls MountDiskForCommand
│   ├── EmulatorShellPresent.cpp, EmulatorShellScene.cpp      # drive labels from the shown status
│   ├── EmulatorShellDebug.cpp            # debug panels attach through a posted command
│   ├── EmulatorShellState.cpp            # LoadMachineStateBytes; SaveDisksFirst before the switch
│   ├── EmulatorShellChrome.cpp           # layout facts; LayoutSwitchBar
│   ├── EmulatorShell.h/.cpp              # ServiceCpuThread (P1), the status wake, P4, shutdown claim,
│   │                                     #   shown status, chrome facts, CPU-only panel copies
│   ├── EmulatorShellReverse.cpp          # GoLiveForDeploy: the deploy's return to live
│   ├── DeploySave.h/.cpp                 # NEW: the deploy's return to live, its held save and its notice rule
│   ├── CpuCommandDispatcher.h/.cpp       # Dispatch split; IDM_DISK_SALVAGE*, IDM_DEBUG_ATTACH_SINKS,
│   │                                     #   IDM_DEPLOY_GO_LIVE
│   ├── CpuManager.h/.cpp                 # ThreadProc split; the deploy hold (HoldForDeploy, ReleaseDeployHold,
│   │                                     #   IsHeldForDeploy) with the ownership handover, on 035's pause park
│   ├── MachineHost.h/.cpp                # Claim/ReleaseDiskOwnership; SoftReset calls the store (done)
│   ├── MachineBuilder.cpp                # WireDiskControllers: every controller on the store's token
│   ├── MachineManager.cpp                # panel copies; bools inside the lock; lastSelectedMachine moves
│   ├── MachineStateFile.cpp              # SaveDisksFirst
│   ├── ScratchHeatReplayer.cpp, ScratchCallReplayer.cpp, ScratchMachineRenderer.cpp   # claim and release
│   ├── ScratchReplayMachine.cpp          # SetFileBacked (035's scratch machine, on its tip)
│   ├── WindowCommandManager.cpp          # shown status; replace prompt; salvage post; external drive lock
│   └── Window/EmulatorWindow.cpp, EmulatorWindowInput.cpp    # handover, menus, tooltips, OnAppMessage split
├── Ui/
│   ├── DriveWidgetController.h/.cpp      # sync-event queue under a mutex; DoorRestore
│   └── DriveWidgetState.h                # isEjectPosted; comments
└── resource.h                            # IDM_DISK_SALVAGE_WRITE, IDM_DEBUG_ATTACH_SINKS, IDM_DEPLOY_GO_LIVE

UnitTest/
├── EmuTests/   ThreadOwnershipTests, OwnerThreadStandIn, DiskOwnershipTests, DriveStatusPublisherTests,
│               DiskManagerRig.h, DiskManagerDriveStatusTests, DriveStatusStressTests,
│               SanitizerCanaryTests, DurableCommitTests, DeploySaveTests, plus regressions added to
│               DiskImageStoreTests, DiskWritePathTests, DiskFlushHoldTests, DiskHistoryTests,
│               DiskResetRemountHoldTests, SharedImageTests, CpuCommandDispatcherTests,
│               DivergenceGateTests, MachineStateFileTests, ResetSemanticsTests, HeatHistoryTests,
│               CommitPlanTests, DiskFailureModeTests, IntentChannelTests, PathResolverTests,
│               WozLoaderTests, DiskImageTests, Disk2Tests, NibblizationTests, CrossFormatWriteTests,
│               PerformanceTests
├── Devices/    Disk2StateTests, Disk2ControllerEventTests, Disk2ControllerAudioTests
├── UiTests/    AnimationSyncTests, ChromeBandLayoutTests, UserConfigStoreTests, CpuManagerCommandTests
└── DebuggerTests/ EmulatorDebugWiringTests, DebugModeTests, CallStackHistoryTests (035's)

ScenarioTests/  a 40-track WOZ read at track 39; a guest format of a WOZ with blank tracks; every real-file
                unit test, moved from UnitTest (65 at 2026-10-09, Win32DiskFileIoTests and
                Win32ImageWatcherTests among them, tasks.md T057), plus R3's six real-file seam cases or
                040's (the EHM breakpoint handler is already installed, through UnitTest/ModuleSetup.cpp)

scripts/        Build.ps1 and RunTests.ps1 gain -Sanitize
Directory.Build.props and all ten .vcxproj   CassoSanitize=Address, $(CassoBuildFlavor) in OutDir and IntDir
```

**Structure Decision**: The existing layout, with new types placed beside the
code they serve: the ownership token in `Core/` (it is not disk-specific),
the published data and the commit in `Devices/Disk/`, and the shell-side
publisher and layout facts in `Shell/`.

## Design

### 1. Ownership (R5)

One `ThreadOwnership` token per `DiskImageStore`, shared with every
`Disk2Controller` wired into that machine. The constructing thread holds it.
Handover is two steps on two threads: the holder calls `Release`, the taker
calls `Claim`. A claim by the holder and a release of an unowned token change
nothing; a claim or a release while another thread holds the token asserts.
The phases:

- **Startup**: the window thread holds the store; `Initialize` mounts command-line disks.
- **Handover**: the window sets the cold-boot mount window off, then releases,
  just before `m_cpuManager.Start`; `OnCpuThreadStart` claims first.
- **Running, paused or not, and machine switch**: the emulation thread.
- **Stop**: the last statement of `OnCpuThreadStop` releases; `~EmulatorShell`
  claims right after `m_cpuManager.Stop()` for the shutdown flush.
- **Scratch machines**: `ScratchHeatReplayer::Run`, 035's
  `ScratchCallReplayer::Run` and `ScratchMachineRenderer::Render` claim at
  entry and release at exit (the two replayers each build their machine
  through 035's `ScratchReplayMachine`).
- **Deploy hold** (the update installer's save, after the `master` merge):
  the emulation thread releases before it signals held and claims after the
  hold is released; the window claims after `HoldForDeploy` returns and
  releases before `ReleaseDeployHold`. The return to live that comes first
  runs on the emulation thread, before the hold (section 8). The hold is not
  035's pause acknowledgement (`IsParked`, `TryWaitUntilParked`, `TryPark`,
  defect b's fix): a paused thread still drains posted commands and runs the
  service function every 20 ms (`kServiceIntervalMs`), never moves disk
  ownership, and `IsParked` is true with no CPU thread at all; the hold stops
  commands and service passes and moves the disks.
- **Tests**: the constructing thread, unless a test hands the store to
  `OwnerThreadStandIn`.

`ASSERT_THREAD_OWNERSHIP` is the first statement after the declaration block of
every guarded entry point (the list is in R5, plus every store and controller
member this design adds, each with its sweep row in the task that adds it),
expands to EHM `ASSERT` in Debug and to nothing in Release. It detects; it does
not lock. `NoteExternalChange`, `GetThreadOwnership`, statics, constructors
and destructors are exempt; no store member reads status for another thread,
because the publisher lives outside the store and checks through the store's
token. `Disk2Controller::SetThreadOwnership` runs a check of its own instead
of the macro (both the current and the new token held by the calling thread)
and has a sweep row like any guarded member. Guarded functions lose
`noexcept`, because the test handler reports by throwing.

The proof is not the existing suite, which is single-threaded and so cannot
see a window-thread caller. It is `DiskOwnershipTests` (a sweep that calls every
guarded entry point from the wrong thread and checks for one assertion each, with
a mutation check), a stand-in test for each reader that moves, and a Debug run
of the app through the window's disk paths with no assertion dialog
(tasks.md T016 records the sites at the start, T051 closes them). Window code
that runs only around a modal dialog or a debug panel window, which no unit test
can drive, is proved by the regression that pins the data it now reads plus the
step of the Debug run that exercises it; tasks.md's Ownership move list gives
each row's proof.

### 2. Published drive status (R1)

`DriveStatus` holds a shared, immutable `BayTable` (16 `BayStatus` records:
`isMounted`, path, format, `WriteProtectInfo` with its causes and damaged
quarter tracks, `isSalvageOffered`, and, after the `master` merge, the WOZ
requirements the info icon shows, tasks.md T136) and per-drive `DriveActivity` for slot 6
(motor, head quarter track, read and write nibble counters), plus whether slot 6
has a controller. No field points into the store, an image or the controller.

- **Generation**: `DiskImageStore::GetStatusGeneration` moves wherever a bay
  field or an image's write-protect cause is written, including through the
  flag-only setters `SetUserWriteProtectFlag` and `SetFileWriteProtect` that the
  writers outside the store use. The bay table is rebuilt only when it moves;
  Debug builds rebuild every time and `ASSERT` the result matches, so a missed
  bump fails the suite.
- **Publish points** (all on the owning thread): the end of every service pass (P1,
  `EmulatorShell::ServiceCpuThread`, which covers drained commands and the
  paused machine), the end of `RunCpuThreadFrame` (P2), just before
  `MountDiskInSlot6` reports completion (P3, once the salvage insert has left
  the window thread), and once at the end of `Initialize` (P4).
- **Wake**: `Publish` returns `DriveStatusChange` (`None`, `Activity`, `Bays`).
  `DiskManager::PublishDriveStatus` returns it and calls the wake the shell
  installs with `SetStatusWake`, which sets `m_frameReadyEvent`, exactly when
  it is `Bays`; an activity-only publish wakes nothing. The FR-003 test runs
  at the `DiskManager` level with a counting wake, because a shell built in a
  test has no `DiskManager` (it is created only on `Initialize`'s path,
  `EmulatorShell.cpp:404`, `:687`); `ServiceCpuThread` is a fixed sequence of
  three calls, and the Debug app run confirms the paused case on screen
  (tasks.md T030, T031, T051).
- **Readers**: the window takes the newest status once per window frame at the
  top of `UpdateDriveWidgets` (split into three helpers first), and every
  window-thread reader uses that shown copy through
  `EmulatorShell::GetShownDriveStatus` (the list is in R1). The mount
  completion is the exception: it gets the drive's `BayStatus` copied on the
  owning thread at P3, because the newest status can describe a later medium
  by the time the window handles the message. The create-disk replace prompt
  reads the shown bay through `DiskManager::IsReplacePromptNeeded`, which also
  checks whether an eject is posted for that drive (entry-flags-race Part B).
- `DriveWidgetController`'s sync queue gets its own leaf mutex (FR-008).

### 3. Window writes become commands (FR-004, FR-005)

- **User write protection**: the existing `IDM_DISK_WRITEPROTECT1/2` command
  stays. It already runs on the emulation thread, is journaled as
  `DriveWriteProtect` and is divergence-gated. Its handler,
  `EmulatorShell::SetDriveUserWriteProtect`, calls the store's new
  `SetUserWriteProtect`: save first when protecting, and on a failed save leave
  the disk writable with its writes and call `NotifyMediaChanged`, so the
  boundary keyframe at that position records the unprotected drive (FR-015).
  `Replayer::ApplyInput` uses the flag-only `SetUserWriteProtectFlag`, so a
  replay never saves.
- **Salvage** (the audit's fix): the Disk menu and the damage report post
  `IDM_DISK_SALVAGE1/2`; the emulation thread assesses and posts
  `WM_APP_SALVAGE_OFFER` with the counts, the media id and the source path; the
  window shows the question and posts `IDM_DISK_SALVAGE_WRITE` with the media id
  and path; `SalvageToFile`, on the emulation thread, writes nothing when the bay holds another medium
  (`ERROR_MEDIA_CHANGED`) and posts `WM_APP_SALVAGE_DONE`; the window shows the
  result and the Insert / Not now question, and on Insert posts the ordinary
  `IDM_DISK_INSERTn`, which is gated and journaled.
- **Damage report and change questions**: `ReportDamagedMount` reads the
  `BayStatus` in `MountCompletion` and no longer decodes on the window thread;
  `AskAboutChange` uses the path the prompt was composed from.
- **Debug sinks**: `IDM_DEBUG_ATTACH_SINKS` makes the emulation thread the only writer of
  device sinks and panel cycle counters; the switch reads CPU-only copies of the
  panel pointers.
- **External changes**: `NoteExternalChange` records the intent and time in an
  inbox keyed by path under `m_pendingMutex` (the latest per path) and touches
  no `Entry`. The emulation thread drains the inbox after `ApplyPendingReload`'s replay
  check and matches paths against `m_entries` only.
- **Layout facts**: `MachineChromeFacts::TryCapture` takes the lifetime lock
  shared and copies the facts layout needs; the switch writes its two bools
  inside its exclusive section, and the external-drive command writes under a
  shared try-lock (FR-006).
- **Settings**: `UserConfigStore` serializes its public operations with an
  internal lock, a save from another thread never reads the window's
  `GlobalUserPrefs`, `UpdateUiPrefs` makes the per-machine read-modify-write
  one step, and the switch's `lastSelectedMachine` write moves to the window
  thread (FR-007).
- **Behind live**: the insert, eject and both write-protect commands change
  nothing when the store's flush hold or replay flag is set, because such a
  command slipped past the window's divergence gate, whose check runs on the
  posting thread. The check is in entry points only those commands reach: a new
  `DiskManager::MountDiskForCommand`, which the insert command's override calls
  in place of `MountDiskInSlot6`, then `EjectDiskInSlot6`,
  `ToggleImageWriteProtect` and `SetDriveUserWriteProtect`. It is not in
  `MountDiskInSlot6`, which the command-line mount and `RemountSlot6Disks` also
  call. A Settings write-protect command whose value equals the drive's current
  setting changes nothing and raises no notice, because Settings sends both
  drives on every apply (`SettingsPanelState.cpp:1054-1057`); the handler
  stores that setting only when the change lands, so a protect FR-015
  declined is tried again by the next apply. The store's explicit flushes
  keep writing while held, as the project owner decided.

### 4. Mount, eject and reload (R2)

The invariant: a disk leaves its bay only when every guest write is in a file,
either its own (the save succeeded) or a lossless recovery copy of exactly the
current content when the change goes elsewhere. Otherwise the change is
declined or waits for an answer, and declining calls none of `RetireBay`,
`EmitBayChange` or `NotifyMediaChanged`, so history records nothing.

- **Different file**: read and load the new image into a local first; on
  success, save the old disk, then retire it and swap the new one in (FR-009).
- **Same file**: save the old disk first; when the file is then unchanged since
  that save (both identities recorded and matching), keep the mounted image and
  read nothing, which also keeps an ordinary reset from capturing a boundary
  keyframe between its `Reset` record and its `SoftReset`; otherwise read the
  file and load, so the image loaded is the one just written (FR-009, SC-006).
  The same-file test compares lexically normalized paths, after FR-013 has
  made command-line paths absolute.
- **Per action**: the table in R2 (insert, re-insert, reset, power cycle,
  machine switch, eject, reload, salvage insert, shutdown, load state). Quit
  runs the rescue question for an unwritable disk too. A state load saves every
  disk before the other-machine switch and before recording stops.
- **Watch** (FR-011): replacing a bay's disk ends the old watch, immediately
  before `RetireBay`, when no other bay uses that folder.
- **Flux burst** (FR-012): the reload, the move-assign and the discard commit
  the open flux write to the outgoing image before their dirty tests.
- **Paths** (FR-013): command-line disk paths, and the paths the CLI states in
  its intents, are made absolute against an explicit base directory.
- **Scratch stores** are not file-backed and write nothing.

### 5. Write path (R3)

- **Durable commit** (FR-018): `DurableCommit` runs write-temporary, copy
  metadata from the target (best-effort: a failure there does not stop the commit,
  and nothing is shown), `FlushToStorage`, `ReplaceAtomically` (or
  `RenameWithoutReplacing` for `CommitMode::CreateNew`) through `IDiskFileIo`,
  and reports the step it reached in `CommitPlan::Progress`. Spec 040 builds it to
  this spec's contract and this branch cherry-picks it (tasks.md T054). The store, the CLI,
  Cassque and `casso debug --write-disks` share it.
- **One recovery copy** (FR-016): `MountedImageState` keeps the recovery path,
  identity, image id and track generations written, and which reports were
  shown, which is the store's only save-failure report flag. A later failure
  rewrites that copy (or nothing, if unchanged) and shows nothing more.
  Cleared on mount, eject, a successful save, and an external reload. The
  report-once rule applies to saves nobody asked for (`FlushMoment::Background`,
  which lands first); a save the user asked for reports every failure.
- **`.nib` re-save** (FR-014): `DiskImage::MarkSaved` replaces the raw source
  bytes with the bytes just written, at every site that writes serialized bytes.
- **Reset** (FR-017, shipped in `e32b2b68c`): `MachineHost::SoftReset` calls
  the store's `SoftReset` (`FlushAllUnlessHeld`) at its top, as `PowerCycle`
  does, and `Disk2Controller::SoftReset` only resets the controller, so a reset
  and a replayed reset write only through the store's checks. 035's tip
  `d3c15b55c` holds no cherry-pick of that commit, only a comment for defect c
  (`fe0427676`), so 035 reaches `master` with the controller's write and 041's
  merge removes it (tasks.md, What 035 brings).
- `FlushEntry` is split before any of this lands, so it stays under 100 lines.

### 6. Drive emulation (R4)

- **Head stop** (FR-020): `Disk2Controller::kMaxQuarterTrack` becomes 158; every
  reader follows the constant, including the end-stop sound, which plays on the
  outward step past 158.
- **Guest-created tracks** (FR-021): `WozLoader::Load` reserves an empty bit slot
  for every whole track 0..39 whose position has no data, mapped at qt/4
  placement; the first bit the sequencer writes over such a slot sizes it to
  51,200 bits. The track-record rule is spec 040's (its FR-053), built in
  040's `51ad867e8`, which this branch takes by cherry-pick or through
  `master` at T136 rather than building its own (tasks.md T111): a bit (TMAP)
  record with a zero bit count has no data, whatever its start block and block
  count, and is reserved (owner confirmed 2026-10-09 for a zero count with a
  start block of 3 or more); an empty FLUX record stays an empty flux track,
  which the guest can already write. A record with a count above zero and a
  zero start block or block count (`RecordLocationMissing`), a start block of
  1 or 2 (`RecordInHeader`) or more data than its blocks hold
  (`CountExceedsBlocks`), and a map entry from 160 to 254 (a
  `DamagedQuarterTrack`), are damage, which write-protects the image. The
  reservation does nothing at all for an image with any damage
  (`DiskImage::IsDamaged`), so every map entry of such an image stays as
  loaded, whatever the reason.
  The slot count is fixed at mount, so 035's keyframes and `LoadState` are
  unaffected; `LoadState` accepts a state with fewer slots when
  every surplus live slot is an empty bit slot; `DiskTrackSnapshot::Restore`
  bumps the layout generation. `StepLss` is split first.

### 7. Proof (R5, R6)

The ownership assertion with its sweeps, stand-in tests and Debug app run
(SC-003); one drive-status stress test, run in Debug, Release and the
AddressSanitizer build, with its writer taking the store through
`OwnerThreadStandIn` (SC-002); a canary that passes in every build and shows the
sanitizer build is instrumented; and ASan itself. The run that shows the stress
test failing against the live bays is a local mutation, run once and recorded in
the commit message. Nothing committed fails under `-Sanitize`.
`scripts/Build.ps1 -Sanitize` and `scripts/RunTests.ps1 -Build -Sanitize`
drive the ASan build into `x64\DebugAsan\`.

### 8. After 035 reaches `master`

Merge `master`, with the Merge procedure's checks; confirm that
`ReverseController::Stop` clears the replay flag (035's fix for defect a,
`c913c9247`, which T008 already brings), which blocks the merge if not; move
the window-thread readers `master` brings (the drive context menu and the
changed menu and tooltip sites) to the shown status; wire FR-019's deploy hold
into the update installer's final save, with the ownership handover and a
return to live first; make the `ARCHITECTURE.md` changes, whose "Disks"
section (§8) reached `master` in merge `1390af1a5` and arrives here with that
merge; then every gate again, and the CHANGELOG and README once those have
passed (spec, Merge order). `CpuManager`'s deploy hold and `DeploySave` are
built and tested before the merge; only the call site waits for it.

**Every update install saves the disk at the live end** (owner decision,
2026-10-08; owner confirmed 2026-10-09 that it covers every install path).
When an update is installed while the machine is behind live, the machine is
returned to the live end before the save, on all three paths: the deploy
`HandleUpdateApplyResult` runs for a bundle ready to deploy, an MSIX update
installed now, from the update dialog or from `ApplyPendingUpdateNow`
(`EmulatorShellUpdate.cpp:679-700`, `:1471-1476` on `master`); a zip update
installed now, which closes the window the ordinary way (`:718-721`), so the
return to live runs just before that `WM_CLOSE` post and quit's save then
saves the live end; and an update left until Casso closes, which installs
after the exit flush (`CommitPendingUpdateAtExit`, `:1500-1502`, or Windows
for an MSIX bundle), so `OnDestroy` runs the return to live before it stops
the emulation thread (`EmulatorWindow.cpp:1737`, `:1768` on `master`) when
an update is pending. One helper, `ReturnToLiveForUpdate`, serves all three
(tasks.md T139). A quit with no update pending is unchanged: it runs
`StopReverseRecording` first and saves the disk where the machine stands
(`EmulatorShellCpuThread.cpp:319-326`). The two close paths save through
quit, with no hold: the emulation thread stops right after, and the
destructor's `FlushAllForShutdown` runs with it joined. The mechanism of the
return, chosen from the two the owner's decision allowed:

- **On the emulation thread, then hold.** The window posts
  `IDM_DEPLOY_GO_LIVE` and waits, with a 5 s deadline, for that request's
  outcome; the emulation thread runs `RunReverseCommand (ReverseCommand::GoLive, 0)`,
  the call and the condition `ApplyReverseOptions` uses
  (`EmulatorShellReverse.cpp:145-147`), and reports the outcome
  (`DeploySave::CompleteLiveReturn`). Only then does the window call
  `HoldForDeploy`. This closes the ordering gap: the hold check sits before
  `DrainCommandQueue` (`CpuManager.cpp:583` at `e32b2b68c`) and posted commands
  stay queued while held, so a return to live posted just ahead of the hold
  could stay queued and the save would run behind live.
- **Why not on the window thread while held.** Nothing in `ReverseController`
  or `Replayer` checks its thread, and a held emulation thread leaves no data
  race, but the return to live is emulation-thread code by contract:
  `RunReverseCommand` (`EmulatorShellReverse.cpp:196-286`) steps the live
  machine through `Replayer::RunTo` (`Replayer.cpp:127-206`), mutes output
  through the replayer's gate, swaps the debug hook, calls the debug session's
  `OnStopped`, renders and publishes the framebuffer and moves the history
  thumbnails. The disk ownership token covers only the store and the drives,
  so a mistake in the rest would go unreported, and the live machine would
  have a second thread stepping it, where today only the CPU thread steps it
  (threading-035.md section 2).
- **The wait is short and cannot deadlock.** A return to live replays from
  where the machine stands, or from the newest keyframe at or before the live
  end, to the live end (`ReverseController.cpp:781-864`), so at most about one
  keyframe interval (10 frames by default, `ReverseOptionsDialog.h:23`) plus
  the wait for keyframes in flight (`:815`). Code on the emulation thread posts
  to the window and never sends to it (no `SendMessage` from it in `Shell/`;
  `ShowNotification` posts when off the window thread,
  `EmulatorShellDialogs.cpp:183-195`), so the window thread can block on the
  wait.
- **A late command never moves the machine.** When the deadline passes, the
  request stops waiting for its serial, and a `GoLiveForDeploy` that has not
  started does nothing (`DeploySave::IsAwaited`), so a failed deploy that
  releases the hold does not then move the machine. One already running when
  the deadline passes finishes before the hold lands, because the hold lands
  only between passes; its outcome is recorded all the same, and the window
  reads it once held (`GetLastOutcome`). A failed `Execute` leaves
  `m_lastReverseOutcome` empty, because `RunReverseCommand` sets it only after
  a successful one, and `GoLiveForDeploy` clears it first, so that maps to
  `NotReached` rather than to the previous command's outcome.
- **Ownership.** The return to live runs on the emulation thread, which holds
  the disks. After `HoldForDeploy` returns, the emulation thread has released
  them and the window claims them for the save; `DeploySave::Resume` releases
  them before `ReleaseDeployHold` on a failed deploy (section 1, Deploy hold).
  A hold that misses its own 5 s deadline is withdrawn, and the deploy does not
  go ahead.
- **When the return does not complete.** The return can give a cut history (a
  replay that diverges ends live at the last good keyframe, with the recorded
  future dropped, `ReverseController.cpp:932-959`), stop short at a gap, or
  miss its deadline; a reverse command that runs after it, before the hold,
  can also leave the machine behind live. In each case the deploy saves the
  disk where the machine stands. Once held, the window reads the store's hold
  and replay flag; when the machine is still behind live it clears the replay
  flag before `FlushAllForShutdown`, because `FlushEntry` returns at that flag
  (`DiskImageStore.cpp:1090`) and would otherwise save nothing (the case a
  forward step from the past leaves, `ReverseController.cpp:1885`, `:1913`).
  A failed deploy sets the flag back before it releases the hold. A stop
  request cannot cut a return to live short: `StopReplay`'s flag is read only
  by the reverse searches (`ReverseController.cpp:597`, `:1298`, `:1365`,
  `:1403`), not by `Seek`.
- **When the user is told.** The notice (contracts/user-messages.md section 13)
  is decided once the machine is held, from the state after the save, not
  from the request's answer alone: it is raised when the machine was still behind live
  once held (`DeployFlushResult::wasBehindLive`) or when the recorded outcome
  is `HistoryCut`, and not otherwise, so a `NotBehindLive` answer (a queued Go
  live had already run) and a `TimedOut` return that finished before the hold
  give no notice, because the disks were saved at the live end.
  `DeploySave::IsSavedBehindLive` holds the rule, with a table test. The zip
  update installed now applies the same rule before it posts `WM_CLOSE`, with
  `wasBehindLive` read from `IsBehindLiveForUi()` after the return; an update
  left until Casso closes shows no notice, because its return runs in
  `OnDestroy`, while the main window is being destroyed.
- **Tests** (tasks.md T128, T129): the hold's own tests, a paused machine with
  no service function included; a held deploy save behind live writes the live
  end's disk; a return that misses its deadline, with its `IDM_DEPLOY_GO_LIVE`
  still queued behind a command that runs until the hold is requested, writes
  the disk as it stands, with the replay flag cleared for the save, set again
  on resume, and the late command leaving the machine where it stands; the
  table tests for the outcome mapping and the notice rule; a return to live
  followed by quit's stop and shutdown flush writes the live end's disk, for
  the two close paths; and the audit's two deploy-flush tests. The forward step that sets the replay flag is one
  instruction run from the past with `machine.StepOne()`, as 035's own tests
  do; the reverse step command would leave the flag clear, because
  `Replayer::RunTo` clears it on its way out (`Replayer.cpp:195`).

## Order of work

tasks.md is the build order; this is its outline. Each phase ends with its gate
(the full Debug unit suite and the scenario suite), a commit, and a merge of any
new 035 commits with the checks tasks.md gives.

1. **Setup**: the sanitizer build, a check that the scenario suite already
   reports assertions, the owner's approval of every user text, and the report
   of 035's defects.
2. **Foundation**: the ownership token, guards and handovers with their sweeps
   and stand-in helper; salvage and the drive widget queue moved to the
   emulation thread, with `CpuCommandDispatcher::Dispatch` and `OnAppMessage`
   split first; `FlushEntry` split; the published status, its wake and its
   publish points.
3. **Readers and commands** (US2): every window-thread reader moved to the
   shown status, the damage report and change questions, the replace prompt,
   the debug sinks and the external-change inbox; the Debug app run shows no
   ownership assertion.
4. **Write path** (US1): the durable commit (store, CLI, debug batch), the
   flush-cost measurement and its decision, save failures reported by moment,
   one recovery copy, reset through the store, the `.nib` re-save.
5. **Paths, watches and layout** (US3): absolute paths with the CLI sender,
   the watch on replace, the layout facts.
6. **Mount, eject and reload** (US1, US2): the flux-burst commit, the
   `OnBayChange` and `MountDiagnosis::Describe` splits, then R2's restructure
   behind its tests (which add the API they call as stubs with today's
   behavior, so the test project builds throughout; the mount outcome texts
   first), user write protection, and the behind-live declines. This phase needs FR-008, FR-011, FR-012, FR-013, FR-016, FR-017,
   FR-018 and the salvage posting, which the earlier phases land.
7. **Drive emulation** (US4), which can start after phase 1, waiting only
   where it shares a file with an earlier phase (tasks.md lists them), with the
   WOZ track-record rule shared with spec 040 ahead of the reservation.
8. **Settings** (US5), which can start after phase 1 on the same terms.
9. **Polish**: the stress test and canary, the sanitizer run, the idle cost,
   the deploy hold and the deploy's return to live and save, the comments and
   docs, the gates and the on-screen check.
10. **After 035 lands**: the `master` merge (with the WOZ info icon's read
    moved into the published status and spec 040's loader change taken),
    the replay-flag check, `master`'s readers and threads, FR-019's three
    call sites, `ARCHITECTURE.md`, the gates again, then
    CHANGELOG and README for the owner's approval before the merge.

Each defect's regression test is written first and seen to fail.

## Complexity Tracking

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| `ThreadOwnership` | SC-003 needs a check at every entry point, and the owning thread changes at runtime | A thread id compared against `GetCurrentThreadId` in place cannot follow the handovers |
| `DriveStatus` + `DriveStatusPublisher` | FR-002 requires a copy consistent within itself across many fields | Per-field atomics and locks are safe per read but not across reads |
| `DurableCommit` | FR-018 for the store, and the CLI already has half the sequence | Patching `WriteFileAtomically` alone leaves two different commit sequences |
| AddressSanitizer build property | SC-002 needs a detector for use-after-free and torn strings | A new solution configuration would grow the four-configuration gate |
| Statements at fixed points in four long functions, with no extraction first: three in `EmulatorShell::Initialize` (250 lines: T031's P4 publish and T078's two layout-facts refreshes, after the config is set and after the build), and one each in `~EmulatorShell` (137: T015's claim), `EmulatorShell::RunMessageLoop` (147: T015's release) and `WozLoader::Load` (274: T115's reservation call) | Each statement belongs at one fixed point in startup, shutdown or load, and the tasks add nothing else to these functions (tasks.md, Rules for every task) | Extracting each function first would rework four functions of 137 to 274 lines for one to three statements apiece; every other function over 100 lines that 041 adds logic to, and `OnBayChange` at 99, is split first, by a task of its own or as the first step of the task that first adds to it |
| One ownership check in `DiskImageStore::ResolvePendingChange` (197 lines) before T085 splits it | T013 guards every public store member in Phase 2, with T014 and T015 in one commit, and the guard sweep needs every row from the start; T085's split belongs with the Phase 6 work that adds to the function | Moving T085 into Phase 2 would renumber every task the 2026-10-08 decisions cite; the check is one statement, and T069's edit to the function before the split replaces `ClearDirty` calls without adding lines |
| `DeploySave` | The deploy must return the machine to the live end on the emulation thread and wait for the outcome before it holds the thread (owner decision 2026-10-08; section 8), keep a late return to live from moving the machine, and decide the notice from the state once held | Posting `GoLive` just ahead of the hold can leave it queued behind the hold; running the return to live on the window thread while held would step the live machine and run the debug session and the framebuffer from a second thread; deciding the notice in the call site would leave it out of reach of a unit test |

## Review issues not applied

The 2026-10-08 review gave 93 issues in four lenses. Each entry below gives the
lens, the issue's 0-based position in that lens's list, and why its correction
was not taken, or not taken in full. Every other correction is applied. Task
numbers below are the current tasks.md's, not the numbers the review used.

- **coverage 5** (ReportDamagedMount and AskAboutChange), in part. The damage
  report reads the `BayStatus` copied into `MountCompletion` at P3, not
  `TakeLatestDriveStatus()`, per with-035 7: by the time the window handles the
  completion, the newest status can describe a later or different medium. The
  rest of the correction is applied (tasks.md T044-T046).
- **coverage 12** (durable commit tests and contract), in part. The contract
  uses a free `CommitMode { Replace, CreateNew }` instead of R3's nested
  `TargetRule`, per rules 18 and the type-placement rule; the parameter list,
  `CommitPlan::Progress`, the step changes and every test are applied.
- **coverage 24** (generation coverage, FR-003), in part. The FR-003 test
  asserts the `DriveStatusChange` that `DiskManager::PublishDriveStatus` returns
  on the paused path and counts the wake it calls, not that `m_frameReadyEvent`
  is signaled, because rules 11 keeps kernel objects out of the unit tests; the
  shell's wake sets the event. The test is at the `DiskManager` level because
  a shell built in a test has no `DiskManager` (second revision, A1), so only
  `ServiceCpuThread`'s fixed call sequence is left to T051's Debug run. The
  rest is applied (T025, T028, T030).
- **rules 2** (user-message list), in part. The `AwaitingAnswer` sentence
  (audit.md, entry-flags-race Part B) is not added: R2's decline keeps the disk
  when an insert arrives while an eject waits on its answer (coverage 20, tested
  by T088), so `MountFailure::AwaitingAnswer` is not part of the design.
  contracts/user-messages.md records it under "Considered and not added".
- **rules 7** (long functions), in part. No `CommitOrDiscardOpenFlux` helper:
  coverage 15 settles that the open flux write is committed, which is one call
  to the existing `DiskImage::CommitPendingWrite`, and `ApplyPendingReloadToBay`
  is split under 100 lines before that call is added (T082). The other
  extractions are applied.
- **sequencing 0** (phase layout), in part. Every dependency edge it gives is
  kept, but the layout facts (FR-006) and the settings lock (FR-007) stay in
  their own story phases instead of the readers phase, because neither blocks
  the mount restructure and US5 can run alongside the other phases.
- **sequencing 4** (merging the guard and handover tasks), in part. The work is
  three tasks (T013 store guards, T014 controller guards and `noexcept`, T015
  handovers, brackets and the cold-boot move) that land in one commit, rather
  than one task, so each stays a size one pass can do; the suite is never red
  between them because nothing is committed until all three are in.
- **sequencing 7** (absolute paths before the same-file branch), in part. The
  same-file test compares lexically normalized paths; comparing file identity to
  defeat 8.3 names and links is not possible, because `ImageIdentity` holds a
  size and a write time, not a file id. FR-013 makes command-line paths
  absolute, and picker paths are already full paths.
- **sequencing 9** (splitting oversized tasks), in part. The split follows its
  outline, with these departures:
  - The `FlushEntry` extraction moves to Phase 2 (T024), ahead of the first
    task that adds a line to it.
  - `FlushMoment::Background` and `FlushOutcome` land in Phase 4 (T063),
    ahead of FR-016's report-once rule (T065), so that rule never reaches a
    save the user asked for; `Replacing` comes with the different-file mount
    (T096).
  - The `MountDiagnosis` additions (`UnsavedWrites`, the four fields,
    `ShouldReport`, `Describe`), `FormatUnsavedWritesMessage`, the kept-disk
    sentence, `ComposeReplacedReport` and the `DebugBatchRunner` switch are
    not part of the same-file task: T087 adds the members its tests call as
    stubs, and T094, a task of its own ahead of both mount tasks, gives them
    their texts and rule. The different-file decline (T096) is the first to
    fill the diagnosis, and taking them out keeps T096 to the restructure
    itself.
  - `TryCreateTrack` and the `DiskTrackSnapshot::Restore` generation bump stay
    with the reservation and its `LoadState` rule (T115), not with the engine
    task (T116): T112's `DiskImageTests` and `Disk2StateTests` cases call
    `TryCreateTrack` and check the bump, so splitting them across two tasks
    would leave the test project unbuildable between the two.
- **sequencing 10** ([P] markers), in part. The decision to register each new
  file in the task that creates it replaces "one sequential project-file task
  per phase", so tasks that each add a file to the same project are not marked
  [P] against each other. The repo has no `.filters` files.
- **sequencing 17** (the external-change inbox regression). The proposed "note a change for a path no bay
  holds, then mount it" test would not show a reload: the reload's identity
  check matches the file that was just mounted, so nothing is reloaded.
  coverage 14's deterministic inbox test is used instead (T049), and the check
  that `NoteExternalChange` reads no `Entry` is the SC-002 stress test's
  watcher thread under `-Sanitize`.
- **sequencing 22** (dependency text), in part. US4 and US5 can start after
  Phase 1, and the settings-store path is corrected, but the claim that they
  touch files disjoint from everything else does not hold: US4 shares
  `Disk2Controller`, `DiskImage`, `Disk2NibbleEngine.h`, `DiskWritePathTests.cpp`,
  `ScenarioTests.vcxproj`, `DriveWidgetState.h` and `EmulatorShellPresent.cpp`
  with earlier phases, and US5's last task shares `MachineManager.cpp` and
  `EmulatorWindow.cpp`. tasks.md's Dependencies section gives every overlap
  and the task on the other side of it.
- **with-035 5** (deciding on the owning thread behind live), in part. The
  decline lives in the entry points only the disk commands reach
  (`DiskManager::MountDiskForCommand`, `EjectDiskInSlot6`,
  `ToggleImageWriteProtect`, `SetDriveUserWriteProtect`), not in
  `DiskManager::MountDiskInSlot6`, which the command-line mount and the remount
  also reach, and not in
  `SaveBeforeReplace`, `Eject`, `SetUserWriteProtect` and
  `SetImageWriteProtect`, because the store's explicit flushes write while held
  by the project owner's decision (`DiskFlushHoldTests`
  `HoldStillSavesOnEjectSwitchAndExit`; `DiskImageStore.h:133-137`). Its
  store-level test would contradict that test, so T093 runs through those
  entry points instead. The gate itself is 035's (spec Assumptions item 7,
  which was not among the six defects reported to 035 on 2026-10-08).
- **with-035 13** (the audit's park for the deploy). Applied in full since the
  second revision: the ownership handover, and the return to live before the
  thread is held, which the owner decided on 2026-10-08 (section 8; tasks.md
  T128, T129, T139). The audit's park is now a deploy hold with names of its
  own (`HoldForDeploy`, `ReleaseDeployHold`, `IsHeldForDeploy`), because 035's
  fix for defect b added a pause acknowledgement, `CpuManager::IsParked`, with
  a weaker meaning. Every update install now saves the live end's disk, where
  a quit with no update pending saves the disk where the machine stands, so
  the audit's aim of keeping the deploy's meaning equal to quit's is dropped
  for an update installed behind live.

The second revision (2026-10-08) took 23 open items (A1 to A23) and the
owner's decisions of that day. These items were not applied as written:

- **A14** (functions over 100 lines), in part. `MountDiagnosis::Describe` is
  split in T086 with `OnBayChange`, and `CpuCommandDispatcher::Dispatch` by a
  task of its own (T017), with the salvage work after it, so every split runs
  with the full Debug suite passing; `CpuManager::ThreadProc` is split as the
  first step of T128, `MachineBuilder::CreateMemoryDevices` loses its
  controller loop in T014 (checked by T015's Debug run), and
  `UserConfigStore::Load`'s body moves into two functions under 100 lines in
  T120. The splits were placed without new task numbers, by moving content
  between tasks (below), so T057, T066, T067 and T105, which the 2026-10-08
  decisions cite, keep their meaning; the two new tasks of this revision did
  renumber every task after T110 (below), so the decisions' T135, T137 and
  T140 are now T137, T139 and T142, and each of those tasks gives its old
  number in parentheses. The list in tasks.md's rule is corrected and
  now gives every function over 100 lines that a task edits, comment-only
  edits included, and `ResolvePendingChange`'s one ownership check before its
  split is a Complexity Tracking row.
- **A4 and A18** (statements added to `EmulatorShell::Initialize`), corrected
  further than asked. Both items count two statements; the audit's design
  (layout-reads-machine-refs-unlocked, fix step 2) refreshes the layout facts
  at two points in `Initialize`, so it gets three, and Complexity Tracking and
  tasks.md say three.
- **B5** (reserve case 1 records only), settled since by the owner and by
  040's code. A record in neither of B5's cases, a zero bit count with a
  start block of 3 or more (`WozLoader::BuildSyntheticV2` writes one), is
  empty, not damage, and is reserved like an unmapped track (owner confirmed
  2026-10-09). 040's loader change, `51ad867e8`, tests the count before any
  location check, so every zero-count record is an empty slot that is not
  damaged; this branch takes that commit (tasks.md T111, T136) and builds no
  loader change of its own. The reservation also does nothing for an image
  with any damage (`DiskImage::IsDamaged`), in place of the second
  revision's per-slot damage test (040 session, 2026-10-09).

Every other second-revision item is applied. One consequence of A20's answer
is worth stating here: a drive-relative path on a drive other than the base's
(`D:a.dsk` with base `C:\work`) comes back from `MakeAbsolutePath` unchanged,
because resolving it needs that drive's working directory, which is process
state the function does not read, so FR-013 holds for every other form and
that one is handled as it is today (contracts/internal-interfaces.md, Paths).

Two tasks are new in the second revision, T111 (the WOZ track-record rule
shared with spec 040) and T129 (the deploy's return to live and save), so the
tasks after T110 are renumbered: the decisions' T135 and T137 are now
T137 and T139 (the Stop check and the deploy call site), and their T140 is
T142. Its verification pass moved content between tasks without renumbering,
so the test project builds at every suite run: T017 and T018 are now the
`Dispatch` and `OnAppMessage` splits, with the salvage store, commands and
window side in T019 to T021, each writing its tests first; T025 to T029 are
the store-level status tests, the status types, the writers outside the
store, the publisher tests and the publisher; T046 writes its own `imagePath`
assertion; R3's store tests 1 to 5 moved from T055 to T059 and 7 to 10 from
T064 to T065; T086 also splits `Describe`; and T087, T090, T092 and T093 add
the API their tests call as stubs that later tasks fill.

The third revision (2026-10-09) took 21 open items (C1 to C21), three owner
decisions and four facts from the 040 and 035 sessions, and renumbered no task:
- **Every update install returns to live first** (owner confirmed
  2026-10-09): the MSIX deploy, the zip update installed now and an update
  left until Casso closes (section 8; tasks.md T129, T139). The second
  revision's reading that only the MSIX deploy did is dropped.
- **Every real-file unit test moves to the scenario suite** (owner decision,
  2026-10-09; tasks.md T057, 65 tests at that day's inventory). Left in the
  unit suite by owner decision (2026-10-09), as read-only suite
  infrastructure that writes nothing: the 487 Harte tests
  (`UnitTest/HarteTestRunner.cpp:233-299`), which read the checked-in reduced
  vectors unless `CASSO_HARTE_DIR` or `%LOCALAPPDATA%\Casso\HarteTests`
  supplies the full set, so they read the environment and, when it is
  present, a folder outside the checkout; and the probes that create nothing
  (the `CliMain` tests that stat a typed path or open `no-such-file-here.a65`,
  `DeviceTests::RomDevice_CreateFromFile_MissingFile_ReturnsNull`,
  `SourceServiceTests::AnEmptyFolderListsTheCurrentDirectory`,
  `PathResolverTests::FindFile_NotFound_ReturnsEmpty`). Reads of checked-in
  fixtures stay, as every `TestMachine` test needs them.
- **A zero-count WOZ record is empty** (owner confirmed 2026-10-09), and
  spec 040's loader change and durable commit are cherry-picked, not built
  here (tasks.md T054, T111, T136).
- **C7** (the CLI's base directory): the base comes through the `IDiskFileIo`
  seam (`GetWorkingDirectory`, tasks.md T073) rather than a constructor
  parameter, so `DiskCommand::Run`, `AssemblerMode::Run` and Cassque's
  `DiskOperations` keep their code and no unit test reads the process's
  working directory.

One reading in this revision is for the owner to confirm:
- 035's tip `d3c15b55c` holds no cherry-pick of `e32b2b68c` and no "follow-up
  comments" commit, though the 035 session described both on 2026-10-08.
  The plan assumes neither arrives: 035 reaches `master` with the
  controller's reset write, 041's merge removes it, and T130 makes the two
  comment corrections. A cherry-pick of `e32b2b68c` that does arrive merges
  with no change; a comment commit for defect c that arrives after T008 or
  T130 conflicts with their edits, and the merge keeps one wording, the one
  that says the reset flush is held behind live (tasks.md, What 035 brings).
