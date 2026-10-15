# Data Model: Super Serial Card

## SscSwitches

The fourteen DIP switch positions. `true` is ON (closed).

| Field | Meaning |
|---|---|
| `sw1[1..7]` | SW1-1 to SW1-4 baud code (OFF = 1, SW1-1 the high bit); SW1-5, SW1-6 mode; SW1-7 DCD source |
| `sw2[1..7]` | meanings per mode, R-002 and the reference card; SW2-6 gates the IRQ |

Derived:

- `GetDipSwitch1Byte()`, `GetDipSwitch2Byte (bool ctsTrue)`: the register bytes of R-001, with undriven bits left to the caller.
- `GetMode()`: Communications, Printer, SicP8, SicP8A from SW1-5 and SW1-6.
- `GetBaudCode()`: 0-15 from SW1-1 to SW1-4.
- `IsIrqConnected()`: SW2-6 ON.

Default (FR-010): SW1 = OFF OFF OFF ON ON ON ON, SW2 = ON ON OFF ON OFF OFF OFF (communications, 9,600 baud, 8 data bits, 1 stop bit, no parity, no line feed, interrupts off, normal CTS).

Persistence, inside the slot entry: `"switches": { "sw1": "off off off on on on on", "sw2": "on on off on off off off" }`. Missing or malformed values fall back to the default per bank.

## SscSettingsModel

The Settings page's view of `SscSwitches`: a list of settings for the current mode, each with its choices and the switches it writes.

| Setting | Modes | Switches |
|---|---|---|
| Mode | all | SW1-5, SW1-6 |
| Baud rate | all | SW1-1 to SW1-4 |
| Data format | communications: data bits and stop bits; printer: stop bits | SW2-1, SW2-2 / SW2-1 |
| Parity | communications | SW2-3, SW2-4 |
| Delay after carriage return | printer | SW2-2 |
| Line width and video | printer | SW2-3, SW2-4 |
| Line feed after carriage return | all | SW2-5 |
| Interrupts | all | SW2-6 |
| Clear to send | all | SW1-7, SW2-7 |

The SIC P8 and P8A emulation modes list the printer-mode settings. A setting whose choice the current switches do not produce exactly (for example the undefined all-ON baud code) is listed as "Custom", and the readout lists the positions.

## Acia6551 state

| Field | Meaning |
|---|---|
| `status`, `command`, `control` | the 6551 registers |
| `rdr` | receive data register |
| `tdr`, `tdrFull` | transmit data register |
| `tsr`, `tsrBusy`, `tsrCyclesLeft` | transmit shift register and time remaining (fixed point) |
| `rxQueue` | bytes queued by the attachment, thread-safe |
| `rxCyclesLeft` | time until the next queued byte lands (fixed point) |
| `cts`, `dsr`, `dcd` | modem inputs from the attachment, true = asserted |
| `irqConnected` | whether the IRQ output is connected to the interrupt controller |
| `cpuClockHz`, `crystalHz` | timing bases; crystal 1,843,200 |

Transitions: RESB (power-on, Ctrl-Reset) clears command and control, empties both transmit registers, clears RDRF and the error and IRQ bits, and sets TDRE; `rxQueue` is not emptied. A write to the status register (the datasheet's program reset) clears command bits 0-4 and does not alter control or command bits 5-7, and it clears the error bits and RDRF. Detaching the attachment empties `rxQueue`.

## SuperSerialCard

| Field | Meaning |
|---|---|
| `slot` | 1-7 |
| `acia` | an `Acia6551` at `$C088 + slot*16` |
| `switches` | `SscSwitches` |
| `firmware` | 2,048 bytes, or empty |
| `firmwareStatus` | Present, Missing, WrongSize; with the path searched |
| `expansionSelected` | the expansion-ROM latch |

## Slot entry (machine config JSON)

```json
{
    "slot": 2,
    "device": "super-serial-card",
    "firmware": "SSC.rom",
    "enabled": false,
    "switches": { "sw1": "off off off on on on on", "sw2": "on on off on off off off" },
    "ports": [ { "device": "serial-file-out", "path": "C:\\Users\\me\\serial.txt" } ]
}
```

`ports[0].device` is one of `""` (nothing), `serial-loopback`, `serial-file-out`, `serial-file-in`; spec 033 adds its own values. `path` applies to the two file attachments. `PortConfig` gains the optional `path`.
