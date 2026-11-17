# Contract: Apple /// hardware map

The addresses and bits the emulation implements. **Checked** is the Apple
document that confirms the row. A row with an empty **Checked** column is
not implemented until it is filled, from a document or from an existing
emulator under the limits in research.md R1.

Sources, all in `%USERPROFILE%\Downloads\Casso\apple3\docs`:

- **SVC**: Apple /// Level 2 Service Reference Manual, 1982, by chapter page
  (`Apple III Service Reference Manual 1982.pdf`; page images, no text layer)
- **SRM**: SOS Reference Manual, volume 1 (`A3SOSReferenceVol.1.pdf`)
- **DDW**: SOS Device Driver Writer's Guide (`A3SOSDeviceDriverWritersGuide_alt.pdf`)
- **SDD**: Standard Device Drivers Manual (`A3StandardDeviceDrivers.pdf`)
- **SOS**: Apple's SOS 1.3 source listing (`Apple 3 Source - SOS 1.3 List.pdf`)
- **CON**: Apple's console driver source (`Apple 3 Source - Console Driver.pdf`)
- **RS**: Apple's RS-232 driver source (`Apple 3 Source - RS-232 Driver.pdf`)
- **SPEC**: "Apple III: Specifications" (`Apple III Specifications.pdf`)
- **POG**: Apple /// Plus Owner's Guide (`Apple III Plus Owner's Guide.pdf`)
- **MON**: Monitor III Owner's Manual (`Apple III Monitor III Owner's Manual.pdf`)
- **J82**, **J83**: John Jeppson, Softalk, August 1982 and May 1983 (not Apple)

## Environment register, `$FFDF` (D VIA port A, all outputs)

| Bit | Signal | 0 | 1 | Checked |
|---|---|---|---|---|
| 0 | ROMSEL1 | ROM deselected: `$F000-$FFFF` is RAM | ROM selected | DDW p. 16-17; SVC 3.11, 3.13; polarity J83 |
| 1 | ROMSEL2 | with bit 0, selects the ROM set | | SVC 3.13 |
| 2 | ALTSTK | alternate stack | primary stack, page `$01` | DDW p. 17; SVC 2.10 |
| 3 | RWPR | `$C000-$FFFF` writable | write-protected (top 16K) | DDW p. 17; SVC 3.13 |
| 4 | RESETLK | reset key ignored, and NMI from the slots off | reset key works | DDW p. 17; SVC 3.13, 8.8 |
| 5 | SCRN | video blanked | video on | DDW p. 17; SVC 3.13 |
| 6 | IOEN | `$C000-$CFFF` is RAM | I/O space on | DDW p. 17 |
| 7 | SEL1M | full speed | 1 MHz | DDW p. 17 |

SOS calls a driver with `$74`, `$64`, `$54` or `$44` in the register (DDW
p. 17).

## Bank register, `$FFEF` (E VIA port A)

| Bits | Function | Checked |
|---|---|---|
| 0-2 (3) | bank select, outputs (DDRA is `$0F`); the bank at `$2000-$9FFF` | SVC 3.14-3.15; SOS; DDW p. 18 |
| 4, 5 | slot interrupt request lines, inputs | SVC 3.14-3.15 (figure and text differ on which slots) |
| 6 | `AII SW`, input | SVC 3.14; its meaning is missing from the text (see Open rows) |
| 7 | the IRQ line, wrapped around for diagnostics | SVC 3.16 |

## Zero page register, `$FFD0` (D VIA port B)

| Fact | Checked |
|---|---|
| Selects the page used for every zero page reference | DDW p. 17; SVC 3.15 |
| `$18` in SOS and drivers, `$1A` in the interpreter, `$00` in interrupt handlers | DDW p. 17; SRM 2.2.1 |
| Alternate stack page is the zero page XOR `$01` (the page after an even zero page, before an odd one) | SVC 2.10 |
| Extension page is the zero page XOR `$0C`: `$16` for the interpreter, `$14` for SOS | SVC 2.10; SRM 2.2.2; DDW p. 18 |
| Port B is also the address register of the clock chip; SOS restores the zero page after each clock access | SVC 3.15 |

## Memory

| Fact | Checked |
|---|---|
| S-bank at `$0000-$1FFF` and `$A000-$FFFF`; one switchable 32K bank at `$2000-$9FFF` | SRM 2.1.1 |
| Banks `$0-$E`; the highest is `$2` for 128K and `$6` for 256K | SRM 2.1.1 |
| ROM is 4K | SPEC; POG appendix D |
| 128K minimum | POG appendix D |

## Enhanced indirect addressing

| Fact | Checked |
|---|---|
| Active only while the zero page register is `$18-$1F` | SVC 2.10 |
| Applies to every instruction with indirect addressing, the X and the Y forms | SRM 2.1.2 |
| Data access only | SRM 2.1.2 |
| The extension byte is at the extension page offset of the pointer's high byte | SRM 2.4.2.2; DDW p. 18 |
| Bit 7 enables it; the low bits are the bank pair; bits 4-6 unused | SRM 2.1.2, Figure 2-3; SVC 2.10 |
| `$8n`: pointer `$0000-$7FFF` is bank `n`, `$8000-$FFFF` is bank `n+1` | SRM 2.1.2 |
| `$8n:0000-$8n:00FF` goes to the current zero page | SRM 2.1.2 |
| Past the highest bank the result is undefined | SRM 2.1.2 |
| `$8F`: the S-bank and bank 0 in the standard arrangement | SRM 2.1.2 |
| During an extended access the I/O and the LSI chips are disabled and RAM is on the data bus (so RAM under `$C000-$CFFF` and under the VIAs is reachable) | SVC 2.10 |

## D VIA, `$FFD0-$FFDF`

| Line | Function | Checked |
|---|---|---|
| Port A | environment register | SVC 3.11, 3.13 |
| Port B | zero page register; clock chip address | SVC 3.11, 3.15 |
| CA1 | OR of the slot interrupt requests ("any slot"); cleared by storing `$02` in `$FFDD` | SVC 2.30, 3.13; DDW chapter 5 |
| CA2 | SW1 / margin switch | SVC 2.30, 3.13 |
| CB1, CB2 | SCO and SER: serial clock and data for the Silentype | SVC 2.30, 3.13 |
| Clock | PRE1M, a 1 MHz clock | SVC 3.10 |
| Initialization | PCR `$76`, IER `$82` | SOS, system initialization |

## E VIA, `$FFE0-$FFEF`

| Line | Function | Checked |
|---|---|---|
| Port A | bank register (above) | SVC 3.14 |
| Port B bits 0-5 | sound register: the 6-bit DAC, outputs (DDRB `$3F`) | SVC 3.14, 3.16; SOS |
| Port B bit 6 | I/O count line from the slots | SVC 3.17 |
| Port B bit 7 | monitors the NMI line from the slots (`IONMI`) | SVC 3.17, 2.29 |
| CA1 | clock chip interrupt, negative edge | SVC 3.16 |
| CA2 | keyboard interrupt, negative edge; IFR bit 0 | SVC 3.16 |
| CB1, CB2 | vertical blanking; with the shift register, one interrupt per 8 blanking periods | SVC 3.16 |
| Initialization | DDRB `$3F`, DDRA `$0F`, ACR `$00`, PCR `$63` | SOS, system initialization |

The 6522 and the 6551 cannot be accessed at full speed; the machine runs
those accesses at 1 MHz (DDW p. 21, p. 80; SVC 5.8).

## I/O space, `$C000-$CFFF`

Decoded in blocks of 16 (SVC 2.30): `$C00x` keyboard read, `$C01x` reset
keyboard, `$C03x` speaker toggle, `$C04x` beeper, `$C05x` display register,
`$C06x` switch register, `$C07x` real time clock, `$C09x-$C0Cx` slots 1-4,
`$C0Dx` I/O control register, `$C0Ex` disk register, `$C0Fx` ACIA.

| Address | Function | Checked |
|---|---|---|
| `$C000` | KA port: ASCII in bits 0-6, bit 7 is the data ready flag | SVC 8.5; CON |
| `$C008` | KB port (table below) | SVC 8.5; CON |
| `$C010` | clear the keyboard strobe | SVC 2.30; CON |
| `$C020` | deselect the `$C800` space (pin 39 of the slots low for about 1 ms) | SVC 10.3-10.4 |
| `$C030` | speaker toggle, as on the Apple II | SVC 10.3-10.4 |
| `$C040` | beeper: a 0.1 second 1 kHz tone | SVC 10.3; CON |
| `$C050` / `$C051` | black and white / color (graphics / text on an Apple II) | SVC 6.24; CON |
| `$C052` / `$C053` | 40 columns / 80 columns | SVC 6.24; CON |
| `$C054` / `$C055` | page 1 / page 2 | SVC 6.24; CON |
| `$C056` / `$C057` | text / graphics | SVC 6.24; CON |
| `$C058`/`9`, `$C05E`/`F`, `$C05A`/`B` | A/D select 0, 1, 2 (off / on) | SVC 10.4; SOS |
| `$C05C` / `$C05D` | A/D ramp charge / start timeout | SVC 10.4; SOS |
| `$C060`-`$C063` | joystick switches 0-3, bit 7 (`$C060` port B switch 1, `$C061` port A switch 0, `$C062` port B switch 0, `$C063` port A switch 1) | SVC 10.4; SOS |
| `$C064`, `$C065` | `SLOT2`, `SLOT1` | SOS |
| `$C066` | A/D timeout, bit 7 | SVC 10.4; SOS |
| `$C070` | clock chip; millisecond counter in bits 7-4 | SVC 10.4; SOS |
| `$C080 + n*16` | slot `n` device I/O (`$C090`-`$C0CF`) | DDW p. 22 |
| `$Cn00-$CnFF` | slot `n` memory space | DDW p. 22 |
| `$C800-$CFFF` | shared slot space, allocated with `SELC800` | DDW p. 22 |
| `$C0D0`/`1`, `$C0D2`/`3`, `$C0D4`/`5` | drive motor deselect / select: external 2, external 1; internal select / deselect | SOS, disk driver |
| `$C0D8` / `$C0D9` | smooth scroll off / on | SOS; CON |
| `$C0DA` / `$C0DB` | character download off / on | CON |
| `$C0DC` / `$C0DD` | `ENSEL` off / on | SOS |
| `$C0DE` / `$C0DF` | `ENSIO` input / output | SOS |
| `$C0E8` / `$C0E9` | drive motors: start the power-off timeout / on | SOS, disk driver |
| `$C0EA` / `$C0EB` | I/O select: internal drive / external drives | SOS, disk driver |
| `$C0EC`-`$C0EF` | Q6L, Q6H, Q7L, Q7H (listed as `$C08C`-`$C08F` indexed) | SOS, disk driver |
| `$C0F0`-`$C0F3` | 6551: data, status, command, control | DDW p. 21; RS |

### KB port, `$C008`

| Bit | Meaning | Checked |
|---|---|---|
| 0 | 1 = any key down | SVC 8.5 |
| 1 | 0 = Shift down | SVC 8.5 |
| 2 | 0 = Control down | SVC 8.5 |
| 3 | 0 = Alpha Lock set | SVC 8.5 |
| 4 | 0 = Apple 1 (Open Apple) down | SVC 8.5 |
| 5 | 0 = Apple 2 (Solid Apple) down | SVC 8.5 |
| 6 | 1 = start-up, mode not yet set | SVC 8.5 |
| 7 | bit 7 of the key code (set for the special keys) | SVC 8.5, 8.6 |

### Drive selection

| Drive | Internal | External 1 | External 2 |
|---|---|---|---|
| `.D1` | 1 | any | any |
| `.D2` | any | 0 | 1 |
| `.D3` | any | 1 | 0 |
| `.D4` | any | 1 | 1 |

Checked: SOS, disk driver equates.

### A/D input selection

| A/D 2 | A/D 1 | A/D 0 | Input |
|---|---|---|---|
| 0 | 0 | 0 | ground |
| 0 | 0 | 1 | port B, X axis |
| 0 | 1 | 0 | port B, Y axis |
| 0 | 1 | 1 | port A, X axis |
| 1 | 0 | 0 | port A, Y axis |
| 1 | 0 | 1 | clock battery |
| 1 | 1 | 0 | no connection |
| 1 | 1 | 1 | reference voltage |

Checked: SVC 10.4. The software selects an input, charges the ramp for at
least 500 microseconds, starts the ramp and times it until `$C066` goes low
(SVC 10.5; SOS).

## Keyboard

| Fact | Checked |
|---|---|
| 74 keys plus a recessed Reset key; an 8 by 10 matrix scanned by an encoder ROM; Shift, Control, Alpha Lock and the two Apple keys are wired directly | SVC 8.1, 8.7 |
| Every key's code, alone and with Shift, Control and both: the table on SVC 8.6 | SVC 8.6 |
| A key repeats after half a second at 10 per second, and at 30 per second with Apple 2 pressed after it | SVC 8.1; SDD, "The Keyboard" |
| The four arrow keys have a second contact that turns on the fast repeat | SVC 8.1, 8.7 |
| Apple 1 sets bit 7 of the code the console driver returns | SDD, "The Keyboard" |
| Control with Reset is a system reset; Reset alone is a soft reset through NMI; both can be turned off by environment bit 4 | SVC 8.5, 8.8 |
| Plus: 75 keys (62 and 13), with a Delete key | POG appendix D |

## Video

| Fact | Checked |
|---|---|
| Mode selection, from `$C056/7`, `$C052/3`, `$C050/1`: 000 40-column black and white; 001 40-column color; 010 and 011 80-column; 100 280 by 192 black and white; 101 280 by 192 with foreground and background colors; 110 560 by 192; 111 140 by 192 in 16 colors. `$C054/5` selects page 1 or 2 | SVC 6.24 |
| 40-column: page 1 `$0400-$07FF`, page 2 `$0800-$0BFF` | SVC 6.3 |
| 40-column color: characters at `$0400`, colors at `$0800`, one to one; color byte bits 4-7 foreground, bits 0-3 background; page 2 swaps the two | SVC 6.3 |
| 80-column: `$0400` has the first, third... characters and `$0800` the second, fourth...; page 2 swaps them | SVC 6.3-6.4 |
| 280 by 192 black and white: page 1 `$2000-$3FFF`, page 2 `$4000-$5FFF` | SVC 6.4 |
| 280 by 192 color: page 1 dots at `$2000`, colors at `$4000`, one color byte per 7 dots; page 2 swaps them | SVC 6.4-6.5; SDD p. 90 |
| 560 by 192: page 1 `$2000` then `$4000` alternately, 7 dots per byte; page 2 `$6000` and `$8000-$9FFF` | SVC 6.5 |
| 140 by 192: 4 bits per pixel, bytes alternately from `$2000` and `$4000`, 4 bytes to 7 pixels; page 2 `$6000` and `$8000` | SVC 6.5-6.8 |
| In every mode 7 bits of each byte are displayed, least significant first; bit 7 is ignored | SVC 6.5-6.6 |
| The 40-column black and white mode has inverse and flashing | SVC 6.2 |
| 16 colors, numbered 0-15: black, magenta, dark blue, purple, dark green, gray 1, medium blue, light blue, brown, orange, gray 2, pink, green, yellow, aqua, white | SDD, console "Foreground color" |
| On a monochrome display the colors are 16 shades of gray, lighter with a higher number; the black and white output is a linear gray scale | SDD, "The Graphics Driver"; SVC 10.2; MON p. 6 |
| Color connector: four TTL signals XRGB1, 2, 4, 8, composite sync, black and white video, NTSC color video and power | SVC 10.2 |
| Character set: 128 characters of 8 rows, 1,024 bytes | SDD, console control code 16; SPEC |
| Download: 8 characters at a time. For cell `c` (0-7) and row `r` (0-7) the character code is written to page `$08 + (c >> 2) * 2 + (r >= 4)`, offset `$78`, `$7C`, `$F8` or `$FC` plus `c & 3`, and the row's dots to the same address with the page XOR `$0C`. `$C0DB` starts the download and it runs during vertical blanking; `$C0DA` ends it | CON, `LOADCHR` and `DNLDINT` |
| A full set downloads in about a quarter of a second | SDD, console control code 16 |
| Plus: text interlace, 560 by 384, turned on by a switch on the left side of the case | POG p. 16, p. 38, appendix D |

## Timing

| Fact | Checked |
|---|---|
| Master clock 14.318 MHz, divided to 7 MHz, 3.5 MHz and the 2 MHz Q clocks | SVC 5.1 |
| A line is 65 states of the 1 MHz video counter: 25 blanked and 40 displayed | SVC 5.7-5.10 |
| A frame is 262 lines: 192 displayed and 70 of vertical blanking | SVC 5.10 |
| One cycle per line is stretched by two clock times (HPE) | SVC 5.5 |
| A 50 or 60 Hz switch exists on the vertical counter | SVC 5.8 |
| Nominal 2 MHz; 1.4 MHz average with the display on, 1.8 MHz with it off | SRM 2.2.2; SPEC; POG appendix D |
| The clock drops to 1 MHz for the VIAs, the ACIA and the clock chip (`FSPACE`), and from the display signal | SVC 5.8 (gearshift PROM inputs) |
| The exact rule for each of those inputs | |

## Sound

| Fact | Checked |
|---|---|
| Speaker toggle at `$C030`; beeper at `$C040`; 6-bit DAC on E VIA port B | SVC 10.3, 3.16; SOS |
| `.AUDIO`: 64 volume levels, 31 to 5090 Hz | POG appendix D |
| A plug in the audio jack silences the internal speaker | SVC 10.3 |

## Apple II emulation

| Fact | Checked |
|---|---|
| No Language Card, no ROM card; paddles differ; entered by software | SVC 10.1 |
| The Apple II ROM is a modified Autostart ROM in write-protected RAM, loaded from the emulation disk | SVC 10.5 |
| The cassette is absent: `$C020` is the slot deselect and `$C060` reads joystick switch 0; the Monitor's tape routines are stubs | SVC 10.3, 10.6 |
| The keyboard returns Apple II codes on the KA port; the KB port is not locked out | SVC 8.7 |
| The machine runs at 1 MHz in emulation | DDW p. 80 |
| Hi-res is as on an Apple II | SVC 10.3 |
| Control-Reset does not end emulation | SVC 8.5 |

## Connectors and physical

| Fact | Checked |
|---|---|
| Back panel, from the rear, left to right: disk drive connector, port A (Silentype), port B (joystick), color video (DB-15), black and white video (RCA), audio, serial (DB-25), power plug, power switch; the slot openings are above this strip | POG chapter 2, Figure 2-1; SPEC |
| Ports A and B are 9-pin D connectors | POG chapter 2; SPEC |
| Size 17.5 by 18.2 by 4.8 inches (44.45 by 46.23 by 12.19 cm); 26 pounds; cast aluminum base, molded plastic cover | POG appendix D; SPEC |
| CPU: 6502B with outside circuitry for extended addressing | SPEC; POG appendix D |

## Settled from an existing emulator

Apple's documents do not settle these rows. Each fact below was read from
MAME's Apple /// driver (`src/mame/apple/apple3_m.cpp` and `apple3_v.cpp`,
BSD-3-Clause, read 2026-11-17). No code or table was copied. Each is
confirmed by behavior (the ROM diagnostics, the Confidence Test disk, a SOS
boot) when the device is built.

| Item | Fact | Consistent with |
|---|---|---|
| Apple II lockout | E VIA port A bit 6 (`AII SW`) is driven as an output. While it is low, `$FFD0-$FFEF` no longer addresses the VIAs, so the environment, bank and zero page registers cannot be changed, and color text is off. A reset of the VIAs returns the line to an input, which reads high | SVC 3.14 (the signal); SVC 10.1 ("only reset will exit"); with environment bit 4 clear the reset key is ignored, which is SVC 8.5 |
| Slot interrupt inputs | E VIA port A bit 4 is slot 4 and bit 5 is slot 3. `$C064` bit 7 is slot 2 and `$C065` bit 7 is slot 1, each 0 while the slot requests an interrupt | SVC 3.14 figure; SOS `SLOT1`, `SLOT2` |
| ROM select | Environment bit 0 turns the ROM on at `$F000-$FFFF`. Bit 1 selects the primary ROM; the machine has one ROM, so bit 1 is 1 whenever bit 0 is | SVC 3.13; J83 |
| Clock chip | An MM58167 with a 32.768 kHz crystal. Any address `$C070-$C07F` reads or writes the chip register selected by D VIA port B | SVC 3.15; SVC 2.30 |
| Smooth scroll | With `$C0D9` on, each 8-line group of the display is offset by 0 to 7 lines. The offset's three bits are the latches at `$C0E0/1`, `$C0E2/3` and `$C0E4/5`, which are also the disk controller's first three phase lines | SOS and CON use of `$C0D8` |
| Character cell | 7 dots by 8 rows, bit 0 leftmost. A character with bit 7 clear is inverse; bit 7 of each glyph row selects steady inverse (0) or flashing (1) for it | SVC 6.2 (inverse and flashing); SVC 6.6 (7 of 8 bits displayed) |
| Flash rate | about 2 per second | |
| Extension byte | Read in the same cycle as the zero page pointer byte, from the S-bank at the pointer byte's address XOR `$0C00`, with no added cycle. It applies to accesses at `$0100` and above until the next opcode fetch | SVC 2.10 ("the sister fetched data byte"); SRM 2.1.2 |
| `$8F` | `$0000-$1FFF` and `$A000-$FFFF` are the S-bank, `$2000-$9FFF` is bank 0, and `$FFD0-$FFEF` is the RAM under the VIAs | SRM 2.1.2; J82 |
| Absent bank | The bank number wraps modulo the number of banks installed | not confirmed elsewhere |
| Zero page in the banked region | A zero page register value of `$20-$9F` selects a page of the current bank, and `$A0` and above a page of the S-bank | SVC 2.10 |
| Keyboard reset | Control with Reset is a CPU reset; Reset alone is an NMI; both only while environment bit 4 is set | SVC 8.5, 8.8 |
| Beeper | `$C040-$C04D`. `$C04E/F` is a character RAM enable that software does not rely on | SVC 10.3 |
| CPU and VIA clocks | CPU nominal clock is 14.31818 MHz / 7; the VIAs run at 14.31818 MHz / 14; the 6551 has a 1.8432 MHz clock | SVC 5.1, 3.10 |

## Open rows

| Item | What is missing |
|---|---|
| CPU speed | The exact conditions for a 1 MHz cycle. The emulator read above runs at 1 MHz across the displayed part of each of the 192 lines when video is on and at 2 MHz otherwise, and cites a logic analyzer measurement of 1.905 MHz at full speed with the screen off. That does not reproduce Apple's 1.4 MHz average exactly. The gearshift PROM's listing (SVC chapter 17) settles it |
| E VIA port B bit 6 | In SVC 3.17 it is the slots' I/O count line; the emulator read above drives it from horizontal blanking. The schematic (`Sound-Serial Logic.jpg`) settles it |
| Color values | The RGB value of each of the 16 colors and of each gray level, and whether the two gray entries are in a different order in text and in graphics. Measured from the resistor weights on the schematic (`Video Logic.jpg`) and SVC 10.2 |
| `ENSEL`, `ENSIO` | Their effect on the port A serial lines. Used only by the Silentype, which is out of scope; the switches are accepted and do nothing |
