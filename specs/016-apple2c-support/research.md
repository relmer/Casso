# Phase 0 Research: Apple //c Support

Design decisions and their rationale. All spec `[NEEDS CLARIFICATION]` markers were resolved in the spec's Clarifications section (2026-07-08); this records the technical decisions behind the plan.

## D1 — 65C02 as an injected `ICpu` strategy (not a mode flag)

**Decision**: Implement `Cpu65C02` as a distinct `ICpu` that shares the `Cpu6502` dispatch structure, selected via `EmuCpu`'s existing strategy-injection constructor.

**Rationale**: `EmuCpu` already owns a `unique_ptr<ICpu>` and exposes `EmuCpu(MemoryBus&, unique_ptr<ICpu>)`; the header even anticipates "future CPU strategies (65C02, …)". A separate core keeps NMOS behavior untouched (zero regression risk) and avoids threading a variant flag through the hot dispatch path.

**Alternatives rejected**: a runtime `bool isCmos` on the NMOS core (branches in the hot path, muddies NMOS correctness); a full rewrite (unjustified — most opcodes are shared).

## D2 — CPU selection via the existing `MachineConfig.cpu` field

**Decision**: Extend `MachineConfig` to accept `cpu == "65C02"` and have `MachineManager` inject the matching strategy per config (default `6502`). Set `Apple2eEnhanced` and `Apple2c` configs to `65C02`.

**Rationale**: The `cpu` field is already parsed and (today) hard-validated to `"6502"` (`MachineConfig.cpp:726`); `MachineManager.cpp:726` currently always builds the default NMOS `EmuCpu`. Wiring selection here is config-driven, requires no new machinery, and **fixes the existing //e-Enhanced defect** (it runs on NMOS today) as a side effect — that is the shippable P1 value.

## D3 — Target ROM 4 (Memory Expansion), 32K bank-switched, 5.25"/128K v1

**Decision**: Target the //c Memory Expansion ROM (ROM 4). Implement the 32K expansion-ROM bank switch ($C800). The ROM's UniDisk 3.5 (SmartPort) and memory-expansion firmware is present but those peripherals report absent in v1.

**Rationale**: ROM 4 is the most refined/compatible non-Plus //c firmware; later //c ROMs are backward-compatible, so this locks out no meaningful software. The only added cost vs. the flat 16K original is bank-switching plus graceful "peripheral absent" handling — both bounded. Deferring 3.5/mem-expansion keeps v1 scoped without a compatibility penalty for 5.25 software.

## D4 — 6551 ACIA owned by 016, consumed by spec 015

**Decision**: This feature builds the complete dual-port `Acia6551` (all registers, RX+TX, IRQ); spec 015 (printer support) consumes it.

**Rationale**: The //c needs the full ACIA; a printer-only build would be a subset. Building the complete device once here (FR-011) avoids duplication and a later rework of a partial implementation.

## D5 — Serial endpoints: file + loopback (v1)

**Decision**: v1 serial endpoints are host-file TX (printing) and a loopback (comms); real host COM / TCP-telnet endpoints are deferred.

**Rationale**: Deterministic and fully unit-testable per the constitution's test-isolation rule (no real device). Real endpoints are additive and out of this spec's scope.

## D6 — IWM delegates to the existing WOZ engine

**Decision**: The `Iwm` device presents the existing Disk II / WOZ nibble engine as the //c's slotless internal + external drives; it does not fork the engine.

**Rationale**: The nibble/WOZ engine (with certified copy-protection fidelity, #67) is register-compatible with the IWM at the software level. Reuse maximizes compatibility and minimizes new code; the delta is the slotless presentation + built-in mounting.

## D7 — 65C02 verification via existing Dormann + Harte infrastructure

**Decision**: Verify with Klaus Dormann's 65C02 extended-opcode functional test + Tom Harte's `synertek65c02` SingleStepTests, run through the same harness as the NMOS corpus.

**Rationale**: Reuses proven CPU-test infrastructure; the two corpuses together cover functional correctness and per-opcode cycle accuracy. Test vectors are data, not a third-party code dependency.

**65C02 tier — why `synertek65c02`, not `wdc65c02`**: The Apple //c and //e-Enhanced are the **base CMOS tier**. Apple sourced 65C02s from a vendor mix (GTE most common, plus WDC and Rockwell parts) whose common instruction set omits both the Rockwell bit ops (`RMB`/`SMB`/`BBR`/`BBS`, the `$x7`/`$xF` columns) and WDC's `WAI`/`STP` (`$CB`/`$DB`); Apple firmware never uses them, and AppleWin models them as NOPs. Casso's `Cpu65C02` therefore installs neither — those opcodes decode as single-byte NOPs — so the matching Harte corpus is `synertek65c02` (base), not `rockwell65c02` or `wdc65c02`. The Dormann 65C02 test must likewise be built/run with its Rockwell/WDC opcode options disabled. The backing operations/modes remain in the CassoCore CMOS superset, inert, so a future Rockwell/WDC variant is cheap (see issue #86 for the //e-Enhanced consumer).

> **Correction (2026-07-08, supersedes the paragraph above)**: the shipped core models the **Rockwell R65C02** — RMB/SMB/BBR/BBS are real instructions (ROM 4 firmware and Enhanced-//e firmware use them; commit 1610cb63) and WAI/STP stay NOPs. Conformance target is therefore the `rockwell65c02` Harte tier (regeneration = T041; assembler BBR/BBS zp-rel emission = T040).
