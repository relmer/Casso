---

description: "Task list for 035-debugger"
---

# Tasks: Debugger

**Input**: Design documents from `/specs/035-debugger/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The constitution requires unit tests for all production code (Principle II), and the spec makes FR-027 (ROM command tables), FR-028 (1979 listing) and FR-029 (ROM facts) acceptance requirements. Every test that reads fixtures asserts a non-zero item count first, and fails rather than skipping when data is missing. Each new test is mutation-checked: stub or revert the code it covers and confirm it goes red.

**Organization**: Tasks are grouped by user story. Plan phase 1 (headless engine, both modes, batch) is US1 and US2, plan phase 2 (the pipe) is US3, and plan phase 3 (the window) is US4.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: US1-US4 from spec.md

## Conventions for every task

- **New source files** go into the project's `.vcxproj` by hand: `CassoCore/CassoCore.vcxproj`, `CassoEmuCore/CassoEmuCore.vcxproj` or `UnitTest/UnitTest.vcxproj`. The projects use no globs.
- **Code style** follows `.github/copilot-instructions.md`: `#include "Pch.h"` first, EHM with a single exit, declarations at the top of scope, class statics rather than free functions, no spec or task numbers in comments.
- **No `S_FALSE`.** The "not available" outcome is `CommandStatus::NotAvailable`.
- **Before committing a new file**, run `scripts/CheckStyle.ps1 -Mode Staged`. Commit once per phase.
- **Validation**: `scripts/RunTests.ps1 -Build -Filter Debugger` during work, and the full suite at each checkpoint.

---

## Phase 1: Setup

**Purpose**: Directories, project entries and fixture scaffolding

- [X] T001 Create directories `CassoCore/Debugger/`, `CassoEmuCore/Debugger/`, `CassoEmuCore/Debugger/Handlers/`, `CassoEmuCore/Debugger/Channel/`, `CassoEmuCore/Ui/Debugger/`, `UnitTest/DebuggerTests/` and `UnitTest/Fixtures/Debugger/Scripts/`. Add a `DebuggerTests` filter folder to `UnitTest/UnitTest.vcxproj.filters` if that project has a filters file.
- [X] T002 [P] Create `UnitTest/Fixtures/Debugger/LICENSE` covering the directory: the transcribed 1979 *Apple II Reference Manual* Monitor listing (Apple Computer, Inc., 1979; archive.org item `Apple_II_Reference_Manual_1979_Apple`), marked read-only to the tests that use it.
- [X] T003 [P] Confirm the 11 fixture ROMs are present with `scripts/FetchRoms.ps1 -Fixtures` (whitelisted). Record in `specs/035-debugger/research.md` R-010 the SHA-256 of `UnitTest/Fixtures/Apple2.rom` that the listing test pins.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The target seam, the side-effect-free memory view, the per-instruction hook, the headless machine, and the pure CPU-table tools every story uses

**⚠️ CRITICAL**: No user story work begins until this phase is complete

### Pure CPU-table tools (CassoCore)

- [X] T004 [P] Write `UnitTest/DebuggerTests/DisassemblerTests.cpp`: every opcode `$00`-`$FF` for the 6502 table (`Cpu::GetMicrocode`, including the undocumented set from `Cpu::InitializeUndocumented`) and for the 65C02 table produced by `Cpu65C02`. Assert the length, mnemonic, operand text in Monitor form (`#$A0`, `($3E),Y`, `$1234,X`), branch target resolution, and `documented` flag per data-model "disassembly" kind. Assert the swept opcode count is 256 per CPU.
- [X] T005 Implement `CassoCore/Debugger/Disassembler.h/.cpp`: `Disassembler (const Microcode * table)`, and `DisassembleOne (Word address, std::span<const Byte> bytes)` returning a `DisassembledInstruction {address, bytes, mnemonic, operand, target (optional Word), documented}`. It is built from `Microcode::instructionName` and `globalAddressingMode` and must not reuse `Cpu::PrintSingleStepInfo`. Makes T004 pass.
- [X] T006 [P] Transcribe `UnitTest/Fixtures/Debugger/AppleII-1979-MonitorListing.txt` from the 1979 Reference Manual's listing of the **original** Monitor ROM (the one with `S` and `T`), not the Autostart ROM listing the same manual also carries. Before transcribing, compare the listing's first page of bytes against `Apple2.rom` at $F800 to confirm the fixture is that ROM. Format: one line per instruction as `ADDR BYTES MNEMONIC OPERAND`, and data regions as `DATA start-end` lines. Record the transcription's line count in a header comment line starting with `;`.
- [X] T007 Write `UnitTest/DebuggerTests/MonitorListing1979Tests.cpp` (FR-028, SC-003), per research R-010:
  1. Load `Apple2.rom` through `FixtureProvider`, and assert its $F800-$FFFF bytes equal the transcription's bytes.
  2. Disassemble from $F800, skipping only declared `DATA` regions.
  3. Assert address, length, mnemonic and operand field by field for every line.
  4. Assert the instruction count equals the transcription's non-zero count.

  Depends on T005 and T006.
- [X] T008 [P] Write `UnitTest/DebuggerTests/LineAssemblerTests.cpp`: one line in Monitor mini-assembler syntax (`LDA #$41`, `JMP ($0036)`, branches to absolute targets turned into relative offsets, out-of-range branch errors) for the 6502 and 65C02 tables. It round-trips through `Disassembler`.
- [X] T009 Implement `CassoCore/Debugger/LineAssembler.h/.cpp` over an `InstructionSetProvider` (base and 65C02 extended tables) and `OpcodeTable::TryLookup` / `GetOperandSize`. `TryAssemble (Word address, const std::string & line, std::vector<Byte> & outBytes, std::string & outError)` returns a status enum, not a bool. Makes T008 pass.
- [X] T010 [P] Write `UnitTest/DebuggerTests/DebugExpressionEvaluatorTests.cpp`: hex with and without `$`, decimal with `#` per AppleWin, `+ - * / & | ^ ! < >`, parentheses, register names `A X Y P S PC`, memory dereference, and symbol lookup through an injected resolver. Also cover errors for unknown symbols and malformed input.
- [X] T011 Implement `CassoCore/Debugger/DebugExpressionEvaluator.h/.cpp` (the assembler already owns `class ExpressionEvaluator` in `CassoCore/ExpressionEvaluator.h`, so the name must differ) with an `IDebugExpressionContext` seam (`TryGetRegister`, `TryPeek`, `TryResolveSymbol`) and a parsed `Expression` API type that breakpoint conditions store. Makes T010 pass.

### API types

- [X] T012 [P] Create `CassoCore/Debugger/DebugCommand.h`: `DebugVerb` enum (total over every engine operation; ends in `Count` for sweeps) and `DebugCommand` with the fields in data-model.md: `verb`, `sourceName`, `a1/a2/a3` plus presence flags, `values`, `expression`, `text`, `count`, `budget` (`std::optional<uint64_t>`).
- [X] T013 [P] Create `CassoEmuCore/Debugger/Reply.h`, holding the reply-side types from data-model.md:
  - `CommandStatus { Ok, Error, NotAvailable, Unknown }`;
  - the `ReplyData` variant, with one struct per data kind listed in `contracts/debug-channel-protocol.md`;
  - `Reply { status, command, data, text, error {label, detail} }`;
  - `StopReason` and `StopEvent`;
  - `RunRequest { kind: Go | StepInto | StepOver | StepOut | RunTo | Trace, untilPc, count, budget }`.

### Target seam, memory view and hook (CassoEmuCore)

- [X] T014 Create `CassoEmuCore/Debugger/IDebugTarget.h` with the operations in data-model.md "IDebugTarget": `GetRegisters/SetRegisters`, `Peek/Poke/GetRegion`, `ReadIo/WriteIo`, `GetSoftSwitches`, `StartRun (const RunRequest &)` with the stop delivered to `IRunObserver::OnStopped`, `RequestPause`, `SetHookInstalled`, `SetWatchedPages`, `GetVideoPosition`, `GetCpuKind`, `GetMachineInfo`, `InjectKey`. Also `IRunObserver.h` and `IRunDriver.h` (`Start`, `Pause`). Add `UnitTest/DebuggerTests/MockDebugTarget.h`, an in-memory 64 KB implementation for session tests that records hook and mask changes.
- [X] T015 [P] Write `UnitTest/DebuggerTests/DebugMemoryViewTests.cpp` (R-003) against `TestMachine` for Apple ][, ][+, //e, Enhanced //e and //c:
  - **Parity below $C000 and above $D000**: across banking states set through the soft switches (RAMRD/RAMWRT, ALTZP, 80STORE, language-card bank 1/2 read and write), `Peek` equals the value a CPU read returns; bus reads there have no side effects. `UnitTest/EmuTests/MemoryProbeHelpers.h` shows the probing pattern.
  - **Parity in $C100-$CFFF**: for each INTCXROM/SLOTC3ROM/INTC8ROM state, `Peek` equals the byte of the slot or internal ROM image that state selects, compared without a bus read, because a bus read there latches the router.
  - **No side effects**: a full peek sweep leaves every soft switch, the `CxxxRomRouter` latches and the speaker state unchanged.
  - **Region labels** match `mainRam|auxRam|lcBank1|lcBank2|rom|slotRom|io`.
  - **Writes**: `Poke` to ROM returns read-only and changes nothing.
  - **Non-zero**: assert that the number of addresses swept is non-zero.
- [X] T016 Implement `CassoEmuCore/Debugger/DebugMemoryView.h/.cpp` per research R-003: the bus's shadow page tables below $C000 (never the published ones, which hold null for a watched page); slot and internal ROM images for $C100-$CFFF selected by `IMmu::GetIntCxRom/GetSlotC3Rom` and the `CxxxRomRouter` state; `LanguageCard::IsReadRam/IsWriteRam/IsBank2/ReadRom` and `Apple2cRomBank` for $D000-$FFFF. $C000-$C0FF is never read; `Peek` reports it unreadable. Makes T015 pass.
- [X] T017 [P] Write `UnitTest/DebuggerTests/DebugHookTests.cpp`:
  - with a null hook, `MachineHost::RunCycles` behavior and cycle counts are unchanged;
  - with a hook that stops at an address, `StepOne` and `RunCycles` stop before that instruction executes, including an address in the middle of a slice;
  - a pending-stop flag set during an instruction stops at the next boundary.
  Also write `UnitTest/DebuggerTests/MemoryBusWatchMaskTests.cpp`:
  - with an empty mask, `SetReadPage`/`SetWritePage` publish the pointer and the fast path is unchanged;
  - marking a page watched publishes null and keeps the MMU pointer in the shadow table; a later `SetReadPage` on that page updates the shadow and leaves the published entry null; unmarking republishes the current shadow pointer;
  - a read or write of a watched page returns and stores the right value through the slow path, and a changed displayed byte in a video-watched page raises the video-dirty flag;
  - a watched address in $C000-$FFFF calls the device exactly once;
  - each access to a watched page reports `{address, value, access}` to the registered `IWatchSink`.
- [X] T018 Create `CassoEmuCore/Debugger/DebugHook.h` (`ShouldStopBefore (Word pc)`, `HasPendingStop()`). Change `CassoEmuCore/Shell/MachineHost.h/.cpp`: add `SetDebugHook (DebugHook *)` and call it before each instruction in `StepOne` and in the `RunCycles` loop, doing only a null-pointer test when unset (R-004). Change `CassoEmuCore/Core/MemoryBus.h/.cpp` per R-004: shadow read and write tables, a 256-entry watch mask, `SetWatchedPage (page, bool)`, `SetWatchSink (IWatchSink *)`, and the mask test on the slow path of `ReadByte` and `WriteByte` before `FindDevice`; `GetReadPageTable` keeps returning the published table for the CPU, and `GetShadowReadPage`/`GetShadowWritePage` are added for the memory view. Microbenchmark instructions per second before and after with no hook and an empty mask; record the numbers in the commit message. Makes T017 pass, and every existing `MemoryBus` and MMU test must still pass unchanged.
- [X] T019 Promote the machine-building code from `UnitTest/EmuTests/TestMachine.h/.cpp` into `CassoEmuCore/Shell/HeadlessMachineFactory.h/.cpp` with an `IRomSource` seam (R-006). `TestMachine` becomes a user of the factory, backed by a `FixtureProvider` ROM source. All existing `EmuTests` must still pass unchanged.
- [X] T020 Implement `CassoEmuCore/Debugger/MachineDebugTarget.h/.cpp`: `IDebugTarget` over `MachineHost`, `DebugMemoryView` and `I6502DebugInfo`, taking an `IRunDriver`; and `SynchronousRunDriver.h/.cpp`, whose `Start` calls `RunCycles` in chunks until a stop, `untilPc` or the budget, and delivers `OnStopped` before returning. A run with no budget is unbounded. `SetHookInstalled` and `SetWatchedPages` forward to `MachineHost` and `MemoryBus`. Add `UnitTest/DebuggerTests/MachineDebugTargetTests.cpp`, covering run-to, budget stop with reason `Budget`, step-over of a recursive subroutine, registers round trip, `GetVideoPosition`, and soft-switch listing on each machine.

**Checkpoint**: Full suite green. The disassembler matches the 1979 listing, peeks are side-effect free on every machine, and the hook costs nothing when unset.

---

## Phase 3: User Story 1 - Break into a running program from a script (Priority: P1) 🎯 MVP

**Goal**: The engine, AppleWin mode for every phase-1 name, symbols and binary formats, and `CassoCli debug` batch mode with text and JSON Lines output

**Independent Test**: `CassoCli debug --machine apple2e --script stop.txt`, where the script enters a loop at $0300 reading $C019 and sets `BPMR C019`, stops after the read with the reading instruction's address, prints registers and three steps, and yields byte-identical output on a second run, in both text and `--json` (quickstart phase 1 steps 2-4)

### Session and tables

- [X] T021 [P] [US1] Write `UnitTest/DebuggerTests/BreakpointTableTests.cpp` over data-model "Breakpoint":
  - kinds `Address`, `Opcode`, `Register`, `Memory`, `Io`, `Brk`, `Interrupt`;
  - enable and disable;
  - stable ids until cleared;
  - `hitCount`;
  - the 64 KB bitmap rebuilt on every change, and consulted before opcode and condition breakpoints.
- [X] T022 [US1] Implement `CassoEmuCore/Debugger/BreakpointTable.h/.cpp`. Makes T021 pass.
- [X] T023 [P] [US1] Write `UnitTest/DebuggerTests/WatchpointTableTests.cpp`:
  - `Read`, `Write` and `ReadWrite` on inclusive `first`/`last` ranges;
  - ids shared with the breakpoint numbering;
  - the watched-page mask handed to the target covers exactly the pages of enabled watchpoints, and shrinks when one is cleared or disabled;
  - a sink report inside a watched range with a matching access kind records `{accessPc, address, value, access}` and sets the pending stop; one outside the range, or of the other kind, is ignored.
- [X] T024 [US1] Implement `CassoEmuCore/Debugger/WatchpointTable.h/.cpp` as the bus's `IWatchSink`, publishing its page mask through `IDebugTarget::SetWatchedPages` (R-004). Makes T023 pass.
- [X] T025 [P] [US1] Implement `CassoEmuCore/Debugger/WatchTable.h/.cpp` for AppleWin watches (`W*`), zero-page pointers (`ZP*`, `P0`-`P4`) and bookmarks (`BM*`), with `UnitTest/DebuggerTests/WatchTableTests.cpp`.
- [X] T026 [P] [US1] Write `UnitTest/DebuggerTests/DebugSessionTests.cpp` against `MockDebugTarget`:
  - the state transitions in data-model "DebugSession", including `FreeRunning` adopted into a `DebugRun` by `g` with reply `ok`;
  - a run command while `DebugRun` or `Stepping` returns `Error` "already running";
  - the hook is installed when the first enabled stop condition of any kind (address, opcode, register, memory, I/O, `BRK`, `BRKOP`, `BRKINT`, watchpoint) appears or a run starts, and removed when the last one goes and no run is active;
  - a stop while `FreeRunning` produces `stopped` and `Paused`;
  - machine switch clears breakpoints and watchpoints, and reset keeps them;
  - mode switch keeps all tables;
  - unknown and malformed commands change no state.
- [X] T027 [US1] Implement `CassoEmuCore/Debugger/DebugSession.h/.cpp`: `Execute (const DebugCommand &) -> Reply`, ownership of the tables, dispatch to handler families, `OnStopped` turning a `StopEvent` into the `stopped` notification, and the session budget: unbounded by default, set by `BUDGET <n>`, cleared by `BUDGET 0`, overridden per run by `DebugCommand::budget` (R-005). Makes T026 pass.

### AppleWin command table, parser and formatter

- [X] T028 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinCommandTableTests.cpp`. It sweeps the name list in spec Assumptions "AppleWin command coverage" in both directions:
  - every listed name resolves to a phase-1 handler, to phase 3, or to not-available with its reason text;
  - every phase-1 `DebugVerb` has at least one name;
  - aliases resolve to their target;
  - a name in no list is `Unknown`;
  - the listed-name count is asserted non-zero, and the phase-1, phase-3 and not-available counts are printed and recorded in plan.md Scale/Scope.
- [X] T029 [US1] Implement `CassoCore/Debugger/AppleWinCommandTable.h/.cpp`, a file-scope `static constexpr` table (`s_kAppleWinCommands`) of `{name, family, phase, availability, reason}`. Makes T028 pass.
- [X] T030 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinParserTests.cpp`:
  - case-insensitive names;
  - `$` and bare hex;
  - expressions through `DebugExpressionEvaluator`;
  - argument forms for each family, taken from AppleWin's help pages `help/dbg-*.html` (names and behavior only, never implementation; R-014);
  - Casso engine commands `MODE`, `MODE APPLEWIN`, `MODE MONITOR`, `PAUSE` and `BUDGET`.
- [X] T031 [US1] Implement `CassoCore/Debugger/AppleWinParser.h/.cpp` producing `DebugCommand`. Makes T030 pass.
- [X] T032 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinFormatterTests.cpp`, pinning text per `contracts/command-modes.md`:
  - `R` as `A:00 X:00 Y:00 P:30 S:FF PC:0300` plus a flag string;
  - `D` as eight bytes per row with ASCII;
  - disassembly lines;
  - breakpoint lists;
  - the two-line error for `Error` and for `NotAvailable` (`Error: command not available` / `NAME needs the debugger window.`).
- [X] T033 [US1] Implement `CassoEmuCore/Debugger/AppleWinFormatter.h/.cpp` rendering `Reply::data` to `Reply::text`. Makes T032 pass.
- [X] T034 [P] [US1] Write `UnitTest/DebuggerTests/ReplyJsonTests.cpp`:
  - every data kind in `contracts/debug-channel-protocol.md` serializes with integer addresses and bytes, never hex strings;
  - unreadable I/O bytes serialize as `null`;
  - each record is one line with no raw newline;
  - for a sample of commands, the JSON `text` array equals the formatter's text (Story 1 scenario 5).
- [X] T035 [US1] Implement `CassoEmuCore/Debugger/ReplyJson.h/.cpp` over `JsonWriter` with pretty printing off. Makes T034 pass.

### Handler families (phase-1 AppleWin names)

Each handler task adds the family's tests in `UnitTest/DebuggerTests/<Family>HandlersTests.cpp`, using `TestMachine` wherever banking or devices matter. Expected outputs come from AppleWin's documented examples, and argument forms and behavior from research R-014, R-018 and R-019, which were checked against AppleWin's own behavior: `Z` is `DB` and `B` lists data blocks; `BPIO` is an alias of `BPM`; `BPA` sets a program-counter breakpoint and a watchpoint; `BRK` takes `[0|1|2|3|ALL] [ON|OFF]`; `BPCHANGE` takes `E`/`T`/`S` flags; `F` also takes `start end value`; `MEB` writes a value above `$FF` as two bytes; `S`/`SH` share the item syntax with `?` wildcards and `@n` results; `PRINT`, `PRINTF`, `CALC` and `LOG` have the forms in R-014 and R-019; the data directives take `[name] [range]` and name their blocks; `SYM<table>` takes `CLEAR | LOAD | ON | OFF`; and `LBR`, `PROFILE` and `TF` record only during debugger-driven runs.

- [X] T036 [P] [US1] `CassoEmuCore/Debugger/Handlers/ExecutionHandlers.h/.cpp` (`BENCHMARK` reports not available until the emulator wiring in T070 supplies a host clock):
  - `G`, `GG` (both unthrottled in batch; in the emulator `GG` sets full speed and restores the previous `SpeedMode` on stop), `P` (step over: run until PC is at the instruction after the `JSR` with SP restored, so recursion is one call), `T`, `TL`, `RTS` (step out) and `=`;
  - `BPV` and `VIDEOINFO` from `GetVideoPosition`;
  - `JSR`, `NOP`/`ZAP`, and `KEY` (queued by cycle, R-015);
  - `LBR`;
  - `TF` (trace to a file through `IFileSystem`);
  - `PROFILE`, `BENCHMARK`/`BENCH`/`EXITBENCH`, `CYCLES` and `RCC`.
- [X] T037 [P] [US1] `CassoEmuCore/Debugger/Handlers/BreakpointHandlers.h/.cpp`:
  - setting: `BP`, `BPA`, `BPR`, `BPX`, `BPIO`, `BPM`, `BPMR`, `BPMW`, `BRK`, `BRKOP`, `BRKINT`;
  - managing: `BPC`, `BPD`, `BPE`, `BPL`, `BPEDIT`, `BPCHANGE`;
  - saving: `BPSAVE`.
- [X] T038 [P] [US1] `CassoEmuCore/Debugger/Handlers/RegisterHandlers.h/.cpp`:
  - `R`/`REGISTER`;
  - `CL`, `CLC`, `CLZ`, `CLI`, `CLD`, `CLB`, `CLR`, `CLV`, `CLN`;
  - `SE`, `SEC`, `SEZ`, `SEI`, `SED`, `SEB`, `SER`, `SEV`, `SEN`;
  - aliases `RC` `RZ` `RI` `RD` `RB` `RR` `RV` `RN` and `SC` `SZ` `SI` `SD` `SB` `SR` `SV` `SN`;
  - stack: `POP`, `PPOP`, `PUSH`, and the Casso engine command `STACK` (FR-007), which lives here with the other stack commands.
- [X] T039 [P] [US1] `CassoEmuCore/Debugger/Handlers/MemoryHandlers.h/.cpp`:
  - view and enter: `D`, `MDB`, `ME`, `MEB`, `MEW`, `ME8`, `ME16`;
  - move, compare, fill: `M`/`MM`, `MC`, `F`;
  - search: `S`/`MS`, `SH`, `@`;
  - files: `BLOAD`, `BSAVE`, `TSAVE`;
  - I/O: `IN`/`INPUT`, `OUT`;
  - the Casso engine command `SWITCHES` (FR-007), as defined in `contracts/command-modes.md`.
- [X] T040 [P] [US1] `CassoEmuCore/Debugger/Handlers/DataDirectiveHandlers.h/.cpp`: `Z`, `X`, `B`, `DB`, `DB2`, `DB4`, `DB8`, `DW`, `DW2`, `DW4`, `ASC`, `DF`, `DA`, and `U` disassembly honoring those data ranges. Also `A addr`, which enters the line-assembly mode Monitor `!` uses (each following line assembled through `LineAssembler`, a blank line ends it), as `contracts/command-modes.md` defines.
- [X] T041 [P] [US1] `CassoEmuCore/Debugger/Handlers/ConfigHandlers.h/.cpp`: `PWD`, `CD`, `LOAD`, `SAVE`, `DISASM`, `STARTUP`, `RUN` (a script through the same session), `DISK` (`INFO` and `SLOT`; `EJECT`, `INSERT` and `PROTECT` report not available until the emulator wiring in T070), `LOG`, `ECHO`, `PRINT`, `PRINTF`, `CALC`, `?`, `HELP`, `VERSION`, `MOTD`. The watch, zero-page and bookmark commands (`W*`, `ZP*`, `P0`-`P4`, `BM*`) and their `WSAVE`, `ZPSAVE` and `BMSAVE` live in `Handlers/WatchHandlers.h/.cpp`, beside the tables they act on.

### Symbols and binary formats (FR-031, FR-032, FR-033)

- [X] T042 [US1] Capture the Merlin symbol-table layout:
  1. On a copy of `UnitTest/Fixtures/Disks/Merlin-proDos2.23.dsk` (never the pristine image), boot Merlin Pro under Casso and load `LABELS.S` (`L`, `E`, `ASM`), following the capture procedure behind `scripts/CaptureMerlinCorpus.ps1`. Record the Merlin version the menu reports.
  2. Read the trailing symbol table off the emulated text screen once, to learn the layout. **No listing file is checked in.** Two routes that look available are not: Casso's printer path renders to a dot raster and then to PNG, never to text, and the listing itself scrolls too fast to scrape whole from a 24-line screen. The symbol table is capturable because it sits static at the end of the assembly.
  3. **Dropped.** There is no repeatable way to get a Merlin listing off the emulated screen into a file, so no `LABELS.listing.txt` is committed and the fixture directory is unchanged. Merlin 32 is a cross-assembler that could generate one, but it is a different program whose format would have to be verified against Merlin Pro 2.23 first; that is separate work, not a prerequisite here.
  4. Record in research R-009 the symbol-table layout it shows: section headings, column widths, and how `]` variables and local labels appear. **Done**, captured from `MAKE DUMP.S` rather than `LABELS.S`, which defines neither construct.
- [X] T043 [US1] Change `CassoCore/Assembler.h/.cpp` `FormatListing`, used by the Merlin dialect, to append a symbol table in the layout recorded in R-009. The table is formatted in `Assembler::FormatMerlinSymbolTable` and appended by `ArtifactWriter::WriteListing`, which is the path `merlin -l` actually takes; `FormatListing` itself is shared with as65 and is unchanged. Add tests in `UnitTest/MerlinListingSymbolTableTests.cpp` checking the table section of `CassoCli merlin -l LABELS.S` output against an inline expected sample quoting R-009, since no listing fixture is committed (T042 step 3). The table is appended only when the dialect profile is Merlin. Add an as65 case to the same test file asserting an as65 listing is unchanged.
- [X] T044 [US1] Add `-g` to `CassoCli merlin`:
  - a flag row in `CassoCore/CommandLineParser.cpp` for the Merlin dialect;
  - `CassoEmuCore/Cli/MerlinMode.cpp` writes `<output>.dbg` per `SAV` output through `ArtifactWriter::WriteDebugInfo`, on the per-output rule `As65Mode::WriteExtraArtifacts` uses;
  - tests in `UnitTest/PerOutputArtifactTests.cpp` and `UnitTest/MerlinCommandLineTests.cpp`;
  - documentation in `docs/Assembler.md` under Merlin.
- [X] T045 [P] [US1] Write `UnitTest/DebuggerTests/SymbolFileReaderTests.cpp` (the Merlin listing case runs against an inline sample until T042 lands the fixture):
  - Casso `-g` (`NAME=$ADDR`, `;` comments, both sections);
  - the Merlin listing symbol table, against an inline sample in the layout R-009 records;
  - AppleWin `.SYM` (`ADDR NAME`);
  - VICE labels (`al ADDR .NAME`);
  - detection from content;
  - an error for an unrecognized file;
  - the symbol count asserted non-zero for each fixture.
- [X] T046 [US1] Implement `CassoCore/Debugger/SymbolFileReader.h/.cpp` and `CassoEmuCore/Debugger/SymbolTable.h/.cpp` (tables `Main`, `Basic`, `Asm`, `User`, `User2`, `Src`, `Src2`, `Dos33`, `ProDos`; case-insensitive lookup). Makes T045 pass.
- [X] T047 [P] [US1] Author ROM symbol tables in `-g` format as file-scope tables in `CassoEmuCore/Debugger/RomSymbols.cpp`, one per machine plus DOS 3.3 and ProDOS entry points. Take names and addresses only from Apple's published Reference Manuals and DOS/ProDOS technical references, never from another emulator's symbol files (FR-031), and record the source of each table in a comment. `UnitTest/DebuggerTests/RomSymbolsTests.cpp` checks spot entries (`COUT $FDED`, `GETLN $FD6A`, `MONZ $FF69`) and that each table is non-empty.
- [X] T048 [P] [US1] Write `UnitTest/DebuggerTests/AppleSingleCodecTests.cpp`: read and write round trip of the data fork, real name, and ProDOS file info (type, aux type); magic `$00051600` detection; errors for truncated and unknown-version input. The 033-cassque branch carried no codec when this was checked, so T049 added one.
- [X] T049 [US1] Implement `CassoEmuCore/Core/AppleSingleCodec.h/.cpp` if 033 has not: no knowledge of memory, disks or the debugger (research R-016). Makes T048 pass.
- [X] T050 [P] [US1] Write `UnitTest/DebuggerTests/BinaryImageReaderTests.cpp`:
  - Intel HEX and S-record, with checksums and multiple segments;
  - AppleSingle, taking its address from the aux type;
  - DOS 3.3 binary and raw, each selected explicitly;
  - an error for a raw load with no address;
  - content detection never guessing DOS 3.3 binary.
- [X] T051 [US1] Implement `CassoEmuCore/Debugger/BinaryImageReader.h/.cpp` (in EmuCore because it uses the codec there; CassoCore cannot reference CassoEmuCore) returning `{segments, format}`. `BLOAD` in T039 uses it. Makes T050 pass.
- [X] T052 [P] [US1] `CassoEmuCore/Debugger/Handlers/SymbolHandlers.h/.cpp`: `SYM`, `SYMMAIN`, `SYMBASIC`, `SYMASM`, `SYMUSER`, `SYMUSER2`, `SYMSRC`, `SYMSRC2`, `SYMDOS33`/`SYMDOS`, `SYMPRODOS`/`SYMPRO`, `SYMINFO`, `SYMLIST`. Tests in `UnitTest/DebuggerTests/SymbolHandlersTests.cpp`. The bookmark commands are in `WatchHandlers` (T041).

### Batch mode

- [x] T053 [P] [US1] Write `UnitTest/DebuggerTests/DebugOptionsParseTests.cpp` for `contracts/cli-debug.md` batch options:
  - `--machine`, `--disk1`, `--disk2`, `--script` (including `-`), repeatable `--command`, `--mode`, `--json`, `--max-cycles`, `--seed`, `--write-disks`;
  - `debug --help`;
  - usage errors.
- [x] T054 [US1] Add `Subcommand::Debug` and `DebugOptions` to `CassoCore/CommandLineOptions.h`, parsing in `CassoCore/CommandLineParser.cpp`, and the `debug` help page in `CassoEmuCore/Cli/CommandLine.cpp`. Update `UnitTest/CliSwitchCoverageTests.cpp` for the new grammar. Makes T053 pass.
- [x] T055 [P] [US1] Write `UnitTest/DebuggerTests/DebugModeTests.cpp` with scripts in `UnitTest/Fixtures/Debugger/Scripts/` and expected output in `expected/*.txt` and `*.jsonl`:
  - Story 1 scenarios 1-5, using the script in quickstart phase 1 step 2 (a loop at $0300 that reads $C019, no disk);
  - the exit statuses 0, 1, 2 and 3 from the contract;
  - `;` comment lines;
  - a mode switch mid-script;
  - disk writes going to the overlay unless `--write-disks`;
  - two runs of each script compared byte for byte (SC-004), including a script that power-cycles, which only passes if the DRAM seed is pinned;
  - exit status 1 taking precedence over 3;
  - all host file access (scripts, `BLOAD`, `BSAVE`, `R`, `W`, `TF`) through a mock `IFileSystem`, with no real files.
  - Note: no command power-cycles the machine, so the seed check dumps never-written DRAM instead (same seed twice matches, another seed differs). `R`/`W` are Monitor-mode commands and wait for US2. The script file itself is read at the console edge; the runner takes its text.
- [x] T056 [US1] Implement `CassoEmuCore/Cli/DebugMode.h/.cpp` (batch runner taking an injected `IFileSystem`, on `HeadlessMachineFactory` and `MachineDebugTarget`, no wall clock, the `Prng` seeded from `--seed` with default `0xCA550001`, every run carrying the `--max-cycles` budget, R-015) and the `debug` arm in `CassoEmuCore/Cli/CliMain.cpp`. Makes T055 pass.
  - Note: `DebugBatchRunner` (CassoEmuCore/Cli) holds the machine, the disks and the session; `DebugMode` is the console edge over it. `DebugHandlerSet` attaches every command family. `FileRomSource` (Shell) finds `Machines/<id>/<id>.json` and the ROMs on the executable, working and asset directories. Found while writing the golden outputs: the bus's watch sink was never connected to the session's watchpoint table on a real machine, and the first instruction of a run reported its accesses from $0000; both fixed in `DebugSession` with `IDebugTarget::SetWatchSink`.

**Checkpoint**: Quickstart phase 1 steps 2-4 and 6 pass by hand, and the full suite is green. The MVP is usable for bug diagnosis.

---

## Phase 4: User Story 2 - Use Apple II Monitor syntax (Priority: P1)

**Goal**: Monitor mode with the union of every ROM's commands on every machine, the `/` prefix, and Monitor-format output

**Independent Test**: A batch script with `--mode monitor` runs `300: A9 41 60`, `300.302`, `300L`, `41<300.3FFS`, `300S`, `^E`, `: 01 02 03`, `/bpl` and gets Monitor-format output (quickstart phase 1 step 5)

### Verification gates (run before any Monitor command work)

- [x] T057 [US2] Write `UnitTest/DebuggerTests/MonitorCommandTableTests.cpp` (FR-027, SC-002), per research R-011:
  - for `Apple2.rom`, `Apple2Plus.rom`, `Apple2e.rom`, `Apple2eEnhanced.rom` and `Apple2c.rom`, read 23 command bytes at $FFCC and handler offsets at $FFE3;
  - decode each byte with the inverse of the transform in the routine at $FFBE, first asserting that known entries (`L`, `G`, `M`) decode correctly on every ROM;
  - exclude only entries matching the declared filler rule (character `$EA` with handler `$00`, the //c's last entry);
  - assert every other decoded command is in `MonitorParser`'s command set;
  - assert 23 entries were read per ROM.

  Until T060 exists, the test compiles against an empty command set and fails; that is expected.

  Done, and `EveryTableEntry_IsAMonitorCommand` is the one red test on the branch until T060. Verified and recorded in R-011: the transform is `((entry - $89) AND $FF) XOR $B0`; the table is read through a built machine rather than at a file offset, because the //c's Monitor table is in ROM bank 0 and the end of that 32 KB file is bank 1, which is all zeros there; the declared filler rule holds exactly (the //c's last entry, character `$EA`, handler `$00`, the only `$00` handler on any ROM); and the ][+ and //e carry three `^Y` entries where the ][ has `^Y`, `T` and `S`, with real but unreachable handlers, which is why the union of every ROM's commands is a real feature.
- [x] T058 [US2] Write `UnitTest/DebuggerTests/MonitorRomFactsTests.cpp` (FR-029), per research R-012:
  - on `Apple2eEnhanced.rom` and `Apple2c.rom`, decode the `!` entry of the $FFCC/$FFE3 command table, follow its handler, and assert it prints the `!` prompt and calls the input routine; assert on `Apple2.rom` that $F666 is that ROM's mini-assembler entry; record what the ][+, //e, Enhanced //e and //c hold at $F666 (Applesoft), which is why `F666G` is an alias, not a jump;
  - boot each of those two machines to the `*` prompt in `TestMachine`, type `300l` with `UnitTest/EmuTests/KeystrokeInjector.h`, and assert with `UnitTest/EmuTests/TextScreenScraper.h` that a listing appears.

  If either assertion fails against the ROM, stop and update spec FR-017/FR-021 and research R-012 with what the ROM does before continuing.

  Both claims hold; two corrections to how they are reached are recorded in R-012. The `!` handler is at `$FE00 + offset + 1` (the dispatcher at $FFBE pushes the page and the offset and returns), and on both machines it reaches the internal $Cxxx firmware: the //c jumps straight there, the Enhanced //e pages it in with $C007 first. The prompt is printed inside that firmware, not in the handler, so the assertion is that it reaches $Cxxx. $F666 holds a JMP only on the original ][; the other four hold Applesoft. For the lowercase check, `CALL -151` is unreachable, because with no disk the //e spins in its startup firmware and the //c stops at "Check Disk Drive" -- so the test enters at $FF59, byte-identical on all five ROMs, and presses keys on `MachineRefs::keyboard` rather than through `KeystrokeInjector`, which waits on the //e-specific pointer and landed no keys. The Enhanced //e and //c then echo `*300l` and disassemble from $0300: the ROM upshifts the command without upshifting the echo. FR-017 and FR-021 stand as written.

### Implementation

- [x] T059 [P] [US2] Write `UnitTest/DebuggerTests/MonitorParserTests.cpp` for every form in `contracts/command-modes.md` "Apple II Monitor mode":
  - examine, range, `.addr`, and continuing on Return or space;
  - deposit with and without an address;
  - `L`, `M`, `V`, `+`, `-`, `G`, `I`, `N`, `n^K`, `n^P`, `^B`, `^C`, `^Y`, and `^E` followed by `:`;
  - `S` as step versus `value<start.endS` as search, where no step command has the search form;
  - `T`, `!`, `F666G`, and `R`/`W` with and without a quoted filename;
  - several commands on one line;
  - control characters as the character and as `^` plus the letter, in either case;
  - lowercase input;
  - `/rest-of-line` routed to the AppleWin parser;
  - a malformed line produces an error and no command.
- [x] T060 [US2] Implement `CassoCore/Debugger/MonitorState.h` (`a1`-`a4`, `lastExamined`, `storeAddress`, `registerEditPending`, `assemblerActive`) and `CassoCore/Debugger/MonitorParser.h/.cpp`, following the Monitor's line scan (R-013). Makes T059 pass and, with T057, the ROM coverage test.
  - Both now pass, so the branch is green again. Notes on what the implementation settled:
    - **No expression context.** Monitor numbers are bare hex, with no operators and no symbols, so `Parse` takes only the line and the state where the AppleWin parser needs an `IDebugExpressionContext`.
    - **No command letter is a hex digit.** G, I, L, M, N, R, S, T, V and W all sit outside A-F, and the control commands arrive as control codes, so the scan never has to guess whether `B` is a digit or a command.
    - **The `/` hand-off moved into the parser.** `MonitorParseResult::appleWinLine` carries what followed the slash, which puts the routing decision in one place instead of leaving it in `DebugSession::ExecuteLine`.
    - **Command shapes match the AppleWin handlers**, so one handler serves both modes: `dest<start.endM` fills a3 with the destination and a1/a2 with the source range, exactly as `M dest range` does, and `41<300.3FFS` fills a1/a2 and `values`. `addrG` sets a3 rather than a1, which is the field `ExecuteRun` already reads to set the program counter rather than run to an address.
    - `MonitorState` keeps `a1`-`a4` as the Monitor's own documented model, but the per-line accumulation is local to the scan and nothing reads those four yet. If T063's handlers do not need them either, they should go.
- [x] T061 [P] [US2] Write `UnitTest/DebuggerTests/MonitorFormatterTests.cpp`:
  - examine `0300- A9 00 8D 00 03 60 ...`, rows aligned to 8-byte boundaries (`303.30F` gives `0303-` with five bytes then `0308-` with eight), identical on every machine;
  - list `0300-   A9 00       LDA   #$00`;
  - verify `0303-41 (42)` per difference;
  - registers `A=00 X=00 Y=00 P=30 S=FF`;
  - arithmetic, 8-bit: `FF+FF` gives `=FE`;
  - step and trace display in the original ]['s step output layout;
  - `ERR` followed by the two-line error.
- [x] T062 [US2] Implement `CassoEmuCore/Debugger/MonitorFormatter.h/.cpp`. Makes T061 pass.
  - Two decisions worth recording:
    - **A reply the Monitor has no layout for keeps the AppleWin text.** The only way to reach such a command from Monitor mode is to type `/`, and a reader who did that asked for the AppleWin command, so its output is the right answer rather than a gap. `TryFormatData` returning false is what routes it.
    - **The step display is two pieces, not one.** `StopEvent` carries registers but not the instruction's bytes, so the formatter cannot disassemble it. The original ]['s step layout is the instruction line from the step's own reply followed by the register line from the stop, which is how T063 will emit it.
  - The row alignment in `Examine_RowsAlignToEightByteBoundaries` is a property of the rows the command builds, not of this class; the test pins the five-then-eight rendering, and the splitting belongs to T063.
- [x] T063 [US2] Implement `CassoEmuCore/Debugger/Handlers/MonitorHandlers.h/.cpp` with the effects in research R-013, applied directly, never by jumping into ROM:
  - `I`/`N` set `INVFLG` ($32) to $3F/$FF;
  - `n^K` sets `KSWL/H` ($38/$39) to $Cn00 and `n^P` sets `CSWL/H` ($36/$37) to $Cn00 for slots 1-7; `0^K` restores $FD1B and `0^P` restores $FDF0;
  - `^B`/`^C` run at $E000/$E003, and `^Y` runs at $03F8;
  - `G` pushes the return address the ROM's `G` handler pushes and sets an internal breakpoint there that takes no id, is absent from `BPL`, and is removed when it fires or the run stops otherwise; `G` never reloads registers from $45-$49; a test sets A in AppleWin mode, runs Monitor `G`, and sees A unchanged;
  - `^E` shows registers and arms `:` register edit, which sets the CPU registers and writes $45-$49;
  - search prints matching addresses;
  - `R`/`W` use `IFileSystem`: `R` reads the smaller of file and range and reports a mismatch; with no filename they return an error in batch and pipe;
  - `!` and `F666G` enter `LineAssembler` mode.

  Tests in `UnitTest/DebuggerTests/MonitorHandlersTests.cpp` cover Story 2 scenarios 1-6, including a breakpoint set in AppleWin mode listed from Monitor mode.
  - Done, with `DebugSession` wired to the Monitor parser and formatter. What it settled, and what it caught:
    - **The session's mode decides the layout**, so `/R` typed at a `*` prompt prints the Monitor's `A=00 X=00 Y=00 P=24 S=FD` rather than AppleWin's line. A reply the Monitor has no layout for keeps the AppleWin text.
    - **A bare `MODE APPLEWIN` is not a mode switch at a Monitor prompt**: `M` is the Monitor's move command and the line reads as one. `/MODE APPLEWIN` is the way back, and two of these tests were wrong about that before the run corrected them.
    - **Return continues to the end of the eight-byte row, not eight bytes on.** Continuing from $0301 shows seven bytes and stops at $0307; asking for eight would spill one byte into the next row and print a second line holding one byte.
    - **`300S` and `300T` set the program counter**, where AppleWin's `T` and `P` take a count and never an address, so `ExecuteRun` keys on the address being present.
    - **A crash, not a wrong answer**: `MemoryHandlers::Search` reads a mask byte for every value byte, so the Monitor parser filling only `values` walked off the end of `mask` and took the test host down. The Monitor has no wildcard, so every mask byte is $FF.
    - **A power-cycled machine has no usable stack pointer.** It is whatever the power-on pattern left, and a Monitor `G` pushes its return address onto that stack; with the pointer near zero the push wraps, the RTS pops two unrelated bytes, and the run never ends. The rig sets the pointer and a cycle budget, because a test that hangs the suite is a far worse way to find out than one that fails.
- [x] T064 [US2] Add Monitor-mode scripts and expected output to `UnitTest/Fixtures/Debugger/Scripts/` and cases to `UnitTest/DebuggerTests/DebugModeTests.cpp` for quickstart phase 1 steps 5 and 6, with `out.bin` held in the mock `IFileSystem`.
  - `monitor.txt` with text and JSON Lines goldens, plus `modes.txt` rewritten to deposit known bytes so its examine is about the mode switch rather than about the power-on pattern. Reading the generated output found two things:
    - **A defect: a stop was printed in AppleWin wording whatever mode the reader was in.** `300S` at a `*` prompt said `Step at $0302` instead of the Monitor's register line, because `DebugBatchSink` formatted stops without asking the session for its mode. Fixed; the JSON form carries the stop as data and never varied.
    - **`^E` arms exactly one colon, and arming it twice eats the next deposit.** The first draft of the script had two, so `400: 00 00 00` set the registers instead of memory. That is the Monitor's own rule working correctly, and it is easy to trip over, so the script says so where a reader of the fixture will see it.
  - `L` is deliberately absent from the script: it lists twenty instructions whatever follows the three deposited, so the golden would mostly pin the disassembly of the power-on pattern.

**Checkpoint**: The FR-027, FR-028 and FR-029 tests pass on every fixture ROM, and the full suite is green. Plan phase 1 is complete and mergeable to master.

---

## Phase 5: User Story 3 - Attach to a running Casso (Priority: P2)

**Goal**: A per-process, current-user named pipe carrying the JSON Lines protocol, many serialized clients, notifications, `debug --list` and `debug --attach`, and `Casso --debugger`

**Independent Test**: Start Casso with `--debugger`, list it, attach, `bpmr C000` (the keyboard read at the `]` prompt), `g`, and receive a `stopped` notification. A second client receives it too (quickstart phase 2)

- [x] T065 [P] [US3] Write `UnitTest/DebuggerTests/ChannelProtocolTests.cpp` for `contracts/debug-channel-protocol.md`:
  - framing: LF-terminated lines, CR tolerated on input, lines capped at 1 MiB;
  - `hello` in both directions, including a client `protocol` higher than the server's;
  - `command` requests with optional `mode` and `budget`, and `pause`;
  - replies echo `id`; notifications carry no `id`, and a stop caused by a command carries its `causeId`;
  - an `error` record for malformed input, with unknown fields ignored.
- [x] T066 [US3] Implement `CassoEmuCore/Debugger/Channel/ChannelProtocol.h/.cpp`. Makes T065 pass.
  - The layer is data in and data out, with no pipe, thread or session in it, so the whole contract is testable without any of them. Notes:
    - **Replies and notifications stay `ReplyJson`'s.** The contract promises the channel's records are the records `--json` prints, and two writers would be two chances to disagree; `WriteReply` and `WriteStopped` already took the optional `id` and `causeId` the channel needs. What this adds is the half `ReplyJson` has no reason to know: reading a request, the handshake, and the error record for a line that never became a request.
    - **The JSON writer pretty-prints by default**, which would put a raw newline inside every record and break the framing for every reader of the stream. The compact option is not a preference here, it is the frame.
    - **The server answers with its own protocol version** whatever the client asked for, so a client built against a later contract learns what it is talking to instead of being refused.
    - An empty drive is `null` rather than an empty string, because "no disk" and "a disk whose path is empty" are different facts.
  - Tests parse each record back rather than comparing text, so key order and spacing stay free; the one thing asserted about the bytes is that no record holds a raw newline.
- [x] T067 [US3] Create `CassoEmuCore/Debugger/Channel/IPipeTransport.h` (listen, accept, read line, write line, close; per-connection ids) and `UnitTest/DebuggerTests/InMemoryPipeTransport.h`.
  - **Pulled rather than pushed.** Every read is a question the server asks when it is ready for an answer, which is what lets commands run one at a time in arrival order without a lock: the server pumps the transport from one thread and no callback arrives from another. It is also why the server's behavior is testable at all, since the double then drives a whole multi-client conversation with no threads and no pipe.
  - **Lines carry no terminator.** Framing belongs to the transport: the real one appends LF and splits on it, and a line split across two reads or two lines in one read are its problem rather than the server's.
  - **A write to a client that has gone is not an error.** A client can disappear between the server deciding to answer and the answer being written, and there is nothing useful left to do about it.
  - The double keeps what each client received per connection rather than merged, because telling a reply that went to one client from a notification that went to all of them is the distinction the contract turns on. Ids are never reused, so a reply cannot reach whoever inherited a closed client's slot.
- [x] T068 [P] [US3] Write `UnitTest/DebuggerTests/DebugChannelServerTests.cpp` over the in-memory transport:
  - several clients, with commands run one at a time in arrival order and each reply sent only to its requester;
  - every notification delivered to every client (Story 3 scenario 6);
  - a client disconnecting while paused leaves the machine paused;
  - server close sends `closing` and disconnects all clients.
- [x] T069 [US3] Implement `CassoEmuCore/Debugger/Channel/DebugChannelServer.h/.cpp` and a thread-safe `IDebugReplySink`, routing commands through `CpuManager::PostCommand` with a new command id. Makes T068 pass.
  - **The server does not hold the session.** Carrying a command out has to happen on the CPU thread and the server runs on its own, so `IDebugCommandRunner` is the seam: the thread hop lives on one side of it and the protocol on the other. The `CpuManager::PostCommand` routing is the runner's, and lands with the emulator wiring in T070; keeping it out of the server is what lets the whole of the server's behavior be tested with no machine.
  - **One at a time in arrival order is a property of pumping, not of locking.** `Pump` reads one line, carries it out, and only then reads the next, so two clients cannot interleave halfway through a command because nothing is ever halfway through one.
  - **Notifications arrive from the other thread**, so they are queued under a lock and written from the pump. That queue is the only shared state, which is why it is the only lock.
  - A stop names the command that caused it, and the name is taken before the line runs, because a synchronous machine delivers the stop before `RunLine` returns. The cause is spent by the stop that used it, so a later unprompted stop does not inherit it.
  - `AClientDisconnectingLeavesTheMachineAlone` guards against a change nobody has made yet rather than against a line that exists: no mutation of today's code can kill it. It is worth keeping because someone may be sitting at the machine, and a client closing its window is not a reason to start it running again.
- [X] T070 [US3] Wire the session into the emulator:
  - `CassoEmuCore/Shell/CpuManager.h/.cpp` and `EmulatorShellCpuThread.cpp`: a debug command id whose payload is the line plus a reply-sink id;
  - `CassoEmuCore/Debugger/CpuManagerRunDriver.h/.cpp` (R-005): `Start` records the run, applies `fullSpeed` to `SpeedMode`, un-pauses `CpuManager` and returns; the frame loop's `RunCycles` slices run with the hook installed and count the budget across slices; on a hook stop, `RunCycles` returns the short slice, the driver pauses `CpuManager`, restores `SpeedMode`, and delivers `OnStopped` from the CPU thread; `Pause` does the same with reason `pause`. The same stop path serves a breakpoint that fires while the machine is `FreeRunning`. Tests drive `EmulatorShell` headless and confirm that a `pause` request is processed while a run is in progress, that a breakpoint set with no `g` stops a free-running machine, and that `g` on a free-running machine replies `ok`;
  - `MachineManager.cpp`: attach the session beside `AttachDebugSinksIfOpen`, emit `machineChanged` and clear the tables on switch, and emit `reset` on soft reset and power cycle;
  - `EmulatorShell` user pause emits `stopped` with reason `pause`.

  Tests go in `UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp`. `EmulatorShell` cannot run a frame in a test (it needs `Initialize`, an HWND and a message pump), so the tests drive the pieces rather than the shell: the run driver against a real `TestMachine`, `CpuManager` and stop hook, standing in for the slice loop; the command dispatcher against a recording target; and the session against the mock target. The shell's one-line forwarders are exercised by running the emulator.
- [X] T071 [P] [US3] Write `UnitTest/DebuggerTests/DebuggerControllerTests.cpp`: opening starts the server and closing stops it and disconnects clients, leaving breakpoints and pause state alone; `--debugger` opens the controller at machine start without pausing. (The controller starts in the machine's current state, tested here; the shell creates it at start once T073 supplies the real transport.)
- [X] T072 [US3] Implement `CassoEmuCore/Debugger/DebuggerController.h/.cpp` and parse `--debugger` in `CommandLineParser::ParseEmulator` (`CassoCore/CommandLineParser.cpp`). Add `--debugger` to the emulator's documented-options table in the same file so `Casso --help` lists it, with a case in `UnitTest/CliSwitchCoverageTests.cpp`. Makes T071 pass.
- [X] T073 [US3] Implement `CassoEmuCore/Debugger/Channel/Win32PipeTransport.h/.cpp`, per research R-008:
  - pipe name `\\.\pipe\Casso.Debug.<pid>`;
  - a DACL granting `GENERIC_READ | GENERIC_WRITE` to the current token's user SID only, built with `GetTokenInformation` and `SetEntriesInAclW`;
  - `PIPE_REJECT_REMOTE_CLIENTS`, `FILE_FLAG_FIRST_PIPE_INSTANCE` on the first instance, and `PIPE_UNLIMITED_INSTANCES`;
  - overlapped I/O with every handle and event released on every path.

  Build the security descriptor in a data-in/data-out function and test it in `UnitTest/DebuggerTests/PipeSecurityTests.cpp`: a given SID yields a DACL with exactly one allow entry, for that SID. Put every pipe call (`CreateNamedPipeW`, `ConnectNamedPipe`, `ReadFile`, `WriteFile`, `GetOverlappedResult`, `CancelIoEx`, `DisconnectNamedPipe`, `CloseHandle`, event creation and waits) behind `CassoEmuCore/Debugger/Channel/INamedPipeApi.h`. `Win32NamedPipeApi.h/.cpp` is a pass-through with no logic. Test `Win32PipeTransport` in `UnitTest/DebuggerTests/Win32PipeTransportTests.cpp` against `UnitTest/DebuggerTests/MockNamedPipeApi.h`:
  - the pipe name for a given PID;
  - `PIPE_REJECT_REMOTE_CLIENTS` on every instance, and `FILE_FLAG_FIRST_PIPE_INSTANCE` on the first instance only;
  - the security descriptor passed to `CreateNamedPipeW`;
  - a failure injected at each call releases every handle and event already acquired, and returns that call's error;
  - pending reads, a disconnect during a read, one line split across reads, and several lines in one read;
  - a line over 1 MiB.

  No test opens a real pipe. Windows enforcing the access list is checked by quickstart phase 2 step 5, which is manual, and the pipe as a whole by the SC-007 client (T077).
- [X] T074 [P] [US3] Write `UnitTest/DebuggerTests/InstanceDirectoryTests.cpp` over a mock `IInstanceDirectory`: listing omits instances that refuse the connection or don't answer `hello`, and the output columns are `pid title machine disk1 disk2`.
- [X] T075 [US3] Implement `CassoEmuCore/Debugger/Channel/IInstanceDirectory.h` and `Win32InstanceDirectory.h/.cpp` (enumerate `\\.\pipe\` for `Casso.Debug.`). Add `debug --list` and `debug --attach <pid>` to `CassoEmuCore/Cli/DebugMode.cpp`, with parsing in `CassoCore/CommandLineParser.cpp`. Every run it starts carries `budget` from `--max-cycles` (default 100000000); after a run command it waits for `stopped` for at most `--timeout` seconds (default 120), then sends `pause` and exits with status 3; it exits with status 2 when the pipe closes. Add `--list` and `--attach` to the `debug` help page in `CassoEmuCore/Cli/CommandLine.cpp`. Makes T074 pass.
- [X] T076 [US3] Write `docs/DebugChannel.md` from `contracts/debug-channel-protocol.md` as user-facing documentation.
- [X] T077 [US3] SC-007 validation: write `scripts/DebugChannelClient.ps1`, a client that uses only `System.IO.Pipes.NamedPipeClientStream` and `docs/DebugChannel.md`, and run quickstart phase 2 steps 1-6. Step 5, the other-user connection, is manual; record its result in the commit message.

**Checkpoint**: Quickstart phase 2 passes, and the full suite is green. Plan phase 2 is mergeable.

---

## Phase 6: User Story 4 - Debug in a window beside the emulator (Priority: P3)

**Goal**: A Dxui debugger window with a command line, disassembly, registers, memory, stack, watches and breakpoints; click-to-set breakpoints; step, step-over, run and run-to-cursor controls; and the phase-3 AppleWin commands

**Independent Test**: Open the window, click a disassembly line to set a breakpoint, run to it, edit a byte in the memory view, and confirm with `d 300` (quickstart phase 3)

- [X] T078 [P] [US4] Write `UnitTest/DebuggerTests/DebuggerViewStateTests.cpp` for the projection of session state:
  - disassembly around PC with the current line flagged;
  - register and flag rows;
  - memory rows with region labels;
  - stack, watches and breakpoints;
  - click-to-toggle breakpoint on a line;
  - a memory edit producing a poke;
  - step, step over, run and run-to-cursor producing the matching `RunRequest`;
  - the command line executing in the selected mode with the same reply as batch (Story 4 scenario 4);
  - a breakpoint set through `DebugChannelServer` over the in-memory transport appearing in the breakpoint pane, and one set by clicking appearing in a client's `bpl` reply (SC-006).
- [X] T079 [US4] Implement `CassoEmuCore/Ui/Debugger/DebuggerViewState.h/.cpp`. Makes T078 pass.
- [X] T080 [US4] Implement `CassoEmuCore/Ui/Debugger/DebuggerWindow.h/.cpp`, a `DxuiWindow` subclass following `Ui/Disk2DebugPanel`:
  - panes built from `DxuiListView`, `DxuiTextInput`, `DxuiToolbar` and `DxuiCommand` (032 widgets);
  - `OnCreate`/`OnWindowClose` drive `DebuggerController` open and close;
  - the `R`/`W` filename prompt when a filename is missing.

  Wire the menu item and open/re-attach in `CassoEmuCore/Shell/EmulatorShellDebug.cpp`, and make `--debugger` open the window.
- [X] T081 [US4] Implement the phase-3 AppleWin names in `CassoEmuCore/Debugger/Handlers/ViewHandlers.h/.cpp`, and move them to phase-1 availability in `CassoCore/Debugger/AppleWinCommandTable.cpp`:
  - cursor: `.`, `RET`, `^`, `v` and their Shift forms, `PAGEUP`, `PAGEUP256`, `PAGEUP4K`, `PAGEDN`, `PAGEDOWN256`, `PAGEDOWN4K`, and the `->` aliases;
  - window: `WIN`, `WINDOW`, `CODE`, `CODE1`, `CODE2`, `CONSOLE`, `DATA`, `DATA1`, `DATA2`, `SOURCE1`, `SOURCE2`, `\`;
  - mini memory panes: `MD1`, `MD2`, `MA1`, `MA2`, `MT1`, `MT2`, `M1`, `M2`;
  - views: `TEXT`, `TEXT1`, `TEXT2`, `TEXT80`, `TEXT81`, `TEXT82`, `TEXT40`, `TEXT41`, `TEXT42`, `GR`, `GR1`, `GR2`, `DGR`, `DGR1`, `DGR2`, `HGR`, `HGR0`-`HGR8`, `DHGR`, `DHGR1`, `DHGR2`;
  - appearance: `BW`, `COLOR`, `FONT`, `HCOLOR`, `MONO`.

  Their projection is covered in `DebuggerViewStateTests.cpp`, and in batch and pipe they still return `notAvailable`.
- [X] T082 [US4] Validate the window by running Casso in the background with `--title 035-debugger` and capturing it with `PrintWindow`. Run quickstart phase 3 steps 1-4 and attach the screenshots to the commit.

**Checkpoint**: All four stories are functional and the full suite is green.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T083 [P] Write `docs/Debugger.md`: ways in, both modes, the `/` prefix, Casso engine commands, symbol and binary formats, and batch examples. Link it from `README.md` and update README's feature list and test counts.
- [ ] T084 [P] Add `CHANGELOG.md` entries under `[Unreleased]` for each merged phase. Keep them terse, give only the user-visible effect with `GH #51` first, and show them for approval before pushing.
- [ ] T085 Run the pre-merge gate for each phase merge:
  - `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`;
  - the full Debug and Release suites with `scripts/RunTests.ps1`;
  - `scripts/RunTests.ps1 -Build -Scenario`, because DISK and disk overlays touch what a guest reads;
  - `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`, keeping `.specify/feature.json` out of the commit;
  - `scripts/RunHarteTests.ps1 -SkipGenerate` at full depth, because `MachineHost`'s instruction loop changed.

  Report any suite that could not run and why.
- [ ] T086 Run all of quickstart.md end to end and record the results in the final merge commit message.
- [ ] T087 Investigate the emulation slowdown measured when T018 landed (Release x64, `CycleEmulation_MeetsBudget`, 1M //e cycles: 5.59/5.63/5.61 ms before, 5.78/5.88/5.87 ms after, about 4%):
  1. Confirm it is real: time a longer window (50M cycles) with 10 or more runs per build, on the commit before `9f12ea90` and on the current head.
  2. If it is real, attribute it by reverting one change at a time: the `m_debugHook` tests in `MachineHost::StepOne` and `RunCycles`; the `m_debugWatched` test and `ReadFromDevice` split on the `MemoryBus::ReadByte` slow path; `StoreToPage` on the write fast path; the placement of the shadow tables inside `MemoryBus`.
  3. Fix what is found (for example, a separate `RunCycles` loop taken only while a hook is set) and record before and after numbers in the commit message. With no session attached the cost must not be measurable, as plan.md Performance Goals require.

---

## Phase 8: Watchpoint modes and corrected command behavior

**Purpose**: FR-004's two watchpoint modes, the value a write replaced, and the
command behavior research R-014, R-018 and R-019 corrected against AppleWin's
own behavior. These run inside US1, before batch mode (T056).

- [X] T088 Report the value a write replaced (FR-004a, R-017): `CassoEmuCore/Core/IWatchSink.h` gains the previous byte on the write path, which `MemoryBus::WriteWatchedPage` already reads for the video-dirty test; it is absent where the page is device-served. `WatchHit` (`Debugger/Reply.h`) gains `previous`, `WatchpointTable` records it, `AppleWinFormatter` prints `Write $41 to $0400 by $0803 (was $A0)`, and `ReplyJson` adds `previous`. Tests in `MemoryBusWatchMaskTests.cpp` (a RAM page reports it, a device page does not) and `WatchpointTableTests.cpp`.
- [X] T089 Implement `CassoCore/Debugger/EffectiveAddress.h/.cpp`: from an instruction's `Microcode` addressing mode, the bytes at the program counter and the current registers, the addresses the instruction would read and write, with the byte fetches taken through a peek seam so nothing is disturbed. Covers indexed, indirect and indexed-indirect modes with their wrapping, and reports read, write, or both for a read-modify-write. Tests in `UnitTest/DebuggerTests/EffectiveAddressTests.cpp` sweep every addressing mode on both CPU tables against hand-computed addresses.
- [X] T090 Add `Before` mode to `WatchpointTable` and `DebugSession` (FR-004): `BPM`, `BPMR` and `BPMW` accept a trailing `BEFORE` or `AFTER` in `AppleWinParser` (default `AFTER`), a `Before` watchpoint puts no page in the bus mask, and the session's `ShouldStopBefore` consults `EffectiveAddress` only while one is enabled. A stop reports `{accessPc, address, access}` and no value. Tests cover a before-stop leaving memory unchanged, an after-stop reporting the value, and the mask holding only after-mode pages.
- [X] T091 Add the `addrL` and `dest<start.endM` shorthands to `AppleWinParser`, and the `BEFORE`/`AFTER` keyword and the corrected argument forms listed in the handler-family note above, with cases in `AppleWinParserTests.cpp`. The behavior itself lands with each handler family (T036-T041, T052).
- [x] T092 Implement the commands AppleWin leaves as stubs (R-018): `MC` listing each difference; `BPEDIT # <definition>` replacing the entry under the same id and resetting its hit count; `WSAVE`, `ZPSAVE`, `BMSAVE`, `BPSAVE`, `SAVE` and `LOAD` writing and replaying command scripts; and `SYM<table> SAVE`. `ME` stays window-only. Tests write through the mock `IFileSystem` and replay each script through the session.
- [X] T093 Watchpoint hit rule and one-stop rule (R-017): `WatchpointTable` lets a write replace a pending read of the same address within one instruction, and a later write replace an earlier one; `DebugSession` suppresses an after-mode stop for the instruction that just caused a before-mode stop on the same range. Tests use an indexed store on a watched page and a range watched in both modes.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Setup and blocks every story. T007 needs T005 and T006; T016 needs T018 (the shadow tables); T020 needs T016, T018 and T019.
- **US1 (Phase 3)**: depends on Foundational. T027 needs T022, T024 and T025. The handler families T036-T041 and T052 need T027 and T033. T043 and T044 need T042. T046 needs T042. T051 needs T049. T056 needs T027, T031, T035 and T054.
- **US2 (Phase 4)**: depends on Foundational and on the US1 session, AppleWin parser and batch runner (T027, T031, T056), because `/` routes to AppleWin mode and scripts run through `DebugMode`. T057 and T058 come first; T063 needs T060 and T062.
- **US3 (Phase 5)**: depends on US1. T069 needs T066 and T067; T073 needs T067; T075 needs T073.
- **US4 (Phase 6)**: depends on US3's `DebuggerController` (T072).
- **Polish**: T083 and T084 run per phase merge; T085 and T086 run before each merge.

### Within each story

Tests are written first and fail, then the implementation makes them pass. After that, each test is mutation-checked.

## Parallel Examples

```text
# Foundational, after T001:
T004 DisassemblerTests       T006 listing transcription    T008 LineAssemblerTests
T010 DebugExpressionEvaluatorTests  T012 DebugCommand.h    T013 Reply.h
T015 DebugMemoryViewTests    T017 DebugHookTests + MemoryBusWatchMaskTests

# US1 handler families, after T027 and T033:
T036 Execution  T037 Breakpoint  T038 Register  T039 Memory  T040 DataDirective  T041 Config

# US1 formats, independent of the handlers:
T045 SymbolFileReaderTests  T047 RomSymbols  T048 AppleSingleCodecTests  T050 BinaryImageReaderTests
```

## Implementation Strategy

### MVP first

Phases 1-3 give a working batch debugger in AppleWin syntax: break, inspect, step, symbols, deterministic output. Stop, validate against quickstart phase 1, and merge.

### Incremental delivery

1. **Setup + Foundational**: the verified disassembler and side-effect-free machine access.
2. **US1**: batch debugger in AppleWin mode. Merge to master.
3. **US2**: Monitor mode with the ROM verification gates. Merge; plan phase 1 is complete.
4. **US3**: the named pipe and attach. Merge; plan phase 2 is complete.
5. **US4**: the window and the phase-3 commands. Merge; plan phase 3 is complete.

Phase 3 (US1) is large by the owner's decision to cover the whole AppleWin table. It may land as several merges, one per handler family, provided each merge passes the name sweep with the unfinished families still reporting `notAvailable`.
