# Implementation Plan: Debugger

**Branch**: `035-debugger` | **Date**: 2026-09-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/035-debugger/spec.md`

## Summary

One debug engine that breaks into a running `MachineHost`, driven through two
command modes (AppleWin, the default, and Apple II Monitor) and reached three
ways: a `CassoCli debug` batch mode, a per-process named pipe, and a Dxui
debugger window. Refs GH #51, GH #59.

The approach turns on three facts from research:

- **Nothing today can read memory without side effects.** `MemoryBus` reads
  toggle soft switches and the speaker, and `Cpu::PeekByte` sees only the raw
  64 KB array, not the banked view. A banking-aware, side-effect-free
  `DebugMemoryView` is the first piece of foundation, because every examine,
  disassembly, search and watch display depends on it (R-003).
- **There is no per-instruction hook.** `MachineHost::RunCycles` and `StepOne`
  run instructions with nothing in between. The engine adds one: a single
  null-pointer test per instruction when no session is attached, and a 64 KB
  address bitmap when one is (R-004).
- **CassoCli cannot run a real machine.** The only headless machine builder is
  `UnitTest/EmuTests/TestMachine`. Batch mode needs it promoted into
  `CassoEmuCore` behind a ROM-source seam (R-006).

Everything else (both parsers, the disassembler, the line assembler, reply
formatting, the pipe protocol and the window's state) is data-in/data-out logic
over an `IDebugTarget` interface, testable against a fake target and against
real fixture machines.

## Technical Context

**Language/Version**: C++ `/std:c++latest`, MSVC v145+

**Primary Dependencies**: Windows SDK only. Named pipes (`CreateNamedPipeW`,
overlapped I/O), a user-SID DACL (`GetTokenInformation`, `SetEntriesInAclW`),
in-tree `JsonWriter`/`JsonParser` (CassoEmuCore), in-tree `Microcode`/
`OpcodeTable`/`Assembler` (CassoCore), in-tree `Dxui` for the window. **No new
third-party dependency.**

**Storage**: Host files only: `R`/`W`, `BLOAD`/`BSAVE`, `BPSAVE`/`WSAVE`/
`BMSAVE`/`ZPSAVE`, `TF`, `TSAVE`, `RUN` scripts, symbol files. All go through
the existing `IFileSystem` seam.

**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. Real-machine
tests use `TestMachine` and the fixture ROMs, which must be fetched with
`scripts/FetchRoms.ps1 -Fixtures` (they are gitignored and absent from this
worktree today). The pipe server is tested over an in-memory transport; a
single integration test exercises a real pipe.

**Target Platform**: Windows 10/11, x64 and ARM64.

**Project Type**: Desktop emulator plus console tool over static core libraries.

**Performance Goals**: No measurable emulation cost with no session attached
(one pointer test per instruction). With address breakpoints only, one bitmap
test per instruction. Memory watchpoints install a bus observer only while at
least one exists. Batch runs execute unthrottled.

**Constraints**:

- Determinism (FR-009, SC-004): batch mode reads no wall clock, performs no
  audio or video pacing, and orders all output by emulated cycle.
- Every scripted or client-started run has a cycle budget (FR-008).
- The pipe accepts only the current user's SID and rejects remote clients.
- Clean-room: AppleWin's table and help pages are consulted for names and
  behavior only.

**Scale/Scope**: About 229 AppleWin names (phase 1: ~175; phase 3: ~50; 5 not
available), 23 Monitor command-table entries plus the Monitor's syntax forms,
~40 new core source pairs across three phases, and ~30 new test files.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

### VI. Thin Executable, Testable Core (NON-NEGOTIABLE) -- the governing gate

1.11.0 allows no code in an executable at all. Nothing in this feature touches
`Casso/` or `CassoCli/`:

- `CassoCli debug` is a new arm of `CliMain` in `CassoEmuCore/Cli/`.
- The pipe server, including the DACL and the overlapped-I/O loop, lives in
  `CassoEmuCore/Debugger/Channel/` behind an `IPipeTransport` seam. The Win32
  transport is a thin class there, and the protocol logic is tested over an
  in-memory transport.
- The window is a `DxuiWindow` subclass in `CassoEmuCore/Ui/Debugger/`, with
  its state in a separate projection that tests drive without an `HWND`, the
  same split `Disk2DebugPanel` and `Ui/Debug/*DialogState` use.
- `EmulatorShell` changes are wiring only: attach the session at machine build
  and switch (beside `AttachDebugSinksIfOpen`), and route commands through
  `PostCommand`.

`EmulatorShell` already lives in `CassoEmuCore`, so this wiring is reachable by
tests too. **PASS.**

### II. Testing Discipline

- **Isolation**: file access goes through `IFileSystem`, pipes through
  `IPipeTransport`, and process identity and instance listing through an
  `IInstanceDirectory` seam. No unit test opens a real pipe or file; the one
  real-pipe test is marked integration.
- **Acceptance tests** required by the user: the ROM command-table coverage
  test (FR-027) and the 1979 Reference Manual listing test (FR-028). Both fail,
  never skip, when fixtures are missing.
- **Degraded operation**: each test asserts a non-zero item count before
  iterating (23 table entries, N listing lines). The AppleWin name sweep covers
  the name list in both directions: every table name resolves to a handler or
  to a declared phase or not-available status, and every handler has a table
  name.

**PASS.**

### III. User Experience Consistency

The new `debug` subcommand follows `--flag` conventions and gets a `--help`
page. Errors go to stderr in the two-line shape. Existing commands are
unchanged. **PASS.**

### IV. Performance Requirements

No per-instruction cost is added without a session beyond one pointer test
(R-004). Batch runs are unthrottled. **PASS.**

### V. Simplicity & Maintainability

- Both modes compile to one `DebugCommand` intermediate form executed by one
  engine, so no command is implemented twice.
- AppleWin `A` and Monitor `!` share one `LineAssembler`.
- The disassembler reuses `Microcode`; the line assembler reuses `OpcodeTable`.
- The scope is large by the owner's decision (the whole AppleWin table).
  Function-size limits are kept by one handler class per command family, not
  one class per command.

**PASS.**

### I. Code Quality

Standard rules. Two points need attention:

- The overlapped pipe loop must release handles and events on every path,
  which is what single-exit EHM is for.
- The "not available" outcome is a `CommandStatus` enum value, never
  `S_FALSE`.

**PASS.**

**Post-design re-check**: the data model, contracts and structure below
introduce no executable code and no un-seamed system access. **PASS.**

## Design

### Layers

```text
 Ways in           CassoCli debug        DebugChannelServer         DebuggerWindow
                   (batch, JSON Lines)   (pipe, JSON Lines)         (Dxui, phase 3)
                         \                    |                        /
 Parsing                  AppleWinParser  /  MonitorParser   (per session mode)
                                     \        /
 Intermediate                        DebugCommand
                                          |
 Engine                            DebugSession ---- Breakpoints / Watchpoints / Symbols
                                          |
 Target seam                        IDebugTarget
                                    /           \
                    MachineDebugTarget          FakeDebugTarget (tests)
                    (MachineHost, DebugMemoryView, per-instruction hook)
```

- **Parsers** turn a line into one or more `DebugCommand`s plus mode-local
  state updates (the Monitor's A1/A2/A3 and last-examined address). They never
  touch the target.
- **`DebugSession`** executes commands against `IDebugTarget` and produces a
  `Reply`, which carries a status, structured data, and text.
- **Formatters** (`AppleWinFormatter`, `MonitorFormatter`) render a reply's
  structured data as that mode's text (FR-013). The JSON form is rendered from
  the same data, so text and JSON cannot diverge (Story 1 scenario 5).
- **Run commands** (`G`, `GG`, `P`, `T`, `RTS`, Monitor `G`/`S`/`T`) return a
  `RunRequest` that the host loop satisfies: batch mode on its own thread, the
  emulator on the CPU thread. A `StopEvent` comes back and becomes a
  notification.

### Threading

- **Batch**: single thread. The session calls `MachineHost::RunCycles`
  directly with the hook installed.
- **Emulator**: the session is owned by the CPU thread. Pipe and window
  commands are posted through `CpuManager::PostCommand` as one new command id
  whose payload is the command line and a reply-sink id. Replies and
  notifications return through a thread-safe `IDebugReplySink`, which the pipe
  server fans out to clients and the window marshals to the UI thread.
- **Serialization** (FR-024): the CPU thread's command queue already runs
  commands one at a time in arrival order.

### Pipe lifetime

`DebuggerController`, owned by the shell, has one boolean fact: whether the
debugger is open. Opening it creates the session if needed and starts the
channel server; closing it stops the server, which disconnects every client,
and leaves breakpoints and pause state alone. In phase 2 the `--debugger` switch
opens the controller with no window. In phase 3 the same switch opens the
window, and the window's open and close drive the controller.

## Order of Work

1. **Foundation (phase 1)**: `DebugMemoryView`, the per-instruction hook,
   `IDebugTarget` and `MachineDebugTarget`, the promoted headless machine
   factory, `Disassembler`, `LineAssembler`, `ExpressionEvaluator`.
2. **Verification gates (phase 1, before any Monitor command work)**:
   - FR-027 ROM command-table test.
   - FR-028 listing test.
   - FR-029 checks against the fixture ROMs: `$F666` is the mini-assembler
     entry on the Enhanced //e and //c, and those ROMs' input path accepts
     lowercase.
   - The results update research.md R-011 and R-012. If either FR-029 check
     fails, the affected Monitor behavior is re-planned before it is built.
3. **Engine and AppleWin mode (phase 1)**: session, breakpoints and
   watchpoints, the full phase-1 name table, JSON and text replies.
4. **Monitor mode (phase 1)**: parser state machine, formatter, every FR-017
   form, the `/` prefix.
5. **Batch mode (phase 1)**: `CassoCli debug`, scripts, `--json`, cycle budget.
6. **Channel (phase 2)**: protocol, `IPipeTransport`, Win32 transport with the
   DACL, `DebuggerController`, `--debugger`, `debug --list` and
   `debug --attach`, protocol documentation.
7. **Window (phase 3)**: `DebuggerWindow` and its projection, the phase-3
   AppleWin commands, and the `R`/`W` filename prompt.

## Risks

- **The 1979 listing is a printed document.** The test needs a transcription
  of its instruction column. It is checked in as a fixture with a `LICENSE`
  note under the constitution's fixture rule. Data regions in the listing are
  marked in the transcription and excluded explicitly, never silently (R-010).
- **Symbol tables**: AppleWin's `.SYM` files are GPL and cannot be used. ROM
  symbols are authored from the Reference Manual's published entry-point
  names (R-009). Coverage is thinner than AppleWin's, but correct.
- **Banking-aware peeks on the //c** have to model `Apple2cRomBank` and
  INTCXROM without triggering them. `MemoryProbeHelpers` in UnitTest shows the
  probing shape, but the production view is new code with the most room for
  subtle error. Every machine gets a peek-versus-bus-read parity test over
  non-I/O ranges.
- **Scope**: the full AppleWin table is large. The name sweep keeps it honest,
  and phase 1 may land as several merges, one per command family.

## Project Structure

### Documentation (this feature)

```text
specs/035-debugger/
├── plan.md                        # This file
├── research.md                    # Phase 0 output
├── data-model.md                  # Phase 1 output
├── quickstart.md                  # Phase 1 output
├── contracts/
│   ├── debug-channel-protocol.md  # pipe protocol, sufficient for the VS Code adapter
│   ├── cli-debug.md               # CassoCli debug subcommand and script format
│   └── command-modes.md           # AppleWin and Monitor grammar, / prefix, output formats
├── checklists/
│   └── requirements.md
└── tasks.md                       # Phase 2 output (/speckit-tasks, NOT created here)
```

### Source Code (repository root)

```text
CassoCore/Debugger/                    # pure logic; no machine dependency
├── Disassembler.h/.cpp                # one instruction at an address, from a Microcode table
├── LineAssembler.h/.cpp               # one line -> bytes, over OpcodeTable (A and !)
├── ExpressionEvaluator.h/.cpp         # AppleWin expressions: hex, symbols, registers, operators
├── DebugCommand.h                     # intermediate command form (API type)
├── AppleWinCommandTable.h/.cpp        # every AppleWin name -> family, phase, availability
├── AppleWinParser.h/.cpp
├── MonitorParser.h/.cpp               # Monitor state machine: A1/A2/A3, continuation, ^X, S forms
└── MonitorState.h

CassoEmuCore/Debugger/
├── IDebugTarget.h                     # registers, peek/poke, step/run, soft switches, reset
├── DebugMemoryView.h/.cpp             # side-effect-free banked read and write, region labels
├── MachineDebugTarget.h/.cpp          # IDebugTarget over MachineHost
├── DebugHook.h                        # per-instruction hook interface MachineHost calls
├── DebugSession.h/.cpp                # executes DebugCommand -> Reply; owns the tables below
├── BreakpointTable.h/.cpp             # address bitmap, opcode, register/memory conditions
├── WatchpointTable.h/.cpp             # read/write ranges, bus observer install/remove
├── WatchTable.h/.cpp                  # AppleWin display watches, ZP pointers, bookmarks
├── SymbolTable.h/.cpp                 # ROM symbols per machine + user/source tables
├── Reply.h                            # status, structured data, text lines (API type)
├── AppleWinFormatter.h/.cpp
├── MonitorFormatter.h/.cpp
├── ReplyJson.h/.cpp                   # Reply <-> JSON via JsonWriter
├── Handlers/                          # one class per command family
│   ├── ExecutionHandlers.h/.cpp
│   ├── BreakpointHandlers.h/.cpp
│   ├── MemoryHandlers.h/.cpp
│   ├── RegisterHandlers.h/.cpp
│   ├── SymbolHandlers.h/.cpp
│   ├── DataDirectiveHandlers.h/.cpp
│   ├── ConfigHandlers.h/.cpp          # PWD, CD, LOAD, SAVE, DISASM, CYCLES, BENCHMARK, PROFILE
│   └── MonitorHandlers.h/.cpp         # I, N, ^K, ^P, ^B, ^C, ^Y, R, W, search, register edit
├── Channel/                           # phase 2
│   ├── IPipeTransport.h
│   ├── Win32PipeTransport.h/.cpp      # overlapped pipe, user-SID DACL, reject remote
│   ├── DebugChannelServer.h/.cpp      # clients, request routing, notification fan-out
│   ├── ChannelProtocol.h/.cpp         # JSON Lines framing, hello, versioning
│   ├── IInstanceDirectory.h
│   └── Win32InstanceDirectory.h/.cpp  # enumerate \\.\pipe\Casso.Debug.*
└── DebuggerController.h/.cpp          # open/closed state -> session + server lifetime

CassoEmuCore/Shell/
├── MachineHost.h/.cpp                 # CHANGE: debug hook in StepOne/RunCycles
├── HeadlessMachineFactory.h/.cpp      # NEW: promoted from UnitTest TestMachine builder
├── CpuManager / EmulatorShell*.cpp    # CHANGE: debug command id, attach at build/switch
└── MachineManager.cpp                 # CHANGE: re-attach session on switch, notify reset

CassoEmuCore/Cli/
├── DebugMode.h/.cpp                   # NEW: batch runner, --list, --attach
└── CliMain.cpp                        # CHANGE: debug arm

CassoCore/
├── CommandLineOptions.h               # CHANGE: Subcommand::Debug, DebugOptions, --debugger
└── CommandLineParser.cpp              # CHANGE: flags and help page

CassoEmuCore/Ui/Debugger/              # phase 3
├── DebuggerWindow.h/.cpp              # DxuiWindow: disasm, registers, memory, stack, watches, bps, command line
└── DebuggerViewState.h/.cpp           # testable projection of session state for the window

UnitTest/DebuggerTests/
├── DisassemblerTests.cpp              # both CPUs, undocumented set
├── MonitorListing1979Tests.cpp        # FR-028
├── MonitorCommandTableTests.cpp       # FR-027, every shipped ROM
├── MonitorRomFactsTests.cpp           # FR-029: $F666 entry, lowercase input
├── LineAssemblerTests.cpp
├── ExpressionEvaluatorTests.cpp
├── DebugMemoryViewTests.cpp           # peek vs bus parity per machine, no side effects
├── DebugHookTests.cpp
├── AppleWinCommandTableTests.cpp      # name sweep, both directions
├── AppleWinParserTests.cpp
├── MonitorParserTests.cpp
├── DebugSessionTests.cpp              # fake target
├── *HandlersTests.cpp                 # one per family, real TestMachine where banking matters
├── FormatterTests.cpp                 # AppleWin and Monitor text; JSON parity
├── DebugModeTests.cpp                 # scripts, budget, determinism (two runs byte-equal)
├── ChannelProtocolTests.cpp           # phase 2
├── DebugChannelServerTests.cpp        # in-memory transport: many clients, fan-out, close
├── DebuggerControllerTests.cpp
└── DebuggerViewStateTests.cpp         # phase 3

UnitTest/Fixtures/Debugger/
├── AppleII-1979-MonitorListing.txt    # transcription of the instruction column
├── LICENSE                            # provenance and attribution
└── Scripts/*.txt + expected/*.txt|.jsonl
```

**Structure Decision**: Parsers, the disassembler, the line assembler and the
expression evaluator depend only on CPU tables, so they live in
`CassoCore/Debugger/`. The engine needs `MachineHost`, `MemoryBus`, JSON and
the devices, so it lives in `CassoEmuCore/Debugger/`. The window follows the
existing debug panels into `CassoEmuCore/Ui/`. Every new file is listed by hand
in its `.vcxproj`, since the projects use no globs.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| A per-instruction hook in `MachineHost`, the hottest loop in the emulator | Breakpoints, opcode breaks and stepping must stop before a given instruction executes | Checking once per `RunCycles` slice misses addresses inside the slice. Rewriting code bytes with `BRK` corrupts guest-visible memory and fails against ROM. The hook is a pointer test when unused (R-004) |
| A second headless machine builder path (`HeadlessMachineFactory`) | `CassoCli debug` must boot a real machine | Leaving it in UnitTest makes batch mode impossible. The factory is promoted, and `TestMachine` becomes a thin user of it, so there is still one builder |
