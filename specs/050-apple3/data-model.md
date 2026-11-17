# Data Model: Apple /// Machine

Types and state the feature adds. Addresses and bits are in
`contracts/hardware-map.md`.

## Machine

### Apple3, Apple3Plus (`IMachine`)

| Question | Apple /// | Apple /// Plus |
|---|---|---|
| Id | `Apple3` | `Apple3Plus` |
| CPU | 6502 | 6502 |
| Keyboard layout | `Apple3` | `Apple3Plus` |
| Memory options | 128K, 256K | 256K |
| Slot count | 4 | 4 |
| Built-in drive | yes, plus 0 to 3 external | the same |
| Cassette port, annunciators | no | no |
| Interlaced video | no | yes |
| Default monitor | `AppleMonitorIII` | `AppleMonitorIII` |

### Machine definition file

`Resources/Machines/Apple3/Apple3.json` and
`Resources/Machines/Apple3Plus/Apple3Plus.json`. Fields beyond the existing
schema are in `contracts/machine-config.md`.

## Memory

### Apple3Mmu

| State | Type | Notes |
|---|---|---|
| System bank RAM | 32K bytes | pages `$00-$1F` and `$A0-$FF` |
| Bank RAM | 32K bytes per bank | 3 banks for 128K, 7 for 256K (to verify) |
| RAM under the VIAs | 32 bytes | visible through extension byte `$8F` |
| Environment | byte | from the D VIA's port A |
| Bank | 4 bits | from the E VIA's port A |
| Zero page | byte | from the D VIA's port B |
| Apple II lockout | bool | set by the emulation disk; cleared by power-off |
| ROM | 4K | user-supplied |

**Derived mapping**, recomputed on any change: for each of the 256 bus
pages, the read cell block, the write cell block or none, and a page source
for the debugger: system bank, bank `n`, ROM, I/O, write-protected RAM.

**Rules**:
- Bus page `$00` maps to RAM page `Z`. Bus page `$01` maps to page `$01` or,
  with the alternate stack, to `Z XOR $01`.
- Bus pages `$20-$9F` map to the selected bank; an absent bank follows the
  documented behavior.
- `$C0-$CF` is I/O or RAM by environment bit 6; `$F0-$FF` is ROM or RAM by
  bit 0; `$C0-$FF` RAM is read-only by bit 3.
- With the lockout set, the mapping is the 48K Apple ][+ layout and writes
  to the three registers do not change it.

**Indirect access extension**: given a zero page pointer offset and the
pointer's value plus Y, returns the memory cell and its `DebugAddress`, or
"ordinary access".

All of it is saved and loaded through `IMachineState`, and the RAM is
registered with the bus for history's page diffs.

## Devices

| Device | State |
|---|---|
| `Apple3SystemVias` | two `Via6522`; the last port outputs, for change detection |
| `Apple3Keyboard` | data latch, modifier flags, strobe, repeat timer, interrupt line |
| `Apple3CharacterSet` | glyph RAM; the loader's position and latch |
| `Apple3Video` | mode switches, display buffer, smooth scroll offset, interlace, beam position |
| `Apple3DiskController` | four drives (each the Disk II drive's state), selected drive, motor, Q6, Q7, data latch |
| `Apple3Sound` | speaker level, beeper phase and remaining time, DAC value |
| `Mm58167` | counters, latches, interrupt status and control, the offset from host time |
| `Acia6551` | existing |
| `Apple3AnalogInputs` | selected channel, conversion timer, button levels |
| `Apple3Slots` | selected slot for the shared ROM space, each slot's card |

Each implements `IMachineState` and `IDiagnosticsProvider`.

## Time

| Value | Meaning |
|---|---|
| Tick | one period of the 2 MHz clock; the unit of history's cycle numbers on this machine |
| CPU cycle cost | 1 tick, or 2 ticks at 1 MHz or while slowed |

## Debugger

### DebugAddress

| Field | Type | Notes |
|---|---|---|
| `address` | `Word` | |
| `space` | enum | `CurrentView`, `System`, `Bank`, `Rom` |
| `bank` | byte | meaningful when `space` is `Bank` |

- A `Word` converts to `CurrentView`.
- Two addresses are equal when all three fields are equal.
- On an Apple II machine every address is `CurrentView`; the existing aux
  and language card views are not changed by this feature.
- Text form: `contracts/debugger-addresses.md`.

### Tables that store a DebugAddress

Breakpoints, data breakpoints, watches, bookmarks, user symbols, data
blocks, heat ranges, the call stack's frames, trace records (program counter
and data access), event log rows, profile routines.

**Matching rule for stops**: an entry with `CurrentView` on an address in
the banked region matches every bank. An entry with `Bank n` matches when the
resolved address of the access is in bank `n`.

### Register breakpoints

| Kind | Condition |
|---|---|
| Bank changed | any change, or a change to a given bank |
| Environment changed | any change, or a given bit to a given level |
| Zero page changed | any change, or a change to a given page |

### Inline call (disassembly)

| Field | Notes |
|---|---|
| Opcode sequence | for SOS: `BRK` |
| Inline byte count | for SOS: 3 (call number, parameter list address) |
| Name lookup | call number to SOS call name |

### Debugger project

Stores every table above with its `DebugAddress`. A project file from before
this feature loads with every address as `CurrentView`.

## Business BASIC

| Entity | Fields |
|---|---|
| Release | signature, version text, the addresses of the interpreter's pointers |
| Line | number, tokens, text as `LIST` prints it |
| Variable | name, type (real, integer, long integer, string), value |
| Array | name, type, dimensions, values |
| Frame | GOSUB return or FOR loop, with its line |

## Desk scene

| Device kind | Model | Sub-meshes by color |
|---|---|---|
| `Apple3Body` | `Resources/Models/Apple3/` | case, drive door, latch, drive lamp, power lamp, badge, back panel connectors |
| `Monitor3` | `Resources/Models/Monitor3/` | case, glass, power lamp, brand stamp |
| `Disk3` | `Resources/Models/Disk3/` | case, door, latch, lamp |
| `ProFile` | from 048 | from 048 |

**Stack**: an ordered list from the desk up: body; ProFile when attached and
set to "on the computer"; monitor. Each item rests on the top surface of the
one below. Everything else is in the side row.

**ProFile placement** (user setting, saved): on the computer, to the side,
or hidden.
