# Contract: Apple /// hardware map

The addresses and bits the emulation implements. **Checked** is the Apple
document that confirms the row. A row with an empty **Checked** column is
not implemented until it is filled.

Sources, all in `%USERPROFILE%\Downloads\Casso\apple3\docs`:

- **SRM**: Apple /// SOS Reference Manual, volume 1 (`A3SOSReferenceVol.1.pdf`)
- **SOS**: Apple's SOS 1.3 source listing (`Apple 3 Source - SOS 1.3 List.pdf`)
- **CON**: Apple's console driver source (`Apple 3 Source - Console Driver.pdf`)
- **RS**: Apple's RS-232 driver source (`Apple 3 Source - RS-232 Driver.pdf`)
- **SPEC**: Apple's "Apple III: Specifications" (`Apple III Specifications.pdf`)
- **J82**, **J83**: John Jeppson, Softalk, August 1982 and May 1983 (not Apple)

## Environment register, `$FFDF`

Address: SOS, `E.REG`. Bit meanings below are from J83 and are not yet
confirmed in an Apple document.

| Bit | 0 | 1 | Checked |
|---|---|---|---|
| 0 | `$F000-$FFFF` is RAM | `$F000-$FFFF` is ROM | |
| 1 | ROM 2 | ROM 1 | |
| 2 | alternate stack | stack at page `$01` | |
| 3 | `$C000-$FFFF` read and write | `$C000-$FFFF` read-only | |
| 4 | reset key off | reset key on | |
| 5 | video off | video on | |
| 6 | `$C000-$CFFF` is RAM | `$C000-$CFFF` is I/O | |
| 7 | 2 MHz | 1 MHz | |

## Bank register, `$FFEF`

| Fact | Checked |
|---|---|
| The bank register is at `$FFEF` and is the E VIA's port A (`B.REG`, `E.IORA`) | SOS; SRM 2.1.1 |
| Bits 0-3 are outputs (E VIA DDRA is set to `$0F`); they select the bank | SOS, system initialization |
| Bits 4-7 are inputs; slot interrupt flags per J83 | |

## Zero page register, `$FFD0`

| Fact | Checked |
|---|---|
| The zero page register is at `$FFD0` (`Z.REG`) | SOS |
| Interpreter zero page `$1A`, stack `$1B`; SOS zero page `$18`, stack `$01` | SRM 2.2.1 |
| Alternate stack page is the zero page XOR `$01` | consistent with SRM 2.2.1; rule itself from J83 |
| Extension page is `$16` for the interpreter and `$14` for SOS (zero page XOR `$0C`) | SRM 2.2.2, 2.2.3 |

## Memory

| Fact | Checked |
|---|---|
| S-bank at `$0000-$1FFF` and `$A000-$FFFF`; one switchable bank at `$2000-$9FFF` | SRM 2.1.1 |
| Banks are numbered `$0-$E`; the highest is `$2` for 128K, `$6` for 256K, `$E` for 512K | SRM 2.1.1 |
| ROM is 4K | SPEC |
| Behavior of a bank beyond the installed memory | |

## Enhanced indirect addressing

| Fact | Checked |
|---|---|
| Applies to every instruction that allows indirect addressing, both the X and the Y forms | SRM 2.1.2 |
| Data access only; code cannot execute through it | SRM 2.1.2 |
| The extension byte for a pointer at zero page `nn`, `nn+1` is at extension page offset `nn+1` | SRM 2.4.2.2 |
| Bit 7 of the extension byte enables it; bits 0-3 are the bank pair; bits 4-6 are unused | SRM 2.1.2, Figure 2-3 |
| Bit 7 clear: ordinary indirect addressing in the S-bank and current bank | SRM 2.1.2 |
| `$8n`: pointer `$0000-$7FFF` is bank `n`, `$8000-$FFFF` is bank `n+1` | SRM 2.1.2, Figure 2-4 |
| `$8n:0000-$8n:00FF` is not reachable: the access goes to the current zero page | SRM 2.1.2 |
| With the highest bank `m`, `$8m:8000` and above is undefined | SRM 2.1.2 |
| `$8F`: the S-bank and bank 0 in the standard arrangement, bank 0 at `$2000-$9FFF` | SRM 2.1.2 |
| `$8F` also exposes the RAM under `$FFD0-$FFEF` | J82 only |
| Whether the extension byte fetch costs a cycle | |

## VIAs

| Address | Function | Checked |
|---|---|---|
| `$FFD0-$FFDF` | D VIA: `D.DDRB` `$FFD2`, `D.DDRA` `$FFD3`, `D.T2` `$FFD8`, `D.ACR` `$FFDB`, `D.PCR` `$FFDC`, `D.IFR` `$FFDD`, `D.IER` `$FFDE` | SOS |
| `$FFE0-$FFEF` | E VIA: `E.IORB` `$FFE0`, `E.DDRB` `$FFE2`, `E.DDRA` `$FFE3`, `E.T2` `$FFE8`, `E.ACR` `$FFEB`, `E.PCR` `$FFEC`, `E.IFR` `$FFED`, `E.IER` `$FFEE` | SOS |
| `$FFE0` | E VIA port B is the sound port; bits 0-5 are outputs (DDRB `$3F`), the 6-bit DAC | SOS, system initialization; SPEC |
| Initialization | D: PCR `$76`, IER `$82` enabled. E: DDRB `$3F`, DDRA `$0F`, ACR `$00`, PCR `$63` | SOS, system initialization |
| Each remaining port and control line (keyboard, vertical blanking, clock chip, slot and Apple II lockout signals) | | from the schematics and the Service Reference Manual |

## I/O space, `$C000-$CFFF`

| Address | Function | Checked |
|---|---|---|
| `$C000` | keyboard data, `KAPORT` | CON |
| `$C008` | keyboard flags, `KBPORT` | CON |
| `$C010` | clear keyboard strobe, `KYBDSTRB` | CON |
| `$C030` | speaker toggle | |
| `$C040` | beeper, `BELL` | CON |
| `$C050-$C057` | video mode switches, `VMODE0` to `VMODE3` at `$C050`, `$C052`, `$C054`, `$C056`, each with its pair at the next address | CON |
| `$C058`, `$C05A`, `$C05E` | A/D select, `AD.SEL0`, `AD.SEL2`, `AD.SEL1` | SOS |
| `$C05C` / `$C05D` | A/D ramp charge / start timeout | SOS |
| `$C060-$C063` | port B switch 1, port A switch 0, port B switch 0, port A switch 1 | SOS |
| `$C064`, `$C065` | `SLOT2`, `SLOT1` | SOS |
| `$C066` | A/D timeout flag | SOS |
| `$C070` | clock chip, `CLOCK` | SOS |
| `$C0D0`-`$C0D5` | drive motor select and deselect: external 2 `$C0D0`/`$C0D1`, external 1 `$C0D2`/`$C0D3`, internal `$C0D4`/`$C0D5` (even deselects, odd selects, except internal: `$C0D4` selects) | SOS, disk driver |
| `$C0D8` / `$C0D9` | smooth scroll off / on, `SCRLDSBL` | SOS; CON |
| `$C0DA` / `$C0DB` | character download off / on, `DNLDDSBL`, `DNLDENBL` | CON |
| `$C0DC` / `$C0DD` | `ENSEL` off / on | SOS |
| `$C0DE` / `$C0DF` | `ENSIO` input / output | SOS |
| `$C0E8` / `$C0E9` | drive motors start power-off timeout / on | SOS, disk driver |
| `$C0EA` / `$C0EB` | I/O select internal drive / external drives | SOS, disk driver |
| `$C0EC`-`$C0EF` | Q6L, Q6H, Q7L, Q7H (the Disk II layout) | SOS, disk driver |
| `$C0F0`-`$C0F3` | 6551: data, status, command, control | RS |
| `$C020`, `$CFFF` | deselect all slots | J83 only |
| `$C090`, `$C0A0`, `$C0B0`, `$C0C0` | slots 1 to 4 device I/O | J83 only |

### Drive selection

| Drive | Internal | External 1 | External 2 |
|---|---|---|---|
| `.D1` | 1 | any | any |
| `.D2` | any | 0 | 1 |
| `.D3` | any | 1 | 0 |
| `.D4` | any | 1 | 1 |

Checked: SOS, disk driver equates.

## Video

| Fact | Checked |
|---|---|
| Seven modes: 80-column and 40-column monochrome text, 40-column text with 16 foreground and background colors, 280 by 192 monochrome, 280 by 192 with 16 colors and limits, 140 by 192 with 16 colors, 560 by 192 monochrome | SPEC |
| A software-definable 128-character set, normal or inverse | SPEC |
| Color signals are a 16-level gray scale on a monochrome display | SPEC |
| Either of two screen buffers can be displayed | SPEC |
| The switch combination for each mode, the color encoding, buffer addresses, the character cell, and the download sequence | | from CON and the Standard Device Drivers Manual |

## Timing

| Fact | Checked |
|---|---|
| Nominal 2 MHz; about 1.4 MHz average with the display on, 1.8 MHz with it off | SRM 2.2.2; SPEC |
| SOS can run a driver at full speed or a fixed 1 MHz | SRM 2.2.3 |
| When exactly the clock slows, the line and frame lengths, the VIA clock | |

## Connectors and physical

| Fact | Checked |
|---|---|
| Black and white video: RCA jack. Color video: DB-15 with NTSC color, NTSC monochrome, RGB, composite sync and power | SPEC |
| Audio: miniature phono jack; a built-in two-inch speaker | SPEC |
| Serial: RS-232-C, DB-25 female | SPEC |
| Joystick and Silentype ports: two DB-9 | SPEC |
| Four 50-pin slots | SPEC |
| Keyboard: 61 keys and a 13-key numeric pad, two Apple keys, four arrow keys with two-speed repeat | SPEC |
| CPU: 6502B with outside circuitry for extended addressing, the relocatable stack and zero page, and memory mapping | SPEC |
