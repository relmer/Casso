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
any         --(reset)--> same state, notification "reset", tables kept
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
struct is `CassqueCommands::kKeys`'s) for run, pause, step into, step over,
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
| kind | `Txs`, `PulledReturn`, `EndedByJump`, `ReturnMismatch`, `StackWrap`, `Reset`, `TrackingBegan` | FR-069 |
| pc | `Word` | the instruction that caused it |
| opcode | `Byte` | |

A `CallStack` is the ordered list of frames innermost first, with breaks
between them; `mechanism` is `Recorded`, `Walk` or `Hybrid`. Built by the
session on request (`CALLS`, `k`) and once per snapshot for the pane.

## StepFilter

`{ entries: vector<{name, first, last}> }`, matched against a `JSR`'s target
at a step into (FR-070). Session state; `SKIP` sets, lists and clears it.