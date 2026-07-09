# //c Mouse Hardware Contract (US4)

Derived by disassembling the //c ROM 4 mouse firmware (`UnitTest/Fixtures/Apple2c.rom`,
32K = 2×16K banks mapped to `$C000-$FFFF`, toggled by `$C028`). The firmware is the
authoritative oracle: implement the hardware below and the real ROM firmware must drive
the mouse correctly in the headless harness. Approach chosen: **full IOU hardware model**
(run the real ROM firmware; emulate the hardware it touches) — not firmware-entry HLE.

## Where the code lives

- Slot-4 phantom page `$C400-$C4FF` (bank 0): Pascal-1.1 signature + entry dispatch;
  bodies bank-switch into **bank 1** via `$C700` trampolines (each a `STA $C028`).
- Real mouse routines: **bank 1** (`INITMOUSE`/enable at `$C100`, ack+ENVBL at `$C18E`,
  interrupt-enable sweep + button at `$D630`).
- IRQ handler: vector `$FFFE` → `$C803` (bank 0). Reads `$C066`/`$C067` at entry
  (`LDX $C066` / `LDY $C067` at `$C80C`/`$C80F`), saves soft-switch state, dispatches to
  the bank-1 servicer. Source discrimination: stacked-P B-flag (BRK), `$C019` (VBL),
  `$C066`/`$C067` (mouse move), ACIA status (serial).

## Soft-switch map (firmware-proven)

| Addr | Access | Meaning | Firmware evidence |
|------|--------|---------|-------------------|
| `$C019` | R bit7 | VBL (RDVBLBAR) — already in Casso via `VideoTiming::IsInVblank` | `C106 LDA $C019`, `C448/D4B0 BIT $C019` |
| `$C048` | W | **RSTXY** — clear/ack mouse X0/Y0 interrupt latch | `C18E STA $C048` |
| `$C058-$C05F` | W | mouse/VBL interrupt enables + edge selects, programmed as a bank; gated by IOU access | `D646 STA $C058,X` sweep |
| `$C05A` / `$C05B` | W | **DISVBL** / **ENVBL** (VBL interrupt disable/enable) | `C115 STA $C05A`, `C19B STA $C05B` |
| `$C063` | R bit7 | **mouse button, active-low** (0 = pressed) | `C124/D689 BIT $C063` → `ORA #$80` when N clear |
| `$C066` | R bit7 | mouse **X0** movement+direction (game-port PADDL2 repurposed) | `C80C LDX $C066` (IRQ entry) |
| `$C067` | R bit7 | mouse **Y0** movement+direction (game-port PADDL3 repurposed) | `C80F LDY $C067` (IRQ entry) |
| `$C078` / `$C079` | W | IOU register access **disable/enable** — brackets every `$C05x` mouse program | `C10B/C198 STA $C079` … `C11A/C19E STA $C078` |
| `$C070` | R | PTRIG (paddle/mouse timer trigger; shared game port) | `C907 LDA $C070` |

Game-port sharing (FR-013a): mouse and joystick share the DB-9 port. `$C064-$C067` are
PADDL0-3; in mouse mode `$C066`/`$C067` carry X0/Y0 quadrature, `$C063` the button.

## Interrupt model

Two IOU interrupt sources drive the CPU IRQ line (via `InterruptController`, level-sensitive):

1. **VBL** — asserted at vertical-blank onset when enabled (`$C05B` ENVBL; `$C05A` DISVBL).
   60 Hz. Ack: cleared by `$C048` write (and/or VBL-period end — validate vs firmware).
2. **Mouse movement (X0/Y0)** — asserted on an X0 or Y0 transition (per the selected edge)
   when enabled (the `$C058-$C05F` sweep). Level-held until `$C048` write clears the latch.
   Getting the latch/ack right is the T026 "neither starve nor double-fire" edge case:
   assert once per movement, hold until ack, do not re-assert until the next movement.

Position model: the firmware counts X0/Y0 transitions in the IRQ handler, reading the
direction bit from `$C066`/`$C067` bit 7, and maintains position in screen holes clamped to
the CLAMPMOUSE bounds (default 0..1023). The **device** owns the hardware X0/Y0 lines +
button + interrupt latches; the **firmware** owns position accumulation and clamping.

## Implementation plan (grounds T026-T031)

- **T027 `AppleMouse` device** (`CassoEmuCore/Devices/AppleMouse.{h,cpp}`, a `MemoryDevice`
  like `Acia6551`): owns host X/Y deltas → X0/Y0 quadrature line state + transition latches,
  button state, and the VBL + mouse-move IRQ sources (`IInterruptController`). Serves
  `$C066`/`$C067` (bit7 = X0/Y0), `$C063` (bit7 = button, active-low), and consumes `$C048`
  (ack), `$C058-$C05F` (enable/edge), `$C05A`/`$C05B` (VBL en), `$C078`/`$C079` (IOU gate).
- **T028** wire these into the //c soft-switch path (`Apple2eSoftSwitchBank`/the //c bank),
  alongside the existing `$C019` VBL read.
- **T026** tests drive the **real firmware entry points** (SETMOUSE/READMOUSE/SERVEMOUSE)
  in the headless harness: inject host motion+click, assert firmware-reported X/Y/button and
  that VBL/mouse IRQs are delivered and `$C019`/latch status read correctly — the firmware
  validates the hardware model, so exact bit/edge/ack semantics are pinned empirically.
- **T029** register `apple-mouse` in `ComponentRegistry`; add to `Apple2c.json` (slot-4).
- **T030 / T030a** (shell): host pointer → guest (non-capturing, absolute over the viewport);
  `InputMappingMode::Mouse` + skeuomorphic device selector (needs joystick/paddle/mouse icon
  assets).

## Open items to pin during TDD (via the firmware oracle)

- Exact `$C058-$C05F` pair semantics (which pair = X0 enable vs edge vs Y0) — decode the
  `STA $C058,X` sweep mask the firmware builds.
- VBL IRQ ack path (`$C048` vs `$C019`-read vs auto-clear at VBL end).
- `$C066`/`$C067` full bit layout beyond bit 7 (any movement-pending bit).
