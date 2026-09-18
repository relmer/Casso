# Implementation Plan: Debugger

**Branch**: `035-debugger` | **Date**: 2026-09-13, revised 2026-09-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/035-debugger/spec.md`

## Summary

One debug engine that breaks into a running `MachineHost`, driven through
three command modes (AppleWin, the default; Apple II Monitor; GSSquared) and
reached three ways: a `CassoCli debug` batch mode, a per-process named pipe,
and a Dxui debugger window. Refs GH #51, GH #59.

The engine, both original modes, batch mode, the channel and a first window
are on the branch (stories 1-3 and the first cut of story 4). The 2026-09-17
comparison against AppleWin and GSSquared (R-020) found the engine at parity
or ahead and the visible parts behind, so this revision plans the rest: a
window worth releasing, memory editing, source-level debugging, docking, an
instruction trace, device panels, expression and value breakpoints, a cycle
profile, and GSSquared's syntax.

The approach turns on these facts from research:

- **The window is the gap, not the engine** (R-020). Every remaining story
  except source-level debugging is a view over state the engine already
  holds, so the engine's seams (`IDebugTarget`, `Reply`, the snapshot the
  window reads) stay as they are and the work lands above them.
- **cc65's debug-info format already does what source-level debugging needs**
  (R-021): segment-relative spans, macro nesting in `line` records, and a
  reader that skips keys it does not know, so a `sha1` key rides along. Both
  Casso assemblers write it and the reader takes it from any assembler.
- **The CPU already keeps an instruction ring** (R-025). The trace extends it
  with the cycle count and the bus access, and turns the bus's watched-page
  path on for every page while tracing; off, nothing changes.
- **Dxui can move a control between windows** (R-023): controls hold no
  device resources, so a pane floats by moving its `unique_ptr` into a new
  `DxuiWindow`'s panel. Docking is a layout tree over `DxuiSplitter` (on
  `origin/033-cassque`, commit `18cec1b2`; the docking story starts by
  merging master once 033 has shipped it) and a new tab-group control.
- **Every device's state is already a getter** (R-034). Diagnostics is one
  interface each device implements to publish rows, rendered by one widget.

Everything new is data-in/data-out logic: the layout tree, the source
service, the trace and profile tables, the diagnostics snapshot, the GSSquared
parser and formatter, the memory editor's caret and undo. Each is tested
without an `HWND`, and the window is wiring over projections, as today.

## Technical Context

**Language/Version**: C++ `/std:c++latest`, MSVC v145+

**Primary Dependencies**: Windows SDK only. Named pipes and the user-SID DACL
as before; in-tree `JsonWriter`/`JsonParser`; in-tree `Microcode`/
`OpcodeTable`/`Assembler`; in-tree `Dxui` (`DxuiWindow`, `DxuiPanel`,
`DxuiListView`, `DxuiSplitter` from 033, `DxuiTextInput`); DirectWrite for
text measurement inside Dxui; `WM_DPICHANGED` handling already in
`DxuiHwndSource`. SHA-1 is implemented in-tree from RFC 3174 (R-021).
**No new third-party dependency.**

**Storage**: Host files through `IFileSystem`: the existing file commands;
the cc65 debug file (read and written); source files (read); Merlin listings
(read); `HISTORY SAVE` and `PROFILE SAVE` (written). The layout, the keyboard
scheme and the source path lists live in `GlobalUserPrefs` through
`UserConfigStore` (R-028, R-032). Fixtures: GSSquared output captures
(R-027), a Merlin `PUT` listing capture (R-022), and cc65 debug files for
FR-066.

**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. Real-machine
tests use `TestMachine` and the fixture ROMs (`scripts/FetchRoms.ps1
-Fixtures`). The window's panes are tested through their projections and the
controls through Dxui's existing headless harness; the layout tree, drop-zone
hit testing, the memory editor's caret and undo, the source service, the
trace and the profile are pure data and are tested directly. No test opens a
window, a pipe or a file.

**Target Platform**: Windows 10/11, x64 and ARM64.

**Project Type**: Desktop emulator plus console tool over static core libraries.

**Performance Goals**: With no window open and nothing enabled, emulation runs
the same code as before the debugger existed: one predicted branch per
instruction for the hook, one for the trace gate, an empty watch mask
(FR-064, SC-008). With the window open and the trace off, throughput within 2%
of closed (SC-009): panes read a snapshot the CPU thread builds once per frame,
never the live machine. The trace's cost is paid only while on (R-025).
Profiling counts in the hook only while on (R-026).

**Constraints**:

- The engine's public seams do not change: `IDebugTarget`, `DebugCommand`,
  `Reply`, the channel protocol. Additions are new command families and new
  `Reply` payloads, so the pipe clients written against
  `docs/DebugChannel.md` keep working.
- Determinism, cycle budgets, the DRAM seed and the pipe's SID rule are as
  before (FR-008, FR-009).
- ROM patches are debugger-only writes (R-024); no guest path can reach them.
- I/O addresses are never written by the memory editor; only `OUT` writes
  them (FR-037).
- Clean-room: AppleWin's and GSSquared's documentation is consulted for names
  and behavior only. GSSquared's output is fixed by captured fixtures, not by
  its source.
- Fixture licensing follows the constitution's fixture rule: the GSSquared
  captures and the Merlin `PUT` listing each carry a `LICENSE` note.

**Scale/Scope**: 252 AppleWin names as before, of which the 64 window-only
ones now have a window to act on; 23 Monitor entries; about 30 GSSquared
commands; 7 device panels and 3 visuals; 9 pane kinds plus up to 4 memory
windows; 2 assemblers emitting one format; ~45 new source pairs and ~35 new
test files across the remaining stories.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

### VI. Thin Executable, Testable Core (NON-NEGOTIABLE) -- the governing gate

1.11.0 allows no code in an executable at all. Nothing in this feature
touches `Casso/` or `CassoCli/`:

- Every window, floating window included, is a `DxuiWindow` created in
  `CassoEmuCore/Ui/Debugger/`. A floating pane is the same `DxuiWindow` class
  the debugger window is, holding one pane.
- The layout tree, drop-zone geometry, tab groups, auto-hide state and
  serialization are a `DockLayout` model in `CassoEmuCore/Ui/Docking/` with
  no window dependency; the window applies the model's output.
- The memory editor's caret, pending digits, completion and undo are a
  `MemoryEditModel`; the control draws it.
- The source service, line tables, hashing and path lists are in
  `CassoEmuCore/Debugger/Source/`, over `IFileSystem`.
- Trace, profile and diagnostics are engine tables read through snapshots.
- The debug-file writer is in `CassoCore/Debugger/` beside the reader; the
  assemblers call it.

**PASS.**

### II. Testing Discipline

- **Isolation**: files through `IFileSystem`, preferences through
  `UserConfigStore`'s in-memory form, monitors through the topology key
  `WindowPlacementProfile` already abstracts, DPI as a number the layout
  takes. The SHA-1 is a pure function tested against RFC 3174's vectors.
- **Acceptance tests** the spec requires: FR-027, FR-028 and FR-029 as
  before; FR-066 (an include file and a macro assembled by both assemblers,
  and every address maps to its line, invocation and body); SC-016 (every
  GSSquared command against the fixtures); SC-011 (step over on a call with
  inline parameters and on a recursive call); SC-013 (100,000 entries
  retained; the pane shows the newest).
- **Degraded operation**: fixture-driven tests assert a non-zero fixture
  count before iterating; the GSSquared sweep covers its command list in
  both directions, as the AppleWin sweep does.

**PASS.**

### III. User Experience Consistency

`CassoCli as65 -g` and `merlin -g` keep their flag and change their output
format, which is recorded in the changelog and the help page as a change
(the old reader still accepts the old file, so a user's existing `.dbg`
loads). `MODE`, `OUTPUT`, `HISTORY`, `PROFILE` and the source commands are
AppleWin-style names documented in `docs/Debugger.md`. Errors keep the
two-line stderr form. **PASS.**

### IV. Performance Requirements

No cost is added to the idle path (FR-064); the trace and the profile cost
only while on; the window reads snapshots (R-025, R-026, R-034). SC-008 and
SC-009 are measured with the pinned A/B procedure the branch already used
for the hook. **PASS.**

### V. Simplicity & Maintainability

- GSSquared mode compiles to the same `DebugCommand` form; no command is
  implemented a third time. Its formatter is the third renderer of the same
  `Reply` data.
- One trace ring (the CPU's), one profile table, one diagnostics interface,
  one layout model, one memory-edit model shared by all four windows.
- The debug-file writer is one class both assemblers call; the reader is one
  class that detects by contents.
- Scope is large by the owner's decision (R-020). Function-size limits are
  kept by one class per pane and per handler family.

**PASS.**

### I. Code Quality

Standard rules. Points needing attention:

- The trace's bus hook and the profile's hook run per instruction while on;
  they are small, allocation-free and take no locks.
- `DxuiPanel`'s detach returns the `unique_ptr`; ownership is never shared.
- ROM patch functions take an offset the memory view has already validated;
  an out-of-range offset is a coding error and asserts.

**PASS.**

**Post-design re-check**: the data model, contracts and structure below
introduce no executable code and no un-seamed system access. The layout,
memory-edit, source, trace, profile and diagnostics models are all
drivable from `UnitTest` without a window or a file. **PASS.**

## Design

### Layers

```text
 Ways in       CassoCli debug       DebugChannelServer                 DebuggerWindow + floating windows
               (batch, JSON Lines)  (pipe, JSON Lines)                 (Dxui; DockLayout; panes over snapshots)
                     \                    |                                 /
 Parsing              AppleWinParser / MonitorParser / GSSquaredParser   (per session mode)
                                     \        |       /
 Intermediate                        DebugCommand
                                          |
 Engine                            DebugSession ---- Breakpoints (address, opcode, register, memory, value, IF expr)
                                          |          Watchpoints, Watches, Symbols, DebugFile + LineTable
                                          |          Trace (CPU ring), Profile, Diagnostics providers
 Formatting                AppleWin / Monitor / GSSquared formatter   (per output format)
                                          |
 Target seam                        IDebugTarget
                                    /           \
                    MachineDebugTarget          MockDebugTarget (tests)
                    (MachineHost, DebugMemoryView + Patch, hook, trace, diagnostics)
```

- **Parsers** are unchanged in kind; `GSSquaredParser` is the third. The
  session's `mode` selects the parser and `outputFormat` the formatter;
  `MODE` sets both, `OUTPUT` the formatter alone (FR-013).
- **Source-level commands** (`SOURCE`, source steps, line breakpoints) act
  on a `LineTable` built from the loaded `DebugFile`; `SYM LOAD` loads both
  symbols and lines from one file.
- **Snapshots**: the CPU thread builds a `DebugViewSnapshot` (registers,
  the disassembly window, the memory windows' bytes, the stack, watches,
  breakpoints, the trace window, the diagnostics, the profile summary) once
  per frame while running and once on a stop. Every pane draws from it.
- **Run commands** are as before; the step-over rule changes to the stack
  pointer (R-033) and source steps repeat instruction steps until the line
  changes.

### The window

- `DebuggerWindow` owns a `DockLayout` and a set of `DebuggerPane`s, one per
  pane kind: disassembly, source, registers, stack, watches, breakpoints,
  trace, console, device panels (one pane per open device), and memory
  windows 1-4. Each pane is a `DxuiPanel` subclass that draws from the
  snapshot and sends commands as text through the host, so a click on a
  disassembly line and a typed `BP` take the same path.
- `DockLayout` (R-028, data-model Layout) is a tree of split and tab nodes.
  `DockHost` maps the tree onto `DxuiSplitter` and `DxuiTabGroup` controls,
  computes drop zones for a drag, and moves a pane's control between hosts
  and floating windows (R-023). Keyboard docking is a `Dock To` menu on the
  pane's tab plus arrow keys over the same operations (FR-042).
- Floating windows are `DxuiWindow`s; their placement is per-monitor and DPI
  is the window's own (FR-043). On load, a missing monitor falls back to
  the primary (FR-044).
- Dense lists (R-029): `DxuiListView` gains per-instance metrics; the
  debugger's lists use the monospace face at the theme's mono size.
- The memory editor (R-030) is `DxuiMemoryGrid` over a `MemoryEditModel`;
  a completed value is a `MEB`-equivalent poke through the host (FR-035),
  routed to the bus for RAM and to `DebugMemoryView::Patch` for ROM
  (R-024); the model refuses I/O cells (FR-037).
- Keyboard schemes (R-031) map keys to actions; the window looks a key up
  in the active scheme.

### Trace, profile, diagnostics

- Trace (R-025): `HISTORY ON|OFF|SAVE`, the window's toggle, and the trace
  pane. While on, `Cpu`'s ring is sized to 100,000, the bus publishes every
  page to the watched path, and the watch sink fills the pending entry's
  access. `HISTORY` renders a window of entries.
- Profile (R-026): `PROFILE ON|OFF|RESET|LIST [ADDR]|SAVE`. The hook adds
  each instruction's base and penalty cycles to the tables while on.
- Diagnostics (R-034): `IDiagnosticsProvider` per device; `MachineHost`
  lists providers; the snapshot carries one `DiagnosticsSnapshot` per open
  panel; `DiagnosticsPane` renders rows and bit decodes; `MemoryMapBar`,
  `DiskHeadView` and `MeterBar` render the visual payloads. `debug "name"`
  in GSSquared mode and the window's panel menu open panels (FR-053).

### Source-level debugging

- `DebugFileWriter` (CassoCore) takes the assembler's line-to-address
  records, its symbol table and its include and macro structure, and writes
  the contract's records. `Assembler` records, per emitted byte range, the
  source position stack (file, line, macro depth), which it already has at
  hand while emitting; that stack becomes the `line` records.
- `DebugFileReader` (CassoCore) parses cc65 v2 and the Merlin 8/16 listing
  form into a `DebugFile`; `SymbolFileReader::Detect` gains the two
  signatures.
- `SourceService` (CassoEmuCore) resolves files (R-032), hashes them,
  builds the `LineTable`, and serves the source pane's text; the drag-drop
  path hashes the dropped file and matches it.
- `RunStopHook` gains the stack-pointer rule and source-step modes (R-033).

### Threading

As before: batch is single-threaded; in the emulator the session lives on
the CPU thread and commands arrive through `PostCommand`. New: the snapshot
is built on the CPU thread and handed to the UI thread as an immutable
object once per frame; the trace's access recording and the profile's
counting run on the CPU thread inside the hook; nothing on the UI thread
touches the machine.

### Pipe lifetime

Unchanged. The layout is saved when the window closes; the pipe's open and
close still follow the controller.

## Order of Work

Stories 1-3 and the first window are done. The rest, by spec priority, each
landing as its own merge to the branch and gated by the full suite:

1. **Window (story 4, P1)**: dense `DxuiListView` metrics and the mono face
   (R-029); symbolic disassembly with per-line labels and operand symbols
   (FR-010a); breakpoint, watch and stack panes sized by content; the
   keyboard schemes (R-031); the snapshot cadence and SC-009 measurement.
2. **Memory editing (story 5, P1)**: `MemoryEditModel`, `DxuiMemoryGrid`,
   the four memory windows, `DebugMemoryView::Patch` and the ROM patch paths
   (R-024), per-window undo.
3. **Source-level (story 6, P1)**: SHA-1; `DebugFileWriter` in both
   assemblers with the FR-066 fixtures; `DebugFileReader` for cc65 and the
   Merlin listing (capture the `PUT` listing first, R-022); `SourceService`
   and the path lists; the source pane; the stack-pointer step-over and
   source steps (R-033); line breakpoints.
4. **Docking (story 7, P2)**: `DockLayout` and its serialization (R-028);
   `DxuiTabGroup`; `DockHost` over `DxuiSplitter`; drag with drop zones;
   floating windows (R-023); auto-hide; keyboard docking; layout save and
   restore with the fallback monitor.
5. **Trace (story 8, P2)**: the extended `TraceEntry`, the bus's all-pages
   watched mode, `HISTORY`, the trace pane, `HISTORY SAVE`, SC-013.
6. **Device panels (story 9, P2)**: `IDiagnosticsProvider`, the seven
   providers, `DiagnosticsPane`, the three visuals, the panel menu.
7. **Expression and value breakpoints (story 10, P3)**: `IF` on address and
   watch breakpoints, `MemoryValue` kind, the I/O-read rejection.
8. **Profiling (story 11, P3)**: the CPU's penalty byte, the profile tables,
   `PROFILE LIST [ADDR]` and `PROFILE SAVE`.
9. **GSSquared mode (story 12, P3)**: capture the fixtures (R-027);
   `GSSquaredParser`, `GSSquaredFormatter`, `OUTPUT`, the command sweep,
   `BPR` without spaces (FR-015a).
10. **Release**: the README screenshot on the Mockingboard speech demo
    (FR-065), `docs/Debugger.md` for every story, the changelog, the
    pre-merge gate, SC-008 measured.

## Risks

- **The Merlin `PUT` listing layout is unknown** (R-022). The listing
  importer is not built until the capture exists; if `PUT` lines are
  unmarked, the listing maps to itself and no source file is opened, which
  the contract already allows.
- **GSSquared's output has to be captured from a build of GSSquared**
  (R-027). If it cannot be built, the documented examples fix the format
  and the deviation is recorded; SC-016 then measures against those.
- **Docking is the largest UI piece in the tree.** It is kept honest by the
  `DockLayout` model being pure data with its own tests for every operation
  (split, tab, float, dock, auto-hide, keyboard moves, serialization,
  fallback monitor) before any control is drawn, so the controls only draw
  what the model says.
- **Tracing every bus access slows the machine while on.** That is the
  documented cost; the risk is a cost while off, which SC-008 measures with
  the pinned procedure.
- **Assembler line records for macros** need the source position stack at
  emit time. The assembler already tracks the current file and line for
  its error messages; the macro depth is added beside them. FR-066's
  fixtures make the mapping visible before the reader depends on it.
- **The old `.dbg` and the new one share an extension.** Detection is by
  contents; the sweep test loads one of each and checks both.

## Project Structure

### Documentation (this feature)

```text
specs/035-debugger/
├── plan.md                        # This file
├── research.md                    # Phase 0 output (R-001..R-034)
├── data-model.md                  # Phase 1 output
├── quickstart.md                  # Phase 1 output
├── contracts/
│   ├── debug-channel-protocol.md  # pipe protocol, sufficient for the VS Code adapter
│   ├── cli-debug.md               # CassoCli debug subcommand and script format
│   ├── command-modes.md           # AppleWin and Monitor grammar, / prefix, output formats
│   ├── gssquared-mode.md          # GSSquared grammar mapped onto the engine
│   └── debug-file-format.md       # cc65 v2 as written and read, plus the sha1 key
├── checklists/
│   └── requirements.md
└── tasks.md                       # Phase 2 output (/speckit-tasks, NOT created here)
```

### Source Code (repository root)

Existing files from the delivered stories are listed in the 2026-09-13
plan and are unchanged in role. New and changed for this revision:

```text
CassoCore/Core/Sha1.h/.cpp                   # NEW: RFC 3174, pure function over bytes
CassoCore/Debugger/
├── DebugFile.h                              # NEW: DebugFile, SourceFileRecord, LineRecord, Span, Segment
├── DebugFileReader.h/.cpp                   # NEW: cc65 v2 + Merlin 8/16 listing -> DebugFile
├── DebugFileWriter.h/.cpp                   # NEW: assembler records -> cc65 v2 with sha1
├── LineTable.h/.cpp                         # NEW: address <-> (file, line, type, depth)
├── SymbolFileReader.cpp                     # CHANGE: detect cc65 and listings, delegate
├── GSSquaredParser.h/.cpp                   # NEW: third mode
└── AppleWinParser.cpp                       # CHANGE: BPR without spaces; IF on BP/BPM; BPV; HISTORY; PROFILE; OUTPUT; SOURCE
CassoCore/Assembler.h/.cpp                   # CHANGE: source position stack per emitted range; -g calls DebugFileWriter
CassoEmuCore/Cli/As65Mode.cpp, MerlinMode.cpp # CHANGE: -g writes the cc65 file

CassoEmuCore/Core/Cpu.h/.cpp                 # CHANGE: TraceEntry gains cycles + access; penalty byte beside last cycles
CassoEmuCore/Core/MemoryBus.h/.cpp           # CHANGE: all-pages watched mode for the trace
CassoEmuCore/Devices/RomDevice.h/.cpp        # CHANGE: PatchByte
CassoEmuCore/Machines/Apple2/Common/LanguageCard.h/.cpp, CxxxRomRouter.h/.cpp  # CHANGE: PatchByte
CassoEmuCore/Machines/**                     # CHANGE: IDiagnosticsProvider on Disk2Controller, Apple2eMmu, video, keyboard, Via6522/Ay8910 (Mockingboard), printer, clock

CassoEmuCore/Debugger/
├── DebugMemoryView.h/.cpp                   # CHANGE: Patch for ROM regions; IsIo
├── BreakpointTable.h/.cpp                   # CHANGE: IF conditions, MemoryValue kind
├── RunStopHook.h/.cpp                       # CHANGE: stack-pointer step-over; source-step modes
├── TraceController.h/.cpp                   # NEW: HISTORY on/off/save; window of entries
├── ProfileTable.h/.cpp                      # NEW: per-opcode, penalty and per-address tables
├── IDiagnosticsProvider.h                   # NEW: rows, bits, visual payloads
├── DiagnosticsSnapshot.h                    # NEW
├── DebugViewSnapshot.h/.cpp                 # NEW: what the window draws, built per frame on the CPU thread
├── GSSquaredFormatter.h/.cpp                # NEW: third output format
├── Source/
│   ├── SourceService.h/.cpp                 # NEW: resolve, hash, match, path lists, text
│   └── SourcePathList.h/.cpp                # NEW: per-program and global lists in prefs
└── Handlers/
    ├── SourceHandlers.h/.cpp                # NEW: SOURCE, line breakpoints, source steps
    ├── TraceHandlers.h/.cpp                 # NEW: HISTORY
    ├── ExecutionHandlers.cpp                # CHANGE: PROFILE LIST [ADDR], SAVE, RESET; BPV value form
    └── ConfigHandlers.cpp                   # CHANGE: OUTPUT

CassoEmuCore/Ui/Docking/
├── DockLayout.h/.cpp                        # NEW: split/tab tree, floating, auto-hide, JSON, fallback monitor
├── DockHost.h/.cpp                          # NEW: tree -> DxuiSplitter/DxuiTabGroup; drop zones; move pane
├── DockDragState.h/.cpp                     # NEW: drag geometry and drop-zone hit test, pure data
└── KeyScheme.h/.cpp                         # NEW: three tables, action lookup

CassoEmuCore/Ui/Debugger/
├── DebuggerWindow.h/.cpp                    # CHANGE: owns DockLayout, panes, floating windows
├── DebuggerViewState.h/.cpp                 # CHANGE: reads DebugViewSnapshot
├── Panes/
│   ├── DebuggerPane.h                       # NEW: base; draws from snapshot; sends commands through host
│   ├── DisassemblyPane.h/.cpp               # NEW: labels, symbolic operands, click-to-break
│   ├── SourcePane.h/.cpp                    # NEW
│   ├── MemoryPane.h/.cpp                    # NEW: one per memory window, over DxuiMemoryGrid
│   ├── MemoryEditModel.h/.cpp               # NEW: caret, pending, completion, undo
│   ├── TracePane.h/.cpp                     # NEW
│   ├── DiagnosticsPane.h/.cpp               # NEW: rows and bits
│   ├── MemoryMapBar.h/.cpp, DiskHeadView.h/.cpp, MeterBar.h/.cpp  # NEW: visuals
│   └── RegistersPane, StackPane, WatchesPane, BreakpointsPane, ConsolePane  # NEW: split out of the first window
└── FloatingPaneWindow.h/.cpp                # NEW: DxuiWindow holding one pane

Dxui/
├── Core/DxuiPanel.h/.cpp                    # CHANGE: DetachChild returns the unique_ptr
├── Widgets/DxuiListView.h/.cpp              # CHANGE: per-instance row height, padding, font
├── Widgets/DxuiSplitter.h/.cpp              # from 033-cassque; merged, not written here
├── Widgets/DxuiTabGroup.h/.cpp              # NEW: tabs over a set of children
└── Widgets/DxuiMemoryGrid.h/.cpp            # NEW: hex + text grid with a caret

CassoEmuCore/Shell/MachineHost.h/.cpp        # CHANGE: diagnostics providers; snapshot build
CassoEmuCore/Config/GlobalUserPrefs.h/.cpp   # CHANGE: debugger layout, key scheme, source paths

UnitTest/DebuggerTests/
├── Sha1Tests.cpp                            # RFC vectors
├── DebugFileWriterTests.cpp                 # FR-066: include + macro, both assemblers; old and new .dbg
├── DebugFileReaderTests.cpp                 # cc65 v2, unknown keys, bad ids, version check, Merlin listing
├── LineTableTests.cpp
├── SourceServiceTests.cpp                   # resolution order, size filter, hash match, mismatch warning, drag
├── SourceStepTests.cpp                      # SC-011: inline parameters, recursion, step out, IRQ during step
├── GSSquaredParserTests.cpp, GSSquaredFormatterTests.cpp, GSSquaredCommandSweepTests.cpp  # SC-016
├── OutputFormatTests.cpp                    # MODE sets both, OUTPUT one
├── ExpressionBreakpointTests.cpp            # IF, value breakpoints, I/O rejection
├── TraceTests.cpp                           # SC-013, access fields, off = no cost path taken
├── ProfileTableTests.cpp                    # buckets, penalties, per-address, SAVE
├── DiagnosticsProviderTests.cpp             # each device's rows; SC-015 cadence
├── MemoryEditModelTests.cpp                 # SC-012; grouping; text column; undo; I/O refusal; ROM patch
├── DebugMemoryViewPatchTests.cpp            # ROM patch lands where the CPU reads, survives $C028
├── DockLayoutTests.cpp                      # every operation; JSON round trip; SC-014 fallback
├── DockDragStateTests.cpp                   # drop zones
├── KeySchemeTests.cpp
└── DebugViewSnapshotTests.cpp

UnitTest/DxuiTests/
├── DxuiListViewMetricsTests.cpp
├── DxuiTabGroupTests.cpp
└── DxuiMemoryGridTests.cpp

UnitTest/Fixtures/Debugger/
├── GSSquared/                               # captured replies + LICENSE
├── Merlin/PI.ADD.LST                        # PUT listing capture + LICENSE
└── DebugFiles/                              # cc65 v2 samples, one from cc65 itself

docs/Debugger.md                             # CHANGE: every story
docs/Assembler.md                            # CHANGE: -g writes cc65 v2
README.md                                    # CHANGE: screenshot (FR-065)
```

**Structure Decision**: the debug-file reader and writer, the line table and
the GSSquared parser depend on nothing but strings and CPU tables, so they
live in `CassoCore/Debugger/`. Docking is UI-library-shaped but debugger-
specific in its pane model, so the layout model lives in
`CassoEmuCore/Ui/Docking/` while the two generic controls (`DxuiTabGroup`,
`DxuiMemoryGrid`) join `Dxui`. Device diagnostics are declared in the
debugger and implemented by each device, so a future machine's devices add
providers without touching the window. Every new file is listed by hand in
its `.vcxproj`.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| A per-instruction hook in `MachineHost`, the hottest loop in the emulator | Breakpoints, opcode breaks and stepping must stop before a given instruction executes | Checking once per `RunCycles` slice misses addresses inside the slice. Rewriting code bytes with `BRK` corrupts guest-visible memory and fails against ROM. The hook is a pointer test when unused (R-004) |
| A second headless machine builder path (`HeadlessMachineFactory`) | `CassoCli debug` must boot a real machine | Leaving it in UnitTest makes batch mode impossible. The factory is promoted, and `TestMachine` becomes a thin user of it, so there is still one builder |
| Routing every bus access through the watched-page path while the trace is on | The trace records each instruction's bus access, which only the bus sees | A second, trace-only bus path duplicates the slow path; a per-instruction effective-address recomputation in the hook cannot see the data byte. The cost exists only while on (R-025) |
| A docking layout model and two new controls | The spec asks for Visual Studio's arrangement operations (FR-038 to FR-044) | Fixed panes with splitters only cannot float, tab or auto-hide; a third-party docking library is not permitted by the constitution's allowlist |
| Writable ROM images (`PatchByte` on three classes) | FR-037: an edit to a ROM address patches what the CPU reads | Copying ROM into RAM at patch time breaks banking; refusing ROM edits leaves the memory editor unable to patch the Monitor, which AppleWin's can |
