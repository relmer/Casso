# Quickstart / Validation: Apple //c Support

How to validate each user story independently. Build + test with the standard tasks (`MSBuild Casso.sln /t:Casso;UnitTest /p:Configuration=Debug /p:Platform=x64`, then `vstest.console UnitTest.dll`).

## P1 — 65C02 core (ships standalone)

1. Set `Apple2eEnhanced` config `cpu: 65C02`.
2. Run `Cpu65C02Tests` — Klaus Dormann 65C02 functional + Tom Harte `wdc65c02` SingleStepTests. **Expect 100% pass.**
3. Regression: run the full CPU suite — NMOS (Dormann/Harte NMOS) unchanged; II/II+/unenhanced //e green.
4. Manual: boot the **Apple //e Enhanced** machine and run a title that uses 65C02 opcodes (previously misbehaved) — verify correct execution.

## P2 — Boot the //c

1. Select **Apple //c** in the machine picker; cold boot.
2. **Expect** the //c monitor / Applesoft `]` prompt with 80-column firmware and 128K.
3. `Apple2cBootTests`: assert boot signature + that built-in peripherals answer at phantom-slot addresses ($C1xx/$C2xx/$C3xx/$C4xx/$C6xx); no user-insertable slots.

## P3 — Serial (6551)

1. `Acia6551Tests`: configure baud/framing, transmit bytes → assert they land in the host-file endpoint; feed loopback RX → assert read + status/IRQ flags.
2. Manual: run a //c serial/terminal or a print task; verify output via the file endpoint.
3. Confirm spec 015 references the same `Acia6551` (no duplicate ACIA in the tree — SC-005).

## P4 — Mouse

1. `AppleMouseTests`: drive host X/Y/button; assert firmware-reported position/button; assert VBL/mouse IRQ delivery + soft-switch status ($C019).
2. Manual: run MousePaint (or a mouse-aware app); verify pointer control.

## P5 — IWM disk

1. Mount a WOZ/DSK to the //c internal drive; cold boot; **expect** boot + RWTS reads via the IWM (slot-6 space) reusing the WOZ engine.
2. Mount to the external drive; verify second-drive access.
3. Regression: existing Disk II machines unaffected.

## Definition of done (per Success Criteria)

- **SC-001**: 65C02 conformance 100% on enhanced/`//c`; NMOS unchanged.
- **SC-002**: //c cold-boots within the //e startup budget.
- **SC-003**: three representative titles (serial/terminal, MousePaint, a disk game) run end-to-end on the //c.
- **SC-004**: full existing suite green — zero regressions.
- **SC-005**: exactly one `Acia6551` in the tree, consumed by 016 + 015.
