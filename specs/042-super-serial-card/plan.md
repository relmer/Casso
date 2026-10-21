# Implementation Plan: Super Serial Card

**Branch**: `042-super-serial-card` | **Date**: 2026-10-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/042-super-serial-card/spec.md`

## Summary

Emulate Apple's Super Serial Card in slot 2 of the ][, ][+ and //e. The card is a new core device, `SuperSerialCard`, that owns an `Acia6551`, the DIP switch state and the user-supplied 2 KB firmware. Slot-ROM selection by `$Cs00` and deselection by `$CFxx` become a general mechanism (`SlotRomPage`, `ExpansionRomBus`) that the //e's `CxxxRomRouter` and the ][/][+ memory bus both use, so the card's expansion ROM follows Apple's protocol on every machine. The 6551 gains the timing it lacks: a double-buffered transmitter that takes one character time per byte and a thread-safe receive queue delivered one character time apart, both measured in CPU cycles, which is what spec 033 FR-095 depends on. Settings gets a Serial card page for the switches and the connector attachment; a missing firmware is reported by an infobar with a Download button.

Hardware facts and the decisions taken from them are in [research.md](research.md).

## Technical Context

**Language/Version**: C++ (MSVC v145, C++ latest as configured in the solution)

**Primary Dependencies**: none new. Dxui for the Settings page and the infobar; WinHTTP through the existing `AssetBootstrap` download path.

**Storage**: machine config JSON (slot entry keys `firmware`, `switches`, `ports[].path`); the firmware file under `Devices/SuperSerialCard/`.

**Testing**: Microsoft Native CppUnitTest in `UnitTest` (`EmuTests` for the chip, card, routing and config; `UiTests` for the Settings model). Whole-machine tests through `TestMachine` / `GuestSession` with a synthetic firmware image.

**Target Platform**: Windows x64 and ARM64 (ARM64 build-only).

**Project Type**: desktop emulator: core static libraries plus a thin GUI shell.

**Performance Goals**: no measurable cost per instruction when no 6551 is busy (one branch per chip); pacing exact to the CPU cycle over 64 KB at 115,200 baud.

**Constraints**: clean room (FR-018); core-only logic (Principle VI); EHM and the style rules; no file I/O in unit tests (file attachments take bytes and streams through seams).

**Scale/Scope**: one card type, the ACIA rework (shared with the //c), slot-ROM selection for all slot machines, one Settings page, one infobar, one config migration.

## Constitution Check

| Principle | Status |
|---|---|
| I. Code Quality | Pass. New classes follow the one-class-per-pair rule; the switch table and baud table are file-scope `static constexpr` tables. |
| II. Testing Discipline | Pass. Chip, card, routing, config and Settings model are unit-tested with synthetic data; no test reads the real firmware or a real file. Each new test is checked against a mutation of what it covers. |
| III. UX Consistency | Pass. The page follows the existing Settings pages, sentence case throughout; the infobar reuses `DxuiActionBanner`. |
| IV. Performance | Pass. The tick is a single test when idle; no allocation on the emulation thread except when the receive queue grows. |
| V. Simplicity | Pass. The expansion-ROM mechanism is shared by both machine families rather than duplicated per card. |
| VI. Thin Executable, Testable Core | Pass. Everything lands in `CassoEmuCore`; the exe is untouched. |

Re-checked after Phase 1 design: no violations, so the complexity table is omitted.

## Project Structure

### Documentation (this feature)

```text
specs/042-super-serial-card/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── serial-attachment.md
└── tasks.md
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Devices/
│   ├── Acia6551.h/.cpp              # timing, receive queue, modem lines, IRQ gate
│   ├── IAciaEndpoint.h              # modem-control and break callbacks
│   ├── AciaEndpoints.h/.cpp         # loopback rewired as a plug; file out
│   ├── AciaByteSourceEndpoint.h/.cpp# file in (bytes supplied by the caller)
│   └── SerialByteQueue.h/.cpp       # thread-safe receive queue
├── Machines/Apple2/Common/
│   ├── SuperSerialCard.h/.cpp       # the card
│   ├── SscSwitches.h/.cpp           # switch state, register bytes, JSON text form
│   ├── IExpansionRomCard.h          # card side of $Cs00 / $C800 selection
│   ├── SlotRomPage.h/.cpp           # $Cs00-$CsFF device forwarding to a card
│   ├── ExpansionRomBus.h/.cpp       # $C800-$CFFF arbitration and $CFxx deselect
│   └── CxxxRomRouter.h/.cpp         # //e: delegate expansion space to ExpansionRomBus
├── Core/
│   ├── MachineConfig.h/.cpp         # SlotConfig::firmware, switches; PortConfig::path
│   ├── MachineConfigUpgrade.h/.cpp  # slot-2 SSC migration
│   └── ComponentRegistry.cpp        # "super-serial-card"
├── Shell/
│   ├── MachineBuilder.cpp           # card wiring, firmware load, attachments
│   ├── MachineHost.h/.cpp           # tick every 6551
│   └── Components/ShellSerialCard.h/.cpp  # firmware infobar
├── Ui/Settings/
│   ├── SscSettingsModel.h/.cpp      # page model: settings per mode, readout
│   ├── SerialCardPage.h/.cpp        # the page
│   └── SettingsPanelState / SettingsSheet  # page visibility, slot-entry staging
├── AssetBootstrap.h/.cpp            # catalog row, on-demand optional download
Resources/Machines/{Apple2,Apple2Plus,Apple2e,Apple2eEnhanced}/*.json

UnitTest/EmuTests/
├── Acia6551Tests.cpp                # extended: timing, queue, modem lines
├── SuperSerialCardTests.cpp
├── ExpansionRomBusTests.cpp
├── SscSwitchesTests.cpp
└── SuperSerialCardMachineTests.cpp  # //e and ][+ through TestMachine
UnitTest/UiTests/SscSettingsModelTests.cpp
```

**Structure Decision**: The card lives beside the other slot cards in `Machines/Apple2/Common`; the chip and its attachments stay in `Devices` beside `Acia6551`. Slot-ROM selection is factored into its own classes because both machine families use it and the //e's router already contains the switch logic it depends on.

## Design notes

1. **ACIA timing.** Character times are computed in fixed point (cycles x 2^16) from the CPU clock in the machine config and the 1.8432 MHz crystal, so no rounding accumulates. `Tick` advances the shift register and the receive delivery; it is called from `MachineHost` per instruction, beside the disk controller and Mockingboard, for every 6551 the builder created (a list on `MachineHost`'s refs).
2. **Thread safety.** `SerialByteQueue` is a mutex-guarded deque with an atomic count, so the emulation thread's idle test is one atomic load. Modem inputs arrive through atomics and are applied at the next tick.
3. **Routing.** On the //e, `MachineBuilder` registers the card's `SlotRomPage` with `CxxxRomRouter::SetSlotIoDevice`, and passes the router the machine's `ExpansionRomBus`, which the router calls for `$C800`-`$CFFF` only under R-004's conditions. On the ][/][+, both are ordinary bus devices; the `$C800` bus device covers `$C800`-`$CFFF`, below which nothing else on those machines maps.
4. **Firmware.** The loader resolves `firmware` through the same search paths as `rom` and stores path and status without failing; the builder reads the file and validates its size. The infobar reads the status from the built machine.
5. **Settings.** The page edits a staged copy of the slot entry's `switches` and `ports`. Switch changes apply live to the running card (they are register reads, R-001); an attachment change rebuilds only the attachment; enabling or disabling the card resets the machine, as for any card.
6. **//c.** Its two 6551s get the same clock and tick, so serial printing from the //c is now paced at the baud rate its firmware sets.
