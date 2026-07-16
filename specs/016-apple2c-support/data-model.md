# Phase 1 Data Model: Apple //c Support

Components / entities introduced or extended. "Entities" here are emulated hardware devices and configuration records, not persisted data models.

## CPU strategy

| Entity | Kind | Key state | Interface | Notes |
|---|---|---|---|---|
| `Cpu65C02` | `ICpu` (new) | registers A/X/Y/SP/PC/P, cycle count, memory[] | `ICpu` (Step, Reset, …); shares `Cpu6502` dispatch | 65C02 opcode table + addressing modes + timing |
| `MachineConfig.cpu` | config field (extend) | `"6502"` \| `"65C02"` | parsed in `MachineConfig` | selects the injected strategy |
| `EmuCpu` | strategy holder (modify) | `unique_ptr<ICpu>` | forwards to strategy | inject per `config.cpu` in `MachineManager` |

## Machine profile

| Entity | Kind | Composition |
|---|---|---|
| `Apple2c` profile | `Resources/Machines/Apple2c/Apple2c.json` (new) | `cpu: 65C02`; 128K; `apple2e-keyboard`, `apple2e-softswitches`, MMU, `language-card`; `acia-6551` ×2; `apple-mouse`; `iwm`; phantom-slot ROM map |
| //c ROM asset | AssetBootstrap entry (new) | ROM 4 (32K), SHA-verified; display name "Apple //c" |
| Phantom-slot map | `CxxxRomRouter::SetSlotRom` config | slot1→serial1, slot2→serial2, slot3→80col, slot4→mouse, slot6→disk |

## Peripheral devices

| Entity | Kind | Key state / registers | Interrupts | Endpoint |
|---|---|---|---|---|
| `Acia6551` | `MemoryDevice` (new; registered `acia-6551`) | data, status, command, control; RX/TX shift; baud/framing | TX-empty / RX-full IRQ via `InterruptController` | TX→host file; loopback (v1) |
| `AppleMouse` | `MemoryDevice` (new; `apple-mouse`) | X, Y, button, mode; clamp bounds | VBL + mouse-move IRQ sources | host pointer → guest |
| `Iwm` | `MemoryDevice` (new; `iwm`) | motor, phase, Q6/Q7, drive-select | — | delegates to Disk II/WOZ nibble engine |

## Soft switches (//c-specific)

| Switch | Address | Behavior |
|---|---|---|
| VBL read | $C019 | vertical-blank status; drives the mouse/VBL IRQ |
| Mouse IRQ enable/status | //c mouse switch range | enable + acknowledge mouse/VBL interrupts |
| IOUDIS / DHIRES | $C07E/$C07F etc. | reuse existing //e handling |

## Relationships

- `MachineManager` builds `EmuCpu` with the `Cpu65C02` or `MemoryBusCpu` strategy per `MachineConfig.cpu`.
- The `Apple2c` JSON composes all devices through `ComponentRegistry` factories.
- `Acia6551`, `AppleMouse`, and `Iwm` raise IRQs through the shared `InterruptController` (per-source lines).
- `Iwm` composes (does not subclass) the existing WOZ nibble engine.
- `Acia6551` is the single ACIA implementation consumed by both this feature and spec 015.
