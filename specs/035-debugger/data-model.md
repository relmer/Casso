# Data Model: Debugger

Types are listed by role. "API type" means callers pass or receive it, so it is
a free type in its own header; plain-data structs used only inside one class are
nested there, per the code style rules.

## DebugSession

The debugger's attachment to one machine.

| Field | Type | Notes |
|---|---|---|
| mode | `CommandMode` | `AppleWin` (default) or `Monitor` |
| state | `RunState` | `FreeRunning`, `Paused`, `DebugRun`, `Stepping` |
| breakpoints | `BreakpointTable` | |
| watchpoints | `WatchpointTable` | |
| watches | `WatchTable` | AppleWin display watches, ZP pointers, bookmarks |
| symbols | `SymbolTable` | |
| monitorState | `MonitorState` | parser-local, but owned here so a mode switch keeps it |
| cycleCounter | `uint64_t` | AppleWin `RCC` resets it |
| lastStop | `StopEvent` | |
| currentDirectory | `std::wstring` | `PWD` / `CD`; base for relative file paths |
| target | `IDebugTarget &` | |

**State transitions**

```text
FreeRunning --(pause | breakpoint | watchpoint | BRK/BRKOP/BRKINT)--> Paused
FreeRunning --(G, GG, run-to)--> DebugRun          adopts the running machine; ok
Paused      --(G, GG, run-to)--> DebugRun
DebugRun    --(pause | breakpoint | watchpoint | budget | run-to reached)--> Paused
Paused      --(T, P, S, RTS)--> Stepping --(done | stop)--> Paused
Paused      --(user resumes in Casso)--> FreeRunning
any         --(reset)--> same state, notification "reset", tables kept, video counters keep running
any         --(machine switch)--> Paused, breakpoints/watchpoints cleared, notification "machineChanged"
```

A batch session starts `Paused`; an emulator session starts in whichever of
`FreeRunning` or `Paused` the machine is in when the debugger opens.

**Rules**

- Only one debugger run is active at a time. A run command while `DebugRun`
  or `Stepping` returns `Error` "already running". While `FreeRunning` it
  returns `ok` and adopts the machine into a `DebugRun`.
- The per-instruction hook is installed while any enabled stop condition
  exists or the state is `DebugRun` or `Stepping`, and removed otherwise
  (R-004).
- Commands that change the machine (memory, registers) are accepted while
  paused. While running they are accepted only from the window and the pipe,
  where they are posted to the CPU thread and applied between instructions.

## CommandMode

Gains `WinDbg` as a fourth value (FR-011).

An enum: `AppleWin`, `Monitor`. It is switched by the AppleWin command `MODE
MONITOR`, or by `/mode applewin` in Monitor mode. The switch is a Casso engine
command, listed in [contracts/command-modes.md](contracts/command-modes.md).

## DebugCommand (API type)

| Field | Type | Notes |
|---|---|---|
| verb | `DebugVerb` | engine operation (e.g. `SetBreakpoint`, `ExamineRange`, `Step`) |
| sourceName | `std::string` | the command name as typed, for replies and errors |
| addresses | `Word` A1, A2, A3 + presence flags | |
| values | `std::vector<Byte>` | deposit, search pattern, fill value |
| expression | `Expression` | breakpoint conditions, `R` assignments |
| text | `std::string` | file names, ECHO text, symbol names |
| count | `uint32_t` | step count, trace count |
| budget | `std::optional<uint64_t>` | a per-run budget; absent uses the session's, which is unbounded unless `BUDGET` set one |

**Validation**: the parser guarantees shape (required operands present, hex in
range). The session validates state (paused, address writable).

## Breakpoint

| Field | Type | Notes |
|---|---|---|
| id | `int` | AppleWin list index, stable until cleared |
| kind | `BreakpointKind` | `Address`, `Opcode`, `Register`, `Memory`, `Io`, `Brk`, `Interrupt` |
| address / range | `Word`, `Word` | |
| opcode | `Byte` | kind `Opcode` |
| condition | `Expression` | register or memory predicate (`BPR A = 41`) |
| enabled | `bool` | `BPD` / `BPE` |
| hitCount | `uint32_t` | |

The table keeps a 64 KB bitmap of enabled `Address` breakpoints, rebuilt on
each change.

## Watchpoint

| Field | Type | Notes |
|---|---|---|
| id | `int` | shared numbering with breakpoints, as in AppleWin `BPM*` |
| access | `Read`, `Write`, `ReadWrite` | `BPMR`, `BPMW`, `BPM` |
| first, last | `Word` | inclusive |
| mode | `After`, `Before` | `After` is the default; `BEFORE` selects the other |
| enabled | `bool` | |

An `After` watchpoint is reported by the bus: a hit records
`{accessPc, address, value, previous, access}` and requests a stop at the next
instruction boundary. `previous` is the byte the write replaced and is absent
on a read, and on any address served by a device, where reading it back would
disturb the machine (R-017). Within one instruction a write to an address
replaces a pending read of it, and a later write replaces an earlier one.

A `Before` watchpoint puts no page in the bus mask. The session decodes the
instruction at the program counter, computes the addresses its operand would
touch from the current registers and memory, and stops before executing when
one falls in the range, with `{accessPc, address, access}` and no value.

## Watch, ZeroPagePointer, Bookmark

Plain entries `{id, address, enabled}` (watch, ZP) or `{id, address}`
(bookmark), matching AppleWin's `W*`, `ZP*` and `BM*` families.

## Symbol

| Field | Type | Notes |
|---|---|---|
| name | `std::string` | case-insensitive lookup |
| address | `Word` | |
| table | `SymbolTableId` | `Main`, `Basic`, `Asm`, `User`, `User2`, `Src`, `Src2`, `Dos33`, `ProDos` |

Each table is independently enabled. Lookup order follows AppleWin's
documented order.

## MonitorState

| Field | Type | Notes |
|---|---|---|
| a1, a2, a3, a4 | `Word` | the Monitor's pointer registers, as the ROM uses them |
| lastExamined | `Word` | Return and space continuation |
| storeAddress | `Word` | target of `:` |
| registerEditPending | `bool` | set by `^E` until the next non-`:` input |
| assemblerActive | `bool` | inside `!` / `F666G` |

## Reply (API type)

| Field | Type | Notes |
|---|---|---|
| status | `CommandStatus` | `Ok`, `Error`, `NotAvailable`, `Unknown` |
| command | `std::string` | echo of the input line |
| data | `ReplyData` | typed payload: `Registers`, `MemoryRows`, `Disassembly`, `BreakpointList`, `SearchHits`, `Stop`, `Message` and so on |
| text | `std::vector<std::string>` | rendered by the mode's formatter from `data` |
| error | `std::string` | two-line error shape for `Error` and `NotAvailable` |

Text and JSON are both rendered from `data`, never from each other.

## StopEvent

| Field | Type | Notes |
|---|---|---|
| reason | `StopReason` | `Breakpoint`, `Watchpoint`, `Step`, `RunTo`, `Budget`, `Pause`, `Brk`, `InvalidOpcode`, `Reset` |
| pc | `Word` | |
| breakpointId | `std::optional<int>` | |
| access | `std::optional<WatchHit>` | |
| cycles | `uint64_t` | cycles executed by the run |
| registers | `Registers` | |

It becomes a `stopped` notification over the channel (see protocol).

## RunRequest

`{kind: Go | StepInto | StepOver | StepOut | RunTo | Trace, fullSpeed,
untilPc, count, budget}`. `budget` is optional; absent means unbounded.
`StepOver` completes when PC reaches the instruction after the `JSR` with the
stack pointer back at its value before the call, so a recursive subroutine is
stepped over as one call. It is produced by the session and consumed by the
target.

## IDebugTarget (interface)

| Operation | Notes |
|---|---|
| `GetRegisters` / `SetRegisters` | via `I6502DebugInfo` |
| `Peek` / `Poke` / `GetRegion` | via `DebugMemoryView` |
| `ReadIo` / `WriteIo` | real bus access, for `IN` / `OUT` only |
| `GetSoftSwitches` | name/value list from MMU, language card, video switches |
| `StartRun (const RunRequest &)` | begins a run through the injected `IRunDriver`; the `StopEvent` is delivered to the `IRunObserver` (the session) through `OnStopped`. `SynchronousRunDriver` completes the run inside the call; `CpuManagerRunDriver` un-pauses `CpuManager` and delivers the stop from the CPU thread when the hook fires (R-005) |
| `RequestPause` | stops a running machine with reason `pause` |
| `SetHookInstalled (bool)` | the session installs the hook while any enabled stop condition exists or a run is active |
| `SetWatchedPages (mask)` | publishes the watch mask to `MemoryBus` (R-004) |
| `GetVideoPosition` | scanline and cycle within the line from `VideoTiming`, for `BPV` and `VIDEOINFO` |
| `GetCpuKind` | `M6502`, `M65C02`, which selects the disassembler table |
| `GetMachineInfo` | machine name, disks, title label |
| `InjectKey` | `KEY` |

## IRunObserver (interface)

`OnStopped (const StopEvent &)`, implemented by `DebugSession`. The target
calls it once per run or free-running stop, on the thread that ran the
machine; the session turns it into the `stopped` notification and the new
state.

## IRunDriver (interface)

`Start (const RunRequest &)`, `Pause ()`. Two implementations:
`SynchronousRunDriver` over `MachineHost::RunCycles` for batch, and
`CpuManagerRunDriver` over `CpuManager` for the emulator (R-005).

## DebugChannelServer (phase 2)

| Field | Type | Notes |
|---|---|---|
| transport | `IPipeTransport &` | |
| clients | list of `{id, connection, pendingRequests}` | |
| sink | `IDebugReplySink` | fans replies to the requesting client and notifications to all |

**Lifetime**: exists only while `DebuggerController` is open. On close, every
client is disconnected and the listening instance is destroyed.

## InstanceInfo

`{pid, title, machine, disks[], protocolVersion}`, returned by `hello` and
listed by `debug --list`.


## OutputFormat

Gains `WinDbg` as a fourth value (FR-013).

An enum: `AppleWin`, `Monitor`, `GSSquared`. Session state beside `mode`.
`MODE x` sets both `mode` and `outputFormat` to `x`; `OUTPUT x` sets
`outputFormat` alone. Every `Reply.text` is rendered by the formatter for the
current `outputFormat` (FR-013). `CommandMode` gains `GSSquared`.

## Breakpoint (additions)

| Field | Type | Notes |
|---|---|---|
| condition | `Expression` | now also an `IF` expression on an address or watch breakpoint; evaluated only when the location or access hits (FR-061) |
| value | `std::optional<Byte>` | kind `MemoryValue`: stop when a write leaves `address` holding `value` (FR-062) |

`BreakpointKind` gains `MemoryValue`. A condition that reads an I/O address is
rejected when set.

## TraceEntry (extended, in `Cpu`)

| Field | Type | Notes |
|---|---|---|
| cycles | `uint64_t` | the CPU's cycle counter before the instruction |
| pc, opcode, op1, op2 | as today | |
| a, x, y, sp, p | as today | registers before execution |
| intr | as today | interrupt-entry tag |
| accessAddress | `Word` | the instruction's last bus access, when `hasAccess` |
| accessData | `Byte` | |
| accessIsWrite | `bool` | |
| hasAccess | `bool` | |

The ring holds up to 100,000 entries while on (R-025). `HISTORY` renders a
window of entries with the symbol for `pc` and `accessAddress`.

## Profile

| Field | Type | Notes |
|---|---|---|
| byOpcode | `[256] {count, cycles}` | keyed by opcode; the formatter groups by mnemonic and addressing mode |
| penalties | `{pageCross, branchTaken, branchCross} cycles` | R-026 |
| byAddress | map `Word -> cycles` | the per-address view |
| totalCycles | `uint64_t` | |

Counted by the hook while `PROFILE ON`; `PROFILE RESET` clears it.

## DebugFile

The in-memory form of a cc65 debug file or of a listing loaded as one.

| Field | Type | Notes |
|---|---|---|
| files | `vector<SourceFileRecord>` | |
| segments | `vector<Segment>` | `{id, name, start, size}` |
| spans | `vector<Span>` | `{id, segment, start, size}`; `start` is segment-relative |
| lines | `vector<LineRecord>` | `{id, file, line, type, depth, spans[]}`; `type` is `Asm`, `Macro` or `MacroParameter` |
| symbols | `vector<SymbolRecord>` | `{name, value, segment, scope}` |
| modules, scopes | | as cc65 records them |
| selfHash | `Sha1` | hash of the debug file's own bytes; keys the per-program path list |

## SourceFileRecord

| Field | Type | Notes |
|---|---|---|
| id | `int` | |
| relativePath | `std::string` | as recorded, relative to the debug file |
| size | `uint64_t` | |
| sha1 | `Sha1` | of the LF-normalized text; absent for a cc65 file from another assembler |
| resolvedPath | `std::optional<std::wstring>` | where the file was found, once found |
| matches | `Match` | `Exact`, `Mismatch` (opened with a warning), `Unresolved` |

## LineTable

Built from a `DebugFile`: address -> the list of `(file, line, type, depth)`
positions that produced it, ordered outermost first; and `(file, line)` ->
address ranges. For a listing loaded as a source, the file is the listing
and each listing line with an address is one entry.

## SourcePathList

`{ perProgram: map<Sha1, vector<folder>>, global: vector<folder> }`, most
recent first, in the global preferences (R-032).

## MemoryWindow

| Field | Type | Notes |
|---|---|---|
| id | `1..4` | |
| address | `Word` | first shown |
| grouping | `Bytes`, `Words`, `DoubleWords` | |
| caret | `{row, column, inText}` | |
| pending | `std::string` | hex digits typed so far for the current cell |
| history | `UndoHistory` | |

## UndoHistory

An ordered list of `{address, written, replaced, region}`; undo pops the last
and writes `replaced` back through the same path (bus write for RAM, patch for
ROM). Cleared on machine switch.

## DiagnosticsSnapshot

| Field | Type | Notes |
|---|---|---|
| device | `std::string` | the panel's title |
| groups | `vector<{title, rows}>` | |
| row | `{label, value, bits}` | `bits` is an optional list of `{name, set}` |
| visual | `variant<none, MemoryMap, DiskHead, Meters>` | `MemoryMap`: 256 pages, each `{readSource, writeSource}`; `DiskHead`: quarter track, phases, motor; `Meters`: named levels 0..1 |

Published by an `IDiagnosticsProvider` per device; built on the CPU thread
once per frame while the machine runs and once on stop (FR-051).

## Layout

A tree: `SplitNode {orientation, ratio, first, second}`, `TabNode {panes[],
active}`, `PaneLeaf {paneId}`; plus `floating: vector<{paneId, monitorKey,
rectDip}>` and `autoHidden: vector<{paneId, edge}>`. Serialized as JSON with a
version, in the global preferences (R-028). `paneId` is a stable string per
pane kind plus an index for memory windows (`memory1`..`memory4`).

## KeyScheme

A `DxuiKeyMap`: `{name, list<{vk, ctrl, alt, shift} -> commandId>}` (the chord
struct is `CassoExplorerCommands::kKeys`'s) for run, pause, step into, step over,
step out, toggle breakpoint and run to cursor. Three maps; the chosen name is
a preference, and the window swaps its active map when it changes.

## CallStackFrame

| Field | Type | Notes |
|---|---|---|
| callSite | `Word` | the `JSR`'s address, or the interrupted instruction for an interrupt frame |
| target | `Word` | the routine entered, or the vector taken |
| kind | `Call`, `Brk`, `Irq`, `Nmi`, `Reset` | |
| provenance | `Recorded`, `Guessed` | which mechanism produced it (FR-068) |
| stackLevel | `Byte` | SP before the push, which is what ends the frame (R-033) |
| note | `std::optional<std::string>` | "returned past inline parameters" and the like |

## CallStackBreak

| Field | Type | Notes |
|---|---|---|
| kind | `Txs`, `PulledReturn`, `EndedByJump`, `ReturnMismatch`, `StackWrap`, `Reset`, `TrackingBegan`, `PowerOn` | FR-069 |
| pc | `Word` | the instruction that caused it |
| opcode | `Byte` | |

A `CallStack` is the ordered list of frames innermost first, with breaks
between them; `mechanism` is `Recorded`, `Walk` or `Hybrid`. Built by the
session on request (`CALLS`, `k`) and once per snapshot for the pane.

## StepFilter

`{ entries: vector<{name, first, last}> }`, matched against a `JSR`'s target
at a step into (FR-070). Session state; `SKIP` sets, lists and clears it.

## Disk breakpoints (User Story 20)

Research R-041 holds the design; this section holds the types. A disk
breakpoint is a `Breakpoint` of a new kind, so its id, condition, hit count,
flags, undo and export are the table's.

### Breakpoint and BreakpointInfo (additions)

| Field | Type | Notes |
|---|---|---|
| disk | `std::optional<DiskBreakSpec>` | kind `Disk` only |
| resolved | `std::optional<Word>` | `BreakpointInfo` only: the routine an RWTS or driver breakpoint stops at now; absent while its vector holds no target |

`BreakpointKind` gains `Disk`. `StopReason` gains `Disk`.

### DiskBreakSpec (API type, `CassoCore/Debugger/DiskBreakSpec.h`)

What `BPDISK` was given, parsed in CassoCore so every mode's parser builds the
same value (FR-135).

| Field | Type | Notes |
|---|---|---|
| event | `DiskEvent` | `SectorRead`, `AddressField`, `Head`, `WriteMode`, `WritePrologue`, `WriteBlocked`, `DosSector`, `ProDosBlock`, `RwtsCall`, `DriverCall`, `Motor`, `DriveSelect`, `Inserted`, `Ejected` |
| slot | `int` | the controller's slot, 6 on every shipped machine; another is an error when set (FR-143) |
| drive | `DiskDriveFilter` | `Either`, `Drive1`, `Drive2` |
| track, sector, volume | `std::optional<Byte>` | absent matches any (`*`); a physical sector for `SectorRead`, `AddressField` and `WritePrologue`, a logical one for `DosSector`, the IOB's for `RwtsCall` |
| block | `std::optional<Word>` | `ProDosBlock` (required) and `DriverCall` |
| head | `DiskHeadFilter` | `Any`, `Range`, `Half`, `Quarter`; with `qtFirst`, `qtLast` for a range, a `T` range stored as its whole-track quarter tracks and `isWholeTracks` set |
| qtFirst, qtLast | `int` | quarter tracks, 0 to 139 |
| isWholeTracks | `bool` | a `T` range: only positions that are a multiple of four match |
| motor | `DiskMotorFilter` | `Any`, `On`, `Off`, `Stopped` |
| command | `std::optional<Byte>` | `RwtsCall`: 0 seek, 1 read, 2 write, 4 format; `DriverCall`: 0 status, 1 read, 2 write, 3 format |
| isPassed | `bool` | `PASSED`: every data field read in full counts (FR-145) |
| isBadChecksum, isWrongTrack | `bool` | `BADSUM`, `WRONGTRACK` (FR-146) |
| isBump | `bool` | `BUMP` (FR-148) |
| isEvery | `bool` | `EVERY` on `SELECT` (FR-152) |
| addressMarks, dataMarks | `std::optional<DiskMarkPattern>` | `ADDR`, `DATA` (FR-147) |
| format | `DiskFieldFormat::Kind` | `Sixteen` or `Thirteen`; with standard marks, the prologue matched; with custom ones, `FORMAT`, `Sixteen` by default |

**Validation**: the parser checks the forms -- quarter tracks 0 to 139, a
`DOS` sector 0 to 15, a block 0 to 279, a mark nibble a hex byte or `??` --
and the session checks the slot and that the machine has a Disk II
controller.

### DiskMarkPattern and DiskFieldFormat (shared with 040)

`DiskMarkPattern` is three nibbles, each a value or any (`??`), and matches a
run of nibbles. `DiskFieldFormat` holds what a field is: for `Sixteen`, address
prologue D5 AA 96, 4-and-4 volume, track, sector and checksum, data prologue D5
AA AD, 342 6-and-2 nibbles and a checksum nibble; for `Thirteen`, address
prologue D5 AA B5 and 410 5-and-3 nibbles; the epilogue DE AA EB; the
translate tables and the checksum rules. Both live in
`CassoEmuCore/Devices/Disk/`, built by whichever of 035 and 040 merges first
(FR-162), and serve 040's analyzer and these breakpoints alike.

### Disk2NibbleRecord (in `Disk2Controller`, saved with the machine)

| Field | Type | Notes |
|---|---|---|
| entries | `std::array<Entry, 512>` | the most recent nibbles the CPU received from or wrote to the drives, oldest overwritten |
| count | `uint64_t` | nibbles recorded since the last power cycle; a reader keeps the count it last read up to |
| Entry.nibble | `Byte` | |
| Entry.flags | `Byte` | drive (bit 0), written (bit 1), passed during spin-up (bit 2), dropped by write protection (bit 3), first after a disk change (bit 4) |
| Entry.instruction | `Word` | the address of the instruction that read or wrote it |

Part of `Disk2Controller`'s state from version 2; a version 1 state loads with
`count` 0. A power cycle clears it. A drive with no disk adds nothing.

### Disk2Controller listeners and head (additions)

| Field | Type | Notes |
|---|---|---|
| windowSink | `IDisk2EventSink *` | `SetEventSink`, the Disk ][ debug window's, as today |
| debuggerSink | `IDisk2EventSink *` | `SetDebuggerEventSink` |
| dispatch | `IDisk2EventSink *` | what every place that reports an event tests: null, the one set, or `tee` |
| tee | `Disk2EventTee` | forwards each event to both; a class of its own, `Disk2EventTee.h/.cpp` |
| quarterTrack | `int [2]` | each drive's head (FR-160); state version 2 |
| instructionSource | `const Word *` | wiring, the address of the instruction `MachineHost` is executing |

`IDisk2EventSink` gains `OnWriteMode (int drive, bool isOn)`, with an empty
default body. The write hook (FR-161) reports `{drive, nibble, quarterTrack,
isBlocked}` from the one place a latch load happens.

### DiskState (from `IDebugTarget::TryGetDiskState`)

`{slot, selectedDrive, quarterTrack[2], isMotorOn, isWriteMode,
isProtected[2], hasDisk[2], imageName[2]}`, read from the controller's getters,
never through the bus; false on a machine with no Disk II controller. The
recent nibbles come from `IDebugTarget::ReadDiskNibbles (since, entries)`, and
the epilogue look-ahead from `IDebugTarget::PeekDiskNibbles (drive, count,
nibbles, isRandom)`.

### Disk values (`DiskValueSymbols`, used by `DebugSession::TryResolveSymbol` before the symbol tables)

| Name | Value |
|---|---|
| `DISK.DRIVE` | `selectedDrive`, 1 or 2 |
| `DISK.QTRACK` | `quarterTrack` of the selected drive |
| `DISK.QTRACK1`, `DISK.QTRACK2` | each drive's `quarterTrack` |
| `DISK.MOTOR` | 1 while `isMotorOn` |
| `DISK.WRITING` | 1 while `isWriteMode` |
| `DISK.PROTECTED` | the selected drive's `isProtected` |
| `DISK.INSERTED` | the selected drive's `hasDisk` |
| `DISK.ATRACK`, `DISK.ASECTOR`, `DISK.AVOLUME` | the last address field with a good checksum in the recent nibbles, or $FFFF |

Resolved ahead of program symbols, as `ACCESS` and `VALUE` are.

### DiskFieldTracker (`CassoEmuCore/Debugger/`)

A pure class fed recent-nibble entries. For each mark set an armed
breakpoint uses (the standard marks, and each set of custom ones), it follows
address fields to their checksum nibble and data fields to theirs, and gives
back `DiskFieldEvent`s:

| Field | Type | Notes |
|---|---|---|
| kind | `AddressField`, `DataChecksum`, `WritePrologue` | |
| format | `DiskFieldFormat::Kind` | |
| track, sector, volume | `Byte` | from the address field, for all three kinds |
| storedChecksum, expectedChecksum | `Byte` | the address field's |
| isDataChecksumGood | `bool` | `DataChecksum` |
| addressInstruction, dataInstruction, lastInstruction | `Word` | the instructions that read the address prologue's first nibble, the data prologue's first nibble, and the event's last nibble |
| isPassed | `bool` | `addressInstruction == dataInstruction` (the read test) |
| drive | `int` | |

`Prime` resets it and feeds it the whole record; `Feed` feeds it the entries
added since. No field crosses an entry flagged as the first after a disk
change, and a field any of whose nibbles passed during spin-up is not
reported.

### DiskBreakpointMonitor (`CassoEmuCore/Debugger/`)

The debugger's `IDisk2EventSink`, in the controller's debugger slot while a
disk breakpoint is armed. It owns a `DiskFieldTracker`, reads new record
entries at each instruction boundary, matches events against the table's
disk entries, and holds the pending stop and its `DiskReport` until the
session takes them, as `WatchpointTable` holds a watch hit. The session's
`HasPendingStop` includes it.

### DiskReport (StopEvent addition)

| Field | Type | Notes |
|---|---|---|
| breakpointId | `int` | the lowest id that stopped (FR-153) |
| event | `DiskEvent` | |
| slot, drive | `int` | |
| quarterTrack, previousQuarterTrack | `int` | previous for a head stop |
| isMotorOn, isProtected, isBump | `bool` | |
| phases | `Byte` | the magnets on, bit n for phase n |
| instruction | `Word` | the instruction responsible (FR-154) |
| field | `std::optional<DiskFieldEvent>` | sector, address, DOS sector, block and write-prologue stops |
| epilogue | `std::array<Byte, 3>`, `isEpilogueRandom` | sector reads, from the look-ahead |
| logicalSector, block, physicalSectors | `std::optional<...>` | DOS sector and block stops, with `order` (`Dos33` or `ProDos`) |
| request | `std::optional<DiskCallRequest>` | RWTS: slot, drive, volume, track, sector, command, buffer; driver: command, unit, buffer, block |
| vector | `Word` | the vector an RWTS or driver stop went through |
| previousDrive | `int` | drive-select stops |
| imageName | `std::string` | insert and eject stops |

`StopEvent` gains `std::optional<DiskReport> disk`.

### IReverseStopTest (addition)

`GetDiskSink ()`, null by default: where the controller's events go during a
replay, in place of the debugger's slot. `ReverseStopTest` returns its own,
primes its own tracker from the record of the snapshot the replay starts from,
and counts nothing. The replayer also reports each bay whose disk a boundary
snapshot changed.