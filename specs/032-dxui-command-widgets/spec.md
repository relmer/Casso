# Feature Specification: Dxui Command Widgets

**Feature Branch**: `032-dxui-command-widgets`

**Created**: 2026-09-10

**Status**: Draft

**Input**: User description: "Bring Dxui's command surfaces up to the model
WPF and WinUI use, so the upcoming Cassque file browser can be built on
them: one command object shared by menu items, toolbar buttons and context
menu items; a toolbar widget extracted from the emulator's CommandToolbar,
which is deleted; one dropdown widget that the menu bar, the toolbar's
pickers and right-click context menus all use. The emulator's window must
look and behave identically before and after."

## Background

Dxui grew its command surfaces one consumer at a time. The menu bar was
built with its own dropdown inside it. A second, poorer popup menu was
written later for right-click menus, holding only a label and a check. The
emulator's toolbar was then built inside the emulator rather than as a
widget, with its own copy of the button, tooltip and collapse mechanics and
a third private notion of what a dropdown is. The result is that the same
action, Reset for example, is declared once in the menu table and again in
the toolbar table, with its label, tip and enabled state kept in two places
that can disagree.

WPF and WinUI settled this long ago. A command is one object carrying its
label, icon, description, accelerator, action, and checked and enabled
state. A menu item, a toolbar button and a context menu item are each a view
of a command. A dropdown is one widget, whatever opens it. This feature
brings Dxui to that model. The emulator's chrome moves onto it with no
visible change, and the Cassque file browser, the next feature, is built on
it from the start.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Emulator chrome unchanged (Priority: P1)

An emulator user runs Casso after this feature ships. The menu bar, its
dropdowns, the command toolbar, the toolbar's pickers and flyout, and the
right-click menus on the debug panels look the same and do the same things.
Every menu item and toolbar button dispatches its command, checks and
enables the way it did, and reads the same label and tooltip. Keyboard
behavior in the menu bar, meaning mnemonics, Alt+letter, arrow keys,
Enter and Escape, is unchanged.

**Why this priority**: Everything here is a relocation. Identical behavior
is the whole acceptance bar for the emulator, and captures plus the
existing test suite are its proof.

**Independent Test**: Capture the screenshot matrix on the branch and on
master. The menu bar band and the toolbar band are pixel identical in each
theme. Each open menu bar dropdown shows the same rows, checks, disabled
rows and accelerator hints, sized to its content instead of a fixed width.
The existing unit test suite passes with no behavioral assertion changed.

**Acceptance Scenarios**:

1. **Given** the emulator at its default width in each theme, **When** the
   window is captured, **Then** the menu bar and toolbar bands match master
   pixel for pixel.
2. **Given** each top-level menu opened by click, **When** captured,
   **Then** the dropdown shows the same rows in the same order with the
   same separators, disabled rows, check marks and accelerator hints as
   master, at the same row height and font, and its width fits the widest
   row rather than a fixed value.
3. **Given** a menu is open, **When** the user presses Left, Right, Up,
   Down, Enter, Escape or a mnemonic letter, **Then** the result matches
   master.
4. **Given** the toolbar, **When** the window narrows, **Then** entries
   collapse to icons one at a time from the right at the same widths as
   master.
5. **Given** the theme picker, **When** opened, arrowed through, and
   dismissed, **Then** the theme previews and snaps back as on master.
6. **Given** a debug panel with a right-click menu, **When** it is opened
   and a row picked, **Then** the same action runs as on master.

---

### User Story 2 - One command, every surface (Priority: P1)

A maintainer declares an action once: its label, glyph, tooltip,
accelerator text, what it does, and how to compute its checked and enabled
state. The Machine menu's Reset item, the toolbar's Reset button and a
future context menu's Reset row each refer to that one declaration. Changing
the label or the enabled rule in that one place changes every surface.

**Why this priority**: This is the model the feature exists to install.
Without it the toolbar and the dropdown are still two tables that drift.

**Independent Test**: A unit test declares one command, places it in a
menu, a toolbar and a context menu, flips its enabled functor, and observes
all three surfaces report it disabled.

**Acceptance Scenarios**:

1. **Given** a command whose enabled functor returns false, **When** its
   menu item, toolbar button and context row are painted, **Then** all three
   draw disabled and none dispatches on click.
2. **Given** a command whose label functor changes, **When** any surface
   repaints, **Then** it shows the new label.
3. **Given** a command with checked state, **When** toggled, **Then** the
   menu item shows a check and a toggle toolbar button shows pressed.
4. **Given** the emulator's Reset, **When** the machine's display name
   changes, **Then** the toolbar tip and any other surface that quotes it
   update from the one command.

---

### User Story 3 - One dropdown, three openers (Priority: P2)

A maintainer opens the same dropdown widget from a menu bar title, from a
toolbar picker button, and from a right-click at a point. The content is a
list of commands with separators. Highlight, keyboard navigation, disabled
rows, check marks, accelerator hints, submenus, preview on highlight, and
commit on selection behave the same in all three.

**Why this priority**: Cassque's context menus need the menu bar's item
model on a right-click popup, and today no widget has both.

**Independent Test**: A unit test opens one dropdown from each of the three
anchors with the same command list and asserts identical row layout,
identical highlight movement per key, and identical dispatch on Enter.

**Acceptance Scenarios**:

1. **Given** a command list with a separator and a disabled row, **When**
   opened from a title, a picker and a point, **Then** the three dropdowns
   lay out identically.
2. **Given** an open dropdown, **When** Down skips a separator and a
   disabled row, **Then** the highlight lands on the next enabled row in all
   three cases.
3. **Given** a picker dropdown with a preview sink, **When** the highlight
   moves, **Then** preview fires; **When** dismissed, **Then** preview
   replays the row it opened on; **When** committed, **Then** commit fires
   once.
4. **Given** a row with a submenu, **When** hovered or Right is pressed,
   **Then** the submenu opens beside it and Escape closes only the submenu.

---

### User Story 4 - A second application hosts a toolbar (Priority: P2)

The author of another Dxui application declares a toolbar by listing
commands in groups, marks some entries as pickers or a flyout, and docks it
below the menu bar. They get the same responsive collapse, tooltips,
keyboard handling and visuals the emulator's toolbar has, without writing
any of it.

**Why this priority**: This is the reuse the toolbar extraction buys. The
first real consumer is the next feature; a test-only consumer proves the
interface here.

**Independent Test**: A unit test builds a toolbar from commands with no
reference to the emulator, plans it at several widths, and verifies which
entries carry labels, which tooltip is reported at a point, and which
command dispatches on a click.

**Acceptance Scenarios**:

1. **Given** five command entries, **When** planned at a width that fits
   all five labels, **Then** all five are labeled; at a width that fits
   four, **Then** only the rightmost is collapsed.
2. **Given** a flyout entry hosting a slider, **When** the pointer dwells,
   **Then** the flyout opens and slider changes reach the author's sink.
3. **Given** a toolbar with no emulator content, **When** compiled into the
   unit test project, **Then** it needs nothing from the emulator library.

---

### User Story 5 - Emulator content lives only in the emulator (Priority: P3)

A maintainer looking for the emulator's toolbar finds no toolbar class in
the emulator. They find a table of commands, a builder that fills a Dxui
toolbar from it, and two small classes for the two things only the emulator
draws: the printer status light and the input-device cluster. The menu
table and the toolbar table are the same table.

**Why this priority**: Maintainability payoff; it keeps the split from
drifting back.

**Independent Test**: The emulator's toolbar source files no longer exist.
The remaining chrome sources refer to commands, the LED and the input
cluster and to nothing about buttons, collapse or dropdown mechanics.

**Acceptance Scenarios**:

1. **Given** the emulator sources after the change, **When** a maintainer
   searches for the old toolbar class, **Then** it is gone.
2. **Given** a new emulator action, **When** added, **Then** it is one
   command declaration and one placement in the menu table and optionally
   the toolbar, with no drawing or layout code.

---

### Edge Cases

- **A command in the toolbar but not in any menu**, such as the volume
  flyout: allowed; the command object is the declaration, placement is
  separate.
- **A menu item with no command**, such as a separator: the item model has
  a separator kind that carries no command.
- **Two surfaces show one command with different labels**, for example a
  toolbar's short label versus a menu's full one: the command carries one
  label and an optional short label; the toolbar uses the short one when
  present.
- **A dropdown's submenu chain when the host resizes**: the whole chain
  closes, as the menu bar does today.
- **Keys while any dropdown is open**: they go to the dropdown and never to
  the emulated machine, as today.
- **A right-click menu opened near the screen edge**: the dropdown is kept
  inside the host client rect, as the menu bar's dropdowns are.
- **Width below the toolbar's last collapse point**: every entry is an icon
  and the strip shrinks no further; leftmost entries stay visible.
- **A flyout entry collapses while its flyout is open**: the flyout closes
  and the collapsed icon reopens it on the next dwell.

## Requirements *(mandatory)*

### Functional Requirements

**Command**

- **FR-001**: The UI library MUST provide a command object carrying an id,
  a label, an optional short label, a glyph, a tooltip, accelerator text, a
  dispatch action, and functors for checked, enabled and dynamic label.
- **FR-002**: Every surface that shows a command MUST take its label, glyph,
  tooltip, accelerator text, checked and enabled state from the command at
  paint time, never from a copy.

**Dropdown**

- **FR-003**: The UI library MUST provide one dropdown widget whose content
  is a list of items, each either a command reference, a separator, or a
  command with a submenu list.
- **FR-004**: The dropdown MUST render check marks, disabled rows,
  accelerator hints and submenu arrows, and MUST skip separators and
  disabled rows during keyboard navigation.
- **FR-005**: The dropdown MUST open anchored under a rect or at a point,
  MUST size its width to its widest row with a small minimum and never to a
  fixed value, MUST stay inside the host client rect, and MUST host itself
  through the window's popup pool when a host is supplied.
- **FR-006**: The dropdown MUST fire a highlight-change callback for
  previewing, a closed callback before the select callback with a
  committed flag, and MUST dispatch the selected command.
- **FR-007**: The menu bar MUST open the dropdown widget under each title
  and MUST no longer paint a dropdown of its own, while keeping mnemonics,
  Alt+letter, Left and Right swapping, and focus return.
- **FR-008**: The UI library MUST provide a one-call way to open the
  dropdown as a context menu at a point with a command list.

**Toolbar**

- **FR-009**: The UI library MUST provide a toolbar widget configured with
  an ordered list of entries, each a command reference plus a kind
  (command, toggle, picker, flyout), a group, and optional decoration or
  custom-entry hooks.
- **FR-010**: The toolbar MUST draw entries frameless until hovered or
  pressed, collapse labels one at a time from the right to fit a width,
  report its band height, and report a tooltip and anchor for a point.
- **FR-011**: Picker entries MUST open the dropdown widget with preview and
  commit sinks; flyout entries MUST open a hover flyout hosting an embedded
  control.
- **FR-012**: The toolbar MUST support an application-drawn decoration over
  an entry's icon and a custom entry that owns its width, layout, painting,
  tooltip and click, so a status light and an indicator cluster need no
  widget knowledge.

**Emulator**

- **FR-013**: The emulator's menu table and toolbar table MUST become one
  table of command objects, with placement in the menu bar and the toolbar
  declared separately from the commands.
- **FR-014**: The emulator's toolbar class MUST be deleted; the shell MUST
  hold the library toolbar directly, filled by a builder from the command
  table, with the printer light as a decoration and the input cluster as a
  custom entry.
- **FR-015**: The emulator's existing right-click menus MUST open through
  the library's context menu call.
- **FR-016**: The emulator's window MUST render and behave identically
  before and after, verified by captures and by the existing unit test
  suite. Existing tests that construct a removed type MUST be retyped at
  their construction sites only; no behavioral assertion in an existing
  test may change, and the command-table parity test MUST keep asserting
  that every command id is present exactly once.
- **FR-017**: Every new widget MUST be exercisable from the unit test
  project with no dependency on the emulator library.

### Key Entities

- **Command**: one action's declaration; id, label, short label, glyph,
  tooltip, accelerator text, dispatch, checked, enabled, dynamic label.
- **Dropdown item**: a command reference, a separator, or a command with a
  submenu.
- **Dropdown**: the widget; item list, anchor, highlight, open submenu,
  callbacks.
- **Menu bar**: titles with mnemonics; opens a dropdown per title.
- **Toolbar entry**: command reference, kind, group, decoration, custom
  entry; runtime hover, press and labeled state.
- **Toolbar**: entries, layout plan, pickers, flyout.
- **Emulator command table**: the single declaration of every emulator
  action, replacing the menu and toolbar tables.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The menu bar band and the toolbar band, cropped from the
  matrix captures and from each theme, are pixel identical between branch
  and master; every open dropdown carries the same rows and states as
  master and is sized to its content.
- **SC-002**: The existing unit test suite passes with no behavioral
  assertion changed; the only edits are construction-site retyping where a
  type was removed, and the parity test reading the new command table.
- **SC-003**: A command placed in a menu, a toolbar and a context menu shows
  one enabled change in all three, in a headless test.
- **SC-004**: A dropdown opened from a title, a picker and a point lays out
  identically for the same list, in a headless test.
- **SC-005**: The emulator's toolbar class no longer exists, and the
  emulator declares each action exactly once.
- **SC-006**: The library's toolbar, dropdown and command compile into the
  unit test project with no emulator dependency.

## Assumptions

- Per-entry label collapse stays as the toolbar's overflow behavior. A
  chevron overflow menu, as classic Win32 and WinUI do, is not part of
  this feature.
- Input routing stays as it is: the shell forwards mouse and key events to
  the widgets. Moving widgets onto the panel tree's own routing is out of
  scope.
- The volume slider stays shell-owned and is handed to the toolbar as its
  flyout control.
- The emulator's command dispatch stays a single handler taking a command
  id; a command's dispatch action calls it. The thread-routing rules and
  the message-driven screenshot script are untouched.
- The screenshot matrix is the pixel oracle at the default window size; the
  open dropdowns and the other themes are captured by hand against a master
  binary on the same machine and DPI.
- Cassque is a separate feature and this one is complete without it.
