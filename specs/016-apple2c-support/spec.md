# Feature Specification: Apple //c Machine Support

**Feature Branch**: `016-apple2c-support`

**Created**: 2026-07-08

**Status**: Draft

**Tracking**: #9 (65C02 core). Per-phase commits reference this issue (constitution Commit Discipline).

**Input**: User description: "Apple //c machine support: 65C02 CPU core first, then //c ROM and slotless firmware map, dual 6551 ACIA serial ports, built-in mouse, and IWM disk integration."

## Context: Existing Foundation *(informational)*

Casso already emulates the Apple //e substrate the //c is built on, and this work reuses it rather than rebuilding it:

- **CPU**: NMOS 6502 (`MemoryBusCpu` behind the `EmuCpu` strategy). No 65C02 yet.
- **Memory**: 128K aux RAM, 80STORE, language card, `Apple2eMmu`, `Apple2eSoftSwitchBank`, `CxxxRomRouter` (slot-ROM routing).
- **Video**: 40/80-column text, lo-res, hi-res, and **double hi-res** (`AppleDoubleHiResMode`, 560×192).
- **Disk**: Disk II controller + WOZ nibble engine (protection fidelity certified, #67).
- **Machines defined today**: Apple II, Apple II+, Apple //e, **Apple //e Enhanced** — the last of which is nominally 65C02-based but currently runs on the NMOS core (a latent defect this feature also corrects).
- **Composition**: JSON-driven device registration (`ComponentRegistry`), per-machine config assets.

This feature adds the pieces the //c has beyond the //e: the **65C02 CPU**, the **//c ROM + slotless "phantom slot" firmware map**, **two 6551 ACIA serial ports**, a **built-in mouse** with interrupts, and **IWM** disk integration.

## Clarifications

### Session 2026-07-08

- **Q: Target //c ROM revision?** → **Memory Expansion ROM (ROM 4)** — the most refined/compatible non-Plus //c firmware. v1 is scoped to a **5.25"/128K //c**: the ROM's UniDisk 3.5 (SmartPort) and memory-expansion firmware are *present* but those peripherals are **not implemented** in this spec (they report absent; the ROM still boots to BASIC/monitor). Requires 32K bank-switched-ROM handling. Compatibility risk vs. the original ROM is negligible — later //c ROMs are backward-compatible; titles that run *only* on the original ROM are rare copy-protection / serial-quirk edge cases.
- **Q: Who builds the shared 6551 ACIA, given spec 015 (printer) is in flight?** → **016 builds the complete dual-port ACIA (all registers, RX+TX, IRQ); spec 015 consumes it.**
- **Q: Serial host endpoints for v1?** → **Virtual only** — host-file output (printing) + loopback (comms). Real host COM / TCP-telnet endpoints deferred.
- **Q: 65C02 conformance verification?** → **Klaus Dormann's 65C02 extended-opcode functional test + Tom Harte's `wdc65c02` SingleStepTests** (reuse the existing CPU-test infrastructure used for the NMOS core).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Enhanced software runs on the 65C02 (Priority: P1)

A user runs software that requires the 65C02 — later ProDOS-era applications, MouseText utilities, and games written for the Enhanced //e or the //c. Today, on the existing "Apple //e Enhanced" machine, that software crashes or misbehaves because Casso executes an NMOS 6502. With a real 65C02 core, the software runs correctly.

**Why this priority**: It is the hard prerequisite for booting any //c ROM, and it *independently* fixes the already-shipped "Apple //e Enhanced" profile — so it delivers user-visible value before the //c machine even exists. It is also the smallest, most self-contained slice.

**Independent Test**: Select the existing **Apple //e Enhanced** machine, run a 65C02 instruction-conformance program (and a known Enhanced-//e title), and verify each 65C02 opcode produces the correct result and cycle count against a reference — with no //c work required.

**Acceptance Scenarios**:

1. **Given** the Apple //e Enhanced machine, **When** software executes a 65C02 opcode (`STZ`, `PHX`/`PLX`/`PHY`/`PLY`, `BRA`, `TSB`/`TRB`, `RMB`/`SMB`, `BBR`/`BBS`, `INC A`/`DEC A`, `(zp)` and `(abs,X)` JMP, etc.), **Then** it executes with the correct result and 65C02 cycle timing, not as an undefined NMOS opcode.
2. **Given** a program that relies on a documented 65C02-vs-NMOS behavioral difference (fixed `JMP ($xxFF)` page-boundary bug; `BRK`/decimal flag handling; extra cycle on decimal ops), **When** run on the 65C02 core, **Then** behavior matches the 65C02, not the NMOS part.
3. **Given** any non-enhanced machine (II, II+, unenhanced //e), **When** selected, **Then** it still executes on the NMOS 6502 core (unchanged), and the existing test suite stays green.

---

### User Story 2 - Boot an Apple //c to its firmware (Priority: P2)

A user selects **Apple //c** from the machine picker and it cold-boots to the //c firmware (monitor / Applesoft BASIC) with the built-in 80-column display — exactly like real hardware, with no expansion slots and peripherals answering at their fixed built-in addresses.

**Why this priority**: The first tangibly "//c" milestone — a bootable machine profile. Depends on P1 (a //c ROM will not run on an NMOS 6502).

**Independent Test**: Select Apple //c, cold boot, and verify it reaches the //c monitor prompt / Applesoft `]` with the //c ROM in place and 80-column firmware active, with built-in peripherals mapped to phantom-slot ROM space.

**Acceptance Scenarios**:

1. **Given** Apple //c is selected, **When** cold-booted, **Then** it reaches the //c monitor / Applesoft using the //c ROM, with 128K standard.
2. **Given** the //c has no card slots, **When** firmware probes slot ROM space, **Then** the built-in peripherals respond at their fixed phantom-slot addresses ($C1xx serial 1, $C2xx serial 2, $C3xx 80-column, $C4xx mouse, $C6xx disk).
3. **Given** a cold boot, **When** the //c firmware runs its startup, **Then** it reaches BASIC / monitor normally. *(The open-apple/closed-apple boot-time **self-test / diagnostics** entry is **deferred to a v1 follow-up** — see Out of scope.)*

---

### User Story 3 - Serial I/O through the built-in ports (Priority: P3)

A user runs software that uses the //c's two built-in serial ports — a terminal/comms program on port 2 (modem), or printing to a serial printer on port 1 — and data flows correctly through the 6551 ACIA and its firmware.

**Why this priority**: Serial is a defining //c capability. The 6551 ACIA is **shared with the in-flight spec 015 (printer support)** — building it here (or coordinating so it is built once) serves both efforts and avoids duplication.

**Independent Test**: Run a program that transmits and receives bytes through the serial firmware; verify the data path via the 6551 registers (data, status, command, control) and a loopback or mock endpoint.

**Acceptance Scenarios**:

1. **Given** a //c, **When** software configures and writes to serial port 1 or 2 through the ACIA + firmware, **Then** bytes are transmitted with the configured baud/framing and status reflects transmit/receive state.
2. **Given** incoming serial data, **When** the guest reads the ACIA, **Then** received bytes and status/IRQ flags are correct.

> **Coordination**: exactly one 6551 ACIA implementation must exist across this feature and spec 015. Neither spec builds its own ACIA.

---

### User Story 4 - Mouse-driven software (Priority: P4)

A user runs mouse-based //c software (MousePaint, mouse-aware AppleWorks) and the host pointer drives it through the //c's built-in mouse firmware and vertical-blanking interrupt.

**Why this priority**: Unlocks a distinct class of //c software. Builds on P2 (firmware map) and introduces the interrupt path.

**Independent Test**: Run the mouse firmware entry points; verify reported X/Y position, button state, and delivery of VBL / mouse interrupts as the host pointer moves and clicks.

**Acceptance Scenarios**:

1. **Given** a //c with mouse firmware active, **When** the host moves and clicks the pointer, **Then** the guest reads correct X/Y and button state via the mouse firmware.
2. **Given** interrupt-driven mouse mode, **When** a vertical blank (or mouse movement) occurs, **Then** the corresponding IRQ is delivered and the //c interrupt soft switches ($C019 VBL, mouse IRQ status) report correctly.

---

### User Story 5 - Internal and external disk via the IWM (Priority: P5)

A user boots and runs disk software from the //c's built-in 5.25" drive (and an attached external drive), which the //c services through the Integrated Woz Machine rather than a Disk II slot card.

**Why this priority**: Completes the //c as a self-contained machine. Lowest priority because it mostly reuses the existing Disk II / WOZ engine — the delta is the slotless IWM presentation.

**Independent Test**: Mount a WOZ/DSK image to the //c internal drive, cold boot, and verify RWTS reads through the IWM interface at slot-6 space; then verify an image mounted to the external drive.

**Acceptance Scenarios**:

1. **Given** a //c with a bootable disk in the internal drive, **When** cold-booted, **Then** it boots and reads/writes through the IWM, reusing the existing nibble/WOZ engine.
2. **Given** an external drive, **When** a disk is mounted there, **Then** the //c accesses it as the second drive.

---

### Edge Cases

- **NMOS-only illegal opcodes on the 65C02**: the 65C02 redefines the NMOS "illegal" opcodes (mostly as multi-byte NOPs). Software relying on NMOS illegal-opcode behavior must see 65C02 behavior on the enhanced/`//c` machines — and unchanged NMOS behavior on the non-enhanced machines. (Relates to #52.)
- **Documented 65C02 vs NMOS differences**: fixed indirect-JMP page-boundary bug, decimal-mode flag correctness + extra cycle, `BRK` behavior, read-modify-write cycle counts. Each must match the 65C02.
- **No slots**: firmware and software that scan slot ROM space must find the //c's built-in peripherals at fixed addresses; nothing may be "inserted" into a //c slot.
- **Mouse interrupt vs VBL timing**: interrupt frequency and acknowledgement must not starve or double-fire.
- **External drive addressing**: drive-2 / external-port selection and (out of scope) 3.5" drives.

## Requirements *(mandatory)*

### Functional Requirements

**65C02 CPU (P1)**
- **FR-001**: The system MUST provide a 65C02 CPU core implementing the full documented 65C02 instruction set — new opcodes (`STZ`, `PHX`/`PLX`/`PHY`/`PLY`, `BRA`, `TSB`/`TRB`, `INC A`/`DEC A`, `RMB`/`SMB`, `BBR`/`BBS`), new addressing modes (`(zp)`, `(abs,X)` for `JMP`), and 65C02 cycle timings.
- **FR-002**: The system MUST match documented 65C02-vs-NMOS behavioral differences (fixed indirect-`JMP` page-boundary bug; decimal-mode flag correctness and cycle penalty; RMW cycle counts).
- **FR-003**: The system MUST select the CPU variant per machine profile: **65C02** for Apple //e Enhanced and Apple //c; **NMOS 6502** for Apple II, II+, and unenhanced //e (unchanged).
- **FR-004**: Selecting a non-enhanced machine MUST leave its emulation behavior unchanged (no regressions in the existing suite).

**//c machine profile + firmware map (P2)**
- **FR-005**: The system MUST define an **Apple //c** machine profile (catalog entry, display name "Apple //c", disk-picker entry) composed of the 65C02, 128K RAM, the //e video/memory substrate, and the //c peripherals.
- **FR-006**: The system MUST load the **Apple //c Memory Expansion ROM (ROM 4)** as a managed asset with integrity verification, consistent with existing machine-ROM handling. This is a **32K bank-switched** ROM; the router MUST handle the //c's expanded-firmware bank switching ($C800 space).
- **FR-006a**: The ROM-4 firmware references UniDisk 3.5 (SmartPort) and the memory-expansion card, which are **out of scope for v1**: those peripherals MUST report absent (the ROM still boots to monitor / Applesoft). Adding them is a follow-up.
- **FR-007**: The system MUST present built-in peripherals at fixed **phantom-slot** ROM/I-O addresses (serial 1 -> slot 1, serial 2 -> slot 2, 80-column -> slot 3, mouse -> slot 4, disk -> slot 6) via the existing `CxxxRomRouter`, with **no** user-insertable expansion slots on this machine.
- **FR-008**: The //c MUST default to 128K and 80-column-capable video, cold-booting to the //c firmware (monitor / Applesoft).

**Serial ports (P3)**
- **FR-009**: The system MUST emulate a **6551 ACIA** (data, status, command, control registers; transmit/receive; baud/framing; IRQ) as a reusable device.
- **FR-010**: The //c MUST expose **two** serial ports (port 1 / printer at slot 1, port 2 / modem at slot 2) backed by the 6551 plus the //c serial firmware.
- **FR-011**: The 6551 ACIA MUST be implemented exactly once. **This feature (016) builds the complete dual-port ACIA (all registers, RX+TX, IRQ); spec 015 (printer support) consumes it.** Neither may duplicate it.
- **FR-011a**: For v1, the serial ports MUST support **virtual endpoints only** — host-file output (printing) and a loopback (comms) — with no dependency on real host serial devices. Real host COM / TCP-telnet endpoints are deferred.

**Mouse (P4)**
- **FR-012**: The system MUST emulate the //c built-in mouse: firmware entry points, X/Y position, button state, and mouse/VBL interrupt delivery.
- **FR-013**: The system MUST map the host pointer to the guest mouse and honor the //c interrupt soft switches (VBL at $C019, mouse IRQ status/enable).

**Disk (P5)**
- **FR-014**: The system MUST integrate disk access via an **IWM** interface presenting the existing Disk II / WOZ nibble engine as the //c's built-in internal drive (slot-6 space) plus an external drive port — without forking the nibble/WOZ engine.

**Cross-cutting**
- **FR-015**: All new components MUST be unit-tested under the existing `UnitTest/` project with mocked I/O per the constitution (no real files/registry/network/system APIs).
- **FR-016**: User preferences and machine selection MUST persist and restore the //c like any other machine.

### Key Entities

- **CPU strategy (65C02 / NMOS 6502)**: pluggable per machine profile; selects the instruction/timing behavior.
- **Apple //c machine profile**: the device-composition + ROM + display metadata that defines the machine.
- **Phantom-slot firmware map**: the fixed built-in-peripheral address layout replacing card slots.
- **6551 ACIA**: serial-port device (shared with spec 015).
- **AppleMouse**: mouse firmware + interrupt source.
- **IWM**: disk interface presenting the existing WOZ engine as built-in drives.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The 65C02 conformance suite — **Klaus Dormann's 65C02 extended-opcode functional test + Tom Harte's `wdc65c02` SingleStepTests** — passes 100% on the enhanced/`//c` machines; the NMOS conformance behavior is unchanged on non-enhanced machines.
- **SC-002**: The Apple //c cold-boots to its monitor / Applesoft within the //e startup budget — **no more than 10% over the //e cold-boot wall-clock time on the same host** (measured, not subjective).
- **SC-003**: At least three representative //c titles run correctly end-to-end: one serial/terminal (or serial-print) task, one mouse application (e.g., MousePaint), and one disk-based game — each booting and operating from the //c profile.
- **SC-004**: The full existing unit-test suite (including CPU conformance for NMOS, and the Apple II/II+///e machines) stays green — zero regressions.
- **SC-005**: Exactly one 6551 ACIA implementation exists in the tree, consumed by both this feature and spec 015 (verified by inspection — no duplicate ACIA).

## Assumptions

- The existing //e substrate (128K aux, 80STORE, language card, `Apple2eMmu`, soft switches, 40/80-column, double hi-res, Disk II/WOZ) is reused unchanged; this feature adds only the //c-specific delta.
- The 65C02 target is the standard/WDC 65C02 with Rockwell bit ops (`RMB`/`SMB`/`BBR`/`BBS`) as used by the //c ROM — **not** the 65C816.
- The //c target ROM is the **Memory Expansion ROM (ROM 4)** — the most refined/compatible non-Plus //c firmware. Later //c ROMs are backward-compatible, so this does not lock out original-ROM software.
- The serial-**printing driver** (routing guest output to a host printer/file) is spec 015's scope; this feature provides the 6551 **hardware** (and builds the ACIA that 015 consumes).
- The disk baseline is the 5.25" drive via the IWM (internal + external port).
- **Delivery order**: although the 65C02 (US1) is the //c's natural first story, the **6551 ACIA device (US3)** is built first — it is a shared component that blocks the in-flight **spec 015** (printer support). The ACIA device is independent of the CPU and the machine (it is tested standalone via file + loopback); only its //c-specific serial *wiring* waits for the //c bring-up. See `tasks.md` Phase 2.
- **Out of scope (this spec)**: the Apple //c Plus (4 MHz accelerator); *implementing* the UniDisk 3.5" drive and the memory-expansion card (their ROM-4 firmware is present but reports absent — deferred to follow-ups); the **open-apple/closed-apple boot-time self-test / diagnostics** entry (deferred to a v1 follow-up); real host serial endpoints (COM/TCP); and non-QWERTY keyboard-layout switches.
