# Data Model: Debugger

Types are listed by role. "API type" means callers pass or receive it, so it is
a free type in its own header; plain-data structs used only inside one class are
nested there, per the code style rules.

## DebugSession

The debugger's attachment to one machine.

| Field | Type | Notes |
|---|---|---|
| mode | `CommandMode` | `AppleWin` (default) or `Monitor` |
| state | `RunState` | `Running`, `Paused`, `Stepping` |
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
Running --(pause | breakpoint | watchpoint | budget | step done)--> Paused
Paused  --(G, GG, run-to)--> Running
Paused  --(T, P, S, RTS)--> Stepping --(done | stop)--> Paused
any     --(reset)--> same state, notification "reset", tables kept
any     --(machine switch)--> Paused, breakpoints/watchpoints cleared, notification "machineChanged"
```

**Rules**

- Only one run request is active at a time. A run command while running
  returns `Error` "already running".
- Commands that change the machine (memory, registers) are accepted while
  paused. While running they are accepted only from the window and the pipe,
  where they are posted to the CPU thread and applied between instructions.

## CommandMode

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
| budget | `std::optional<uint64_t>` | overrides the default run budget |

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
| enabled | `bool` | |

A hit records `{accessPc, address, value, access}` and requests a stop at the
next instruction boundary.

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

`{kind: Go | StepInto | StepOver | StepOut | RunTo | Trace, untilPc, count,
budget}`. It is produced by the session and consumed by the host loop.

## IDebugTarget (interface)

| Operation | Notes |
|---|---|
| `GetRegisters` / `SetRegisters` | via `I6502DebugInfo` |
| `Peek` / `Poke` / `GetRegion` | via `DebugMemoryView` |
| `ReadIo` / `WriteIo` | real bus access, for `IN` / `OUT` only |
| `GetSoftSwitches` | name/value list from MMU, language card, video switches |
| `Run (RunRequest) -> StopEvent` | installs the hook, runs `RunCycles` in chunks |
| `GetCpuKind` | `M6502`, `M65C02`, which selects the disassembler table |
| `GetMachineInfo` | machine name, disks, title label |
| `InjectKey` | `KEY` |

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
