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

The unit has two Atari jacks on its front edge and two 16-pin Apple game sockets at the rear of its top face, under a removable cover. Left and right are as seen facing the front of the unit, for both pairs. Two switches on the top face choose which of the four is read. The Apple / Atari select is near the front edge and the Controller Select sits just behind it, which is why the manual's test program calls them the front switch and the rear or back switch:

- **Apple / Atari select** (front or rear): toward the front reads the Atari jacks, toward the rear the Apple sockets. It has no middle position, so the two kinds are never connected at once.
- **Controller Select** (left, center or right): left reads only the left jack or socket, right only the right one, and center lets AN0 choose. The manual puts it as making "the right set, the left set or both sets of controllers" active.

Together they select any one of the four controllers, or both of one kind. In Apple mode the Joyport passes Apple paddles through to the paddle inputs, which is what Casso's four paddle inputs already do without it. So Casso shows the Apple / Atari switch as the control that turns the Joyport on: Atari mode is the Joyport in use, and Apple mode is the game port as it is with no Joyport. Whether the Controller Select switch is emulated is not decided (FR-016); until it is, Casso behaves as if it is at Center. Wherever this spec calls the Joyport attached, it means the switch is in Atari mode on a machine that can use it. (Superseded 2026-09-28: Casso no longer shows the Apple / Atari switch, and the Controller Select switch is not emulated; FR-016 is closed as not needed.)

Since 2026-09-28 each player chooses a mode: Joystick, Joyport left (Atari), Joyport right (Atari) or Paddle, and Player 2 can also be Automatic. The Joyport is in Atari mode whenever at least one player is on a Joyport jack, and in Apple mode otherwise. Wherever this spec calls the Joyport attached, on or in effect, it means at least one player is on a Joyport mode, on a machine that can use it. A player's jack takes the place of the Controller Select switch for the manual's test program: its one-stick sections read one jack, and a player can be put on either one.

## Clarifications

### Session 2026-09-24

- Q: Where is the Joyport attached and detached? -> A: In two places, which show and change the same per-machine setting. The Machine tab's device tree in Settings lists it as the device on the game port, with None and Sirius Joyport, beside the slot cards. The command bar's controller picker lists a checkable Sirius Joyport row, which attaches or detaches it at once without opening Settings; the check mark is also what shows the Joyport is attached. The Joyport sits outside the machine on the game port rather than in a slot, so attaching it takes effect on the next button read with no reset (FR-001, FR-002, FR-012).
- Q: Does the Controllers page in Settings show what the Joyport reads? -> A: Yes. While the Joyport is attached, the page shows a live light for each of the five switches of the controller in Editing, in place of the stick position and button lights, so a user can see the threshold and check a profile without booting a game (User Story 5, FR-015).

### Session 2026-09-27

Decisions from GH #156, where a user with two controllers found the one-stick sections of the manual's test program both reading the left controller, and found multiplayer hard to reach. Who is Player 1 and who is Player 2 is decided in spec 034's session of the same date.

- Q: Is the Controller Select switch emulated? -> A: Not decided. Casso behaves as if it is at Center. The manual's test program sets AN0 low for both one-stick sections and relies on this switch, which it calls the rear switch, to pick the jack; that is why both sections read the left controller in Casso. Games set AN0 themselves and work at Center. Emulating it, with Left and Right locking their jack whatever AN0 says, is proposed in User Story 6 and FR-016 and waits on a decision. (Superseded 2026-09-28: closed as not needed; each player's explicit jack covers what the switch did for the test program.)
- Q: Where is the Joyport turned on? -> A: On the Controllers page in Settings, drawn as the unit's own Apple / Atari switch, and by the picker's row, now labeled "Joyport (Atari mode)". The Machine tab's device tree no longer lists it. This replaces the 2026-09-24 answer: the user who raised GH #156 looked for it on the Controllers page, and one page now holds the Joyport, its switches and the players (User Story 4, FR-001, FR-012). (Superseded 2026-09-28: the switch, the picker row and the setting are removed; the Joyport is on while a player is on a Joyport mode.)
- Q: Is the Joyport setting per machine? -> A: No, global, like the players' entries. Whether it should be on follows the game being played, not the machine. The //c cannot use it, so there it reads as off and the setting is left alone (FR-002). (Superseded 2026-09-28: there is no Joyport setting; the players' modes are global, and on the //c a Joyport mode plays as Joystick and is kept.)
- Q: Why can the //c not use it? -> A: Three reasons. The Joyport plugs into the 16-pin game I/O socket, and the //c has only a 9-pin joystick port with no annunciator lines. The annunciator addresses program the //c's mouse and VBL interrupt while IOU access is on. And the //c has no PB2, which carries the right and down switches (FR-001, Assumptions).
- Q: What does a controller play with while the Joyport is on, if the user has never set one up for it? -> A: A built-in Joyport profile beside the Default, in which every stick and the D-pad steer and every fire-like button fires, so a game plays however the controller is held. It resets to its own mapping and cannot be renamed or deleted (User Story 7, FR-017).
- Q: Is a controller's chosen profile shared between play with and without the Joyport? -> A: No. Each controller keeps one choice for each mode. Turning the Joyport on or off switches to that mode's choice; with none made, the Default plays with it off and the Joyport profile with it on (FR-018). (Superseded 2026-09-28: a controller follows its own player's mode, so only a player on a Joyport jack plays its Joyport choice.)
- Q: Can any profile be picked in either mode? -> A: No. Each profile belongs to the mode it was created in, and each mode lists its own. Default belongs to normal mode and Joyport to Joyport mode. The user never sets this: creating a profile while the Joyport is on makes a Joyport-mode profile (FR-020). (Superseded 2026-09-28: a profile created for a player takes that player's kind, Joyport for a player on either jack.)
- Q: What can a new profile start from? -> A: Only the mode in effect: the Default mapping, Paddles or a copy of a normal-mode profile in normal mode, and the Joyport mapping or a copy of a Joyport-mode profile in Joyport mode. The profile to copy is chosen from a list, which leaves out the built-in profile while it still matches the built-in mapping (FR-020).
- Q: How do paddles fit beside the Joyport? -> A: As a third kind of profile. Each player has a mode, Joystick or Paddle, while the Joyport is off; with it on, both players are Atari sticks and Paddle cannot be chosen. Profiles belong to Joystick, Paddle or Joyport, and each player's list follows that player's mode. Mouse-as-paddle is offered only to a player in Paddle mode, so it can never be on with the Joyport (FR-009, FR-020). (Superseded 2026-09-28: the Joyport jacks are modes beside Joystick and Paddle, so one player can be on Paddle while the other is on the Joyport; mouse-as-paddle is offered only to a Player 1 in Paddle mode.)
- Q: How are the switch's positions labeled? -> A: "Apple (rear)" above the switch and "Atari (front)" below it, both centered on it, with "Joyport" to the left, centered vertically. Labels beside the knob's two positions sat too close together (FR-001). (Superseded 2026-09-28: the switch is removed from the page.)
- Q: What is the heading above the switch lights? -> A: "Atari joystick", followed by the jack the controller in Editing drives, as in "Atari joystick: left jack". It had read "Joyport", the same as the switch's label (FR-015).
- Q: With two controllers, which drives which jack? -> A: Player 1 drives the left jack and Player 2 the right, once both are playing. One controller playing alone drives both jacks, so a one-player game works whichever jack it reads. A player who leaves keeps their jack, and the one who remains does not regain it, since in a two-player game their stick would then move both players (User Story 3, FR-008). (Superseded 2026-09-28: each player's mode gives its jack; a lone Joyport player drives both jacks while the other jack is free.)
- Q: What are the players called while the Joyport is on? -> A: Joyport left and Joyport right, in the picker and in the notices. Player 2's Disabled entry reads Same as left (FR-019). (Superseded 2026-09-28: the players keep the labels Player 1 and Player 2, and the jack is shown by the mode.)
- Q: Could one controller be read as an Apple joystick and an Atari stick at once, since the paddle inputs and the Joyport's switches are at separate addresses? -> A: No. The axes do not collide, but both kinds of fire button are read at `$C061`-`$C063` with opposite polarity, so only one kind can own them. A joystick game would steer and then read fire inverted, which looks broken rather than unplugged. The hardware never connects both either (FR-009, Assumptions).
- Q: Can a program read the Apple analog joystick while the Joyport is in Atari mode? -> A: Yes. The reporter confirmed on the hardware that paddles and joysticks in the rear sockets are always readable at PDL(0)-PDL(3); the Apple / Atari switch at the front only cuts off their buttons, which the Atari jacks use. Casso does not emulate the rear sockets in Atari mode, because no known software reads paddles and Atari sticks together, so the paddle inputs read as no paddle connected. If such a game turns up, each player's Joystick / Paddle mode would drive the paddle inputs in Atari mode, with no buttons (FR-009, Assumptions). (Superseded 2026-09-28: a player on Joystick or Paddle beside a Joyport player now drives its paddle inputs, with no buttons; see the session below.)

### Session 2026-09-28

The owner replaced the Joyport's global Apple / Atari switch with a Joyport mode for each player. Spec 034's session of the same date records the picker and profile side.

- Q: How is the Joyport turned on? -> A: By putting a player on a Joyport jack. Each player's mode list reads, in order, Joystick, Joyport left (Atari), Joyport right (Atari), Paddle; Player 2's list starts with Automatic, its default. The list is in each player's submenu of the command bar picker and on each player's row on the Controllers page. The Joyport is on whenever at least one player is on a Joyport mode, and off otherwise. The global setting, the Apple / Atari switch, the picker's "Joyport (Atari mode)" row and its menu command are removed (FR-001, FR-002, FR-012, FR-021).
- Q: What does Automatic mean? -> A: Player 1's mode, except that with Player 1 on a Joyport jack it means the other jack (FR-021).
- Q: What about the //c? -> A: It does not offer the two Joyport entries, since it cannot take a Joyport. A Joyport mode saved earlier plays as Joystick there and is kept for the next machine (FR-002, FR-021).
- Q: What becomes of the old saved setting? -> A: It is read once to migrate. A global value of Sirius Joyport, or where the global was never set the launched machine's value, becomes Player 1 on Joyport left and Player 2 on Automatic. It runs only while no player mode has been saved, since the saved modes mark it done, so a machine's old value is never adopted again; the old global key is then removed, and the machines' own values are left for older builds (FR-002).
- Q: Can both players take the same jack? -> A: No. A jack held by one player is shown disabled in the other player's list, like any taken choice, on the picker and on the Controllers page, whose drop-down gains disabled items for this. A Disabled Player 2 holds no jack (FR-021).
- Q: Which jacks does a lone Joyport player drive? -> A: Both, while the other jack is free: no player on it, or its player not playing. A one-player game then works whichever jack it reads. A player who leaves keeps their jack, which reads open, as before (FR-008).
- Q: What does a player on Joystick or Paddle do while the other is on the Joyport? -> A: It drives its paddle inputs, since the rear sockets are readable in Atari mode, but its buttons do not reach the machine, because the Joyport owns all three button lines, as on the hardware. It plays its mode as a lone player would: Joystick drives joystick 0 (PDL0 and PDL1), Paddle drives paddle 0 (PDL0). The paddle inputs read as no paddle connected only while every playing player is on the Joyport. On the Controllers page that player's button binding rows are disabled, and a notice with the warning triangle shows under its row: "This controller's buttons are disabled because Player N is using the Joyport." (FR-009, FR-022)
- Q: Which profiles does a player's list show? -> A: The kind of its own mode: Joystick, Paddle, or Joyport for either jack. Only the players on a Joyport jack switch to the Joyport kind (FR-018, FR-020).
- Q: What do the keys and the mouse do? -> A: Keys as joystick are offered to Player 1 in Joystick or a Joyport mode; on a Joyport mode the keys drive Player 1's jack or jacks, not the paddle inputs. Mouse as paddle is offered only to a Player 1 in Paddle mode (FR-009, FR-010).
- Q: What do the picker and the notices call the players now? -> A: Player 1 and Player 2, always. The row reads "Player 1: <what plays>", ending in " (paddle)" in Paddle mode as before and with no suffix for any other mode, the mode being in the submenu. A notice for a Joyport player adds its jack: "Player 1 (Joyport left): description", "Player 2 (Joyport right): description", or "Player 1 (Joyport left and right): description" while it drives both. The Controllers page's note beside each row gives what the player drives: "left jack", "right jack", "both jacks", "joystick 0", "paddle 1" and so on (FR-015, FR-019).
- Q: Is the Controller Select switch emulated (FR-016)? -> A: No; closed as not needed. The manual's test program uses it to pick the jack for its one-stick sections, and a player can now be put on either jack, which covers that (FR-016, SC-008).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Play a Joyport game with one controller (Priority: P1)

A user puts Player 1 on Joyport left (Atari) on an Apple ][+ or //e, which turns the Joyport on, boots a Joyport-aware game, selects the game's Joyport option, and plays it with whatever controller they already use: an Xbox controller's D-pad or stick, a flight stick, or a gamepad. Directions and fire respond the way an Atari joystick would, including a diagonal and fire held with a direction.

**Why this priority**: This is the feature. Nothing else in the spec is useful without it.

**Independent Test**: With a Joyport readout disk, each of the five switches of the left jack reads closed exactly while the matching direction or fire is held, and open otherwise. In Boulder Dash, the player moves in all four directions and can hold fire with a direction.

**Acceptance Scenarios**:

1. **Given** a //e with the Joyport attached and one controller selected, **When** a program sets AN0 low and AN1 high and reads the buttons while the stick is held up, **Then** PB0 and PB2 read open and PB1 reads closed.
2. **Given** the same setup, **When** the stick is held up and left at once, **Then** the program reads left closed with AN1 low and up closed with AN1 high.
3. **Given** a controller whose profile maps the D-pad to PDL0 and PDL1, **When** the D-pad is pressed, **Then** the matching direction switch closes with no threshold involved.
4. **Given** a controller whose profile maps an analog stick to PDL0, **When** the stick is moved slightly off center, **Then** no switch closes; **When** it is moved past the threshold, **Then** the switch for that direction closes.
5. **Given** a single controller and no multiplayer, **When** a program reads the right jack (AN0 high), **Then** it reads the same switches as the left jack, so a turn-based two-player game works by passing the controller. (2026-09-28: this holds for a lone Joyport player while the other jack is free, FR-008.)

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
3. **Given** two players, **When** the slots' targets (joystick 0, joystick 1, single paddles) are set to anything, **Then** Player 1 is still the left jack and Player 2 the right; the targets apply only while the Joyport is off. (Superseded 2026-09-28: each player's mode gives its jack; see scenario 7.)
4. **Given** both players on Automatic and two controllers attached at launch, neither used yet, **When** the user presses fire on one, **Then** it becomes Joyport left, drives both jacks, and the other controller drives nothing. (2026-09-28: with Player 1 on Joyport left and Player 2 on Automatic, it becomes Player 1.)
5. **Given** that state, **When** the other controller is used, **Then** it becomes Joyport right, and the first drives the left jack alone. (2026-09-28: it becomes Player 2, on Joyport right through Automatic.)
6. **Given** Player 2 set to Same as left, **When** a second controller is used, **Then** Player 1 still drives both jacks. (Superseded 2026-09-28: there is no Same as left entry; Player 2 set to Disabled leaves Player 1 driving both jacks, since a Disabled Player 2 holds no jack.)
7. **Given** Player 1 on Joyport right and Player 2 on Joyport left, both playing, **When** a program reads with AN0 low, **Then** it reads Player 2's switches; **When** it reads with AN0 high, **Then** it reads Player 1's.
8. **Given** Player 1 on Joyport left, **When** the user opens Player 2's mode list, **Then** Joyport left is shown disabled and Joyport right is enabled.
9. **Given** Player 1 on Joyport left and Player 2 on Joystick, both playing, **Then** Player 1 drives both jacks, Player 2 drives PDL0 and PDL1, and Player 2's buttons do not reach the machine.

---

### User Story 4 - Turning it on and off (Priority: P2)

**Superseded 2026-09-28**: the story and scenarios 1-5 below describe the removed Apple / Atari switch and picker row. Since then a user turns the Joyport on by choosing Joyport left (Atari) or Joyport right (Atari) in a player's mode list, in that player's submenu of the picker or on its row on the Controllers page, and off by choosing Joystick or Paddle for every player on a jack. The players' modes are global, and the //c offers no Joyport entry. Scenarios 6-10 replace 1-5.

A user turns the Joyport on with one click on the command bar's controller picker before starting a Joyport game, and off the same way afterward. The Controllers page in Settings shows the same setting as the switch on top of the unit: toward the front for Atari mode, and toward the rear for Apple mode, which is the game port with no Joyport. The setting is the same on every machine that can use it, and the picker's check mark shows it is on, so a user who switches to a game that expects an analog joystick knows why it does not respond. The //c does not offer the Joyport.

**Why this priority**: Joyport games need it on and every other game needs it off, so switching has to be quick. Still simple.

**Independent Test**: Turn the Joyport on from the picker on the //e, confirm the Controllers page shows Atari mode, relaunch, and confirm it is still on, and on the ][+ as well; switch to the //c and confirm neither place offers it; switch back to the //e and confirm it is on.

**Acceptance Scenarios**:

1. **Given** the //e, **When** the user checks the Joyport (Atari mode) row in the controller picker, **Then** the Joyport is on at once, without resetting the machine, and the row shows a check mark.
2. **Given** the Joyport turned on from the picker, **When** the user opens the Controllers page, **Then** the switch is toward the front and labeled Atari mode; **When** the user moves it to Apple mode, **Then** the Joyport is off at once and the picker's row is unchecked.
3. **Given** the Joyport on, **When** the user relaunches or switches to the ][+, **Then** it is still on.
4. **Given** the //c, **When** the user opens the controller picker or the Controllers page, **Then** neither offers the Joyport, and the game port reads as it does with no Joyport; **When** the user switches back to the //e, **Then** the Joyport is on as it was left.
5. **Given** Apple mode, and the Controller Select switch emulated (FR-016, not decided), **When** the user looks at that switch, **Then** it is disabled, since it has no effect there.
6. **Given** the //e with both players on Joystick, **When** the user chooses Joyport left (Atari) in Player 1's submenu, **Then** the Joyport is on at once, without resetting the machine, and the Controllers page shows Player 1's mode as Joyport left (Atari).
7. **Given** Player 1 on Joyport left, **When** the user chooses Joystick for Player 1 on the Controllers page, with Player 2 on Automatic, **Then** the Joyport is off at once.
8. **Given** Player 1 on Joyport left, **When** the user relaunches or switches to the ][+, **Then** Player 1 is still on Joyport left and the Joyport is on.
9. **Given** Player 1 on Joyport left, **When** the user switches to the //c, **Then** neither the picker nor the Controllers page offers a Joyport entry, Player 1 plays as Joystick and the game port reads as it does with no Joyport; **When** the user switches back to the //e, **Then** Player 1 is on Joyport left again.
10. **Given** a settings file saved with the Joyport on, **When** Casso first launches with this change, **Then** Player 1 is on Joyport left and Player 2 on Automatic, and a later launch does not adopt any machine's old value again.

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

**Status**: proposed, not decided. Nothing in this story is built or planned until FR-016 is. (Superseded 2026-09-28: closed as not needed, with FR-016. Putting a player on Joyport left or Joyport right does for the manual's test program what the switch did; the story is kept as the record.)

A user runs the test program from the Joyport manual, which asks for the switch it calls the rear switch to be at Right for one stick and at Left for the other. They set the switch in Settings next to the Joyport, and each section reads the stick on that side, as the hardware does.

**Why this priority**: The manual's own test program depends on it, and without it both one-stick sections read the left jack. Games set AN0 themselves and work at Center, so it is not needed for play.

**Independent Test**: With two controllers each holding a player slot, run the manual's test program: the Right section reads only player 2's controller and the Left section only player 1's, and the Center section reads each in turn.

**Acceptance Scenarios**:

1. **Given** the switch at Right and AN0 low, **When** player 2 holds fire, **Then** fire reads closed; **When** player 1 holds fire instead, **Then** fire reads open.
2. **Given** the switch at Left and AN0 high, **When** player 1 holds up with AN1 high, **Then** PB1 reads closed.
3. **Given** the switch at Center, **When** a program changes AN0, **Then** the jack follows AN0, as before this story.
4. **Given** the switch at Left or Right, **When** the user relaunches or switches machines, **Then** the position is kept.

---

### User Story 7 - Any controller plays a Joyport game as it is held (Priority: P1)

A user puts a player on a Joyport jack and plays with the D-pad, the left stick or the right stick, and fires with whichever face button, bumper or trigger is under their finger, without making a profile. Someone who prefers the D-pad and someone who prefers the stick both get what they expect.

**Why this priority**: The Default profile steers with the left stick and fires with A only, which surprised the first user to try the Joyport. Needing a profile before a game works defeats the one-click attach.

**Independent Test**: On an Xbox controller with nothing set up, attach the Joyport: each of the D-pad, left stick and right stick closes the direction switches, and each of A, B, X, Y, both bumpers and both triggers closes fire. Detach it: the controller plays its Default again.

**Acceptance Scenarios**:

1. **Given** a controller with no profile chosen for Joyport play, **When** the Joyport is attached, **Then** it plays the Joyport profile, shown checked in the profile list.
2. **Given** a profile created and picked while the Joyport is on, **When** the user turns the Joyport off and on again, **Then** the normal-mode choice plays while it is off, and their pick plays again once it is on.
3. **Given** the Joyport profile edited, **When** the user presses Reset profile, **Then** it returns to the Joyport mapping, not the Default's.
4. **Given** the Joyport profile selected on the Controllers page, **When** the user looks for Rename and Delete, **Then** both are unavailable, as they are for the Default.
5. **Given** the Joyport on, **When** the user opens a player's profile list, **Then** it shows the Joyport profile and the profiles created in Joyport mode, and no normal-mode profile (FR-020). (2026-09-28: this holds for a player on a Joyport jack; a player on Joystick or Paddle beside it lists its own kind.)

---

### Edge Cases

- **No controller at all**: with the Joyport attached and no controller selected, every switch reads open, which is what an Apple with a Joyport and no joysticks plugged in reads.
- **Keys as joystick**: with arrows-to-joystick on, the arrow keys drive player 1's direction switches and the fire key drives fire, through the same rules as any other source that drives PDL0, PDL1 and PB0. The fire key is X. The Alt keys, which also fire in arrows-to-joystick mode today, do not drive the Joyport, because on the //e they are Open Apple and Closed Apple (FR-010). (2026-09-28: the keys are offered to Player 1 in Joystick or a Joyport mode; on a Joyport mode they drive Player 1's jack or jacks and not the paddle inputs.)
- **Mouse as paddle**: the mouse does not drive the Joyport; an Atari joystick has no paddle for it to stand in for. The picker leaves mouse-as-paddle out while the Joyport is on (FR-009). (Superseded 2026-09-28: it is offered only to a Player 1 in Paddle mode, which can be beside a Player 2 on the Joyport; it then drives PDL0 with no buttons.)
- **The paddle inputs**: an Atari joystick has no potentiometers, so while the Joyport is attached, controllers drive the switches and not the paddle inputs. The paddle inputs read as no paddle connected. (Superseded 2026-09-28: a Joyport player drives no paddle input, but a player on Joystick or Paddle beside it drives its own; the paddle inputs read as no paddle connected only while every playing player is on the Joyport.)
- **A player left on Joystick or Paddle beside a Joyport player**: its paddles are read, since the rear sockets stay readable in Atari mode, and its buttons are not, since the Joyport owns all three button lines. The Controllers page disables its button binding rows and shows the warning notice under its row (FR-022).
- **Player 1 on a Joyport jack and Player 2 on Automatic**: Player 2 takes the other jack.
- **Player 2 Disabled beside a Joyport Player 1**: Player 2 holds no jack, and Player 1 drives both.
- **PB1 and PB2 bindings**: a profile's PB1 and PB2 bindings have no Joyport switch to drive and are ignored while the Joyport is attached. They are kept in the profile.
- **Opposite directions at once**: a D-pad or pair of bindings that reports left and right together closes neither, since a physical Atari stick cannot close both.
- **A program that never sets the annunciators**: it reads whichever jack and switch pair the annunciators were left at, as on real hardware.
- **Shift-key mod**: on the //e, PB2 carries the Joyport's right-or-down switch instead of the Shift key, so programs that detect Shift through PB2 do not see it while the Joyport is attached. This is also true of the real hardware.
- **The //c IOU**: on the //c, `$C058`-`$C05F` program the mouse and the VBL interrupt when IOU access is on. That behavior is unchanged; the //c never has a Joyport.
- **Double hi-res on the //e**: AN3 (`$C05E`/`$C05F`) keeps selecting double hi-res. The Joyport uses only AN0 and AN1.
- **Machine switch with a controller held**: switching to the //c returns the pushbuttons and paddles to the normal game-port behavior at once, with nothing stuck pressed.
- **The picker used while Settings is open**: the Controllers page shows the change at once, and pressing OK does not undo it. (2026-09-28: this now applies to a player's mode chosen from the picker.)
- **Reset while a switch is held**: during the post-reset window the Joyport releases every line, so a held fire button cannot trigger self-test or a reboot either.
- **A controller never touched**: it closes no switch, so a stick left plugged in does not keep the controller in the user's hands off either jack (spec 034 FR-042).
- **The wrong controller on the left jack**: the user picks the controller they want from Joyport left's submenu in the picker, and the other player returns to Automatic (spec 034 FR-041). (2026-09-28: from the submenu of the player on the left jack, Player 1 or Player 2.)
- **A second controller bumped while one person plays**: it becomes Joyport right and takes the right jack, and the player keeps the left. A one-player game that reads the right jack would then stop responding, which setting Joyport right to Same as left prevents. (Superseded 2026-09-28: it becomes Player 2, on the other jack when Player 2 is on Automatic; setting Player 2 to Disabled prevents it.)
- **A trigger resting on an axis**: a gamepad that reports its analog triggers as axes has them used only as fire, never as a stick, since a trigger at rest sits at one end of its travel and would hold a direction closed.
- **The Controller Select switch and a single controller** (if FR-016 is adopted): with one controller in use it drives both jacks, so it reads the same at Left, Center and Right. (Superseded 2026-09-28: FR-016 is closed.)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Joyport MUST be on, in Atari mode, whenever at least one player's mode (FR-021) resolves to a Joyport jack on a machine that can use it, and off, the game port as it is with no Joyport, otherwise. There MUST be no separate Joyport setting, no Apple / Atari switch on the Controllers page, no "Joyport (Atari mode)" row in the picker and no menu command for it. The Machine tab's device tree MUST NOT list the Joyport. On the //c no Joyport entry MUST be offered. (Superseded 2026-09-28: ~~Casso MUST have a Joyport setting with two positions, Apple mode and Atari mode, defaulting to Apple mode. Atari mode is the Sirius Joyport in use with its Atari jacks selected; Apple mode is the game port as it is with no Joyport. It MUST be offered in two places that show and change the same setting: a Joyport section on the Controllers page in Settings, and a checkable "Joyport (Atari mode)" row in the command bar's controller picker. The section MUST show it as the unit's own switch seen from above, with the unit's front toward the bottom: the knob toward the bottom is Atari mode and toward the top is Apple mode. The section's "Joyport" label MUST sit to the left of the switch, centered vertically on it. Both positions MUST be labeled, "Apple (rear)" above the switch and "Atari (front)" below it, each centered horizontally on the switch. The Machine tab's device tree MUST NOT list the Joyport. On the //c neither place MUST offer it.~~)
- **FR-002**: The players' modes MUST be saved globally with their entries (spec 034 FR-037) and restored at launch. A mode change from the picker or the Controllers page MUST take effect at once, without resetting the emulated machine and without waiting for OK. On the //c a Joyport mode MUST play as Joystick and MUST be kept, so switching to a machine that can use it restores it. The old saved Joyport setting MUST be read once to migrate: a global value of Sirius Joyport, or, where the global value was never set, the launched machine's value of Sirius Joyport, MUST become Player 1 on Joyport left and Player 2 on Automatic. The migration MUST run only while no player mode has been saved, the saved modes marking it done, so no machine's old value is adopted again. The old global key MUST then be removed from the global prefs; each machine's own value MUST be left as it is. (Superseded 2026-09-28: ~~The Joyport setting, and the Controller Select position if FR-016 is adopted, MUST be saved globally, not per machine, and restored at launch. A change from either place MUST take effect at once, without resetting the emulated machine and without waiting for OK. On the //c the Joyport MUST read as off whatever the setting is, and the setting MUST be left as it was, so switching to a machine that can use it restores it. The first launch after this change MUST adopt the setting saved for the machine being launched.~~)
- **FR-003**: The machine MUST record the state of Annunciators 0, 1 and 2 as programs set them through `$C058`-`$C05D`, on every machine that has them, with or without a Joyport. The //c's use of `$C058`-`$C05F` for its mouse and VBL interrupt, and the //e's use of AN3 for double hi-res, MUST be unchanged.
- **FR-004**: While the Joyport is attached, a read of PB0, PB1 or PB2 MUST return the switch that the current AN0 and AN1 settings select, per the table in Context, with a closed switch reading bit 7 clear and an open one bit 7 set. The value MUST reflect the annunciator settings at the moment of the read, not at the time of an earlier sample.
- **FR-005**: Each player's switches MUST come from that controller's active profile. A PDL0 binding past the switch threshold toward its low end closes left, and toward its high end closes right. PDL1 does the same for up and down. The PB0 bindings drive fire. A digital binding (a D-pad, a hat or a button pair) closes its switch whenever it is pressed. Each axis is judged on its own, so a diagonal closes one horizontal and one vertical switch.
- **FR-006**: The switch threshold MUST be a fixed fraction of an axis's travel from center, measured after the profile's deadzone and calibration, and MUST close a switch well before full deflection, as an Atari stick does.
- **FR-007**: A binding that reports both directions of one axis at once MUST close neither switch of that pair.
- **FR-008**: Each player on a Joyport mode MUST drive the jack its resolved mode gives (FR-021). A player on a Joyport jack MUST also drive the other jack while that jack is free: no player's mode is on it, or its player is not playing (spec 034 FR-042). A jack whose player is held for a controller that left MUST read every switch open, and the other player MUST NOT regain it (spec 034 FR-040). The arrow keys picked for a Player 1 on a Joyport mode count as a playing Player 1. (Superseded 2026-09-28: ~~While one controller is playing alone (spec 034 FR-042), or the arrow keys are, it MUST drive the left jack and MUST also appear on the right jack. Once two players are playing, Player 1's controller MUST drive the left jack and Player 2's the right jack, whatever the slots' targets. A jack whose controller is absent MUST read every switch open, and the player who remains MUST NOT regain it (spec 034 FR-040). With Player 2 set to Same as left, Player 1 MUST drive both jacks always.~~)
- **FR-009**: A player on a Joyport jack MUST NOT drive the paddle inputs, and the mouse MUST NOT drive the Joyport. A player on Joystick or Paddle MUST drive its paddle inputs whether or not the other player is on the Joyport, as a lone player would (spec 034 FR-039): Joystick drives PDL0 and PDL1, Paddle drives PDL0. The paddle inputs MUST read as no paddle connected only while every playing player is on a Joyport jack. A controller MUST NOT be offered to programs as an analog joystick and an Atari stick at once: one player is one or the other. (Superseded 2026-09-28: ~~While the Joyport is attached, the paddle inputs MUST read as no paddle connected, and the mouse MUST NOT drive the Joyport. While the Joyport is on, Paddle mode MUST NOT be offered to either player, and so neither is mouse-as-paddle, and a controller MUST NOT be offered to programs as an analog joystick and an Atari stick at once.~~)
- **FR-010**: On the //e, while the Joyport is attached and active, the Open Apple, Closed Apple and Shift keys MUST NOT change what PB0, PB1 and PB2 read. This follows the real machine, where the Joyport's idle lines already read as those keys held down. Keys as joystick MUST be offered to Player 1 in Joystick or a Joyport mode, and on a Joyport mode the arrow keys MUST drive Player 1's jack or jacks and not the paddle inputs (2026-09-28).
- **FR-011**: After every reset, whether from power-on, Ctrl-Reset or a machine switch, the Joyport MUST release every line for a window long enough for the machine's reset handling to read the pushbuttons, and MUST become active when that window ends. While released, the pushbuttons MUST read as they would with no Joyport attached, including the //e's Open Apple and Closed Apple keys, so holding a key through a reset still has its usual effect.
- **FR-012**: Each player's submenu in the picker, and its row on the Controllers page, MUST show that player's mode checked, whichever place set it. (Superseded 2026-09-28: ~~The controller picker's "Joyport (Atari mode)" row MUST be checked exactly while the Joyport is on, whichever place turned it on.~~)
- **FR-013**: With the Joyport not attached, every pushbutton, paddle and key behavior MUST be exactly as before this feature.
- **FR-014**: The switch logic, the annunciator selection, the reset window, and the choice of which controller drives which jack MUST be testable without a real controller or a real Joyport, as spec 034's controller logic is.
- **FR-015**: While the Joyport is attached, the Controllers page MUST show a live light for each of the five switches (up, down, left, right, fire) of the controller in Editing, lit exactly when that switch would read closed, in place of the stick position and button lights. It MUST show which jack that controller drives. With the Joyport detached the page MUST be unchanged. (2026-09-28: the lights show while the controller in Editing plays for a player on a Joyport jack; a controller whose player is on Joystick or Paddle shows the stick position and button lights.) The heading above the lights MUST read "Atari joystick", followed by ": left jack", ": right jack" or ": both jacks" for the controller in Editing.
- **FR-016**: Closed 2026-09-28 as not needed. Casso MUST behave as if the Controller Select switch is at Center, where AN0 chooses the jack. Putting a player on Joyport left or Joyport right (FR-021) covers what the switch did for the manual's test program, whose one-stick sections read one jack. The text below is kept as the record. (Superseded 2026-09-28: ~~[NEEDS CLARIFICATION: whether Casso emulates this switch is not decided; until it is, Casso behaves as if the switch is at Center and none of this applies.]~~) If adopted: Casso MUST have a Controller Select setting with Left, Center and Right, defaulting to Center, saved globally with the Joyport setting (FR-002). It MUST be shown on the Controllers page beside the Joyport switch, as a switch with its three positions laid out left to right as on the unit, and MUST be disabled in Apple mode. At Left or Right, every read MUST come from that jack whatever AN0 says; at Center, AN0 MUST choose, per the table in Context. Changing it MUST take effect on the next read, with no reset.
- **FR-017**: Every controller model MUST have a built-in Joyport profile beside its Default, which leads the list of Joyport-mode profiles (FR-020). In it, the primary stick, the D-pad and any second stick MUST steer, and the controller's fire-like buttons MUST fire:
  - On an Xbox-class controller: both sticks and the D-pad steer; A, B, X, Y, both bumpers and both triggers fire; Back, Start and the stick clicks do not.
  - On any other gamepad: the primary stick and the D-pad steer, and a second stick steers when the device has one: Z and Rz when it has both, otherwise Rx and Ry. Every button fires.
  - On a joystick or wheel: the primary stick and the D-pad steer, and every button fires. No other axis steers, since it may be a throttle, twist or pedal that rests off center.
  - An axis that is an analog trigger MUST never steer.
  
  Reset MUST restore the Joyport profile's own mapping. It MUST NOT be renamed or deleted. A user profile already called Joyport becomes the built-in one, keeping its mapping.
- **FR-018**: Each controller's chosen profile MUST be kept separately for each kind of profile (FR-020), and saved. A profile picked from the picker or the Controllers page MUST be recorded for the kind in effect for that controller's player. Changing a player's mode, including onto or off a Joyport jack, MUST switch that player's controller to its choice for the new kind, and MUST NOT change the kind of the other player's controller (2026-09-28); with none made for it, a controller MUST play that kind's built-in profile: the Default, Paddles or the Joyport profile.
- **FR-019**: The picker's rows MUST read "Player 1: <what plays>" and "Player 2: <what plays>" whatever the players' modes, ending in " (paddle)" for a controller or Automatic in Paddle mode and with no suffix for any other mode, with the mode shown checked in the player's submenu. The assignment notice (spec 034 FR-044) for a player on Joystick or Paddle MUST read "Player N: description"; for a player on a Joyport jack it MUST add the jack: "Player N (Joyport left): description", "Player N (Joyport right): description", or "Player N (Joyport left and right): description" while it drives both jacks. The Controllers page's note beside each player row MUST give what that player drives: "left jack", "right jack", "both jacks", "joystick 0", "paddle 1" and so on. (Superseded 2026-09-28: ~~While the Joyport is on, the picker and the assignment notices (spec 034 FR-008, FR-044) MUST call the players Joyport left and Joyport right in place of Player 1 and Player 2, and Player 2's Disabled entry MUST read "same as left", in lower case wherever it follows a colon. While Joyport right is on Automatic with no controller playing, its row MUST read "Joyport right: same as left". The notice for a controller playing alone MUST read "Joyport left and right: description".~~)
- **FR-020**: Every profile MUST belong to one kind, Joystick, Paddle or Joyport, fixed when it is created: a profile created for a player takes that player's kind, which is Joyport for a player on either jack (2026-09-28; superseded: ~~or Joyport while the Joyport is on~~). The picker and the Controllers page MUST list only the profiles of the kind in effect for that player, its built-in profile first: Default for Joystick, Paddles for Paddle, Joyport for Joyport. Changing a player's mode on the Controllers page MUST swap that player's list in place (2026-09-28; superseded: ~~Moving the Joyport switch on the Controllers page MUST swap the list in place.~~). Creating a profile MUST offer only starting points of its kind: the Default mapping or a copy of a Joystick profile; the Paddles mapping or a copy of a Paddle profile; the Joyport mapping or a copy of a Joyport profile. The profile to copy MUST be chosen from a list of that kind's profiles. The kind's built-in profile MUST be left out of that list while its mapping is still the built-in mapping, since copying it would duplicate the built-in mapping entry. Resetting a profile MUST restore the built-in mapping of its kind. Profiles saved before this change are classified by spec 034 FR-043's rule. Names stay unique per model across all kinds (spec 034 FR-027).
- **FR-021** (2026-09-28): Each player MUST have a mode chosen from, in this order, Joystick, Joyport left (Atari), Joyport right (Atari) and Paddle; Player 2's list MUST start with Automatic, which is its default, shown with the mode it resolves to, as in "Automatic (Joyport right)" (2026-09-28; it was labeled "Same as Player 1", and the saved token is unchanged). Automatic MUST resolve to Player 1's mode, except that with Player 1 on a Joyport jack it MUST resolve to the other jack. The list MUST be offered in each player's submenu of the picker and on each player's row on the Controllers page. A jack that the other player's resolved mode holds MUST be shown disabled and MUST NOT be choosable, in both places; a Disabled Player 2 holds no jack. The //c MUST NOT offer the two Joyport entries.
- **FR-022** (2026-09-28): While either player's resolved mode is a Joyport jack, the Joyport MUST own PB0, PB1 and PB2: a player on Joystick or Paddle MUST NOT drive any button line, as on the hardware, while still driving its paddle inputs (FR-009). On the Controllers page that player's button binding rows MUST be disabled, and a notice with Dxui's warning badge MUST show under that player's row: "This controller's buttons are disabled because Player N is using the Joyport.", where N is the other player.

### Key Entities

- **Joyport setting**: global, Apple mode or Atari mode. Apple mode is the game port as it has always been emulated; Atari mode is the Sirius Joyport in use. (Superseded 2026-09-28: removed; the old saved value is read once to migrate, then the global key is removed.)
- **Player mode** (2026-09-28): global, per player, one of Joystick, Joyport left, Joyport right, Paddle, and for Player 2 Automatic. The Joyport is on while either player's resolved mode is a jack.
- **Annunciator state**: per machine, the on or off state of AN0, AN1 and AN2 as last set by a program.
- **Atari joystick state**: per jack, whether each of fire, up, down, left and right is closed, derived from the controller that drives the jack.
- **Reset window**: the interval after a reset during which the Joyport releases every line.
- **Controller Select position** (if FR-016 is adopted): global, Left, Center or Right. (Superseded 2026-09-28: FR-016 is closed; not modeled.)
- **Joyport profile**: per controller model, the built-in profile played with the Joyport attached when no other is chosen.
- **Chosen profile per mode**: per controller, the profile chosen for play without the Joyport and the one chosen for play with it; either may be unset. (Since 2026-09-27, one per kind: Joystick, Paddle and Joyport.)
- **Profile mode**: per profile, normal or Joyport, fixed when the profile is created (FR-020). (Since 2026-09-27, the kind: Joystick, Paddle or Joyport.)

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a readout disk, all ten switches (five per jack) read correctly in both AN1 states, for both jacks: 20 of 20 combinations.
- **SC-002**: A switch change reaches the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled, the same as controller input in spec 034.
- **SC-003**: With the Joyport attached on a //e, 20 of 20 Ctrl-Resets and 20 of 20 power-ons end in a normal reset, with no self-test and no unintended reboot.
- **SC-004**: In a Joyport-aware game, a user can move in all eight directions and fire, including fire held with a direction, within one minute of attaching the Joyport, with no mapping changes to a controller whose profile already works in joystick games.
- **SC-005**: Two players on two controllers each drive only their own jack, with neither one's input changing the other's switches, over five minutes of simultaneous play.
- **SC-006**: With the Joyport not attached, every existing game-port and controller test passes unchanged.
- **SC-007**: The Joyport setting, and the Controller Select position if FR-016 is adopted, are restored on 100% of relaunches, and are the same on every machine that can use them. (Superseded 2026-09-28: the players' modes are restored on 100% of relaunches and are the same on every machine that can use them; on the //c a Joyport mode plays as Joystick and is kept.)
- **SC-008**: (If FR-016 is adopted.) With two controllers each holding a slot, every step of the manual's test program passes in its one-stick and two-stick sections, with each step answered only on the controller for the jack it asks about, at the Controller Select position the program asks for. (Superseded 2026-09-28: FR-016 is closed. With two controllers on Joyport left and Joyport right, every step of the two-stick section passes; each one-stick section passes with a player put on the jack that section reads.)
- **SC-011** (2026-09-28): With Player 1 on a Joyport jack and Player 2 on Joystick, a program reads Player 2's stick at PDL0 and PDL1 and Player 1's switches at the jacks, and no press of Player 2's buttons changes PB0-PB2.
- **SC-009**: With a stick and a gamepad both attached and only the gamepad used, a one-player Joyport game responds to the gamepad on the first try once the game's own Joyport option is selected, with no setup in Casso.
- **SC-010**: Bandits, which takes Ctrl-@ to select the Joyport (GH #155), plays with a controller on the Joyport profile with no mapping changes.

## Assumptions

- **Source of truth**: the [Sirius Joyport owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf) and public write-ups (Wikipedia, Nerdly Pleasures, Lukazi's Apple II Projects) define the behavior. The implementation is clean-room: no GPL emulator source is read or copied.
- **AN0's sense**: low (`$C058`) selects the left jack and player 1, as the owner's manual gives it. At least one emulator documents the opposite sense for AN0 and a reversed AN1, and reports Wavy Navy steering with up and down on a ][+. The manual takes precedence, and Wavy Navy on the ][+ is a validation case for it.
- **Controller Select switch**: fixed at Center, where AN0 selects the jack, which is what two-player games need and what every game works with. Emulating Left and Right is proposed and not decided (FR-016). (Superseded 2026-09-28: closed as not needed; the switch stays at Center.)
- **Reset window length**: a few hundred milliseconds of emulated time covers the //e's reset handling with room to spare. The exact length is a planning decision, verified by SC-003.
- **Switch threshold**: about half of an axis's travel from center. The exact fraction is a planning decision, and is not user-adjustable in this spec.
- **Power-on annunciator state**: all annunciators start off (low) at power-on. A Ctrl-Reset changes them only if the machine's reset code writes them.
- **No new settings page**: the switches come from each controller's profile. The built-in Joyport profile (FR-017) is an ordinary profile a user can edit or copy, so a user who wants a D-pad-only feel edits it or picks another profile for Joyport play.
- **The //c**: a Joyport cannot be connected to it. It has no 16-pin game I/O socket, and its 9-pin joystick connector has no annunciator lines; its annunciator addresses program the mouse and the VBL interrupt while IOU access is on; and it has no PB2 for the right and down switches. Emulating one anyway would describe a machine that never existed and could disturb the mouse in //c software. The IIgs is not emulated.
- **Apple mode**: the Joyport's Apple paddle passthrough is not emulated as such, since Casso's four paddle inputs already provide what it did. The Apple / Atari switch is shown, with Apple mode meaning the Joyport is off (FR-001). (Superseded 2026-09-28: the switch is not shown; Apple mode is every player off the Joyport jacks.)
- **Paddle inputs in Atari mode**: the hardware reads the rear sockets' paddles in Atari mode and cuts off only their buttons (confirmed by the reporter on GH #156). Casso leaves the rear sockets empty in Atari mode because no known software reads both kinds together; FR-009 keeps the paddle inputs reading as no paddle connected until one does. (Superseded 2026-09-28: a player on Joystick or Paddle beside a Joyport player stands in for a rear-socket joystick or paddle, with its buttons cut, FR-009 and FR-022.)
- **Earlier validation**: the first pass through the manual's test program closed the switches on both jacks for its one-stick sections, because only Center was emulated. That hid the fact that the one-stick sections depend on the Controller Select switch.
- **Questions for a hardware owner** (GH #156): whether any software depends on the Controller Select switch being at Left or Right; whether the Atari jacks' switches reach the pushbutton inputs in Apple mode; which jack one-player Joyport games read; and which button line a second Apple joystick uses (spec 034 Assumptions).
- **Disk associations**: not built; GH #78 owns them. An association should hold one profile preference per mode for each controller model, and the one for the mode in effect applies; with none for that mode, the controller's own choice plays.
- **Decided 2026-09-27 during planning**: mouse as paddle picked for Player 1 and then the Joyport turned on leaves nothing driving the Joyport, and the pick stays saved for when it is turned off. (Superseded 2026-09-28: the mouse is offered only to a Player 1 in Paddle mode; beside a Player 2 on the Joyport it drives PDL0 with no buttons, and while Player 2 plays it drives PDL0 only.) A built-in Joyport profile saved before the DirectInput gamepad rule keeps its saved mapping; Reset brings in the rule. DirectInput has no trigger type, so "a trigger never steers" rests on trying Z and Rz before Rx and Ry. An older build shows a Joyport-mode profile as an ordinary one.
- **Planning notes (2026-09-27)**: the vertical switch is an orientation option on Dxui's toggle, and the three-position switch a new Dxui control in the same style. The setting keeps the saved values the game-port adapter used, None and Sirius Joyport, moved from each machine's preferences to the global ones. (Superseded 2026-09-28: the switch is removed from the page, and Dxui's toggle keeps its vertical orientation option and label-visibility API as library code; the three-position control is not built. The old saved value is read only by the one-time migration, FR-002.)
- **Dependency on spec 034**: controller profiles, the player slots, Automatic, the notices and the controller picker come from spec 034.
