# Feature Specification: Drop Target Highlight

**Feature Branch**: `010-drop-target-highlight`

**Created**: 2026-05-26 (rewritten 2026-10-14)

**Status**: Not started

**Input**: User description: "While a supported file is dragged over the window, the whole window dims except for the drives that are drop targets. Those do not dim and are surrounded by a pulsing aura. When the drop cursor hovers over one of these drop targets, the aura doubles in intensity, and a pulsing golden ray lights it up like a beam of sunlight, with sparkles that twinkle on the drive until the cursor leaves. It applies to the current presentations of the drives: the Skeuomorphic theme's 3D desk scene (and its 3D drive band when the monitor is off), the flat themes' 2D drive widgets, and full-screen mode (the slide-in drive strip in either theme)."

## Background

The first draft of this spec (2026-05-26) planned a blur over a flat dimming
overlay and a hover tint on 2D drive widgets. Neither the overlay nor the tint
exists now: during a drag, the only feedback is the copy or no-drop cursor.
The drives are drawn four ways today:

- **Skeuomorphic, windowed**: 3D drives (and the cassette recorder) in the desk
  scene, or, with the theme's monitor turned off, a band of 3D drives.
- **Skeuomorphic, full screen**: a slide-in strip of the 3D drives and the
  recorder.
- **Flat themes (Dark Modern, Retro Terminal), windowed**: a band of 2D drive
  widgets, with the recorder in the same row.
- **Flat themes, full screen**: the same 2D widgets in the slide-in band.

This spec replaces the first draft. It dims rather than blurs.

## Clarifications

### Session 2026-10-14

- Q: What should a drag look like? → A: The whole window dims except for the
  drives that are drop targets. Those stay bright inside a pulsing aura. The
  drive under the drop cursor gets an aura of double intensity, a pulsing
  golden ray of light onto it like a beam of sunlight, and sparkles that
  twinkle on it until the cursor leaves.
- Q: Which presentations? → A: All current ones: the desk scene and its 3D
  drive band, the flat themes' 2D widgets, and full screen in either theme.
- Q: In full screen, the drive strip is hidden until summoned. Does a drag
  bring it in? → A: Yes. A drag of a supported file brings the strip in, and
  it slides away again when the drag ends.
- Q: Does the recorder get the same treatment for tape files? → A: Yes. A tape
  file lights only the recorder; the drives dim with the rest of the window.
- Q: Is the aura only a pulse? → A: No. The aura dances around the drive or
  the recorder continuously, in addition to pulsing.
- Q: What happens with an unsupported file? → A: Windows' standard no-drop
  cursor, as today, and nothing dims.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See where a disk can go (Priority: P1)

A user drags a disk image from File Explorer over the Casso window. The window
dims, and the drives stay bright, each inside a dancing, pulsing aura, so the
places the disk can land are obvious at a glance.

**Why this priority**: This is the guidance the feature exists for.

**Independent Test**: In each of the four presentations, drag a `.dsk` over
the window and confirm everything but the drives dims and each drive's aura dances and pulses.
Drag it back out and confirm the window returns to normal.

**Acceptance Scenarios**:

1. **Given** a machine with two drives, **When** a disk image is dragged over
   any part of the window, **Then** the whole window dims except the two
   drives, and each drive is surrounded by a dancing, pulsing aura.
2. **Given** the highlight on, **When** the drag leaves the window or is
   canceled, **Then** the dimming and the auras fade out and the window looks
   exactly as it did before the drag.
3. **Given** a file the drives do not accept (a `.txt`), **When** it is dragged
   over the window, **Then** nothing dims, no aura appears, and the cursor is
   Windows' no-drop cursor, as today.
4. **Given** a machine with one drive, **When** a disk image is dragged over
   the window, **Then** only that drive is bright and has an aura.

---

### User Story 2 - See which drive the disk will land in (Priority: P1)

As the user moves the disk over drive 1, that drive lights up: its aura
doubles, a golden beam of light pulses down onto it, and sparkles twinkle on
it. Moving to drive 2 moves the beam and the sparkles there. Releasing the
mouse mounts the disk in the lit drive.

**Why this priority**: A drop into the wrong drive is the most likely mistake,
and the hover state marks the right one before the release.

**Independent Test**: Drag a disk image across drive 1, then drive 2, then off
both, and confirm the beam, the doubled aura and the sparkles follow the
pointer and stop when it leaves a drive. Drop on drive 2 and confirm it
mounts there.

**Acceptance Scenarios**:

1. **Given** the highlight on, **When** the drop cursor enters drive 1's drop
   area, **Then** drive 1's aura rises to double intensity, a pulsing golden
   beam lights it, and sparkles twinkle on it.
2. **Given** drive 1 lit, **When** the cursor moves to drive 2, **Then** drive
   1 returns to its base aura alone and drive 2 lights up.
3. **Given** a lit drive, **When** the cursor leaves it for a part of the
   window that is not a drop target, **Then** the beam and sparkles stop and
   both drives have their base aura.
4. **Given** a lit drive, **When** the user releases the mouse, **Then** the
   disk mounts in that drive exactly as it does today, and the highlight fades
   out.

---

### User Story 3 - The same in full screen (Priority: P2)

A user running full screen drags a disk image onto the screen. The drive strip
is normally hidden until summoned, so the drag brings it in: the strip slides
into view as soon as the drag enters the screen, its drives highlighted as in
a window, and slides away again when the drag ends.

**Why this priority**: Full screen is a supported mode, but drags into it are
less common than into a window.

**Independent Test**: In full screen in each theme, drag a disk image onto the
screen and confirm the drives in the strip get the same dimming, aura, beam
and sparkles as in a window, and that a drop mounts the disk.

**Acceptance Scenarios**:

1. **Given** full screen with the drive strip hidden, **When** a disk image is
   dragged onto the screen, **Then** the strip slides into view and the screen
   dims except the drives in the strip, which glow as in a window.
1a. **Given** the strip brought in by a drag, **When** the drag leaves the
   screen, is canceled or drops, **Then** the strip slides away again, unless
   the user had summoned it before the drag.
2. **Given** a drive in the strip lit by the cursor, **When** the user drops,
   **Then** the disk mounts in that drive.

---

### User Story 4 - Tapes and the recorder (Priority: P3)

A user drags a tape recording onto a ][+ with the recorder connected. The
recorder gets the same treatment as a drive does for a disk: everything else
dims, including the drives, which do not take tapes; the recorder glows in its
dancing aura; and with the cursor over it, it gets the doubled aura, the beam
and the sparkles.

**Why this priority**: Tapes are used far less than disks.

**Independent Test**: Drag a `.wav` over the window on a ][+ and confirm the
recorder, and only the recorder, is highlighted.

**Acceptance Scenarios**:

1. **Given** a ][+ with the recorder connected, **When** a `.wav` is dragged
   over the window, **Then** the recorder is the only bright, glowing device.
2. **Given** the cursor over the recorder, **Then** it gets the doubled aura,
   the beam and the sparkles, and a drop inserts the tape as today.
3. **Given** a //c, which has no recorder, or a recorder that is disconnected,
   **When** a `.wav` is dragged over the window, **Then** nothing dims and the
   cursor is the no-drop cursor.

---

### Edge Cases

- A drag of several files at once: the highlight follows the first file, the
  one a drop uses today.
- A machine with no Disk II drives: a disk image dragged over the window
  causes no dimming, since there is no target.
- A write-protected disk in a drive, or an empty drive: both are drop targets
  as today, and both are highlighted.
- The desk scene orbited, zoomed or panned so a drive is partly or wholly off
  screen: the part in view is highlighted; a drive wholly off screen cannot be
  hovered.
- A drive behind another device from the current camera angle: the aura and
  beam are drawn for the part of the drive that is visible.
- The Windows "Show animations" setting turned off: the aura neither dances nor
  pulses, the beam is steady, the sparkles are drawn without twinkling, and in
  full screen the strip appears and disappears without sliding.
- A window resized, moved to another monitor or switched in or out of full
  screen during the drag: the highlight follows the drives' new positions.
- A modal dialog open: Casso does not accept drops, and nothing dims.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: While a file that a drive would accept is dragged over the
  window, the whole window MUST dim except the drives that are drop targets
  for it.
- **FR-002**: Each drop target MUST stay undimmed and MUST be surrounded by an
  aura that dances around it continuously (its glow shifting and flickering
  around the device's outline) and pulses, while the drag is over the window.
- **FR-003**: The drive under the drop cursor MUST have its aura at double
  intensity, a pulsing golden beam of light falling onto it from above like a
  sunbeam, and sparkles that twinkle on the drive. All three MUST stop when
  the cursor leaves that drive.
- **FR-004**: The dimming, the auras, the beam and the sparkles MUST fade in
  when the drag enters and fade out when it leaves, is canceled or drops,
  leaving the window exactly as it was before the drag.
- **FR-005**: Which drives are targets and which drive a drop lands in MUST
  come from the existing drop handling, so the highlighted drive is always
  the drive the disk mounts in.
- **FR-006**: The effect MUST apply in the desk scene, in the Skeuomorphic
  theme's 3D drive band, in the flat themes' 2D drive widgets, and in the
  full-screen drive strip of either theme.
- **FR-007**: In full screen, a drag of a supported file MUST bring the drive
  strip into view, and the strip MUST slide away again when the drag ends,
  unless the user had summoned it before the drag.
- **FR-008**: For a tape file, the recorder MUST be the drop target and get the
  same treatment as a drive gets for a disk image; the drives MUST dim with the
  rest of the window.
- **FR-009**: A file no drive or recorder accepts MUST NOT dim the window or
  draw any aura, and the cursor MUST be Windows' no-drop cursor, as today. A
  supported file over a part of the window that is not a drop target MUST also
  get the no-drop cursor, as today.
- **FR-010**: With the Windows "Show animations" setting off, the aura, beam
  and sparkles MUST be drawn without motion.
- **FR-011**: The effect MUST NOT change drop behavior: accepted file types,
  the drive a drop goes to and what a drop does all stay as today.
- **FR-012**: The effect MUST NOT slow emulation or drop the display below its
  normal frame rate while it is on.

### Key Entities

- **Drop target**: a drive or the recorder (FR-008) that accepts the
  dragged file; it has a base state (aura) and a hovered state (doubled aura,
  beam, sparkles).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In each of the four presentations, a first-time user dragging a
  disk image can point to every drive it can be dropped on within 1 second of
  the drag entering the window.
- **SC-002**: In 20 drops alternating between drive 1 and drive 2, every disk
  mounts in the drive that was lit at the moment of release.
- **SC-003**: The highlight is fully visible within 0.2 seconds of the drag
  entering the window and fully gone within 0.3 seconds of it leaving.
- **SC-004**: While the highlight is on, the display runs at its normal frame
  rate and emulation at its normal speed.
- **SC-005**: One frame after the highlight has faded out, the window is
  identical to a frame taken before the drag began.

## Assumptions

- "The whole window" includes the emulated picture, the menu bar, the toolbar
  and the title bar.
- The dim darkens the rest of the window to about 40% of its brightness, and
  the aura pulses about once every 1.5 seconds; both are tuned by eye in the
  running app.
- The beam falls from the top of the window down onto the hovered drive, in
  screen space, whatever the desk scene's camera angle.
- The highlight colors are the same in every theme: a warm white aura and a
  golden beam.
