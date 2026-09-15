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

- [ ] T001 Create directories `CassoCore/Debugger/`, `CassoEmuCore/Debugger/`, `CassoEmuCore/Debugger/Handlers/`, `CassoEmuCore/Debugger/Channel/`, `CassoEmuCore/Ui/Debugger/`, `UnitTest/DebuggerTests/` and `UnitTest/Fixtures/Debugger/Scripts/`. Add a `DebuggerTests` filter folder to `UnitTest/UnitTest.vcxproj.filters` if that project has a filters file.
- [ ] T002 [P] Create `UnitTest/Fixtures/Debugger/LICENSE` covering the directory: the transcribed 1979 *Apple II Reference Manual* Monitor listing (Apple Computer, Inc., 1979; archive.org item `Apple_II_Reference_Manual_1979_Apple`), marked read-only to the tests that use it.
- [ ] T003 [P] Confirm the 11 fixture ROMs are present with `scripts/FetchRoms.ps1 -Fixtures` (whitelisted). Record in `specs/035-debugger/research.md` R-010 the SHA-256 of `UnitTest/Fixtures/Apple2.rom` that the listing test pins.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The target seam, the side-effect-free memory view, the per-instruction hook, the headless machine, and the pure CPU-table tools every story uses

**⚠️ CRITICAL**: No user story work begins until this phase is complete

### Pure CPU-table tools (CassoCore)

- [ ] T004 [P] Write `UnitTest/DebuggerTests/DisassemblerTests.cpp`: every opcode `$00`-`$FF` for the 6502 table (`Cpu::GetMicrocode`, including the undocumented set from `Cpu::InitializeUndocumented`) and for the 65C02 table produced by `Cpu65C02`. Assert the length, mnemonic, operand text in Monitor form (`#$A0`, `($3E),Y`, `$1234,X`), branch target resolution, and `documented` flag per data-model "disassembly" kind. Assert the swept opcode count is 256 per CPU.
- [ ] T005 Implement `CassoCore/Debugger/Disassembler.h/.cpp`: `Disassembler (const Microcode * table)`, and `DisassembleOne (Word address, std::span<const Byte> bytes)` returning a `DisassembledInstruction {address, bytes, mnemonic, operand, target (optional Word), documented}`. It is built from `Microcode::instructionName` and `globalAddressingMode` and must not reuse `Cpu::PrintSingleStepInfo`. Makes T004 pass.
- [ ] T006 [P] Transcribe `UnitTest/Fixtures/Debugger/AppleII-1979-MonitorListing.txt` from the 1979 Reference Manual's listing of the **original** Monitor ROM (the one with `S` and `T`), not the Autostart ROM listing the same manual also carries. Before transcribing, compare the listing's first page of bytes against `Apple2.rom` at $F800 to confirm the fixture is that ROM. Format: one line per instruction as `ADDR BYTES MNEMONIC OPERAND`, and data regions as `DATA start-end` lines. Record the transcription's line count in a header comment line starting with `;`.
- [ ] T007 Write `UnitTest/DebuggerTests/MonitorListing1979Tests.cpp` (FR-028, SC-003), per research R-010:
  1. Load `Apple2.rom` through `FixtureProvider`, and assert its $F800-$FFFF bytes equal the transcription's bytes.
  2. Disassemble from $F800, skipping only declared `DATA` regions.
  3. Assert address, length, mnemonic and operand field by field for every line.
  4. Assert the instruction count equals the transcription's non-zero count.

  Depends on T005 and T006.
- [ ] T008 [P] Write `UnitTest/DebuggerTests/LineAssemblerTests.cpp`: one line in Monitor mini-assembler syntax (`LDA #$41`, `JMP ($0036)`, branches to absolute targets turned into relative offsets, out-of-range branch errors) for the 6502 and 65C02 tables. It round-trips through `Disassembler`.
- [ ] T009 Implement `CassoCore/Debugger/LineAssembler.h/.cpp` over an `InstructionSetProvider` (base and 65C02 extended tables) and `OpcodeTable::TryLookup` / `GetOperandSize`. `TryAssemble (Word address, const std::string & line, std::vector<Byte> & outBytes, std::string & outError)` returns a status enum, not a bool. Makes T008 pass.
- [ ] T010 [P] Write `UnitTest/DebuggerTests/DebugExpressionEvaluatorTests.cpp`: hex with and without `$`, decimal with `#` per AppleWin, `+ - * / & | ^ ! < >`, parentheses, register names `A X Y P S PC`, memory dereference, and symbol lookup through an injected resolver. Also cover errors for unknown symbols and malformed input.
- [ ] T011 Implement `CassoCore/Debugger/DebugExpressionEvaluator.h/.cpp` (the assembler already owns `class ExpressionEvaluator` in `CassoCore/ExpressionEvaluator.h`, so the name must differ) with an `IDebugExpressionContext` seam (`TryGetRegister`, `TryPeek`, `TryResolveSymbol`) and a parsed `Expression` API type that breakpoint conditions store. Makes T010 pass.

### API types

- [ ] T012 [P] Create `CassoCore/Debugger/DebugCommand.h`: `DebugVerb` enum (total over every engine operation; ends in `Count` for sweeps) and `DebugCommand` with the fields in data-model.md: `verb`, `sourceName`, `a1/a2/a3` plus presence flags, `values`, `expression`, `text`, `count`, `budget` (`std::optional<uint64_t>`).
- [ ] T013 [P] Create `CassoEmuCore/Debugger/Reply.h`, holding the reply-side types from data-model.md:
  - `CommandStatus { Ok, Error, NotAvailable, Unknown }`;
  - the `ReplyData` variant, with one struct per data kind listed in `contracts/debug-channel-protocol.md`;
  - `Reply { status, command, data, text, error {label, detail} }`;
  - `StopReason` and `StopEvent`;
  - `RunRequest { kind: Go | StepInto | StepOver | StepOut | RunTo | Trace, untilPc, count, budget }`.

### Target seam, memory view and hook (CassoEmuCore)

- [ ] T014 Create `CassoEmuCore/Debugger/IDebugTarget.h` with the operations in data-model.md "IDebugTarget": `GetRegisters/SetRegisters`, `Peek/Poke/GetRegion`, `ReadIo/WriteIo`, `GetSoftSwitches`, `StartRun (const RunRequest &)` with the stop delivered to an `IRunObserver::OnStopped`, `RequestPause`, `GetVideoPosition`, `GetCpuKind`, `GetMachineInfo`, `InjectKey`. Add `UnitTest/DebuggerTests/MockDebugTarget.h`, an in-memory 64 KB implementation for session tests.
- [ ] T015 [P] Write `UnitTest/DebuggerTests/DebugMemoryViewTests.cpp` (R-003) against `TestMachine` for Apple ][, ][+, //e, Enhanced //e and //c:
  - **Parity below $C000 and above $D000**: across banking states set through the soft switches (RAMRD/RAMWRT, ALTZP, 80STORE, language-card bank 1/2 read and write), `Peek` equals the value a CPU read returns; bus reads there have no side effects. `UnitTest/EmuTests/MemoryProbeHelpers.h` shows the probing pattern.
  - **Parity in $C100-$CFFF**: for each INTCXROM/SLOTC3ROM/INTC8ROM state, `Peek` equals the byte of the slot or internal ROM image that state selects, compared without a bus read, because a bus read there latches the router.
  - **No side effects**: a full peek sweep leaves every soft switch, the `CxxxRomRouter` latches and the speaker state unchanged.
  - **Region labels** match `mainRam|auxRam|lcBank1|lcBank2|rom|slotRom|io`.
  - **Writes**: `Poke` to ROM returns read-only and changes nothing.
  - **Non-zero**: assert that the number of addresses swept is non-zero.
- [ ] T016 Implement `CassoEmuCore/Debugger/DebugMemoryView.h/.cpp` per research R-003: the bus page tables below $C000; slot and internal ROM images for $C100-$CFFF selected by `IMmu::GetIntCxRom/GetSlotC3Rom` and the `CxxxRomRouter` state; `LanguageCard::IsReadRam/IsWriteRam/IsBank2/ReadRom` and `Apple2cRomBank` for $D000-$FFFF. $C000-$C0FF is never read; `Peek` reports it unreadable. Makes T015 pass.
- [ ] T017 [P] Write `UnitTest/DebuggerTests/DebugHookTests.cpp`:
  - with a null hook, `MachineHost::RunCycles` behavior and cycle counts are unchanged;
  - with a hook that stops at an address, `StepOne` and `RunCycles` stop before that instruction executes, including an address in the middle of a slice;
  - a pending-stop flag set during an instruction stops at the next boundary.
- [ ] T018 Create `CassoEmuCore/Debugger/DebugHook.h` (`ShouldStopBefore (Word pc)`, `HasPendingStop()`). Change `CassoEmuCore/Shell/MachineHost.h/.cpp`: add `SetDebugHook (DebugHook *)` and call it before each instruction in `StepOne` and in the `RunCycles` loop, doing only a null-pointer test when unset (R-004). Microbenchmark instructions per second before and after with no hook; record the numbers in the commit message. Makes T017 pass.
- [ ] T019 Promote the machine-building code from `UnitTest/EmuTests/TestMachine.h/.cpp` into `CassoEmuCore/Shell/HeadlessMachineFactory.h/.cpp` with an `IRomSource` seam (R-006). `TestMachine` becomes a user of the factory, backed by a `FixtureProvider` ROM source. All existing `EmuTests` must still pass unchanged.
- [ ] T020 Implement `CassoEmuCore/Debugger/MachineDebugTarget.h/.cpp`: `IDebugTarget` over `MachineHost`, `DebugMemoryView` and `I6502DebugInfo`, in its synchronous form: `StartRun` installs the hook, calls `RunCycles` in chunks until a stop, `untilPc` or the budget, and delivers `OnStopped` before returning. A run with no budget is unbounded. Add `UnitTest/DebuggerTests/MachineDebugTargetTests.cpp`, covering run-to, budget stop with reason `Budget`, step-over of a recursive subroutine, registers round trip, `GetVideoPosition`, and soft-switch listing on each machine.

**Checkpoint**: Full suite green. The disassembler matches the 1979 listing, peeks are side-effect free on every machine, and the hook costs nothing when unset.

---

## Phase 3: User Story 1 - Break into a running program from a script (Priority: P1) 🎯 MVP

**Goal**: The engine, AppleWin mode for every phase-1 name, symbols and binary formats, and `CassoCli debug` batch mode with text and JSON Lines output

**Independent Test**: `CassoCli debug --machine apple2e --script stop.txt`, where the script enters a loop at $0300 reading $C019 and sets `BPMR C019`, stops after the read with the reading instruction's address, prints registers and three steps, and yields byte-identical output on a second run, in both text and `--json` (quickstart phase 1 steps 2-4)

### Session and tables

- [ ] T021 [P] [US1] Write `UnitTest/DebuggerTests/BreakpointTableTests.cpp` over data-model "Breakpoint":
  - kinds `Address`, `Opcode`, `Register`, `Memory`, `Io`, `Brk`, `Interrupt`;
  - enable and disable;
  - stable ids until cleared;
  - `hitCount`;
  - the 64 KB bitmap rebuilt on every change, and consulted before opcode and condition breakpoints.
- [ ] T022 [US1] Implement `CassoEmuCore/Debugger/BreakpointTable.h/.cpp`. Makes T021 pass.
- [ ] T023 [P] [US1] Write `UnitTest/DebuggerTests/WatchpointTableTests.cpp`:
  - `Read`, `Write` and `ReadWrite` on inclusive `first`/`last` ranges;
  - ids shared with the breakpoint numbering;
  - a page's read and write page-table entries are null only while it holds an enabled watchpoint, and are restored when the last one is cleared or disabled;
  - a watched page's accesses still return and store the right values;
  - a watchpoint in $C000-$FFFF forwards to the underlying device exactly once;
  - a hit records `{accessPc, address, value, access}` and sets the pending stop.
- [ ] T024 [US1] Implement `CassoEmuCore/Debugger/WatchpointTable.h/.cpp` and `WatchpointDevice.h/.cpp` against `MemoryBus`'s page tables and device list (R-004). Makes T023 pass.
- [ ] T025 [P] [US1] Implement `CassoEmuCore/Debugger/WatchTable.h/.cpp` for AppleWin watches (`W*`), zero-page pointers (`ZP*`, `P0`-`P4`) and bookmarks (`BM*`), with `UnitTest/DebuggerTests/WatchTableTests.cpp`.
- [ ] T026 [P] [US1] Write `UnitTest/DebuggerTests/DebugSessionTests.cpp` against `MockDebugTarget`:
  - the state transitions in data-model "DebugSession";
  - a run command while running returns `Error` "already running";
  - machine switch clears breakpoints and watchpoints, and reset keeps them;
  - mode switch keeps all tables;
  - unknown and malformed commands change no state.
- [ ] T027 [US1] Implement `CassoEmuCore/Debugger/DebugSession.h/.cpp`: `Execute (const DebugCommand &) -> Reply`, ownership of the tables, dispatch to handler families, `OnStopped` turning a `StopEvent` into the `stopped` notification, and the session budget: unbounded by default, set by `BUDGET <n>`, cleared by `BUDGET 0`, overridden per run by `DebugCommand::budget` (R-005). Makes T026 pass.

### AppleWin command table, parser and formatter

- [ ] T028 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinCommandTableTests.cpp`. It sweeps the name list in spec Assumptions "AppleWin command coverage" in both directions:
  - every listed name resolves to a phase-1 handler, to phase 3, or to not-available with its reason text;
  - every phase-1 `DebugVerb` has at least one name;
  - aliases resolve to their target;
  - a name in no list is `Unknown`;
  - the listed-name count is asserted non-zero, and the phase-1, phase-3 and not-available counts are printed and recorded in plan.md Scale/Scope.
- [ ] T029 [US1] Implement `CassoCore/Debugger/AppleWinCommandTable.h/.cpp`, a file-scope `static constexpr` table (`s_kAppleWinCommands`) of `{name, family, phase, availability, reason}`. Makes T028 pass.
- [ ] T030 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinParserTests.cpp`:
  - case-insensitive names;
  - `$` and bare hex;
  - expressions through `DebugExpressionEvaluator`;
  - argument forms for each family, taken from AppleWin's help pages `help/dbg-*.html` (names and behavior only, never implementation; R-014);
  - Casso engine commands `MODE`, `MODE APPLEWIN`, `MODE MONITOR`, `PAUSE` and `BUDGET`.
- [ ] T031 [US1] Implement `CassoCore/Debugger/AppleWinParser.h/.cpp` producing `DebugCommand`. Makes T030 pass.
- [ ] T032 [P] [US1] Write `UnitTest/DebuggerTests/AppleWinFormatterTests.cpp`, pinning text per `contracts/command-modes.md`:
  - `R` as `A:00 X:00 Y:00 P:30 S:FF PC:0300` plus a flag string;
  - `D` as eight bytes per row with ASCII;
  - disassembly lines;
  - breakpoint lists;
  - the two-line error for `Error` and for `NotAvailable` (`Error: command not available` / `NAME needs the debugger window.`).
- [ ] T033 [US1] Implement `CassoEmuCore/Debugger/AppleWinFormatter.h/.cpp` rendering `Reply::data` to `Reply::text`. Makes T032 pass.
- [ ] T034 [P] [US1] Write `UnitTest/DebuggerTests/ReplyJsonTests.cpp`:
  - every data kind in `contracts/debug-channel-protocol.md` serializes with integer addresses and bytes, never hex strings;
  - unreadable I/O bytes serialize as `null`;
  - each record is one line with no raw newline;
  - for a sample of commands, the JSON `text` array equals the formatter's text (Story 1 scenario 5).
- [ ] T035 [US1] Implement `CassoEmuCore/Debugger/ReplyJson.h/.cpp` over `JsonWriter` with pretty printing off. Makes T034 pass.

### Handler families (phase-1 AppleWin names)

Each handler task adds the family's tests in `UnitTest/DebuggerTests/<Family>HandlersTests.cpp`, using `TestMachine` wherever banking or devices matter. Expected outputs come from AppleWin's documented examples.

- [ ] T036 [P] [US1] `CassoEmuCore/Debugger/Handlers/ExecutionHandlers.h/.cpp`:
  - `G`, `GG` (both unthrottled in batch; in the emulator `GG` sets full speed and restores the previous `SpeedMode` on stop), `P` (step over: run until PC is at the instruction after the `JSR` with SP restored, so recursion is one call), `T`, `TL`, `RTS` (step out) and `=`;
  - `BPV` and `VIDEOINFO` from `GetVideoPosition`;
  - `JSR`, `NOP`/`ZAP`, and `KEY` (queued by cycle, R-015);
  - `LBR`;
  - `TF` (trace to a file through `IFileSystem`);
  - `PROFILE`, `BENCHMARK`/`BENCH`/`EXITBENCH`, `CYCLES` and `RCC`.
- [ ] T037 [P] [US1] `CassoEmuCore/Debugger/Handlers/BreakpointHandlers.h/.cpp`:
  - setting: `BP`, `BPA`, `BPR`, `BPX`, `BPIO`, `BPM`, `BPMR`, `BPMW`, `BRK`, `BRKOP`, `BRKINT`;
  - managing: `BPC`, `BPD`, `BPE`, `BPL`, `BPEDIT`, `BPCHANGE`;
  - saving: `BPSAVE`.
- [ ] T038 [P] [US1] `CassoEmuCore/Debugger/Handlers/RegisterHandlers.h/.cpp`:
  - `R`/`REGISTER`;
  - `CL`, `CLC`, `CLZ`, `CLI`, `CLD`, `CLB`, `CLR`, `CLV`, `CLN`;
  - `SE`, `SEC`, `SEZ`, `SEI`, `SED`, `SEB`, `SER`, `SEV`, `SEN`;
  - aliases `RC` `RZ` `RI` `RD` `RB` `RR` `RV` `RN` and `SC` `SZ` `SI` `SD` `SB` `SR` `SV` `SN`;
  - stack: `POP`, `PPOP`, `PUSH`.
- [ ] T039 [P] [US1] `CassoEmuCore/Debugger/Handlers/MemoryHandlers.h/.cpp`:
  - view and enter: `D`, `MDB`, `ME`, `MEB`, `MEW`, `ME8`, `ME16`;
  - move, compare, fill: `M`/`MM`, `MC`, `F`;
  - search: `S`/`MS`, `SH`, `@`;
  - files: `BLOAD`, `BSAVE`, `TSAVE`;
  - I/O: `IN`/`INPUT`, `OUT`;
  - the Casso engine commands `SWITCHES` and `STACK` (FR-007), as defined in `contracts/command-modes.md`.
- [ ] T040 [P] [US1] `CassoEmuCore/Debugger/Handlers/DataDirectiveHandlers.h/.cpp`: `Z`, `X`, `B`, `DB`, `DB2`, `DB4`, `DB8`, `DW`, `DW2`, `DW4`, `ASC`, `DF`, `DA`, and `U` disassembly honoring those data ranges. Also `A addr`, which enters the line-assembly mode Monitor `!` uses (each following line assembled through `LineAssembler`, a blank line ends it), as `contracts/command-modes.md` defines.
- [ ] T041 [P] [US1] `CassoEmuCore/Debugger/Handlers/ConfigHandlers.h/.cpp`: `PWD`, `CD`, `LOAD`, `SAVE`, `DISASM`, `STARTUP`, `RUN` (a script through the same session), `DISK`, `LOG`, `ECHO`, `PRINT`, `PRINTF`, `CALC`, `?`, `HELP`, `VERSION`, `MOTD`, `WSAVE`, `ZPSAVE`, `BMSAVE`.

### Symbols and binary formats (FR-031, FR-032, FR-033)

- [ ] T042 [US1] Capture the Merlin listing fixture:
  1. On a copy of `UnitTest/Fixtures/Disks/Merlin-proDos2.23.dsk` (never the pristine image), boot Merlin Pro under Casso and load `LABELS.S` (`L`, `E`, `ASM`), following the capture procedure behind `scripts/CaptureMerlinCorpus.ps1`. Record the Merlin version the menu reports.
  2. Print the assembly listing with the printer output going to a file.
  3. Check it in as `UnitTest/Fixtures/Merlin/LABELS.listing.txt`, and add it to `UnitTest/Fixtures/Merlin/LICENSE` and `README.md`.
  4. Record in research R-009 the symbol-table layout it shows: section headings, column widths, and how `]` variables and local labels appear.
- [ ] T043 [US1] Change `CassoCore/Assembler.h/.cpp` `FormatListing`, used by the Merlin dialect, to append a symbol table in the layout recorded in T042. Add tests in `UnitTest/MerlinListingSymbolTableTests.cpp` comparing the table section of `CassoCli merlin -l LABELS.S` output against `LABELS.listing.txt`. The table is appended only when the dialect profile is Merlin. Add an as65 case to the same test file asserting an as65 listing is unchanged.
- [ ] T044 [US1] Add `-g` to `CassoCli merlin`:
  - a flag row in `CassoCore/CommandLineParser.cpp` for the Merlin dialect;
  - `CassoEmuCore/Cli/MerlinMode.cpp` writes `<output>.dbg` per `SAV` output through `ArtifactWriter::WriteDebugInfo`, on the per-output rule `As65Mode::WriteExtraArtifacts` uses;
  - tests in `UnitTest/PerOutputArtifactTests.cpp` and `UnitTest/MerlinCommandLineTests.cpp`;
  - documentation in `docs/Assembler.md` under Merlin.
- [ ] T045 [P] [US1] Write `UnitTest/DebuggerTests/SymbolFileReaderTests.cpp`:
  - Casso `-g` (`NAME=$ADDR`, `;` comments, both sections);
  - the Merlin listing symbol table, against `LABELS.listing.txt`;
  - AppleWin `.SYM` (`ADDR NAME`);
  - VICE labels (`al ADDR .NAME`);
  - detection from content;
  - an error for an unrecognized file;
  - the symbol count asserted non-zero for each fixture.
- [ ] T046 [US1] Implement `CassoCore/Debugger/SymbolFileReader.h/.cpp` and `CassoEmuCore/Debugger/SymbolTable.h/.cpp` (tables `Main`, `Basic`, `Asm`, `User`, `User2`, `Src`, `Src2`, `Dos33`, `ProDos`; case-insensitive lookup). Makes T045 pass.
- [ ] T047 [P] [US1] Author ROM symbol tables in `-g` format as file-scope tables in `CassoEmuCore/Debugger/RomSymbols.cpp`, one per machine plus DOS 3.3 and ProDOS entry points. Take names and addresses only from Apple's published Reference Manuals and DOS/ProDOS technical references, never from another emulator's symbol files (FR-031), and record the source of each table in a comment. `UnitTest/DebuggerTests/RomSymbolsTests.cpp` checks spot entries (`COUT $FDED`, `GETLN $FD6A`, `MONZ $FF69`) and that each table is non-empty.
- [ ] T048 [P] [US1] Write `UnitTest/DebuggerTests/AppleSingleCodecTests.cpp`: read and write round trip of the data fork, real name, and ProDOS file info (type, aux type); magic `$00051600` detection; errors for truncated and unknown-version input. First check with the 033-cassque session whether it has already added `AppleSingleCodec`; if so, reuse it and skip T049.
- [ ] T049 [US1] Implement `CassoEmuCore/Core/AppleSingleCodec.h/.cpp` if 033 has not: no knowledge of memory, disks or the debugger (research R-016). Makes T048 pass.
- [ ] T050 [P] [US1] Write `UnitTest/DebuggerTests/BinaryImageReaderTests.cpp`:
  - Intel HEX and S-record, with checksums and multiple segments;
  - AppleSingle, taking its address from the aux type;
  - DOS 3.3 binary and raw, each selected explicitly;
  - an error for a raw load with no address;
  - content detection never guessing DOS 3.3 binary.
- [ ] T051 [US1] Implement `CassoEmuCore/Debugger/BinaryImageReader.h/.cpp` (in EmuCore because it uses the codec there; CassoCore cannot reference CassoEmuCore) returning `{segments, format}`. `BLOAD` in T039 uses it. Makes T050 pass.
- [ ] T052 [P] [US1] `CassoEmuCore/Debugger/Handlers/SymbolHandlers.h/.cpp`: `SYM`, `SYMMAIN`, `SYMBASIC`, `SYMASM`, `SYMUSER`, `SYMUSER2`, `SYMSRC`, `SYMSRC2`, `SYMDOS33`/`SYMDOS`, `SYMPRODOS`/`SYMPRO`, `SYMINFO`, `SYMLIST`, and bookmark commands `BM`, `BMA`, `BMC`, `BML`, `BMG`. Tests in `UnitTest/DebuggerTests/SymbolHandlersTests.cpp`.

### Batch mode

- [ ] T053 [P] [US1] Write `UnitTest/DebuggerTests/DebugOptionsParseTests.cpp` for `contracts/cli-debug.md` batch options:
  - `--machine`, `--disk1`, `--disk2`, `--script` (including `-`), repeatable `--command`, `--mode`, `--json`, `--max-cycles`, `--seed`, `--write-disks`;
  - `debug --help`;
  - usage errors.
- [ ] T054 [US1] Add `Subcommand::Debug` and `DebugOptions` to `CassoCore/CommandLineOptions.h`, parsing in `CassoCore/CommandLineParser.cpp`, and the `debug` help page in `CassoEmuCore/Cli/CommandLine.cpp`. Update `UnitTest/CliSwitchCoverageTests.cpp` for the new grammar. Makes T053 pass.
- [ ] T055 [P] [US1] Write `UnitTest/DebuggerTests/DebugModeTests.cpp` with scripts in `UnitTest/Fixtures/Debugger/Scripts/` and expected output in `expected/*.txt` and `*.jsonl`:
  - Story 1 scenarios 1-5, using the script in quickstart phase 1 step 2 (a loop at $0300 that reads $C019, no disk);
  - the exit statuses 0, 1, 2 and 3 from the contract;
  - `;` comment lines;
  - a mode switch mid-script;
  - disk writes going to the overlay unless `--write-disks`;
  - two runs of each script compared byte for byte (SC-004), including a script that power-cycles, which only passes if the DRAM seed is pinned;
  - exit status 1 taking precedence over 3;
  - all host file access (scripts, `BLOAD`, `BSAVE`, `R`, `W`, `TF`) through a mock `IFileSystem`, with no real files.
- [ ] T056 [US1] Implement `CassoEmuCore/Cli/DebugMode.h/.cpp` (batch runner taking an injected `IFileSystem`, on `HeadlessMachineFactory` and `MachineDebugTarget`, no wall clock, the `Prng` seeded from `--seed` with default `0xCA550001`, every run carrying the `--max-cycles` budget, R-015) and the `debug` arm in `CassoEmuCore/Cli/CliMain.cpp`. Makes T055 pass.

**Checkpoint**: Quickstart phase 1 steps 2-4 and 6 pass by hand, and the full suite is green. The MVP is usable for bug diagnosis.

---

## Phase 4: User Story 2 - Use Apple II Monitor syntax (Priority: P1)

**Goal**: Monitor mode with the union of every ROM's commands on every machine, the `/` prefix, and Monitor-format output

**Independent Test**: A batch script with `--mode monitor` runs `300: A9 41 60`, `300.302`, `300L`, `41<300.3FFS`, `300S`, `^E`, `: 01 02 03`, `/bpl` and gets Monitor-format output (quickstart phase 1 step 5)

### Verification gates (run before any Monitor command work)

- [ ] T057 [US2] Write `UnitTest/DebuggerTests/MonitorCommandTableTests.cpp` (FR-027, SC-002), per research R-011:
  - for `Apple2.rom`, `Apple2Plus.rom`, `Apple2e.rom`, `Apple2eEnhanced.rom` and `Apple2c.rom`, read 23 command bytes at $FFCC and handler offsets at $FFE3;
  - decode each byte with the inverse of the transform in the routine at $FFBE, first asserting that known entries (`L`, `G`, `M`) decode correctly on every ROM;
  - exclude only entries matching the declared filler rule (character `$EA` with handler `$00`, the //c's last entry);
  - assert every other decoded command is in `MonitorParser`'s command set;
  - assert 23 entries were read per ROM.

  Until T060 exists, the test compiles against an empty command set and fails; that is expected.
- [ ] T058 [US2] Write `UnitTest/DebuggerTests/MonitorRomFactsTests.cpp` (FR-029), per research R-012:
  - on `Apple2eEnhanced.rom` and `Apple2c.rom`, decode the `!` entry of the $FFCC/$FFE3 command table, follow its handler, and assert it prints the `!` prompt and calls the input routine; assert on `Apple2.rom` that $F666 is that ROM's mini-assembler entry; record what the ][+, //e, Enhanced //e and //c hold at $F666 (Applesoft), which is why `F666G` is an alias, not a jump;
  - boot each of those two machines to the `*` prompt in `TestMachine`, type `300l` with `UnitTest/EmuTests/KeystrokeInjector.h`, and assert with `UnitTest/EmuTests/TextScreenScraper.h` that a listing appears.

  If either assertion fails against the ROM, stop and update spec FR-017/FR-021 and research R-012 with what the ROM does before continuing.

### Implementation

- [ ] T059 [P] [US2] Write `UnitTest/DebuggerTests/MonitorParserTests.cpp` for every form in `contracts/command-modes.md` "Apple II Monitor mode":
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
- [ ] T060 [US2] Implement `CassoCore/Debugger/MonitorState.h` (`a1`-`a4`, `lastExamined`, `storeAddress`, `registerEditPending`, `assemblerActive`) and `CassoCore/Debugger/MonitorParser.h/.cpp`, following the Monitor's line scan (R-013). Makes T059 pass and, with T057, the ROM coverage test.
- [ ] T061 [P] [US2] Write `UnitTest/DebuggerTests/MonitorFormatterTests.cpp`:
  - examine `0300- A9 00 8D 00 03 60 ...`, rows aligned to 8-byte boundaries (`303.30F` gives `0303-` with five bytes then `0308-` with eight), identical on every machine;
  - list `0300-   A9 00       LDA   #$00`;
  - verify `0303-41 (42)` per difference;
  - registers `A=00 X=00 Y=00 P=30 S=FF`;
  - arithmetic, 8-bit: `FF+FF` gives `=FE`;
  - step and trace display in the original ]['s step output layout;
  - `ERR` followed by the two-line error.
- [ ] T062 [US2] Implement `CassoEmuCore/Debugger/MonitorFormatter.h/.cpp`. Makes T061 pass.
- [ ] T063 [US2] Implement `CassoEmuCore/Debugger/Handlers/MonitorHandlers.h/.cpp` with the effects in research R-013, applied directly, never by jumping into ROM:
  - `I`/`N` set `INVFLG` ($32) to $3F/$FF;
  - `n^K` sets `KSWL/H` ($38/$39) to $Cn00 and `n^P` sets `CSWL/H` ($36/$37) to $Cn00 for slots 1-7; `0^K` restores $FD1B and `0^P` restores $FDF0;
  - `^B`/`^C` run at $E000/$E003, and `^Y` runs at $03F8;
  - `G` pushes the return address the ROM's `G` handler pushes and sets an internal breakpoint there, and never reloads registers from $45-$49; a test sets A in AppleWin mode, runs Monitor `G`, and sees A unchanged;
  - `^E` shows registers and arms `:` register edit, which sets the CPU registers and writes $45-$49;
  - search prints matching addresses;
  - `R`/`W` use `IFileSystem`: `R` reads the smaller of file and range and reports a mismatch; with no filename they return an error in batch and pipe;
  - `!` and `F666G` enter `LineAssembler` mode.

  Tests in `UnitTest/DebuggerTests/MonitorHandlersTests.cpp` cover Story 2 scenarios 1-6, including a breakpoint set in AppleWin mode listed from Monitor mode.
- [ ] T064 [US2] Add Monitor-mode scripts and expected output to `UnitTest/Fixtures/Debugger/Scripts/` and cases to `UnitTest/DebuggerTests/DebugModeTests.cpp` for quickstart phase 1 steps 5 and 6, with `out.bin` held in the mock `IFileSystem`.

**Checkpoint**: The FR-027, FR-028 and FR-029 tests pass on every fixture ROM, and the full suite is green. Plan phase 1 is complete and mergeable to master.

---

## Phase 5: User Story 3 - Attach to a running Casso (Priority: P2)

**Goal**: A per-process, current-user named pipe carrying the JSON Lines protocol, many serialized clients, notifications, `debug --list` and `debug --attach`, and `Casso --debugger`

**Independent Test**: Start Casso with `--debugger`, list it, attach, `bpmr C000` (the keyboard read at the `]` prompt), `g`, and receive a `stopped` notification. A second client receives it too (quickstart phase 2)

- [ ] T065 [P] [US3] Write `UnitTest/DebuggerTests/ChannelProtocolTests.cpp` for `contracts/debug-channel-protocol.md`:
  - framing: LF-terminated lines, CR tolerated on input, lines capped at 1 MiB;
  - `hello` in both directions, including a client `protocol` higher than the server's;
  - `command` requests with optional `mode` and `budget`, and `pause`;
  - replies echo `id`; notifications carry no `id`, and a stop caused by a command carries its `causeId`;
  - an `error` record for malformed input, with unknown fields ignored.
- [ ] T066 [US3] Implement `CassoEmuCore/Debugger/Channel/ChannelProtocol.h/.cpp`. Makes T065 pass.
- [ ] T067 [US3] Create `CassoEmuCore/Debugger/Channel/IPipeTransport.h` (listen, accept, read line, write line, close; per-connection ids) and `UnitTest/DebuggerTests/InMemoryPipeTransport.h`.
- [ ] T068 [P] [US3] Write `UnitTest/DebuggerTests/DebugChannelServerTests.cpp` over the in-memory transport:
  - several clients, with commands run one at a time in arrival order and each reply sent only to its requester;
  - every notification delivered to every client (Story 3 scenario 6);
  - a client disconnecting while paused leaves the machine paused;
  - server close sends `closing` and disconnects all clients.
- [ ] T069 [US3] Implement `CassoEmuCore/Debugger/Channel/DebugChannelServer.h/.cpp` and a thread-safe `IDebugReplySink`, routing commands through `CpuManager::PostCommand` with a new command id. Makes T068 pass.
- [ ] T070 [US3] Wire the session into the emulator:
  - `CassoEmuCore/Shell/CpuManager.h/.cpp` and `EmulatorShellCpuThread.cpp`: a debug command id whose payload is the line plus a reply-sink id;
  - the emulator form of `MachineDebugTarget::StartRun` (R-005): record the run, apply `fullSpeed` to `SpeedMode`, un-pause `CpuManager` and return; the frame loop's `RunCycles` slices run with the hook installed and count the budget across slices; on a hook stop, `RunCycles` returns the short slice, the target pauses `CpuManager`, restores `SpeedMode`, and delivers `OnStopped` from the CPU thread; `RequestPause` does the same with reason `pause`. Tests drive `EmulatorShell` headless and confirm a `pause` request is processed while a run is in progress;
  - `MachineManager.cpp`: attach the session beside `AttachDebugSinksIfOpen`, emit `machineChanged` and clear the tables on switch, and emit `reset` on soft reset and power cycle;
  - `EmulatorShell` user pause emits `stopped` with reason `pause`.

  Tests go in `UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp`, which drives `EmulatorShell` headless.
- [ ] T071 [P] [US3] Write `UnitTest/DebuggerTests/DebuggerControllerTests.cpp`: opening starts the server and closing stops it and disconnects clients, leaving breakpoints and pause state alone; `--debugger` opens the controller at machine start without pausing.
- [ ] T072 [US3] Implement `CassoEmuCore/Debugger/DebuggerController.h/.cpp` and parse `--debugger` in `CommandLineParser::ParseEmulator` (`CassoCore/CommandLineParser.cpp`). Add `--debugger` to the emulator's documented-options table in the same file so `Casso --help` lists it, with a case in `UnitTest/CliSwitchCoverageTests.cpp`. Makes T071 pass.
- [ ] T073 [US3] Implement `CassoEmuCore/Debugger/Channel/Win32PipeTransport.h/.cpp`, per research R-008:
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
- [ ] T074 [P] [US3] Write `UnitTest/DebuggerTests/InstanceDirectoryTests.cpp` over a mock `IInstanceDirectory`: listing omits instances that refuse the connection or don't answer `hello`, and the output columns are `pid title machine disk1 disk2`.
- [ ] T075 [US3] Implement `CassoEmuCore/Debugger/Channel/IInstanceDirectory.h` and `Win32InstanceDirectory.h/.cpp` (enumerate `\\.\pipe\` for `Casso.Debug.`). Add `debug --list` and `debug --attach <pid>` to `CassoEmuCore/Cli/DebugMode.cpp`, with parsing in `CassoCore/CommandLineParser.cpp`. Every run it starts carries `budget` from `--max-cycles` (default 100000000); after a run command it waits for `stopped` for at most `--timeout` seconds (default 120), then sends `pause` and exits with status 3; it exits with status 2 when the pipe closes. Add `--list` and `--attach` to the `debug` help page in `CassoEmuCore/Cli/CommandLine.cpp`. Makes T074 pass.
- [ ] T076 [US3] Write `docs/DebugChannel.md` from `contracts/debug-channel-protocol.md` as user-facing documentation.
- [ ] T077 [US3] SC-007 validation: write `scripts/DebugChannelClient.ps1`, a client that uses only `System.IO.Pipes.NamedPipeClientStream` and `docs/DebugChannel.md`, and run quickstart phase 2 steps 1-6. Step 5, the other-user connection, is manual; record its result in the commit message.

**Checkpoint**: Quickstart phase 2 passes, and the full suite is green. Plan phase 2 is mergeable.

---

## Phase 6: User Story 4 - Debug in a window beside the emulator (Priority: P3)

**Goal**: A Dxui debugger window with a command line, disassembly, registers, memory, stack, watches and breakpoints; click-to-set breakpoints; step, step-over, run and run-to-cursor controls; and the phase-3 AppleWin commands

**Independent Test**: Open the window, click a disassembly line to set a breakpoint, run to it, edit a byte in the memory view, and confirm with `d 300` (quickstart phase 3)

- [ ] T078 [P] [US4] Write `UnitTest/DebuggerTests/DebuggerViewStateTests.cpp` for the projection of session state:
  - disassembly around PC with the current line flagged;
  - register and flag rows;
  - memory rows with region labels;
  - stack, watches and breakpoints;
  - click-to-toggle breakpoint on a line;
  - a memory edit producing a poke;
  - step, step over, run and run-to-cursor producing the matching `RunRequest`;
  - the command line executing in the selected mode with the same reply as batch (Story 4 scenario 4);
  - a breakpoint set through `DebugChannelServer` over the in-memory transport appearing in the breakpoint pane, and one set by clicking appearing in a client's `bpl` reply (SC-006).
- [ ] T079 [US4] Implement `CassoEmuCore/Ui/Debugger/DebuggerViewState.h/.cpp`. Makes T078 pass.
- [ ] T080 [US4] Implement `CassoEmuCore/Ui/Debugger/DebuggerWindow.h/.cpp`, a `DxuiWindow` subclass following `Ui/Disk2DebugPanel`:
  - panes built from `DxuiListView`, `DxuiTextInput`, `DxuiToolbar` and `DxuiCommand` (032 widgets);
  - `OnCreate`/`OnWindowClose` drive `DebuggerController` open and close;
  - the `R`/`W` filename prompt when a filename is missing.

  Wire the menu item and open/re-attach in `CassoEmuCore/Shell/EmulatorShellDebug.cpp`, and make `--debugger` open the window.
- [ ] T081 [US4] Implement the phase-3 AppleWin names in `CassoEmuCore/Debugger/Handlers/ViewHandlers.h/.cpp`, and move them to phase-1 availability in `CassoCore/Debugger/AppleWinCommandTable.cpp`:
  - cursor: `.`, `RET`, `^`, `v` and their Shift forms, `PAGEUP`, `PAGEUP256`, `PAGEUP4K`, `PAGEDN`, `PAGEDOWN256`, `PAGEDOWN4K`, and the `->` aliases;
  - window: `WIN`, `WINDOW`, `CODE`, `CODE1`, `CODE2`, `CONSOLE`, `DATA`, `DATA1`, `DATA2`, `SOURCE1`, `SOURCE2`, `\`;
  - mini memory panes: `MD1`, `MD2`, `MA1`, `MA2`, `MT1`, `MT2`, `M1`, `M2`;
  - views: `TEXT`, `TEXT1`, `TEXT2`, `TEXT80`, `TEXT81`, `TEXT82`, `TEXT40`, `TEXT41`, `TEXT42`, `GR`, `GR1`, `GR2`, `DGR`, `DGR1`, `DGR2`, `HGR`, `HGR0`-`HGR8`, `DHGR`, `DHGR1`, `DHGR2`;
  - appearance: `BW`, `COLOR`, `FONT`, `HCOLOR`, `MONO`.

  Their projection is covered in `DebuggerViewStateTests.cpp`, and in batch and pipe they still return `notAvailable`.
- [ ] T082 [US4] Validate the window by running Casso in the background with `--title 035-debugger` and capturing it with `PrintWindow`. Run quickstart phase 3 steps 1-4 and attach the screenshots to the commit.

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

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Setup and blocks every story. T007 needs T005 and T006; T020 needs T016, T018 and T019.
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
T015 DebugMemoryViewTests    T017 DebugHookTests

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
