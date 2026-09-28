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
- Q: Must identical Xbox controllers be told apart? -> A: No. Xbox-class controllers are factory-calibrated and need no calibration, so they are recognized by model only (FR-018a). Revised 2026-09-15: every connected XInput slot is its own device, so several Xbox controllers can be listed, picked and assigned to players at once. The unit id is the slot (0-3), which is all XInput exposes; the model key is unchanged, so profiles, deadzone and calibration still cover every Xbox-class controller together. Windows assigns slots in connection order and they can change across replugs, so a slot identifies a controller only for as long as it stays plugged in. Two identical controllers are told apart by moving a stick and watching the readout on the Controllers page, not by anything in the description.
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
- Q: (hardware) Should two people be able to play at once on two controllers? -> A: Yes. The game port has four analog axes on the ][, ][+ and //e and two on the //c, and two-player games read them independently, so one selected controller cannot serve them. A machine is either in single-source mode or in a two-player multiplayer mode with a slot per player (User Story 7, FR-034 to FR-039).
- Q: Should the controller picker list controllers alone, or every source that drives the paddle axes? -> A: One combined picker: arrows-to-joystick, mouse-as-paddle, and each attached controller, as mutually exclusive entries. There is one game port and one thing driving it, so the exclusivity belongs on the face of the control instead of happening behind the user's back when they pick a controller. The //c IOU mouse stays a separate control: it drives a slot card, not the game port, so it is not an answer to this question (FR-008, FR-031).
- Q: Where does the combined picker live -- a Machine-menu submenu, or the command bar? -> A: The command bar. A cascading submenu puts two hovers between the user and their controller, for a list they pick from while playing. The Machine menu keeps the per-source toggles it already had, unchanged; whether menu items that duplicate command-bar items earn their place at all is a question about the whole chrome, not this feature, and is left open (FR-008, FR-031).
- Q: What does the picker show when closed? -> A: The source that is driving -- "Keys", "Mouse", or the device -- rather than the word for what the picker is for. That is the one place the answer is worth a permanent seat, and having it there is what lets the input cluster stop carrying the same answer in icons. It reads "Controller" when nothing drives the axes (FR-008b).
- Q: In what order are the picker's entries listed? -> A: Attached controllers first, then the keys and the mouse. These games are better played with a real controller than with either, and the keys and the mouse are always there to pick. It also puts the checked entry at the top once a controller is plugged in (FR-008b).
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

### Session 2026-09-24

- Q: Two players both on Xbox One S controllers: can they use different profiles? -> A: Yes. Profiles still belong to the model, but each controller unit has its own active profile, so two pads of one model can use different profiles or the same one. In Settings, the Profile drop-down is the active profile of the controller in Editing: pick player one's controller and choose its profile, then player two's (FR-028, FR-029).
- Q: Is the active profile per machine or global? -> A: Global, keyed by controller unit. A user can still make //c and //e profiles, and can use either on any machine. The //c's only difference, no second joystick, is not something a profile can solve (FR-029).
- Q: Where is the active profile picked outside Settings? -> A: A Profiles submenu on the command bar's paddle-source picker, listing Default first, then the model's other profiles, then New... below a separator. The separate profile picker on the toolbar is removed. With more than one controller in play, each gets its own section under a "Player N -- <controller>" header, with its own check; with one, there is no header. New... opens the Controllers page with the New Profile dialog for the controller in Editing (FR-028, FR-031).

### Session 2026-09-27

Decisions from GH #156, made with spec 036's session of the same date. Single-source and multiplayer modes give way to two player slots that are always present, so that two controllers play two players with no setup.

- Q: How does the picker present two players? -> A: As a row for each, Player 1 and Player 2. Each row shows what is playing and opens a submenu: Automatic, then every attached controller, with arrows-to-joystick and mouse-as-paddle under Player 1 and Disabled under Player 2. This reverses the 2026-09-11 decision against a cascading submenu. That decision assumed the user picks a controller often; with Automatic expected to be right nearly every time, the submenu is opened rarely (FR-008, FR-041).
- Q: With Automatic, who is Player 1? -> A: When Casso has seen two different controllers connect while running, the first to connect is Player 1 and the second Player 2. Otherwise the first controller used is Player 1 and the next Player 2, so a stick that is always plugged in does not come ahead of the controller the user picks up. A lone controller plays as Player 1 at once (FR-032, FR-042).
- Q: What does one person playing drive while a second controller sits unused? -> A: Everything a single controller always has: PDL0, PDL1 and PB0-PB2. A slot filled by Automatic counts only once its controller has been used, so the split between two players happens when the second person joins (FR-042).
- Q: Does a controller connecting still replace arrows-to-joystick? -> A: No. The keys or the mouse picked for Player 1 stay until the user changes the pick. This reverses the 2026-09-11 answer; few users are expected to prefer the keys, and one who does should not lose them to a controller being plugged in (FR-032).
- Q: Which button lines does each player drive? -> A: The ones their slot's target has on the hardware: joystick 0 has PB0 and PB1, joystick 1 has PB2, and a single paddle has its own line. The earlier rule, one line per player with player 2 on PB1, fit two paddles and was wrong for two joysticks: it took player 1's second button away (FR-039).
- Q: How is a two-player paddle game set up? -> A: By both players choosing a paddle profile. With nothing set by hand, a slot's target follows its controller's active profile: one that binds PDL0 alone plays as a paddle, any other as a joystick (FR-043). (Superseded later the same day: each player's mode decides, not the profile.)
- Q: Are the players' entries per machine? -> A: No, global. They describe the controllers on the user's desk, which do not change with the emulated machine, the reason profiles and calibration were already global (FR-011, FR-037).
- Q: How does the user learn which controller Automatic chose? -> A: A notice, "Player 1: <controller>", whenever the controller differs from the last one to hold that slot. The same controller in the same slot as last time shows nothing, so a user with one controller is not shown a notice at every launch, and a changed order always is. Notices stack: each stays up for its full time, a later one appears below, and it slides up when the one above expires (FR-044).
- Q: What does the picker's button show with two players? -> A: Player 1's description followed by "+1". A label too long for the button is shortened with an ellipsis in the middle of the description, keeping the "+1" whole (FR-008b).
- Q: Where are profiles picked, now that each player has a submenu? -> A: In a section at the foot of each player's submenu, for the controller playing there. The separate Profiles submenu is removed. The list holds the profiles of the mode in effect (FR-028, spec 036 FR-020).
- Q: Where is two-player play turned off? -> A: Player 2's Disabled entry, which the Controllers page also shows as a Multiplayer checkbox, ticked by default (FR-037). (Superseded later the same day: there is no checkbox.)
- Q: What happens to a slot when its controller leaves? -> A: It is kept for that controller while anyone is still playing, and the player who remains keeps only what they had. With nobody left playing, Automatic starts over (FR-040).
- Q: What counts as a controller being used? -> A: A button press, or any axis outside its deadzone. The deadzone already defines rest for that controller, and anything beyond it moves an analog game's paddles; a knock hard enough to count moves a stick well past any higher threshold anyway (FR-042).
- Q: Is a controller assigned at launch the same as one being used? -> A: No. A lone controller is assigned to Player 1 at once, so it drives the game port the moment it is picked up, but it is not in use until it gives input. If a second controller arrives before either has been used, the first controller to give input becomes Player 1, even the one that just arrived, and the other becomes Player 2 once it is used. The rules for one controller driving everything, for the split and for a held slot count controllers in use, not assigned ones (FR-042).
- Q: Do the keys or the mouse picked for Player 1 count as in use? -> A: Yes, from the moment they are picked, so a controller as Player 2 takes joystick 1 (FR-042).
- Q: What does the one-time adoption make of the old per-machine selection? -> A: A saved controller only becomes the last holder of Player 1, and Player 1 stays on Automatic, because the old selection was usually chosen automatically too. A multiplayer setup that was turned on was set up by hand, so its controllers become picks. The keys and the mouse as paddle become Player 1's entry, and the //c's own mouse stays per machine (FR-011).
- Q: How quickly does a newly used controller drive the port? -> A: Within 100 ms of its first input. A controller not yet assigned is watched at a slower rate than one in use, which costs nothing measurable while idle, and after that its input reaches the machine within one frame as before (SC-005).
- Q: How does a player choose between a joystick and a paddle? -> A: Each player has a mode, Joystick or Paddle, chosen on their row. The mode alone decides what the player drives, as the hardware wires the game port; the "maps to" choice and the rule that a slot's target follows its profile are removed. A player's profile list holds only the profiles of that player's mode. This replaces both the per-slot target and the target-from-profile rule, which were hard to understand (FR-037, FR-039, FR-043).
- Q: Is there a Multiplayer checkbox? -> A: No. Player 2's row is always shown, and its entry, Automatic by default, is where two-player play is turned off: Disabled. The checkbox only repeated that entry (FR-037).
- Q: What does the picker's button show when Player 1's controller has left and Player 2 plays on? -> A: Player 1's controller followed by "(disconnected) +1" (FR-008b).

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

**Independent Test**: With two mock controllers, picking each in turn for Player 1 routes only that controller's input to the game port. Saving and reloading the preferences restores the pick.

**Acceptance Scenarios**:

1. **Given** two controllers are attached, **When** the user opens the input selector, **Then** both appear by their product description.
2. **Given** controller B is selected, **When** controller A's stick moves, **Then** the game port does not change.
3. **Given** a controller is picked for Player 1, **When** Casso restarts with that controller attached, **Then** the same controller is picked with no user action, on every machine.
4. **Given** the picked controller is not attached at launch, **When** Casso starts, **Then** the pick is kept, shown as not connected, and takes effect when that controller connects.
5. **Given** arrows-to-joystick picked for Player 1, **When** a controller is plugged in, **Then** the keys keep driving Player 1 and the controller is listed in the picker (FR-032).

---

### User Story 3 - Unplug and replug mid-session (Priority: P2)

A wireless controller's battery dies, or a USB cable is pulled, mid-game. Casso notices, releases the game port to rest, and picks the controller back up when it returns, without a restart.

**Why this priority**: Wireless controllers disconnect routinely. A game port stuck at a hard deflection with a button held is worse than no controller support.

**Independent Test**: A mock controller holding the stick at full deflection with PB0 pressed is removed; the game port returns to center with both buttons released. The mock is re-added; its input drives the port again.

**Acceptance Scenarios**:

1. **Given** a selected controller is held at full deflection with a button pressed, **When** it disconnects, **Then** within one polling interval both axes return to center and both buttons read released, and nothing drives the axes until a controller connects or the user picks a source.
2. **Given** the selected controller disconnected and no other controller is attached, **When** it reconnects, **Then** it is selected again and drives the game port with no user action.
2a. **Given** one person playing on Automatic whose controller disconnected, and a second controller then used in its place, **When** the first reconnects, **Then** the second stays Player 1, and the first is listed in the picker and becomes Player 2 if it is used.
3. **Given** the input selector is open, **When** a controller is attached or removed, **Then** the list reflects the change.
4. **Given** a controller that holds a slot disconnects, **Then** a brief notice gives its description; the command bar shows what is playing, or "Controller" when nothing is.

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
4. **Given** a profile, **When** the user renames, duplicates or deletes it, **Then** the list updates; deleting the active profile makes the built-in profile of its mode active.
5. **Given** two controllers of the same model, **When** the user creates a profile on one, **Then** it is available on the other too.

---

### User Story 7 - Two people play at once (Priority: P2)

Two people sit down to a two-player game, The Bilestoad or a Pong-descended paddle game, each with their own controller, and play. Nobody opens a menu: the first controller is Player 1 and the second Player 2, and a notice shows which is which whenever that differs from last time. Each drives their own axes and their own buttons, independently, at the same time.

**Why this priority**: a two-player game reads its players' axes separately, so a single controller does not make it merely inconvenient, it makes it unplayable. The game port has had the axes for this since 1977. Needing setup first is what the first user to try it with two controllers could not find.

**Independent Test**: two mock controllers connect in turn with both players on Automatic, and each gives input. Moving each one moves only its own axes, and each player's buttons reach their own lines, with both held at once.

**Acceptance Scenarios**:

1. **Given** two players on joystick 0 and joystick 1 on a //e, **When** both sticks are moved at once, **Then** each player's paddles follow only their own controller.
2. **Given** those two players, **When** both hold their first button at once, **Then** PB0 reads player one and PB2 reads player two (FR-039).
3. **Given** those two players, **When** player one presses a control their profile binds to PB1, **Then** PB1 reads pressed; **When** player two presses one their profile binds to PB1 or PB2, **Then** nothing reaches the game port (FR-039).
4. **Given** a //c with both players in Joystick mode, **Then** Player 2 drives nothing, because the //c has no PDL2/PDL3; **When** both players are in Paddle mode, **Then** each drives its own paddle (FR-035, FR-039).
5. **Given** Player 1 in Joystick mode and Player 2 in Paddle mode, **Then** Player 1 drives PDL0, PDL1, PB0 and PB1 and Player 2 drives PDL2 and PB2 (FR-039).
6. **Given** two players, **When** one controller disconnects, **Then** only that player's paddles return to center and their buttons release; the other keeps playing what they had, without interruption, and the slot is kept for the controller that left (FR-040).
7. **Given** two players, **When** the user sets Player 2 to Disabled, **Then** Player 1 drives the game port alone, as a single controller always has, and the other controller drives nothing (FR-037).
8. **Given** a controller picked for each player, **When** Casso starts with neither attached, **Then** both stay picked, shown as not connected, and plugging one in plays it with no user action (FR-011).
9. **Given** both players on Automatic and no controller attached, **When** two controllers connect in turn, **Then** the first is Player 1 and the second Player 2, and a notice shows each one that did not hold that slot last time (FR-042, FR-044).
10. **Given** both players on Automatic and a stick and a gamepad attached at launch, **When** the gamepad is the first to be used, **Then** it is Player 1 and drives PDL0, PDL1 and PB0-PB2, and the stick drives nothing until it is used (FR-042).
11. **Given** two players both in Paddle mode, **Then** player one drives PDL0 and PB0 and player two PDL1 and PB1, and each player's profile list holds only Paddle profiles (FR-039, FR-043).
12. **Given** a lone controller that was Player 1 when Casso last ran, **When** Casso starts with it attached, **Then** it plays as Player 1 and no notice is shown; **When** a different controller is attached instead, **Then** the notice is shown (FR-044).

---

### Edge Cases

- **The keys picked for Player 1 with a controller attached**: the pick holds until the user changes it, across controllers connecting and relaunches (FR-032).
- **Controller that took over is unplugged while the original is back**: with nobody left playing, Automatic starts over and the next controller used is Player 1 (FR-040, FR-042).
- **Several controllers attached at launch with Player 1 on Automatic**: none plays until one is used, and the first used is Player 1 (FR-042).
- **Last controller unplugged while arrows-to-joystick is off**: X and Z keep typing into the guest; the axes rest at center.
- **Keyboard button and controller button at once**: a button reads pressed if either source holds it. Releasing one does not release a button the other still holds.
- **Another application becomes active, or Casso is minimized**: controller input stops driving the game port and the controller's contribution returns to rest, the same as a held keyboard button. It resumes when Casso is active again.
- **Controllers page open in the Settings sheet**: the sheet is a Casso window, so controller input continues; the game port keeps following the applied mapping, and the live readings and press-to-assign keep working. Confirmed on hardware: XInput kept delivering at the same rate with a second top-level window of the same process active.
- **Machine switch**: controller input never reaches a machine being torn down; the players' entries are global and carry over to the new machine.
- **A second controller bumped while one person plays**: it becomes Player 2 and takes PDL2, PDL3 and PB2, which a one-player game does not read; Player 1 keeps PDL0, PDL1, PB0 and PB1. On the //e, PB2 shares its line with the Shift key, so that controller's first button reads as Shift to software that checks it there. Setting Player 2 to Disabled prevents both.
- **A wireless controller that sleeps and wakes**: it returns to the slot it held, and its reconnection does not count as a new connection for ordering the players (FR-042).
- **Input while Casso is not active**: it does not count as a controller's first use, so playing another game on a controller does not give it a slot (FR-033, FR-042).
- **Machine with no game port** (for example a configuration without one): controller selection is unavailable or has no effect, and nothing faults.
- **Paddle games**: with the default absolute response, a self-centering stick returns the paddle to the middle when released. Rate response (FR-021a) makes the paddle hold its position, and the "Paddles" starting point sets up one player's paddle on one controller. Two players use a controller each, one per player slot (User Story 7); two people cannot share one controller, and the two slots may not name the same one.
- **Controller with no second button** (some joysticks): PB1 stays released; nothing faults.
- **Controllers that expose the stick on non-standard axes**: the default mapping uses the device's primary X/Y axes; the user can remap to the right ones.
- **An axis with no hardware behind it**: a device may advertise axes belonging to an attachment that is not plugged in, which read pinned at one end of their travel rather than at center. Confirmed on hardware: a flight stick whose rudder pedals were unplugged still reported their axes, resting at a rail. Automatic calibration MUST NOT learn limits from an axis that never moves, and the default mapping MUST NOT land on one, or software reads a control held hard over.
- **Many controllers attached** (8+): all appear in the selector.
- **Two Xbox-class controllers of the same model**: both appear in the selector and share the model's profiles. Selecting either selects the model; if both are connected, the one in the lower slot drives the game port.
- **Two DirectInput controllers of the same model**: they remain distinguishable in the selector, share the model's profiles, keep separate calibration, and the persisted selection restores the right one when possible. If a specific unit cannot be told apart after a reconnect (for example, a controller with no serial number moved to another port), it starts with automatic calibration rather than silently taking the other unit's.
- **Profile names**: names are unique per controller model, case-insensitively. Creating or renaming to a name already in use is refused with a message; empty names are refused.
- **Default profile**: every model always has one. It can be edited and reset, but not deleted or renamed.
- **Profile deleted while another controller has it active**: every controller of that model whose active profile it was moves to Default, and a controller whose remembered active profile no longer exists uses Default, and the deleted profile is not recreated.
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
- **FR-008**: The command bar MUST offer ONE picker for what drives the game port, with a row for each of the two players, labeled Player 1 and Player 2 (spec 036 FR-019 gives the labels used while the Joyport is on). Each row MUST show what is playing for that player, and MUST open a submenu listing Automatic first, then every attached controller. Each submenu also offers the player's mode, Joystick or Paddle (FR-037). Player 1's submenu also lists arrows-to-joystick while Player 1 is in Joystick mode and mouse-as-paddle while it is in Paddle mode, and Player 2's also lists Disabled. Exactly one entry in each submenu MUST be checked (FR-041). The //c IOU mouse is NOT in this picker and keeps its own separate control, because it drives a slot card rather than the game port. The Machine menu keeps its existing per-source toggles, which set Player 1's entry.
- **FR-008a**: A player's controller MUST stay theirs until it disappears or the user picks another entry. What follows a disappearance is FR-040's. No disconnect MUST turn on arrows-to-joystick or mouse-to-paddle, since arrows-to-joystick takes keys away from the guest.
- **FR-008b**: The picker's button MUST show the source that is driving rather than the word for what it is for: Player 1's device description, or the keys or the mouse, and "Controller" when nothing drives the game port. While Player 2 is also playing, the label MUST end in "+1". While Player 1's slot is held for a controller that left and Player 2 plays on, the label MUST be Player 1's description followed by "(disconnected) +1". The label MUST be capped in length, with a device description's trailing vendor and product parenthetical dropped first. A label still too long MUST be shortened with an ellipsis in the middle of the description, keeping the "+1" whole, so the picker's width moves as little as possible on a strip where every entry's position depends on it.
- **FR-009**: Only a controller that holds a player slot, by the user's pick or by Automatic (FR-042), MUST drive the game port; other attached controllers are ignored.
- **FR-010**: On disconnect of a controller that holds a slot, Casso MUST return its axes to center and release its buttons within one sampling interval, and then treat the slot as FR-040 says.
- **FR-011**: Each player's entry (Automatic, a controller, the keys, the mouse or Disabled) MUST persist globally, not per machine, since it describes the controllers on the user's desk and not the emulated machine, and MUST be restored at launch. A picked controller that is absent MUST stay picked: it is shown as not connected and that player waits for it. What Automatic chose is saved only to decide whether a notice is shown (FR-044), and MUST NOT be used to assign a controller. The first launch after this change MUST adopt what was saved for the machine being launched: its arrow keys or mouse as paddle become Player 1's entry; its saved controller becomes only the last holder of Player 1 (FR-044), with Player 1 left on Automatic; and a multiplayer setup that was turned on gives each player its controller as a pick. The //c's own mouse stays per machine. What other machines saved is ignored.
- **FR-012**: Calibration MUST persist per controller unit, profiles MUST persist per controller model, and both MUST be restored automatically when a controller connects, without any user action.
- **FR-013**: When a controller that holds a slot disconnects, Casso MUST show a brief notice (FR-044) giving that controller's description. What is playing afterward, or "Controller" when nothing is, is already on the command bar's picker (FR-008b).
- **FR-014**: Button state MUST combine with keyboard and mouse button sources: a button reads pressed while any enabled source holds it.
- **FR-015**: A controller-reading failure (device lost, access denied, driver error) MUST NOT fault emulation; it is treated as a disconnect, and the failure MUST be observable (for example as a disconnected indicator), not silently read as a centered stick.
- **FR-016**: All mapping, deadzone, calibration, selection, source-combination and connect/disconnect state logic MUST be exercisable by the unit test suite through a substitute controller, with no access to real devices in unit tests.
- **FR-017**: On machines without a game port, controller selection MUST be ignored without faulting: the selection is still offered and saved, and has no effect on that machine.
- **FR-018**: Casso MUST recognize a controller at two levels: the specific unit, and its model (vendor and product). Calibration is keyed by unit; profiles are keyed by model, so every controller of a model shares them. A unit with no saved calibration starts with automatic calibration.
- **FR-018a**: Every Xbox-class controller shares ONE model key, not one per vendor and product. XInput reports every controller it supports through a single fixed layout (two sticks, two triggers, a D-pad and ten buttons), so there is nothing per-model for a mapping to capture, and the same controller reports different product IDs depending on whether it is on USB or Bluetooth. Its vendor and product IDs are kept for display only. Xbox-class controllers are factory-calibrated, so they get no automatic calibration and no Calibrate action; the deadzone and profiles are all that apply. Every connected XInput slot MUST be enumerated as its own device, so two Xbox controllers are two devices sharing one model key; the unit id is the slot number, since XInput exposes no other identity. A saved Xbox-class selection whose slot is empty, with exactly one Xbox-class controller connected, MUST adopt that controller, which also covers a selection saved before XInput units carried a slot. Slots are assigned in connection order and can change across replugs, so a saved slot taken by a different physical controller is accepted as the same one.
- **FR-019**: Casso MUST provide a Controllers page in the Settings sheet where the user picks an attached controller, edits its calibration (FR-007) and deadzone, and manages and edits its profiles. A Controller Settings item in the input selector MUST open the Settings sheet to that page. The page MUST follow the sheet's Apply/Cancel model: mapping, deadzone and profile edits take effect on the emulated machine only on Apply and are discarded on Cancel, while the live readings (FR-023) preview the edited mapping before it is applied. The players' entries and modes (FR-037) are outside that model: they apply as they are changed, as a pick from the picker does.
- **FR-020**: Each game-port target (PDL0-PDL3, PB0, PB1, PB2) MUST accept one or more assigned controls. PB2 is available on the ][, ][+ and //e, where on the //e it shares `$C063` with the Shift key; on the //c, whose `$C063` is the mouse button, the PB2 target MUST be shown as unavailable and its bindings ignored. PB2 has no default binding. Which analog targets exist follows the machine's axis count (FR-034, FR-035). Assignable controls are every axis, button, D-pad direction and trigger the controller reports.
- **FR-021**: An axis target MUST accept an analog axis (optionally inverted) or a pair of digital controls (one for each direction, driving the axis to its extreme while held). A button target MUST accept a button, a D-pad direction, or an analog axis or trigger past a threshold.
- **FR-021a**: An analog axis binding MUST offer two responses. **Absolute**: stick position sets paddle position. **Rate**: deflection beyond the deadzone moves the paddle at a speed proportional to deflection, up to a user-set maximum speed, and the paddle holds its position when the stick is released; the held position resets to center when the controller selection, the active profile, or the machine changes. Creating a profile MUST offer a "Paddles" starting point for one player: left stick X in rate mode to PDL0 and the first face button to PB0, with PDL1 and PB1 left unassigned. Two people cannot share a controller, so a two-player paddle game uses a controller per player, each assigned to its own paddle (FR-036, FR-037).
- **FR-022**: The user MUST be able to assign a control by activating it on the controller while the target is waiting for input, as well as by choosing it from a list. Waiting for input MUST be cancelable, and MUST ignore a control already deflected or held when waiting began.
- **FR-023**: The controller settings MUST show live readings of the controller's controls and of the resulting PDL0-PDL3 and PB0-PB2 values while open, showing only the axes the machine has (FR-035).
- **FR-024**: The user MUST be able to reset a profile's mapping to the built-in mapping of the mode the profile belongs to (spec 036 FR-020).
- **FR-025**: The settings MUST show which controls drive more than one target, and MUST NOT remove an existing assignment as a side effect of adding one.
- **FR-026**: Each controller model MUST have three built-in profiles, created automatically: Default, with the default mapping (FR-003, FR-005); Paddles, with the Paddles mapping (FR-043); and Joyport (spec 036 FR-017). All three can be edited and reset but not deleted or renamed.
- **FR-027**: The user MUST be able to create a profile (from its kind's built-in mapping or as a copy of an existing profile of its kind), rename it, and delete it. Profile names MUST be unique per model, case-insensitively, and 1 to 40 characters after trimming surrounding whitespace. A profile belongs to the kind in effect for its player when it is created, and each kind offers its own starting points (spec 036 FR-020).
- **FR-028**: Each controller in play MUST have its active profile selectable from a section at the foot of its player's submenu in the picker, without opening the controller settings, and the choice MUST take effect on the next sample without resetting the emulated machine. The section is headed by the controller's description and lists the profiles of the mode in effect, the built-in one first (spec 036 FR-020), then New... below them; New... opens the Controllers page with the New Profile dialog. The section is absent when the player's entry has no controller playing: Automatic before it has chosen, the keys, the mouse, Disabled, or a picked controller that is not connected. In the settings, the Profile drop-down is the active profile of the controller in Editing.
- **FR-029**: The active profile MUST be remembered per controller unit and per mode (spec 036 FR-018), globally rather than per machine, so two controllers of one model can use different profiles and a profile can be used on any machine. If it no longer exists, the built-in profile of the mode in effect MUST be used. A machine's active profile saved by an earlier build MUST pass once to that machine's saved controller, as its normal-mode choice, when that controller has none of its own.
- **FR-030**: Switching profiles MUST release any button and center any axis that the new profile no longer drives from a currently held or deflected control.
- **FR-031**: The Machine menu entries and the toolbar's paddle-source picker with its Profiles submenu (FR-008, FR-013, FR-028) MUST be built on the command, dropdown and toolbar widgets from the Dxui command-widgets feature (032), which is on master.
- **FR-032**: With Player 1 on Automatic and exactly one controller attached, that controller MUST play as Player 1 without waiting for input, whether it connects while Casso runs or is already attached at launch. The keys or the mouse picked for Player 1 MUST stay until the user changes the pick: a controller connecting does not replace them. When a picked DirectInput unit is absent at launch and exactly one unit of the same model is attached, that unit MUST be taken as the picked controller, so a controller whose unit identity changed (for example, moved to another port) is picked back up.
- **FR-033**: Controller input MUST drive the game port only while Casso is active, meaning one of its windows (the emulator window or the Settings sheet) is the active window. When Casso becomes inactive or is minimized, the controller's contribution MUST return to rest; it resumes on reactivation. Xbox-class controllers are always read through XInput, and this activation rule applies to them the same as to every other controller.
- **FR-034**: "The selected controller", wherever the requirements above say it, means the controller playing as Player 1. Selection and axis assignment are one thing seen at two levels of detail, so FR-008a to FR-013 need no rewording for the axis model below. Each machine MUST carry how many analog game-port axes it exposes: four (PDL0-PDL3) on the ][, ][+ and //e, and two (PDL0/PDL1) on the //c, whose PDL2/PDL3 lines are the mouse direction lines instead. Everything else follows from that count. A joystick is two axes wired to one stick, so four axes serve four paddles, two joysticks, or one joystick and two paddles, and the //c's two serve two paddles or one joystick. The spec states no separate joystick maximum.
- **FR-035**: A paddle the machine does not expose MUST NOT be offered as a player slot's target or as a mapping target, and a saved slot naming one MUST be ignored without faulting, the way a machine without a game port ignores a selection entirely (FR-017). The slot MUST be kept rather than discarded when the user switches to a machine with fewer axes, so switching back plays it again.
- **FR-036**: Each analog axis MUST have at most one owner at a time. The two player slots MUST NOT name the same controller, and their paddle claims MUST NOT overlap: the settings MUST leave a taken choice out of the other slot's list, and a saved or hand-edited setup that repeats a controller or overlaps MUST be refused or normalized rather than trusted.
- **FR-037**: The game port MUST always have two player slots, global rather than per machine. Each slot holds an entry (FR-008) and a mode, Joystick or Paddle, defaulting to Joystick and saved globally with the entry. A controller in neither slot is not used. There is no separate single-source mode: one person playing is Player 1, with Player 2 on Automatic and unused or on Disabled. The Controllers page MUST show both players' rows at all times, each with its entry and its mode; there is no Multiplayer checkbox, and Player 2's Disabled entry is how two-player play is turned off.
- **FR-038**: A player's controller MUST use its own profile, placed where that player's mode puts it (FR-039): the controller's own PDL0.. targets land on the player's paddles in ascending order, so a player in Paddle mode uses only its PDL0 bindings, and two players share one profile without a per-player copy. A controller playing alone MUST claim no more axes than FR-039 gives it; a controller with enough controls to drive four axes MUST NOT claim them by default.
- **FR-039**: What each player drives MUST follow the two players' modes, as the hardware wires the game port. Player 1 in Joystick mode drives PDL0 and PDL1 from its PDL0 and PDL1 bindings and PB0 and PB1 from its PB0 and PB1 bindings; in Paddle mode it drives PDL0 and PB0. Player 2 in Joystick mode drives PDL2 and PDL3, and PB2 from its PB0 bindings. Player 2 in Paddle mode drives PDL1 and PB1 while Player 1 is in Paddle mode, and PDL2 and PB2 while Player 1 is in Joystick mode. Bindings with no input in the player's place MUST be kept in the profile and ignored. A player in Joystick mode playing alone MUST drive PDL0, PDL1 and PB0-PB2 as its profile binds them; one in Paddle mode playing alone drives PDL0 and PB0. What a machine lacks plays nothing (FR-035): on the //c a second joystick drives nothing. Button state MUST still combine with the keyboard and mouse sources (FR-014).
- **FR-040**: What is played MUST be derived from the players' entries and from what is attached. One player's controller leaving MUST NOT change what the other drives: only the leaver's paddles center and their buttons release (SC-012), and their slot MUST be kept for them, so that the player still holding a controller is not handed the other's paddles, buttons or Joyport jack. A controller that returns MUST take its slot back. When no slot's controller is attached any more and the slots were filled by Automatic, Automatic MUST start over (FR-042) rather than leave the game port dead. A picked entry MUST NOT be rewritten by a disconnect. Anything that reports who is playing, the picker above all, MUST report what is played rather than what is saved.
- **FR-041**: In a player's submenu, picking a controller MUST take effect at once, whether or not that controller has given input. Picking a controller the other player holds MUST return the other player to Automatic. A picked controller that is not attached MUST stay checked, marked as not connected. Automatic MUST show the controller it has chosen, as "Automatic (description)", once it has one.
- **FR-042**: A player on Automatic MUST be given a controller from those no player has picked, as follows:
  - When Casso has seen two different controllers connect while it has been running, the first to connect is Player 1 and the second Player 2.
  - Otherwise the first controller to give real input is Player 1 and the next is Player 2. Real input is a button press, or any axis outside its deadzone, while Casso is active (FR-033).
  - Until any controller has given input, a lone attached controller is assigned to Player 1 and drives the port (FR-032), without being in use. If another arrives before either is used, the first to give input becomes Player 1, whichever it is.
  - A controller that reconnects returns to the slot it held (FR-040), and its reconnection is not a new connection for this ordering.

  A slot filled by Automatic is in use only once its controller has given input; a picked controller is in use as soon as it is attached, and the keys or the mouse as soon as they are picked. While one slot is in use and the other is neither in use nor held for a controller that left (FR-040), the one in use MUST drive PDL0, PDL1 and PB0-PB2 as a single controller always has, whichever slot it holds. Once both are in use, each MUST drive what its slot maps to.
- **FR-043**: Each profile MUST belong to one kind, Joystick, Paddle or Joyport (spec 036 FR-020), and a player's profile lists, in the picker and on the Controllers page, MUST hold only the profiles of that player's mode: Joystick or Paddle while the Joyport is off, Joyport while it is on. Each kind has a built-in profile, listed first: Default for Joystick, Paddles for Paddle, and Joyport for Joyport. Profiles saved before this change that bind PDL0 and not PDL1, or were created from the Paddles starting point, become Paddle profiles; the rest become Joystick profiles.
- **FR-044**: When Automatic gives a player a controller that differs from the last controller to hold that slot, Casso MUST show a notice reading "Player N: description". When it is the same controller, by unit identity (FR-018), no notice is shown. The last controller to hold each slot, however it came to hold it, MUST be saved across launches for this comparison. Every notice MUST stay up for its full time. A notice arriving while another is up MUST appear below it, and MUST slide up into the freed place when the one above expires, over the duration and easing the menus use to open. Every notice Casso shows, for a disconnect, a screenshot or a write-protect change as much as for a player, MUST go through the same stack. No notice is shown for an entry the user picked.

### Key Entities

- **Controller**: an attached input device. Has a unit identity usable to recognize it across reconnects and launches, a model identity (vendor and product) shared by every unit of the same model, a human-readable product description, a connected state, the set of controls it reports, and a current sample of those controls.
- **Player entry**: global, for each of the two players: Automatic, a picked controller, the keys or the mouse (Player 1 only), or Disabled (Player 2 only). A picked controller is a controller identity that may or may not currently be attached.
- **Player slots**: global, for each of the two players, the controller playing there (picked, or chosen by Automatic) and the player's mode, Joystick or Paddle (FR-037, FR-039). The slots may not repeat a controller (FR-036).
- **Last controller per slot**: global, the controller that last held each slot, saved only so that a notice is shown when the holder changes (FR-044). It plays no part in assigning.
- **Game-port axis budget**: per machine, how many analog axes exist -- four on the ][, ][+ and //e, two on the //c (FR-034). Bounds what can be assigned and what the settings offer.
- **Controller profile**: a named control mapping belonging to a controller model, shared by every unit of that model. Each model has three built-in profiles, Default, Paddles and Joyport, plus any the user creates, and each profile belongs to one kind, Joystick, Paddle or Joyport (FR-043, spec 036 FR-020). Machine-independent.
- **Active profile**: per controller unit and per mode, global: which of its model's profiles that controller uses with the Joyport off, and which with it on.
- **Calibration**: per DirectInput controller unit. Rest center and travel limits per axis, and whether they are automatic or user-set. Xbox-class controllers have none.
- **Deadzone**: per controller model.
- **Control mapping**: the content of a profile. For each game-port target, the list of assigned controls with their options (invert for an axis, threshold for an analog control driving a button, direction for a digital control driving an axis).
- **Game port state**: the existing PDL0-PDL3 positions and PB0-PB2 states. Controller samples feed it through the same path the keyboard and mouse sources use.
- **Notice stack**: the notices on screen, each with its own remaining time (FR-044).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with an Xbox controller can go from plugging it in to playing a joystick game in under 30 seconds, including selecting it.
- **SC-002**: Stick and button changes reach the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled.
- **SC-003**: With the stick untouched, paddle readings stay exactly at center across 10 seconds of sampling on a controller within its deadzone.
- **SC-004**: Both axes reach 0 and 255, and both buttons register, on each of: an Xbox-class controller, a generic USB gamepad, and a USB joystick.
- **SC-005**: After a disconnect the game port reads centered and released within 100 ms. A lone controller connecting with Player 1 on Automatic drives the game port within 2 seconds, and with two or more attached a controller drives it within 100 ms of its first input, with no user action in any case.
- **SC-006**: The players' entries, calibration and control mapping are restored on 100% of relaunches and reconnects with the same controller attached.
- **SC-007**: Controller support adds no measurable CPU cost while no controller is playing, whether or not controllers are attached, including while attached controllers are watched for their first input.
- **SC-008**: A user can reassign both buttons and switch the axes to the D-pad in under one minute from opening the controller settings.
- **SC-009**: A controller not yet recognized as a unit, but of a model with saved profiles, drives the game port with those profiles available on its first connection.
- **SC-010**: Switching the active profile from the input selector takes two actions or fewer and no more than 5 seconds, with no emulated machine reset.
- **SC-011**: Two players on separate paddles both drive the machine at once, with neither one's stick or button affecting the other's paddles or button line.
- **SC-012**: When one player's controller disconnects, the other player keeps driving their own paddles with no interruption to their readings.
- **SC-013**: With a stick that stays attached and a gamepad that is picked up and used, the gamepad plays as Player 1 on the first try, with no user action.
- **SC-014**: Two controllers connected in turn while Casso runs play as Player 1 and Player 2 in that order on 100% of tries. Once both are in use, a player's inputs stay where their slot maps them until a controller disconnects, and a second person joining never takes PDL0, PDL1, PB0 or PB1 from a first player who was already in use.

## Assumptions

- **Scope of mapping**: controllers drive the machine's analog axes (PDL0-PDL3, or PDL0/PDL1 on the //c) and PB0-PB2. Rumble and mapping controller controls to Apple II keyboard keys are out of scope for v1. The Guide button is not assignable.
- **Automatic calibration assumes the stick is at rest when it connects**; the Calibrate action exists for the case where it is not, and for sticks whose automatic limits never settle.
- **Device access**: XInput for Xbox-class controllers, DirectInput for everything else, with XInput devices filtered out of the DirectInput enumeration so no controller appears twice. DirectInput alone was ruled out because it reports an Xbox-class controller's two triggers on one shared axis by design, which control mapping (FR-020) cannot live with.
- **Xbox-class unit recognition is the slot and nothing else**: XInput exposes only a slot number (0-3), with no product, vendor or serial identity, and a controller can change slots across reconnects. The slot is used as the unit id anyway, because two connected Xbox controllers have to be two entries a user can pick between; it is not durable across a replug, which costs nothing, since calibration is the only per-unit data and Xbox-class controllers need none (FR-018a). The user tells two identical controllers apart by moving a stick and watching the live readings on the Controllers page. Recognizing the model still has to come from correlating the slot with its underlying device.
- **DirectInput unit recognition is best effort**: a device without a unique serial number may not be recognized as the same unit after moving to a different port; FR-018's model-level fallback covers that case.
- **XInput's four-controller limit** applies to Xbox-class controllers; DirectInput devices are not counted against it.
- **Focus**: controller input applies only while Casso is active (FR-033). Background input was considered and dropped: Windows documents XInput as gated by window focus, and XInput is required for Xbox controllers.
- **Absolute positioning by default**: the default mapping sets paddle position directly (joystick semantics); rate response is opt-in per binding (FR-021a).
- **Persistence location**: everything here is global: the players' entries, profiles, active profiles, calibration and deadzone, keyed by controller model or unit where they belong to a controller. A stick's physical quirks, the user's layouts for it and the controllers on the desk do not change with the emulated machine.
- **Profiles are chosen by hand**: activating a profile automatically when a particular disk is mounted is tracked by GH #78, which owns known-disk recognition. The profile storage here must allow a profile to be looked up by controller model and profile name so that work can associate disks with profiles. An association should hold one profile preference per mode for each controller model, and the one for the mode in effect applies (spec 036 Assumptions).
- **Profiles belong to a model**: a profile refers to that model's controls, so profiles are not shared across different models. Copying a profile to another model is out of scope.
- **Dependency on 032**: satisfied; the Dxui command-widgets feature is on master and merged into this branch.
- **Deadzone default**: the default deadzone comes from the device API's published recommendation where one exists (XInput publishes one); where none exists, Casso supplies a default. The values are a planning detail (research R8).
- **Button wiring of a second joystick**: joystick 2's first button on PB2 is the common wiring of two-joystick adapters, taken from memory of the hardware rather than from a document in hand. It is one of the questions put to a hardware owner on GH #156.
- **Planning notes (2026-09-27)**: XInput has no input events, so Xbox-class controllers that hold no slot are watched by comparing packet numbers at a slow rate (about 100 ms), and DirectInput ones by the events already in use; a controller that holds a slot is read at the measured rate as before. The notice stack is a Dxui control that owns arrival, timing, expiry and the slide, and the shell keeps the thread hand-off and the anchor. The middle ellipsis is a Dxui text option if the library has none.
