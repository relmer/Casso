# Contract: Apple /// hardware map

The addresses and bits the emulation implements. **Source** is where the row
came from: `article` (John Jeppson, Softalk, August 1982 and May 1983, on
apple3.org), `spec sheet` (Apple's Apple /// specifications), or `unverified`
(from memory). **Checked** is the Apple document and page that confirms the
row; it is empty until research task R1 fills it. A row with an empty
**Checked** column is not implemented.

## Environment register, `$FFDF`

| Bit | 0 | 1 | Source | Checked |
|---|---|---|---|---|
| 0 | `$F000-$FFFF` is RAM | `$F000-$FFFF` is ROM | article | |
| 1 | ROM 2 | ROM 1 | article | |
| 2 | alternate stack | stack at page `$01` | article | |
| 3 | `$C000-$FFFF` read and write | `$C000-$FFFF` read-only | article | |
| 4 | reset key off | reset key on | article | |
| 5 | video off | video on | article | |
| 6 | `$C000-$CFFF` is RAM | `$C000-$CFFF` is I/O | article | |
| 7 | 2 MHz | 1 MHz | article | |

## Bank register, `$FFEF`

| Bits | Function | Source | Checked |
|---|---|---|---|
| 0-3 | bank at `$2000-$9FFF`; `$F` reserved | article | |
| 4-7 | slot interrupt request flags, read-only | article | |

## Zero page register, `$FFD0`

| Fact | Source | Checked |
|---|---|---|
| Selects the page used for zero page accesses | article | |
| Alternate stack page is the zero page XOR `$01` | article | |
| Extended addressing is active for values `$18-$1F` | article | |
| SOS conventions: user `$1A`, kernel and drivers `$18`, interrupts `$00`, monitor `$03` | article | |

## Extended addressing

| Fact | Source | Checked |
|---|---|---|
| Applies to `(zp),Y` | article | |
| Applies to `(zp,X)` | unverified | |
| Extension byte is in the page of the pointer XOR `$0C` | article | |
| Extension byte outside `$80-$8F`: ordinary access | article | |
| `$80-$8E`: pointer `$0000-$7FFF` is bank `n`, `$8000-$FFFF` is bank `n+1` | article | |
| `$8F`: bank 0 at `$2000-$9FFF`, and the RAM under `$FFD0-$FFEF` is visible | article | |

## Memory

| Fact | Source | Checked |
|---|---|---|
| 128K or 256K | spec sheet | |
| System bank at `$0000-$1FFF` and `$A000-$FFFF` | unverified | |
| Switched banks of 32K at `$2000-$9FFF` | article | |
| Highest bank: 128K, 2; 256K, 6 | unverified (3 and 7 in the article) | |
| ROM is 4K at `$F000-$FFFF` | unverified | |

## VIAs

| Address | Function | Source | Checked |
|---|---|---|---|
| `$FFD0-$FFDF` | D VIA; port B is the zero page register, port A the environment register | article | |
| `$FFE0-$FFEF` | E VIA; port A is the bank register | article | |
| `$FFE0` | E VIA port B: sound DAC, bits 0-5 | unverified | |
| `$FFEC`, `$FFEE` | E VIA control used in character set loading | article (unconfirmed there) | |
| `$FFDD` | any-slot interrupt flag | article | |

Every other port line (keyboard, vertical blanking, clock chip select, drive
and slot signals, interrupt sources) is `unverified` and is listed here from
the Service Reference Manual before `Apple3SystemVias` is written.

## I/O space, `$C000-$CFFF`

| Address | Function | Source | Checked |
|---|---|---|---|
| `$C000` | keyboard data | article | |
| `$C008` | keyboard modifier flags | article | |
| `$C010` | clear keyboard strobe | article | |
| `$C020` | deselect all slots | article | |
| `$C030` | speaker toggle | article | |
| `$C040` | beeper | article | |
| `$C050` / `$C051` | black and white / color | article | |
| `$C052` / `$C053` | 40-column or low resolution / 80-column or high resolution | article | |
| `$C054` / `$C055` | display buffer 1 / 2 | article | |
| `$C056` / `$C057` | text / graphics | article | |
| `$C060-$C06F` | joystick and A/D inputs | unverified | |
| `$C070-$C07F` | clock chip | unverified | |
| `$C090`, `$C0A0`, `$C0B0`, `$C0C0` | slots 1 to 4 device I/O | article | |
| `$C100-$C4FF` | slot ROM space | article | |
| `$C800-$CFFF` | shared expansion ROM space; `$CFFF` deselects | article | |
| `$C0D0-$C0DF` | drive select, character set loading, smooth scroll | unverified | |
| `$C0E0-$C0EF` | disk controller (Disk II layout) | unverified | |
| `$C0F0-$C0F3` | 6551: data, status, command, control | article | |

## Video modes

| Mode | Resolution | Colors | Source | Checked |
|---|---|---|---|---|
| Text | 40 by 24 | black and white | spec sheet | |
| Text | 40 by 24 | 16 foreground and background | spec sheet | |
| Text | 80 by 24 | black and white | spec sheet | |
| Graphics | 280 by 192 | black and white | spec sheet | |
| Graphics | 280 by 192 | 16, with limits per cell | spec sheet | |
| Graphics | 140 by 192 | 16 | spec sheet | |
| Graphics | 560 by 192 | black and white | spec sheet | |

Text pages at `$0400-$07FF` and `$0800-$0BFF`; graphics buffers in bank 0 at
`$2000` and `$4000`, with the second high resolution buffer at `$6000`
(article). The switch combination for each mode, the color encoding, the
character cell and the smooth scroll control are `unverified`.

## Timing

Every row is `unverified`: the tick counts of a line and a frame, when the
CPU is slowed to 1 MHz, the VIA clock, the keyboard repeat rates and the
beeper's tone.
