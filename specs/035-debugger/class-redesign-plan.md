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
- **Per-instruction cost.** `MachineHost::FinishStep`, `Cpu::StepOne` and
  `Disk2Controller::Tick` run once per instruction. Measure before and after with
  the method in memory note `reference-measure-per-instruction-perf`, pinned to
  one CCD. A slowdown over 2% is a defect.
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
all operate on a drive or on the notifications. The missing object is the drive.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `Disk2Drive` (two instances) | quarter track, `Disk2NibbleEngine`, the internal empty `DiskImage`, the active disk pointer, visit counts, `DiskAccessLog`, the write-blocked flag | One drive. `Pull (phases)` moves the head and returns the move (from, to, bumped). `SetMotorOn`, `StepCell`, `Mount`, `Eject`, `SetExternalDisk`, `IsWriteProtected`. `static GetPull` is here; its test moves here. |
| `Disk2Signals` | audio sink, the event sink slots, the motor-off flush callback, the idle callback and its cycle count | Everything the controller reports outward: motor engaged and disengaged, head step and bump, drive select, motor commands, idle pump. Both front ends call it. |
| `Disk2Recorder` | `Disk2NibbleRecord`, `Disk2AddressMarkWatcher`, the latch-load count, the instruction address source | `NoteRead (drive, nibble, flags)` and `NoteWrite (drive, nibble, isDropped)`. Replaces the duplicated blocks in `HandleReadDispatch`, `Write` and the IWM path. |
| `Disk2Clock` | the CPU cycle source, the last synced cycle | `TakeElapsed ()` returns the cycles since the last call. Both front ends use it. |
| `Disk2LatchPeek` | the base copy, the advanced copy, drive, sync, delta | The side-effect-free read of the data latch. |
| `Disk2CardSequencer` | Q6, Q7, the phase latches, the last phase, the motor flag, spin-up and spin-down counters, the active drive, the switch access count | The Disk II card's logic: `HandleSwitch`, `HandlePhase`, the read dispatch, `Tick`'s motor timing, catch-up. Operates the drives, signals, recorder and clock by reference. |
| `Disk2IwmPort` | the `Iwm`, the two enables, the IWM cycle count | The //c front end. Its two `IDiskDriveBus` adapters call `Disk2Drive` directly. It calls `Disk2Signals` and `Disk2Recorder`. `IDisk2IwmOwner` is deleted. |
| `Disk2Controller` | slot, I/O range, mode flag, port hook, and the parts above | `MemoryDevice`, `IMachineState`, `IDiagnosticsProvider`. `Read`, `Write` and `TryPeekIo` pick the front end by mode, then call the port hook. |

**Shared sequencer state.** Today the IWM path writes `m_activeDrive`, `m_q6`,
`m_q7` and `m_motorOn` so that diagnostics, state and `GetQuarterTrack` work in
both modes. Put those four in one small struct (`Disk2Lines`) that both front ends
write and `Disk2Controller` reads, or have `Disk2Controller` read them from the
active front end. Choose in step 2 of the method; there must be one copy.

**State.** `Disk2Controller::SaveState` and `LoadState` call a `SaveState` on each
part in today's field order. Check the order against the current function line by
line; the golden test confirms it.

**Callers.** `GetEngine (drive)`, `GetAccessLog`, `GetVisitCount`,
`IsWriteBlocked`, `GetDisk`, `MountDisk`, `EjectDisk`, `SetExternalDisk` become
`GetDrive (drive).X`. `SetAudioSink`, `SetEventSink`, `SetMotorOffFlushCallback`,
`SetIdleCallback` become `GetSignals().X`. `GetNibbleRecord` becomes
`GetRecorder().GetNibbleRecord ()`. The virtual `OnLatchLoad` hook (used by
`Disk2LatchLoadHookTests`) becomes a sink on `Disk2Recorder`.

### 2. MachineHost

**Problem.** The header is far too long. The class is the composition root and also
the run loop, host input, storage, muting, observer wiring, tape and a diagnostics
panel. The first cut left one-line forwarders for the connectors and the state
stream.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `MachineHost` | the bus, registry, interrupt controller, CPU, Prng, MMU, ROM bank, mouse, accelerator, Joyport, video timing, video scanner, owned devices, video modes, character ROM, `MachineRefs`, soft-switch mirror, lifetime lock, config, machine name, asset folder, and the components below | What the machine is made of, and `SoftReset` and `PowerCycle`, which touch every part. Getters for the components. Nothing else. |
| `MachineRunner` | position, debug hook, stop flag, history recorder pointer, opcode watch | `StepOne`, `RunCycles`, the hook path, `FinishStep`, the card catch-up. Constructed with the parts it ticks (CPU slot, `MachineRefs`, `MachineConnectors`, accelerator slot). |
| `MachineHostInput` | `InputJournal`, `HostInputGate`, the `HeldInputWatch` pointer, the host-input part list | `Record`, `SetJournalOn`, `AttachJournal`, `ApplyDeviceInput`, `Sample`, `SaveState`, `LoadState`, `DropConnections`, `ResyncConnections`. |
| `MachineStorage` | the block media file I/O and probe, the problem list, the stock folder | `FindFloppy35`, `InsertFloppy35`, `EjectFloppy35`, `AttachHardDisk`, `ConnectDiskToController`, `BindDiskDrives`, and `MachineReplayMedia`'s three functions as methods. `MachineReplayMedia` is deleted. |
| `MachineOutputMute` | the two mute flags, the muted drive audio sink, the motor flag at mute | `SetMuted` and the queries. |
| `MachineObserverWiring` | the comm sink | `AttachObservers`, `AttachCommEventSink`. |
| `MachineTape` | `TapeDeck`, `CassetteDiagnostics`, the reset count | `Stop`, the count. |
| `MachineClockPanel` | the speed mode | The clock panel (`IDiagnosticsProvider`) and `GetProviders`. `MachineHost` no longer implements `IDiagnosticsProvider`. |
| `MachineConnectors`, `MachineStateStream`, `MachineRomIdentity` | as now | Remove every forwarder on `MachineHost` (`SetSerialPort` through `DescribeAttachmentFailure`, `SaveState` through `CheckSharedSave`, `MediaIds`, `kStateTag`, `kStateVersion`, `kOldestStateVersion`). |

**Slots.** `m_cpu`, `m_mmu` and the other `unique_ptr` members are replaced on a
machine switch, so a component cannot store a raw pointer taken at construction.
Pass each component a reference to the `unique_ptr` (or a small `MachineParts`
struct of them that `MachineHost` has as a member), not `MachineHost &`.

**Member order.** Components that take references are declared after what they
refer to. Write the order as a comment once, at the member list.

**Callers.** Counts at `4b20fcd8c`, outside `MachineHost`: `GetTapeDeck` 66,
serial and printer methods 64, `GetInputJournal` 41, `AttachObservers` 34,
`SampleHostInputs` 31, `GetHostInputGate` 28, `RecordInput` 19, `AttachHardDisk`
16, `InsertFloppy35` 16, `GetDiagnosticsProviders` 13, plus every `SaveState`,
`LoadState`, `StepOne`, `RunCycles` and `GetPosition` call. `TestMachine` derives
from `MachineHost`; update it and the test rigs first, since most test call sites
go through them.

**Not in this plan.** `MachineRefs` (a struct of raw pointers the builder fills)
is a separate design problem and is out of scope. Add what you find to tasks.md.

### 3. KeyframeStore

**Problem.** One class has the storage ring, the layout math, the background
packing pipeline and the public API. The first cut moved the codec, the easiest
part.

**Classes.**

| Class | State | Thread | Responsibility |
|---|---|---|---|
| `KeyframeLayout` | none (static functions of `KeyframeSettings` and sizes) | any | `ComputeLayout`, `ComputeFitLayout`, `GetGroupSlots`, `GetLeastArena`. Direct tests. |
| `KeyframeArena` | the entry ring, first, count, the arena and its size and end, stored and side byte totals, the group counters, the full flag | caller | `Place`, `TryFindRoom`, `Relayout`, `PopOldest`, drop oldest group, drop newer than, find at or before, find by position, find group start, side bytes of a collected entry. Direct tests with small arenas. |
| `KeyframePackQueue` | the job array, next job, pending count, the work queue | caller submits, worker packs | `Take`, `Submit`, `Collect` (in order), `WaitForPending`, `DropPending`. Has the `KeyframeCodec`. |
| `KeyframeCodec` | as now | worker during a job, caller otherwise | as now |
| `KeyframeStore` | settings, the drop listeners, the next due cycle, the wall clock | caller | `Configure`, `ChangeBudget`, `Add`, `Capture`, `Restore`, `RestoreAtPosition`, `CopyPacked`, `DoesStateMatch`, truncation, the listeners. Composes the three parts. |

**Invariant to write into the headers.** The codec's buffers are touched by the
worker only while a job is in flight, and by the caller only while none is. The
arena is touched by the caller's thread only. `Collect` is the one place results
cross.

**Callers.** `GetInfo`, `GetCount`, `TryFindByPosition` and the other queries move
to `GetArena ()`. `ComputeChecksum` and `XorBytes` callers use `KeyframeCodec::`.

### 4. ReverseController

**Problem.** One class has the recording state machine, keyframe capture, seeking,
the step table, the step-back commands and divergence handling.
`ReverseStepSearch` reads its private members as a friend.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `HistorySpan` | recording, live, paused, pause start, live end position and cycle, edit pending, the stretch the machine last ran in | The recording state machine and its queries: `IsInHistory`, `GetLiveEndPosition`, `GetLiveEndCycle`, `GetRecordedEnd`, `GetOldestPosition`. Pure state transitions, with direct tests. |
| `KeyframeCapturer` | the two `StateWriter`s, the side buffer, the observer, the keyframe listeners, the work queue and its test replacement | `CaptureNow`, `CaptureBoundary`, `OnCaptureDue`, `ScheduleCaptures`, `NoteActivity`, `PruneRetainedMedia`. |
| `ReplayStepTable` | the steps, their start and end, the build count | `IsBuiltFor (stretch)`, `Get (position)`, `GetRecent (position, count)`, `Discard`, and filling from a replay. |
| `ReverseSeeker` | the cut flag and result, the stopped flag, the last divergence text, the cut count | `Seek`, `RestoreAtOrBefore`, `ReplayHere`, `LeaveLive`, `BecomeLive`, `LandAt`, `HandleDivergence`. Implements `IReverseSeeker` (see below). |
| `ReverseStepper` | `ReverseStepSearch`'s state (caller links, stack pointers, the probes) | `StepBack`, `StepBackOver`, `StepBackOut`, `StepBackCycles`, `PrepareRecentSteps`. Absorbs `ReverseStepSearch`. Uses `IReverseSeeker`, `ReplayStepTable` and `HistoryTimeline`. |
| `HistoryTimeline` | as now | as now, with `HistorySpan` supplying the newest end |
| `ReverseController` | the parts above, `KeyframeStore`, `Replayer` | `Start`, `Stop`, `SetUserMaximumSpeed`, `ChangeBudget`, the `HistoryRecorder` callbacks, `ReverseContinue`, `SeekToCycle`, `SeekToPosition`, and getters for the parts. |

**`IReverseSeeker`.** The calls the stepper makes on the seeker, to be confirmed against
`ReverseStepSearch.cpp`: replay a stretch into a step or stack-pointer table, land
at a position with an outcome, and report whether the last replay cut history. If
it grows past four methods, apply rule 3.

**Callers.** `HasStepTable`, `GetTableBuildCount`, `GetRecentSteps` move to the
step table. `GetLastDivergence`, `GetCutCount` move to the seeker. The step
commands move to `GetStepper ()`. `GetWallTimeAt` and `GetWallTimeAtPosition`
forwarders are removed; callers use `GetTimeline ()`.

### 5. Cpu

**Problem.** The execution core also has the instruction trace ring and its file
dump, the fetch observers, and a step printer that nothing calls.

**Classes.**

| Class | State | Responsibility |
|---|---|---|
| `CpuTrace` | the entry ring, capacity, head, count, enabled, the pending interrupt tag, the cycle sources, the peek pages | `Enable`, `Stop`, `Restart`, `Push` (inline, called from `Cpu::ObserveFetch` with the registers), `RecordAccess`, `GetSize`, `TryGetEntry`, `DumpToFile`, the illegal-opcode dump. `TraceEntry` and the interrupt tags move here. |
| `CpuFetchObservers` | the opcode watch tables, the watcher, the fetch sink, the observed flag | `SetOpcodeWatch`, `SetFetchSink`, `Observe` (inline). Has the `CpuTrace` enabled test in its flag. |
| `CpuStepPrinter` | none | The three print functions as statics taking the register values, the `Microcode` and a byte reader. `OperandInfo` becomes a public struct in a separate header. It has no caller today and must stay compiled. |
| `InstructionSetBuilder` | as now | as now |
| `Cpu` | registers, status, memory, read pages, the instruction set, last-instruction facts | `StepOne`, operand fetch, execute, stack, memory access, `LoadBinary`. |

**Hot path.** `ObserveFetch` is `__forceinline` and tests one flag per
instruction. That cost must not change: the flag is a member the fetch path reads
with no call, and `CpuTrace::Push` is called only when it is set. Run the
per-instruction measurement before and after this class alone.

**Callers.** `EnableTrace`, `GetTraceSize`, `TryGetTraceEntry`, `DumpTraceToFile`,
`RecordTraceAccess` and `Cpu::TraceEntry` users (the debugger's trace pane,
`TraceController`, `MemoryBusCpu`) use `GetTrace ()` and `CpuTrace::Entry`.

### 6. AppleWinParser

**Problem.** One static class parses every command family. The first cut moved the
breakpoint family and added forwarders.

**Classes.** Confirm the grouping against `AppleWinCommandFamily` in
`AppleWinCommandTable.h`; one parser class per family or per closely related set.

| Class | Contents |
|---|---|
| `AppleWinArguments` (struct, in a separate header) | the command entry, tokens, rest, context |
| `AppleWinArgumentForms` | the forms several families share: `TryEvaluate`, `TryParseRange`, `TryParseSourceLine`, `TryParseValues` and `ValueWidth`, `TryParseCount`, `TryParseDecimal`, `IsNumberOrRange`, `TryParseIdOrAll`, `TryParseSearchItems`, `TryParseSearchWord` |
| `AppleWinText` | as now |
| `AppleWinBreakpointParser` | as now, plus nothing of other families |
| `AppleWinMemoryParser` | memory, data, `IsBlockName` |
| `AppleWinRunParser` | run, register, flag, skip, engine, history, calls |
| `AppleWinViewParser` | list, symbol, output, panel, the BASIC reference form |
| `AppleWinShorthand` | the shorthand and move shorthand forms |
| `AppleWinParser` | `Parse` and the dispatch by family. No argument parsing. |

**Callers.** Remove `AppleWinParser::TryParseIfClause`, `TryParseCondition` and
`TryMakeBreakpoint`. `WinDbgParser.cpp`, `GSSquaredParser.cpp`,
`BreakpointDefinitionCheck.cpp`, `DebuggerViewState.cpp` (or wherever group 1
moved that code) and `DirectPaneActionTests.cpp` call
`AppleWinBreakpointParser` or `AppleWinArgumentForms` directly.

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

- `refs/heads/claude/035-split-debugger-ui-2c6403`. Its report rates the
  `DebuggerViewState` split (into `CodeViews` and other state classes) as sound,
  and the window files (`DebuggerWindow`, `DebuggerWindowInput`,
  `DebuggerActions`, `SourcePane`) as chosen for line count. The owner's instruction to that
  session: redo those four as redesigns and drop forwarders.
- `refs/heads/claude/035-split-debug-engine-080423`. Its report rates
  `CallRecordKeeper` and others as sound and some boundaries as weaker.
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
