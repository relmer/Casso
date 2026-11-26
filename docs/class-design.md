# Class Design and Splitting Large Classes

These rules apply to every new class, and to every redesign of a class that
has grown too large. They came out of the owner's reviews of the Casso
Explorer window split (spec 033) and the debugger's pane host redesign (spec
035).

## The limit

A `.cpp` file stays under about 2,000 lines. Measure with
`(Get-Content <file>).Count`; `Measure-Object -Line` skips blank lines and
undercounts. A class headed past the limit is split while its spec is in
progress, not at the end, so that manual testing isn't repeated after a
refactor.

## Principles

### 1. Split into components, not files

A class past the limit becomes several classes, each with its state and one
job. Spreading one class across several `.cpp` files isn't a split.

Example: `EmulatorShell` delegates to `ShellChrome`, `ShellDeskScene`,
`ShellDisks` and the other classes in `CassoEmuCore/Shell/Components`. Each has
a reference to the shell and its part of the state.

### 2. Scoped interfaces, not friends

A component calls the class it came from, and its sibling components,
through small interfaces sized to what it calls. Design the interfaces first.
Write the reason beside any `friend` declaration.

Example: the debugger's single `IDebuggerPaneHost` was replaced by small
interfaces such as `IPaneRouting`, `IPaneNavigator`, `IPaneCommands`,
`IPaneDialogs`, `IPaneFocus`, `IPaneTips` and `IPaneBars`, all in
`PaneHostInterfaces.h`. Each pane is constructed from a
`DebuggerPaneParts` with references to those interfaces and to
`LatestSnapshot`, and to nothing else in `DebuggerWindow`.

### 3. UI elements and helpers are different kinds of class

- A class that draws and has `OnXxx` handlers is a UI element. It derives from
  the matching Dxui base (`DxuiPanel`, `DxuiListView`, `IDxuiControl`).
  Examples: `HeatMapView`, `SoundBoardView`.
- A class that only coordinates is a manager or controller. It has no window.
  Examples: `ExplorerCommandManager`, `CallStackController`,
  `MemoryBarCommands`, `DisassemblyBranchArrows`.
- A class written as every variant of something becomes a manager that
  creates and manages the variants. Examples: `DisassemblyGroup`,
  `SourceDocumentGroup` and `MemoryWindowGroup` manage their individual views.

### 4. Logic goes to core

Whatever a unit test can call goes in `CassoCore` or `CassoEmuCore` as plain
code with data in and data out: layout math, state machines, formatting,
decisions. Only drawing and input stay in the windowed class. The test is
"can the `UnitTest` project call it?", never "is it portable?". See "Architecture:
Thin Exe, Rich Testable Core" in `.github/copilot-instructions.md`.

Examples: `SoundBoardLayout`, `HeatStrip` and `DisassemblyCells` for layout;
`CallRecordKeeper` for call stack state.

### 5. No forwarders

When a function moves to a new class, every caller is changed to call the new
class, and the old method is deleted. A method that only passes the call
along isn't allowed.

### 6. A split doesn't change behavior

- Each split is a separate commit, with no feature or fix in it.
- Tests move with the code and test the component, not the whole window.
- Record the line counts before and after in the commit or the spec's task.

### 7. Layout and alignment

- Order within a class: friends, constructor and destructor, public methods,
  protected, private. `OnXxx` handlers are grouped together.
- Headers have few comments, and identifiers are chosen to be clear without
  them. Detailed comments go at the implementation.
- Every declaration block aligns its columns, function declarations included:
  type, the `*` or `&` column, name, the `(` of argument lists, `=`, and `{ }`.
  CheckStyle doesn't check function declaration columns, so check them
  yourself.
- Window message IDs and timer IDs are enums, and timer IDs come from one
  app-wide header so they can't collide.
- Text styles and padding constants used in several places go in a shared
  settings class for that use, not in each widget.

## Steps for a split

1. Measure the file and list its members by job.
2. Propose the components and their interfaces, and get the owner's approval
   before moving code.
3. Move one component at a time. Build and run its tests after each.
4. Change every caller to the new class, and delete the old methods.
5. Move or rewrite the tests to test the component.
6. Measure again, run `scripts/CheckStyle.ps1`, and check alignment yourself.
7. Commit each split separately, with a subject such as
   `refactor(debugger): Move the call stack menu into its controller`.
