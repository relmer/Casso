# Feature Specification: Debugger Window Refactor

**Feature Branch**: `044-debugger-window-refactor` (written on `035-debugger`; implemented on its own branch off master after 035 merges)

**Created**: 2026-10-17

**Status**: Draft

**Input**: Owner decision of 2026-10-17, after `/bigobj` was added so that `DebuggerWindow.cpp` compiles: restructure the debugger window so that each debug pane is its own class, with the debugger's behavior identical to the build before the refactor.

## Background

Measured 2026-10-17 on `035-debugger`:

- `DebuggerWindow.cpp` is 14,996 lines and `DebuggerWindow.h` 1,837; twelve
  `DebuggerWindow*.cpp` partial files add about 8,200 more. Together they are
  one class of about 25,000 lines with about 350 member variables.
- The partial files spread the code across files, not across classes: every
  pane's state is a `DebuggerWindow` member.
- Central dispatchers handle every pane by name: `OnMouse` is 484 lines,
  `OnKey` 343, `UpdateTooltip` 299, `AddListMenuItems` 264,
  `ConfigureDockSite` 247, `ConfigureWidgets` 241 and `ApplySnapshot` 219.
  About 6,400 lines of the main file are layout, routing, tips and focus, in
  parallel per-pane lists.
- Adding one bar to the Profiler pane took edits in seven separate places:
  creation, the pane's frame, placement, the mouse chain, tips, the menu
  tick list and the pane's list of controls.
- The object file for `DebuggerWindow.cpp` passed the compiler's section
  limit; `/bigobj` on that one file is a stopgap.

## User Scenarios & Testing *(mandatory)*

The users of this work are the debugger's maintainers. The user stories are
in build order, smallest first; each is a commit, or a short run of commits,
that builds, passes every test, and matches the build before the refactor on
screen.

### User Story 1 - A pane interface, with the tool panes moved onto it (Priority: P1)

A maintainer adds or changes a debug pane by writing one class, with no edit
to the window's dispatchers. Define the pane interface and the host
interface, and move the Symbols, Bookmarks, Profiler and Data ranges panes
onto them first, since they are the smallest and newest.

**Why this priority**: Every later story moves a pane onto the same
interfaces, so they must exist and be proven on real panes first.

**Independent Test**: The four tool panes match the build before the
refactor: their lists, bars, row menus, tips, the profiler's timeline and
flame graph, and their saved state across a restart. Their tests pass from
the pane classes, and the window's dispatchers mention none of the four.

**Acceptance Scenarios**:

1. **Given** the refactored window, **When** the Profiler pane is opened,
   profiling is turned on, the machine is paused and each view is chosen,
   **Then** each view, the timeline, the flame graph's zoom and tips, and the
   bar match the build before the refactor.
2. **Given** the refactored window, **When** its mouse, key, tip, snapshot
   and placement code is searched for a tool pane, **Then** no match is
   found; the window calls each tool pane through the pane list.

---

### User Story 2 - One helper for every pane's bar (Priority: P1)

A pane's toolbar is placed in the slot at the top of its pane, moves with the
pane into a floating window, and gets its mouse input and its tips through
one shared helper, not through a copy of that code per pane.

**Why this priority**: The per-pane bar code is repeated for the undo bars,
the color keys, the breakpoints bar, the heat map bars, the event log bars,
the memory bar, the console bar and the code and source bars; folding it
into one helper removes the largest single block of repetition, and every
later pane move uses it.

**Independent Test**: Every pane bar places, routes, ticks its menus and
draws its tips as in the build before the refactor, docked, floating and
slid out.

**Acceptance Scenarios**:

1. **Given** a pane with a bar floated into its own window, **When** its
   drop-down is opened and a row chosen, **Then** the result matches the
   build before the refactor.
2. **Given** the helper, **When** a pane's bar is added, **Then** the only
   edit is in the pane's own class.

---

### User Story 3 - The heat map, its ranges and the event logs as panes (Priority: P2)

Move the heat map with its two bars, the heat map ranges editor, and the
Disk II and input event logs into pane classes. The two event logs share one
base class in place of the window's parallel structures for them.

**Why this priority**: These panes have the most window code of their own
after the tool panes, and the event logs' shared base removes duplicated
filtering, replay and bar code.

**Independent Test**: The heat map, its ranges and both event logs match the
build before the refactor, including the heat map's recording while its pane
is open, range editing and undo, and the event logs' filters, severity
colors, replay and saved options.

**Acceptance Scenarios**:

1. **Given** the refactored window, **When** a heat map range is added,
   edited and deleted, **Then** the map and the ranges match the build
   before the refactor.
2. **Given** the refactored window, **When** the Disk II log's filters are
   changed and Casso is restarted, **Then** the restored filters match the
   ones set.

---

### User Story 4 - Memory windows, registers, stack, watches, breakpoints and call stack as panes (Priority: P2)

Move each of these into a pane class. A memory window's compare, fill and
find flows, and the watches' BASIC watch logic and undo history, become
classes of their own beside their pane's controller.

**Why this priority**: They are the middle-sized panes, with edit and undo
behavior that the move must reproduce exactly.

**Independent Test**: Editing a byte, a register, a stack entry or a watch,
undoing and redoing the edit, setting and editing breakpoints, and following
the call stack match the build before the refactor, across a restart where
they are saved.

**Acceptance Scenarios**:

1. **Given** a memory window, **When** a byte is edited, undone and redone,
   and a range is compared with another, **Then** each result matches the
   build before the refactor.
2. **Given** a watch on a BASIC variable, **When** the machine stops in the
   program, **Then** the watch's value matches the build before the
   refactor.

---

### User Story 5 - Source documents, disassembly views and console as panes (Priority: P3)

Move the source documents, the disassembly views and the console into pane
classes, each split further: a disassembly view into its view controller,
its gutter (breakpoints and the PC drag), its code edit commands, and its
branch arrow and operand tips; the console into its output view, its
command box with completion and history, its mode and its find.

**Why this priority**: They are the largest and most entangled panes, so
they move last, onto interfaces proven by the panes before them.

**Independent Test**: Stepping, gutter breakpoints, the PC drag, assembling
into memory, the branch arrow, operand tips, source following, and the
console's commands, completion, history, modes and find match the build
before the refactor.

**Acceptance Scenarios**:

1. **Given** the refactored window, **When** the PC's arrow is dragged to
   another line and the machine is stepped, **Then** the result matches the
   build before the refactor.
2. **Given** the console, **When** a partial command is completed, run and
   recalled from history, **Then** each result matches the build before the
   refactor.

---

### User Story 6 - The window as the panes' host (Priority: P3)

The window is left with the dock, the menus, the command bar, the status bar
and the list of panes. Its mouse, key, tip, snapshot and placement code are
loops over the panes. A tooltip coordinator, the mouse and key routing for
floating and slid-out panes, and the View menu built from the pane list are
classes of their own. `/bigobj` is removed.

**Why this priority**: It is the outcome the earlier stories build toward,
and it is checkable only once every pane has moved.

**Independent Test**: The project builds without `/bigobj`; no debugger
window source file is over about 3,000 lines; adding a pane takes its class
and one line in the pane list.

**Acceptance Scenarios**:

1. **Given** the finished refactor, **When** the project is built, **Then**
   no file has `/bigobj`.
2. **Given** a new pane class, **When** it is added to the pane list,
   **Then** it is listed in the View menu, takes its mouse input, keys and
   tips, and saves its state with no other edit to the window.

### Edge Cases

- A pane floated into its own window, slid out from an edge, or behind
  another tab routes mouse input, keys, tips and focus exactly as in the
  build before the refactor.
- A pane closed until first opened (the BASIC program pane and the tool
  panes) opens the same way, and its saved ` open=` token is read and
  written the same way.
- A snapshot from a running machine has less in it than one from a paused
  machine; each pane handles both as the build before the refactor does.
- A test that reads the window's private members moves to the pane class,
  or tests through the window's public behavior; no test is deleted to make
  a step pass.
- A change on master to a pane being moved is merged into the moved class;
  the work merges master early and often.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Each debug pane MUST be its own controller class behind one
  pane interface that creates and configures its widgets, adds itself to
  the dock, applies a snapshot, places its bar, routes a mouse event, takes
  a key, returns its tip at a point, adds its context menu rows, lists its
  controls, and saves and restores its state.
- **FR-002**: A pane MUST call the window only through a host interface:
  run an action, go to an address, the theme and the heat palette, the
  popup host, the scaler, and a repaint request.
- **FR-003**: The window MUST contain the dock, the menus, the command bar,
  the status bar and the list of panes; its mouse, key, tip, snapshot and
  placement code MUST be loops over the panes, with no pane mentioned by
  name.
- **FR-004**: Tooltip coordination, the mouse and key routing for floating
  and slid-out panes, the View menu's rows, and the placing, routing and
  tips of a pane's bar MUST each be a class of its own; the bar helper MUST
  serve every pane's bar.
- **FR-005**: A pane larger than about 1,500 lines MUST be split further,
  with pure logic out of its controller; the split of each pane MUST be
  settled when its task comes up, from what is in it.
- **FR-006**: The Disk II and input event logs MUST share one base class.
- **FR-007**: The debugger's behavior MUST match the build before the
  refactor: every existing debugger test MUST pass after each step, tests
  that read the window's internals MUST move to the pane classes, and the
  window MUST be exercised on screen after each step.
- **FR-008**: `/bigobj` MUST be removed from the project once the last pane
  has moved.

### Key Entities

- **Pane**: One debug pane's controller: its widgets, its state and its
  behavior, called by the window through the pane interface.
- **Pane host**: The window's calls available to a pane.
- **Pane list**: The panes the window owns, in View menu order.
- **Bar slot helper**: Places, routes and tips any pane's bar, docked or
  floating.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The project builds with `/bigobj` on no file.
- **SC-002**: No debugger window source file is over about 3,000 lines, and
  no pane class is over about 1,500.
- **SC-003**: Adding a new pane takes its class and one line in the pane
  list, with no other edit to the window, as a unit test with a trivial
  test pane demonstrates.
- **SC-004**: Every debugger test that passed before the refactor passes
  after each step, and no test is deleted to make a step pass.
- **SC-005**: After each step, the moved panes are exercised on screen,
  docked and floating, and match the build before the refactor.

## Assumptions

- The work starts after 035 merges to master, on its own branch, so 035's
  history is untouched and spec 041, built on 035, merges forward.
- No user-visible behavior, look, saved setting or file format changes; a
  setting's saved text is readable by the build before the refactor.
- The Dxui library's widgets change only where a move requires it.
- The debugger's thread model is the same; rendering off the UI thread is
  future work.
- Each step merges master first, since master has taken sweeping renames
  before.
