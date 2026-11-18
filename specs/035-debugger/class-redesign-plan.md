# Class redesign plan: the classes behind T1533 and the new storage hardware

**Branch**: `035-debugger`. **Written**: 2026-11-18. **For**: the session that implements it.

## Why this exists

CheckStyle rule CS0026 limits the size of a `.cpp` file. Six classes were cut to
fit: `MachineHost`, `Disk2Controller`, `ReverseController`, `KeyframeStore`,
`AppleWinParser` and `Cpu`. Each cut moved the piece that was easiest to move and
stopped once the file fit. Each file is just under the limit, and each class
is the same class with one piece missing.

The owner's direction, 2026-11-18: "Apply the best designs possible and don't fear
the code churn." This plan redesigns each class from its state. Line counts are a
result, not a goal.

## Starting point

- Local `035-debugger` is at `4b20fcd8c`, not pushed. It contains the first cuts:
  `MachineConnectors`, `MachineStateStream`, `MachineRomIdentity`,
  `MachineReplayMedia`, `KeyframeCodec`, `HistoryTimeline`, `InstructionSetBuilder`,
  `AppleWinBreakpointParser`, `AppleWinText`, `Disk2IwmPort`.
- Other T1533 files are on three unmerged branches. See "The overseer", section 8
  and "Dxui" below for which files, and for what to do with each branch.
- Read `.github/copilot-instructions.md`, `CLAUDE.md` and `~/.claude/CLAUDE.md`
  before any edit.

## Method, for every class

1. **List the state.** Every field of the class, in a table: field, which methods
   write it, which read it, which thread.
2. **Group the fields that change together.** Each group plus the invariant it
   protects is a class. A group with one writer thread is a class with one thread.
3. **Design each new class's API from its callers**, not from the old class's
   method list.
4. **Write the design note** for the class in this file's "Design notes" section
   (replace the sketch below with the final tables) before moving code. Where the
   code contradicts a sketch below, the sketch is wrong: fix the note first.
5. **Move code in steps that each build and pass the area's tests.** One new class
   per commit where possible.
6. **Update callers.** No forwarders (rule 1 below).

## Rules

1. **No forwarders.** A method whose body is one call to a component is removed,
   and its callers call the component: `host.GetConnectors().ConnectSerialPort (...)`.
   The exception is an interface the class implements for other code
   (`MemoryDevice`, `IMachineState`, `IDiagnosticsProvider`).
2. **No friends.** Use a scoped interface. This plan removes `ReverseStepSearch`'s access to
   `ReverseController`'s private members.
3. **A callback interface with more than about four methods marks a wrong seam.**
   Stop and find the object the callbacks are really operating on.
   `IDisk2IwmOwner` (ten methods) is the example, and this plan removes it.
4. **A component has references to the parts it uses**, not to the whole class it
   came from. A component constructed with `MachineHost &` or
   `ReverseController &` is a second file of the same class.
5. **The header shrinks with the `.cpp`.** Measure both before and after.
6. **Saved-state bytes do not change.** No tag, version or field order changes.
   Each component writes its fields at the same place in the stream as today.
7. **Never delete unused library code.** Move it.
8. **Tests move with the code.** A new class that has logic gets direct
   tests where they are cheap (pure math, a ring, a table). Tests that call a
   removed method on the old class call the component instead.
9. Prose rules, banner placement, EHM, alignment, `.vcxproj` entries, commit
   format (subject only, `git commit -F`, no attribution trailer, never
   `--no-verify`), never commit `foo`, never set CL_MPCount.

## Guards

- **State characterization, before the first move.** Add a test
  (`UnitTest/EmuTests/MachineStateGoldenTests.cpp`) that, for each shipped machine,
  runs a fixed script (boot, a mounted pattern disk spinning, a reset, a power
  cycle), saves the state at fixed positions and compares
  `KeyframeStore::ComputeChecksum` of the bytes with constants in the test. Record
  the constants from `4b20fcd8c`. Every later commit must pass it without a
  constant changing. This is the proof for rule 6.
- **Replay tests** are the net for timing: `DiskHistoryTests`,
  `ReverseReplayTests`, `ReverseSessionStopTestTests`, `CallStackHistoryTests`,
  `HeatHistoryTests`, `TraceHistoryTests`, `ProfileHistoryTests`.
- **Per-instruction cost.** `MachineRunner::FinishStep`, `Cpu::StepOne` and
  `Disk2Controller::Tick` run once per instruction. Measure before and after with
  `MachineRunCostTests`, pinned to one CCD (see the test's banner). A slowdown over 2% is a defect.
- **After a class layout change**, build with `-Target Rebuild`.
- **Per commit**: Release build, the area's tests, `CheckStyle -Mode Staged`,
  `FixDeclAlign -Apply -Paths <one file>`, `Check-Prose.ps1` on new comments and
  on the commit message.
- **Before each push of `035-debugger`**: the full suite and `CheckStyle -Mode Tree`.

## Order

1. Guards (the golden test, the perf baseline).
2. `Disk2Controller` (the first cut is wrong, and `MachineHost` storage code calls it).
3. `MachineHost`.
4. `KeyframeStore`.
5. `ReverseController`.
6. `Cpu`.
7. `AppleWinParser`.
8. The storage hardware (section 7). Do 7a's Disk II part with step 2, and 7c with step 3.

Push after each class once the full gates pass, so branch `050-apple-III` can
merge in stages.

## Design notes

Each sketch is the intended result. Step 1 to 4 of the method confirms or corrects
it against the code.

### 1. Disk2Controller

**Problem.** The class is five things: the slot's I/O device, the Disk II card's
sequencer, two drives, the debugger's nibble and access logs, and the //c's IWM front end. The
first cut moved the IWM front end out behind `IDisk2IwmOwner`, whose ten callbacks
all operate on a drive, on the lines both front ends share, or on the reports. The
missing objects are the drive and the shared lines.

**State, at `4b20fcd8c`.** "Card" is the Disk II path (`Read`, `Write`, `Tick`,
`HandleSwitch`); "IWM" is `Disk2IwmPort` through `IDisk2IwmOwner`. Everything
runs on the CPU thread; `TryPeekIo` and `GetDiagnostics` run there too.

| Field | Written by | Read by | Group |
|---|---|---|---|
| `m_slot`, `m_ioStart`, `m_ioEnd` | constructor | `Read`, `Write`, `TryPeekIo`, `GetStart`, `GetEnd` | controller |
| `m_iwmMode`, `m_portHook` | `SetIwmMode`, `SetPortHook` | `Read`, `Write`, `TryPeekIo`, `Tick`, `PollFloppyEnable`, state | controller |
| `m_quarterTrack[d]` | `MoveHeadToPull`, `Reset`, `LoadState` | engines, diagnostics, access log, state | drive |
| `m_visits[d]` | `MoveHeadToPull`, `SetExternalDisk` | `GetVisitCount` | drive |
| `m_access[d]` | read path (card and IWM), `CountLatchLoad`, motor off, `SetExternalDisk` | `GetAccessLog` | drive |
| `m_isWriteBlocked[d]` | `CountLatchLoad`, `SetExternalDisk` | `IsWriteBlocked` | drive |
| `m_disks[d]`, `m_activeDisk[d]` | `MountDisk`, `EjectDisk`, `SetExternalDisk`, `PowerCycle` | every path that tests write protection or a loaded disk | drive |
| `m_engine[d]` | every path | every path, state | drive |
| `m_phases`, `m_phase` | `HandlePhase` (card and IWM), `Reset`, `LoadState` | head moves, diagnostics, state | lines |
| `m_motorOn` | card switch 9, card `Tick`, IWM `OnIwmMotor`, `Reset`, `LoadState` | head moves, latch loads, diagnostics, state | lines |
| `m_activeDrive` | card switches A and B, IWM `OnIwmSequencer` and `OnIwmPhases`, `Reset`, `LoadState` | every path | lines |
| `m_q6`, `m_q7` | card switches C to F, IWM `OnIwmSequencer`, `Reset`, `LoadState` | read dispatch, engine selection, diagnostics, state | lines |
| `m_motorSpindownCycles`, `m_motorSpinupRemaining` | card switches 8 and 9, card `Tick`, `PollFloppyEnable`, `Reset`, `LoadState` | card read dispatch, idle pump, diagnostics, state | card |
| `m_switchAccesses` | `HandleSwitch` | `GetSwitchAccessCount` | card |
| `m_cpuCycleSource`, `m_lastCpuSync` | `SetCpuCycleSource`, `CatchUpToCpu`, `AdvanceIwmClock`, `LoadState` | card catch-up, IWM clock, peek, state | clock |
| `m_peekBase`, `m_peekEngine`, `m_peekDrive`, `m_peekSync`, `m_peekDelta` | `PeekLatch` | `PeekLatch` | latch peek |
| `m_audioSink`, `m_eventSinks`, `m_motorOffFlushCallback`, `m_idleCallback` | setters | every report | signals |
| `m_cyclesSinceIdleCallback`, `m_busySinceIdleCallback` | `PumpIdleCallback`, `LoadState` | `PumpIdleCallback`, state | signals |
| `m_addrMarkWatcher`, `m_nibbleRecord`, `m_instructionAddressSource` | read and write paths, head moves, disk changes, `Reset`, `PowerCycle`, `LoadState` | diagnostics, `GetNibbleRecord`, state | recorder |
| `m_iwmPort` (the `Iwm`, two enables, IWM cycle count) | IWM path | diagnostics, state | IWM |

The IWM path writes the lines (`m_activeDrive`, `m_q6`, `m_q7`, `m_motorOn`) so
that diagnostics, state and `GetQuarterTrack` work in both modes, and it steps the
head through the card's `HandlePhase`. The lines are therefore a separate class, not part of
either front end.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `Disk2Drive` (two instances) | quarter track, `Disk2NibbleEngine`, the internal empty `DiskImage`, the active disk pointer, visit counts, `DiskAccessLog`, the write-blocked flag | One drive. `MoveHead (phases)` moves the head where the phases pull it and returns a `Disk2HeadMove` (from, to, bumped). `Mount`, `Eject`, `SetExternalDisk`, `HasExternalDisk`, `GetDisk`, `IsWriteProtected`, `HasDisk`, `NoteRead (nibble)`, `CountLatchLoad`, `EndRead`, `Reset`. `static GetPull` and the quarter-track constants are here; the `GetPull` tests move here. |
| `Disk2Lines` | phases, phase, motor flag, active drive, Q6, Q7 | The lines both front ends set and the controller reports: one copy. `SetPhase (phase, on)` and `SetPhases (phases)` latch the magnets and, with the motor on, move the selected drive's head, reporting the move to the recorder and the signals. `SelectDrive`, `SetMotorOn`, `SetQ6`, `SetQ7`, the queries, and the saved fields. Has references to the drives, the recorder and the signals. |
| `Disk2Signals` | audio sink, event sink slots, motor-off flush callback, idle callback and its window | Everything the controller reports outward: motor engaged and disengaged (with the flush), motor commands, head step and bump, drive select, write mode, write protect, disk inserted and ejected, and `PumpIdle (cycles, isBusy)`. Both front ends call it. |
| `Disk2Recorder` | `Disk2AddressMarkWatcher`, `Disk2NibbleRecord`, the instruction address source | `NoteRead (drive, index, nibble, flags, cycle)` and `NoteWrite (drive, index, nibble)`. Replaces the duplicated blocks in `HandleReadDispatch`, `Write` and the IWM path, and the virtual `OnLatchLoad`: its test reads the nibble record. |
| `Disk2Clock` | the CPU cycle source, the last synced cycle | `TakeElapsed (out)` returns whether there is a source and the cycles since the last call; both front ends use it, so the card's re-anchor after a power cycle and the IWM's are one rule. `GetPending` for a peek. |
| `Disk2LatchPeek` | the base copy, the run-on copy, drive, sync, delta | `Peek (engine, drive, sync, delta)`: the side-effect-free read of the data latch. |
| `Disk2CardSequencer` | spin-down and spin-up counters, the switch access count, the `Disk2LatchPeek` | The Disk II card's logic: `Read`, `Write`, `TryPeek`, `Tick` (motor timing), the read dispatch, the catch-up. Has references to the lines, the drives, the signals, the recorder and the clock. |
| `Disk2IwmPort` | the `Iwm`, the two enables, the IWM cycle count | The //c front end. Its two `IDiskDriveBus` adapters call `Disk2Drive` directly. It sets the lines and calls `Disk2Signals` and `Disk2Recorder`. `IDisk2IwmOwner` is deleted. |
| `Disk2Controller` | slot, I/O range, IWM mode flag, port hook, and the parts above | `MemoryDevice`, `IMachineState`, `IDiagnosticsProvider`. `Read`, `Write` and `TryPeekIo` pick the front end by mode, then call the port hook. `SetEventSink` sets the signals' slot and the watcher's, so it stays on the controller. Getters: `GetDrive (d)`, `GetLines()`, `GetSignals()`, `GetRecorder()`, `GetClock()`, `GetCard()`, `GetIwmPort()`. |

**State.** `Disk2Controller::SaveState` writes the same fields in the same order
from the parts' getters: phases, phase, both heads, motor, spin-down, spin-up,
active drive, Q6, Q7, IWM mode, the IWM mode register, the idle window, the last
sync, both engines, the watcher, the record, the IWM port. `LoadState` validates
as today, then sets each part. The golden test confirms the bytes.

**Callers.** `GetEngine (d)`, `GetDisk`, `MountDisk`, `EjectDisk`,
`SetExternalDisk`, `HasExternalDisk`, `GetVisitCount`, `GetAccessLog`,
`IsWriteBlocked` and `GetQuarterTrack (d)` become `GetDrive (d).X`.
`GetActiveDrive`, `IsMotorOn`, `GetQuarterTrack()`, `GetCurrentTrack`, `IsQ6`,
`IsQ7` and `GetPhases` become `GetLines().X`. `SetAudioSink`, `GetAudioSink`,
`SetMotorOffFlushCallback`, `SetIdleCallback`, `NotifyDiskInserted` and
`NotifyDiskEjected` become `GetSignals().X`. `GetNibbleRecord` and
`SetInstructionAddressSource` become `GetRecorder().X`. `SetCpuCycleSource`
becomes `GetClock().SetSource`. `IsMotorAtSpeed`, `GetMotorSpinupRemaining` and
`GetSwitchAccessCount` become `GetCard().X`. `GetIwm` becomes
`GetIwmPort().GetIwm()`.

### 2. MachineHost

**Problem.** The header is far too long. The class is the composition root and also
the run loop, host input, storage, muting, observer wiring, tape and a diagnostics
panel. The first cut left one-line forwarders for the connectors and the state
stream.

**State, at `27dd627d4`.** All on the CPU thread unless marked; "UI" is the UI
thread through `GetLifetimeLock`.

| Field | Written by | Read by | Group |
|---|---|---|---|
| `m_memoryBus`, `m_registry`, `m_interruptController` | constructor, builder | every part | parts |
| `m_cpu`, `m_prng`, `m_mmu`, `m_apple2cRomBank`, `m_mouse`, `m_accelerator`, `m_joyport`, `m_videoTiming`, `m_videoScanner` | the setters, from the builder on each machine switch | the run loop, input, reset, state, diagnostics | parts |
| `m_ownedDevices`, `m_ownedAciaEndpoints`, `m_videoModes`, `m_charRom`, `m_refs` | builder | every part; UI | parts |
| `m_softSwitches`, `m_lifetimeLock` | `SelectVideoMode`; both threads | renderer | host |
| `m_config`, `m_currentMachineName`, `m_assetBaseDir` | builder, shell | identity, state header, storage, diagnostics | host |
| `m_diskStore` | shell | state, storage, reset | host |
| `m_position`, `m_debugHook`, `m_stopFlag` (another thread raises it), `m_historyRecorder`, `m_watchOpcodes`, `m_watcher` | the setters, `FinishStep` | `StepOne`, `RunCycles`, `FinishStep` | runner |
| `m_inputJournal`, `m_hostInputGate` (UI and CPU), `m_heldInputWatch`, `m_hostInputParts` | the journal and watch setters, replay | `RecordInput`, `ApplyDeviceInput`, `SampleHostInputs`, host-input state, `RunCycles` | host input |
| `m_hostBlockFileIo`, `m_blockFileIo`, `m_blockProbe`, `m_blockMediaProblems`, `m_stockImageFolder` | `SetBlockMediaFileIo`, builder, `SetStockImageFolder` | the builder, `InsertFloppy35`, `AttachHardDisk`, the replay media | storage |
| `m_isSoundMuted`, `m_isPrinterMuted`, `m_mutedDiskAudio`, `m_wasMotorOnAtMute` | `SetOutputMuted` | the queries | output mute |
| `m_commSink` | `AttachCommEventSink` | `AttachInputJournal` (again after a switch) | observer wiring |
| `m_tapeDeck`, `m_cassetteDiagnostics`, `m_tapeResetCount` (read on UI) | `StopTape` | the shell, the cassette panel | tape |
| `m_speedMode` | `SetSpeedMode` | the clock panel | clock panel |
| `m_printerRing` | printer card, drain thread | connectors | host |
| `m_romIdentity`, `m_connectors`, `m_stateStream` | first cut | first cut | as they are |

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `MachineParts` (struct, in a separate header) | the bus, registry, interrupt controller, the replaceable parts in `unique_ptr` slots, the video scanner, the owned devices and endpoints, the video modes, the character ROM, `MachineRefs` | What the machine is made of. A component that uses a part has a reference to this struct, so it reads the slot after a machine switch replaced it. |
| `MachineHost` | `MachineParts`, config, name, asset folder, disk store, printer ring, soft-switch mirror, lifetime lock, and the components below | What the machine is: getters for the parts and the components, the part setters, `SoftReset` and `PowerCycle`, which touch every part, `NoteDebuggerEdit` and the media-change callback (the history recorder takes the host). `SetHeldInputWatch` sets the watch on the host input, which points the devices at it, and on the runner, which ends a run on a hit. `AttachDevices` attaches the journal and the comm sink to the devices of a machine the builder switched to. Implements `IMachineStateParts` for the state stream; nothing else. |
| `MachineRunner` | position, debug hook, stop flag, history recorder, opcode watch, the `HeldInputWatch` pointer | `StepOne`, `RunCycles`, the hook path, `FinishStep`, the card catch-up. Has references to the parts and the connectors. |
| `MachineHostInput` | `InputJournal`, `HostInputGate`, the `HeldInputWatch` pointer, the host-input part list | `Record`, `SetJournalOn`, `AttachJournal`, `ApplyDeviceInput`, `Sample`, `SaveState`, `LoadState`, `DropConnections`, `ResyncConnections`, `SetHeldInputWatch`. Has references to the parts and the runner's position. |
| `MachineStorage` | the block media file I/O and probe, the problem list, the stock folder | `FindFloppy35`, `InsertFloppy35`, `EjectFloppy35`, `AttachHardDisk`, `ConnectDiskToController`, `BindDiskDrives`, and `MachineReplayMedia`'s `Get`, `Copy` and `Mount` as methods. `MachineReplayMedia` is deleted. Has references to the parts, the config and the disk store. |
| `MachineOutputMute` | the two mute flags, the muted drive audio sink, the motor flag at mute | `SetMuted` and the queries. Has references to the parts and the connectors. |
| `MachineObserverWiring` | the comm sink | `AttachObservers`, `AttachCommEventSink`, `AttachAgain` (after a machine switch). Has a reference to the parts. |
| `MachineTape` | `TapeDeck`, `CassetteDiagnostics`, the reset count | `GetDeck`, `Stop`, the count, the cassette panel. `CassetteDiagnostics` takes the deck and the refs, not the host. |
| `MachineClockPanel` | the speed mode | The clock panel (`IDiagnosticsProvider`) and `GetProviders`, the list of every panel. `MachineHost` no longer implements `IDiagnosticsProvider`. |
| `MachineConnectors`, `MachineStateStream`, `MachineRomIdentity` | as now | Every forwarder on `MachineHost` is removed: `SetSerialPort` through `DescribeAttachmentFailure`, `SaveState` through `CheckSharedSave`, `MediaIds`, `kStateTag`, `kStateVersion`, `kOldestStateVersion`. |

**Member order.** The parts, then the fields of the host, then the components in
the order they refer to each other: connectors, state stream, runner, host input,
storage, output mute, observer wiring, tape, clock panel.

**Callers.** `GetConnectors().X`, `GetStateStream().X`, `GetRunner().X` (`StepOne`,
`RunCycles`, `SetDebugHook`, `GetPosition` and the rest), `GetHostInput().X`
(`GetJournal`, `GetGate`, `Record`, `Sample`), `GetStorage().X`,
`GetOutputMute().X`, `GetObserverWiring().X`, `GetTape().X`, `GetClockPanel().X`.
`AttachInputJournal` after a machine switch is `MachineHost::AttachDevices`.

**Not in this plan.** `MachineRefs` (a struct of raw pointers the builder fills)
is a separate design problem and is out of scope. Add what you find to tasks.md.

### 3. KeyframeStore

**Problem.** One class has the storage ring, the layout math, the background
packing pipeline and the public API. The first cut moved the codec, the easiest
part.

**State, at `45f9c5c6b`.** All on the caller's thread (the one that runs the
machine) except the jobs, which a worker writes while they are in flight.

| Field | Written by | Read by | Group |
|---|---|---|---|
| `m_settings` | `Configure`, `ChangeBudget` | the layout, `IsDue`, the drops | store |
| `m_entries`, `m_first`, `m_count` | `Reserve`, `Relayout`, `SubmitJob`, `Collect`, the drops, `Clear` | every query | arena |
| `m_arena`, `m_arenaBytes`, `m_arenaEnd` | `Reserve`, `Relayout`, `Place`, `DropNewerThan`, `Clear` | `TryFindRoom`, `Restore`, `CopyPacked`, `GetSide` | arena |
| `m_storedBytes`, `m_sideBytes`, `m_isFull` | `Collect`, the drops, `ChangeBudget`, `Clear` | the budget, the meters | arena |
| `m_groupLength`, `m_wholeBytes`, `m_groupWhole`, `m_groupDiffs` | `SubmitJob`, `Collect`, `ReloadLatestWhole`, `Clear` | `IsGroupDone`, the layout, `GetByteCount` | arena (the newest group) |
| `m_pendingCount` | `SubmitJob`, `Collect`, `DropPending`, `Clear` | `TryFindRoom`, `TryDropOldestGroup`, `GetSide`, `CopyPacked` | queue, and the arena's unplaced entries |
| `m_jobs`, `m_nextJob`, `m_queue` | `TakeJob`, `SubmitJob`, `RunJob` (worker), `SetWorkQueue` | `Collect`, `GetSide` | queue |
| `m_codec` | `RunJob` (worker, while a job is in flight), `Restore`, `ReloadLatestWhole`, `Clear` | as written | queue |
| `m_nextDueCycle`, `m_wallClock`, `m_dropListeners` | `ScheduleAfter`, setters | `IsDue`, `SubmitJob`, every drop | store |

`m_pendingCount` is two facts that move together: the jobs in flight, and the
newest entries listed without their packed bytes. The queue counts the first and
the arena counts the second; only `KeyframeStore` changes either, and it changes
both in the same call (`Submit` lists an entry and submits its job, `Collect`
takes a job and places its entry, `DropPending` drops both).

**Classes.**

| Class | State | Thread | Responsibility |
|---|---|---|---|
| `KeyframeLayout` | none (static functions of `KeyframeSettings` and sizes) | any | `GetStateRoom`, `GetPackedBound`, `GetGroupSlots`, `GetLeastArena`, `ComputeLayout`, `ComputeFitLayout`, `IsGroupDone`. Direct tests. |
| `KeyframeArena` | the entry ring, first, count, the unplaced count, the arena and its size and end, stored and side byte totals, the newest group's counters, the full flag, the drop callback | caller | `Allocate`, `Relayout`, `List`, `PlaceNext`, `DropUnplaced`, `TryDropOldestGroup`, `DropOldestGroups`, `DropNewerThan`, `RecountNewestGroup`, `Clear`, and the queries: `GetCount`, `GetUnplacedCount`, `GetInfo`, `GetPacked`, `GetSide`, `TryFindAtOrBefore`, `TryFindByPosition`, `FindGroupStart`, the byte counts. Direct tests with small arenas. |
| `KeyframePackQueue` | the jobs, next job, pending count, the work queue, the `KeyframeCodec` | caller submits, worker packs | `Reserve`, `Release`, `SetWorkQueue`, `IsFull`, `GetNext`, `Submit`, `TryTakeDone`, `WaitAll`, `DropAll`, `GetPending`, the codec. |
| `KeyframeCodec` | as now | worker during a job, caller otherwise | as now |
| `KeyframeStore` | settings, the drop listeners, the next due cycle, the wall clock, the arena, the pack queue | caller | `Configure`, `ChangeBudget`, `Clear`, `Release`, `Add`, `Capture`, `WaitForPending`, `Restore`, `RestoreAtPosition`, `CopyPacked`, `DoesStateMatch`, `TruncateAfter`, `DropAfterPosition`, the newest keyframe's marks, `GetSide`, `GetReservedBytes`, `IsDue`, `GetArena`. Composes the parts. |

**Invariant, in the headers.** The codec's buffers and a job's are touched by
the worker only while the job is in flight, and by the caller only while none
is. The arena is touched by the caller's thread only. `Collect` is the one place
results cross.

**Callers.** `GetCount`, `GetPendingCount` (now `GetUnplacedCount`), `GetInfo`,
`GetSideByteCount`, `GetByteCount`, `GetUsedBytes`, `IsFull`, `GetCapacity`,
`GetArenaBytes`, `TryFindAtOrBefore` and `TryFindByPosition` move to
`GetArena()`. `ComputeChecksum` and `XorBytes` callers use `KeyframeCodec::`.
`KeyframeStore::kBufferCount` is `KeyframeLayout::kJobCount`.

### 4. ReverseController

**Problem.** One class had the recording state machine, keyframe capture,
seeking, the step table, the step-back commands and divergence handling.
`ReverseStepSearch` read its private members as a friend.

**State, at `aa449d56d`.** All on the thread that runs the machine, except
`m_control`, which the shell sets and reads from the UI thread.

| Field | Written by | Read by | Group |
|---|---|---|---|
| `m_isRecording`, `m_isLive`, `m_isPaused`, `m_isEditPending`, `m_pauseStart`, `m_liveEndPosition`, `m_liveEndCycle`, `m_hereStart`, `m_hereEnd` | `Start`, `Stop`, `SetUserMaximumSpeed`, `LeaveLive`, `BecomeLive`, `OnMachineEdited`, `CaptureBoundary`, `ReplayHere` | every command, the search | span |
| `m_steps`, `m_stepsStart`, `m_stepsEnd`, `m_hasSteps`, `m_tableBuilds` | `BuildStepTable`, `DiscardStepTable` | `GetStep`, `LandAt`, `GetRecentSteps`, the search | step table |
| `m_writer`, `m_hostWriter`, `m_side`, `m_observer`, `m_keyframeListeners`, `m_workQueueOverride` | `CaptureNow`, setters | `CaptureNow`, the replayer's load callback | capturer |
| `m_isCut`, `m_cutResult`, `m_isStopped` | `BuildStepTable`, the search | the step commands | stepper |
| `m_lastDivergence`, `m_cutCount`, `m_control` | `HandleDivergence`, setter | the debugger, `IsStopDue` | seeker |
| `m_search` (caller probes and links, stack pointers) | the search | the search | stepper |
| `m_liveListeners`, and the base class's `m_nextDueCycle`, `m_isArrivalDue`, `m_pagesRun` | `BecomeLive`, `LeaveLive`, `ScheduleCaptures`, `CaptureNow` | the per-instruction hooks | controller |
| `m_workQueue`, `m_keyframes`, `m_timeline`, `m_replayer` | as now | every part | parts |

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `HistorySpan` | recording, live, paused, pause start, edit pending, live end position and cycle, the stretch the machine last ran in | The recording state machine: `Start`, `Stop`, `SetPaused`, `Leave`, `BecomeLive`, `NotePause`, `MarkEdit`, `TakeEdit`, and queries that take the machine's position or cycle. Pure state, with direct tests. |
| `ReplayStepTable` | the steps, their start and end, the build count | `BeginFill`, `Commit`, `IsBuiltFor`, `Contains`, `Leads`, `Get`, `GetRecent`, `Discard`. Direct tests. |
| `KeyframeCapturer` | the two `StateWriter`s, the side buffer, the observer, the keyframe listeners, the work queue override | `CaptureNow`, `CaptureBoundary`, `UseWorkQueue`, and the activity, journal and retained-media bookkeeping of each capture. A callback after each capture schedules the next. |
| `ReverseSeeker` | the replay control, the last divergence, the cut count | Moves the machine between live and history: `SeekToPosition`, `SeekToCycle`, `StepForward`, `ReverseContinue`, `LandAt`, `LeaveLive`, `BecomeLive`, `ReplayHere`, `OnMachineChanged`, `HandleDivergence`, `IsStopDue`, and the position queries. A callback on each change between live and history. Implements `IReverseSeeker`. |
| `ReverseStepper` | the caller probes and links, the stack pointers, the cut flag and result, the stopped flag | `StepBack`, `StepBackOver`, `StepBackOut`, `StepBackCycles`, `PrepareRecentSteps`, `GetRecentSteps`. Absorbs `ReverseStepSearch`, which is deleted. Uses `IReverseSeeker`, the step table and the timeline. |
| `HistoryTimeline` | as now | with `GetOldestPosition` |
| `ReverseController` | the parts above, `KeyframeStore`, `Replayer`, the work queue, the live listeners | `Start`, `Stop`, `SetUserMaximumSpeed`, `ChangeBudget`, `IsRecordingLive`, the `HistoryRecorder` callbacks, the capture schedule, and getters for the parts. |

**`IReverseSeeker`.** Six methods: `LeaveLive`, `LandAt`, `SeekToPosition`,
`SeekToCycle`, `HandleDivergence`, `IsStopDue`. More than four, but each is one
of the moves the stepper makes, and none goes past the seeker; the cut state
moved to the stepper, since only the step commands read it.

**Callers.** The step commands, `PrepareRecentSteps`, `GetRecentSteps` and the
caller probes use `GetStepper()`. `SeekToCycle`, `SeekToPosition`, `StepForward`,
`ReverseContinue`, `OnMachineChanged`, the position queries, `GetLastDivergence`,
`GetCutCount` and `SetReplayControl` use `GetSeeker()`. `IsInHistory`,
`IsPaused` and `IsRecording` use `GetSpan()`. `GetWallTimeAt` and
`GetWallTimeAtPosition` use `GetTimeline()`. `HasStepTable` and
`GetTableBuildCount` use `GetStepTable()`. `SetHistoryObserver`,
`SetKeyframeListener` and `SetWorkQueue` use `GetCapturer()`.
`ReverseController::CallerProbe` is `ReverseStepper::CallerProbe`, and
`ReverseController::KeyframeListener` is `KeyframeCapturer::KeyframeListener`.

### 5. Cpu

**Problem.** The execution core also has the instruction trace ring and its file
dump, the fetch observers, and a step printer that nothing calls.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `CpuTrace` | the entry ring, capacity, head, count, enabled, the pending interrupt tag, the cycle sources | `Enable`, `Stop`, `Restart`, `PushNext` (inline, returns the next entry to fill), `RecordAccess`, `MarkInterrupt`, `GetSize`, `TryGetEntry`, `DumpToFile`, and `DumpRecent` for an illegal opcode. `CpuTrace::Entry`, `kIntrNone`, `kIntrIrq`, `kIntrNmi` and `kDefaultLookback` are declared here. Takes the `CpuFetchObservers` to set its trace flag and the instruction set for mnemonics. |
| `CpuFetchObservers` | the opcode watch tables, the watcher, the fetch sink, the observed flag | `SetOpcodeWatch`, `SetFetchSink`, `SetTraceOn`, `IsObserved`, and the inline `ObserveWatch`, `ObserveSink` and `ObserveInterrupt`. `IOpcodeWatcher` and `IFetchSink` are declared here. |
| `CpuStepPrinter` | none | The three print functions as statics taking a `Registers` struct, the instruction set and a byte reader. It has no caller and stays compiled. |
| `CpuOperandInfo` | location, effective address, operand | A public struct in a separate header, used by the operand fetch and the printer. |
| `Cpu` | registers, status, memory, read pages, the instruction set, the trace peek pages, last-instruction facts, the two parts | `StepOne`, operand fetch, execute, stack, memory access, `LoadBinary`, `TracePush`, `PeekForTrace`, and `GetTrace` and `GetFetchObservers`. |

**Corrections to the first design.** The peek pages stay in `Cpu`, since the
operand bytes are read through the CPU's read table, and `TracePush` stays a
`Cpu` member that fills the entry `PushNext` returns, so the trace doesn't access the
registers. `RecordAccess` takes the PC of the next instruction, so the trace
doesn't read the CPU. The members are declared after `instructionSet`, so the
trace is constructed with it.

**Hot path.** `ObserveFetch` is `__forceinline` and tests one flag per
instruction. After: //e 5.09 ns, //c 10.34 ns, every storage
card 14.91 ns per cycle.

**Callers.** `EnableTrace`, `StopTrace`, `RestartTrace`, `GetTraceSize`,
`TryGetTraceEntry`, `DumpTraceToFile` and `RecordTraceAccess` callers (the
debugger's trace pane, `TraceController`, `TraceEventProbe`, `MemoryBusCpu`)
use `GetTrace ()`; `SetOpcodeWatch` and `SetFetchSink` callers use
`GetFetchObservers ()`. `Cpu::TraceEntry` is `CpuTrace::Entry`.

### 6. AppleWinParser

**Problem.** One static class parsed every command family. The first cut moved the
breakpoint family and added forwarders.

**Classes.** One parser per set of families in `AppleWinCommandFamily`, with the
dispatch in `AppleWinParser::TryParseArguments`.

| Class | Contents |
|---|---|
| `AppleWinArguments` (struct, in a separate header) | the command entry, tokens, rest, context, and the `Tokens` type |
| `AppleWinArgumentForms` | `TryEvaluate`, `GetTextAfterFirstWord`, `TryParseRange`, `TryParseSourceLine`, `TryParseValues` and `ValueWidth`, `TryParseCount`, `TryParseDecimal`, `IsNumberOrRange`, `TryParseIdOrAll`, `TryParseSearchItems`, and the private `TryParseSearchWord`. Direct tests in `AppleWinArgumentFormsTests`. |
| `AppleWinText` | as now |
| `AppleWinBreakpointParser` | the Breakpoints and Video families, `TryParseIfClause`, `TryParseCondition`, `TryMakeBreakpoint` |
| `AppleWinMemoryParser` | the Memory, Assembler and Data families; `IsBlockName` is private |
| `AppleWinRunParser` | the Cpu, Flags and Engine families, with register, skip, calls and history arguments private |
| `AppleWinViewParser` | the Watch, ZeroPage, Bookmarks, Symbols and Output families, and `PANEL`, which the Engine family's parser calls; the BASIC reference form is private |
| `AppleWinShorthand` | `TryParse` and the private `TryParseMove`: the Monitor forms in place of a command |
| `AppleWinParser` | `Parse` and the dispatch by family. No argument parsing. |

**Callers.** `AppleWinParser::TryParseIfClause`, `TryParseCondition` and
`TryMakeBreakpoint` are removed. `WinDbgParser.cpp`, `GSSquaredParser.cpp`,
`BreakpointDefinitionCheck.cpp`, `DebuggerViewState.cpp`, the handlers, the heat
map's range sets and `DirectPaneActionTests.cpp` call `AppleWinBreakpointParser`
or `AppleWinArgumentForms` directly. Header 122 to 55 lines, `.cpp` 1,910 to 114.

### 7. The storage hardware from master (state and peek work, 2026-11-17)

These classes are under the size limit, so CS0026 never flagged them. The state
work (T1525 to T1531) and the peek work (T1532) were added to them quickly and
have design problems of the same kind. Classes: `Iwm`, `Ncr5380`, `ScsiBus`,
`AppleScsiCard`, `ProFileCard`, `ProFileDrive`, `LironCard`, `SmartPortHost`,
`Apple35ControllerCard`, `Apple35CardMemory`, `Apple2cSmartPort`,
`Apple2cPlusDiskPort`, `Sony35Drive`, `BlockDeviceMedia`, and the interfaces
`IDiskPortHook` and `IExpansionRomCard`.

**7a. One evaluation for a read and its peek.** Each `Peek...` function is a second
copy of its `Read...` function, rewritten to predict the side effects.
`Ncr5380::PeekRegister` and `ReadRegister` are the same switch twice, with
`WouldTakeBus` and `PeekBusAndStatus` as predictions of `UpdateArbitration` and
`GetBusAndStatus`. Only the tests check that the two return the same byte. Redesign each register read
as two steps:

- `Evaluate (reg) const` returns the byte and a small struct of the effects the
  read has (interrupt cleared, arbitration taken, byte consumed, counter moved).
- `Read` calls `Evaluate` and applies the effects. `Peek` calls `Evaluate` and
  discards them.

`Iwm::GetSwitchesAfter` is already this form; make `Iwm::Access` use it, so the
switch logic exists once. Apply the same form to `Ncr5380` (registers, DMA, DRQ),
`ProFileCard` and `ProFileDrive`, the slot ROM and expansion ROM accesses of the
four cards, `Apple2cSmartPort`, `Apple2cPlusDiskPort`, and the Disk II card
sequencer's `TryPeekIo` (section 1). `IDiskPortHook` then has one evaluation
method in place of `OnIwmAccess` plus `TryPeekIwmAccess`, and
`AreFloppiesEnabled` plus `PeekFloppiesEnabled`. Where a peek cannot be worked
out today (a status read that moves a phase), the evaluation form usually makes
it possible; remove each `return false` that it makes unnecessary.

**7b. `BlockDeviceMedia` has three modes behind flags.** A file, a decoded WOZ and
a replay copy are distinguished by `m_file`, `m_isWoz` and `m_isReplayCopy`, and
`ReadBlock`, `WriteBlock`, `LoadState`, `Flush` and `Detach` each branch on them.
Redesign as a block source interface with three implementations (file, WOZ
blocks, shared copy) and one class for what is common: the path, the one-drive-per-path rule, the
write protection, the written-block overlay and its state. `Sony35Drive` has the
same two-way branch (WOZ image or block media) in `Insert`, `Eject`, `Flush`,
`SaveState`, `LoadState` and the replay copy. It becomes one disk interface with
two implementations.

**7c. `MachineHost` lists four card types to catch them up.** `CatchUpCards`
loops over `lironCards`, `apple35Cards`, the //c SmartPort and the //c Plus port
by type. Add an interface (`ICatchUpDevice`, one method) and one list in
`MachineRefs`, as `serialChips` is, filled by the builder. `MachineReplayMedia::Get`
has the same list by type; the cards should report their `IReplayMedia` drives
through an interface, and section 2's `MachineStorage` collects them.

**7d. `IMachineState::HasMedia`.** A virtual on the core state interface, added so
a picture load can skip the storage cards. Either it stays, or
`MachineStateStream` tests something narrower (for example, whether
the part implements the interface from 7c that reports drives). Choose one. One mechanism,
not two.

**7e. State code written from field lists.** Each `SaveState` and `LoadState` in
these classes was generated from the member list. Check each against the class's
invariants: fields that are caches (rebuilt on load) and must not be saved, fields
validated on load, and pointers encoded as IDs (`Apple2cPlusDiskPort::PortDrive`'s
driven ID). The byte layout is fixed by rule 6 from `4b20fcd8c` on; a layout
change found necessary here requires the owner's decision and a version bump.

**Tests.** The peek tests from T1532 (`IwmTests`, `Ncr5380Tests`, `ProFileTests`,
`AppleScsiCardTests`, `Apple35ControllerCardTests`, `SmartPortBusTests`,
`IoPeekTests`) and its mutation checks are the net for 7a. `BlockDeviceMediaTests`
and the replay tests are the net for 7b.

## The overseer

The session that implements this plan is also the overseer of every T1533
refactor except Dxui. The owner's decision, 2026-11-18: one overseer for the
rules, the interfaces between groups and the merge order; Dxui stays separate.

**Duties.**

1. **One standard.** The Method, Rules and Guards sections above apply to every
   group. Each group writes a design note in the form of the notes above (the
   state table, the classes, the callers that change) before it moves code, and
   the overseer reads it first.
2. **Core first.** The classes in sections 1 to 7 are called by the debugger
   groups. Publish each new component's getter and method list (in this file)
   as soon as its design note is final, before the code lands, so the debugger
   groups write against the new calls.
3. **One design for the debugger pair.** The debugger UI files and the debug
   engine files call each other (`DebuggerViewState`, `ShellDebugger`,
   `DebugSession`). They get one joint design note, section 8 below, written
   before either side continues, whoever implements it.
4. **No duplicate classes.** Before a new class is added anywhere, search the
   tree and the other groups' notes for one with the same job.
5. **Merges go through the overseer.** A branch merges into `035-debugger` only
   after its design note is reviewed and its seams pass rules 1 to 5. After a
   merge, redo this plan's call-site renames in the merged files, build, and run
   that area's tests.
6. **Record who has which files** in `CLAUDE.md` under "Who is where" while more
   than one session is writing.

### 8. The debugger pair (to be designed)

Files: `DebuggerViewState.cpp`, `DebuggerViewStateTests.cpp`,
`DebuggerWindow.cpp`, `DebuggerWindowInput.cpp`, `DebuggerActions.cpp`,
`SourcePane.cpp`, `DisassemblyViews.cpp`, `BreakpointHandlers.cpp`,
`DebugSession.cpp`, `ShellDebugger.cpp`, `HeatMapView.cpp`, `SoundBoardView.cpp`,
`HeatHistoryTests.cpp`, `OperandResultTipTests.cpp`.

Existing work, not merged, for reference and for the parts that pass the rules:

- `refs/heads/claude/035-split-debugger-ui-2c6403`, stopped 2026-11-18 at
  `174f0f731` (`3cba8b5b3` plus a blank-line fix), based on `276f8eb7e`. Its report:
  - The tip builds in Release. Its filtered debugger tests are green except three
    that fail at its base too; `09d81afd9` on `035-debugger` fixed those.
  - All eight files are under the limit, with no forwarders: callers use the new
    classes directly.
  - Rated sound:
    - view state: `DebuggerViewSnapshot`, `CodeViews`, `MemoryWindowViews`,
      `DevicePanelViews`, `HeatMapReader`, `AutoWatchBuilder`,
      `DebuggerViewCommands`, `DebuggerCommandLines`, `WatchEdits`,
      `OpenViewsText`, `MemoryGoTo`, `InstructionLookup`, `CodeAnnotations`
    - breakpoints: `BrkHandlers`, `BreakpointGroupHandlers`
    - disassembly: `DisassemblyViewPane`, `DisassemblyBranchArrows`
    - source: `SourceLineText`, `SourceBannerText`, `SourceRowStyle`,
      `SourceRows`, `SourceLineLookup`, `SourceMacroPlaces`
    - actions: `RunActions`, `BreakpointActions`, `WatchActions`,
      `MemoryActions`, `SymbolActions`, `SessionActions`, `DebuggerKeyActions`,
      `SavedActionText`
    - window: `TextZoomLevels`, `OpenViewsKeeper`, `CommandBarController` with
      `ICommandBarHost`, `PaneContentMouse` with `IPaneContentMouseHost`
  - Rated chosen for line count: `ControlTreeWalk`, `GoToAddressText`,
    `DebuggerWindowText`, `DensePaneList`, and `ScrollCodeAtEdge` and
    `SelectCodeViewEdge` moved into `DisassemblyGroup`.
  - Not done: the front half of `RouteWindowMouse` and `RenderFrame` are long
    functions in the window.
  - It edits many files outside its group, nearly all one-line qualifier renames:
    `ShellDebugger.cpp`, `ShellDebuggerProject.cpp`, `DebugProject.cpp`, the
    other `DebuggerWindow*.cpp` files, most files under `Panes/`, the history
    files, and much of `UnitTest/DebuggerTests`.
    `git diff --name-only 276f8eb7e..174f0f731` lists them.
  - Its edits to the debug engine group's files are call-site renames only.
    `ShellDebugger.cpp` gets an include of `DebuggerViewCommands.h`, and its
    `m_debugViewState.X` calls go through `GetCodeViews()`, `GetMemoryWindows()`,
    `GetPanels()`, `GetHeatMap()` or `GetCommands()`. `OperandResultTipTests.cpp`
    has `SourcePane::` qualifiers changed to `SourceRows::` and `SourceLineText::`.
    It edits nothing under `Dxui/`.
  - These ratings are that session's. The overseer checks each class against
    rules 1 to 5 before taking it.
- `refs/heads/claude/035-split-debug-engine-080423`, stopped 2026-11-18 at
  `1927160e4`, based on `276f8eb7e`, worktree clean. From the branch itself:
  - All six of its files are under the limit.
  - New classes:
    - from `DebugSession`: `CallRecordKeeper`, `DebugStopMonitor`,
      `BeamBreakMonitor`, `DebugLineInterpreter`, `DebugProgramFiles`,
      `DebugHostFiles`, `DebugProjectMerge`, `HistoryDivergence`
    - from `ShellDebugger`: `ShellDebugView`, `DebugViewModel`,
      `DebugWindowMailbox`, `ShellDebugProject`, `ShellHistory`, with
      `IShellDebugContext`
    - from `HeatMapView`: `HeatMapGrid`, `HeatMapCells`, `HeatMapCellText`,
      `HeatMapBands`, `HeatMapSections`, `HeatMapFrame`, `HeatMapPainter`,
      `HeatMapPicker`, `HeatMapControls`, `HeatMapTips`
    - from `SoundBoardView`: `SoundVoiceCard`, `SoundBoardInk`
    - tests: `OperandTipWindowTests`, `HeatChannelHistoryTests`,
      `HeatHistoryChecks.h`
  - Its session rated `CallRecordKeeper` and others as sound and some boundaries
    as weaker. The full ratings, its build and test results and its list of
    files outside the group are in that session's last report, not here: read
    the report, or check each class against rules 1 to 5.
  - It edits `ShellDebugger.cpp`, which the UI branch also edits for renames.
- `specs/044-debugger-window-refactor/` has an earlier plan for `DebuggerWindow`
  (pane interface, host interface, pane list). Use it.

Write the joint note with the method above: state tables for `DebugSession`,
`ShellDebugger`, `DebuggerViewState` and `DebuggerWindow` first, since the other
files hang off those four. Take from the two branches the classes that pass rules
1 to 5 as they are, and redesign the rest.

### Dxui (separate)

`claude/035-split-dxui-b5f537`: `DxuiTextView.cpp`, `DxuiDockSite.cpp`,
`DxuiWindow.cpp`, `DxuiPaneLayout.cpp`, `DxuiTooltip.cpp`, `DxuiImageStrip.cpp`.
The owner runs this one apart from the overseer. Its session wrote
`group3-redesign-plan.md` for `DxuiTextView` and `DxuiDockSite`. Dxui's public
control APIs are the only contact with the other groups: if one changes, the
Dxui session reports the change, and the overseer updates the callers after the
merge. Do not edit files under `Dxui/`.

## Done

- Each of the six classes has its final design note in this file, with the state
  table.
- No forwarder, no friend, no callback interface over four methods in the six.
- The golden state test is green with the constants from `4b20fcd8c`.
- Per-instruction cost within 2% of the baseline.
- The full suite passes and `CheckStyle -Mode Tree` is clean for these files.
- `specs/035-debugger/tasks.md` T1533 has a line per class with what it became.
- Section 8 has its design note, and the debugger pair's files pass the same rules.
- `035-debugger` is pushed, and session 050-apple-III has the message to merge it.
