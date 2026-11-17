# Contract: Apple /// device panels

Each panel is an `IDiagnosticsProvider`: an id, a title, and groups of rows
(label, value, optional bit decode), per 035's FR-049. Titles are UI strings
and are also listed in `ui-strings.md`.

| Id | Title | Groups and rows | Visual |
|---|---|---|---|
| `apple3.memory` | Memory and banking | Registers: environment (bit decode), bank, zero page, stack page, Apple II lockout. Mapping: one row per run of pages with the same source | memory map bar with banks |
| `apple3.via.d` | VIA D | Port A, port B (bit decode by Apple /// function), DDRA, DDRB, timer 1, timer 2, shift register, ACR, PCR, IFR, IER (bit decode) | timer meters |
| `apple3.via.e` | VIA E | the same rows for the second VIA | timer meters |
| `apple3.video` | Video | Mode, each switch, display buffer, smooth scroll, interlace (Plus), video enable, beam line and column | none |
| `apple3.charset` | Character set | Loader state, glyph count | glyph grid; the code of the glyph under the pointer |
| `apple3.keyboard` | Keyboard | Data, flags (bit decode), strobe, repeat, interrupt | none |
| `apple3.disk` | Floppy drives | Per drive: quarter track, motor, spin-up, read or write, write protect. Controller: selected drive, select lines, Q6, Q7, latch | head position graphic, four drives |
| `apple3.sound` | Sound | Speaker level, beeper state, DAC value | DAC meter and waveform |
| `apple3.clock` | Clock | Each counter and latch, the time as text, interrupt status and control | none |
| `apple3.serial` | Serial port | the 6551's existing rows | none |
| `apple3.joystick` | Joystick ports | Selected input, each axis, each button | none |
| `apple3.slots` | Slots | Card in each slot, selected slot for `$C800`, slot interrupt flags | none |

## Rules

- A panel is in the panel list only on an Apple /// or Apple /// Plus.
- With the Apple II lockout set, `apple3.memory` has the lockout row set and
  the mapping rows are those of the Apple II layout; the other panels are the
  same devices and stay listed.
- Rows update once per frame while running, per 035's FR-051.
- Panels are read-only.

## New snapshot payloads

| Payload | Content |
|---|---|
| Character set | glyph count, cell width and height, one bit per dot |
| DAC trace | the most recent samples, with the tick of the last one |
| Page sources | per page: system, bank `n`, ROM, I/O, write-protected, none |

## Event logs

| Log | Apple /// rows |
|---|---|
| Disk | the Disk II rows with Drive 1 to Drive 4; a drive select row lists the drive selected |
| Input | reads of `$C000`, `$C008` and `$C010`, joystick reads, host key and controller events |
| Serial | the built-in port's traffic, port labeled "Built-in" |
| I/O | each access labeled by the hardware map's function text |
