# Contract: ISoftSwitchBank (layered)

Layered soft-switch surface. Each layer registers its own handlers; resolution
is by address ownership at machine-build time, not by per-access fallthrough.

## Layers

1. **`AppleSoftSwitchBank`** — ][ base. Owns $C030 (speaker), keyboard
   address $C000/$C010, $C050-$C057 display switches.
2. **(implicit ][+ additions)** — composed via additional registrations.
3. **`AppleIIeSoftSwitchBank`** — //e additions:
   - **Write switches** $C000-$C00F: 80STORE/!80STORE, RAMRD/!RAMRD,
     RAMWRT/!RAMWRT, INTCXROM/!INTCXROM, ALTZP/!ALTZP, SLOTC3ROM/!SLOTC3ROM,
     80COL/!80COL, ALTCHARSET/!ALTCHARSET. Forwards to `AppleIIeMmu`
     (and own state for 80COL/ALTCHARSET).
   - **Status reads** $C011-$C01F: bit 7 from owning subsystem (LC for
     BSRBANK2 $C011, BSRREADRAM $C012; MMU for $C013-$C018; video timing
     for RDVBLBAR $C019; bank for RDTEXT $C01A, RDMIXED $C01B, RDPAGE2
     $C01C, RDHIRES $C01D, RDALTCHAR $C01E, RD80VID $C01F). Bits 0-6
     from keyboard latch (floating-bus stand-in).
   - **Display switches** $C050-$C05F.

## Semantics

- $C011-$C01F reads MUST NOT clear the keyboard strobe (FR-014).
- Every write to a //e write switch triggers
  `AppleIIeMmu::OnSoftSwitchChanged()` (eventually) so display-driven
  routing is re-resolved without per-access cost.
- Layering is **composition**: ][ and ][+ machine configs do not register
  the //e bank; their behavior is unchanged.

## Test obligations

`SoftSwitchTests` (modified):
- Full $C000-$C00F write coverage.
- Full $C011-$C01F read coverage with bit-7 source verification.
- Floating-bus low-7 from keyboard latch.
- Strobe-clear isolation: $C011-$C01F does NOT touch strobe.
- Backwards-compat: ][/][+ banks do NOT respond to //e-only addresses.
