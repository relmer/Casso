# Feature Specification: Sirius Joyport Emulation

**Feature Branch**: `036-sirius-joyport`

**Created**: 2026-09-24

**Status**: Draft

**Input**: Emulate the Sirius Software Joyport (1981), the game-port adapter that let an Apple ][, ][+ or //e read two Atari CX40-style digital joysticks, so the thirty-odd games written for it can be played with the controllers Casso already supports (spec 034). The Joyport multiplexes ten switches onto the game port's three pushbutton inputs, using two annunciator outputs as select lines.

## Context

An Atari joystick has no analog axes: it is four direction switches and a fire button. The Apple game port has three pushbutton inputs, so the Joyport multiplexes. A program sets Annunciator 0 to select a jack and Annunciator 1 to select a pair of directions, then reads the three buttons. From the [Sirius Joyport owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf) (a searchable [OCR copy](Sirius%20Joyport%20Manual%20%28OCR%29.pdf) is kept beside this spec):

| Annunciator | Low | High |
|---|---|---|
| AN0 (`$C058` / `$C059`) | left jack, player 1 | right jack, player 2 |
| AN1 (`$C05A` / `$C05B`) | the left and right switches | the up and down switches |

| Button | Carries |
|---|---|
| PB0 (`$C061`) | fire |
| PB1 (`$C062`) | left, or up |
| PB2 (`$C063`) | right, or down |

The lines are active low: a closed switch reads with bit 7 clear, and an open one with bit 7 set. With nothing pressed, all three read the same as a pressed Apple button.

Joyport-aware games include Boulder Dash I and II, Miner 2049er I and II, Wavy Navy, Sea Dragon, Stellar 7, Spy's Demise, Jawbreaker II, Dino Eggs, Free Fall, Snake Byte, Buzzard Bait, Computer Foosball and Lemmings. Most of them have to be told to use it, through a setup menu or a key combination such as Ctrl-Shift-P.

The unit has two Atari jacks on its front edge and two 16-pin Apple game sockets at the rear of its top face, under a removable cover. Left and right are as seen facing the front of the unit, for both pairs. Two switches on the top, near the front edge, choose which of the four is read:

- **Apple / Atari select** (front or rear): toward the front reads the Atari jacks, toward the rear the Apple sockets. It has no middle position, so the two kinds are never connected at once.
- **Controller Select** (left, center or right): left reads only the left jack or socket, right only the right one, and center lets AN0 choose. The manual puts it as making "the right set, the left set or both sets of controllers" active.

Together they select any one of the four controllers, or both of one kind. In Apple mode the Joyport passes Apple paddles through to the paddle inputs, which is what Casso's four paddle inputs already do without it. So Casso shows the Apple / Atari switch as the control that turns the Joyport on: Atari mode is the Joyport in use, and Apple mode is the game port as it is with no Joyport. The Controller Select switch is emulated as it is. Wherever this spec says the Joyport is attached, it means the switch is in Atari mode on a machine that can use it.

## Clarifications

### Session 2026-09-24

- Q: Where is the Joyport attached and detached? -> A: In two places, which show and change the same per-machine setting. The Machine tab's device tree in Settings lists it as the device on the game port, with None and Sirius Joyport, beside the slot cards. The command bar's controller picker lists a checkable Sirius Joyport row, which attaches or detaches it at once without opening Settings; the check mark is also what shows the Joyport is attached. The Joyport sits outside the machine on the game port rather than in a slot, so attaching it takes effect on the next button read with no reset (FR-001, FR-002, FR-012).
- Q: Does the Controllers page in Settings show what the Joyport reads? -> A: Yes. While the Joyport is attached, the page shows a live light for each of the five switches of the controller in Editing, in place of the stick position and button lights, so a user can see the threshold and check a profile without booting a game (User Story 5, FR-015).

### Session 2026-09-27

Decisions from GH #156, where a user with two controllers found the one-stick sections of the manual's test program both reading the left controller, and found multiplayer hard to reach. Who is Player 1 and who is Player 2 is decided in spec 034's session of the same date.

- Q: Is the rear Controller Select switch emulated? -> A: Yes, with Left, Center and Right, defaulting to Center. Left and Right lock their jack whatever AN0 says. The manual's test program sets AN0 low for both one-stick sections and relies on this switch to pick the jack, which is why both read the left controller (User Story 6, FR-016).
- Q: Where is the Joyport turned on? -> A: On the Controllers page in Settings, drawn as the unit's own Apple / Atari switch, and by the picker's row, now labeled "Joyport (Atari mode)". The Machine tab's device tree no longer lists it. This replaces the 2026-09-24 answer: the user who raised GH #156 looked for it on the Controllers page, and one page now holds the Joyport, its switches and the players (User Story 4, FR-001, FR-012).
- Q: Is the Joyport setting per machine? -> A: No, global, like the players' entries. Whether it should be on follows the game being played, not the machine. The //c cannot use it, so there it reads as off and the setting is left alone (FR-002).
- Q: Why can the //c not use it? -> A: Three reasons. The Joyport plugs into the 16-pin game I/O socket, and the //c has only a 9-pin joystick port with no annunciator lines. The annunciator addresses program the //c's mouse and VBL interrupt while IOU access is on. And the //c has no PB2, which carries the right and down switches (FR-001, Assumptions).
- Q: What does a controller play with while the Joyport is on, if the user has never set one up for it? -> A: A built-in Joyport profile beside the Default, in which every stick and the D-pad steer and every fire-like button fires, so a game plays however the controller is held. It resets to its own mapping and cannot be renamed or deleted (User Story 7, FR-017).
- Q: Is a controller's chosen profile shared between play with and without the Joyport? -> A: No. Each controller keeps one choice for each mode. Turning the Joyport on or off switches to that mode's choice; with none made, the Default plays with it off and the Joyport profile with it on (FR-018).
- Q: Can any profile be picked in either mode? -> A: No. Each profile belongs to the mode it was created in, and each mode lists its own. Default belongs to normal mode and Joyport to Joyport mode. The user never sets this: creating a profile while the Joyport is on makes a Joyport-mode profile (FR-020).
- Q: With two controllers, which drives which jack? -> A: Player 1 drives the left jack and Player 2 the right, once both are playing. One controller playing alone drives both jacks, so a one-player game works whichever jack it reads. A player who leaves keeps their jack, and the one who remains does not regain it, since in a two-player game their stick would then move both players (User Story 3, FR-008).
- Q: What are the players called while the Joyport is on? -> A: Joyport left and Joyport right, in the picker and in the notices. Player 2's Disabled entry reads Same as left (FR-019).
- Q: Could one controller be read as an Apple joystick and an Atari stick at once, since the paddle inputs and the Joyport's switches are at separate addresses? -> A: No. The axes do not collide, but both kinds of fire button are read at `$C061`-`$C063` with opposite polarity, so only one kind can own them. A joystick game would steer and then read fire inverted, which looks broken rather than unplugged. The hardware never connects both either (FR-009, Assumptions).
- Q: Can a program read the Apple analog joystick while the Joyport is in Atari mode? -> A: Unknown. The manual does not say, and the Apple / Atari switch suggests it cannot. Kept as an open question for a hardware owner; until answered, the paddle inputs read as no paddle connected while the Joyport is on (FR-009, Assumptions).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Play a Joyport game with one controller (Priority: P1)

A user turns the Joyport on for an Apple ][+ or //e, boots a Joyport-aware game, selects the game's Joyport option, and plays it with whatever controller they already use: an Xbox controller's D-pad or stick, a flight stick, or a gamepad. Directions and fire respond the way an Atari joystick would, including a diagonal and fire held with a direction.

**Why this priority**: This is the feature. Nothing else in the spec is useful without it.

**Independent Test**: With a Joyport readout disk, each of the five switches of the left jack reads closed exactly while the matching direction or fire is held, and open otherwise. In Boulder Dash, the player moves in all four directions and can hold fire with a direction.

**Acceptance Scenarios**:

1. **Given** a //e with the Joyport attached and one controller selected, **When** a program sets AN0 low and AN1 high and reads the buttons while the stick is held up, **Then** PB0 and PB2 read open and PB1 reads closed.
2. **Given** the same setup, **When** the stick is held up and left at once, **Then** the program reads left closed with AN1 low and up closed with AN1 high.
3. **Given** a controller whose profile maps the D-pad to PDL0 and PDL1, **When** the D-pad is pressed, **Then** the matching direction switch closes with no threshold involved.
4. **Given** a controller whose profile maps an analog stick to PDL0, **When** the stick is moved slightly off center, **Then** no switch closes; **When** it is moved past the threshold, **Then** the switch for that direction closes.
5. **Given** a single controller and no multiplayer, **When** a program reads the right jack (AN0 high), **Then** it reads the same switches as the left jack, so a turn-based two-player game works by passing the controller.

---

### User Story 2 - The //e keeps working with the Joyport attached (Priority: P1)

A user with the Joyport attached to a //e presses Ctrl-Reset, or powers the machine on, and the machine resets normally. It does not enter the built-in self-test and does not reboot, even though the Joyport's idle lines read the same as Open Apple and Closed Apple held down. Holding Open Apple through a Ctrl-Reset still reboots the machine, the same as without the Joyport.

**Why this priority**: The real Joyport made every Ctrl-Reset on a //e enter self-test. An emulated one that did the same would make the //e unusable with it attached, so this ships with User Story 1 or not at all.

**Independent Test**: With the Joyport attached on a //e, repeat Ctrl-Reset and power-on twenty times each: the machine reaches the normal reset result every time. Then hold Open Apple through Ctrl-Reset: the machine reboots.

**Acceptance Scenarios**:

1. **Given** a //e with the Joyport attached and nothing held, **When** the user presses Ctrl-Reset, **Then** the machine resets and does not enter self-test or reboot.
2. **Given** the same setup, **When** the machine is powered on, **Then** it boots normally.
3. **Given** the same setup, **When** the user holds Open Apple through Ctrl-Reset, **Then** the machine reboots, as it would with no Joyport attached.
4. **Given** a //e with the Joyport attached, **When** the reset has finished and a Joyport game is running, **Then** the Open Apple and Closed Apple keys have no effect on the pushbutton inputs, as on the real machine.

---

### User Story 3 - Two players at once (Priority: P2)

Two people, each holding a controller, play a Joyport game that reads both joysticks. Player 1 is the left jack and Player 2 the right, decided with no setup as spec 034 FR-042 says, and neither one's input reaches the other's switches. A user with a stick that stays plugged in and a controller they pick up gets the controller they pick up on both jacks.

**Why this priority**: The Joyport's two jacks are what made it popular for two-player games, but single-player play is useful without them.

**Independent Test**: With the readout disk and two controllers attached at launch, use one: it closes the switches of both jacks. Then use the other: from then on each closes only its own jack's switches, including when both are moved at once.

**Acceptance Scenarios**:

1. **Given** two players playing, **When** player 2 holds fire and a program reads with AN0 low, **Then** fire reads open; **When** the program reads with AN0 high, **Then** fire reads closed.
2. **Given** the same setup, **When** player 2's controller is disconnected, **Then** the right jack reads every switch open, and player 1 still drives the left jack alone.
3. **Given** two players, **When** the slots' targets (joystick 0, joystick 1, single paddles) are set to anything, **Then** Player 1 is still the left jack and Player 2 the right; the targets apply only while the Joyport is off.
4. **Given** both players on Automatic and two controllers attached at launch, neither used yet, **When** the user presses fire on one, **Then** it becomes Joyport left, drives both jacks, and the other controller drives nothing.
5. **Given** that state, **When** the other controller is used, **Then** it becomes Joyport right, and the first drives the left jack alone.
6. **Given** Player 2 set to Same as left, **When** a second controller is used, **Then** Player 1 still drives both jacks.

---

### User Story 4 - Turning it on and off (Priority: P2)

A user turns the Joyport on with one click on the command bar's controller picker before starting a Joyport game, and off the same way afterward. The Controllers page in Settings shows the same setting as the switch on top of the unit: toward the front for Atari mode, and toward the rear for Apple mode, which is the game port with no Joyport. The setting is the same on every machine that can use it, and the picker's check mark shows it is on, so a user who switches to a game that expects an analog joystick knows why it does not respond. The //c does not offer the Joyport.

**Why this priority**: Joyport games need it on and every other game needs it off, so switching has to be quick. Still simple.

**Independent Test**: Turn the Joyport on from the picker on the //e, confirm the Controllers page shows Atari mode, relaunch, and confirm it is still on, and on the ][+ as well; switch to the //c and confirm neither place offers it; switch back to the //e and confirm it is on.

**Acceptance Scenarios**:

1. **Given** the //e, **When** the user checks the Joyport (Atari mode) row in the controller picker, **Then** the Joyport is on at once, without resetting the machine, and the row shows a check mark.
2. **Given** the Joyport turned on from the picker, **When** the user opens the Controllers page, **Then** the switch is toward the front and labeled Atari mode; **When** the user moves it to Apple mode, **Then** the Joyport is off at once and the picker's row is unchecked.
3. **Given** the Joyport on, **When** the user relaunches or switches to the ][+, **Then** it is still on.
4. **Given** the //c, **When** the user opens the controller picker or the Controllers page, **Then** neither offers the Joyport, and the game port reads as it does with no Joyport; **When** the user switches back to the //e, **Then** the Joyport is on as it was left.
5. **Given** Apple mode, **When** the user looks at the Controller Select switch, **Then** it is disabled, since it has no effect there.

---

### User Story 5 - See the switches on the Controllers page (Priority: P2)

With the Joyport attached, a user opens the Controllers page in Settings and moves the stick or presses the D-pad and fire. Five lights (up, down, left, right, fire) show which switches the controller in Editing is closing, live, so the user can see how far the stick has to travel before a direction closes, and whether a profile puts fire on the button they expect, without booting a game.

**Why this priority**: Joyport games have to be set up in-game before they read the stick at all, so a mapping problem is hard to tell apart from a game-setup problem without it. It reuses the page's existing live readings.

**Independent Test**: With the Joyport attached and the Controllers page open, each direction and fire lights its own light exactly while it is held, a diagonal lights two, and a stick moved short of the threshold lights none.

**Acceptance Scenarios**:

1. **Given** the Joyport attached and the Controllers page open, **When** the user pushes the stick up past the threshold, **Then** the up light turns on, and turns off when the stick returns.
2. **Given** the same setup, **When** the user pushes the stick diagonally up and left, **Then** the up and left lights are both on.
3. **Given** two players playing, **When** the user moves Editing to player 2's controller, **Then** the lights follow player 2's controller, and the page shows that it is the right jack.
4. **Given** the Joyport detached, **When** the user opens the Controllers page, **Then** it shows the stick position and button lights as before.

---

### User Story 6 - The Controller Select switch (Priority: P2)

A user runs the test program from the Joyport manual, which asks for the rear switch at Right for one stick and at Left for the other. They set the switch in Settings next to the Joyport, and each section reads the stick on that side, as the hardware does.

**Why this priority**: The manual's own test program depends on it, and without it both one-stick sections read the left jack. Games set AN0 themselves and work at Center, so it is not needed for play.

**Independent Test**: With two controllers each holding a player slot, run the manual's test program: the Right section reads only player 2's controller and the Left section only player 1's, and the Center section reads each in turn.

**Acceptance Scenarios**:

1. **Given** the switch at Right and AN0 low, **When** player 2 holds fire, **Then** fire reads closed; **When** player 1 holds fire instead, **Then** fire reads open.
2. **Given** the switch at Left and AN0 high, **When** player 1 holds up with AN1 high, **Then** PB1 reads closed.
3. **Given** the switch at Center, **When** a program changes AN0, **Then** the jack follows AN0, as before this story.
4. **Given** the switch at Left or Right, **When** the user relaunches or switches machines, **Then** the position is kept.

---

### User Story 7 - Any controller plays a Joyport game as it is held (Priority: P1)

A user attaches the Joyport and plays with the D-pad, the left stick or the right stick, and fires with whichever face button, bumper or trigger is under their finger, without making a profile. Someone who prefers the D-pad and someone who prefers the stick both get what they expect.

**Why this priority**: The Default profile steers with the left stick and fires with A only, which surprised the first user to try the Joyport. Needing a profile before a game works defeats the one-click attach.

**Independent Test**: On an Xbox controller with nothing set up, attach the Joyport: each of the D-pad, left stick and right stick closes the direction switches, and each of A, B, X, Y, both bumpers and both triggers closes fire. Detach it: the controller plays its Default again.

**Acceptance Scenarios**:

1. **Given** a controller with no profile chosen for Joyport play, **When** the Joyport is attached, **Then** it plays the Joyport profile, shown checked in the profile list.
2. **Given** a profile created and picked while the Joyport is on, **When** the user turns the Joyport off and on again, **Then** the normal-mode choice plays while it is off, and their pick plays again once it is on.
3. **Given** the Joyport profile edited, **When** the user presses Reset profile, **Then** it returns to the Joyport mapping, not the Default's.
4. **Given** the Joyport profile selected on the Controllers page, **When** the user looks for Rename and Delete, **Then** both are unavailable, as they are for the Default.
5. **Given** the Joyport on, **When** the user opens a player's profile list, **Then** it shows the Joyport profile and the profiles created in Joyport mode, and no normal-mode profile (FR-020).

---

### Edge Cases

- **No controller at all**: with the Joyport attached and no controller selected, every switch reads open, which is what an Apple with a Joyport and no joysticks plugged in reads.
- **Keys as joystick**: with arrows-to-joystick on, the arrow keys drive player 1's direction switches and the fire key drives fire, through the same rules as any other source that drives PDL0, PDL1 and PB0. The fire key is X. The Alt keys, which also fire in arrows-to-joystick mode today, do not drive the Joyport, because on the //e they are Open Apple and Closed Apple (FR-010).
- **Mouse as paddle**: the mouse does not drive the Joyport; an Atari joystick has no paddle for it to stand in for. The picker leaves mouse-as-paddle out while the Joyport is on (FR-009).
- **The paddle inputs**: an Atari joystick has no potentiometers, so while the Joyport is attached, controllers drive the switches and not the paddle inputs. The paddle inputs read as no paddle connected.
- **PB1 and PB2 bindings**: a profile's PB1 and PB2 bindings have no Joyport switch to drive and are ignored while the Joyport is attached. They are kept in the profile.
- **Opposite directions at once**: a D-pad or pair of bindings that reports left and right together closes neither, since a physical Atari stick cannot close both.
- **A program that never sets the annunciators**: it reads whichever jack and switch pair the annunciators were left at, as on real hardware.
- **Shift-key mod**: on the //e, PB2 carries the Joyport's right-or-down switch instead of the Shift key, so programs that detect Shift through PB2 do not see it while the Joyport is attached. This is also true of the real hardware.
- **The //c IOU**: on the //c, `$C058`-`$C05F` program the mouse and the VBL interrupt when IOU access is on. That behavior is unchanged; the //c never has a Joyport.
- **Double hi-res on the //e**: AN3 (`$C05E`/`$C05F`) keeps selecting double hi-res. The Joyport uses only AN0 and AN1.
- **Machine switch with a controller held**: switching to the //c returns the pushbuttons and paddles to the normal game-port behavior at once, with nothing stuck pressed.
- **The picker used while Settings is open**: the Controllers page shows the change at once, and pressing OK does not undo it.
- **Reset while a switch is held**: during the post-reset window the Joyport releases every line, so a held fire button cannot trigger self-test or a reboot either.
- **A controller never touched**: it closes no switch, so a stick left plugged in does not keep the controller in the user's hands off either jack (spec 034 FR-042).
- **The wrong controller on the left jack**: the user picks the controller they want from Joyport left's submenu in the picker, and the other player returns to Automatic (spec 034 FR-041).
- **A second controller bumped while one person plays**: it becomes Joyport right and takes the right jack, and the player keeps the left. A one-player game that reads the right jack would then stop responding, which setting Joyport right to Same as left prevents.
- **A trigger resting on an axis**: a gamepad that reports its analog triggers as axes has them used only as fire, never as a stick, since a trigger at rest sits at one end of its travel and would hold a direction closed.
- **The Controller Select switch and a single controller**: with one controller in use it drives both jacks, so it reads the same at Left, Center and Right.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST have a Joyport setting with two positions, Apple mode and Atari mode, defaulting to Apple mode. Atari mode is the Sirius Joyport in use with its Atari jacks selected; Apple mode is the game port as it is with no Joyport. It MUST be offered in two places that show and change the same setting: a Joyport section on the Controllers page in Settings, and a checkable "Joyport (Atari mode)" row in the command bar's controller picker. The section MUST show it as the unit's own switch seen from above, with the unit's front toward the bottom: the knob toward the bottom is Atari mode and toward the top is Apple mode, and the position in effect is labeled beside it. The Machine tab's device tree MUST NOT list the Joyport. On the //c neither place MUST offer it.
- **FR-002**: The Joyport setting and the Controller Select position MUST be saved globally, not per machine, and restored at launch. A change from either place MUST take effect at once, without resetting the emulated machine and without waiting for OK. On the //c the Joyport MUST read as off whatever the setting is, and the setting MUST be left as it was, so switching to a machine that can use it restores it. The first launch after this change MUST adopt the setting saved for the machine being launched.
- **FR-003**: The machine MUST record the state of Annunciators 0, 1 and 2 as programs set them through `$C058`-`$C05D`, on every machine that has them, with or without a Joyport. The //c's use of `$C058`-`$C05F` for its mouse and VBL interrupt, and the //e's use of AN3 for double hi-res, MUST be unchanged.
- **FR-004**: While the Joyport is attached, a read of PB0, PB1 or PB2 MUST return the switch that the current AN0 and AN1 settings select, per the table in Context, with a closed switch reading bit 7 clear and an open one bit 7 set. The value MUST reflect the annunciator settings at the moment of the read, not at the time of an earlier sample.
- **FR-005**: Each player's switches MUST come from that controller's active profile. A PDL0 binding past the switch threshold toward its low end closes left, and toward its high end closes right. PDL1 does the same for up and down. The PB0 bindings drive fire. A digital binding (a D-pad, a hat or a button pair) closes its switch whenever it is pressed. Each axis is judged on its own, so a diagonal closes one horizontal and one vertical switch.
- **FR-006**: The switch threshold MUST be a fixed fraction of an axis's travel from center, measured after the profile's deadzone and calibration, and MUST close a switch well before full deflection, as an Atari stick does.
- **FR-007**: A binding that reports both directions of one axis at once MUST close neither switch of that pair.
- **FR-008**: While one controller is playing alone (spec 034 FR-042), or the arrow keys are, it MUST drive the left jack and MUST also appear on the right jack. Once two players are playing, Player 1's controller MUST drive the left jack and Player 2's the right jack, whatever the slots' targets. A jack whose controller is absent MUST read every switch open, and the player who remains MUST NOT regain it (spec 034 FR-040). With Player 2 set to Same as left, Player 1 MUST drive both jacks always.
- **FR-009**: While the Joyport is attached, the paddle inputs MUST read as no paddle connected, and the mouse MUST NOT drive the Joyport. While the Joyport is on, the picker MUST NOT offer mouse-as-paddle, and a controller MUST NOT be offered to programs as an analog joystick and an Atari stick at once.
- **FR-010**: On the //e, while the Joyport is attached and active, the Open Apple, Closed Apple and Shift keys MUST NOT change what PB0, PB1 and PB2 read. This follows the real machine, where the Joyport's idle lines already read as those keys held down.
- **FR-011**: After every reset, whether from power-on, Ctrl-Reset or a machine switch, the Joyport MUST release every line for a window long enough for the machine's reset handling to read the pushbuttons, and MUST become active when that window ends. While released, the pushbuttons MUST read as they would with no Joyport attached, including the //e's Open Apple and Closed Apple keys, so holding a key through a reset still has its usual effect.
- **FR-012**: The controller picker's "Joyport (Atari mode)" row MUST be checked exactly while the Joyport is on, whichever place turned it on.
- **FR-013**: With the Joyport not attached, every pushbutton, paddle and key behavior MUST be exactly as before this feature.
- **FR-014**: The switch logic, the annunciator selection, the reset window, and the choice of which controller drives which jack MUST be testable without a real controller or a real Joyport, as spec 034's controller logic is.
- **FR-015**: While the Joyport is attached, the Controllers page MUST show a live light for each of the five switches (up, down, left, right, fire) of the controller in Editing, lit exactly when that switch would read closed, in place of the stick position and button lights. It MUST show which jack that controller drives. With the Joyport detached the page MUST be unchanged.
- **FR-016**: Casso MUST have a Controller Select setting with Left, Center and Right, defaulting to Center, saved globally with the Joyport setting (FR-002). It MUST be shown on the Controllers page beside the Joyport switch, as a switch with its three positions laid out left to right as on the unit, and MUST be disabled in Apple mode. At Left or Right, every read MUST come from that jack whatever AN0 says; at Center, AN0 MUST choose, per the table in Context. Changing it MUST take effect on the next read, with no reset.
- **FR-017**: Every controller model MUST have a built-in Joyport profile beside its Default, which leads the list of Joyport-mode profiles (FR-020). In it, the primary stick, the D-pad and any second stick MUST steer, and the controller's fire-like buttons MUST fire:
  - On an Xbox-class controller: both sticks and the D-pad steer; A, B, X, Y, both bumpers and both triggers fire; Back, Start and the stick clicks do not.
  - On any other gamepad: the primary stick and the D-pad steer, and a second stick steers when the device has one: Z and Rz when it has both, otherwise Rx and Ry. Every button fires.
  - On a joystick or wheel: the primary stick and the D-pad steer, and every button fires. No other axis steers, since it may be a throttle, twist or pedal that rests off center.
  - An axis that is an analog trigger MUST never steer.
  
  Reset MUST restore the Joyport profile's own mapping. It MUST NOT be renamed or deleted. A user profile already called Joyport becomes the built-in one, keeping its mapping.
- **FR-018**: Each controller's chosen profile MUST be kept separately for play with and without the Joyport, and saved. A profile picked from the picker or the Controllers page MUST be recorded for the mode in effect. Attaching or detaching the Joyport MUST switch every controller to the choice for the new mode; with none made for it, a controller MUST play the Default without the Joyport and the Joyport profile with it.
- **FR-019**: While the Joyport is on, the picker and the assignment notices (spec 034 FR-008, FR-044) MUST call the players Joyport left and Joyport right in place of Player 1 and Player 2, and Player 2's Disabled entry MUST read Same as left. While Joyport right is on Automatic with no controller playing, its row MUST read "Joyport right: same as left". The notice for a controller playing alone MUST read "Joyport left and right: description".
- **FR-020**: Every profile MUST belong to one mode, normal or Joyport, fixed when it is created: a profile created while the Joyport is on is a Joyport-mode profile. The picker and the Controllers page MUST list only the profiles of the mode in effect, that mode's built-in profile first: Default in normal mode and Joyport in Joyport mode. Moving the Joyport switch on the Controllers page MUST swap the list in place. Creating a profile MUST offer the same starting points in both modes: the Default mapping, the Joyport mapping, Paddles, or a copy of any profile of either mode. Resetting a profile MUST restore the built-in mapping of its mode. Profiles saved before this change are normal-mode profiles. Names stay unique per model across both modes (spec 034 FR-027).

### Key Entities

- **Joyport setting**: global, Apple mode or Atari mode. Apple mode is the game port as it has always been emulated; Atari mode is the Sirius Joyport in use.
- **Annunciator state**: per machine, the on or off state of AN0, AN1 and AN2 as last set by a program.
- **Atari joystick state**: per jack, whether each of fire, up, down, left and right is closed, derived from the controller that drives the jack.
- **Reset window**: the interval after a reset during which the Joyport releases every line.
- **Controller Select position**: global, Left, Center or Right.
- **Joyport profile**: per controller model, the built-in profile played with the Joyport attached when no other is chosen.
- **Chosen profile per mode**: per controller, the profile chosen for play without the Joyport and the one chosen for play with it; either may be unset.
- **Profile mode**: per profile, normal or Joyport, fixed when the profile is created (FR-020).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a readout disk, all ten switches (five per jack) read correctly in both AN1 states, for both jacks: 20 of 20 combinations.
- **SC-002**: A switch change reaches the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled, the same as controller input in spec 034.
- **SC-003**: With the Joyport attached on a //e, 20 of 20 Ctrl-Resets and 20 of 20 power-ons end in a normal reset, with no self-test and no unintended reboot.
- **SC-004**: In a Joyport-aware game, a user can move in all eight directions and fire, including fire held with a direction, within one minute of attaching the Joyport, with no mapping changes to a controller whose profile already works in joystick games.
- **SC-005**: Two players on two controllers each drive only their own jack, with neither one's input changing the other's switches, over five minutes of simultaneous play.
- **SC-006**: With the Joyport not attached, every existing game-port and controller test passes unchanged.
- **SC-007**: The Joyport setting and the Controller Select position are restored on 100% of relaunches, and are the same on every machine that can use them.
- **SC-008**: With two controllers each holding a slot, every step of the manual's test program passes in its one-stick and two-stick sections, with each step answered only on the controller for the jack it asks about, at the Controller Select position the program asks for.
- **SC-009**: With a stick and a gamepad both attached and only the gamepad used, a one-player Joyport game responds to the gamepad on the first try once the game's own Joyport option is selected, with no setup in Casso.
- **SC-010**: Bandits, which takes Ctrl-@ to select the Joyport (GH #155), plays with a controller on the Joyport profile with no mapping changes.

## Assumptions

- **Source of truth**: the [Sirius Joyport owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf) and public write-ups (Wikipedia, Nerdly Pleasures, Lukazi's Apple II Projects) define the behavior. The implementation is clean-room: no GPL emulator source is read or copied.
- **AN0's sense**: low (`$C058`) selects the left jack and player 1, as the owner's manual gives it. At least one emulator documents the opposite sense for AN0 and a reversed AN1, and reports Wavy Navy steering with up and down on a ][+. The manual takes precedence, and Wavy Navy on the ][+ is a validation case for it.
- **Controller Select switch**: emulated (FR-016). Center, where AN0 selects the jack, stays the default because it is what two-player games need and what every game works with.
- **Reset window length**: a few hundred milliseconds of emulated time covers the //e's reset handling with room to spare. The exact length is a planning decision, verified by SC-003.
- **Switch threshold**: about half of an axis's travel from center. The exact fraction is a planning decision, and is not user-adjustable in this spec.
- **Power-on annunciator state**: all annunciators start off (low) at power-on. A Ctrl-Reset changes them only if the machine's reset code writes them.
- **No new settings page**: the switches come from each controller's profile. The built-in Joyport profile (FR-017) is an ordinary profile a user can edit or copy, so a user who wants a D-pad-only feel edits it or picks another profile for Joyport play.
- **The //c**: a Joyport cannot be connected to it. It has no 16-pin game I/O socket, and its 9-pin joystick connector has no annunciator lines; its annunciator addresses program the mouse and the VBL interrupt while IOU access is on; and it has no PB2 for the right and down switches. Emulating one anyway would describe a machine that never existed and could disturb the mouse in //c software. The IIgs is not emulated.
- **Apple mode**: the Joyport's Apple paddle passthrough is not emulated as such, since Casso's four paddle inputs already provide what it did. The Apple / Atari switch is shown, with Apple mode meaning the Joyport is off (FR-001).
- **Paddle inputs in Atari mode (open question)**: the manual does not say whether the paddle inputs can still be read in Atari mode, and its Apple / Atari switch, which has no middle position, suggests they cannot. FR-009 keeps them reading as no paddle connected until a hardware owner answers. Whatever the answer, Casso does not offer one controller as both kinds at once. The paddle inputs (`$C064`-`$C067`) and the Joyport's switches are at separate addresses, but both kinds of fire button are read at `$C061`-`$C063` with opposite polarity, so a joystick game would read fire inverted, and a game that checks the paddle inputs to find out which controller is present could choose the wrong one.
- **Earlier validation**: the first pass through the manual's test program closed the switches on both jacks for its one-stick sections, because only Center was emulated. That hid the missing Controller Select switch; SC-008 answers each step on one jack only.
- **Questions for a hardware owner** (GH #156): whether the paddle inputs can be read in Atari mode; whether the Atari jacks' switches reach the pushbutton inputs in Apple mode; which jack one-player Joyport games read; and which button line a second Apple joystick uses (spec 034 Assumptions).
- **Disk associations**: not built; GH #78 owns them. An association should hold one profile preference per mode for each controller model, and the one for the mode in effect applies; with none for that mode, the controller's own choice plays.
- **Planning notes (2026-09-27)**: the vertical switch is an orientation option on Dxui's toggle, and the three-position switch a new Dxui control in the same style. The setting keeps the saved values the game-port adapter used, None and Sirius Joyport, moved from each machine's preferences to the global ones.
- **Dependency on spec 034**: controller profiles, the player slots, Automatic, the notices and the controller picker come from spec 034.
