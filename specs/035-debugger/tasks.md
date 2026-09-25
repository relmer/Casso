---

description: "Task list for 035-debugger"
---

# Tasks: Debugger

**Input**: Design documents from `/specs/035-debugger/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The constitution requires unit tests for all production code (Principle II), and the spec makes FR-027 (ROM command tables), FR-028 (1979 listing) and FR-029 (ROM facts) acceptance requirements. Every test that reads fixtures asserts a non-zero item count first, and fails rather than skipping when data is missing. Each new test is mutation-checked: stub or revert the code it covers and confirm it goes red.

**Organization**: Tasks are grouped by user story. Phases 1-8 are delivered: the engine (US1), Monitor mode (US2), the channel (US3) and the first window (the delivered part of US4), plus the watchpoint-mode corrections. Phases 9-18 are the 2026-09-18 expansion: US4 finished as a window worth releasing, then US5-US12 in spec priority order, then the release gates.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: US1-US12 from spec.md

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
- [X] T048 [P] [US1] Write `UnitTest/DebuggerTests/AppleSingleCodecTests.cpp`: read and write round trip of the data fork, real name, and ProDOS file info (type, aux type); magic `$00051600` detection; errors for truncated and unknown-version input. The 033-casso-explorer branch carried no codec when this was checked, so T049 added one.
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

## Phase 6: User Story 4 - Debug in a window beside the emulator (first window, delivered; finished in Phase 9)

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

**Checkpoint**: The first window works and the full suite is green. Its panes, fonts and symbol column are redone in Phase 9.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T083 [P] Write `docs/Debugger.md`: ways in, both modes, the `/` prefix, Casso engine commands, symbol and binary formats, and batch examples. Link it from `README.md` and update README's feature list and test counts. **As built:** written as the user guide and extended with every story by T163; the fit-and-finish work since (watch pane, result annotations, text selection, soft-switch naming) is covered by T166's release pass.
- [X] T084 [P] Add `CHANGELOG.md` entries under `[Unreleased]` for each merged phase. Keep them terse, give only the user-visible effect with `GH #51` first, and show them for approval before pushing. **Folded into T166:** the changelog records the branch's net effect in one entry at the end, not an entry per phase, so this is done by T166 rather than separately.
- [X] T085 Run the pre-merge gate for each phase merge:
  - `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`;
  - the full Debug and Release suites with `scripts/RunTests.ps1`;
  - `scripts/RunTests.ps1 -Build -Scenario`, because DISK and disk overlays touch what a guest reads;
  - `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`, keeping `.specify/feature.json` out of the commit;
  - `scripts/RunHarteTests.ps1 -SkipGenerate` at full depth, because `MachineHost`'s instruction loop changed.

  Report any suite that could not run and why.
- [X] T086 Run all of quickstart.md end to end and record the results in the final merge commit message.
- [X] T087 Investigate the emulation slowdown measured when T018 landed (Release x64, `CycleEmulation_MeetsBudget`, 1M //e cycles: 5.59/5.63/5.61 ms before, 5.78/5.88/5.87 ms after, about 4%):
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

## Phase 9: User Story 4 - Debug in a window beside the emulator (Priority: P1)

**Purpose**: The first window (Phase 6) becomes one worth releasing: dense
monospace panes, symbolic disassembly, panes sized by content, keyboard
schemes, and the per-frame snapshot every later pane draws from.

**Goal**: FR-010a, FR-026, FR-026a, FR-026c; SC-009

**Independent Test**: With the speech demo's debug file loaded, stop at
`Sing` and confirm the disassembly shows `Sing` on its line and `STA PTR`;
measure the row height against the font's line height; confirm three
breakpoints, two watches and the stack are visible without scrolling
(quickstart Story 4).

- [X] T094 [US4] Merge `origin/033-casso-explorer` into this branch (033's Dxui is stable and 035 builds on `DxuiSplitter`, `DxuiHexView`, `DxuiTextView` and `DxuiCommandRouter`; 033 reaches master first and 035 merges cleanly after it). Expect the accessor renames in CLAUDE.md's hazard; fix every stale call site the compiler surfaces, run `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`, and the full suite in Debug and Release.
- [X] T095 [P] [US4] Write `UnitTest/Dxui/DxuiListViewMetricsTests.cpp`: `SetRowHeightDip`, `SetCellPaddingDip (horizontal, vertical)` and `SetFontFamily` change one instance only; the defaults leave every existing list's measurements unchanged (30 DIP rows, 12 and 16 DIP padding, the theme's body face); measured auto-fit (`SetPreciseAutoFit`) sizes each column to its widest cell plus padding and, with stretch off, leaves the remaining width empty. Use `MockDxuiTextRenderer` for measurement.
- [X] T096 [US4] Implement the per-instance metrics in `Dxui/Widgets/DxuiListView.h/.cpp` (R-029). Makes T095 pass; every existing `DxuiListView` test must still pass unchanged.
- [X] T097 [P] [US4] Snapshot cadence tests in `UnitTest/DebuggerTests/DebuggerViewStateTests.cpp` (`CadenceTests`). The snapshot itself (`DebuggerViewSnapshot`, built on the CPU thread from command replies and handed to the UI thread as an immutable `shared_ptr`) was delivered with the first window; what FR-051 lacked was the cadence: it rebuilt every 100 ms while running. The tests pin one frame (`kBuildIntervalMs` at most 1000/60), no rebuild of a stopped machine for time alone, and an immediate rebuild on an action, a stop or a start.
- [X] T098 [US4] `DebuggerViewState::IsBuildDue` and `kBuildIntervalMs` (16 ms) in `CassoEmuCore/Ui/Debugger/DebuggerViewState.h/.cpp`, used by `EmulatorShell::PublishDebuggerView`, which already runs once per CPU-loop pass from `ServiceDebugger`; the 100 ms `kDebugViewIntervalMs` is gone. The label, operand-symbol and memory-window fields are added to `DebuggerViewSnapshot` by T099 and T110, where they are first used.
- [X] T099 [P] [US4] Symbolic disassembly (FR-010a) in every way in: `DisassembledInstruction` (`CassoCore/Disassembler.h`) gains `hasOperandAddress`/`operandAddress` for every memory-referencing mode, and `Disassembler::SubstituteSymbol` rewrites the address inside operand text (`($06),Y` to `(PTR),Y`; a bit branch names its destination). `DisassemblyLine.symbol` splits into `label` (the name at the line's address, or the data block it starts) and `operandSymbol`, filled by `U` from the session's symbol table. `U` prints the label in its own column (present only when some line has one) and the operand with the symbol in place of the address, replacing the trailing `; NAME`; Monitor `L` sets a labeled line's label on the line above; JSON `disassembly` records carry `label`, `operandSymbol` and `operandAddress` beside the numeric operand; the window's code pane shows both. Tests in `DisassemblerTests.cpp`, `AppleWinFormatterTests.cpp`, `MonitorFormatterTests.cpp`, `ReplyJsonTests.cpp` and `DataDirectiveHandlersTests.cpp`; `contracts/debug-channel-protocol.md` and `docs/DebugChannel.md` updated.
- [X] T100 [P] [US4] Write `UnitTest/Dxui/DxuiKeyMapTests.cpp` and implement `Dxui/Core/DxuiKeyMap.h/.cpp` (R-031): a named table of key chords to application command ids, the chord struct identical to `CassoExplorerCommands::kKeys` (vk, ctrl, alt, shift) so Casso Explorer can move onto it later; `TryTranslate (vk, ctrl, alt, shift, int & outCommandId)`; a `DxuiWindow` holds one active map, swappable at run time, and consults it in `OnKeyDown` after `DxuiCommandRouter::TranslateKey` finds no standard command. Then `CassoEmuCore/Ui/Debugger/DebuggerKeySchemes.h/.cpp`: the three maps `VisualStudio`, `AppleWin` and `GSSquared` with the keys in FR-026c for run, pause, step into, step over, step out, toggle breakpoint and run to cursor, tested in `UnitTest/DebuggerTests/DebuggerKeySchemesTests.cpp`; the chosen scheme name is a `GlobalUserPrefs` field (`CassoEmuCore/Config/GlobalUserPrefs.h/.cpp`) with a round-trip case in `UnitTest/GlobalUserPrefsTests.cpp`, default `VisualStudio`.
- [X] T101 [US4] Dense panes (FR-026a) in `CassoEmuCore/Ui/Debugger/DebuggerWindow.cpp`: every list through `DebuggerWindow::MakeDense` (Cascadia Mono at 12 DIP, 16-DIP rows, 22-DIP headers, 4-DIP cell padding, measured auto-fit, re-fit on every refresh), every column fitted to its contents with none stretching, the code pane's Label column ahead of the instruction, and a layout that owes each pane eight rows at the default size (default height 840 DIP; the stack moves to the bottom row beside memory). `DxuiListView` gains a per-instance header height and an opt-in `SetRefitOnSetRows` (033's default of fitting once is kept for large lists), with tests in `DxuiListViewMetricsTests.cpp`. **Split into pane classes moved to US7 (T133):** nothing needs a pane to be its own control until the dock site moves panes between groups and windows, so the split happens there.
- [X] T102 [US4] Route keys through the scheme: `DebuggerWindow` installs the chosen `DxuiKeyMap` and `OnMappedCommand` sends the line the matching button sends (`DebuggerViewState::GetActionLine`; Pause stays the channel's pause). A focused text box keeps its keys by `DebuggerKeySchemes::DoesBoxKeepKey`: F-keys and Ctrl/Alt chords reach the scheme, Space and Enter only from an empty box (the Space's character is swallowed), letters never. The toolbar gains Step Out, which the window lacked, and a Keys button that cycles the scheme and saves it through `IDebuggerWindowHost::SetDebuggerKeyScheme` into `GlobalUserPrefs::debuggerKeyScheme`. Tests: `ActionTests` in `DebuggerViewStateTests.cpp`, `DebuggerKeySchemesTests.cpp`, `GlobalUserPrefsTests.cpp`. Checked live: F10 posted to the window stepped the machine and the console showed `>P`.
- [X] T103 [US4] Validated live with Casso started in the background under the title `dbg-dev`: F10 posted to the window stepped the machine (PC checked over the pipe), and the captured window shows every pane in the monospace face at 16-DIP rows with columns fitted to their contents, all six registers, and eight memory and stack rows.
- [X] T104 [US4] SC-009 measured by what the window adds to the CPU thread: one snapshot build, which is time-gated to at most 60 a second whatever the emulation speed. Five runs pinned to one CCD (Release x64): 26.6-27.7 us a build, so about 1.6 ms of CPU-thread time a second, 0.16% of one core, against the 3% bound. The re-fit on refresh costs the UI thread, not emulation.

**Checkpoint**: The window is dense and symbolic, every pane shows its list, and the full suite is green.

---

## Phase 10: User Story 5 - Edit memory in place (Priority: P1)

**Goal**: FR-034 to FR-037; SC-012

**Independent Test**: Open a memory window at $0300, type `A9 41 60` into three consecutive byte cells, confirm memory after each pair, undo twice, and confirm the first byte remains (quickstart Story 5).

- [X] T105 [P] [US5] Write `UnitTest/DebuggerTests/MemoryEditModelTests.cpp` for data-model `MemoryWindow` and `UndoHistory`. The model is the `IDxuiHexSource` a memory window gives `DxuiHexView`: it reads through the snapshot and writes through a poke callback. Grouping `Bytes`, `Words` (written low byte first), `DoubleWords`; `pending` accumulates typed nibbles and a value is emitted when complete (two, four or eight digits, or one character in the text column); Escape clears `pending`; the history records `{address, written, replaced, region}` and undo pops most recent first and emits the replaced value; undo of one window leaves another's history alone; undo writes the edit's replaced value even when the machine has since changed the byte, and reports the value it wrote; a write into the `io` region is refused with the message naming `OUT`; the history is cleared on machine switch; the model is pure data.
- [X] T106 [US5] Implement `CassoEmuCore/Ui/Debugger/Panes/MemoryEditModel.h/.cpp`. Makes T105 pass. As built, the pending digits live in `DxuiHexView` (T109), where the caret is; the model serves the whole 64K from the window's latest snapshot bytes, turns each accepted write into a `PATCH` line, refuses I/O and any byte it was not shown, marks I/O and ROM for coloring, and keeps the window's undo.
- [X] T107 [P] [US5] Write `UnitTest/DebuggerTests/DebugMemoryViewPatchTests.cpp` (R-024) against `TestMachine`: `Patch` on an address whose region is `rom` changes the byte the CPU reads, in each of `RomDevice`, `LanguageCard`'s ROM and `CxxxRomRouter`'s internal and slot images; on the //c a patch at $C100-$CFFF survives a `$C028` bank flip; `Patch` on `mainRam` is a bus write; `Patch` on `io` is refused; an out-of-range `PatchByte` offset asserts.
- [X] T108 [US5] `TryPatch` on `RomDevice`, `CxxxRomRouter` (whichever image the switches select), `LanguageCard::TryPatchRom` and `Apple2cRomBank` (the current bank's own image plus the live router and language-card copies, so a patch survives a `$C028` flip and stays in its bank); `DebugMemoryView::TryPatch` (RAM as a poke, ROM through those, I/O refused) and `IDebugTarget::TryPatch`. The window reaches it through a new engine command, `PATCH addr value...`, in the Memory family beside `MEB` (which still refuses ROM), whose I/O refusal names `OUT`. Tests: `DebugMemoryViewPatchTests.cpp` (every machine's CPU reads the patch; the //c flip; RAM; I/O) and `Patch_RamAndRom_IoRefused` in `MemoryHandlersTests.cpp`; `contracts/command-modes.md` and `docs/Debugger.md` updated.
- [X] T109 [P] [US5] Add editing to `Dxui/Widgets/DxuiHexView.h/.cpp` (R-030, agreed with 033): `IDxuiHexSource` gains a virtual `WriteBytes` that defaults to refusing, so read-only sources stay read-only; an overwrite caret per nibble in the hex column and per byte in the text column, drawn on the focused cell and advancing on each typed digit or character; a completed value goes to `WriteBytes`; Escape abandons the pending nibbles; a refused write leaves the cell unchanged and reports through a callback. Tests in `UnitTest/Dxui/DxuiHexViewEditingTests.cpp` through `MockDxuiPainter` and a recording source; every existing `DxuiHexView` test must still pass.
- [X] T110 [US5] `CassoEmuCore/Ui/Debugger/Panes/MemoryPane.h/.cpp` over `DxuiHexView` with `MemoryEditModel` as its source: sixteen bytes a row, Apple text, I/O and ROM bytes colored, editing on; the view scrolls the 64K and the pane asks the CPU thread for a read around the rows on screen once they leave the last one (`MemoryPane::GetReadStartFor`, tested in `MemoryPaneTests.cpp`). `DebuggerViewState` holds up to four windows (the first always open; `OpenMemoryWindow`, `CloseMemoryWindow`) and `Build` reads 512 bytes with a region per byte for each into `DebuggerViewSnapshot::memoryWindows`; the snapshot also names the machine, and a change clears every window's undo (FR-036). The view payload gains `memory2`-`memory4 <hex>|close` (`CpuCommandDispatcher`), and `IDebuggerWindowHost::SetDebuggerMemoryWindow (id, address)` replaces `SetDebuggerMemoryAddress`. The window replaces its memory list with the panes (the open ones share the bottom row), adds Bytes/Words/Longs, + Memory and - Memory buttons, routes clicks, drags and the wheel to the panes, sends characters to a focused pane, treats a focused pane as a box that keeps its typing keys, and runs Ctrl+Z as that pane's undo. Tests: `MemoryWindowTests` in `DebuggerViewStateTests.cpp`, `MemoryWindowsTwoToFourOpenMoveAndClose` in `CpuCommandDispatcherTests.cpp`.
- [X] T111 [US5] Validated live (title `dbg-dev`): typing `A941` into the first memory window at `$0010` wrote `A9 41` (checked over the pipe with `d 10:11`), one `PATCH` per completed byte in the console; typing into `$C030` was refused with "$C030 is I/O, which an edit does not write; use OUT." ROM patches, words, text-column typing and undo are covered by the unit tests; Ctrl+Z could not be posted without a real keyboard state, so it was not driven live.

**Checkpoint**: Bytes, words, double words and text edit in place with undo, ROM patches take, I/O refuses, and the full suite is green.

---

## Phase 11: User Story 6 - Debug at source level (Priority: P1)

**Goal**: FR-033, FR-033a, FR-033b, FR-054 to FR-060, FR-066; SC-010, SC-011, SC-017

**Independent Test**: Assemble the speech demo with `-g`, load the debug file, set a breakpoint on `Sing`'s source line, run, and confirm the source pane marks it; step over a `JSR` with inline parameters and land on the next line; change a byte of the source, drag it on, and see the mismatch warning (quickstart Story 6).

### Hashing and the debug file

- [X] T112 [P] [US6] Write `UnitTest/DebuggerTests/Sha1Tests.cpp` (RFC 3174's vectors: `abc`, the 56-character string, one million `a`) and `NormalizeLineEndings` cases (`\r\n` and lone `\r` become `\n`; UTF-8 bytes untouched). Implement `CassoCore/Sha1.h/.cpp` (CassoCore has no Core folder) from RFC 3174 as class statics: `Sha1::Compute (std::span<const uint8_t>) -> Sha1Digest`, `Sha1::ToHex`, `Sha1::NormalizeLineEndings`.
- [X] T113 [P] [US6] Add fixtures under `UnitTest/Fixtures/Debugger/DebugFiles/`: one cc65 v2 file produced by cc65's own linker (built from source at `C:\Users\relmer\source\repos\cc65`, 2026-09-18) from a small program (with a `LICENSE` note giving its origin), and hand-written cases for the errors below. Write `UnitTest/DebuggerTests/DebugFileReaderTests.cpp` per `contracts/debug-file-format.md`: every record kind read into `DebugFile`; unknown keys and unknown record types skipped; several spans per `line` joined with `+`; `type` and `count` on macro lines; a `file` with and without `sha1`; a major version other than 2 is an error naming the version found; a `line` whose `file` or `span` id does not exist, or a `span` whose `seg` does not exist, is an error and nothing loads; the record count asserted non-zero.
- [X] T114 [US6] Implement `CassoCore/Debugger/DebugFile.h` (data-model `DebugFile`, `SourceFileRecord`, `LineRecord`, `Span`, `Segment`) and `CassoCore/Debugger/DebugFileReader.h/.cpp`. Makes T113 pass.
- [X] T115 [P] [US6] Write `UnitTest/DebuggerTests/LineTableTests.cpp` and implement `CassoCore/Debugger/LineTable.h/.cpp` (data-model `LineTable`): address to positions ordered outermost first; `(file, line)` to address ranges resolved through `seg.start + span.start`; a line with no code maps to the next line with code and says so (FR-055); an included file used twice has entries for each inclusion; the SC-010 sweep over the T113 fixture resolves every line that produced code both ways.
- [X] T116 [US6] Record the source position stack in `CassoCore/Assembler.h/.cpp`: per emitted byte range, `{fileId, line, macroDepth, invocationLine}`; include files get ids as they open; a macro expansion pushes the body position under the invocation. Tests in `UnitTest/AssemblerDebugRecordsTests.cpp` for as65 and Merlin dialects with an include and a two-level macro.
- [X] T117 [P] [US6] Write `UnitTest/DebuggerTests/DebugFileWriterTests.cpp` per `contracts/debug-file-format.md`: the record order; `file` with `size`, `mtime` and `sha1`; `span.start` segment-relative; the invocation line and each body line as their own `line` records over the same spans with `type=2` and `count` (FR-033a); `scope` records for local and macro-generated labels; one `mod` and `seg` per Merlin `SAV` output; the output parses back through `DebugFileReader` to an equal `DebugFile`.
- [X] T118 [US6] Implement `CassoCore/Debugger/DebugFileWriter.h/.cpp` and switch `ArtifactWriter::WriteDebugInfo` (used by `CassoEmuCore/Cli/As65Mode.cpp` and `MerlinMode.cpp`) to it, removing the `NAME=$ADDR` writer; keep `-g` and its per-output rule. Update `PerOutputArtifactTests.cpp`, `MerlinCommandLineTests.cpp` and `docs/Assembler.md`. Makes T117 pass.
- [X] T119 [US6] FR-066: add `UnitTest/Fixtures/Debugger/Sources/include-macro.a65`, `include-macro.inc` and the Merlin form `INCLUDE.MACRO.S` (with its `PUT` file `T.INCLUDE.MACRO`) with a two-level macro; write `UnitTest/DebuggerTests/DebugFileRoundTripTests.cpp` that assembles each with its assembler, writes the debug file, reads it back, and confirms every emitted address resolves to its invocation line and its body line with the right depth. SC-017 (cc65's own reader accepts the file) is a quickstart step run by hand, since cc65 is not a dependency; record the result in the commit.
- [X] T120 [US6] `SymbolFileReader::Detect` (`CassoCore/Debugger/SymbolFileReader.cpp`) recognizes a first line of `version` with `major=` as cc65 and `SYMBOL TABLE` as a Merlin listing; `SYM LOAD` and `SymbolHandlers` load symbols and lines from a `DebugFile` into the session, which now holds the `DebugFile` and `LineTable`; the earlier `NAME=$ADDR` file still loads. Cases in `SymbolFileReaderTests.cpp` and `SymbolHandlersTests.cpp`, one of each format.

### Merlin listings

- [X] T121 [US6] Capture the `PI.ADD.S` listing from Merlin under emulation (R-022) by the procedure in `UnitTest/MerlinCorpus/README.md`, check it in as `UnitTest/Fixtures/Debugger/Merlin/PI.ADD.LST` with a `LICENSE` note, and record in research R-022 how `PUT` and `USE` lines appear and whether line numbers restart. **As built (2026-09-19):** captured by printing rather than screen-scraping. `PRTR 1` sends Merlin's listing to the slot-1 printer card, which now keeps a text copy of what a guest prints when `CASSO_PRINTER_TEXT` names a file, so the whole listing, the end-of-assembly line and both symbol tables arrive as text in one run. Checked against an independent capture taken at the character-output routine: of the 232 listing lines in common, 228 match exactly and four differ only in trailing spaces, which the 80-column screen cuts. `UnitTest/Fixtures/Debugger/Merlin/PI.ADD.LST` with a LICENSE note; read by `MerlinFixtureTests` in `DebugFileReaderTests.cpp`.
- [X] T122 [US6] Implement the Merlin 8/16 listing form in `DebugFileReader` (FR-033b): every listing line with an address becomes a `line` record for a single `file` that is the listing itself, and the trailing symbol table becomes `sym` records; a `>` line (a `PUT` file's line, whose number restarts at 1, per the R-022 capture) is a line of the listing like any other, and no `PUT` file is opened. Tests in `DebugFileReaderTests.cpp` against the fixture, asserting a non-zero line count. The same fixture's trailing symbol table verifies the layout `CassoCli merlin -l` appends (FR-033, R-009): add a case to `UnitTest/MerlinListingSymbolTableTests.cpp` comparing against the fixture instead of the inline sample.

### Finding sources

- [X] T123 [P] [US6] Write `UnitTest/DebuggerTests/SourceServiceTests.cpp` and implement `CassoEmuCore/Debugger/Source/SourceService.h/.cpp` and `SourcePathList.h/.cpp` (R-032) over `IFileSystem` and an in-memory `GlobalUserPrefs`: resolution in FR-058's order (relative to the debug file, the per-program list keyed by the debug file's own SHA-1, the global list, a dragged file); name and size filter before any hash; a hash match resolves silently; a size match with a different hash opens with `Mismatch`; two candidates of the same name and size take the first whose hash matches, else ask; a dragged file that matches no record opens as plain text; a found folder is added most-recent-first to both lists and survives a save and load of the preferences (FR-060).

### Stepping

- [X] T124 [P] [US6] Write `UnitTest/DebuggerTests/SourceStepTests.cpp` against `TestMachine` with the T119 fixture program: step over by the stack-pointer rule (R-033) on a `JSR` followed by inline parameters, on a recursive call, and on a routine that discards its return address (SC-011); step out; with granularity `source`, the same `T`, `P` and `RTS` commands stop at the first instruction of a different line, repeat until the line changes, and stop on the line after the call; an IRQ during a source step into lands in the handler; with granularity `instruction` they behave as before.
- [X] T125 [US6] Change `CassoEmuCore/Debugger/RunStopHook.h/.cpp` to the stack-pointer rule for step over and step out, so AppleWin `P` and `RTS` and Monitor `S` forms use it too; add the source-step modes; add `CassoEmuCore/Debugger/Handlers/SourceHandlers.h/.cpp` with the engine commands `SRC` (the current file and line), `SRC ON|OFF` (the session's step granularity: source line or instruction) and `BP <file>:<line>` in `AppleWinParser`; the existing step commands in every mode (`T`, `P`, `RTS`; Monitor `S`; GSSquared Space, `o`, `r`) and the window's step actions step by source line while granularity is `source`, so no dialect gains a step name; the stop report carries the source position. Record `SRC` in `contracts/command-modes.md`. Makes T124 pass, and the `P` cases in `ExecutionHandlersTests.cpp` are updated to the new rule.

### The source pane

- [X] T126 [US6] Implement `CassoEmuCore/Ui/Debugger/Panes/SourcePane.h/.cpp` over `DxuiTextView` (line number and text as its two cells; the row warning mark for a line that moved a breakpoint): the file and line for the PC with the line marked; selection synchronized with `DisassemblyPane` both ways (FR-054); an indicator when the position is inside a macro, with the body line shown on request (FR-057); double-click toggles a line breakpoint, moving to the next line with code and saying so (FR-055); focus entering the source pane sets the session's step granularity to `source` and focus entering the disassembly pane sets it to `instruction`, so the toolbar, the key schemes and typed step commands follow the active view; a `DxuiActionBanner` for the mismatch warning (FR-059); a dropped file that matches no record shows as plain text with no line mapping and a banner saying so; a file dropped on the window (`WM_DROPFILES` in `DxuiWindow`, forwarded to the host) goes to `SourceService`. Tests in `DebuggerViewStateTests.cpp`.
- [ ] T127 [US6] Validate quickstart Story 6 steps 1-8 by hand, including SC-017 with cc65's `dbginfo` reader, and record the results in the commit. **Scripted 2026-09-23 at d991edff; all pass except the drop, which is still for the owner:** (1) `as65 -g` on `include-macro.a65` wrote `file`, `line`, `span`, `seg`, `sym` records with a `sha1` on both files; `dbgsh` loaded it with only the two `sha1` warnings. (2) The source pane opened on the PC's line. (3) Stopped inside the nested macro, the pane showed the invocation line with a "Show body" banner giving line 5; `T` from `start` went to line 5, then 10, then 15. (4) `BP include-macro.a65:17` set $030A and `BP include-macro.inc:2` set $030E; `G` stopped on the include. (5) `SRC ON`, `P` over the inline-parameter `JSR` stopped at `inlback` (line 33) and over the recursive `JSR rec` at `recurx` (line 21). (6) With the sources gone, the pane said to drop the file on the window; a load from another folder found the source through the folder remembered from an earlier load. The drop itself cannot be posted from another process. (7) An edited source opened with the "not the file that was assembled" warning. (8) `PI.ADD.LST` loaded 60 symbols and 104 lines and showed as the source view with the PC on its line.

**Checkpoint**: Both assemblers write cc65 v2, the debugger reads it from any assembler and from a Merlin listing, the source pane follows the machine, and the full suite is green.

---

## Phase 12: User Story 7 - Arrange the debugger like Visual Studio (Priority: P2)

**Goal**: FR-038 to FR-044; SC-014

**Independent Test**: Drag the memory pane to the right edge, tab the trace under it, float the source pane onto a second monitor, reopen the debugger and see the layout restored; remove the monitor and see the pane on the primary (quickstart Story 7).

- [X] T128 [US7] Merge `origin/033-casso-explorer` again, or master if 033 has reached it, to pick up any `DxuiSplitter` and `DxuiPanel` changes since T094; run `scripts/CheckStyle.ps1 -Mode Tree` after the merge. **As built (2026-09-18):** neither `origin/master` nor `origin/033-casso-explorer` had commits this branch lacked, so there was nothing to merge.
- [X] T129 [P] [US7] Write `UnitTest/Dxui/DxuiPaneLayoutTests.cpp` and implement `Dxui/Core/DxuiPaneLayout.h/.cpp` (R-028, data-model `Layout`; not `DxuiDockLayout`, the emulator window's edge-band layout): `SplitNode`, `TabNode`, `PaneLeaf`, floating and auto-hidden lists; operations dock-to-side, tab-with, float, dock-back, auto-hide, restore, move within group by arrow, with a drop onto a pane's own position a no-op; split ratios clamped to each pane's minimum; a text round trip with a version (the library's own nested-list form, since Dxui has no JSON; preferences store the text as one string, T135); an unknown pane id dropped on load; an unreadable document falls back to the default tree; a floating pane whose `monitorKey` is absent from the given topology opens on the primary at its saved size (SC-014); a device pane absent from the current machine is closed on restore and keeps its place (FR-044).
- [X] T130 [P] [US7] Write `UnitTest/Dxui/DxuiDockDropZonesTests.cpp` and implement `Dxui/Core/DxuiDockDropZones.h/.cpp`: given the tree's laid-out rects, the drop-zone rects for each side of each group, the tab zone, and the window edges; hit-testing a point to a zone; the zone's meaning as a `DxuiPaneLayout` operation.
- [X] T131 [P] [US7] Write `UnitTest/Dxui/DxuiTabGroupTests.cpp` and implement `Dxui/Widgets/DxuiTabGroup.h/.cpp`: tabs over a set of children, the active child laid out in the body, tab hit-testing, Ctrl+Tab switching, and a per-tab indicator flag.
- [X] T132 [US7] Add `DetachChild (IDxuiControl *) -> std::unique_ptr<IDxuiControl>` to `Dxui/Core/DxuiPanel.h/.cpp` (R-023) with a case in `UnitTest/Dxui/DxuiPanelTests.cpp` that a detached control re-attached to another panel keeps its state and is laid out by the new panel.
- [X] T133 [US7] First split `CassoEmuCore/Ui/Debugger/DebuggerWindow.cpp` into pane classes under `CassoEmuCore/Ui/Debugger/Panes/` (moved here from T101): `DebuggerPane.h` (base: a `DxuiPanel` that draws from the snapshot and sends commands as text through the host), `DisassemblyPane`, `RegistersPane`, `StackPane`, `WatchesPane`, `BreakpointsPane`, `ConsolePane`, `MemoryPane`, each `.h/.cpp`, each owning its list and keeping `MakeDense`'s metrics, so the dock site can move each as one control. Then implement `Dxui/Widgets/DxuiDockSite.h/.cpp`: the control that maps a `DxuiPaneLayout` onto `DxuiSplitter` and `DxuiTabGroup` controls, applies each operation by moving pane controls with `DetachChild`, draws the drop-zone overlay during a drag from `DxuiDockDropZones`, hosts auto-hidden panes as edge tabs, and offers the Dock To menu (each side, each pane to tab with, Float, Auto Hide) and arrow-key moves (FR-042). Tests in `UnitTest/Dxui/DxuiDockSiteTests.cpp` with `MockDxuiControl` panes assert the control tree after each operation. **As built:** a list is already one control, so only the two panes made of several controls became containers, as `Panes/DebuggerPaneFrame` (source: banner over text; console: list over command box); the per-pane class split was not needed for docking. The default arrangement is `Ui/Debugger/DebuggerLayout` (tests in `DebuggerLayoutTests.cpp`). Edge tabs for auto-hidden panes move to T134 with the rest of auto-hide. Drop-zone squares are not yet DPI-scaled (T136).
- [X] T134 [US7] Implement `Dxui/Window/DxuiDockedWindow.h/.cpp`, a `DxuiWindow` whose content is one `DxuiDockSite`: the main window holds the whole layout and a floating pane is another `DxuiDockedWindow` holding one pane; dragging a pane past the window edge floats it and dragging a floating window over a drop zone docks it (FR-040); `WM_DPICHANGED` through `DxuiHwndSource` rescales (FR-043); auto-hidden edge tabs slide out on hover or click and hide when focus leaves, with the indicator when content changes (FR-041). Tests in `UnitTest/Dxui/DxuiDockedWindowTests.cpp` drive the float and dock operations without an `HWND`. **As built:** the main window stays `DebuggerWindow` with a `DxuiDockSite`; each floating pane is a `DxuiDockedWindow` its controls move into with `DetachChild`/`AttachChild`, and the window's hand routing serves every window, filtered by pane (`IsRoutable`). Closing a float docks it back. Auto-hidden panes are edge tabs; a hover or press slides the pane out and the docked panes make room beside it (the text renderer draws after all fills, so a pane cannot be painted over another). The float and dock operations are tested through `DxuiPaneLayout`, `DxuiDockSite` and `DebuggerLayout`; the window itself needs an `HWND`, so `DxuiDockedWindowTests.cpp` was not written. Posted input verified float, restore on its monitor after restart, routing in a float, close-to-dock, and slide out/in; the caption drag back onto a drop zone runs in the OS move loop and is left to T136.
- [X] T135 [US7] Persist the layout: a versioned `debugger.layout` subtree in `CassoEmuCore/Config/GlobalUserPrefs.h/.cpp` through `UserConfigStore`, with monitors keyed the way `WindowPlacementProfile` keys them; saved when the debugger closes and applied when it opens; `DebuggerWindow` becomes a `DxuiDockedWindow` and composes its panes through its dock site. Round-trip case in `GlobalUserPrefsTests.cpp`. **As built:** one string, `global.debuggerLayout`, holding the layout text (which carries its own version); floating panes are keyed by the monitor's device name. `DebuggerLayout::Restore` drops unknown panes and adds missing ones.
- [ ] T136 [US7] Validate quickstart Story 7 steps 1-4 on the two-monitor machine, including the DPI boundary, and record the results in the commit.

**Checkpoint**: Panes split, tab, float, auto-hide and come back where they were, from mouse and keyboard, and the full suite is green.

---

## Phase 13: User Story 8 - Look back at what ran (Priority: P2)

**Goal**: FR-045 to FR-048; SC-013; FR-064 kept

**Independent Test**: Turn tracing on, run the speech demo to a watchpoint on the speech chip's data register, and see the write as the last trace entry with `SPHON`; scroll back through increasing cycle counts (quickstart Story 8).

- [X] T137 [P] [US8] Write `UnitTest/DebuggerTests/TraceTests.cpp`: the extended `TraceEntry` (data-model) carries `cycles` and the last access `{address, data, isWrite}` per instruction; with capacity 100,000 the ring keeps the newest 100,000 (SC-013); while on, every page is in the bus's watched set and the access is recorded from the watch path; while off, `MemoryBus::GetWatchedPageCount()` is the watchpoints' count alone and `Cpu`'s trace gate is false; steps append; a machine switch clears it; `HISTORY` renders a window of entries with symbols for `pc` and the access address. **As built:** the access recorded is the instruction's last DATA access: reads of its own bytes (pc to pc+2), the next opcode fetch (a read at PC before the next entry exists) and an interrupt's stack writes and vector reads are left out, so an immediate or implied instruction has none.
- [X] T138 [US8] Extend `TraceEntry` in `CassoEmuCore/Core/Cpu.h/.cpp` and fill the access from the bus; add `SetTraceAllPages (bool)` to `CassoEmuCore/Core/MemoryBus.h/.cpp` (R-025: publish every page to the watched path while on, restore the mask when off) and a trace sink; implement `CassoEmuCore/Debugger/TraceController.h/.cpp` (`On`, `Off`, `GetWindow (first, count)`, `Save`), reachable through `IDebugTarget` and `MachineDebugTarget`. Makes T137 pass; every existing `MemoryBusWatchMaskTests` case must still pass. **As built:** the bus keeps the watchpoints' mask and a separate watched-path mask (the watch mask, or every page while tracing) that the access paths test, so the fast path is unchanged; the watch sink sees only its own pages and the trace sink every access. `IDebugTarget` gets `SetTraceOn`, `IsTraceOn`, `ClearTrace`, `GetTraceSize` and `GetTraceWindow` rather than the controller itself, so the mock target serves the handler tests. `Save` is in `TraceHandlers`, since the text is the formatter's. `HISTORY OFF` keeps the entries; a machine switch discards them (and, in a Debug build, the CPU's own 256-entry illegal-opcode look-back, once the trace has been used). `PeekForTrace` now reads operand bytes through the bus's shadow table, which a watched page keeps.
- [X] T139 [P] [US8] `HISTORY ON|OFF|SAVE <file>|<first> [count]` in `AppleWinParser`, `CassoEmuCore/Debugger/Handlers/TraceHandlers.h/.cpp`, the three formatters and a `trace` reply kind in `ReplyJson`; `HISTORY SAVE` writes every retained entry through `IFileSystem`. Tests in `AppleWinParserTests.cpp`, `FormatterTests`, `ReplyJsonTests.cpp` and `TraceHandlersTests.cpp` with the mock file system. Update `contracts/command-modes.md` and `contracts/debug-channel-protocol.md`. **As built:** `first` and `count` are decimal (entry numbers pass 16 bits); a bare `HISTORY` shows the newest 20. The Monitor formatter has no layout of its own and falls back to AppleWin's, as for other engine commands. Parser tests are `Engine_History`; formatter tests are in `AppleWinFormatterTests`.
- [X] T140 [US8] Implement `CassoEmuCore/Ui/Debugger/Panes/TracePane.h/.cpp`: a virtualized list that requests only the visible window of entries from the snapshot, ends at the most recent, scrolls to any entry (SC-013), and shows symbols; a toolbar toggle for the trace. Tests in `DebuggerViewStateTests.cpp` assert the window requested for a scroll position. **As built:** `TracePane` drives a virtual `DxuiListView` (pane id `trace`, a tab of the console by default and in a restored layout that lacks it) whose row count is the trace size; the snapshot carries a 128-entry window read by `HISTORY first 128` around the pane's position (`DebuggerViewState::SetTraceTop`, empty to follow the newest), and the pane asks for a new window through `IDebuggerWindowHost::SetDebuggerTraceTop` only when the rows on screen leave the last one. The toolbar button reads `Trace: On`/`Trace: Off` and sends `HISTORY ON`/`HISTORY OFF`. Not yet driven by hand; T141 covers it.
- [X] T141 [US8] Validate quickstart Story 8 steps 1-3 by hand, and measure FR-064/SC-008 with the trace off by the pinned procedure against the baseline commit before `9f12ea90`; record both in the commit. **Scripted 2026-09-23 at d991edff:** (1) `HISTORY ON`, one second of //e, then `HISTORY`: 100,000 entries retained, each with cycle count, PC, instruction, registers, access and symbol, but no opcode bytes (T197). (2) `HISTORY SAVE` wrote all 100,000. (3) Measured as T165 was, against master `325bfbb5` rather than the commit before `9f12ea90`, so the numbers line up with T165's (Release x64, 50M //e cycles, pinned `/affinity 10`, 12 alternating rounds, medians): master 278.4 ms; head, debugger closed, 259.9 ms (SC-008: 6.6% faster); head, debugger attached with call recording and the trace off, 263.3 ms (SC-009: +1.3% over closed). After T197, in the window on a ][+: the trace pane showed each entry's bytes in a column of their own, as many as the instruction has, and dragging its scrollbar to the top reached entry 0 of 100,000. Home in the pane did not move it: the key went to the command box.

**Checkpoint**: 100,000 instructions are retained with their accesses, the pane scrolls through them, and the idle path is unchanged.

---

## Phase 14: User Story 9 - Inspect a device (Priority: P2)

**Goal**: FR-049 to FR-053; SC-015

**Independent Test**: Boot a disk with the Disk II panel open and watch the quarter track and phases change; stop and see the panel freeze; switch to a machine without a Mockingboard and see that panel gone (quickstart Story 9).

- [X] T142 [P] [US9] Write `UnitTest/DebuggerTests/DiagnosticsProviderTests.cpp` for `IDiagnosticsProvider` and data-model `DiagnosticsSnapshot`: each provider fills groups of `{label, value, bits}` rows; the Disk II provider's phases, quarter track, motor and spin-up and read state change during a seek on `TestMachine`; the //e MMU provider lists every switch with its bit decode and a `MemoryMap` payload of 256 `{readSource, writeSource}` pages that changes on `$C008`/`$C009`; the video provider on `AppleSoftSwitchBank` and `Apple2eSoftSwitchBank` (text, mixed, page 2, hi-res, 80-column, double hi-res), the `AppleKeyboard` provider, the Mockingboard provider (per 6522 ports, timers, IFR, IER; per AY register set) with a `Meters` payload of each AY channel's level and each 6522 timer's remaining count (FR-052), the printer provider, and the clock provider on `MachineHost` (cycle counters and speed mode); `MachineHost::GetDiagnosticsProviders` lists only the current machine's (FR-053), and the ][+ has no MMU or Mockingboard entry; the snapshot is built once per frame and once on stop (SC-015). **As built:** the ][+ case is tested with its disk-only slots, since the shipped ][+ configuration has a Mockingboard in slot 4 (quickstart Story 9 step 4 holds only with that slot emptied). The build cadence is checked through `DebuggerViewState::IsBuildDue` and an open panel appearing in every build (`DebuggerViewStateTests`), not a separate timing test.
- [X] T143 [US9] Implement `CassoEmuCore/Debugger/IDiagnosticsProvider.h` and `DiagnosticsSnapshot.h` (R-034), the providers on `Disk2Controller`, `Apple2eMmu`, `AppleSoftSwitchBank`/`Apple2eSoftSwitchBank` (video mode), `AppleKeyboard`, `Via6522` and `Ay8910` (grouped by Mockingboard), the printer, and `MachineHost` (the clock), `MachineHost::GetDiagnosticsProviders`, and the per-open-panel snapshots in `DebugViewSnapshot`. Makes T142 pass. **As built:** `Via6522` and `Ay8910` append their groups and meter levels (`AppendDiagnostics`, `AppendTimerLevels`, `AppendChannelLevels`) and `MockingboardCard` is the provider. The printer provider is on `PrinterCard` and shows the status byte and the byte ring; the head and carriage live in the UI-thread print engine, which the CPU thread does not read, so they are not published. The keyboard provider shows the latch, strobe, key down and auto-repeat; the //e's Apple keys are game-port buttons, not keyboard state, and are not in it. The clock shows cycles, the configured clock rate, the speed mode (set on `MachineHost` by the shell before each build) and the video beam. `IDebugTarget::GetDiagnosticsProviders` (default empty) carries the list to the session; the snapshot is `DebuggerViewSnapshot::panels` plus `diagnostics`, one per open panel, and `DebuggerViewState` closes a panel whose device has left.
- [X] T144 [P] [US9] Implement `CassoEmuCore/Ui/Debugger/Panes/DiagnosticsPane.h/.cpp`: groups, rows and bit decodes rendered from the snapshot with no device-specific code; the panel menu lists the current providers; a panel closes when its device leaves (machine switch or slot emptied). Tests in `DebuggerViewStateTests.cpp` render a synthetic snapshot. **As built:** the panel menu is a Panels toolbar button whose popup lists the providers, checked when open, and sends `PANEL`/`PANEL CLOSE` lines. Pane ids are `diag-<id>` from a fixed list in `DebuggerLayout::GetDiagnosticsPanels` (clock, keyboard, video, mmu, disk, mockingboard, printer) so a saved layout keeps their places; a provider id outside that list gets rows in the snapshot but no pane until the list gains it. The panels default to tabs of the stack.
- [X] T145 [P] [US9] Implement the three visuals as controls under `CassoEmuCore/Ui/Debugger/Panes/`: `MemoryMapBar.h/.cpp` (256 pages colored by source), `DiskHeadView.h/.cpp` (quarter-track head position, phases, motor), `MeterBar.h/.cpp` (named levels 0..1). Tests in `UnitTest/DebuggerTests/DiagnosticsVisualsTests.cpp` through `MockDxuiPainter`. **As built:** the memory map draws a read strip over a write strip with a key of the sources in use; the disk head view draws a track ruler, the head, four phase lamps and a motor lamp.
- [X] T146 [US9] Add the engine command `PANEL LIST|<name>|CLOSE <name>` to `AppleWinParser` and `ConfigHandlers` so panels open from any way in; it reports not available in batch. Tests in `AppleWinParserTests.cpp` and `ConfigHandlersTests.cpp`; contract in `contracts/command-modes.md`. **As built:** `PANEL` alone also lists. The window runs it through `DebuggerViewState::ExecuteWindowLine`, parsed by `AppleWinParser` so both agree on its syntax; `ConfigHandlers` answers it outside the window.
- [ ] T147 [US9] Validate quickstart Story 9 steps 1-4 by hand and SC-015 on a capture sequence; record in the commit. **Partly checked 2026-09-19:** `PANEL mockingboard` from the command box opened the panel, which showed the AY channels and both 6522 timers while the machine ran; the rest of Story 9 is still a hand check. **Scripted 2026-09-23 at d991edff:** (1) `PANEL disk`, then a boot from $C600: six captures during the recalibrate seek showed the quarter track at 52, 30, 10, then 0, with the phase lamps and head marker following; each capture differed, but captures 150 ms apart cannot show the once-a-frame rate of SC-015. (2) `PANEL mmu` on the enhanced //e listed every switch with its bit decode; `$C009` turned ALTZP on, the map bar showed aux at the bottom and the MMU byte read $10, and `$C008` put both back. (3) On the speech demo the 6522 timers and port values changed between captures; the AY meters stayed idle, because the demo's speech does not go through the AY. (4) On the ][+ the panel list omits the MMU; it lists the Mockingboard because this ][+ has one in slot 4, as FR-053 requires. Open for SC-015's frame rate and the AY meters on a demo that plays through the AY.

**Checkpoint**: Seven panels and three visuals update every frame and freeze on stop, and a new device gets a panel by publishing rows.

---

## Phase 15: User Story 10 - Stop on a condition (Priority: P3)

**Goal**: FR-015a, FR-061, FR-062

**Independent Test**: `BP 0300 IF A == 41` on a counting loop stops with A=$41; a value breakpoint on $0400 becoming $C1 stops on the tenth pass with that hit count (quickstart Story 10).

- [X] T148 [P] [US10] Write `UnitTest/DebuggerTests/ExpressionBreakpointTests.cpp`: `IF <expr>` on `BP`, `BPX`, `BPM`, `BPMR` and `BPMW`; the expression is evaluated only when the address or access hits (a counting evaluator context); false leaves the hit count unchanged and the machine running; true stops and the report carries the expression and its value; an expression that cannot be evaluated when set (unknown symbol) is an error and creates nothing; an expression that reads an I/O address is refused when set; on a watch hit the pseudo-symbols `ACCESS` and `VALUE` name the accessed address and the value; `BPMV <addr> <value>` (`MemoryValue` kind) stops when a write leaves the address holding the value and not on any other write; `BPR A=0`, `BPR A = 0` and `BPR A 0` set the same breakpoint (FR-015a), and `BPR A=` is still an error.
- [X] T149 [US10] Implement: `condition` and `value` on `Breakpoint` and the `MemoryValue` kind in `CassoEmuCore/Debugger/BreakpointTable.h/.cpp`; evaluation at hit in `DebugSession` and on the watch path in `WatchpointTable`; `IF`, `BPMV` and the spacing forms in `CassoCore/Debugger/AppleWinParser.cpp`; the stop report in the formatters and `ReplyJson`. Record `IF`, `BPMV`, `ACCESS` and `VALUE` in `contracts/command-modes.md`. Makes T148 pass.
- [X] T150 [US10] Add batch scripts and goldens for quickstart Story 10 steps 1-3 under `UnitTest/Fixtures/Debugger/Scripts/` with cases in `DebugModeTests.cpp`.

**Checkpoint**: Conditional and value breakpoints stop where they should and nowhere else.

---

## Phase 16: User Story 11 - Find the expensive code (Priority: P3)

**Goal**: FR-063

**Independent Test**: Profile a loop of `LDA $10FF,X` with X crossing the page half the time and see the page-crossing cycles attributed apart from the base cycles (quickstart Story 11).

- [X] T151 [P] [US11] Add the penalty byte beside `GetLastInstructionCycles` in `CassoEmuCore/Core/Cpu.h/.cpp` (R-026): page crossing on an indexed read, branch taken, branch crossed a page. Cases in `UnitTest/EmuTests/EmuCpuTests.cpp` and `Cpu65C02Tests.cpp` for each kind on both CPUs, including the 65C02 modes where crossing costs a cycle.
- [X] T152 [P] [US11] Write `UnitTest/DebuggerTests/ProfileTableTests.cpp` and implement `CassoEmuCore/Debugger/ProfileTable.h/.cpp` (data-model `Profile`): per-opcode `{count, cycles}`, the three penalty buckets, per-address cycles, the total; `Reset`; counting only while on, through the per-instruction hook, and the hook absent when off (a recording target sees no hook).
- [X] T153 [US11] `PROFILE ON|OFF|RESET|LIST [ADDR]|SAVE <file>` in `AppleWinParser` and `CassoEmuCore/Debugger/Handlers/ExecutionHandlers.h/.cpp`: `LIST` grouped by mnemonic and addressing mode with count, cycles and share, and the penalty rows; `LIST ADDR` the hottest addresses with symbols; `SAVE` the same rows through `IFileSystem`; a `profile` reply kind in `ReplyJson`. Tests in `ExecutionHandlersTests.cpp`, `FormatterTests` and `ReplyJsonTests.cpp`; contracts updated.
- [X] T154 [US11] Add the `LDA $10FF,X` script and goldens for quickstart Story 11 under `UnitTest/Fixtures/Debugger/Scripts/` with cases in `DebugModeTests.cpp`.

**Checkpoint**: The profile names the cycles by opcode, mode, address and penalty kind.

---

## Phase 17: User Story 12 - Use GSSquared's syntax (Priority: P3)

**Goal**: FR-011, FR-013, FR-022a, FR-022b; SC-016

**Independent Test**: In batch with GSSquared mode, run each command in its list against a fixture machine and compare the engine effect with the same command in AppleWin mode; compare replies to the captured fixtures (quickstart Story 12).

- [X] T155 [US12] Capture GSSquared's replies (R-027): build GSSquared from source, run each command in `contracts/gssquared-mode.md` against the program in `Scripts/stop.txt` on the same machine type, and check the replies in under `UnitTest/Fixtures/Debugger/GSSquared/` with a `LICENSE` note. If GSSquared cannot be built, use its documented examples, record the deviation in research R-027, and note which fixtures are documentation-derived. **As built:** GSSquared was not built (it needs SDL3 and more); the fixtures under `UnitTest/Fixtures/Debugger/GSSquared/` were written from the format strings in its `src/debugger/Monitor.cpp` and `trace.cpp`, read at commit 4a14e98, one file per reply kind, with a `LICENSE` note saying so and research R-027 updated. The layouts differ from a capture in three recorded ways: `list` prints 20 instructions (GSSquared 30), no trailing spaces, and a range watch is one watch per address.
- [X] T156 [P] [US12] Write `UnitTest/DebuggerTests/GSSquaredParserTests.cpp` for every row of the contract's command table: `first.last` ranges, deposit, `set`, `move`, `l` with and without an address, `bp`/`bpd`/`bpi`/`nobp`, `watch`/`nowatch`, `load`/`save`, `sload`/`slookup`/`sclear`, `o`, `r`, `s`, an empty line, `g`, `debug`/`nodebug`; case-insensitive; `m`, `x`, `map` and `video` produce `NotAvailable` with their reason; `00/300` is $0300 and `e1/300` is refused naming the bank (FR-022b); engine commands are bare names, as the contract's R-037 note says, and `/` is never a marker here; `300l` lists at $0300 but a command word ending in `l` is not split; a malformed line produces an error and no command. **As built:** `nobp N` carries the number as a decimal id and a hex address, and the session decides which (an id some entry has, else an execution breakpoint's address). `debug`/`nodebug` are not available with a reason (no `PANEL` yet; see T161). `verify` is not available (GSSquared parses it and does nothing). `bpi` takes only $C000-$C0FF, as GSSquared does. `bp`, `bpd` and `bpi` take a trailing `IF`.
- [X] T157 [US12] Implement `CassoCore/Debugger/GSSquaredParser.h/.cpp` producing `DebugCommand`, add `CommandMode::GSSquared`, `MODE GSSQUARED` and `/mode gssquared`, and route the mode in `DebugSession`. Makes T156 pass. **As built:** each GSSquared line is rewritten into the AppleWin line with the same effect and parsed by `AppleWinParser`, so the two modes share every argument rule; `watch first.last` yields one command per address. Mode names live in `CassoCore/Debugger/CommandModeNames` (MODE, OUTPUT, `--mode`, `--output`, the channel's `mode` and JSON). `Reply` gained `verb`, so a format can tell a deposit from a dump.
- [X] T158 [P] [US12] Write `UnitTest/DebuggerTests/OutputFormatTests.cpp` (FR-013, data-model `OutputFormat`): `MODE x` sets both mode and output format; `OUTPUT x` sets the format alone; the setting is reachable from batch (`--output` in `contracts/cli-debug.md` and `DebugOptions`), the channel and the window; every reply, including stops, is rendered by the current format. Implement `OUTPUT` in `ConfigHandlers`, the field in `DebugSession`, and the rendering switch in `DebugBatchSink` and the channel's reply path. **As built:** the test file is `DebugOutputFormatTests.cpp`, since `UnitTest/OutputFormatTests.cpp` already exists for the assembler's output formats. `OUTPUT` is in `ConfigHandlers`; `DebugSession::FormatReply` renders in the output format, except that a channel line run in a mode other than the session's renders in that mode's own format. Monitor's multi-command merge still renders each part with the Monitor formatter.
- [X] T159 [P] [US12] Write `UnitTest/DebuggerTests/GSSquaredFormatterTests.cpp` comparing each reply kind against the T155 fixtures line for line, and implement `CassoEmuCore/Debugger/GSSquaredFormatter.h/.cpp`; a reply kind GSSquared has no form for keeps the AppleWin text, as the Monitor formatter does. **As built:** GSSquared has no stop line of its own, so stops, registers, errors and engine replies keep the AppleWin text.
- [X] T160 [US12] Write `UnitTest/DebuggerTests/GSSquaredCommandSweepTests.cpp` (SC-016): for every command in the contract's table, the engine effect (tables, memory, registers, run request) equals the AppleWin equivalent's, and every `DebugVerb` reachable from GSSquared mode is in the table. **As built:** each row runs in two fresh sessions (GSSquared and AppleWin) over the mock target and compares memory, registers, breakpoint, watchpoint and watch tables, run requests, the saved file, the symbol count and the step filter.
- [X] T161 [US12] In the window's `ConsolePane`, Space and F10 step and Return resumes when the mode is GSSquared and the input line is empty (the contract's batch note); `debug "name"` and `nodebug` map onto `PANEL`. Tests in `DebuggerViewStateTests.cpp`. Record the mode in `contracts/command-modes.md` and `docs/Debugger.md`. **As built:** `DebuggerViewState::GetConsoleKeyAction` decides the keys and `RouteBoxKey` applies it before the scheme. `debug`/`nodebug` do not map onto `PANEL`, which T146 has not built; they report not available until it exists. The window's controls send AppleWin lines, so `DebuggerViewState::GetModeLine` rewrites them into GSSquared words (`s`, `o`, `r`, `g`, `bp`, `nobp`, `addr:`) in GSSquared mode; run to cursor has no GSSquared form and stays an AppleWin line, which GSSquared mode does not read.
- [X] T162 [US12] Add a GSSquared-mode script and goldens for quickstart Story 12 steps 1-5 under `UnitTest/Fixtures/Debugger/Scripts/` with cases in `DebugModeTests.cpp`. **As built:** `Scripts/gssquared.txt` with `expected/gssquared.txt` and `.jsonl`, plus a case for `--mode gssquared` and `--output gssquared`.
- [X] T168 [US12] FR-014 across every mode: write `UnitTest/DebuggerTests/EngineMarkerTests.cpp` asserting that every engine command (`MODE`, `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH`, `SRC`, `PANEL`, `OUTPUT`, `HISTORY`, `PROFILE`, and later `CALLS` and `SKIP`) is reachable as a bare name in AppleWin and GSSquared modes, through `/` in Monitor mode and through `!` in WinDbg mode, and that a command added to the table needs no parser change; implement one engine-command table (the `Engine` family in `CassoCore/Debugger/AppleWinCommandTable`) that each parser hands its marker-stripped line to. Record the marker table in `docs/Debugger.md` (the contract already has it). **As built:** the Engine family is the table; `PATCH` and `PROFILE` moved into it (their arguments still parse as before) and `OUTPUT` joined it. `PANEL`, `HISTORY` and `CALLS` do not exist yet and join by being added to the family, which `EngineMarkerTests` walks. WinDbg's `!` row waits for WinDbg mode: its parser strips `!` and hands the rest to `AppleWinParser`, as `GSSquaredParser` does for a bare engine name, and adds its row to the marker tests.

**Checkpoint**: GSSquared mode is functional, and the engine commands are reachable the same way in every mode.

---

## Phase 18: User Story 13 - See how the program got here (Priority: P2)

**Goal**: FR-067 to FR-069; SC-018; FR-064 kept

**Independent Test**: On the call-stack fixture, stop three calls deep and see the chain innermost first labeled recorded; then provoke each FR-069 break and see it reported at its instruction (quickstart Story 13).

- [X] T169 [P] [US13] Add the fixture source `UnitTest/Fixtures/Debugger/Sources/callstack.a65` (assembled by the tests with the as65 assembler, as `SourceStepTests.cpp` assembles its program): a three-deep call chain, a recursive routine, an inline-parameter routine (`PLA`/`PHA` past two bytes), a routine that pulls its return address and jumps away, a `TXS` reload, a return address rewritten in place, a loop that wraps the stack pointer past $0100, and an IRQ handler; every SC-018 case is a label in it. **As built:** the fixture is loaded at ``; each case starts at its own label and ends at a label the tests run to.
- [X] T170 [P] [US13] Write `UnitTest/DebuggerTests/CallStackTests.cpp` (data-model `CallStackFrame`, `CallStackBreak`): `StackWalker` as a pure function over a memory view (the W-2 opcode test of R-035; candidates dropped when a loaded debug file has no routine entry at the `JSR`'s target; the scan ends at $01FF); `CallStackRecorder` fed by a run on `TestMachine` over the fixture: three frames innermost first labeled recorded; an interrupt frame pushed on dispatch and popped on `RTI`; each FR-069 break recorded with the pc and opcode that caused it, and the inline-parameter return noted rather than broken; hybrid gives recorded frames above the attach point, guessed below, and marks the boundary; a reset clears the record; with nothing attached, a recording target sees no hook (FR-064). **As built:** the CALLS and CALLS MODE session tests are in `CallStackHandlersTests.cpp`; the reset case calls `DebugSession::OnReset` directly.
- [X] T171 [US13] Implement `CassoEmuCore/Debugger/CallStack.h/.cpp`: `CallStackRecorder` driven from `RunStopHook`'s per-instruction path (push on `JSR`, `BRK` and interrupt dispatch; pop on `RTS` and `RTI`; the break signals of R-035; the stack-level rule shared with R-033), `StackWalker`, and `CallStack::Build` for the three mechanisms with every frame labeled by its provenance (FR-068); the session holds the chosen mechanism. Makes T170 pass. **As built:** the recorder is fed from `DebugSession::OnInstruction`, which `RunStopHook` calls for every instruction it lets through, and holds each instruction until the next arrives (or a stop settles it), so an interrupt is recognized by the three bytes pushed in place of the held instruction. It runs while `DebuggerController` is open and for a batch run (`DebugSession::SetCallRecording`); the hook is installed for it only then. A break is dropped once the frames beneath it return, and a newer break over the same frames replaces an older one; frame kinds are `Call`, `Brk`, `Irq` and `Nmi` (no `Reset` frame: a reset is a break). `lastReturn` carries the inline-parameter note after the frame is gone.
- [X] T172 [P] [US13] `CALLS` and `CALLS MODE RECORDED|WALK|HYBRID` in `AppleWinParser`, `CassoEmuCore/Debugger/Handlers/CallStackHandlers.h/.cpp`, the formatters (a frame line per frame and a `--` line per break) and a `callStack` reply kind in `ReplyJson`. Tests in `AppleWinParserTests.cpp`, `CallStackHandlersTests.cpp`, the formatter tests and `ReplyJsonTests.cpp`; the reply recorded in `contracts/debug-channel-protocol.md`. **As built:** `CALLS MODE` replies with a second kind, `callStackMode`; Monitor mode reaches both through `/calls`.
- [X] T173 [US13] Implement `CassoEmuCore/Ui/Debugger/Panes/CallStackPane.h/.cpp`: frames from the snapshot with a provenance column, break rows as separators, a mechanism selector, and double-click moving the disassembly to the call site; `DebugViewSnapshot` carries the `CallStack`. Tests in `DebuggerViewStateTests.cpp`. **As built:** the pane id is `callstack`, docked by default as a tab of `stack`, and a saved layout without it gains it there; the mechanism selector is a button above the list that cycles hybrid, recorded, walk through `CALLS MODE`; activating a break row moves the disassembly to the instruction that broke the chain.
- [X] T174 [US13] Validate quickstart Story 13 steps 1-6 by hand and record the results in the commit. **Scripted 2026-09-23 at d991edff; steps 1-5 pass; step 6 passed once T195 was fixed:** (1) `CALLS` at `threein` listed three, two, one, innermost first, each recorded, with the fixture's symbols, above the power-on marker. (2) The call-stack pane listed the same frames; double-clicking the `$0800` frame brought Disassembly 1 forward with `JSR one` marked on its middle line. (3) `CALLS MODE WALK` gave the same chain, each `guessed`. (4) At `txsnext`, `-- TXS at $0850 --` with the frame below it `recorded, unverified`. (5) At `inlback`, "returned past inline parameters" and no break. Also seen: at `rewrrts` the frame read "return address changed by the store at $085D", and `recur` gave three `rec` frames. (6) `mockingboard-irq-test.dsk` gives a real timer interrupt, but a breakpoint on its handler wedged the machine (T195). After T195, `BP 86F` on its handler stopped there on each `G`, and `CALLS` listed `$0851  IRQ $C3FA  recorded` above the power-on marker.

**Checkpoint**: The call chain is shown with its provenance, and every way the program can defeat it is reported rather than hidden.

---

## Phase 19: User Story 15 - Skip routines when stepping (Priority: P3)

**Goal**: FR-070; SC-020

**Independent Test**: With `COUT` in the filter, a step into `JSR COUT` stops at the instruction after the `JSR`; with the filter cleared it stops at $FDED (quickstart Story 15).

- [X] T175 [P] [US15] Write `UnitTest/DebuggerTests/StepFilterTests.cpp` (data-model `StepFilter`): entries by name, address and range; a step into a `JSR` whose target is filtered completes by the step-over rule (R-033), in instruction granularity and in source granularity on the T119 fixture; a filtered routine that never returns leaves the run going; `SKIP name|addr|first.last`, `SKIP`, `SKIP - name` and `SKIP CLEAR` from `AppleWinParser`, and the same through `/skip` in Monitor mode; a name resolves through the symbol tables when set and is an error when it does not.
- [X] T176 [US15] Implement `CassoEmuCore/Debugger/StepFilter.h/.cpp`, the check in `RunStopHook::Begin` (a step into whose `JSR` target is filtered becomes a step over), `SKIP` in `AppleWinParser` and `CallStackHandlers`, and the reply in the formatters. Makes T175 pass.
- [X] T177 [US15] Validate quickstart Story 15 steps 1-5 by hand and record the results in the commit. **Scripted 2026-09-23 at d991edff, on a //e booted to the prompt so COUT's output hook is set:** (1) `SKIP COUT`, then `T` on `JSR COUT` stopped at $0305. (2) After `SKIP CLEAR`, `T` stopped at $FDED. (3) `SKIP F800.FFFF`, then `T` stopped at $0305. (4) With `one` filtered and `SRC ON`, `T` at `deep` stopped at `deepx`, the next source line. (5) `/skip` in Monitor mode and `!skip` in WinDbg mode both listed `$F800-$FFFF`.

**Checkpoint**: Stepping never lands inside a filtered routine, in any mode or granularity.

---

## Phase 20: User Story 14 - Use WinDbg's syntax (Priority: P3)

**Goal**: FR-011, FR-013, FR-022c, FR-022d; SC-019

**Independent Test**: In batch with WinDbg mode, run each command in `contracts/windbg-mode.md` against a fixture machine and compare the engine effect with the AppleWin command in the same row; each excluded command replies with its family (quickstart Story 14).

- [X] T178 [P] [US14] Write `UnitTest/DebuggerTests/WinDbgParserTests.cpp` for every row of the contract's command table: `0x300`, `300` and `$300`; `l<count>` lengths; `ba r1|w1|e1`; `bc *`, `bd`, `be`; `r a=41`; `file:line` with and without backquotes; `!name` reaching the engine table (T168); every excluded family in the contract producing `NotAvailable` naming the family, never `unknown`; `wt` deferred with its reason; a malformed line produces an error and no command.
- [X] T179 [US14] Implement `CassoCore/Debugger/WinDbgParser.h/.cpp` producing `DebugCommand`, `CommandMode::WinDbg`, `MODE WINDBG` and each mode's form of it, and the routing in `DebugSession`. Makes T178 pass. **As built:** each WinDbg command is rewritten as its AppleWin line and parsed by `AppleWinParser`, so the two cannot differ; excluded families return `NotAvailable` with the label `no meaning on this machine`; `WinDbg` is a row of `CommandModeNames`, and `DebuggerViewState::GetModeLine` sends the controls as `t`, `p`, `gu`, `g`, `bp`, `bc`, `eb`.
- [X] T180 [P] [US14] Write `UnitTest/DebuggerTests/WinDbgFormatterTests.cpp` for the contract's layouts (`r`, `db`, `dw`, `dd`, `da`, `bl`, `u`, `k`, `.formats`, `?`, the stop line) and implement `CassoEmuCore/Debugger/WinDbgFormatter.h/.cpp` and `OutputFormat::WinDbg` (T158); a reply kind with no WinDbg form keeps the AppleWin text. **As built:** the layout is chosen from the echoed command (`db`/`dw`/`dd`/`da`, `?`/`.formats`); `k` numbers frames in a three-wide column, so its header has one more space than the contract's; `bl`'s pass count is always `0001 (0001)`; `da` clears the high bit.
- [X] T181 [US14] Write `UnitTest/DebuggerTests/WinDbgCommandSweepTests.cpp` (SC-019): every command in the contract's table has the same engine effect as its AppleWin equivalent; every `DebugVerb` reachable from WinDbg mode is in the table; every excluded example in the contract replies with its family. **As built:** effect is compared at the parsed `DebugCommand`, plus an executed `db`/`D` comparison; `EngineMarkerTests` carries the `!` rows.
- [X] T182 [US14] `k` over `CallStack` (T171), `l+s`, `l-s` and `lsa` over `SRC`, `.formats`, and `--mode windbg` for batch in `contracts/cli-debug.md` and `DebugOptions`; a WinDbg-mode script and goldens for quickstart Story 14 steps 1-5 under `UnitTest/Fixtures/Debugger/Scripts/` with cases in `DebugModeTests.cpp`. Record the mode in `docs/Debugger.md`. **As built:** `x` needed wildcards, so `SYM` gained `*`/`?` patterns; `lsa file:line`, `wt`, `s -a` and `s -b` reply not available yet; the prompt is `0:000>`.

**Checkpoint**: All fifteen stories are functional and the full suite is green.

---

## Phase 21: What the code pane tells you about an instruction

**Purpose**: FR-110, FR-111, FR-112. From the window review: the result
annotation is a table of the 6502 written beside the CPU, which can disagree
with it, and the I/O page is annotated as though it were memory. Before the
release gate, since both change what the pane claims is true.

**Independent Test**: Single-step the //e ROM with the code pane open; every
annotated line's claim matches what the machine does when the step runs, and
every soft-switch operand shows the switch that instruction operates.

- [X] T183 FR-110: build the operand and result annotations only while the
  machine is paused, in `DebuggerViewState::BuildCode`. A case in
  `DebuggerViewStateTests` asserts a running snapshot carries neither and a
  paused one carries both. This also takes the effective-address prediction
  and its peeks off the CPU thread's path at full speed, where they run per
  visible line per snapshot today. **As built:** the CPU manager's run state
  is handed to DebuggerViewState::Build rather than stamped on the snapshot
  after it, since the session does not hold it and BuildCode needs it.
- [X] T184 Measure what a scratch `Cpu6502` costs to construct (64 KB of
  memory plus the 256-entry microcode table), Release, in a
  `Logger::WriteMessage` case beside the other measured tests. The number
  decides T185: cheap enough means a fresh instance per prediction and no
  reseeding to get wrong. If the microcode table dominates, build it once and
  share it rather than reuse a dirty CPU. **Measured 2026-09-22**, Release
  x64, median of 5 rounds of 2000 constructions in
  UnitTest/DebuggerTests/ShadowCpuCostTests.cpp: **1.7 us each**, so 0.07 ms
  for the 40 predictions a full pane asks for at one stop. T185 builds a fresh
  CPU per prediction; nothing is re-seeded, and the microcode table needs no
  sharing.
- [X] T185 FR-111: replace the hand-written table in
  `CassoEmuCore/Ui/Debugger/InstructionEffect.cpp` with an execution of the
  instruction by the emulator's own core: a `Cpu6502` subclass whose
  `ReadByteSlow` answers from `IDebugExpressionContext::TryPeek` and whose
  `WriteByte`/`WriteWord` record rather than perform, with `m_readPages` left
  null so no read escapes to the bus. Seed it from the target's registers,
  `StepOne`, and report the registers, flags and captured writes that changed.
  The test steps the REAL core over the same opcode and operand and asserts the
  prediction matches, across every addressing mode, both carry states, decimal
  mode, and a spread of operand values -- a test that can fail, which the
  table's own test could not. Decimal arithmetic then needs no special case.
  **As built:** ShadowCpu in InstructionEffect.h/.cpp; the answer is
  whatever differs between the registers going in and coming out, plus the
  captured writes, plus the flags that moved, plus the PC when it did not
  simply advance. ThePredictedEffectMatchesWhatTheCoreActuallyDoes runs 18
  cases -- including decimal ADC and SBC, the undocumented LAX, JSR, PHA and
  a branch both taken and not -- predicting each, then stepping the real
  machine and restating what changed in the same words. The old table was
  wrong or silent on most of those.
- [X] T186 FR-112: choose which of a soft switch's two titles to show by
  direction. `$C000` read is KBD and `$C000` written is 80STOREOFF, and
  `SymbolTable::TryFindName` returns whichever table matched first, so the
  pane can show `STA $C000` as `STA KBD` today -- a store to the keyboard,
  which the machine cannot do. The instruction's direction is already known,
  from the same prediction the annotation uses. Test both directions of
  every address that carries two titles. **As built:** `SymbolTable::FindNames`
  gives every symbol at an address, `SymbolDescriptions::ChooseByDirection`
  picks by the opening word of the shipped description ("Read:", "Write:"),
  and `DataDirectiveHandlers::ChooseOperandSymbol` asks
  `EffectiveAddress::ClassifyOperand`, now public, for the opcode's
  direction. `$C05E` turned out NOT to be a pair: annunciator 3 and double
  hi-res are one switch under two names, neither claiming a direction, so the
  first stands whichever way it is touched. The test covers that as well as
  `$C000`.
- [X] T187 FR-112: annotate an operand in `$C000-$C0FF` with the switch's
  description rather than a byte value, from `SymbolDescriptions`, and have
  the result column give a write's action ("speaker toggle") rather than
  claim a store. The value is not merely unread there -- it does not exist
  until a read happens, and the read is what makes the sound. **As built:**
  `SymbolDescriptions::GetAction` drops the description's leading "Read:",
  "Write:" or "Read or write:", since the instruction already shows the
  direction; `InstructionEffect::Describe` takes a lookup for the addresses it
  reports as written, and the code pane supplies one that answers for the I/O
  page. So `LDA KBD` reads "keyboard data; bit 7 set when a key is waiting"
  and `STA SPKR` reads "Toggle the speaker (each access is a click)" in both
  columns.
- [X] T188 Fill the gaps the review found in `RomSymbols.cpp` and
  `SymbolDescriptions.cpp`: `$C084-$C08F` (3 of the 16 language-card switches
  have titles), all of `$C0E0-$C0EF` (the disk controller -- phases, motor,
  drive select, read and write mode, which is most of a boot ROM single-step),
  and `$C068-$C07F`. The existing test that every shipped title has a
  description covers the new rows; datasheets and the Apple II reference,
  never another emulator's source. **As built:** the Disk II block at
  `$C0E0-$C0EF` (phases, motor, drive select, Q6 and Q7), five more
  bank-switched RAM switches, and the four IOU access switches, which include
  a genuine read/write pair at `$C07E` and `$C07F`. TWO THINGS LEFT
  DELIBERATELY UNNAMED: the `$C084-$C087` and `$C08C-$C08F` mirrors, because
  one name must not stand for two addresses, and because a mirror symbol
  would misread disk code -- a boot ROM reads the controller as `$C08C,X`
  with X holding the slot, so a language-card symbol on `$C08C` would claim a
  bank switch where the drive is being read. The slot-6 symbols help only
  code that writes `$C0Ex` outright.

**Checkpoint**: Every claim the code pane makes about an instruction comes
from the emulator or from a switch the machine documents, and none of it is
built while the machine runs.

---

## Phase 22: Release

**Purpose**: The material the release needs and the gates before merging to master (FR-065, SC-008, SC-009).

- [X] T163 [P] Extend `docs/Debugger.md` (T083) with every story: the window and its panes, memory editing, source-level debugging and the debug file, docking, the trace, device panels, conditional and value breakpoints, profiling, GSSquared mode and the output format, the call stack and its mechanisms, the step filter, WinDbg mode, and the engine-command marker per mode.
- [ ] T164 [P] FR-065: assemble `Apple2/Demos/Mockingboard/Speech/mockingboard-speech-demo-dhgr.a65` with `-g`, boot `Apple2/Demos/mockingboard-speech-demo-dhgr.dsk` in a Casso started in the background with a `--title` that is not a spec name, load the debug file, stop on a write to the speech chip's data register with the source pane, the Mockingboard panel, the trace and a memory window open, capture the window, and add it to `README.md` under the Debugger section beside the existing images. **Captured 2026-09-19:** the demo assembled with `-g`, booted in a Casso titled `speech-demo`, stopped on a write to $C440 (`mockingboard-speech-engine.inc` line 203) with the source pane, the Mockingboard panel, the trace and a memory window open. The image and the README text await the owner's approval before they go in.
- [X] T165 Measure SC-008 and SC-009 with the pinned procedure against the baseline commit before `9f12ea90`, 10 or more runs per arm; record in the merge commit. If either exceeds its bound, find the cost before merging. **Measured 2026-09-19 at dd4c8edd** (Release x64, 50M //e cycles, pinned `/affinity 10`, 12 alternating rounds, medians; head with the test machine's video timing removed so it does the same work as master's test machine, which has none): master `325bfbb5` 273.6 ms; head, debugger closed, 255.4 ms (SC-008: 6.7% faster); head, debugger open with call recording and the trace off, 259.3 ms (SC-009: +1.5% over closed). Before the opcode-watch change the open case was 733 ms.
  **As built:** SC-009 on 035-perf, Release x64, pinned to one core, alternating rounds of 50M //e cycles: closed (`CycleEmulation_MeetsBudget`) against open (a `MachineHandlerRig` session with call recording on, running freely), medians 254.5 against 257.6 ms over 8 rounds (+1.2%) and 266.3 against 263.8 ms over 10 (-0.9%), within the noise; before the change the same measure was 272 ms against 733 ms. The call record no longer needs the per-instruction hook: the CPU reports the fetches of the opcodes that can change it, the instruction after each, and each interrupt (`IOpcodeWatcher`), and the hook, when a breakpoint installs it, is asked only about instructions on the pages its breakpoints mark. SC-008 not yet measured.
- [ ] T166 Bring `CHANGELOG.md` and `README.md` to the net effect of the whole branch (T084): one `GH #51` entry from the owner's 2026-09-18 wording (a full debugger; interactive and scripted; the standard features; source-level debugging; selectable dialects, GSSquared and WinDbg listed only if their phases shipped; key schemes), headlines only, the README test count brought current, shown for approval before pushing.
- [ ] T167 Confirm 033-casso-explorer has merged to master and merge master once more (that branch was 033-cassque until 2026-09-22, when Cassque became Casso Explorer; 035 takes the rename through master rather than merging the branch, since the only Dxui commit 035 lacks is one it has no use for); then run the pre-merge gate (T085's list) on the final head, plus `scripts/CheckStyle.ps1 -Mode Tree` after `git add -A`, and every quickstart section end to end (T086); record the results in the merge commit. The merge to master is the owner's call.

---

## Dependencies & Execution Order

### Phase dependencies

- **Phases 1-8**: delivered; see each phase's checkpoint.
- **US4 (Phase 9)**: depends on the delivered window (T080) and starts with the 033 merge (T094), which every later phase builds on. T096 needs T095; T098 needs T097; T101 needs T096, T098 and T099; T102 needs T100 and T101; T103 and T104 need T102.
- **US5 (Phase 10)**: depends on US4's snapshot and panes (T098, T101). T106 needs T105; T108 needs T107; T110 needs T106, T108 and T109.
- **US6 (Phase 11)**: depends on US4 (T101). T114 needs T113; T115 needs T114; T117 needs T116; T118 needs T112, T114 and T117; T119 needs T118; T120 needs T114; T122 needs T121; T123 needs T112 and T114; T125 needs T124 and T115; T126 needs T120, T123 and T125.
- **US7 (Phase 12)**: depends on US4 (T101). T133 needs T129, T130, T131 and T132; T134 needs T133; T135 needs T134.
- **US8 (Phase 13)**: depends on US4's snapshot (T098). T138 needs T137; T139 needs T138; T140 needs T139.
- **US9 (Phase 14)**: depends on US4's snapshot (T098). T143 needs T142; T144 and T145 need T143; T146 needs T144.
- **US10 (Phase 15)**: depends on the delivered engine only. T149 needs T148; T150 needs T149.
- **US11 (Phase 16)**: depends on the delivered engine only. T153 needs T151 and T152; T154 needs T153.
- **US12 (Phase 17)**: depends on the delivered engine; T161 needs US4 (T101) and US9 (T146). T157 needs T156; T159 needs T155; T160 needs T157 and T158; T161 needs T157; T168 needs T157.
- **US13 (Phase 18)**: depends on US4's snapshot (T098) and shares the hook with US8 (T138), which it follows. T170 needs T169; T171 needs T170; T172 and T173 need T171.
- **US15 (Phase 19)**: depends on the delivered engine and the source steps (T125). T176 needs T175.
- **US14 (Phase 20)**: depends on US12 (T157, T158, T168) and US13 (T171). T179 needs T178; T180 needs T158; T181 needs T179 and T180; T182 needs T171 and T179.
- **Phase 21**: depends on US4; its work is in the window the stories built.
- **Release (Phase 22)**: depends on every story and on Phase 21. T164 needs US6, US8 and US9.

### Within each story

Tests are written first and fail, then the implementation makes them pass. After that, each test is mutation-checked. Each story merges to the branch behind the full suite; the branch merges to master when the owner decides.

## Parallel Examples

```text
# US4, after T094:
T095 DxuiListViewMetricsTests   T097 DebugViewSnapshotTests   T099 symbolic disassembly   T100 DxuiKeyMap

# US6, after T101:
T112 Sha1   T113 DebugFileReaderTests   T115 LineTable   T117 DebugFileWriterTests   T121 Merlin capture   T123 SourceService   T124 SourceStepTests

# US7, after T128:
T129 DxuiPaneLayoutTests   T130 DxuiDockDropZonesTests   T131 DxuiTabGroupTests   T132 DetachChild

# Stories with no dependency on each other, after US4:
US5 memory editing   US8 trace   US9 device panels   US10 conditions   US11 profiling   US15 step filter

# US13, after T138:
T169 callstack.a65 fixture   T170 CallStackTests

# US14, after T157 and T171:
T178 WinDbgParserTests   T180 WinDbgFormatterTests
```

## Implementation Strategy

### What is done

Phases 1-8: the engine, AppleWin and Monitor modes, batch mode, the channel, and the first window. The branch is green and the engine is at parity or ahead of both competitors (R-020).

### Incremental delivery

1. **US4** the window worth releasing, and the snapshot every later pane draws from.
2. **US5** memory editing and **US6** source-level debugging: the two P1 stories that put Casso ahead.
3. **US8** trace and **US9** device panels: the visible parity items.
4. **US7** docking.
5. **US13** the call stack, on the trace's hook, and **US15** the step filter.
6. **US10**, **US11**, **US12**: engine additions with small surfaces, in any order.
7. **US14** WinDbg mode, last of the dialects, since its `k` needs US13.
8. **Release**: screenshot, docs, changelog, the measured gates, and the merge to master.

Each story lands as its own merge to the branch behind the full suite. Nothing merges to master until the release phase, by the owner's decision (R-020), and not before 033-casso-explorer has, since 035 carries its Dxui.


## Phase 23: Convergence

**Purpose**: What the 2026-09-23 audit of spec.md against the code found unfinished. FR-071 through FR-109 came from the fit-and-finish review and were built without task entries; the audit found each of them in the code, and this phase carries only what remains.

- [ ] T189 FR-086: once 033-casso-explorer is on master (T167), draw Continue, Break, Stop, Restart, Show Next Statement, Step Into, Step Over and Step Out as `DxuiVectorIcon` shapes matching Visual Studio's debugging toolbar, move Run to Cursor from `DxuiToolbar::Entry::icon` onto `DxuiVectorIcon`, and delete `Entry::icon` and the `Flatten` helper in `Dxui/Widgets/DxuiToolbar.{h,cpp}`; command entries stay in `CassoEmuCore/Ui/Debugger/DebuggerCommands.cpp`. Check each icon on screen, enabled and disabled, in every theme, beside a Visual Studio screenshot per FR-086 (partial)
- [ ] T190 US7: stop drawing the dock tabs in `Dxui/Widgets/DxuiTabGroup.cpp` and draw them with the tab strip 033-casso-explorer built for its File Explorer window, `Dxui/Widgets/DxuiTabStrip.{h,cpp}`, which already brings overflow scroll arrows, drag to reorder, the `+` button, close buttons and theme fills. Where the dock needs something that strip lacks -- a shorter tool-window height, no icon, the PC triangle ahead of a title (FR-106), a drag that leaves the strip to dock or float, the tab menu -- add it to `DxuiTabStrip` as an option or a derived tool-window tab strip, rather than keeping a second set of tabs. Survey Visual Studio's tool-window tabs for those options first and get the owner's approval, then compare captures in every theme against Casso Explorer's tabs and Visual Studio's per FR-084 and FR-106 (partial) **Design approved by the owner 2026-09-23**, from a Visual Studio screenshot: (1) Source and Disassembly 1-4 are documents -- tabs at the top, the selected one filled and outlined with its close button (and on hover), the PC triangle ahead of the following view's title, the `+` after the last Disassembly tab; (2) every other pane is a tool window -- each group has a title bar with the active pane's title, a menu button (the tab menu's dock, float, auto-hide and close), a pin and a close button, and tabs along the bottom when it holds more than one pane, the selected tab filled with the pane's color and joined to it, the others plain text; the Memory group keeps its `+`; (3) the focused group has a 1-px accent border and its selected tab an accent outline, an unfocused group's selected tab a neutral one; (4) built on `DxuiTabStrip`, which gains a compact tool-window style joined upward, a leading mark, and a hand-off to the dock site for a drag that leaves the strip; the dock's own tab drawing in `DxuiTabGroup` goes.
- [ ] T191 Walk every debugger pane and control by hand with the owner (code, registers, stack, call stack, memory, breakpoints, watch, trace, profile, console, device panels, command bar, tabs, floating windows) and record each usability problem found as its own task below this one, with the pane, what happens, and what should happen. Then fix those tasks. Owner's 2026-09-23 review (partial)
- [X] T192 [P] SC-026: add a test that one text-size change reaches every debugger content pane, a floating one included, and leaves the caption and the command bar at their size, in `UnitTest/EmuTests/` next to the other debugger window tests (missing)
- [X] T193 [P] SC-022: extend `TheWheelScrollsTheCodePaneThroughMemory` in `UnitTest/EmuTests/DebuggerViewStateTests.cpp` to scroll down until the last shown line reaches $FFFF, so both ends of the address space are covered by scrolling alone (partial) **Done 2026-09-23:** the test also found that a view within a screenful of $FFFF listed nothing, because its range wrapped below its own start; fixed, with scrolling down now stopping at the page that ends at $FFFF.
- [X] T194 `CassoCli debug --disk1 <image>` never boots the disk: `mockingboard-test.dsk`, `wargames.dsk` and `AppleStellarInvaders.woz` all end their budget in the Disk II ROM's read loop with the same registers (A=$CD X=$60, PC $C663), and `Mount` reports success. Find where the image fails to reach the drive in `CassoEmuCore/Cli/DebugBatchRunner.cpp` (its image reader goes through `IFileSystem::ReadAllText`) or `HeadlessMachineFactory`'s `DiskOnly` slots, fix it, and add a batch test that boots a fixture disk past $C600 per FR-018 (partial)
- [X] T195 A breakpoint on an IRQ handler's first instruction wedges the machine: boot `Apple2/Demos/mockingboard-irq-test.dsk` in the window, `BP 86F` (the handler `$03FE` points to), `G`. The PC stays at $086F, the window and the channel both report running (`"running":true`), Pause from the window and from `--attach` does nothing, and the emulation thread keeps using CPU. Reproduce in a test with a 6522 timer interrupt, fix, then finish T174 step 6 (contradicts)
- [X] T196 `CALLS` on a runaway recursion (`JSR` to itself at $0300) prints 256 frames and then 2,604 identical `-- JSR at $0300 wrapped the stack pointer --` rows. Collapse a run of identical breaks into one row with its count in `CassoEmuCore/Debugger/CallStack.cpp` or the formatter (partial)
- [X] T197 FR-045: show the opcode bytes each trace entry retains in the text line (`AppleWinFormatter::FormatTraceLine`), in `HISTORY SAVE`'s file and in the trace pane (`CassoEmuCore/Ui/Debugger/Panes/TracePane.cpp`); the JSON record already carries them, and quickstart Story 8 step 1 expects them (partial)
- [X] T198 `--attach` with a command that starts no run, sent while the machine runs, waits for a stop and then pauses the machine: `PANEL LIST` and `R` each ended "the run did not stop within N seconds, so the machine was paused", while `MDB` did not. Only a command that starts a run should wait for it (contradicts)
- [X] T199 Found in the 2026-09-23 validation, for T191's walk: columns run past the pane's right edge with no scroll or fit -- the disassembly's operand and result annotations, the call-stack pane's Found by column (FR-099's words are there but unseen), and the disassembly's Instruction column itself at the default width **Done 2026-09-23:** every pane scrolls sideways when its columns are wider than it. The code pane's breakpoint gutter scrolls with the rest, where Visual Studio keeps its margin fixed; the list has no frozen columns.
- [X] T200 Found in the 2026-09-23 validation, for T191's walk: a device panel in a narrow tab clips its own header -- the MMU legend's "LC bank" runs under its ROM swatch and "slot ROM" is cut off; the Disk II header reads "track 1" at track 13; the Mockingboard tab's title shows as "Moc" **Done 2026-09-23** for the MMU key and the Disk II header. The Mockingboard tab's title cut to "Moc" is the dock tab strip overflowing, which T190's move to `DxuiTabStrip` covers.
- [X] T201 Found in the 2026-09-23 validation, for T191's walk: the command bar drops the Dialect entry's label and keeps only its glyph once the window narrows, while wider entries keep theirs; the watch pane's Automatic and Watches group headings fill only the first column; the source pane's missing-file banner still offers Show body and runs its two messages together; long Merlin listing lines wrap onto a second row **Done 2026-09-23** for the watch headings (the Value column now runs to the pane's edge) and the banner. Left as they are: the Dialect label, since the toolbar drops labels one at a time from the right by design, as the main window's does; and the wrapped Merlin lines, since the text view wraps a long line under itself by design and has no sideways scroll -- the owner's call.
- [X] T202 Step Out from inside an interrupt handler on the enhanced //e stops in the ROM's interrupt code: from the `mockingboard-irq-test.dsk` handler at $086F (SP $F0), the first Step Out stopped at $C3F4 (SP $F3) after the handler's `RTI` returned into the ROM's own frame, and the second at $C47C (SP $F4) after the ROM pulled bytes and jumped, all while the interrupted code waits at $0851 (SP $FD). End Step Out when the frame the call-stack recorder held innermost at the start is popped -- a call by its `RTS`, an interrupt by the `RTI` back to the interrupted instruction -- in `CassoEmuCore/Debugger/RunStopHook.cpp`, keeping the stack-pointer rule only where no frame was recorded; test from a handler reached through the //e Enhanced ROM (contradicts)
- [X] T203 Check whether the debugger's memory view follows the //e's internal $C100-$CFFF ROM while it is switched in: during the same run a `U` over the $C4xx page showed `FF` while the CPU ran ROM code at $C47C. If the view reads the slot there instead of what the CPU reads, fix `DebugMemoryView` so the code pane, `U` and memory windows show what the CPU executes, with a test (partial). **Checked 2026-09-23: not a bug.** The view follows INTCXROM: the ROM's IRQ entry at $C3FA writes `INTCXROMON`, and with it on, $C47C disassembles to the ROM's exit code (`LDA ROMIN`, `PLA`, ...). The `FF` listing was taken at $C3F4, after the ROM had switched the internal ROM back out, so it showed the Mockingboard slot, which has no ROM.
- [ ] T204 Found on 2026-09-23, for T191's walk: the source pane opened with a banner reading " was not found. Drop it on this window to open it.", with no file name, while the PC was outside every source line of the debug file restored from the last session; and Home in the trace pane went to the command box instead of scrolling the pane to entry 0

## Phase 24: Convergence

**Purpose**: The 2026-09-23 amendment -- source documents, one per file (FR-054, FR-113), and the Visual Studio tab presentation T190 approved (FR-114 to FR-116). In the owner's order: source documents first, then the tab strip, then the dock.

- [X] T205 Open a source document per file per FR-054, US6/AC11 and SC-027 (missing): replace `DebuggerWindow`'s single `m_sourceView`/`m_sourceBanner`/`SourcePane` with one `SourcePane` (text view and banner) per open file, each a dock pane of its own titled with the file's name, in the Disassembly views' group; a stop or navigation into a file brings its document forward, opening it only if it is not open. Keep each document's line in `DebuggerViewState` (`CassoEmuCore/Ui/Debugger/DebuggerViewState.{h,cpp}`) so the logic is testable without a window; test that stepping between the `include-macro` fixture's two files leaves exactly two documents **Done 2026-09-23:** eight document panes, `source` to `source8`, on the pool pattern of the disassembly views; `SourceDocuments` (`CassoEmuCore/Ui/Debugger/Panes/SourceDocuments.{h,cpp}`) keeps which file each holds, with `SourceDocumentsTests`. Checked in the window: stepping from `include-macro.a65` into `include-macro.inc` left two tabs, titled with the files' names, the PC's triangle moving to the one it entered; both reopened after a restart once the debug file was loaded. Closing is from the tab's menu until T211 gives the tabs close buttons.
- [X] T206 Carry the PC marker on the source document that holds the PC per FR-113 (missing), through the dock site's leading mark as the following Disassembly view's tab does, with the same triangle, face and color, and a test that the mark moves with the PC between documents
- [X] T207 Close one source document and restore the open ones per FR-113 and US6/AC12 (missing): closing a document closes only it; `DebuggerViewState::FormatOpenViews`/`ParseOpenViews` gain a `source=<file id>:<line>` entry so the documents open at close reopen at their lines; a document whose file is gone at restore reopens with its not-found notice (edge case); the document holding the PC, if closed, reopens at the next stop in its file (edge case); tests in `UnitTest/DebuggerTests/DebuggerViewStateTests.cpp`
- [X] T208 Open a macro's body in the document of the file that holds it per FR-057 (partial): `SourcePane::ToggleBody` shows the body in the body file's document, opening or bringing it forward, and `CanShowBody` still withholds it when that file could not be found
- [X] T209 Give each source document its own warning and not-found notice per FR-059 (partial), so two documents can each show their own state
- [X] T210 Add to `Dxui/Widgets/DxuiTabStrip.{h,cpp}` the options T190 needs per FR-116 (missing), each with tests beside Casso Explorer's existing ones: a document style (the current look, compact, the close button on the selected tab and on hover only), a tool-window style (compact, no icon, the strip below its pane, the selected tab filled with the pane's color and joined upward, the others plain text), a leading mark per tab (glyph, face, color) for the PC triangle, a tip per tab, the selected tab's outline color (accent or neutral) set by the host, and a hand-off to the host when a tab drag leaves the strip, so the dock can take it to dock or float. Casso Explorer's tabs must look and behave as before
- [X] T211 Present document groups with `DxuiTabStrip` per FR-114 and US7/AC9 (missing): the Source and Disassembly group's tabs along the top in the document style, the `+` after the last Disassembly tab, the leading marks of T206 and FR-106, and each tab's menu as today; `Dxui/Widgets/DxuiTabGroup.cpp` stops drawing tabs of its own **Done 2026-09-23:** `DxuiTabGroup` is a `DxuiTabStrip` in its document or tool-window style; a group holding a Disassembly view or a source document is a document group. Close buttons show only on tabs that can close (any Disassembly view but the first, a source document), through a per-tab `closable` on the strip. Checked in the window.
- [X] T212 Present tool-window groups per FR-115, FR-084 and US7/AC8 (missing): a title bar on each group with the active pane's title, a menu button offering what the tab menu offers (dock, float, auto-hide, close), a pin that auto-hides the group (FR-041) and a close button; the group's tabs along the bottom in `DxuiTabStrip`'s tool-window style when it holds more than one pane; the memory group's `+` after its last tab (FR-089); the keyboard Dock To menu of FR-042 unchanged **Done 2026-09-23:** the menu button opens the pane's Dock To menu, the pin runs its Auto Hide, and the close button shows while the active pane can close (memory windows but the first, device panels). A drag on the title bar docks or floats the active pane, as a tab drag does. Checked in the window.
- [ ] T213 Show focus and overflow per FR-116, SC-028 and US7/AC10-11 (missing): a 1-pixel accent border on the focused group and an accent outline on its selected tab, a neutral outline on an unfocused group's; tabs that do not fit scroll with the strip's arrows at every group width down to the pane's minimum; then compare captures of the debugger in every theme against Casso Explorer's tabs and the owner's Visual Studio screenshot, and close T190 (partial) **2026-09-23:** the focused group's accent border and outline are built (`DxuiDockSite::SetFocusedPane`, set once a frame from the focused control) and checked in the dark theme; the narrow-width overflow check and the captures in every theme remain- [X] T214 Found in the owner's 2026-09-24 review, for T191's walk: the Trace pane's columns, empty, were only as wide as their headings, which ran into the next column's separator. **Done 2026-09-24:** `DxuiListView` gives a heading at least 16 DIP of room however tight its cells are packed; Casso Explorer's lists, whose padding is wider, are unchanged.
- [X] T215 Found in the owner's 2026-09-24 review: the selected tab should be rounded at all four corners. **Done 2026-09-24:** in `DxuiTabStrip`'s document and tool-window styles the selected tab, and the hovered one, is a chip rounded at every corner, inset from the strip's far edge, and the group's hairline runs unbroken beside it.
- [X] T216 Found in the owner's 2026-09-24 review: the console's command box sat on the pane's bottom edge, against the tab strip. **Done 2026-09-24:** `DebuggerPaneFrame::SetBottomMarginDip` leaves room below the last part; the console leaves 6 DIP.
- [X] T217 Found in the owner's 2026-09-24 review: tabs of panes hidden against a side should read down the edge. **Done 2026-09-24:** a side strip is one tab high, its tabs run along the edge, and their titles are turned a quarter clockwise (`IDxuiTextRenderer::PushTextRotation`).
- [X] T218 Found in the owner's 2026-09-24 review: a slid-out pane should lie over the others rather than take room from them, and be dragged by a title bar to any drop zone. **Done 2026-09-24:** the docked panes keep their places; the slid pane is painted in a top layer of its own (`DxuiPanel::SetTopLayer`, `DxuiHwndSource::SetTopLayerHooks`) so it covers their text, under a tool-window title bar whose pin docks it back and whose drag slides it in and shows the drop zones. Checked in the window, sliding out from the right and the top and dragging into a group.
- [X] T219 Found while restoring the layout after T218: pinning Registers' group, at the top of the right-hand column, hid it against the top. **Done 2026-09-24:** between a side and the top or bottom a group touches, one no wider than half the window hides to its side and a wider one to the top or bottom.

## Phase 25: Convergence

**Purpose**: The 2026-09-25 amendment -- the Breakpoints pane as Visual Studio's Breakpoints window (FR-117 to FR-121). FR-041's rewrite is met by T217 and T218 and adds no task.

- [ ] T220 Add a per-row checkbox to `Dxui/Widgets/DxuiListView.{h,cpp}` per FR-117 and US17/AC9 (missing): a `Cell` flag that draws a themed checkbox ahead of the cell's icon and text, a hit test, and a callback when it is clicked or toggled with Space, with tests beside the list's others; the breakpoints pane's first column shows it, checked while enabled, followed by the FR-092 mark, and toggling it sends `BPE`/`BPD <id>`
- [ ] T221 Give the breakpoints pane its columns per FR-117 (missing): Name, Condition, Hit count ("count only" when it does not stop), Kind, Address, Label, File (file:line for a source breakpoint, from `SourceState::breakpointLines`) and When hit (Break, Break once, Count), built in `DebuggerViewState` from `BreakpointInfo` so the text is testable without a window, and sorting by a heading click
- [ ] T222 Add Show columns per FR-118 and US17/AC12 (missing): a drop-down of the optional columns with a check each, Name always shown, Condition and Hit count on by default, the choice saved through the debugger's saved view state and restored on reopen
- [ ] T223 Add the pane's toolbar per FR-119, US17/AC10-11 and SC-029 (missing): a `DxuiToolbar` of icon buttons with tips above the list -- New (a drop-down: Breakpoint at address, Function breakpoint, Data breakpoint, Register condition, Opcode, I/O range, each opening `BreakpointDialog` set to that kind), Delete, Delete all, Enable all, Disable all, Undo, Redo, Go to source code, Go to disassembly, Show columns, Export, Import -- each disabled while it cannot act; glyphs chosen from a rendered MDL2 sheet
- [ ] T224 Add breakpoint undo and redo per FR-120, SC-030 and the edge case "Breakpoints changed from the console" (missing): in `DebuggerViewState`, each pane action recorded as one step of command lines and their inverses (a delete's inverse re-adds from `MakeDefinition` and restores the enabled state and flags; Delete all and Disable all are one step each), redo cleared by a new action, an inverse whose breakpoint has gone doing nothing; tested without a window
- [ ] T225 Add Go to source code and Go to disassembly per FR-119 and US17/AC13 (missing): Go to source code opens the document of the selected breakpoint's file at its line (`OpenSourceDocument`), enabled only for one set from source or at an address with a source line; Go to disassembly moves the code pane to its address, as double-clicking does
- [ ] T226 Add Export and Import per FR-121, US17/AC14 and the edge case "Importing a file that is not a breakpoint export" (missing): Export asks for a file and writes the `BPSAVE` script; Import asks for one and adds its breakpoints to those already set -- reading only breakpoint lines, skipping `BPC` and every other command, applying each saved enabled state and flags to the breakpoint its line created, and reporting how many lines were skipped -- as one undo step; tested round trip
