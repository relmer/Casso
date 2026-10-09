# Implementation Plan: Debugger

**Branch**: `035-debugger` | **Date**: 2026-09-13, revised 2026-09-18 and 2026-10-08 (disk breakpoints) | **Spec**: [spec.md](spec.md)

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
  `DxuiWindow`'s panel. Docking is a pane layout over `DxuiSplitter` and a
  new tab-group control, and it is generic, so it lives in `Dxui`.
- **033-casso-explorer's Dxui is the base.** `DxuiSplitter`, `DxuiHexView`,
  `DxuiTextView` and `DxuiCommandRouter` are on `origin/033-casso-explorer` (91
  Dxui commits ahead of master) and stable; 035 merges that branch first,
  builds on them, and merges to master only after 033 has.
- **Every device's state is already a getter** (R-034). Diagnostics is one
  interface each device implements to publish rows, rendered by one widget.

Everything new is data-in/data-out logic: the layout tree, the source
service, the trace and profile tables, the diagnostics snapshot, the GSSquared
parser and formatter, the memory editor's caret and undo. Each is tested
without an `HWND`, and the window is wiring over projections, as today.

**2026-10-08: disk breakpoints (User Story 20, R-041).** Added at the request
of the disk inspector work (040). `BPDISK` adds breakpoint-table entries of a
`Disk` kind. The Disk II controller gains a second listener slot behind its
one dispatch pointer, a saved record of the most recent nibbles with the
instruction that read or wrote each, a head position per drive (GH #135) and
one write hook; the field tracker, the matching and the reports are debugger
code over that record, so a live run, a replay and reverse continue match the
same fields. The field definitions and `??` matcher, the per-drive head and the
write hook are shared with 040 and built by whichever merges first. Scheduled
with the owner.

## Technical Context

**Language/Version**: C++ `/std:c++latest`, MSVC v145+

**Primary Dependencies**: Windows SDK only. Named pipes and the user-SID DACL
as before; in-tree `JsonWriter`/`JsonParser`; in-tree `Microcode`/
`OpcodeTable`/`Assembler`; in-tree `Dxui` (`DxuiWindow`, `DxuiPanel`,
`DxuiListView`, `DxuiTextInput`, and from 033 `DxuiSplitter`,
`DxuiHexView`, `DxuiTextView` and `DxuiCommandRouter`); DirectWrite for
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
(FR-064, SC-008). With the window open and the trace off, throughput within 3%
of closed (SC-009): panes read a snapshot the CPU thread builds once per frame,
never the live machine. The trace's cost is paid only while on (R-025).
Profiling counts in the hook only while on (R-026). One exception to "the
same code", from story 20: the Disk II controller's record of recent nibbles
(FR-158) is machine state, kept whether or not the debugger is open, at a few
stores per nibble the CPU receives or writes and nothing while the drive is
idle (FR-064, FR-164); T732 measures it against T165's baseline for SC-008.

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

- Every window, floating window included, is a `DxuiDockedWindow`, a
  `DxuiWindow` whose content is a `DxuiDockSite`; the debugger window
  holds the whole layout and a floating pane holds one pane.
- The layout tree, drop-zone geometry, auto-hide state and serialization are
  a `DxuiPaneLayout` model in `Dxui/Core/` with no window dependency; the
  dock site applies the model's output.
- The memory editor's pending digits, completion and undo are a
  `MemoryEditModel`, the `IDxuiHexSource` behind `DxuiHexView`; the
  view's caret advances per nibble and the model writes on completion.
- The source service, line tables, hashing and path lists are in
  `CassoEmuCore/Debugger/Source/`, over `IFileSystem`.
- Trace, profile and diagnostics are engine tables read through snapshots.
- The debug-file writer is in `CassoCore/Debugger/` beside the reader; the
  assemblers call it.

**PASS.**

**2026-10-09 additions** (stories 21 to 23): the project model, scope keys and
store, the Applesoft reader, detokenizer and stepper, the C lexer and the
editable grid's row logic are core classes tested with in-memory files and
synthetic memory; only the dialogs, panes and menu wiring are window code.
No gate is violated.

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

**Re-checked 2026-10-08 for story 20.** The nibble record (FR-158) adds a
cost to the path FR-064 covers, and FR-064 now states it as its one
exception: machine state rather than a hook, so a replay and a step back
match fields as a forward run does (R-041, "Where fields are decoded"). The
cost is a few stores per nibble the CPU receives or writes, none while the
drive is idle, and it must fit within SC-008 against T165's baseline, which
T732 measures before the story merges; armed breakpoints are held to SC-041.
**PASS, with the exception recorded in FR-064 and the Complexity Tracking
table.**

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
- `DxuiKeyMap`'s chord struct matches `CassoExplorerCommands::kKeys` so Casso Explorer
  can move onto it (agreed with 033); `IDxuiHexSource::WriteBytes` defaults
  to refusing so read-only sources stay read-only.
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
               (batch, JSON Lines)  (pipe, JSON Lines)                 (Dxui; DxuiDockSite; panes over snapshots)
                     \                    |                                 /
 Parsing              AppleWinParser / MonitorParser / GSSquaredParser / WinDbgParser   (per session mode)
                                     \        |       /
 Intermediate                        DebugCommand
                                          |
 Engine                            DebugSession ---- Breakpoints (address, opcode, register, memory, value, IF expr)
                                          |          Watchpoints, Watches, Symbols, DebugFile + LineTable
                                          |          Trace (CPU ring), Profile, Diagnostics providers
                                          |          CallStack (recorder + walker), StepFilter
 Formatting                AppleWin / Monitor / GSSquared / WinDbg formatter   (per output format)
                                          |
 Target seam                        IDebugTarget
                                    /           \
                    MachineDebugTarget          MockDebugTarget (tests)
                    (MachineHost, DebugMemoryView + Patch, hook, trace, diagnostics)
```

- **Parsers** are unchanged in kind; `GSSquaredParser` is the third and
  `WinDbgParser` the fourth. The session's `mode` selects the parser and
  `outputFormat` the formatter; `MODE` sets both, `OUTPUT` the formatter alone
  (FR-013).
- **Engine commands** are reached through each mode's own marker (FR-014,
  R-037): `/` in Monitor mode, bare names in AppleWin and GSSquared modes, `!`
  in WinDbg mode. Each parser strips its marker and hands the rest to the
  shared engine-command table, so the table exists once.
- **Source-level commands** (`SOURCE`, source steps, line breakpoints) act
  on a `LineTable` built from the loaded `DebugFile`; `SYM LOAD` loads both
  symbols and lines from one file.
- **Snapshots**: the CPU thread builds a `DebugViewSnapshot` (registers,
  the disassembly window, the memory windows' bytes, the stack, watches,
  breakpoints, the trace window, the diagnostics, the profile summary) once
  per frame while running and once on a stop. Every pane draws from it.
- **Run commands** are as before; the step-over rule changes to the stack
  pointer (R-033). Step granularity is session state (`SRC ON|OFF`): with
  `source` on, every mode's existing step commands and the window's step
  actions repeat instruction steps until the source line changes, so no
  dialect gains a step name. In the window, focus entering the source pane
  or the disassembly pane sets the granularity, as Visual Studio does.

### The window

- `DebuggerWindow` is a `DxuiDockedWindow` owning a set of `DebuggerPane`s,
  one per pane kind: disassembly, source, registers, stack, watches, breakpoints,
  trace, console, device panels (one pane per open device), and memory
  windows 1-4. Each pane is a `DxuiPanel` subclass that draws from the
  snapshot and sends commands as text through the host, so a click on a
  disassembly line and a typed `BP` take the same path.
- `DxuiPaneLayout` (R-028, data-model Layout) is a tree of split and tab
  nodes. It is not `DxuiDockLayout`, which lays the emulator window's edge
  bands around a center and has no tabs, floating or nesting.
  `DxuiDockSite` maps the tree onto `DxuiSplitter` and `DxuiTabGroup`
  controls, computes drop zones for a drag through `DxuiDockDropZones`, and
  moves a pane's control between sites and floating windows (R-023).
  Keyboard docking is a `Dock To` menu on the pane's tab plus arrow keys
  over the same operations (FR-042).
- Floating windows are `DxuiDockedWindow`s holding one pane; their placement
  is per-monitor and DPI is the window's own (FR-043). On load, a missing
  monitor falls back to the primary (FR-044).
- Dense lists (R-029): `DxuiListView` gains per-instance metrics; the
  debugger's lists use the monospace face at the theme's mono size.
- The memory editor (R-030) is 033's `DxuiHexView` with editing added
  (`IDxuiHexSource::WriteBytes`, a per-nibble overwrite caret) over a
  `MemoryEditModel` source; a completed value is a `MEB`-equivalent poke
  through the host (FR-035), routed to the bus for RAM and to
  `DebugMemoryView::Patch` for ROM (R-024); the model refuses I/O writes
  (FR-037).
- The source pane is 033's `DxuiTextView` with the line number and text as
  its cells.
- Keyboard schemes (R-031) are three `DxuiKeyMap` tables; `DxuiWindow`
  consults its active map after `DxuiCommandRouter` finds no standard
  command, and the debugger swaps the map when the scheme changes.

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
- `RunStopHook` gains the stack-pointer rule and the source-line repeat
  (R-033); the session's granularity chooses which the step commands use.

### Call stack, step filter, WinDbg mode

- `CallStackRecorder` (CassoEmuCore/Debugger) is fed by the run hook while
  the debugger is attached: it pushes on `JSR`, `BRK` and interrupt dispatch,
  pops on `RTS` and `RTI`, and records every break in FR-069 with the
  instruction and address that caused it (R-035). It is off, and not on the
  code path, when nothing is attached (FR-064).
- `StackWalker` scans the stack page for return addresses whose preceding
  opcode is `JSR`, and drops candidates whose `JSR` targets no known routine
  entry when a debug file is loaded. It is a pure function over a memory view
  and the symbol table, so it is tested without a machine.
- `CallStack` combines the two per the selected mechanism (recorded, walk,
  hybrid) into frames labeled by provenance, with break rows between them.
  It serves the `CALLS` engine command in every mode, WinDbg's `k`, and the
  `CallStackPane`, which draws from the snapshot like every other pane.
- `StepFilter` is session state: a list of symbols, addresses and ranges.
  `RunStopHook` consults it on a step into: a `JSR` whose target is in the
  filter is treated as a step over, in instruction and in source
  granularity. The `SKIP` engine command sets, lists and clears it (R-038).
- `WinDbgParser` (CassoCore) and `WinDbgFormatter` (CassoEmuCore) are the
  fourth mode (R-036): the command table in `contracts/windbg-mode.md`, a
  defined "no meaning on this machine" reply for the excluded families, and
  WinDbg's documented output layouts for `db`, `r`, `bl`, `u` and `k`.

### Disk breakpoints (story 20)

R-041 holds the decisions and their alternatives; data-model.md, "Disk
breakpoints", the types; contracts/command-modes.md the `BPDISK` grammar and
replies; contracts/debug-channel-protocol.md the `disk` stop reason and
objects.

- **Controller** (`Disk2Controller`): the window's slot stays `SetEventSink`;
  `SetDebuggerEventSink` adds the debugger's; every place that reports an
  event tests one dispatch pointer, null, either sink, or a `Disk2EventTee`
  when both are set. `OnWriteMode` joins `IDisk2EventSink` with an empty
  default. Each drive keeps its own quarter track. One write hook reports each
  latch load in write mode while the motor runs, with whether write
  protection dropped it. A 512-entry `Disk2NibbleRecord` of the nibbles the
  CPU received or wrote, with each one's instruction address from
  `MachineHost`, joins the saved state (the next state version; every earlier
  one loads with an empty record).
- **Shared parts**: `DiskMarkPattern` and `DiskFieldKind` in CassoCore, where
  `AppleWinParser` builds `DiskBreakSpec`; `DiskFieldFormat` in
  `CassoEmuCore/Devices/Disk/` (FR-162).
- **Debugger**: `DiskFieldTracker` decodes 16- and 13-sector fields from the
  record with the shared `DiskFieldFormat` and `DiskMarkPattern`; the
  `DiskBreakpointMonitor` sits in the debugger's slot while a disk breakpoint
  is armed, reads the record at each instruction boundary, matches the
  table's `Disk` entries, and holds the pending stop and its `DiskReport` for
  the session, which reports it with `StopReason::Disk`. RWTS and driver
  breakpoints stop on the routine their vector holds, taken again by
  `DiskCallVector` after a write to the vector's bytes (an internal watch with
  no id) and after every other change FR-151 lists; the request in the IOB or
  the driver's zero page is matched from the `DiskBreakSpec`, never stored as
  the entry's condition. `DebugSession::TryResolveSymbol` resolves the
  `DISK.` values through `DiskValueSymbols`, from
  `IDebugTarget::TryGetDiskState`, before it looks in the symbol tables, so
  they serve every expression.
- **Reverse**: `IReverseStopTest::GetDiskSink`; `Replayer` swaps the debugger
  slot as it swaps the hook and the watch sink, and reports bays whose disk a
  boundary snapshot changed; trackers prime from the restored record.
- **Pane**: New > Disk event..., the dialog's Disk type, the columns, Go to
  disassembly and the gutter for resolved routines, undo, Export and Import.
- **Threading**: every disk event and the record are on the CPU thread, where
  the session runs; nothing on the UI thread touches the controller.

### Debug projects (story 21)

R-042 holds the decisions; data-model.md, "Debug projects", the types;
contracts/command-modes.md the `BPNAME` and `PROJECT` commands.

- **Model** (`CassoEmuCore/Debugger/Project/DebugProject.h/.cpp`, core): the
  FR-166 state as one value. The engine's part is the script the debugger's
  `SAVE` already writes (`BreakpointHandlers::MakeScript`,
  `WatchHandlers::MakeScript` for watches, zero-page pointers and
  bookmarks), extended with data directives and step filters, in Casso's own
  mode so it reads back exactly; beside it the names, the window's state
  (layout, range sets, disassembly options, open views) and the startup
  commands. Serialized as JSON through `JsonParser`; paths relative to the
  project file where they lie beside or below it.
- **Scope** (`ProjectScope`): the FR-167 signal and its key. A debug or
  symbol file is keyed by its content hash (SHA-1, which the debug file
  reader already computes), so a moved file finds its project; a debugger
  loaded binary by its content hash and load address; otherwise the machine
  type and the drive-1 disk's path at power-on, or the machine type alone.
  Automatic projects live under `%LOCALAPPDATA%\Casso\DebugProjects\`, one
  file per key; a named project (Save project as) lives where the user puts
  it, and the automatic file for its scope then points at it.
- **Store** (`DebugProjectStore`, core over `IFileSystem`): load, save and
  switch. A save is debounced to 2 seconds after the last change on the UI
  thread, built from the snapshot and the window's state, and written whole
  to a temporary file that replaces the old one (FR-168). An unreadable file
  is renamed with the date and replaced by an empty project.
- **Change sources**: `DebugSession`'s stop-condition, watch, bookmark,
  symbol and filter changes; the dock site's `onChanged`; the range sets and
  options. Each marks the project dirty and restarts the debounce.
- **Switching** (FR-169): `SYM LOAD`, `BLOAD` and power-on report their
  signal; a more specific signal switches projects after saving the old one,
  and the window offers to carry breakpoints, watches and bookmarks across.
- **Names** (FR-172): `Breakpoint` and `Watchpoint` gain `name`;
  `BreakpointInfo` carries it; the pane's Name cell is editable in place;
  `BPNAME id "text"` is a Casso engine command reachable in every mode and
  listed among AppleWin mode's commands; `BPSAVE` and AppleWin's `SAVE` never write
  names.
- **Startup commands** (FR-171): stored with their mode; run in order through
  the console's path after the project loads; edited in File > Project
  settings... on the editable grid (FR-208).

### Applesoft at source level (story 22)

R-043 holds the interpreter's addresses and layouts, each to be confirmed
against the fixture ROMs before it is relied on; data-model.md, "Applesoft".

- **Reader** (`CassoEmuCore/Debugger/Basic/ApplesoftProgram.h/.cpp`, core):
  everything through the target's side-effect-free peek. The program from
  TXTTAB along the line links; the detokenizer over the token table ($80 to
  $EA) prints as LIST does; CURLIN gives the line being run ($FFxx in direct
  mode); the variable table from VARTAB to ARYTAB and the arrays from ARYTAB
  to STREND, decoded as reals (five-byte floating point, printed as Applesoft
  prints them), integers and string descriptors; the GOSUB and FOR frames
  read from the 6502 stack by their tokens.
- **Presence**: an Applesoft ROM in the machine (not the original Apple ][
  Integer ROM), a program chain that links up within memory, and the
  interpreter running; otherwise the BASIC view says so (edge case).
- **Stops** (`BasicStepper`, core): an internal fetch hook at the
  interpreter's statement entry (NEWSTT), armed only while a BASIC step, a
  BASIC breakpoint or an error stop is set, so a program with none runs as
  before (FR-064). A statement step stops at the next entry; a line step at
  the next entry whose CURLIN differs; step out at the entry after the
  frame's RETURN. A line breakpoint is a table entry of a new BASIC kind
  matched at the entry; a variable breakpoint watches the variable's value
  bytes, found from the variable table and found again after CLEAR, RUN or
  NEW moves it, and holds the stop until the next entry so the statement
  completes. An error stop is a fetch hook at the error handler's entry,
  reporting the error code and CURLIN. Reverse continue stops on them as on
  every other kind.
- **Window**: a BASIC source document (the listing, the line mark, gutter
  breakpoints, stepping keys as in a source document), a BASIC variables
  pane and a BASIC stack pane; Kind "BASIC", Trigger "Line" or "Variable
  change" in the Breakpoints pane.

### C source coloring (FR-178)

A pure C lexer in CassoCore (keywords, types, number, character and string
literals, comments, preprocessor lines) feeding the source view's existing
color runs; files are taken as C by the extension the debug file gives
(`.c`, `.h`) and the theme's source colors are shared with assembly's.

### Window entry points (story 23)

R-044 holds the owner's tiering of ui-entry-point-audit.md; each entry calls
the handler's function, never a typed command (the console is not the
plumbing).

- **Menus and toolbars** in `DebuggerWindowMenuBar.cpp` and
  `DebuggerCommands.cpp`: Load file into memory (and its toolbar button),
  Save memory to file, Project items, Step filters, Find in Memory panes,
  bookmarks next and previous.
- **Context menus**: the code pane (mark as data, Add symbol here, Never step
  into, Edit instruction, Replace with NOPs, Toggle bookmark, stopwatch
  here, Call subroutine here), the Memory pane (Fill, Copy to, Compare with,
  Add as pointer watch), the Stack pane (Push, Pop), the call stack (Never
  step into), the Registers pane (reset Trip).
- **New panes**: Symbols, Bookmarks, Profiler, data directives list, BASIC
  variables and BASIC stack, each in the View menu and the dock layout.
- **Editable grid** (`Dxui/Widgets/DxuiEditableGrid.h/.cpp`): a list view with
  a trailing hint row, cell editing in place with per-column validation and
  required columns, row deletion and gripper reordering; the range list and
  the startup command list use it.
- **Disassembly selection**: Ctrl+drag keeps the selection in its starting
  column; rows selected for Replace with NOPs come from the selection's rows.
- **Status bar**: a speed section with a popup of 1x, 2x and Maximum, posting
  the same command the emulator's speed menu posts.
- **Keys** (FR-203, FR-204): `DebuggerKeySchemes::Action` gains an action per
  pane and the console; the Visual Studio map binds Visual Studio's chords,
  every map binds Ctrl+`; every menu item takes its accelerator text from the
  active map, as the Debug menu does.
- **Removed**: `BENCHMARK`'s verbs and handler.

### Disassembly, docking and heat map (FR-200 to FR-209)

- **DISASM** settings move from `ConfigHandlers` to the view options the
  window and the snapshot share, so the command and the check boxes set one
  value; Tools > Options shows them. Branch rows take a direction glyph in
  the gutter. The result annotation appends the instruction's cycles from
  the CPU's timing table, with a branch's taken and not-taken counts and the
  page-crossing cycle where the mode allows one.
- **Docking**: a title drag hands the dock site the whole group, whose
  floating window follows the pointer from the threshold on (the T279 drag);
  a press on any tab arms its drag; a torn-off group's float takes its
  docked size. The pane look is the T710 work.
- **Heat map**: the Default set is created on first use with the built-in
  ranges; the range list moves to `DxuiEditableGrid`; the FR-209 defects are
  fixed where they lie (edit box corners in `DxuiTextInput`'s frame, the
  range grid's widths, the watch row's single click, the tooltip delay, the
  range set name over the drop-down, the range titles over short ranges'
  address labels).

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

1. **Window (story 4, P1)**: merge `origin/033-casso-explorer` first; then dense
   `DxuiListView` metrics and the mono face
   (R-029); symbolic disassembly with per-line labels and operand symbols
   (FR-010a); breakpoint, watch and stack panes sized by content; the
   keyboard schemes (R-031); the snapshot cadence and SC-009 measurement.
2. **Memory editing (story 5, P1)**: `MemoryEditModel`, editing in
   `DxuiHexView`,
   the four memory windows, `DebugMemoryView::Patch` and the ROM patch paths
   (R-024), per-window undo.
3. **Source-level (story 6, P1)**: SHA-1; `DebugFileWriter` in both
   assemblers with the FR-066 fixtures; `DebugFileReader` for cc65 and the
   Merlin listing (capture the `PUT` listing first, R-022); `SourceService`
   and the path lists; the source pane; the stack-pointer step-over and
   source steps (R-033); line breakpoints.
4. **Docking (story 7, P2)**: `DxuiPaneLayout` and its serialization
   (R-028); `DxuiTabGroup`; `DxuiDockSite` over `DxuiSplitter`; drag with
   `DxuiDockDropZones`; `DxuiDockedWindow` (R-023); auto-hide; keyboard
   docking; layout save and restore with the fallback monitor.
5. **Trace (story 8, P2)**: the extended `TraceEntry`, the bus's all-pages
   watched mode, `HISTORY`, the trace pane, `HISTORY SAVE`, SC-013.
5a. **Call stack and step filter (stories 13 and 15)**: `CallStackRecorder`
    in the hook the trace shares; `StackWalker`; `CallStack` and `CALLS`;
    the pane; the FR-069 break fixtures (SC-018); `StepFilter`, `SKIP`, and
    the step-into check in `RunStopHook` (SC-020).
6. **Device panels (story 9, P2)**: `IDiagnosticsProvider`, the seven
   providers, `DiagnosticsPane`, the three visuals, the panel menu.
7. **Expression and value breakpoints (story 10, P3)**: `IF` on address and
   watch breakpoints, `MemoryValue` kind, the I/O-read rejection.
8. **Profiling (story 11, P3)**: the CPU's penalty byte, the profile tables,
   `PROFILE LIST [ADDR]` and `PROFILE SAVE`.
9. **GSSquared mode (story 12, P3)**: capture the fixtures (R-027);
   `GSSquaredParser`, `GSSquaredFormatter`, `OUTPUT`, the command sweep,
   `BPR` without spaces (FR-015a); the bank rule and the IIgs refusals
   (FR-022b); the engine-marker rule across all modes (FR-014).
9a. **WinDbg mode (story 14, P3)**: `WinDbgParser`, `WinDbgFormatter`, the
    `!` marker, the excluded-family reply, the command sweep (SC-019), and
    `k` over the call stack.
9b. **Disk breakpoints (story 20, P3)**, tasks.md Phase 29, scheduled with
    the owner: measure the read test on real DOS 3.3 and ProDOS boots first
    (T712); then the parts shared with 040 (field definitions and `??`
    matching, the per-drive head, the write hook), unless 040 has merged
    them; then the controller's record and listener slot, the debugger's
    tracker, table entries, `BPDISK`, matching, vectors, logical sectors,
    disk values, reports, reverse continue, the pane, the scenario boots and
    the cost measurements.
9c. **Review defects and docking (FR-205, FR-209)**: the review's defects
    first, then the tear-off drag that moves the group, any tab draggable,
    the float at its docked size.
9d. **Window entry points and keys (story 23, P2)**: Load and save binary
    with the toolbar button; the pane keys and menu accelerators; the
    editable grid, then the range list and the Default set on it; the
    context menus, the new panes and the status-bar speed control; the
    DISASM wiring, branch glyphs and cycle annotation; C coloring;
    `BENCHMARK` removed.
9e. **Debug projects (story 21, P1)**: names on breakpoints and `BPNAME`; the
    project model over the existing scripts; scope keys; the store with its
    debounce and whole-file replace; switching and carrying over; Save as,
    Open and Project settings with startup commands.
9f. **Applesoft (story 22, P2)**: confirm R-043's addresses against the
    fixture ROMs; the reader and detokenizer, checked against LIST on
    fixture programs; the stepper and its stops; the BASIC kind; the
    document, variables and stack panes.
10. **Release**: the README screenshot on the Mockingboard speech demo
    (FR-065), `docs/Debugger.md` for every story, the changelog, the
    pre-merge gate, SC-008 measured, and 033 on master before 035 merges.

## Risks

- **The Merlin `PUT` listing layout** was unknown; resolved 2026-09-18 by
  capture (R-022): `PUT` lines are marked `>` and numbered from 1 again, so
  the listing maps to itself and the importer treats a `>` line as a line of
  the listing.
- **The call stack is a heuristic in two of its three modes.** The recorder
  is exact for ordinary code and says where it lost the thread; the walk is
  a guess and is labeled as one. The risk is a frame shown with the wrong
  label; SC-018's fixtures provoke every break in FR-069 to keep the labels
  honest.
- **GSSquared's output has to be captured from a build of GSSquared**
  (R-027). If it cannot be built, the documented examples fix the format
  and the deviation is recorded; SC-016 then measures against those.
- **035 carries 033's unmerged Dxui.** Merging `origin/033-casso-explorer` brings
  199 commits, and 033's own merge waits on its owner's review. The cost is
  ordering only: 035 cannot merge before 033, which the release phase
  already requires; if 033's history is rewritten, 035 re-merges.
- **Docking is the largest UI piece in the tree.** It is kept honest by the
  `DxuiPaneLayout` model being pure data with its own tests for every operation
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
- **The read test is checked by reading code, not run** (R-041). It
  compares the instruction that read a data field's checksum nibble with the
  one that read its address prologue's first nibble, which separates DOS
  3.3's RDADR16 from READ16 and the boot ROM's search loop from its body
  loops. If a loader reads a checksum nibble with the instruction that found
  the address field, FR-145's default misses its reads. T712 measures it on
  both boots, the ROM's reads included, before anything is built on it, and
  the spec changes first if it fails.
- **Each drive's own head changes the emulation** (GH #135). A program that
  relied on the shared position, which no real drive has, would now behave as
  on hardware. The scenario suite's DOS 3.3 and ProDOS boots run on it, since
  `Disk2Controller` is one of its trigger areas.
- **The nibble record grows every snapshot** by 2 KB that changes while the
  drive is busy. T732 measures history's bytes per emulated second against
  R-040's table; the record shrinks or packs if the growth is out of line.
- **Sharing with 040 sets an order of merges.** Whichever merges second
  adopts the first's field definitions, matcher, head and write hook, and a
  merge of the two may conflict in `Disk2Controller`; the tasks that build
  them adopt 040's when it is on master.

- **Applesoft's internals** (R-043): the zero-page pointers, the statement
  entry and the error entry are from published maps and must be confirmed in
  each machine's fixture ROM before the stepper relies on them; a wrong one
  shows as a test failure, never as a silent misstep.
- **Project scope**: a boot disk used for many programs gives one shared
  project, which the owner accepted; a debug file's content key misses a
  program rebuilt with changes, which then starts a fresh project, offered
  the old one's breakpoints through FR-169's carry-over.
- **Two windows saving one project**: the last save wins; whole-file replace
  keeps the file readable.
- **Always-on logs** (FR-198): bounded buffers keep memory flat; their cost
  per speaker toggle is measured against SC-008 before they ship.

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
├── WinDbgParser.h/.cpp                      # NEW: fourth mode, `!` engine marker
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
├── RunStopHook.h/.cpp                       # CHANGE: stack-pointer step-over; source-step modes; step filter; feeds the call recorder
├── CallStack.h/.cpp                         # NEW: CallStackRecorder, StackWalker, CallStack (frames, breaks, mechanism)
├── StepFilter.h/.cpp                        # NEW: symbols, addresses, ranges a step into steps over
├── TraceController.h/.cpp                   # NEW: HISTORY on/off/save; window of entries
├── ProfileTable.h/.cpp                      # NEW: per-opcode, penalty and per-address tables
├── IDiagnosticsProvider.h                   # NEW: rows, bits, visual payloads
├── DiagnosticsSnapshot.h                    # NEW
├── DebugViewSnapshot.h/.cpp                 # NEW: what the window draws, built per frame on the CPU thread
├── GSSquaredFormatter.h/.cpp                # NEW: third output format
├── WinDbgFormatter.h/.cpp                   # NEW: fourth output format
├── Source/
│   ├── SourceService.h/.cpp                 # NEW: resolve, hash, match, path lists, text
│   └── SourcePathList.h/.cpp                # NEW: per-program and global lists in prefs
└── Handlers/
    ├── SourceHandlers.h/.cpp                # NEW: SOURCE, line breakpoints, source steps
    ├── TraceHandlers.h/.cpp                 # NEW: HISTORY
    ├── CallStackHandlers.h/.cpp             # NEW: CALLS, CALLS MODE, SKIP
    ├── ExecutionHandlers.cpp                # CHANGE: PROFILE LIST [ADDR], SAVE, RESET; BPV value form
    └── ConfigHandlers.cpp                   # CHANGE: OUTPUT

CassoEmuCore/Ui/Debugger/
├── DebuggerWindow.h/.cpp                    # CHANGE: a DxuiDockedWindow owning the panes
├── DebuggerKeySchemes.h/.cpp                # NEW: the three DxuiKeyMap tables
├── DebuggerViewState.h/.cpp                 # CHANGE: reads DebugViewSnapshot
├── Panes/
│   ├── DebuggerPane.h                       # NEW: base; draws from snapshot; sends commands through host
│   ├── DisassemblyPane.h/.cpp               # NEW: labels, symbolic operands, click-to-break
│   ├── SourcePane.h/.cpp                    # NEW
│   ├── MemoryPane.h/.cpp                    # NEW: one per memory window, over DxuiHexView
│   ├── MemoryEditModel.h/.cpp               # NEW: the IDxuiHexSource: pending, completion, undo, refusal
│   ├── TracePane.h/.cpp                     # NEW
│   ├── CallStackPane.h/.cpp                 # NEW: frames with provenance, break rows, double-click to call site
│   ├── DiagnosticsPane.h/.cpp               # NEW: rows and bits
│   ├── MemoryMapBar.h/.cpp, DiskHeadView.h/.cpp, MeterBar.h/.cpp  # NEW: visuals
│   └── RegistersPane, StackPane, WatchesPane, BreakpointsPane, ConsolePane  # NEW: split out of the first window
└── (floating panes are DxuiDockedWindows; nothing debugger-specific)

Dxui/                                        # DxuiSplitter, DxuiHexView, DxuiTextView, DxuiCommandRouter come from 033-casso-explorer
├── Core/DxuiPanel.h/.cpp                    # CHANGE: DetachChild returns the unique_ptr
├── Core/DxuiKeyMap.h/.cpp                   # NEW: named chord -> command id table; swappable per window
├── Core/DxuiPaneLayout.h/.cpp               # NEW: split/tab tree, floating, auto-hide, JSON, fallback monitor
├── Core/DxuiDockDropZones.h/.cpp            # NEW: drop-zone geometry and hit test, pure data
├── Widgets/DxuiListView.h/.cpp              # CHANGE: per-instance row height, padding, font
├── Widgets/DxuiHexView.h/.cpp               # CHANGE: IDxuiHexSource::WriteBytes; per-nibble overwrite caret
├── Widgets/DxuiTabGroup.h/.cpp              # NEW: tabs over a set of children
├── Widgets/DxuiDockSite.h/.cpp              # NEW: tree -> DxuiSplitter/DxuiTabGroup; drag; auto-hide; Dock To
├── Window/DxuiWindow.h/.cpp                 # CHANGE: consults the active DxuiKeyMap after the standard router
└── Window/DxuiDockedWindow.h/.cpp           # NEW: DxuiWindow whose content is one DxuiDockSite

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
├── WinDbgParserTests.cpp, WinDbgFormatterTests.cpp, WinDbgCommandSweepTests.cpp  # SC-019; excluded families reply
├── CallStackTests.cpp                       # SC-018: recursion, inline parameters, interrupt; every FR-069 break
├── StepFilterTests.cpp                      # SC-020: COUT skipped in both granularities
├── EngineMarkerTests.cpp                    # FR-014: every engine command through every mode's marker
├── OutputFormatTests.cpp                    # MODE sets both, OUTPUT one
├── ExpressionBreakpointTests.cpp            # IF, value breakpoints, I/O rejection
├── TraceTests.cpp                           # SC-013, access fields, off = no cost path taken
├── ProfileTableTests.cpp                    # buckets, penalties, per-address, SAVE
├── DiagnosticsProviderTests.cpp             # each device's rows; SC-015 cadence
├── MemoryEditModelTests.cpp                 # SC-012; grouping; text column; undo; I/O refusal; ROM patch
├── DebugMemoryViewPatchTests.cpp            # ROM patch lands where the CPU reads, survives $C028
├── DebuggerKeySchemesTests.cpp
└── DebugViewSnapshotTests.cpp

UnitTest/Dxui/
├── DxuiListViewMetricsTests.cpp
├── DxuiKeyMapTests.cpp
├── DxuiHexViewEditingTests.cpp
├── DxuiTabGroupTests.cpp
├── DxuiPaneLayoutTests.cpp                  # every operation; JSON round trip; SC-014 fallback
├── DxuiDockDropZonesTests.cpp
├── DxuiDockSiteTests.cpp
└── DxuiDockedWindowTests.cpp

UnitTest/Fixtures/Debugger/
├── GSSquared/                               # captured replies + LICENSE
├── Merlin/PI.ADD.LST                        # PUT listing capture + LICENSE
├── Sources/callstack.a65                    # SC-018 fixture: recursion, inline parameters, IRQ, each break
└── DebugFiles/                              # cc65 v2 samples, one from cc65 itself

docs/Debugger.md                             # CHANGE: every story
docs/Assembler.md                            # CHANGE: -g writes cc65 v2
README.md                                    # CHANGE: screenshot (FR-065)
```

Disk breakpoints (story 20, R-041), new and changed:

```text
CassoCore/Debugger/
├── DiskBreakSpec.h                          # NEW: what BPDISK was given (API type)
├── DebugCommand.h                           # CHANGE: DebugVerb::SetDiskBreakpoint, the parsed spec
├── AppleWinCommandTable.cpp                 # CHANGE: BPDISK in the Breakpoints family
├── CassoCommandReference.cpp                # CHANGE: BPDISK's syntax and description
└── AppleWinParser.cpp                       # CHANGE: the BPDISK grammar

CassoCore/                                   # shared with 040, built by whichever merges first
├── DiskMarkPattern.h/.cpp                   # NEW: three-nibble marks with ?? matching (pure)
└── DiskFieldKind.h                          # NEW: Sixteen or Thirteen

CassoEmuCore/Devices/Disk/                   # shared with 040, built by whichever merges first
└── DiskFieldFormat.h/.cpp                   # NEW: 16- and 13-sector field definitions, tables, checksums

CassoEmuCore/Machines/Apple2/Common/
├── IDisk2EventSink.h                        # CHANGE: OnWriteMode with an empty default
├── Disk2EventTee.h/.cpp                     # NEW: forwards each event to two sinks
├── Disk2NibbleRecord.h/.cpp                 # NEW: the saved record of recent nibbles
├── Disk2Controller.h/.cpp                   # CHANGE: debugger slot and dispatch; head per drive; write hook; record; the next state version
└── Disk2NibbleEngine.h/.cpp                 # CHANGE: its own head position; look-ahead on a copy of its state

CassoEmuCore/Shell/
├── MachineHost.h/.cpp                       # CHANGE: the executing instruction's address, for the record
├── MachineBuilder.cpp                       # CHANGE: wires that address into the controller
└── DiskManager.cpp                          # CHANGE: the drive widget reads its own drive's head

CassoEmuCore/Debugger/
├── DiskFieldTracker.h/.cpp                  # NEW: fields from the record; the read test
├── DiskBreakpointMonitor.h/.cpp             # NEW: the debugger's disk sink; matching; pending stop; DiskReport
├── DiskCallVector.h/.cpp                    # NEW: RWTS and driver vectors, followed; IOB and zero-page requests
├── DiskLogicalMap.h/.cpp                    # NEW: DOS 3.3 sectors and ProDOS blocks to physical sectors
├── DiskValueSymbols.h/.cpp                  # NEW: the DISK. values, from the target's disk state
├── BreakpointTable.h/.cpp                   # CHANGE: Disk entries
├── Reply.h                                  # CHANGE: BreakpointKind::Disk, StopReason::Disk, DiskReport
├── IDebugTarget.h, MachineDebugTarget.h/.cpp # CHANGE: debugger disk sink, disk state, record, look-ahead
├── DebugSession.h/.cpp                      # CHANGE: pending disk stop; report; hook filter; priming after history moves; DISK. values ahead of the symbol tables
├── ReplyJson.cpp, AppleWinFormatter.cpp, MonitorFormatter.cpp, GSSquaredFormatter.cpp, WinDbgFormatter.cpp  # CHANGE: disk kind and stop
├── Handlers/BreakpointHandlers.cpp          # CHANGE: BPDISK, listing, BPSAVE, BPEDIT, the confirmation line
└── Reverse/IReverseStopTest.h, ReverseStopTest.h/.cpp, Replayer.cpp  # CHANGE: disk sink in replays; boundary disk changes

CassoEmuCore/Ui/Debugger/
├── DebuggerWindowBreakpoints.cpp            # CHANGE: New > Disk event...
├── BreakpointDialog.h/.cpp                  # CHANGE: the Disk type
├── BreakpointColumns.h/.cpp                 # CHANGE: disk rows
└── BreakpointImport.h/.cpp                  # CHANGE: BPDISK lines are definitions

UnitTest/Devices/   Disk2NibbleRecordTests.cpp, Disk2PerDriveHeadTests.cpp, Disk2WriteHookTests.cpp, Disk2ControllerEventTests.cpp, Disk2StateTests.cpp, DiskFieldFormatTests.cpp, DiskMarkPatternTests.cpp
UnitTest/DebuggerTests/   DiskFieldTrackerTests.cpp, DiskBreakpointParserTests.cpp, DiskBreakpointTests.cpp, DiskCallBreakpointTests.cpp, DiskLogicalMapTests.cpp, DiskValueTests.cpp, and the formatter, JSON, handler, column, import and undo tests
UnitTest/EmuTests/   ReverseDiskBreakpointTests.cpp, Disk2NibbleEngineTests.cpp
ScenarioTests/   DiskReadProbeTests.cpp, DiskBreakpointBootTests.cpp
```

**Structure Decision**: the debug-file reader and writer, the line table and
the GSSquared parser depend on nothing but strings and CPU tables, so they
live in `CassoCore/Debugger/`. Docking, the key map and hex editing have
nothing debugger-specific in them, so they live in `Dxui`; the debugger
supplies its pane set, its three key tables and its hex source. Device
diagnostics are declared in the
debugger and implemented by each device, so a future machine's devices add
providers without touching the window. Every new file is listed by hand in
its `.vcxproj`.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| A per-instruction hook in `MachineHost`, the hottest loop in the emulator | Breakpoints, opcode breaks and stepping must stop before a given instruction executes | Checking once per `RunCycles` slice misses addresses inside the slice. Rewriting code bytes with `BRK` corrupts guest-visible memory and fails against ROM. The hook is a pointer test when unused (R-004) |
| A second headless machine builder path (`HeadlessMachineFactory`) | `CassoCli debug` must boot a real machine | Leaving it in UnitTest makes batch mode impossible. The factory is promoted, and `TestMachine` becomes a thin user of it, so there is still one builder |
| Routing every bus access through the watched-page path while the trace is on | The trace records each instruction's bus access, which only the bus sees | A second, trace-only bus path duplicates the slow path; a per-instruction effective-address recomputation in the hook cannot see the data byte. The cost exists only while on (R-025) |
| A docking layout model and three new Dxui controls | The spec asks for Visual Studio's arrangement operations (FR-038 to FR-044) | Fixed panes with splitters only cannot float, tab or auto-hide; a third-party docking library is not permitted by the constitution's allowlist |
| Writable ROM images (`PatchByte` on three classes) | FR-037: an edit to a ROM address patches what the CPU reads | Copying ROM into RAM at patch time breaks banking; refusing ROM edits leaves the memory editor unable to patch the Monitor, which AppleWin's can |
| A record of recent nibbles in `Disk2Controller`'s saved state, written on every nibble the CPU receives | Disk breakpoints must match fields the same way after a step back, a seek or at a replay's snapshot as in a forward run (FR-158, FR-159) | A tracker with no saved record starts blind at a snapshot and misses a field under way; saving a tracker's state with the machine would put breakpoint configuration into snapshot checksums (R-041) |
| A second listener slot and a tee on `Disk2Controller` | The Disk ][ debug window keeps its listener (FR-157), and its machine-switch reattach rewrites that slot | A list of sinks adds a loop to every event for a set of at most two; sharing the window's slot would lose the debugger's listener on every machine switch |
