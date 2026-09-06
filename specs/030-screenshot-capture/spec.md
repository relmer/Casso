# Feature Specification: Screenshot capture modes, file output, and metadata

**Feature Branch**: `claude/screenshot-save-format-location-7eee56`

**Created**: 2026-09-05

**Status**: Draft

**Input**: Design settled in conversation before this spec was written. Closes GH #132
(*Copy screenshot captures the raw framebuffer, not the CRT-processed picture*).

## Why this exists

Casso's Screenshot command puts the unprocessed guest framebuffer on the clipboard and
nothing else. Two things are wrong with that, and they are separate complaints:

1. **It does not look like Casso.** The whole CRT chain -- scanlines, bloom, color
   bleed, gamma, phosphor persistence -- runs on the GPU, and the framebuffer read never
   sees any of it. Neither does the desk scene. Someone sharing a screenshot of a
   phosphor look gets a picture with no phosphor look in it. That is GH #132.
2. **It is not a file.** A clipboard image survives exactly until the next copy. There
   is no way to keep a screenshot without pasting it into something else first.

This feature fixes both, and adds the metadata that makes a shared screenshot
self-describing.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Capture what is actually on screen, and keep it (Priority: P1)

A user has a machine running with a monitor and theme they like the look of. They press
the Screenshot button. A PNG appears in their Screenshots folder showing what they were
looking at -- monitor, glass, phosphor, drives -- and the same image is on the clipboard
for immediate pasting. No dialog appeared, the machine never paused, and nothing was
covered by a menu bar or a toolbar.

**Why this priority**: This is the whole complaint in GH #132 plus the missing file, and
it is the only story that has to ship for the feature to be worth having. The other
stories refine it.

**Independent Test**: Take one screenshot with the default settings. Confirm the file
exists, opens, shows the CRT-processed scene, excludes app chrome, and matches the
clipboard contents.

**Acceptance Scenarios**:

1. **Given** a machine running under a theme with a visible CRT look, **When** the user
   invokes Screenshot, **Then** a PNG is written to the Screenshots folder showing the
   scene with its CRT effects, and the same image is on the clipboard.
2. **Given** the frame-rate and scene-pose readouts are switched on, **When** the user
   invokes Screenshot, **Then** neither readout appears in the captured image.
3. **Given** the scene compass is visible, **When** the user invokes Screenshot,
   **Then** the compass does not appear in the captured image.
4. **Given** any capture, **When** the file has been written, **Then** a notice names
   the file that was written.
5. **Given** the machine is running, **When** the user invokes Screenshot, **Then** the
   emulated machine keeps running and the command returns without waiting on the user.

---

### User Story 2 - Choose what the screenshot contains (Priority: P2)

Three different jobs want three different pictures. Someone showing off their desk scene
wants the whole thing. Someone reporting "the scanlines are wrong" wants the picture with
its effects but no furniture. Someone extracting pixel art, or reporting "this pixel is
the wrong color", wants the exact framebuffer with nothing done to it. The user picks
which one the Screenshot command produces, once, in Settings.

**Why this priority**: The default serves the common case, so this is a refinement rather
than the feature. But without it the raw framebuffer capture -- which is the only correct
answer for pixel-exact work -- becomes unreachable, and today it is the only behavior
there is.

**Independent Test**: Switch between the three modes and take one capture in each.
Confirm three visibly different images with the documented content and dimensions.

**Acceptance Scenarios**:

1. **Given** `scene` is selected, **When** the user captures, **Then** the image contains
   the scene viewport and excludes the menu bar, command toolbar, title band and drive
   band.
2. **Given** `crt` is selected, **When** the user captures, **Then** the image is the
   picture with its CRT effects applied and contains no scene furniture and no chrome.
3. **Given** `raw` is selected, **When** the user captures, **Then** the image is exactly
   560x384 regardless of window size, and carries no CRT effects.
4. **Given** `raw` is selected and a monochrome monitor is active, **When** the user
   captures, **Then** the image carries that monitor's phosphor color.
5. **Given** any mode is selected, **When** Casso is restarted, **Then** the same mode is
   still selected.

---

### User Story 3 - A shared screenshot explains itself (Priority: P3)

A screenshot arrives attached to a bug report. Without the reporter typing anything, the
file says which Casso version produced it, which machine and monitor were running, which
capture mode it came from, and -- for a scene capture -- the exact view angle and the CRT
parameters in force. The reader can reproduce the setup instead of asking for it.

**Why this priority**: It costs nothing at capture time and pays off only when something
goes wrong, but it is the difference between a reproducible report and a round trip. It
also removes the one reason the scene-pose readout is printed over the picture.

**Independent Test**: Capture in each mode, then read the metadata back out of each file
and check it against the table in FR-020.

**Acceptance Scenarios**:

1. **Given** a `scene` capture, **When** the file's metadata is read, **Then** it carries
   the Casso version, the machine name, the capture time, the mode, the monitor, the
   scene pose and the CRT parameters.
2. **Given** a `raw` capture, **When** the file's metadata is read, **Then** it carries
   no scene pose and no CRT parameters.
3. **Given** any capture, **When** the file's metadata is read, **Then** it contains no
   filesystem path and nothing identifying the host machine or user.
4. **Given** a scene capture, **When** the pose is read from metadata and applied,
   **Then** the view matches the one the screenshot was taken from.

---

### User Story 4 - Control where screenshots land, or stop saving them (Priority: P4)

A user who does not want files piling up turns saving off and keeps the clipboard copy. A
user who wants them somewhere specific -- a synced folder, a project directory -- points
Casso at it and can open that folder from Settings.

**Why this priority**: The default destination is correct for most users, and the feature
works fully without this. It exists so that "it writes files now" is not something done
*to* the user.

**Independent Test**: Turn saving off, capture, confirm the clipboard still receives the
image and no file is written. Turn it back on, change the folder, capture, confirm the
file lands in the new folder.

**Acceptance Scenarios**:

1. **Given** file saving is off, **When** the user captures, **Then** the clipboard
   receives the image and no file is written.
2. **Given** a custom folder is set, **When** the user captures, **Then** the file is
   written there.
3. **Given** file saving is off, **When** the settings are shown, **Then** the folder
   controls are visibly disabled.

---

### Edge Cases

- **A theme with no desk scene is active and `scene` is selected.** Compact and flat
  themes never draw the desk scene, so there is no scene to capture. The capture is the
  scene viewport region regardless, which under those themes holds the CRT-processed
  picture. The mode is not silently switched and no error is raised -- the user gets the
  viewport, which is what the mode promises.
- **Every CRT effect is switched off and `crt` is selected.** The result is the picture
  at window resolution with no effects, which is correct and is not the same image as
  `raw` (different dimensions).
- **The window is minimized.** There is nothing rendered to read back. `scene` and `crt`
  cannot be satisfied; the command reports that it cannot capture rather than writing a
  black or stale image. `raw` is unaffected and still works.
- **The window is very small.** `scene` and `crt` produce a correspondingly small image.
  That is what the user is looking at, so it is not corrected.
- **The Settings sheet or another dialog is open over the window.** Dialogs are separate
  windows and are not part of the captured region, so they do not appear in the image.
- **A file of the intended name already exists.** A numeric suffix is appended until a
  free name is found, matching the existing print-output behavior.
- **The destination folder does not exist, or has been deleted since it was configured.**
  It is created. If it cannot be created or written to, the capture reports the failure
  and the clipboard copy still succeeds.
- **The clipboard cannot be opened** (another application is holding it). The file is
  still written; the failure is reported and does not prevent the save.
- **Ten captures in quick succession.** Ten distinct files, no dialogs, no lost captures.
- **A capture is taken while the emulated machine is paused.** The picture is whatever is
  on screen, which is the paused frame. Nothing special happens.

## Requirements *(mandatory)*

### Functional Requirements

#### Capture modes

- **FR-001**: The Screenshot command MUST offer three capture modes, identified by the
  stable tokens `scene`, `crt` and `raw`.
- **FR-002**: `scene` MUST capture the scene viewport region: the desk scene, CRT glass,
  monitor housing and 3D drives as rendered, with CRT effects applied.
- **FR-003**: `scene` MUST NOT include the menu bar, command toolbar, title band or drive
  band.
- **FR-004**: `crt` MUST capture the emulated picture with the CRT chain applied --
  scanlines, bloom, color bleed, gamma and phosphor persistence -- and no scene furniture
  and no chrome.
- **FR-005**: `raw` MUST capture the emulated framebuffer at exactly 560x384, unaffected
  by window size, with no CRT processing. The monitor's phosphor tint is part of the
  framebuffer and is therefore present.
- **FR-006**: `scene` MUST be the default for a user who has never chosen a mode.
- **FR-007**: For the capture image only, Casso MUST hide the overlays that describe the
  application rather than the machine: the scene compass, the frame-rate readout, the
  scene-pose readout and the mouse-capture notice. They MUST be restored immediately
  afterwards.
- **FR-008**: Capture MUST NOT pause, slow or otherwise disturb the emulated machine, and
  MUST NOT require the user to wait on or dismiss anything.

#### Output

- **FR-010**: Every capture MUST place the image on the system clipboard, in every mode,
  preserving today's clipboard behavior.
- **FR-011**: Every capture MUST additionally write an image file, unless the user has
  turned file saving off.
- **FR-012**: The file format MUST be PNG. The user is NOT offered a format choice.
- **FR-013**: The file MUST be written without prompting the user for a location.
- **FR-014**: The default destination MUST be a `Casso Screenshots` folder under the
  user's Pictures folder, created on demand, mirroring the existing print-output
  destination.
- **FR-015**: The filename MUST identify Casso and carry the capture date and time, in
  the form `Casso YYYY-MM-DD HHMMSS.png`. The word "screenshot" is deliberately absent:
  the folder supplies it, and what has to survive the file being dragged elsewhere is the
  application's name.
- **FR-016**: A name collision MUST be resolved by appending a numeric suffix until a
  free name is found, using the same policy as print output.
- **FR-017**: After a successful write, Casso MUST show a notice naming the file written.
- **FR-018**: A clipboard failure MUST NOT prevent the file write, and a file-write
  failure MUST NOT prevent the clipboard copy. Each failure MUST be reported.
- **FR-019**: Screenshots MUST NOT declare a physical resolution. Casso presents the
  picture at square pixels, so a square-pixel image already matches the application, and
  declaring anything else would make the file disagree with what the user saw.

#### Metadata

- **FR-020**: Every written file MUST carry embedded text metadata exactly as specified
  by **[contracts/screenshot-metadata.md](contracts/screenshot-metadata.md)**, which
  defines the entries, their per-mode emission, and their values.

  That document is the single authority and is deliberately not restated here. It is a
  published contract -- keywords may not be renamed or repurposed once released -- and a
  second copy of the table is the obvious place for the two to drift apart.

  In summary: seven entries for `scene`, six for `crt` (no scene pose), five for `raw`
  (no scene pose, no CRT parameters).

- **FR-021**: `Software`, `Source`, `Creation Time` and the three `Casso ` entries MUST
  use the standard text-metadata mechanism of the image format, so that ordinary image
  tools can read them without knowing anything about Casso.
- **FR-022**: `Source` MUST be the machine's own display name as the machine declares it,
  not an internal identifier.
- **FR-023**: `Casso Monitor` MUST be the identifier under which that monitor's CRT
  settings are stored, so a reader can look the user's adjustments up by it directly.
- **FR-024**: `Casso Scene Pose` MUST be written in the same form the on-screen pose
  readout uses, so a pose read from a file and a pose read from a picture are the same
  text.
- **FR-025**: Metadata MUST NOT contain any filesystem path, and MUST NOT contain
  anything identifying the host machine, its user, or its hardware. A screenshot is
  routinely attached to a public issue.
- **FR-026**: Image dimensions MUST NOT be duplicated into text metadata; the image
  format already records them.

#### Settings

- **FR-030**: The screenshot settings MUST be reachable from the Settings sheet and MUST
  NOT be placed on the Display page, which covers CRT display effects only.
- **FR-031**: The existing Printing page MUST be retitled to cover both subjects and MUST
  present Printing first, screenshots second, so an existing user looking for printer
  settings still lands on them.
- **FR-032**: The capture mode MUST be presented as a radio group in the order `scene`,
  `crt`, `raw` -- default first -- with descriptive text for each option, because the
  three are not self-explanatory from a label alone.
- **FR-033**: A control MUST allow file saving to be turned off while leaving the
  clipboard copy in place.
- **FR-034**: The destination folder MUST be settable, with a way to browse for it and a
  way to open it. These controls MUST be disabled when file saving is off.
- **FR-035**: The three settings MUST be the only exposure of this feature. The command
  keeps its single toolbar button, single menu item and single shortcut, all of which
  follow the selected mode.

#### Persistence

- **FR-040**: The capture mode, the save-file toggle and the destination folder MUST
  persist across restarts as global (not per-machine) preferences.
- **FR-041**: The capture mode MUST be stored as a stable text token, never as a position
  or index, so that reordering the radio options in a later release cannot silently
  repoint an existing user's setting.
- **FR-042**: An empty stored folder MUST mean "the default destination", so that the
  default can move in a later release without stranding users on a stale path.
- **FR-043**: Preferences written by a newer Casso MUST survive being loaded and re-saved
  by an older one, consistent with the existing preferences contract.

### Key Entities

- **Capture mode**: Which of three pictures the Screenshot command produces. One of
  `scene`, `crt`, `raw`. Persisted globally; default `scene`.
- **Screenshot file**: A PNG in the destination folder, named for Casso and the moment it
  was taken, carrying the metadata record below.
- **Screenshot metadata record**: The set of text entries embedded in a screenshot,
  varying by capture mode per FR-020. It is a published contract: files already written
  must remain readable, and entries may be added but not repurposed.
- **Destination folder**: Where screenshot files are written. Empty means the default.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A screenshot taken with default settings is visually indistinguishable from
  what the user sees in the scene viewport at that moment, apart from the four
  deliberately hidden overlays.
- **SC-002**: Taking a screenshot costs the user no interaction beyond the single button
  press, hotkey or menu item -- zero dialogs, zero prompts, zero decisions.
- **SC-003**: Ten screenshots taken in succession produce ten distinct files with no
  overwrite and no lost capture.
- **SC-004**: The emulated machine loses no more than one frame of display time per
  capture, and never stops running.
- **SC-005**: Given only a screenshot file, a reader can state the Casso version, the
  emulated machine, the monitor and color mode, the capture mode, and -- for a scene
  capture -- the exact view angle and CRT parameters, without asking the person who took
  it.
- **SC-006**: No screenshot file discloses a filesystem path, a user name, a host name or
  any hardware identifier.
- **SC-007**: A user can reproduce a reported scene-render fault from the screenshot
  alone, by reading the pose out of the file and restoring that view.
- **SC-008**: A user who does not want screenshot files can stop them being written, in
  one control, without losing the clipboard copy.
- **SC-009**: GH #132 is closed: the default screenshot carries the CRT effects and the
  scene, and the raw framebuffer remains available as an explicit choice.

## Assumptions

- **The scene viewport is the right boundary for `scene` mode.** "The whole window"
  would include the menu bar, command toolbar and drive bands, which the user explicitly
  does not want in a screenshot. The viewport is the part that shows the machine.
- **The hidden-overlay rule is "hide what describes the application".** The compass, the
  frame-rate readout, the scene-pose readout and the mouse-capture notice all describe
  Casso rather than the emulated machine. A side effect is that scene captures no longer
  depend on which diagnostics the user has switched on, so two captures of the same view
  are the same image.
- **A brief flicker as the overlays drop for the capture frame is acceptable** -- it
  reads as a shutter rather than as a fault.
- **`scene` in a theme that draws no desk scene captures the viewport anyway** rather
  than falling back to another mode. Silently changing the user's selected mode would be
  worse than giving them the region the mode names.
- **PNG is worth its size over a lossy format.** Once a capture carries scanlines and
  bloom, a lossy encoder's artifacts cluster on exactly the high-contrast edges a
  render-bug report needs legible.
- **560x384 is the correct raw size**, not 280x192. 560 is the native width of 80-column
  text and double hi-res, and reducing to 280 would destroy both; 384 is a reversible
  doubling of the native 192 scanlines, so nothing is interpolated.
- **The existing print-output conventions are the right precedent to follow** for the
  destination folder, the timestamped filename and the collision suffix. Screenshots and
  printouts then sit side by side in Pictures under one naming grammar.
- **Metadata is written on the file only.** The clipboard carries the image alone, which
  is what the clipboard image format supports and what paste targets expect.
- **A version string, the machine's display name, the monitor settings key and the scene
  pose all already exist** in forms this feature can reuse, so none of them introduces a
  second spelling of an existing fact.

## Dependencies

- **GH #132** is closed by this feature.
- **GH #134 and GH #82** (theme preview shows no CRT effects) are explicitly NOT in
  scope. They share a root cause with #132 but not a solution: the preview must show a
  theme that is *not* the active one, which no capture of the live picture can provide.
  They need their own independent CRT processing, and folding them in here would couple
  two features that only look alike.
- The project constitution's **Thin Executable, Testable Core** principle applies: the
  filename policy and the metadata composition are decision-making logic and must be
  drivable by the unit tests, not stranded in the executable because they sit near a
  platform API.
