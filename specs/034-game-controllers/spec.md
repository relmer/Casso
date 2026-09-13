# Feature Specification: Physical Game Controllers

**Feature Branch**: `034-game-controllers`

**Created**: 2026-09-10

**Status**: Draft

**Input**: GitHub issue #97, "physical game-controller support (Xbox / gamepads / joysticks)". Apple II joystick and paddle input is synthesized today from the keyboard (arrows to joystick) and the mouse (paddle or mouse). Add Xbox controllers, generic USB gamepads and joysticks as a new input source feeding the existing paddle and pushbutton state. No new game-port hardware model.

## Context

The Apple II game port has four analog axes and three buttons. Nearly all software uses the first two axes and the first two buttons; two-player games read the rest (FR-034):

| Game port input | Soft switch | Driven today by |
|---|---|---|
| PDL0 (X) | `$C064` | arrow keys (joystick), mouse (paddle) |
| PDL1 (Y) | `$C065` | arrow keys (joystick), mouse (paddle) |
| PDL2 | `$C066` | nothing; absent on the //c, where the line is the mouse X direction |
| PDL3 | `$C067` | nothing; absent on the //c, where the line is the mouse Y direction |
| PB0 | `$C061` | fire key, Open-Apple, mouse button |
| PB1 | `$C062` | second fire key, Solid-Apple |
| PB2 | `$C063` | //e Shift key; the mouse button on the //c |

Input selection is split in two today, persisted per machine:

- **Keys**: arrows-to-joystick on or off.
- **Pointer**: off, paddle, or mouse.

Paddle mode and arrows-to-joystick are mutually exclusive, since both drive PDL0/PDL1. A physical controller drives the same two axes and two buttons, so it enters the same model as a third way to drive them.

## Clarifications

### Session 2026-09-11

- Q: How does a controller coexist with arrows-to-joystick? -> A: Mutually exclusive. Revised 2026-09-12: the arrow keys never take the axes on their own, not even while no controller is attached, because arrows-to-joystick also takes X and Z away from the guest's keyboard (FR-008, FR-008a).
- Q: How is calibration triggered? -> A: Both: automatic by default (center at connect, limits from observed travel), plus a user Calibrate action that overrides it (FR-007, FR-007a).
- Q: Should control mapping be customizable, and should a controller's settings follow it? -> A: Yes. Controller settings UI edits the control mapping, calibration and deadzone, restored automatically when that controller connects (User Story 5, FR-018 to FR-025).
- Q: Which device APIs? -> A: XInput for Xbox-class controllers and DirectInput for all other controllers. DirectInput alone cannot read an Xbox controller's triggers independently, and XInput alone does not see non-Xbox devices.
- Q: Must identical Xbox controllers be told apart? -> A: No. Xbox-class controllers are factory-calibrated and need no calibration, so they are recognized by model only (FR-018a).
- Q: Can a user keep different mappings for different games? -> A: Yes. Named profiles per controller model, each a control mapping, chosen from the input selector and remembered per machine. Calibration is split out of the profile and stays with the physical unit (User Story 6, FR-026 to FR-030).
- Q: How do toolbar controller and profile pickers relate to the 032 command-widget work, which replaces `CommandToolbar` and supplies the shared dropdown? -> A: 034 depends on 032. The toolbar pickers are built on 032's toolbar and dropdown widgets after 032 merges, not on today's `CommandToolbar` (FR-031). Update: 032 is now on master and merged into this branch, so the dependency is satisfied and the Machine menu entries use the same widgets.
- Q: When a controller connects and none is selected, is it selected automatically? -> A: Yes, always: whenever the current machine has no controller selected, a newly connected controller becomes its selection, even if the user had deliberately chosen arrow keys or paddle mode (FR-032).
- Q: Where do the controller settings live? -> A: A new Controllers page in the Settings sheet, following its Apply/Cancel model; the input selector's Controller Settings item opens the sheet to that page (FR-019).
- Q: Does controller input apply while Casso is not the foreground window? -> A: No. Revised the same day (and see the two entries below): XInput takes priority over background input, and Windows documents XInput as focus-gated, so controller input follows Casso's activation like the keyboard. Casso counts as active while any of its windows (including the Settings sheet) is active (FR-003, FR-033).
- Q: How does a controller serve paddle games? -> A: Analog axis bindings get a rate response that moves the paddle at a speed proportional to deflection and holds it on release, plus a "Paddles" starting point for new profiles (FR-021a).
- Q: (analysis) A selected DirectInput controller comes back with a different unit identity after moving ports; does it stay unselected? -> A: No. If the selected unit is absent and exactly one unit of its model is attached, that unit is adopted (FR-032).
- Q: (analysis) Should the spec set a deadzone value? -> A: No. The default comes from the API's published value where one exists; values live in research R8.
- Q: Why is PB2 excluded? -> A: It no longer is. PB2 is a target on the ][, ][+ and //e (shared with Shift on the //e) and unavailable on the //c, where `$C063` is the mouse button (FR-020).
- Q: (hardware) When the selected controller is unplugged and another is attached, what takes the game port? -> A: The other controller. Revised 2026-09-12: it becomes the selection, and the original does not take the port back when it returns (FR-008a, FR-010).
- Q: (hardware) Should two people be able to play at once on two controllers? -> A: Yes. The game port has four analog axes on the ][, ][+ and //e and two on the //c, and two-player games read them independently, so one selected controller cannot serve them. Controllers are assigned to axes rather than to a single "joystick source" slot (User Story 7, FR-034 to FR-038).
- Q: Should the controller picker list controllers alone, or every source that drives the paddle axes? -> A: One combined picker: arrows-to-joystick, mouse-as-paddle, and each attached controller, as mutually exclusive entries. There is one game port and one thing driving it, so the exclusivity belongs on the face of the control instead of happening behind the user's back when they pick a controller. The //c IOU mouse stays a separate control: it drives a slot card, not the game port, so it is not an answer to this question (FR-008, FR-031).
- Q: Where does the combined picker live -- a Machine-menu submenu, or the command bar? -> A: The command bar. A cascading submenu puts two hovers between the user and their controller, for a list they pick from while playing. The Machine menu keeps the per-source toggles it already had, unchanged; whether menu items that duplicate command-bar items earn their place at all is a question about the whole chrome, not this feature, and is left open (FR-008, FR-031).
- Q: What does the picker show when closed? -> A: The source that is driving -- "Keys", "Mouse", or the device -- rather than the word for what the picker is for. That is the one place the answer is worth a permanent seat, and having it there is what lets the input cluster stop carrying the same answer in icons. It reads "Controller" when nothing drives the axes (FR-008b).
- Q: In what order are the picker's entries listed? -> A: Attached controllers first, then the keys and the mouse. A real controller plays these games better than either, and the keys and the mouse are always there to pick. It also puts the checked entry at the top once a controller is plugged in (FR-008b).
- Q: Should the picker's icon track the device, and what is shown when nothing drives the axes? -> A: Yes, it follows the DEVICE: a gamepad, a stick, the paddle, or the arrow keys. ControllerKind says which API reads a controller, which is a different question -- an Xbox pad and a USB gamepad are one shape read two ways. The form factor comes from DirectInput's own device type; XInput is a gamepad by definition; a wheel draws as a stick, not being worth a drawing of its own. With nothing driving it reads "Controller" over the gamepad outline: what COULD go there is the useful answer, and a slashed icon would be a new drawing for a state the automatic selection makes uncommon (FR-008b).
- Q: Does the picker's glyph need a distinct disconnected state, or do the label and tooltip carry it? -> A: No; the glyph gets no third state. The glyph follows the DEVICE that drives the axes. Revised 2026-09-12: since the selection moves to whatever is attached (FR-008a), the selected controller is never absent, so there is no disconnected state to draw (FR-008a, FR-008b, FR-013).
- Q: Does mouse mode belong in the paddle picker, or keep its own control? -> A: Its own, and as a plain toolbar toggle rather than a segment. It drives the //c's IOU mouse, not the game port, so it is not an answer to what drives the paddles; and once the picker wore the driving source on its face, the input cluster was left toggling one thing, which a toggle button says without a segmented control or a machine-dependent segment count. The cluster is deleted (FR-008).
- Q: Should mouse mode simply always be on, since a real //c has no such mode? -> A: Parked, not rejected. It is arguably more faithful -- the //c mouse is soldered in and its button always readable at `$C063` -- and `IsGuestMouseLive` already keeps it inert until the firmware enables interrupts. It would also delete the mouse toggle. Three things would have to be settled first: left-clicks in the viewport would reach the guest button, paddle capture would need an explicit suspend rule where mutual exclusion does the work today, and the absolute host-to-guest mapping would run against apps that move their own cursor. It changes //c behavior unrelated to controllers, so it belongs in its own spec with its own hardware check against MousePaint, not in 034.
- Q: Do menu items that duplicate command-bar items earn their place? -> A: Left open deliberately, and the menu items stay for now. Answering it is a question about the whole chrome rather than this feature -- as the owner put it, removing them is an argument for writing a Ribbon control instead.

### Session 2026-09-12

- Q: Should a disconnected selection be kept, with another controller or the arrow keys standing in until it returns? -> A: No. The selection locks on the controller in use: it stays until that controller disappears or the user picks another. On a disappearance the longest-attached remaining controller becomes the selection and is saved, and the original does not take the axes back when it returns. Having to pick an entry the picker already showed checked, to get back to a controller that took over, is what retired the old model (FR-008a, FR-010, FR-011).
- Q: When the last controller disappears, do the arrow keys drive the axes until one returns? -> A: No. The axes rest at center and nothing drives them. Arrows-to-joystick takes X and Z away from the guest's keyboard, so it is only ever on because the user turned it on. The next controller to connect is selected automatically (FR-008a, FR-032).
- Q: (hardware) How are "four paddles" and "two joysticks" both expressed? -> A: As one number per machine. A joystick is two analog axes wired to one stick, so four axes means four paddles or two joysticks or one joystick plus two paddles, and the //c's two axes mean two paddles or one joystick. The spec carries the axis count and nothing else (FR-034).
- Q: Which API reports a controller that is ready to read, rather than a raw HID interface arriving? -> A: None that Casso will use; detection stays a HID notification followed by rescans over XInput and DirectInput (research R4). XInput and DirectInput have no arrival notifications. Windows.Gaming.Input is out because Casso uses no WinRT. GameInput was probed on this machine on 2026-09-12 and rejected: the runtime that ships with Windows (GameInput.dll 0.2309) never reported the VKBsim Gladiator, returned E_NOTIMPL from EnableOemDeviceSupport, and did not report an Xbox controller that was already connected when the probe started. The 3.5 redistributable was not tested; requiring every user to install it, for an API that has not been shown to read a generic stick, is not a trade worth making.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Play with a connected controller (Priority: P1)

A user plugs in an Xbox controller, selects it as the joystick source, boots a joystick game (Choplifter, Lode Runner, Karateka) and plays with the left stick and face buttons. Stick deflection moves the game's cursor or character in proportion, and the face buttons fire.

**Why this priority**: This is the feature. Everything else is convenience around it.

**Independent Test**: With a mock controller reporting known axis and button states, the paddle values and pushbutton states read by the emulated machine match the expected mapping. On real hardware, a joystick-test program such as the one on the DOS 3.3 System Master shows full range on both axes and both buttons.

**Acceptance Scenarios**:

1. **Given** a controller is selected and connected, **When** the left stick is at rest, **Then** PDL0 and PDL1 read at center (127 or 128).
2. **Given** a controller is selected, **When** the stick is pushed fully left, right, up and down, **Then** the corresponding paddle reads 0 or 255.
3. **Given** a controller is selected, **When** the stick is at half deflection, **Then** the paddle reads approximately halfway between center and the extreme, not snapped to either.
4. **Given** a controller is selected, **When** the first face button is held, **Then** PB0 reads pressed; **When** the second is held, **Then** PB1 reads pressed.
5. **Given** a //e or //c, **When** a controller button is held, **Then** software reading Open-Apple or Solid-Apple as a button sees it pressed, as it does for the keyboard today.

---

### User Story 2 - Choose the controller and keep the choice (Priority: P2)

A user with more than one controller attached (a gamepad and a flight stick) picks which one drives the game port from the existing input selector. The choice survives restarting Casso and switching machines and back.

**Why this priority**: Without a selector the feature only works for single-controller users, and without persistence the user reselects every launch.

**Independent Test**: With two mock controllers, selecting each in turn routes only that controller's input to the game port. Saving and reloading the machine's preferences restores the selection.

**Acceptance Scenarios**:

1. **Given** two controllers are attached, **When** the user opens the input selector, **Then** both appear by their product description.
2. **Given** controller B is selected, **When** controller A's stick moves, **Then** the game port does not change.
3. **Given** a controller is selected for a machine, **When** Casso restarts with that controller attached, **Then** the same controller is selected with no user action.
4. **Given** the selected controller is not attached at launch, **When** Casso starts, **Then** the selection is kept and takes effect when that controller connects.
5. **Given** a machine with no controller selected and arrows-to-joystick on, **When** a controller is plugged in, **Then** it becomes the selection, arrows-to-joystick turns off, and the command bar's picker shows the controller.

---

### User Story 3 - Unplug and replug mid-session (Priority: P2)

A wireless controller's battery dies, or a USB cable is pulled, mid-game. Casso notices, releases the game port to rest, and picks the controller back up when it returns, without a restart.

**Why this priority**: Wireless controllers disconnect routinely. A game port stuck at a hard deflection with a button held is worse than no controller support.

**Independent Test**: A mock controller holding the stick at full deflection with PB0 pressed is removed; the game port returns to center with both buttons released. The mock is re-added; its input drives the port again.

**Acceptance Scenarios**:

1. **Given** a selected controller is held at full deflection with a button pressed, **When** it disconnects, **Then** within one polling interval both axes return to center and both buttons read released, and nothing drives the axes until a controller connects or the user picks a source.
2. **Given** the selected controller disconnected and no other controller is attached, **When** it reconnects, **Then** it is selected again and drives the game port with no user action.
2a. **Given** the selected controller disconnected and a second controller took over, **When** the first reconnects, **Then** the second keeps the axes and the first is listed, unchecked, in the selector.
3. **Given** the input selector is open, **When** a controller is attached or removed, **Then** the list reflects the change.
4. **Given** a controller disconnects, **Then** a brief notice names the controller that disconnected; the command bar shows what took over, or "Controller" when nothing did.

---

### User Story 4 - A worn or off-center stick still works (Priority: P3)

An older gamepad's stick rests slightly off center, or a flight stick never quite reaches its corners. The user gets a steady centered reading at rest and full range at the limits.

**Why this priority**: Most modern controllers work acceptably with a default deadzone; calibration matters for the minority that do not.

**Independent Test**: A mock controller whose rest position is offset and whose travel stops short of the raw extremes produces center at rest and 0/255 at its own limits after calibration.

**Acceptance Scenarios**:

1. **Given** a stick resting within the deadzone of center, **When** it is untouched, **Then** the paddles read center with no jitter.
2. **Given** a stick whose physical travel stops at 90% of the raw range, **When** calibrated and pushed to its limits, **Then** the paddles reach 0 and 255.
3. **Given** a calibrated controller, **When** Casso restarts, **Then** the calibration is still in effect for that controller.
4. **Given** a newly connected controller the user has never calibrated, **When** it connects at rest and the stick is then pushed to its limits once, **Then** center is captured at connect and the paddles reach 0 and 255 from then on, with no user action.
5. **Given** a controller whose stick was deflected when it connected, **When** the user runs the Calibrate action (center the stick, then move it through its full travel), **Then** the captured center and limits replace the automatic ones and are kept across restarts.

---

### User Story 5 - Remap controls for a particular controller (Priority: P3)

A user's flight stick puts its fire button on button 3, and a Lode Runner player prefers the D-pad to the analog stick. The user opens the controller settings, picks the controller, and assigns which controls drive the joystick axes and the two buttons, pressing the control they want rather than hunting through a list. Casso recognizes that controller the next time it is plugged in and applies its mapping and calibration without being asked.

**Why this priority**: The default mapping covers Xbox-class controllers and most gamepads. Remapping is what makes odd joysticks, D-pad play, and personal preference work.

**Independent Test**: With a mock controller, assign a nondefault button to PB0 and the D-pad to the axes; the game port follows the new controls and ignores the old ones. Save, reload, and reconnect the mock; the mapping is restored. Connect a second mock of the same model with a different unit identity; it uses the same mapping but its own calibration.

**Acceptance Scenarios**:

1. **Given** the controller settings are open for a controller, **When** the user starts assigning PB0 and presses button 3, **Then** button 3 is assigned to PB0.
2. **Given** the D-pad is assigned to the joystick axes, **When** D-pad left is held, **Then** PDL0 reads 0; **When** released, **Then** PDL0 returns to center.
3. **Given** an axis is marked inverted, **When** the stick is pushed fully up, **Then** PDL1 reads 255 instead of 0.
4. **Given** both A and X are assigned to PB0, **When** either is held, **Then** PB0 reads pressed.
5. **Given** the controller settings are open, **When** the user moves any control, **Then** the settings show the live reading of that control and of the resulting game-port values.
6. **Given** a remapped profile, **When** the user chooses Reset to Defaults, **Then** that profile's mapping returns to the default mapping.
7. **Given** a remapped and calibrated controller is unplugged and plugged back in, possibly into a different port, **When** it connects, **Then** its active profile and its calibration apply.
8. **Given** a DirectInput controller with no calibration of its own, **When** another controller of the same model is connected for the first time, **Then** it uses that model's profiles and starts with automatic calibration.

---

### User Story 6 - Keep a mapping per game and switch between them (Priority: P3)

A user plays Lode Runner with the D-pad and A/B, and a flight simulator with the analog stick, trigger as fire, and inverted Y. They save each as a named profile for the controller and pick the one they want before playing, without re-assigning controls every time they change games.

**Why this priority**: Remapping without profiles means redoing the mapping on every game change, which in practice means users stop remapping.

**Independent Test**: With a mock controller, create two profiles with different PB0 assignments; activating each routes only that profile's controls. Save, reload, and confirm both profiles exist and the last active one is still active.

**Acceptance Scenarios**:

1. **Given** a controller with only its Default profile, **When** the user creates a profile called "Lode Runner" from the current mapping and edits it, **Then** Default is unchanged.
2. **Given** a controller with profiles "Default" and "Flight", **When** the user chooses "Flight" from the input selector, **Then** the game port follows Flight's mapping on the next sample, without opening the settings and without resetting the emulated machine.
3. **Given** "Flight" is active for a machine, **When** Casso restarts or the user switches machines and back, **Then** "Flight" is still active on that machine.
4. **Given** a profile, **When** the user renames, duplicates or deletes it, **Then** the list updates; deleting the active profile makes Default active.
5. **Given** two controllers of the same model, **When** the user creates a profile on one, **Then** it is available on the other too.

---

### User Story 7 - Two people play at once (Priority: P2)

Two people sit down to a two-player game -- The Bilestoad, or a Pong-descended paddle game -- each with their own controller. Each drives their own paddle axis and their own button, independently, at the same time.

**Why this priority**: a two-player game reads its players' axes separately, so a single selected controller does not make it merely inconvenient, it makes it unplayable. The game port has had the axes for this since 1977; only Casso's one-source-at-a-time assumption stands in the way.

**Independent Test**: two mock controllers are assigned to PDL0 and PDL1. Moving each one moves only its own axis, and each one's button reaches its own button line, with both held at once.

**Acceptance Scenarios**:

1. **Given** two controllers assigned to PDL0 and PDL1 on a //e, **When** both sticks are moved at once, **Then** each axis follows its own controller and neither follows the other.
2. **Given** two controllers each assigned one axis, **When** both buttons are held at once, **Then** both button lines read pressed.
3. **Given** a //e with two controllers assigned two axes each, **When** both are moved, **Then** PDL0-PDL3 all follow their assigned controller.
4. **Given** a //c, **When** the user opens the assignment settings, **Then** only PDL0 and PDL1 are offered, because the //c has no PDL2/PDL3 (FR-035).
5. **Given** a controller assigned to PDL0, **When** the user assigns a second controller to PDL0, **Then** the second one takes the axis and the first no longer drives it (FR-036).
6. **Given** two assigned controllers, **When** one disconnects, **Then** only its axes return to center and the other keeps driving its own without interruption.
7. **Given** a single Xbox controller on a //e, **When** the user assigns its left stick to PDL0/PDL1 and its right stick to PDL2/PDL3, **Then** one controller drives a four-axis game.

---

### Edge Cases

- **User switches back to arrow keys with a controller still attached**: the switch holds for the session, because the controller is not newly connecting. The next time a controller connects (unplug and replug, relaunch, or switching to this machine) it is selected again (FR-032).
- **Controller that took over is unplugged while the original is back**: the original becomes the selection again, by the same longest-attached rule, not because it was ever preferred.
- **Several controllers attached at launch with none selected**: the first one enumerated is selected; the rest only appear in the selector.
- **Last controller unplugged while arrows-to-joystick is off**: X and Z keep typing into the guest; the axes rest at center.
- **Keyboard button and controller button at once**: a button reads pressed if either source holds it. Releasing one does not release a button the other still holds.
- **Another application becomes active, or Casso is minimized**: controller input stops driving the game port and the controller's contribution returns to rest, the same as a held keyboard button. It resumes when Casso is active again.
- **Controllers page open in the Settings sheet**: the sheet is a Casso window, so controller input continues; the game port keeps following the applied mapping, and the live readings and press-to-assign keep working. Confirmed on hardware: XInput kept delivering at the same rate with a second top-level window of the same process active.
- **Machine switch**: controller input never reaches a machine being torn down; the new machine adopts its own saved selection.
- **Machine with no game port** (for example a configuration without one): controller selection is unavailable or has no effect, and nothing faults.
- **Paddle games**: with the default absolute response, a self-centering stick returns the paddle to the middle when released. Rate response (FR-021a) makes the paddle hold its position, and the "Paddles" starting point sets up two-player paddle games on the two sticks.
- **Controller with no second button** (some joysticks): PB1 stays released; nothing faults.
- **Controllers that expose the stick on non-standard axes**: the default mapping uses the device's primary X/Y axes; the user can remap to the right ones.
- **An axis with no hardware behind it**: a device may advertise axes belonging to an attachment that is not plugged in, which read pinned at one end of their travel rather than at center. Confirmed on hardware: a flight stick whose rudder pedals were unplugged still reported their axes, resting at a rail. Automatic calibration MUST NOT learn limits from an axis that never moves, and the default mapping MUST NOT land on one, or software reads a control held hard over.
- **Many controllers attached** (8+): all appear in the selector.
- **Two Xbox-class controllers of the same model**: both appear in the selector and share the model's profiles. Selecting either selects the model; if both are connected, the one in the lower slot drives the game port.
- **Two DirectInput controllers of the same model**: they remain distinguishable in the selector, share the model's profiles, keep separate calibration, and the persisted selection restores the right one when possible. If a specific unit cannot be told apart after a reconnect (for example, a controller with no serial number moved to another port), it starts with automatic calibration rather than silently taking the other unit's.
- **Profile names**: names are unique per controller model, case-insensitively. Creating or renaming to a name already in use is refused with a message; empty names are refused.
- **Default profile**: every model always has one. It can be edited and reset, but not deleted or renamed.
- **Profile deleted while another machine has it active**: a machine whose remembered active profile no longer exists uses Default, and does not recreate the deleted profile.
- **Switching profiles while a button is held**: controls that stop being assigned release their targets immediately; nothing stays stuck pressed or deflected across the switch.
- **Unsaved edits when switching profiles in the settings**: the user is asked to keep or discard them; they are never silently applied to the newly chosen profile.
- **Assigning a control already used elsewhere**: the control is added to the new target; the settings show every target a control drives, and the user can remove the old assignment. Nothing is removed silently.
- **Axis at rest assigned to a button target**: an analog trigger or axis assigned to a button reads pressed past a threshold, not at any nonzero value.
- **Digital and analog sources on the same axis**: the source deflected furthest from center wins, so a D-pad press is not diluted by a stick at rest.
- **A target with nothing assigned**: an axis reads center and a button reads released.
- **Active profile switched from the menu while the Controllers page has unapplied edits to it**: the switch takes effect on the emulated machine immediately; the page's unapplied edits stay pending and are applied to the profile they were made to, not to the newly active one.
- **Calibrate action and Cancel**: a calibration captured on the Controllers page follows Apply/Cancel like any other edit on the page.
- **Controller removed while its settings are open**: the settings stay open, show it disconnected, and keep unsaved edits until it returns or the user closes them.
- **Profile or calibration data that cannot be read** (hand-edited, from a newer version): the affected profile falls back to the default mapping, or the controller to automatic calibration, and the problem is reported rather than silently producing an unmapped controller. Readable profiles for the same model are unaffected.
- **Pause or step**: controller state is sampled into the port the same way keyboard state is; pausing emulation does not queue stale presses.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST enumerate attached game controllers, including Xbox-class controllers, generic USB/Bluetooth gamepads, and joysticks.
- **FR-002**: Casso MUST detect controllers being attached and removed while running, without a restart.
- **FR-003**: When a controller is the selected source, Casso MUST sample it continuously while Casso is active (FR-033), at least as often as the display refreshes (60 Hz), and apply its mapped axis controls to the axes it holds over the full 0-255 range. With no custom mapping, the left stick (or a joystick's primary axes) drives PDL0 (X) and PDL1 (Y).
- **FR-004**: The mapping MUST be proportional: center at rest maps to 127/128, full deflection maps to 0 or 255, and intermediate deflection maps monotonically between them.
- **FR-005**: The mapped button controls MUST drive PB0 and PB1, reaching the same machine inputs Open-Apple and Solid-Apple reach on a //e and //c. With no custom mapping, the first two face buttons (on an Xbox-class controller, A and B; on a joystick, its first two buttons) drive them.
- **FR-006**: A deadzone around center MUST suppress rest jitter; a stick within it reads exactly center.
- **FR-007**: Casso MUST calibrate center and travel limits per controller, so an off-center or short-throw stick still reads center at rest and 0/255 at its limits. Calibration is automatic by default: center is captured when the controller connects and the limits widen to the travel actually observed. A user-invoked Calibrate action (center, then full travel) MUST override the automatic values for that controller.
- **FR-007a**: A controller with a user calibration MUST use it in place of automatic calibration, including the connect-time center capture. The user MUST be able to discard a user calibration and return to automatic.
- **FR-008**: The command bar MUST offer ONE picker for what drives the paddle axes, listing every attached controller, then arrows-to-joystick and mouse-as-paddle, as mutually exclusive entries with the one in use checked. Picking any entry gives the axes to it and takes them from the others; the exclusivity is what the control shows rather than something that happens behind the user's back. It MUST NOT be a cascading submenu: this is a list the user picks from while playing. The //c IOU mouse is NOT in this picker and keeps its own separate control, because it drives a slot card rather than the game port. The Machine menu keeps its existing per-source toggles unchanged.
- **FR-008a**: The selection MUST stay on the controller in use until that controller disappears or the user picks another entry. When the selected controller disappears, the controller that has been attached longest, among those not already assigned to an axis, MUST become the selection for the session; a controller returning later MUST NOT take the axes back. When no such controller is attached, the selection MUST become none and nothing drives the axes. No disconnect MUST turn on arrows-to-joystick or mouse-to-paddle, since arrows-to-joystick takes keys away from the guest.
- **FR-008b**: The picker MUST show the source that is driving rather than the word for what it is for: the device's description, or the keys or the mouse, and "Controller" when nothing drives the axes. Source labels MUST be capped in length, and a device description's trailing vendor and product parenthetical dropped before capping, so the picker's width moves as little as possible on a strip where every entry's position depends on it.
- **FR-009**: Only controllers the user has assigned to an axis MUST drive the game port; other attached controllers are ignored.
- **FR-010**: On disconnect of an assigned controller, Casso MUST return its axes to center and release its buttons within one sampling interval, then move the selection as FR-008a says.
- **FR-011**: The selection MUST persist per machine alongside the existing input-mapping preferences, and MUST be restored at launch and on machine switch when its controller is attached. A saved controller that is absent at launch or on machine switch MUST be replaced for the session by an attached controller as FR-032 chooses, or by none when none is attached. Only a user's pick, or the first automatic selection on a machine with nothing saved, MUST change what is saved: a takeover or a clear (FR-008a) lasts for the session and MUST NOT overwrite the saved controller.
- **FR-012**: Calibration MUST persist per controller unit, profiles MUST persist per controller model, and both MUST be restored automatically when a controller connects, without any user action.
- **FR-013**: When a disconnect changes the selection, Casso MUST show a brief notice naming the controller that disconnected. What took over, or "Controller" when nothing did, is already on the command bar's picker (FR-008b).
- **FR-014**: Button state MUST combine with keyboard and mouse button sources: a button reads pressed while any enabled source holds it.
- **FR-015**: A controller-reading failure (device lost, access denied, driver error) MUST NOT fault emulation; it is treated as a disconnect, and the failure MUST be observable (for example as a disconnected indicator), not silently read as a centered stick.
- **FR-016**: All mapping, deadzone, calibration, selection, source-combination and connect/disconnect state logic MUST be exercisable by the unit test suite through a substitute controller, with no access to real devices in unit tests.
- **FR-017**: On machines without a game port, controller selection MUST be ignored without faulting: the selection is still offered and saved, and has no effect on that machine.
- **FR-018**: Casso MUST recognize a controller at two levels: the specific unit, and its model (vendor and product). Calibration is keyed by unit; profiles are keyed by model, so every controller of a model shares them. A unit with no saved calibration starts with automatic calibration.
- **FR-018a**: Every Xbox-class controller shares ONE model key, not one per vendor and product. XInput reports every controller it supports through a single fixed layout (two sticks, two triggers, a D-pad and ten buttons), so there is nothing per-model for a mapping to capture, and the same controller reports different product IDs depending on whether it is on USB or Bluetooth. Its vendor and product IDs are kept for display only. Xbox-class controllers are factory-calibrated, so they get no automatic calibration and no Calibrate action; the deadzone and profiles are all that apply. A saved Xbox-class selection matches whichever Xbox-class controller is connected.
- **FR-019**: Casso MUST provide a Controllers page in the Settings sheet where the user picks an attached controller, edits its calibration (FR-007) and deadzone, and manages and edits its profiles. A Controller Settings item in the input selector MUST open the Settings sheet to that page. The page MUST follow the sheet's Apply/Cancel model: mapping, deadzone and profile edits take effect on the emulated machine only on Apply and are discarded on Cancel, while the live readings (FR-023) preview the edited mapping before it is applied.
- **FR-020**: Each game-port target (PDL0-PDL3, PB0, PB1, PB2) MUST accept one or more assigned controls. PB2 is available on the ][, ][+ and //e, where on the //e it shares `$C063` with the Shift key; on the //c, whose `$C063` is the mouse button, the PB2 target MUST be shown as unavailable and its bindings ignored. PB2 has no default binding. Which analog targets exist follows the machine's axis count (FR-034, FR-035). Assignable controls are every axis, button, D-pad direction and trigger the controller reports.
- **FR-021**: An axis target MUST accept an analog axis (optionally inverted) or a pair of digital controls (one for each direction, driving the axis to its extreme while held). A button target MUST accept a button, a D-pad direction, or an analog axis or trigger past a threshold.
- **FR-021a**: An analog axis binding MUST offer two responses. **Absolute**: stick position sets paddle position. **Rate**: deflection beyond the deadzone moves the paddle at a speed proportional to deflection, up to a user-set maximum speed, and the paddle holds its position when the stick is released; the held position resets to center when the controller selection, the active profile, or the machine changes. Creating a profile MUST offer a "Paddles" starting point: left stick X in rate mode to PDL0, right stick X in rate mode to PDL1, first two face buttons to PB0 and PB1.
- **FR-022**: The user MUST be able to assign a control by activating it on the controller while the target is waiting for input, as well as by choosing it from a list. Waiting for input MUST be cancelable, and MUST ignore a control already deflected or held when waiting began.
- **FR-023**: The controller settings MUST show live readings of the controller's controls and of the resulting PDL0-PDL3 and PB0-PB2 values while open, showing only the axes the machine has (FR-035).
- **FR-024**: The user MUST be able to reset a profile's mapping to the default mapping.
- **FR-025**: The settings MUST show which controls drive more than one target, and MUST NOT remove an existing assignment as a side effect of adding one.
- **FR-026**: Each controller model MUST have a Default profile, created automatically with the default mapping (FR-003, FR-005), which can be edited and reset but not deleted or renamed.
- **FR-027**: The user MUST be able to create a profile (from the default mapping or as a copy of an existing profile), rename it, and delete it. Profile names MUST be unique per model, case-insensitively, and 1 to 40 characters after trimming surrounding whitespace.
- **FR-028**: The active profile MUST be selectable from the input selector (Machine menu and toolbar input control) without opening the controller settings, and MUST take effect on the next sample without resetting the emulated machine.
- **FR-029**: The active profile MUST be remembered per machine alongside the controller selection. If it no longer exists, Default MUST be used.
- **FR-030**: Switching profiles MUST release any button and center any axis that the new profile no longer drives from a currently held or deflected control.
- **FR-031**: The Machine menu entries and the toolbar's paddle-source and profile pickers (FR-008, FR-013, FR-028) MUST be built on the command, dropdown and toolbar widgets from the Dxui command-widgets feature (032), which is on master.
- **FR-032**: Whenever the current machine has no controller selected and a controller connects, that controller MUST become the machine's selection automatically, replacing arrows-to-joystick or mouse-to-paddle if either is on. No notice is shown: the command bar's picker already shows the selected controller (FR-008b). A controller already attached when Casso starts, or when the user switches to a machine with no controller selected, counts as connecting. A controller connecting while one is already selected MUST NOT change the selection. When a saved DirectInput unit is absent at launch or machine switch and exactly one unit of the same model is attached, that unit MUST be chosen ahead of enumeration order, so a controller whose unit identity changed (for example, moved to another port) is picked back up; otherwise the first attached controller in enumeration order is chosen.
- **FR-033**: Controller input MUST drive the game port only while Casso is active, meaning one of its windows (the emulator window or the Settings sheet) is the active window. When Casso becomes inactive or is minimized, the controller's contribution MUST return to rest; it resumes on reactivation. Xbox-class controllers are always read through XInput, and this activation rule applies to them the same as to every other controller.
- **FR-034**: "The selected controller", wherever the requirements above say it, means the controller assigned to PDL0 and PDL1. Selection and axis assignment are one thing seen at two levels of detail, so FR-008a to FR-013 need no rewording for the axis model below. Each machine MUST carry how many analog game-port axes it exposes: four (PDL0-PDL3) on the ][, ][+ and //e, and two (PDL0/PDL1) on the //c, whose PDL2/PDL3 lines are the mouse direction lines instead. Everything else follows from that count. A joystick is two axes wired to one stick, so four axes serve four paddles, two joysticks, or one joystick and two paddles, and the //c's two serve two paddles or one joystick. The spec states no separate joystick maximum.
- **FR-035**: An axis the machine does not expose MUST NOT be offered as an assignment or mapping target, and an assignment naming one MUST be ignored without faulting, the way a machine without a game port ignores a selection entirely (FR-017). A saved assignment MUST be kept rather than discarded when the user switches to a machine with fewer axes, so switching back restores it.
- **FR-036**: Each analog axis MUST have at most one owner at a time. Assigning a controller to an axis another controller holds MUST displace the previous owner rather than letting both drive it. Buttons are not restricted this way: they already combine across sources, so several controllers and the keyboard can hold PB0 and the guest sees one press.
- **FR-037**: The user MUST be able to assign each attached controller to the axes it drives, and the settings MUST show which controller holds which axis. A controller may hold one axis (a paddle), two (a joystick), or, where the machine has four and nothing else claims them, all four.
- **FR-038**: The default assignment for a newly connected controller MUST claim PDL0 and PDL1 and no more, leaving the remaining axes free for a second controller. A controller with enough controls to drive four axes MUST NOT claim them by default; assigning the extra axes by hand (FR-037) is how a user opts into that.

### Key Entities

- **Controller**: an attached input device. Has a unit identity usable to recognize it across reconnects and launches, a model identity (vendor and product) shared by every unit of the same model, a human-readable product description, a connected state, the set of controls it reports, and a current sample of those controls.
- **Controller selection**: per machine, which controller (if any) drives which analog axes. Refers to controller identities that may or may not currently be attached. One controller may hold several axes, and each axis has at most one owner (FR-036).
- **Game-port axis budget**: per machine, how many analog axes exist -- four on the ][, ][+ and //e, two on the //c (FR-034). Bounds what can be assigned and what the settings offer.
- **Controller profile**: a named control mapping belonging to a controller model, shared by every unit of that model. Each model has a Default profile plus any the user creates. Machine-independent.
- **Active profile**: per machine and controller selection, which profile is in use.
- **Calibration**: per DirectInput controller unit. Rest center and travel limits per axis, and whether they are automatic or user-set. Xbox-class controllers have none.
- **Deadzone**: per controller model.
- **Control mapping**: the content of a profile. For each game-port target, the list of assigned controls with their options (invert for an axis, threshold for an analog control driving a button, direction for a digital control driving an axis).
- **Game port state**: the existing PDL0-PDL3 positions and PB0-PB2 states. Controller samples feed it through the same path the keyboard and mouse sources use.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with an Xbox controller can go from plugging it in to playing a joystick game in under 30 seconds, including selecting it.
- **SC-002**: Stick and button changes reach the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled.
- **SC-003**: With the stick untouched, paddle readings stay exactly at center across 10 seconds of sampling on a controller within its deadzone.
- **SC-004**: Both axes reach 0 and 255, and both buttons register, on each of: an Xbox-class controller, a generic USB gamepad, and a USB joystick.
- **SC-005**: After a disconnect the game port reads centered and released within 100 ms, and a controller connecting while nothing is selected drives it within 2 seconds, with no user action in either case.
- **SC-006**: The selection, calibration and control mapping are restored on 100% of relaunches and reconnects with the same controller attached.
- **SC-007**: Controller support adds no measurable CPU cost while no controller is selected, whether or not controllers are attached.
- **SC-008**: A user can reassign both buttons and switch the axes to the D-pad in under one minute from opening the controller settings.
- **SC-009**: A controller not yet recognized as a unit, but of a model with saved profiles, drives the game port with those profiles available on its first connection.
- **SC-010**: Switching the active profile from the input selector takes two actions or fewer and no more than 5 seconds, with no emulated machine reset.
- **SC-011**: Two controllers assigned to separate axes both drive the machine at once, with neither one's stick or button affecting the other's axis or button line.
- **SC-012**: When an assigned controller disconnects, the other assigned controllers keep driving their own axes with no interruption to their readings.

## Assumptions

- **Scope of mapping**: controllers drive the machine's analog axes (PDL0-PDL3, or PDL0/PDL1 on the //c) and PB0-PB2. Rumble and mapping controller controls to Apple II keyboard keys are out of scope for v1. The Guide button is not assignable.
- **Automatic calibration assumes the stick is at rest when it connects**; the Calibrate action exists for the case where it is not, and for sticks whose automatic limits never settle.
- **Device access**: XInput for Xbox-class controllers, DirectInput for everything else, with XInput devices filtered out of the DirectInput enumeration so no controller appears twice. DirectInput alone was ruled out because it reports an Xbox-class controller's two triggers on one shared axis by design, which control mapping (FR-020) cannot live with.
- **Xbox-class controllers need no unit recognition**: XInput exposes only a slot number (0-3), with no product, vendor or serial identity, and a controller can change slots across reconnects. That would make telling two identical Xbox controllers apart unreliable, but nothing requires it: calibration is the only per-unit data and they need none (FR-018a). Recognizing the model still has to come from correlating the slot with its underlying device.
- **DirectInput unit recognition is best effort**: a device without a unique serial number may not be recognized as the same unit after moving to a different port; FR-018's model-level fallback covers that case.
- **XInput's four-controller limit** applies to Xbox-class controllers; DirectInput devices are not counted against it.
- **Focus**: controller input applies only while Casso is active (FR-033). Background input was considered and dropped: Windows documents XInput as gated by window focus, and XInput is required for Xbox controllers.
- **Absolute positioning by default**: the default mapping sets paddle position directly (joystick semantics); rate response is opt-in per binding (FR-021a).
- **Persistence location**: the controller selection and active profile live with the existing per-machine input preferences; profiles, calibration and deadzone are global and keyed by controller model or unit, since a stick's physical quirks and the user's layouts for it do not change with the emulated machine.
- **Profiles are chosen by hand**: activating a profile automatically when a particular disk is mounted is tracked by GH #78, which owns known-disk recognition. The profile storage here must allow a profile to be looked up by controller model and profile name so that work can associate disks with profiles.
- **Profiles belong to a model**: a profile refers to that model's controls, so profiles are not shared across different models. Copying a profile to another model is out of scope.
- **Dependency on 032**: satisfied; the Dxui command-widgets feature is on master and merged into this branch.
- **Deadzone default**: the default deadzone comes from the device API's published recommendation where one exists (XInput publishes one); where none exists, Casso supplies a default. The values are a planning detail (research R8).
