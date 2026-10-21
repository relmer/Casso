# Research: Super Serial Card

Sources, all permitted by FR-018:

- Apple, *Super Serial Card Installation and Operating Manual* (1981), A2L0044: chapter 4 "How the SSC works" (pages 45-48) and appendix A "Firmware" (pages 49-57). Read from the scanned copy on the Apple II Documentation Project mirror. Cited below as IOM.
- Apple, *Super Serial Card Reference Card* and *User's Manual* (1985), same mirror. Cited as the reference card.
- Western Design Center, *W65C51N Datasheet* (2024), for 6551 register semantics shared with the Synertek SY6551 the SSC uses. Its transmitter-empty erratum is specific to the W65C51N and is not modeled.
- Jim Sather, *Understanding the Apple IIe* (1985), chapter 5, for I/O SELECT' and I/O STROBE' under the //e's ROM switches.

No emulator source was consulted.

## R-001: Switch registers

**Decision**: The card's I/O window at `$C080 + slot*16` decodes as IOM table A-7:

| Offset | Bits | Content |
|---|---|---|
| +1 (DIPSW1) | 0 | SW1-6 |
| | 1 | SW1-5 |
| | 4-7 | SW1-4, SW1-3, SW1-2, SW1-1 (bit 7 is SW1-1) |
| +2 (DIPSW2) | 0 | CTS, 0 when true |
| | 1-3 | SW2-5, SW2-4, SW2-3 |
| | 5 | SW2-2 |
| | 7 | SW2-1 |
| +8 to +B | | 6551 data, status, command, control |

A switch that is OFF reads 1 and one that is ON reads 0 (IOM page 47: 3.3K pull-ups, a closed switch pulls the input low). Bits the LS365 buffers do not drive (DIPSW1 bits 2-3, DIPSW2 bits 4 and 6) and offsets the card does not decode read as the floating bus. Writes to the switch offsets are ignored.

**Rationale**: The firmware reads exactly these bits, so any other layout runs Apple's firmware in the wrong mode.

**Alternatives considered**: Driving undecoded bits as 1. Not chosen: the buffers do not drive those bits, and Casso models an undriven read as the floating bus.

## R-002: Switches with no register bit

**Decision**: SW2-6, SW1-7 and SW2-7 are not readable (IOM page 47). SW2-6 ON connects the 6551's IRQ output to the Apple's IRQ line; OFF disconnects it, and the 6551's own status bit 7 latches as usual. SW1-7 and SW2-7 select which RS-232 pin feeds the 6551's DCD input. A serial connector attachment in Casso presents logical CTS, DSR and DCD lines rather than DB-25 pins, so SW1-7 and SW2-7 are saved and listed in Settings but change no emulated behavior.

**Rationale**: SW2-6 is the switch that FR-005 depends on; the other two route pins the emulation has no counterpart for.

## R-003: Firmware ROM mapping and the expansion-ROM latch

**Decision**: The ROM is 2 KB. IOM table 4-4:

| ROM offset | Apple address |
|---|---|
| `$0700`-`$07FF` | `$Cs00`-`$CsFF` |
| `$0000`-`$06FF` | `$C800`-`$CEFF` |

`$CF00`-`$CFFF` is not mapped: an access there resets the card's latch (IOM page 46, "the $CFxx addresses are unusable"), and reads float. The latch is set by any access, read or write, to `$Cs00`-`$CsFF`, reset by any access to `$CF00`-`$CFFF`, and reset by the Apple's RESET line. The ROM drives `$C800`-`$CEFF` only while the latch is set.

**Rationale**: This corrects the spec's first draft, which had `$CFFF` alone deselecting and the whole of `$C800`-`$CFFF` mapped. The spec is updated to match.

## R-004: The //e's ROM switches and the card

**Decision**: Following Sather chapter 5, the motherboard asserts slot `s`'s I/O SELECT' (its `$Cs00` page) only while INTCXROM is reset, and for slot 3 only while SLOTC3ROM is also set. it asserts I/O STROBE' (`$C800`-`$CFFF`) only while INTCXROM and INTC8ROM are both reset. An access the card does not see neither sets nor resets its latch. In particular, a `$CFFF` access while INTC8ROM is set clears INTC8ROM and does not deselect the card.

**Rationale**: Sather: "if INTCXROM OR INTC8ROM is configured for internal response, then access to $C800-$CFFF results in ROMEN1' low (active) and I/O STROBE' high (inactive)."

## R-005: Two cards selected at once

**Decision**: When more than one card's latch is set, a read of `$C800`-`$CEFF` returns the AND of every selected card's byte.

**Rationale**: Apple's protocol prevents the case (every card's firmware touches `$CFFF` before selecting itself), so only a misbehaving program causes it. On the hardware two TTL drivers fight and the one driving low generally pulls the line low, which an AND approximates. Choosing the lowest slot instead would hide the contention entirely.

## R-006: Baud rates and the crystal

**Decision**: The SSC clocks its 6551 from a 1.8432 MHz oscillator (IOM page 45). Control bits 0-3 select the transmit rate: code 0 is XTLI/16 = 115,200 baud; codes 1-15 are 50, 75, 109.92, 134.58, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600 and 19,200 (W65C51N datasheet table, matching the reference card). Control bit 4 set runs the receiver at the transmit rate; clear runs it at XTLI/16, which on the SSC is again 115,200 baud. The //c's ports use the same crystal, so the same table applies there.

A character time is (1 start + word length + parity bit if enabled + stop bits) bit times. Stop bits are 1, or 2 when control bit 7 is set, or 1.5 with a 5-bit word and no parity (the datasheet's rule). Cycle counts are stored in fixed point against the machine's CPU clock (the `clockSpeed` in the machine config), so pacing does not drift.

**Alternatives considered**: Treating code 0 as unsupported, as the IOM table lists the external clock. Not chosen: that table documents the firmware's commands, while the hardware runs at 115,200 baud, which is what ADTPro selects.

## R-007: Transmitter timing

**Decision**: Model the 6551's two transmit registers. A write to the data register fills the transmit data register (TDR) and clears TDRE. Whenever the transmit shift register is idle, CTS is true and the transmitter is enabled (DTR on and the TIC bits not "transmitter off"), the TDR moves into the shift register and TDRE sets again. The shift register finishes one character time later, and only then is the byte passed to the attachment. With the TIC bits at "transmit break", nothing shifts and the attachment is notified that break is asserted.

A transmitter interrupt is raised when TDRE sets with the TIC bits at "interrupt enabled".

**Rationale**: This is the datasheet's double buffering: after the first byte from idle TDRE sets again almost at once, and after the second byte TDRE stays clear for a whole character time, which is what a program polling TDRE observes on the hardware (SC-004).

## R-008: Receiver timing and the receive queue

**Decision**: Bytes from the attachment go into a receive queue owned by the 6551 model, safe to fill from any thread. On the emulation thread, while DTR is on and the queue is not empty, the 6551 moves one byte into the receive data register each receiver character time, starting one character time after the first byte is queued into an idle receiver. A byte moved while RDRF is set sets the overrun bit and replaces the unread byte. The receiver interrupt is raised as each byte lands, when enabled. With DTR off the queue is paused, not discarded (IOM table A-7: DTR off disables the receiver). Echo mode retransmits each received byte through the transmitter.

Word lengths under 8 bits mask the received byte to that length; parity and framing errors are never produced, since an attachment delivers bytes rather than a waveform.

**Rationale**: FR-012 and spec 033 FR-095: a host can deliver bytes in bursts of any size, and the guest reads them one character time apart.

**Alternatives considered**: A FIFO in front of the data register. Not chosen: the 6551 has none, and a guest that is too slow must see an overrun as it would on the hardware.

## R-009: Modem lines

**Decision**: The attachment is notified when DTR or RTS changes (DTR from command bit 0; RTS low for TIC values 01, 10 and 11) and when break starts or stops. It sets CTS, DSR and DCD. Status bit 5 (DCD) and bit 6 (DSR) read 0 while the line is true. A change in DCD or DSR sets the status IRQ bit and raises the interrupt while DTR is on. CTS false stops the transmitter from loading a new character; DIPSW2 bit 0 reports the same CTS line. With nothing attached, CTS, DSR and DCD are all true, which is what the //c's ports report today.

## R-010: Ticking

**Decision**: `MachineHost` ticks every 6551 once per instruction with the cycles it took, as it ticks the disk controller and the Mockingboard. An idle 6551 (empty queue, idle transmitter) returns on its first test. Pausing the machine stops the ticks, so pacing pauses with it, and maximum speed changes wall time but not emulated time.

## R-011: Firmware file and its discovery

**Decision**: The firmware lives at `Devices/SuperSerialCard/SSC.rom` under Casso's asset roots, 2,048 bytes. A slot entry specifies it with a new optional `firmware` key rather than `rom`, because `rom` is a required 256-byte `$Cn00` image whose absence fails the load. `firmware` resolves through the same search paths; a missing file or one of the wrong size results in a card without firmware, with the reason stored for the infobar. `CollectRomFiles` and `GetRequiredRoms` skip the key, so the startup download dialog never lists it.

The download source is the Apple II Documentation Project mirror's `Apple II Super Serial Card ROM - 341-0065-A.bin`, the same host the //c ROM comes from. A catalog row lists it, and a new on-demand download entry point serves the Settings offer and the infobar's Download button.

## R-012: Machine configs and migration

**Decision**: The shipped Apple ][, ][+, //e and enhanced //e configs gain `{ "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": false }`, with their version stamps bumped and the prior hashes added to the upgrade table. `MachineConfigUpgrade::MigrateUserConfig` adds the same entry to a user config that has no slot-2 entry, following the existing slot-1 printer migration, and does not alter a slot 2 that is already occupied.

## R-013: Settings

**Decision**: A "Serial card" page joins the Settings sheet while the staged hardware has the SSC enabled, as the Disk page follows the disk controller. It has drop-downs for mode, baud rate, data format, parity, line feed after carriage return and interrupts, plus the printer-mode delay and line-width settings in printer mode; the resulting SW1 and SW2 positions as a readout; the connector attachment with a file path for the file attachments; and the firmware status with a Download button when it is missing. The switch settings and attachment are saved in the slot entry (`switches`, `ports`), so they travel with the machine config. Enabling the card on the Hardware tab with no firmware offers the download at once.

The page's model (switch encoding, the per-mode setting list, the readout) lives in core and is unit-tested; the page itself follows the existing pages.

## R-014: Infobar

**Decision**: When the machine starts with an enabled SSC whose firmware is missing or invalid, a `DxuiActionBanner` reports it with a Download button, hosted the way `ShellDisks` hosts the disk-change banner. A successful download installs the file and the banner offers a reset to load it.
