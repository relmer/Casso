# Feature Specification: Physical Game Controllers

**Feature Branch**: `034-game-controllers`

**Created**: 2026-09-10

**Status**: Draft

**Input**: GitHub issue #97, "physical game-controller support (Xbox / gamepads / joysticks)". Apple II joystick and paddle input is synthesized today from the keyboard (arrows to joystick) and the mouse (paddle or mouse). Add Xbox controllers, generic USB gamepads and joysticks as a new input source feeding the existing paddle and pushbutton state. No new game-port hardware model.

## Context

The Apple II game port has 2 analog axes and 2 buttons that matter to nearly all software:

| Game port input | Soft switch | Driven today by |
|---|---|---|
| PDL0 (X) | `$C064` | arrow keys (joystick), mouse (paddle) |
| PDL1 (Y) | `$C065` | arrow keys (joystick), mouse (paddle) |
| PB0 | `$C061` | fire key, Open-Apple, mouse button |
| PB1 | `$C062` | second fire key, Solid-Apple |

Input selection is split in two today, persisted per machine:

- **Keys**: arrows-to-joystick on or off.
- **Pointer**: off, paddle, or mouse.

Paddle mode and arrows-to-joystick are mutually exclusive, since both drive PDL0/PDL1. A physical controller drives the same two axes and two buttons, so it enters the same model as a third way to drive them.

## Clarifications

### Session 2026-09-11

- Q: How does a controller coexist with arrows-to-joystick? -> A: Mutually exclusive, but while the selected controller is disconnected the arrow keys stand in until it returns (FR-008, FR-008a).
- Q: How is calibration triggered? -> A: Both: automatic by default (center at connect, limits from observed travel), plus a user Calibrate action that overrides it (FR-007, FR-007a).
- Q: Should control mapping be customizable, and should a controller's settings follow it? -> A: Yes. Controller settings UI edits a per-controller profile (calibration, deadzone, mapping) that is restored automatically when that controller connects, recognized by unit and falling back to model (User Story 5, FR-018 to FR-025).

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

---

### User Story 3 - Unplug and replug mid-session (Priority: P2)

A wireless controller's battery dies, or a USB cable is pulled, mid-game. Casso notices, releases the game port to rest, and picks the controller back up when it returns, without a restart.

**Why this priority**: Wireless controllers disconnect routinely. A game port stuck at a hard deflection with a button held is worse than no controller support.

**Independent Test**: A mock controller holding the stick at full deflection with PB0 pressed is removed; the game port returns to center with both buttons released. The mock is re-added; its input drives the port again.

**Acceptance Scenarios**:

1. **Given** a selected controller is held at full deflection with a button pressed, **When** it disconnects, **Then** within one polling interval both axes return to center and both buttons read released, and the arrow keys drive the joystick from then on.
2. **Given** the selected controller disconnected and the arrow keys are standing in, **When** it reconnects, **Then** it drives the game port again with no user action and the arrow keys stop driving it.
3. **Given** the input selector is open, **When** a controller is attached or removed, **Then** the list reflects the change.
4. **Given** a controller disconnects, **Then** the user can see that the selected controller is not connected (for example, in the selector or the toolbar input indicator).

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

**Independent Test**: With a mock controller, assign a nondefault button to PB0 and the D-pad to the axes; the game port follows the new controls and ignores the old ones. Save, reload, and reconnect the mock with the same identity; the mapping is restored. Connect a second mock of the same model with a different identity; it receives the model's profile.

**Acceptance Scenarios**:

1. **Given** the controller settings are open for a controller, **When** the user starts assigning PB0 and presses button 3, **Then** button 3 is assigned to PB0.
2. **Given** the D-pad is assigned to the joystick axes, **When** D-pad left is held, **Then** PDL0 reads 0; **When** released, **Then** PDL0 returns to center.
3. **Given** an axis is marked inverted, **When** the stick is pushed fully up, **Then** PDL1 reads 255 instead of 0.
4. **Given** both A and X are assigned to PB0, **When** either is held, **Then** PB0 reads pressed.
5. **Given** the controller settings are open, **When** the user moves any control, **Then** the settings show the live reading of that control and of the resulting game-port values.
6. **Given** a remapped controller, **When** the user chooses Reset to Defaults, **Then** the default mapping and automatic calibration are restored.
7. **Given** a controller with a saved profile is unplugged and plugged back in, possibly into a different port, **When** it connects, **Then** its own mapping and calibration apply.
8. **Given** a controller with no saved profile of its own, **When** another controller of the same model has one, **Then** the new controller starts from that model's profile.

### Edge Cases

- **Arrow keys held at the moment the controller reconnects**: the controller takes the joystick back immediately; held arrows stop driving the axes.
- **Arrow keys during fallback on a machine where arrows-to-joystick was never used**: the fallback still applies, since it follows the controller selection, not the arrows-to-joystick setting.
- **Keyboard button and controller button at once**: a button reads pressed if either source holds it. Releasing one does not release a button the other still holds.
- **Casso loses focus**: controller input stops driving the game port while Casso is not the foreground window, matching the keyboard's existing behavior, and the port returns to rest.
- **Machine switch**: controller input never reaches a machine being torn down; the new machine adopts its own saved selection.
- **Machine with no game port** (for example a configuration without one): controller selection is unavailable or has no effect, and nothing faults.
- **Paddle games**: a controller stick drives PDL0/PDL1 as absolute positions, so paddle software (Breakout-style games reading PDL0) works with the X axis.
- **Controller with no second button** (some joysticks): PB1 stays released; nothing faults.
- **Controllers that expose the stick on non-standard axes**: the default mapping uses the device's primary X/Y axes; the user can remap to the right ones.
- **Many controllers attached** (8+): all appear in the selector.
- **Two controllers of the same model**: they remain distinguishable in the selector, each keeps its own profile, and the persisted selection restores the right one when possible. If a specific unit cannot be told apart after a reconnect (for example, a controller with no serial number moved to another port), it falls back to the model's profile rather than silently taking the other unit's.
- **Assigning a control already used elsewhere**: the control is added to the new target; the settings show every target a control drives, and the user can remove the old assignment. Nothing is removed silently.
- **Axis at rest assigned to a button target**: an analog trigger or axis assigned to a button reads pressed past a threshold, not at any nonzero value.
- **Digital and analog sources on the same axis**: the source deflected furthest from center wins, so a D-pad press is not diluted by a stick at rest.
- **A target with nothing assigned**: an axis reads center and a button reads released.
- **Controller removed while its settings are open**: the settings stay open, show it disconnected, and keep unsaved edits until it returns or the user closes them.
- **Profile data that cannot be read** (hand-edited, from a newer version): that controller falls back to defaults, and the problem is reported rather than silently producing an unmapped controller.
- **Pause or step**: controller state is sampled into the port the same way keyboard state is; pausing emulation does not queue stale presses.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST enumerate attached game controllers, including Xbox-class controllers, generic USB/Bluetooth gamepads, and joysticks.
- **FR-002**: Casso MUST detect controllers being attached and removed while running, without a restart.
- **FR-003**: When a controller is the selected source, Casso MUST sample it continuously while the emulator window is foreground, at least as often as the display refreshes (60 Hz), and apply its mapped axis controls to PDL0 (X) and PDL1 (Y) over the full 0-255 range. With no custom mapping, the left stick (or a joystick's primary axes) drives them.
- **FR-004**: The mapping MUST be proportional: center at rest maps to 127/128, full deflection maps to 0 or 255, and intermediate deflection maps monotonically between them.
- **FR-005**: The mapped button controls MUST drive PB0 and PB1, reaching the same machine inputs Open-Apple and Solid-Apple reach on a //e and //c. With no custom mapping, the first two face buttons (on an Xbox-class controller, A and B; on a joystick, its first two buttons) drive them.
- **FR-006**: A deadzone around center MUST suppress rest jitter; a stick within it reads exactly center.
- **FR-007**: Casso MUST calibrate center and travel limits per controller, so an off-center or short-throw stick still reads center at rest and 0/255 at its limits. Calibration is automatic by default: center is captured when the controller connects and the limits widen to the travel actually observed. A user-invoked Calibrate action (center, then full travel) MUST override the automatic values for that controller.
- **FR-007a**: A controller with a user calibration MUST use it in place of automatic calibration, including the connect-time center capture. The user MUST be able to discard a user calibration and return to automatic.
- **FR-008**: A physical controller MUST appear as a joystick source in the existing input selector (Machine menu and toolbar input control). Selecting a controller is mutually exclusive with arrows-to-joystick and mouse-to-paddle, the way paddle mode and arrows-to-joystick are today.
- **FR-008a**: While the selected controller is disconnected, the arrow keys MUST drive the joystick in its place. When the controller reconnects, it takes the joystick back. The fallback does not change the saved selection, and the toolbar input indicator MUST show that the arrow keys are standing in.
- **FR-009**: Only the selected controller MUST drive the game port; other attached controllers are ignored.
- **FR-010**: On disconnect of the selected controller, Casso MUST return both axes to center and release both buttons within one sampling interval, then hand the joystick to the arrow keys (FR-008a), and MUST resume from that controller automatically on reconnect.
- **FR-011**: The selection MUST persist per machine alongside the existing input-mapping preferences, and MUST be restored at launch and on machine switch. A selection whose controller is absent MUST be retained rather than discarded.
- **FR-012**: Calibration and control mapping MUST persist together as a profile per controller and be restored automatically when that controller connects, without any user action.
- **FR-013**: The user MUST be able to tell when the selected controller is not connected.
- **FR-014**: Button state MUST combine with keyboard and mouse button sources: a button reads pressed while any enabled source holds it.
- **FR-015**: A controller-reading failure (device lost, access denied, driver error) MUST NOT fault emulation; it is treated as a disconnect, and the failure MUST be observable (for example as a disconnected indicator), not silently read as a centered stick.
- **FR-016**: All mapping, deadzone, calibration, selection, source-combination and connect/disconnect state logic MUST be exercisable by the unit test suite through a substitute controller, with no access to real devices in unit tests.
- **FR-017**: Machines without a game port MUST NOT offer controller selection, or MUST ignore it without faulting.
- **FR-018**: Casso MUST recognize a controller at two levels: the specific unit, and its model (vendor and product). A profile saved for the unit MUST take precedence; failing that, a profile saved for the model applies; failing that, defaults apply.
- **FR-019**: Casso MUST provide controller settings UI, reachable from the input selector, where the user picks an attached controller and edits its profile: calibration (FR-007), deadzone, and control mapping.
- **FR-020**: Each game-port target (PDL0, PDL1, PB0, PB1) MUST accept one or more assigned controls. Assignable controls are every axis, button, D-pad direction and trigger the controller reports.
- **FR-021**: An axis target MUST accept an analog axis (optionally inverted) or a pair of digital controls (one for each direction, driving the axis to its extreme while held). A button target MUST accept a button, a D-pad direction, or an analog axis or trigger past a threshold.
- **FR-022**: The user MUST be able to assign a control by activating it on the controller while the target is waiting for input, as well as by choosing it from a list. Waiting for input MUST be cancelable, and MUST ignore a control already deflected or held when waiting began.
- **FR-023**: The controller settings MUST show live readings of the controller's controls and of the resulting PDL0/PDL1 and PB0/PB1 values while open.
- **FR-024**: The user MUST be able to reset a controller's profile to defaults, and to choose whether a saved profile applies to that unit only or to every controller of its model.
- **FR-025**: The settings MUST show which controls drive more than one target, and MUST NOT remove an existing assignment as a side effect of adding one.

### Key Entities

- **Controller**: an attached input device. Has a unit identity usable to recognize it across reconnects and launches, a model identity (vendor and product) shared by every unit of the same model, a human-readable product description, a connected state, the set of controls it reports, and a current sample of those controls.
- **Controller selection**: per machine, which controller (if any) is the joystick source. Refers to a controller identity that may or may not currently be attached.
- **Controller profile**: saved per unit or per model. Holds the calibration, the deadzone, and the control mapping. Machine-independent.
- **Calibration**: part of a profile. Rest center and travel limits per axis, and whether they are automatic or user-set.
- **Control mapping**: part of a profile. For each game-port target, the list of assigned controls with their options (invert for an axis, threshold for an analog control driving a button, direction for a digital control driving an axis).
- **Game port state**: the existing PDL0/PDL1 positions and PB0/PB1 states. Controller samples feed it through the same path the keyboard and mouse sources use.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with an Xbox controller can go from plugging it in to playing a joystick game in under 30 seconds, including selecting it.
- **SC-002**: Stick and button changes reach the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled.
- **SC-003**: With the stick untouched, paddle readings stay exactly at center across 10 seconds of sampling on a controller within its deadzone.
- **SC-004**: Both axes reach 0 and 255, and both buttons register, on each of: an Xbox-class controller, a generic USB gamepad, and a USB joystick.
- **SC-005**: After a disconnect the game port reads centered and released within 100 ms, and after reconnect the controller drives it again within 2 seconds, with no user action in either case.
- **SC-006**: The selection, calibration and control mapping are restored on 100% of relaunches and reconnects with the same controller attached.
- **SC-008**: A user can reassign both buttons and switch the axes to the D-pad in under one minute from opening the controller settings.
- **SC-009**: A controller not yet recognized as a unit, but of a model with a saved profile, drives the game port with that profile on its first connection.
- **SC-007**: Controller support adds no measurable CPU cost while no controller is selected.

## Assumptions

- **Scope of mapping**: one controller drives one joystick (PDL0/PDL1, PB0/PB1). A second joystick on PDL2/PDL3, PB2, rumble, and mapping controller controls to Apple II keyboard keys are out of scope for v1. The Guide button is not assignable.
- **Automatic calibration assumes the stick is at rest when it connects**; the Calibrate action exists for the case where it is not, and for sticks whose automatic limits never settle.
- **Device access**: the issue suggested DirectInput on the grounds that only the left stick and two buttons were needed. Control mapping (FR-020) undoes part of that reasoning: DirectInput reports an Xbox-class controller's two triggers on one shared axis, so it cannot tell LT from RT or read both held at once. The device API is a planning decision, and the plan must address how triggers are read for Xbox-class controllers.
- **Unit recognition is best effort**: a controller without a unique serial number may not be recognizable as the same unit after moving to a different port. FR-018's model-level fallback covers that case.
- **Focus**: controller input applies only while Casso is the foreground window, matching the keyboard's existing button behavior.
- **Absolute positioning**: the stick sets paddle position directly (joystick semantics). It does not integrate stick deflection into relative motion the way the mouse-to-paddle capture does.
- **Persistence location**: the selection lives with the existing per-machine input preferences; profiles (calibration and mapping) are global and keyed by controller identity, since a stick's physical quirks and the user's preferred layout for it do not change with the emulated machine.
- **Deadzone default**: a conventional default (on the order of 10-15% of travel) works for most controllers without calibration.
