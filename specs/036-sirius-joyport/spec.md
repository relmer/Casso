# Feature Specification: Sirius Joyport Emulation

**Feature Branch**: `036-sirius-joyport`

**Created**: 2026-09-24

**Status**: Draft

**Input**: Emulate the Sirius Software Joyport (1981), the game-port adapter that let an Apple ][, ][+ or //e read two Atari CX40-style digital joysticks, so the thirty-odd games written for it can be played with the controllers Casso already supports (spec 034). The Joyport multiplexes ten switches onto the game port's three pushbutton inputs, using two annunciator outputs as select lines.

## Context

An Atari joystick has no analog axes: it is four direction switches and a fire button. The Apple game port has three pushbutton inputs, so the Joyport multiplexes. A program sets Annunciator 0 to select a jack and Annunciator 1 to select a pair of directions, then reads the three buttons. From the Sirius Joyport owner's manual:

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

The Joyport also has an Apple mode that passes two sets of Apple paddles through to the four paddle inputs. Casso's four paddle inputs already provide that, so this spec covers Atari mode only.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Play a Joyport game with one controller (Priority: P1)

A user attaches the Joyport to an Apple ][+ or //e in Settings, boots a Joyport-aware game, selects the game's Joyport option, and plays it with whatever controller they already use: an Xbox controller's D-pad or stick, a flight stick, or a gamepad. Directions and fire respond the way an Atari joystick would, including a diagonal and fire held with a direction.

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

Two people, each holding a controller assigned to a player slot in multiplayer mode (spec 034), play a Joyport game that reads both joysticks. Player 1's controller is the left jack and player 2's is the right jack, and neither one's input reaches the other's switches.

**Why this priority**: The Joyport's two jacks are what made it popular for two-player games, but single-player play is useful without them.

**Independent Test**: With the readout disk and two controllers in multiplayer mode, each controller closes only its own jack's switches, including when both are moved at once.

**Acceptance Scenarios**:

1. **Given** multiplayer mode with a controller in each slot, **When** player 2 holds fire and a program reads with AN0 low, **Then** fire reads open; **When** the program reads with AN0 high, **Then** fire reads closed.
2. **Given** the same setup, **When** player 2's controller is disconnected, **Then** the right jack reads every switch open, and player 1's jack is unaffected.
3. **Given** multiplayer mode, **When** the slots' paddle assignments (joystick 0, joystick 1, single paddles) are set to anything, **Then** slot 1 is still the left jack and slot 2 the right; the paddle assignments apply only while the Joyport is not attached.

---

### User Story 4 - Turning it on and off (Priority: P2)

A user attaches the Joyport to a machine in Settings, and the choice is saved with that machine. The command bar shows that the Joyport is attached, so a user who switches to a game that expects an analog joystick knows why it does not respond. The //c does not offer the Joyport.

**Why this priority**: Needed for the feature to be usable day to day, but simple.

**Independent Test**: Attach the Joyport on the //e, relaunch, and confirm it is still attached on the //e and not on the ][+; switch to the //c and confirm the option is absent.

**Acceptance Scenarios**:

1. **Given** the //e, **When** the user attaches the Joyport in Settings and presses OK, **Then** it takes effect without resetting the machine and is still attached after a relaunch.
2. **Given** the Joyport attached on the //e, **When** the user switches to the ][+, **Then** the ][+ has the Joyport only if it was attached there.
3. **Given** the //c, **When** the user opens Settings, **Then** the Joyport option is not offered.
4. **Given** the Joyport attached, **When** the user looks at the command bar's controller picker, **Then** it shows that the Joyport is attached.

---

### Edge Cases

- **No controller at all**: with the Joyport attached and no controller selected, every switch reads open, which is what an Apple with a Joyport and no joysticks plugged in reads.
- **Keys as joystick**: with arrows-to-joystick on, the arrow keys drive player 1's direction switches and the fire key drives fire, through the same rules as any other source that drives PDL0, PDL1 and PB0.
- **Mouse as paddle**: the mouse does not drive the Joyport; an Atari joystick has no paddle for it to stand in for.
- **The paddle inputs**: an Atari joystick has no potentiometers, so while the Joyport is attached, controllers drive the switches and not the paddle inputs. The paddle inputs read as no paddle connected.
- **PB1 and PB2 bindings**: a profile's PB1 and PB2 bindings have no Joyport switch to drive and are ignored while the Joyport is attached. They are kept in the profile.
- **Opposite directions at once**: a D-pad or pair of bindings that reports left and right together closes neither, since a physical Atari stick cannot close both.
- **A program that never sets the annunciators**: it reads whichever jack and switch pair the annunciators were left at, as on real hardware.
- **Shift-key mod**: on the //e, PB2 carries the Joyport's right-or-down switch instead of the Shift key, so programs that detect Shift through PB2 do not see it while the Joyport is attached. This is also true of the real hardware.
- **The //c IOU**: on the //c, `$C058`-`$C05F` program the mouse and the VBL interrupt when IOU access is on. That behavior is unchanged; the //c never has a Joyport.
- **Double hi-res on the //e**: AN3 (`$C05E`/`$C05F`) keeps selecting double hi-res. The Joyport uses only AN0 and AN1.
- **Machine switch with a controller held**: switching to a machine without the Joyport returns the pushbuttons and paddles to the normal game-port behavior at once, with nothing stuck pressed.
- **Reset while a switch is held**: during the post-reset window the Joyport releases every line, so a held fire button cannot trigger self-test or a reboot either.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Each machine with a 16-pin game I/O socket (the ][, ][+, //e and enhanced //e) MUST offer a game-port adapter setting with the choices None and Sirius Joyport. The default MUST be None. The //c MUST NOT offer it.
- **FR-002**: The adapter setting MUST be saved per machine and restored when that machine is next used. Changing it MUST take effect without resetting the emulated machine.
- **FR-003**: The machine MUST record the state of Annunciators 0, 1 and 2 as programs set them through `$C058`-`$C05D`, on every machine that has them, with or without a Joyport. The //c's use of `$C058`-`$C05F` for its mouse and VBL interrupt, and the //e's use of AN3 for double hi-res, MUST be unchanged.
- **FR-004**: While the Joyport is attached, a read of PB0, PB1 or PB2 MUST return the switch that the current AN0 and AN1 settings select, per the table in Context, with a closed switch reading bit 7 clear and an open one bit 7 set. The value MUST reflect the annunciator settings at the moment of the read, not at the time of an earlier sample.
- **FR-005**: Each player's switches MUST come from that controller's active profile. A PDL0 binding past the switch threshold toward its low end closes left, and toward its high end closes right. PDL1 does the same for up and down. The PB0 bindings drive fire. A digital binding (a D-pad, a hat or a button pair) closes its switch whenever it is pressed. Each axis is judged on its own, so a diagonal closes one horizontal and one vertical switch.
- **FR-006**: The switch threshold MUST be a fixed fraction of an axis's travel from center, measured after the profile's deadzone and calibration, and MUST close a switch well before full deflection, as an Atari stick does.
- **FR-007**: A binding that reports both directions of one axis at once MUST close neither switch of that pair.
- **FR-008**: In single-source mode, the selected controller (or the arrow keys, when arrows-to-joystick is on) MUST drive the left jack and MUST also appear on the right jack. In multiplayer mode, slot 1's controller MUST drive the left jack and slot 2's the right jack, whatever the slots' paddle assignments; a jack whose controller is absent MUST read every switch open.
- **FR-009**: While the Joyport is attached, the paddle inputs MUST read as no paddle connected, and the mouse MUST NOT drive the Joyport.
- **FR-010**: On the //e, while the Joyport is attached and active, the Open Apple, Closed Apple and Shift keys MUST NOT change what PB0, PB1 and PB2 read. This follows the real machine, where the Joyport's idle lines already read as those keys held down.
- **FR-011**: After every reset, whether from power-on, Ctrl-Reset or a machine switch, the Joyport MUST release every line for a window long enough for the machine's reset handling to read the pushbuttons, and MUST become active when that window ends. While released, the pushbuttons MUST read as they would with no Joyport attached, including the //e's Open Apple and Closed Apple keys, so holding a key through a reset still has its usual effect.
- **FR-012**: The command bar's controller picker MUST show when the Joyport is attached.
- **FR-013**: With the Joyport not attached, every pushbutton, paddle and key behavior MUST be exactly as before this feature.
- **FR-014**: The switch logic, the annunciator selection, the reset window, and the choice of which controller drives which jack MUST be testable without a real controller or a real Joyport, as spec 034's controller logic is.

### Key Entities

- **Game-port adapter**: per machine, the device plugged into the 16-pin game I/O socket: None, which is the game port as it has always been emulated, or the Sirius Joyport.
- **Annunciator state**: per machine, the on or off state of AN0, AN1 and AN2 as last set by a program.
- **Atari joystick state**: per jack, whether each of fire, up, down, left and right is closed, derived from the controller that drives the jack.
- **Reset window**: the interval after a reset during which the Joyport releases every line.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a readout disk, all ten switches (five per jack) read correctly in both AN1 states, for both jacks: 20 of 20 combinations.
- **SC-002**: A switch change reaches the emulated machine within one displayed frame (about 17 ms at 60 Hz) of being sampled, the same as controller input in spec 034.
- **SC-003**: With the Joyport attached on a //e, 20 of 20 Ctrl-Resets and 20 of 20 power-ons end in a normal reset, with no self-test and no unintended reboot.
- **SC-004**: In a Joyport-aware game, a user can move in all eight directions and fire, including fire held with a direction, within one minute of attaching the Joyport, with no mapping changes to a controller whose profile already works in joystick games.
- **SC-005**: Two players on two controllers each drive only their own jack, with neither one's input changing the other's switches, over five minutes of simultaneous play.
- **SC-006**: With the Joyport not attached, every existing game-port and controller test passes unchanged.
- **SC-007**: The adapter setting is restored on 100% of relaunches and machine switches.

## Assumptions

- **Source of truth**: the Sirius Joyport owner's manual and public write-ups (Wikipedia, Nerdly Pleasures, Lukazi's Apple II Projects) define the behavior. The implementation is clean-room: no GPL emulator source is read or copied.
- **AN0's sense**: low (`$C058`) selects the left jack and player 1, as the owner's manual gives it. At least one emulator documents the opposite sense for AN0 and a reversed AN1, and reports Wavy Navy steering with up and down on a ][+. The manual takes precedence, and Wavy Navy on the ][+ is a validation case for it.
- **Controller Select switch**: the real Joyport has a Left / Center / Right switch. Center, where AN0 selects the jack, is what two-player games need and what every game works with, so it is the only behavior in scope. Left and Right can be added later without changing anything here.
- **Reset window length**: a few hundred milliseconds of emulated time covers the //e's reset handling with room to spare. The exact length is a planning decision, verified by SC-003.
- **Switch threshold**: about half of an axis's travel from center. The exact fraction is a planning decision, and is not user-adjustable in this spec.
- **Power-on annunciator state**: all annunciators start off (low) at power-on. A Ctrl-Reset changes them only if the machine's reset code writes them.
- **No new settings page**: the switches come from each controller's existing profile, so a user who wants a D-pad-only feel for Joyport games uses a profile that maps only the D-pad. Joyport-specific bindings are out of scope.
- **The //c**: its joystick connector has no annunciator lines, so a Joyport cannot be connected to it. The IIgs is not emulated.
- **Apple mode**: the Joyport's Apple paddle passthrough mode is not emulated, since Casso's four paddle inputs already provide what it did.
- **Dependency on spec 034**: controller selection, profiles, multiplayer slots and the controller picker come from spec 034, which is on master.
