# Feature Specification: Super Serial Card

**Feature Branch**: `042-super-serial-card`

**Created**: 2026-10-14

**Status**: Draft

**Input**: Emulate Apple's Super Serial Card (SSC) in a slot of an Apple ][+ or //e: its 6551 ACIA, its DIP switches as card settings, Apple's firmware ROM at $Cn00 and in the $C800-$CFFF expansion space, and its interrupt. The user supplies the firmware, like Casso's other ROMs; without it, programs that drive the 6551 directly work, and Casso reports the firmware missing. The serial chip paces its traffic at the baud rate the program sets, so spec 033's Apple II link can attach host connections to the card without data arriving faster than on real hardware. Clean room: datasheets and Apple's published SSC manuals only.

## Context

Spec 033 (Casso Explorer) adds an Apple II link that moves files and disks between an Apple II and the PC, compatible with ADTPro. On a //c the link runs over the built-in serial ports; on a ][+ or //e it runs over a Super Serial Card. The owner has no SSC, so Casso has to emulate one. This feature lands on master by itself and is merged into 033 when the link's SSC work starts. Spec 033's FR-095 connects an emulated serial port, the //c's or an SSC's, to Casso Explorer in-process, to a TCP port or to a PC COM port; those connections are 033's work, and they attach to the card this feature adds.

Casso already emulates the 6551 ACIA for the //c's two built-in ports. It has a single receive data register with no buffering behind it, and it transmits each byte the moment the program writes it, whatever baud rate is set.

### The card

From Apple's *Super Serial Card Reference Card* and *Super Serial Card User's Manual* (1985), and its *Installation and Operating Manual* (1981):

| Location | Function |
|---|---|
| `$C0n1`, `$C0n2` (n = slot + 8) | the two DIP switch banks, SW1 and SW2, read by the firmware |
| `$C0n8`-`$C0nB` | the 6551: data, status, command, control |
| `$Cn00`-`$CnFF` | the top page of the 2 KB firmware ROM; any access selects the card's expansion ROM |
| `$C800`-`$CEFF` | the rest of the firmware ROM while the card is selected |
| `$CF00`-`$CFFF` | unmapped; any access deselects every card |

The firmware is Apple part 341-0065-A, 2 KB.

| SW1-5, SW1-6 | Mode |
|---|---|
| ON, ON | Communications (modem) |
| OFF, ON | Printer |
| ON, OFF | SIC P8 emulation |
| OFF, OFF | SIC P8A emulation |

SW1-1 to SW1-4 select the baud rate as a 4-bit value, OFF reading as 1 and SW1-1 the high bit: all OFF is 19,200 baud, OFF OFF OFF ON is 9,600, ON OFF OFF ON is 300. The value matches the 6551's own baud-rate code, which is also what the firmware's `<n>B` command takes; all ON is undefined in Apple's tables, and Casso offers it as 115,200 baud, the speed baud code 0 runs at on the card (FR-013). SW1-7 and SW2-7 choose which RS-232 pin drives the 6551's DCD input (the reference card lists them as normal or secondary clear to send).

In communications mode SW2-1 and SW2-2 set the data format (8 or 7 data bits, 1 or 2 stop bits), SW2-3 and SW2-4 the parity, SW2-5 a line feed after each carriage return sent, and SW2-6 interrupts on or off. In printer mode SW2-1 sets the stop bits, SW2-2 a delay after carriage return, SW2-3 and SW2-4 the line width and whether the Apple's screen echoes the output, and SW2-5 and SW2-6 are as in communications mode.

The 6551's baud code 0 selects 16 times the external clock. The SSC clocks its 6551 from a 1.8432 MHz crystal, so code 0 runs at 115,200 baud, which is the rate ADTPro uses on the card.

## Clarifications

### Session 2026-10-14

- Q: How does a user put the Super Serial Card in a slot? → A: In slot 2 only. The shipped ][, ][+ and //e configs gain a Super Serial Card in slot 2, off by default, turned on and off by its checkbox on the Hardware tab like the parallel printer and the Mockingboard, with a migration that adds it to existing configs. Another slot means a custom machine config. A Hardware tab picker for any card in any slot is a separate spec (FR-001, FR-019).
- Q: How do the card's DIP switches look in Settings? → A: As settings for what the switches select, not drawn switches: drop-downs for mode, baud rate, data format, parity, line feed after carriage return, interrupts and the printer-mode options, each offered only in the modes where its switches have that meaning. Below them a readout lists the resulting positions of SW1 and SW2, so a user can match them against Apple's manual (FR-009).
- Q: Which attachments does the serial connector offer before spec 033? → A: Nothing, a loopback plug, a file that receives what the card sends, and a file sent into the card, whose bytes arrive one character time apart at the card's baud rate and format (FR-015).
- Q: How does Casso report missing firmware? → A: Turning the card on in Settings offers to download the firmware at once. At each start with the card on and no valid firmware, an infobar reports the missing file with a Download button. The startup download dialog never stops a launch for it (FR-006, FR-007).

### Session 2026-10-17

- Q: Can the card be turned on in Settings without its firmware? → A: No. Turning it on with the firmware missing turns it off again and offers to download Apple's ROM; on success the card is turned on and its Serial card tab opens, and after a declined or failed download the card stays off. The infobar remains for a config edited outside Settings or a firmware file removed after the card was turned on (FR-006, FR-007).

### Session 2026-10-19

- Q: How do the card's DIP switches look in Settings? → A: The drop-downs stay, and the text readout below them is replaced by a drawing of the card's two DIP switch banks, SW1 and SW2. Clicking a switch flips it, and the drop-downs follow. This supersedes the 2026-10-14 answer (FR-009).
- Q: What does the Mode list offer, and where is each mode covered? → A: "Communications (bidirectional)", "Printer (output only)", "Emulate Apple II Serial Interface Card P8 firmware" and "Emulate Apple II Serial Interface Card P8A firmware". A note under Mode covers what the selected mode is for (FR-009).
- Q: Is the all-ON baud setting offered? → A: Yes, as 115,200 baud (FR-013).
- Q: Where are line feed after carriage return and interrupts covered? → A: In info tips after their labels. Line feed applies in every mode; interrupts are for programs written to use them, such as Kermit (FR-009).
- Q: What does the connector drop-down call its attachments? → A: "None", "Loopback (TX to RX)", "Write output to file" and "Read input from file", with a note under the drop-down for each; the loopback note lists its wiring (FR-015).
- Q: How does the user notice the Serial card tab when turning the card on adds it? → A: Its label shimmers once, as the update indicator's text does, unless animations are turned off (FR-020).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Put a Super Serial Card in a slot (Priority: P1)

A user opens Settings on an Apple ][, ][+ or //e, turns on the Super Serial Card in slot 2, and resets the machine. The card is now in that slot: software that scans the slots for Apple's firmware identification bytes finds a serial card there, and `PR#n` and `IN#n` jump into Apple's firmware.

**Why this priority**: Every other story assumes the card is in the machine.

**Independent Test**: Turn on the card in slot 2 of a //e with the firmware installed, and check that the Pascal 1.1 identification bytes in $C200-$C2FF identify a serial card; turn it off, reset, and check that the slot is empty again.

**Acceptance Scenarios**:

1. **Given** a //e with no card in slot 2, **When** the user turns on the Super Serial Card in slot 2 and confirms the reset, **Then** the machine restarts with the card in slot 2.
2. **Given** the card is in slot 2 and its firmware is installed, **When** a program reads `$C200`-`$C2FF`, **Then** it reads the top page of Apple's firmware.
3. **Given** the card is in slot 2, **When** a program accesses `$C200` and then reads `$C800`-`$CEFF`, **Then** it reads the first 1,792 bytes of the firmware ROM; **When** it then accesses any address in `$CF00`-`$CFFF`, **Then** `$C800`-`$CEFF` no longer reads the card's ROM.
4. **Given** a //c, **When** the user opens Settings, **Then** no Super Serial Card is offered, since the //c has no slots and its serial ports are built in.
5. **Given** the card is in a slot, **When** Casso is closed and started again, **Then** the card is in that slot with the switch settings the user set.

---

### User Story 2 - Print and type through Apple's firmware (Priority: P1)

A user with the card in slot 2 types `PR#2` and `LIST` in Applesoft, and the listing goes out the card's serial connector. With the connector looped back, `IN#2` takes input from the same connector, and the firmware's commands (Ctrl-A for communications mode, Ctrl-I for printer mode) change the baud rate, data format and the other settings for the session, as Apple's manual documents.

**Why this priority**: Apple's firmware is how BASIC, DOS and most software use the card. A card that only works for programs that program the 6551 themselves is not an SSC.

**Independent Test**: With the firmware installed and the connector sending to a file, run `PR#2`, `LIST` on a short program, `PR#0`, and compare the file with the listing.

**Acceptance Scenarios**:

1. **Given** the card in slot 2 in printer mode with the connector sending to a file, **When** the user runs `PR#2` and `LIST`, **Then** the file contains the listing, with line endings as the switches and Apple's firmware set them.
2. **Given** the card in communications mode with the connector looped back, **When** a program sends a line through `PR#2` and reads it back through `IN#2`, **Then** it reads the line it sent.
3. **Given** the card in communications mode at 300 baud, **When** a program prints 30 characters through the firmware, **Then** they take at least one second of emulated time to go out, as at 300 baud on the hardware.
4. **Given** the firmware is running, **When** the user enters the command for 1200 baud, **Then** the card transmits at 1200 baud until reset, whatever the switches select.

---

### User Story 3 - Drive the 6551 directly, with or without the firmware (Priority: P1)

A program that programs the 6551 itself, as ADTPro's client and most terminal programs do, sends and receives through the card's connector. This works even when the user has not installed Apple's firmware, and in that case Casso reports the firmware missing rather than leaving the slot to fail silently.

**Why this priority**: Spec 033's link drives the 6551 directly, and the firmware is the one part of the card Casso cannot ship.

**Independent Test**: With the firmware absent and the connector looped back, run a short machine-language program that sets the 6551 to 19,200 baud, 8 data bits, no parity, sends bytes and reads them back.

**Acceptance Scenarios**:

1. **Given** the card in slot 2 with no firmware installed, **When** the machine starts, **Then** an infobar reports that the Super Serial Card firmware is missing, with the file Casso looked for and a Download button, and the machine starts.
2. **Given** no firmware is installed, **When** a program writes the 6551's control and command registers and transmits a byte, **Then** the byte goes out the connector at the baud rate set in the control register.
3. **Given** no firmware is installed, **When** a program reads `$C200`-`$C2FF` or the expansion space after selecting the card, **Then** it reads what an empty ROM socket reads on the hardware.
4. **Given** a program sets baud code 0, **When** it transmits, **Then** the card transmits at 115,200 baud, as on the hardware.

---

### User Story 4 - Set the card's switches (Priority: P2)

A user opens the card's settings and sets its switches: mode, baud rate, data format, parity, line feed after carriage return, interrupts, and the printer-mode options. The switches read the way the user set them, so the firmware starts in that configuration after `PR#n`, `IN#n` or a reset, as on the hardware.

**Why this priority**: The defaults suit ADTPro and most modem software, so many users never change them; a printer, a terminal or an old program fixed at one baud rate depends on them.

**Independent Test**: Set the card to printer mode at 1200 baud, run `PR#2`, print a line, and check from the transmit timing and from the 6551's control register that the firmware set 1200 baud.

**Acceptance Scenarios**:

1. **Given** the card's settings, **When** the user opens them, **Then** the page lists the switch settings for the selected mode with their current values, and draws the SW1 and SW2 positions they produce.
2. **Given** the user changes the baud rate switches, **When** a program next reads `$C0n1`, **Then** it reads the new positions, without a reset.
3. **Given** the user changes the mode, **When** the user views the settings, **Then** the page offers the settings that apply in the new mode, and the drawing has the SW2 positions they produce.
4. **Given** an Applesoft program running on the card, **When** the user changes a switch, **Then** the change takes effect where the hardware would apply it: the firmware reads the switches when it starts, so a running session continues with the settings it started with until the next `PR#n`, `IN#n` or reset.

---

### User Story 5 - Receive at real-hardware speed (Priority: P2)

A host connection (in this spec the loopback and the file attachments; in spec 033 Casso Explorer, a TCP port or a COM port) delivers bytes to the card. Bytes arrive at the 6551 one character time apart at the baud rate and format set in its registers, never faster, so a program written for the real card receives them without overruns. When the program is too slow, the 6551 reports an overrun as the hardware does.

**Why this priority**: This is the requirement spec 033's link depends on (033 FR-095), and the reason the chip changes in this spec rather than in 033.

**Independent Test**: Queue 4 KB on a looped-back connector at 19,200 baud and measure the emulated time between successive receive-register-full flags.

**Acceptance Scenarios**:

1. **Given** a host connection with 4 KB ready to send at 19,200 baud, 8N1, **When** the program reads each byte as it arrives, **Then** successive bytes arrive one character time apart (10 bit times) and no overrun is reported.
2. **Given** bytes are arriving, **When** the program does not read the data register before the next byte completes, **Then** the status register reports an overrun and the new byte replaces the unread one, as on the hardware.
3. **Given** the receiver interrupt is enabled and interrupts are switched on, **When** a byte arrives, **Then** the card raises an IRQ, and reading the status register clears it.
4. **Given** the program sets the 6551's DTR off, **When** the host connection has bytes ready, **Then** the receiver gets no byte until DTR is on again.
5. **Given** Casso runs at maximum speed, **When** bytes arrive, **Then** they arrive one emulated character time apart, so the program receives them as it would at normal speed.

---

### User Story 6 - Choose what the connector is attached to (Priority: P3)

In the card's settings the user chooses what is plugged into its serial connector: nothing, a loopback plug, a file that receives what the card sends, or a file to send into the card. Spec 033 adds Casso Explorer, a TCP port and a COM port to the same choice.

**Why this priority**: Until spec 033 arrives, the loopback and file attachments are the only attachments a user can test the card with, and both serve as test fixtures for the stories above.

**Independent Test**: Attach a file, print through the card, detach it, and check that the file contains what was printed and is closed.

**Acceptance Scenarios**:

1. **Given** the connector has nothing attached, **When** a program transmits, **Then** the bytes go out at the baud rate and are discarded, and the receiver gets nothing.
2. **Given** the user attaches a file, **When** the card transmits, **Then** the bytes are appended to the file.
3. **Given** a loopback plug, **When** the card transmits a byte, **Then** the same byte arrives at the receiver one character time later.
4. **Given** the user attaches a file to send in, with the card at 2400 baud, **When** a program reads through `IN#2`, **Then** it receives the file's bytes one character time apart, in order, and the receiver gets nothing more after the last byte.

---

### Edge Cases

- The firmware file is the wrong size or not the SSC firmware: Casso reports it as invalid, with the correct size, and runs the card without firmware.
- Two cards both have their expansion ROM selected, which a program can cause by accessing two `$Cn00` pages without an access to `$CF00`-`$CFFF` between them: a read of `$C800`-`$CEFF` returns the AND of the selected cards' bytes, approximating two drivers fighting over the bus.
- On the //e, `INTCXROM` or `INTC8ROM` set: the card's `$Cn00` page and expansion ROM are hidden behind the internal ROM, exactly as for any other slot card, and an access while hidden does not select the card.
- The card is in slot 3 of a //e: the //e's 80-column firmware owns `$C300` unless `SLOTC3ROM` is set, as for any card in slot 3.
- A program sets parity or a word length shorter than 8 bits: the character time changes accordingly, and a received byte has its unused high bits clear.
- A program writes the data register while the transmitter is sending: the 6551's behavior for that case, as its datasheet specifies, applies.
- A program writes the status register, which the datasheet calls a program reset: the 6551 resets as its datasheet specifies.
- A Ctrl-Reset: the 6551 resets as the hardware's reset line resets it, the expansion ROM is deselected, and bytes queued by a host connection remain queued for delivery.
- The host connection is detached while bytes are queued: the queued bytes are discarded.
- A machine with two SSCs, in different slots: each card works independently.
- The debugger pauses the machine: no bytes arrive while the machine is paused, and delivery resumes on its pacing when it runs again.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST offer a Super Serial Card in slot 2 of the Apple ][, ][+, //e and enhanced //e, and MUST NOT offer it on the //c. A custom machine config MAY put it in any slot 1-7.
- **FR-002**: The card MUST decode the 6551 at `$C0n8`-`$C0nB`, the DIP switch banks at `$C0n1` and `$C0n2`, and the rest of its 16-byte I/O window as the hardware does, with the bit layout of the switch registers taken from Apple's SSC documentation.
- **FR-003**: With the firmware installed, `$Cn00`-`$CnFF` MUST read the top 256 bytes of the 2 KB firmware, and `$C800`-`$CEFF` MUST read the firmware's first 1,792 bytes while the card's expansion ROM is selected.
- **FR-004**: An access to `$Cn00`-`$CnFF` MUST select the card's expansion ROM, and an access to `$CF00`-`$CFFF` or a reset MUST deselect it, on every machine the card is offered for, including under the //e's `INTCXROM`, `SLOTC3ROM` and `INTC8ROM` rules.
- **FR-005**: The 6551's interrupt MUST drive the CPU's IRQ line through the card as the hardware routes it, including the effect of switch SW2-6.
- **FR-006**: The firmware MUST be supplied by the user as a file, found the way Casso's other ROMs are. Settings MUST NOT turn the card on without it: turning the card on while the firmware is absent MUST offer to download it, and the card MUST be turned on only once the download succeeds; after a declined or failed download the card MUST stay off. Its absence MUST NOT stop or delay the machine's start, and the startup download dialog MUST NOT open for it.
- **FR-007**: When the card is on and its firmware is missing or invalid, which only a config edited outside Settings or a firmware file removed later can cause, Casso MUST display an infobar each time the machine starts, with the file Casso looked for and a Download button. A successful download MUST install the firmware for the next reset.
- **FR-008**: Without the firmware, the 6551, the switches and the interrupt MUST work as with it, and the ROM space MUST read as an empty ROM socket reads.
- **FR-009**: The user MUST be able to set the card's DIP switches from Settings as the settings they select (mode, baud rate, data format, parity, line feed after carriage return, interrupts, and the printer-mode options), each offered only in the modes where its switches have that meaning, with a drawing of SW1 and SW2 at the resulting positions. Clicking a switch in the drawing MUST flip it and update the settings. A note under Mode MUST state what the selected mode is for, and info tips after the line feed after carriage return and interrupts labels MUST state what those settings do. The settings MUST be saved per machine and MUST take effect in the switch registers without a reset.
- **FR-010**: The default switch settings MUST be communications mode, 9,600 baud, 8 data bits, 1 stop bit, no parity, no line feed after carriage return, and interrupts off.
- **FR-011**: The 6551 MUST transmit each byte over one character time at the baud rate, word length, parity and stop bits set in its registers, with its transmit-register-empty flag and transmitter interrupt following that timing, wherever the 6551 is emulated, the //c's ports included.
- **FR-012**: The 6551 MUST deliver received bytes from a host connection no closer together than one character time at the format set in its registers, with a single receive data register and overrun reported as the datasheet specifies. The host side MUST be able to queue bytes ahead of delivery without loss.
- **FR-013**: Baud code 0 MUST run at 115,200 baud on the SSC, as its 1.8432 MHz crystal clocks it.
- **FR-014**: The 6551's modem lines MUST be visible to a host connection: DTR and RTS from the command register out, and DSR, DCD and CTS in, with CTS gating the transmitter and DCD and DSR read in the status register as the datasheet specifies.
- **FR-015**: The card's serial connector MUST accept an attachment chosen in Settings: none, a loopback plug, a file that receives transmitted bytes, or a file whose bytes are sent into the card one character time apart, each with a note under the drop-down. The attachment interface MUST let spec 033 add Casso Explorer, a TCP port and a COM port without changing the card.
- **FR-016**: The card's pacing MUST be measured in emulated CPU cycles, so it is the same at any emulation speed and pauses with the machine.
- **FR-017**: Ctrl-Reset and power-on MUST reset the card as the hardware's reset line resets it.
- **FR-018**: The emulation MUST be written from the 6551 datasheet and Apple's published SSC manuals only, never from the source of another emulator.
- **FR-019**: The shipped Apple ][, ][+, //e and enhanced //e configs MUST list a Super Serial Card in slot 2, off by default and turned on from its checkbox on the Hardware tab, and existing configs MUST be migrated to list it. The migration MUST NOT replace a card already in slot 2. A card in another slot is configured by editing a custom machine config.
- **FR-020**: When turning the card on in Settings adds the Serial card tab, the tab's label MUST shimmer once, as the update indicator's text does, unless the system's animations are turned off.

### Key Entities

- **Super Serial Card**: a card in one slot of a ][, ][+ or //e, with its 6551, its two switch banks, its firmware ROM and its expansion-ROM select.
- **Switch settings**: the positions of SW1-1 to SW1-7 and SW2-1 to SW2-7, saved per machine with the card's slot entry.
- **Firmware ROM**: Apple's 2 KB SSC firmware, supplied by the user; present, missing or invalid.
- **Serial connector attachment**: what is plugged into the card's connector: nothing, a loopback plug, a file to receive what it sends, a file to send in, or, from spec 033, a host connection. It exchanges bytes and modem-line states with the 6551.
- **Receive queue**: bytes a host connection has sent that the 6551 has not yet received, delivered one character time apart.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With Apple's firmware installed, `PR#2` and `LIST` of a 100-line Applesoft program produce a file identical to the listing as the firmware formats it.
- **SC-002**: With the firmware installed, the Pascal 1.1 identification bytes in the card's $Cn00 page identify a serial card, as Apple's manual documents them.
- **SC-003**: Over a looped-back connector, a program receives 64 KB at 19,200 baud with no overruns when it reads each byte within one character time, and with an overrun reported on the first byte it fails to read in time.
- **SC-004**: At every baud rate the switches offer and at 115,200 baud, the emulated time to transmit 1,000 bytes is within 1% of the character time multiplied by 1,000.
- **SC-005**: With the firmware absent, the machine starts, Casso reports which file is missing, and a program driving the 6551 directly sends and receives as in SC-003.
- **SC-006**: The //c's serial ports pass the same transmit-timing check as the SSC, and the //c's existing serial behavior passes its existing tests.

## Assumptions

- The Apple ][ is included beside the ][+ and //e, since it has the same slots and Apple's firmware supports it.
- The card's DIP switch register layout, its interrupt routing and its RS-232 signal wiring are taken from Apple's *Installation and Operating Manual* (1981) during planning; the reference card and the 1985 user's manual list the switch meanings but not the register bits.
- The firmware download fetches Apple's 341-0065-A image from the same Apple II Documentation Project mirror Casso already uses for the //c ROM.
- The jumper block's MODEM and TERMINAL positions swap RS-232 signals outside the chip, so they matter only to a host connection that exposes those signals; Casso presents the card as if wired MODEM, straight through.
- Host connections to Casso Explorer, a TCP port or a COM port, and the link itself, are spec 033's. This spec supplies the attachment point and its pacing.
- Spec 043 (Uthernet II) is separate and unaffected.
