# Feature Specification: Desk Scene Arrangement

**Feature Branch**: `009-desk-scene-arrangement`

**Created**: 2026-05-25 (rewritten 2026-10-14)

**Status**: Not started

**Input**: User description: "Let the user reposition the Disk II drives and the cassette recorder (tape deck) in the Skeuomorphic theme's 3D desk scene. Today the scene places them at fixed positions: the monitor is centered on the drives' lids, and the recorder lies flat on the desk to the right of the stack. The user wants to arrange them differently on the desk."

## Background

This spec first covered a dockable 2D drive bar, a 2D monitor frame and a
default theme per machine. The 3D desk scene (spec 018, 1.21.0) replaced the 2D
monitor frame and the 2D drive band in the Skeuomorphic theme, and spec 013
removed the chrome layout those features extended. The remaining scope is the
desk scene's version of moving the drives: arranging the drives and the
recorder on the desk.

Today the scene's arrangement is fixed. The monitor is centered on the
drives' lids, the drives sit side by side under it, and the recorder lies flat
on the desk to the right of the stack. On the //c, which has no cassette port,
there is no recorder. The camera is fitted to contain the whole arrangement.

## Clarifications

### Session 2026-10-14

- Q: What earlier scope remains in this spec? → A: Only the ability to
  reposition the drives and the tape deck in the desk scene. The 2D drive bar
  docking, the 2D monitor frame and the per-machine default theme are dropped.
- Q: How is a device moved? → A: An arrange mode, toggled from the View menu.
  While it is on, an Alt+drag on a drive or the recorder moves it, and the
  devices' other controls are off. Zoom, pan and orbit work through their
  usual gestures, during a move as well, so the user can look from any
  angle before placing a device. An infobar lists what each modifier does
  while arrange mode is on.
- Q: Where does a device go when the scene is fitted tightly around the
  arrangement? → A: Arrange mode makes room. The fitted view has no
  space beside the monitor for a drive, and with the drives stacked to one
  side no space below it, so arrange mode frames free desk space around the
  arrangement on every side.
- Q: Where can devices go? → A: On the desk, and stacked: drives on each other
  and under the monitor. Nothing can be placed on top of the recorder, because
  its top must be clear for it to open. The monitor is not moved by the user.
- Q: How does a dragged device line up? → A: It snaps. It tends to snap to any
  other object's boundary lines and its horizontal and vertical center lines,
  and to the desk plane. A drive tends to snap against the side of another
  object, and is slightly sticky at alignment with an object's corner: drive 1
  under the monitor left-aligned to the monitor's left edge, drive 2 under it
  right-aligned to the right edge.
- Q: Where is the desk? → A: The desk plane is the bottom of the lowest object.
  While a device is dragged, it is the bottom of the lowest object not being
  dragged: with one drive under the monitor, grabbing the drive puts the desk
  plane at the monitor's bottom.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Move a drive or the recorder on the desk (Priority: P1)

A user with the Skeuomorphic theme wants the recorder on the left of the
monitor instead of the right, or wants the two drives on the desk beside the
monitor rather than under it, the way their own Apple II was set up. They turn
on arrange mode from the View menu, which steps the view back to reveal free
desk around the monitor, orbit the view to see the desk from the side, and
Alt+drag the device to the new spot, where
it snaps into line with the monitor or the other devices. It stays where they
put it, the camera
refits so the whole desk is in view, and every control on the moved
device (drive doors, recorder keys, volume wheel, tape and disk labels, drag
and drop of disk and tape files) works the same.

**Why this priority**: This is the feature. The other stories refine it.

**Independent Test**: On a //e with two drives and the recorder connected,
move the recorder to the left of the monitor and drive 2 to the right of it.
Confirm both appear where they were put, the whole arrangement is in view,
drive 2's door opens from its notch, and a `.wav` dropped on the recorder
loads.

**Acceptance Scenarios**:

1. **Given** the default arrangement and arrange mode on, **When** the user
   Alt+drags the recorder to the left of the monitor, **Then** the recorder
   lies flat on the desk plane at the new spot.
2. **Given** arrange mode off and a moved drive, **When** the user clicks its
   door notch, drops a disk image on it or opens its right-click menu, **Then**
   each acts on that drive exactly as in the default arrangement.
3. **Given** arrange mode on, **When** the user Alt+drags a drive, **Then** the
   drag moves it and does not open its door; Alt+dragging the recorder does not
   press its keys or turn its volume wheel.
3a. **Given** arrange mode on, **When** the user drags without Alt, turns the
   wheel or uses the compass, **Then** the view orbits, zooms or pans as it does
   outside arrange mode, and no device moves.
3b. **Given** a device being Alt+dragged, **When** the user turns the wheel or
   changes the view, **Then** the view changes and the device stays under the
   pointer at the new view.
3c. **Given** arrange mode turned on, **Then** the view steps back so free desk
   is visible on every side of the arrangement, and an infobar lists what
   each modifier does; **When** arrange mode is turned off, **Then** the infobar
   closes and the view refits to the arrangement.
3d. **Given** an Alt+drag that ends, **When** Alt is released, **Then** the
   menu bar does not activate.
4. **Given** a drag in progress, **When** the device would overlap another
   device, **Then** it does not settle overlapping; it stops at the nearest
   position clear of the others.
5. **Given** a drive dragged over the top of the recorder, **When** it is
   released, **Then** it does not settle on the recorder.
6. **Given** drive 1 dragged near the monitor's lower left corner, **When** it
   comes within the snap distance, **Then** it snaps under the monitor with its
   left edge aligned to the monitor's left edge, and stays there against a
   small further drag.
7. **Given** a drive dragged near another object's edge or center line,
   **When** it comes within the snap distance, **Then** it snaps into line, and
   dragging past the snap distance releases it.
8. **Given** one drive under the monitor, **When** the user grabs that drive,
   **Then** the desk plane becomes the monitor's bottom for the drag, and the
   drive snaps to it.
9. **Given** a move that ends where a drive's door has no room to open,
   **Then** the drive is placed far enough from other devices for its door to
   open.

---

### User Story 2 - The arrangement is kept (Priority: P2)

A user arranges the desk once and expects it to look the same next time,
including after switching to another machine and back.

**Why this priority**: An arrangement that resets on every launch is not worth
making.

**Independent Test**: Arrange a //e's desk, quit, relaunch: the arrangement is
the same. Switch to a ][+ and back: the //e's arrangement returns.

**Acceptance Scenarios**:

1. **Given** an arranged desk, **When** Casso is relaunched, **Then** each
   device is where the user left it.
2. **Given** an arranged //e desk, **When** the user switches to another
   machine and back, **Then** the //e's arrangement returns, and the other
   machine has its own arrangement.
3. **Given** a saved arrangement that a later change makes impossible (a drive
   removed, the recorder disconnected), **When** the scene is drawn, **Then**
   the remaining devices are at their saved positions and nothing overlaps.

---

### User Story 3 - Put the desk back (Priority: P3)

A user who has moved things around wants the original arrangement back in one
step.

**Why this priority**: A safety net; without it a bad arrangement has to be
undone one move at a time.

**Independent Test**: Arrange the desk, choose the reset command, and confirm
the default arrangement returns and is the one saved.

**Acceptance Scenarios**:

1. **Given** an arranged desk, **When** the user chooses the command to reset
   the arrangement, **Then** the default arrangement returns.
2. **Given** a reset arrangement, **When** Casso is relaunched, **Then** the
   default arrangement is drawn.
3. **Given** a moved camera (orbit, pan or zoom), **When** the user resets the
   view with Ctrl+0, **Then** only the camera resets; the arrangement stays.

---

### Edge Cases

- A device moved so far from the monitor that the scene becomes very wide: the
  camera contains the whole scene, so the devices get smaller. The
  allowed area of the desk is bounded so the monitor's picture stays usable.
- A second drive added to a machine whose desk was arranged with one: the new
  drive takes a free default spot without overlapping anything.
- The recorder connected again after being disconnected: it returns to its
  saved spot if that spot is free, otherwise to the nearest free spot.
- The fullscreen drive strip: it lays out its devices in its own row and is not
  affected by the desk arrangement.
- The flat themes (Dark Modern, Retro Terminal): they have no desk scene and
  are not affected.
- Arrange mode turned off mid-drag: the drag ends and the device settles at
  the nearest valid position.
- A drive dragged out from under the monitor: the monitor does not move. The
  desk plane is the bottom of the lowest object, so with nothing under it the
  monitor is on the desk.
- A drive released in the air with nothing under it: it settles onto the
  object below it or onto the desk plane, whichever is higher.
- The //c's drive, whose door latch rises above its lid: nothing settles in
  the latch's path, as FR-004 requires for any door.
- A small window: the free desk FR-002d frames makes the devices small. The
  user can zoom in on the area they are working in, or maximize the window.
- Alt pressed or released partway through a drag: pressing Alt over a device
  starts a move; releasing it ends the move as if the button were released.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: In the Skeuomorphic theme's desk scene, the user MUST be able to
  move each Disk II drive and the cassette recorder to another position on the
  desk.
- **FR-002**: The View menu MUST have an arrange mode toggle. While it is on,
  an Alt+drag on a drive or the recorder MUST move that device, and the
  devices' other pointer controls (doors, keys, the volume wheel, labels) MUST
  NOT respond. While it is off, devices MUST NOT move.
- **FR-002a**: In arrange mode, zoom, pan and orbit MUST work through their usual
  gestures, including during an Alt+drag. A device being dragged MUST stay
  under the pointer while the view changes.
- **FR-002b**: While arrange mode is on, an infobar MUST list what each
  modifier and gesture does (Alt+drag moves a device; the camera gestures
  change the view).
- **FR-002c**: Releasing Alt after an Alt+drag MUST NOT activate the menu bar.
- **FR-002d**: Turning arrange mode on MUST frame the view to include free desk
  on every side of the arrangement, at least the width of the widest movable
  device beside it and the height of the tallest one below and above it, and
  MUST allow zooming out at least that far. Turning it off MUST refit the
  view to the arrangement.
- **FR-003**: A drive MUST be placeable on the desk plane, on another drive or
  under the monitor. The recorder MUST be placeable on the desk plane or on a
  drive. Nothing MUST be placeable on top of the recorder, whose top must be
  clear for it to open. The monitor MUST NOT be movable by the user.
- **FR-004**: A device MUST NOT settle overlapping another device, on top of
  the recorder, or in the travel of a drive door or the recorder's lid.
- **FR-005**: The desk plane MUST be the bottom of the lowest object. During a
  drag, it MUST be the bottom of the lowest object not being dragged.
- **FR-005a**: A dragged device MUST snap, within a snap distance, to every
  other object's boundary lines and horizontal and vertical center lines, and
  to the desk plane. Dragging past the snap distance MUST release it.
- **FR-005b**: A dragged drive MUST snap against the side of another object.
  At a corner alignment (its edge aligned to that object's edge, as drive 1
  left-aligned and drive 2 right-aligned under the monitor), it MUST release
  only after a slightly longer drag than at other snap lines.
- **FR-005c**: A device released with nothing under it MUST settle onto the
  object below it or onto the desk plane, whichever is higher.
- **FR-006**: The camera MUST NOT refit while arrange mode is on, so the view
  does not move under the pointer. Outside arrange mode, the fitted view MUST
  contain the whole arrangement, as it does for the default arrangement.
- **FR-007**: Every interaction a device has in the default arrangement MUST
  work the same at any position: doors, keys, the volume wheel, labels, the
  right-click menu, hover text and drag and drop of disk and tape files.
- **FR-008**: The arrangement MUST be saved per machine and restored on launch
  and on switching back to that machine.
- **FR-009**: A command MUST reset the current machine's arrangement to the
  default. Ctrl+0 (reset view) MUST reset the camera only.
- **FR-010**: When a saved arrangement no longer fits the machine (a device
  added or removed), the scene MUST draw the devices present without overlap,
  keeping saved positions where they remain valid.
- **FR-011**: The fullscreen drive strip and the flat themes MUST NOT change.

### Key Entities

- **Desk arrangement**: per machine, the position of each drive and of the
  recorder, including what each one rests on. Absent until the user first
  moves something; the default arrangement applies until then.
- **Snap line**: an object's left, right, top and bottom boundary, its
  horizontal and vertical center line, and the desk plane.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can move the recorder from the right of the monitor to the
  left in under 10 seconds, on the first attempt, counting turning arrange
  mode on and off.
- **SC-001a**: A user can place drive 1 under the monitor left-aligned and
  drive 2 under it right-aligned, each aligned to within 1 mm, without
  adjusting either after the snap.
- **SC-002**: After 20 random moves of each device on a //e with two drives and
  the recorder, no two devices overlap and every drive door opens fully.
- **SC-003**: After any arrangement, the whole scene is in view at the
  window's size, with no device clipped at any edge.
- **SC-004**: An arrangement survives a relaunch and a machine switch with every
  device within 1 mm of where it was left.
- **SC-005**: With the default arrangement (no moves made), the scene is
  identical to today's, frame for frame.

## Assumptions

- The default arrangement is today's fixed arrangement, and it stays the
  default for every machine.
- Positions are saved per machine model in the user's preferences, not in the
  machine's configuration file.
- The //c, which has no recorder, arranges its drives and monitor only.
- Devices rotate with the scene's orbit only; turning one device on its own is
  out of scope.
- Arrange mode does not resize or maximize the window; the free desk comes
  from stepping the view back (FR-002d).
- A drag moves a device left and right and up and down as seen from the front.
  Front-to-back depth follows the stacking rules, with each device's front on
  the shared front plane as today.
- The snap distance and the corner release distance are tuned against the
  running scene, at a few pixels at the window's scale.
- Moving a device is a desk-scene change only; it has no effect on emulation.
