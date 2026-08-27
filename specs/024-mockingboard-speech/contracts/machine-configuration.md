# Contract: Machine configuration and presentation

**Feature**: `024-mockingboard-speech` | **Issue**: #123

How a machine says which Mockingboard it has, and how that reaches the user.

---

## Registered device types

Two type names in the component registry, per D1:

| Type name | Card | Registered as |
|---|---|---|
| The existing name | Mockingboard A (sound only) | Unchanged — **must keep its current spelling** |
| A new name | Mockingboard C (sound + speech) | Added |

**The existing name keeps its spelling.** Renaming it would invalidate every
machine profile on disk and every user override already persisted, for no gain.
The sound-only card is what that name has always meant and continues to mean.

### Compatibility rules

- **C1** — A profile naming the existing type gets the sound-only card, exactly
  as today. No profile on disk changes meaning.
- **C2** — An unrecognized device type is handled as it is today; this feature
  introduces no new failure mode for stale or hand-edited configuration.
- **C3** — Both types accept any slot, like every other card in the registry
  (each card already derives its base from `config.slot`). Nothing here presumes
  slot 4.

---

## Machine profile defaults

FR-008. The three profiles that ship a Mockingboard move to the sound+speech
type; the profile that ships none is untouched.

| Profile | Before | After |
|---|---|---|
| Apple ][ | *(no Mockingboard)* | *(unchanged — no Mockingboard)* |
| Apple ][+ | sound-only, slot 4 | **sound+speech**, slot 4 |
| Apple //e | sound-only, slot 4 | **sound+speech**, slot 4 |
| Apple //e Enhanced | sound-only, slot 4 | **sound+speech**, slot 4 |
| Apple //c | *(no slots)* | *(unchanged)* |

The slot does not move. Only the model changes.

---

## User-facing presentation

| Guarantee | |
|---|---|
| **P1** | The Hardware tab's slot entry names the model — "Mockingboard A" / "Mockingboard C", or equivalent naming that distinguishes them — rather than a raw type string or a chip part number. *(FR-013)* |
| **P2** | Changing the installed model reports that a machine reset is required, through the same mechanism as existing hardware-configuration changes. *(FR-012)* |
| **P3** | A chosen model persists across sessions and survives a machine switch and back. *(FR-011, SC-007)* |
| **P4** | Release notes state that the emulated Mockingboard is now the sound+speech model by default, and how to get the sound-only card. *(FR-022)* |

**On naming (P1)**: users see product names, not part numbers. "Mockingboard C"
is what someone bought; the chip inside it is an implementation detail of the
product, and surfacing it would ask the user to know more than they should need
to. The spec records this as an assumption; it is restated here because it is a
contract with the user, not an internal preference.

---

## Relationship to GH #124

Per-slot card configuration will present these two types as ordinary entries in a
slot dropdown, which is why D1 chose two type names over a type-plus-variant
tuple — no special-case UI is required for the Mockingboard.

**This feature does not depend on #124.** Selection here is by machine
configuration. Whatever selection surface #124 builds consumes the same two
registered types and needs no change to this contract.
