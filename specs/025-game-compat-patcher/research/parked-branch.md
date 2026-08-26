# The parked `game-patch-table` proof-of-concept

## What exists

Branch **`game-patch-table`**, single commit **`a67c4922`**
(`feat(patch): add GamePatcher pattern table with Apple //c VBL-spin shim`,
2026-07-15). NEVER merged to master. Read it with:

```
git show game-patch-table:CassoEmuCore/Core/GamePatcher.h
git show game-patch-table:CassoEmuCore/Core/GamePatcher.cpp
git show a67c4922 --stat
```

Files it adds / touches:
- `CassoEmuCore/Core/GamePatcher.{h,cpp}` (new, ~66 + ~147 lines)
- `CassoEmuCore/CassoEmuCore.vcxproj` (registers the new files)
- `Casso/EmulatorShell.{h,cpp}` (per-frame scan wire-up)
- `Casso/Shell/MachineManager.cpp` (installs rules per machine, clears on switch)

## The PoC design (as-built)

`GamePatcher` is a `Rule` table scanned over live guest RAM through the
`MemoryBus`:

- `struct Rule { std::vector<Byte> signature; size_t patchOffset;
  std::vector<Byte> replacement; const char* label; }`
- `Scan(bus, lo, hi)` applies every rule whose signature matches and is not
  already patched; returns count newly patched. Idempotent (a patched site
  stops matching).
- `ScanRam(bus)` scans `$0400-$BFF9` (skips zero page / stack / IO), called once
  per frame by the shell.
- Writes go through the bus, so the MMU page map is honored (patches land in the
  bank the guest runs from, not the flat CPU backing store).
- Built-in `AddApple2cVblSpin()`: signature `AD 19 C0 30 FB`
  (`LDA $C019 / BMI *-3`), NOPs the branch. Installed for the //c only.

This already satisfies several of the spec's functional requirements in
skeleton form (FR-001..FR-006). It is explicitly a proof of concept: per-title
gating, user-facing disclosure (FR-009), the enable/disable setting (FR-010),
and provenance/attribution (FR-015) are all absent.

## Reconciliation hazard (this is the main planning risk)

The branch predates a substantial August-2026 refactor of the shell. Notably,
commit `74d6e22d` ("name the video modes and the //e devices instead of
re-deriving them") reworked `EmulatorShell` and `MachineManager`: video modes
and the //e keyboard / soft-switch bank are now named pointers in a
`MachineRefs` struct rather than positional lookups / `dynamic_cast`s. The
PoC's wire-up in those two files will not apply cleanly.

Planning decision (NOT settled by the spec): **rebase the PoC and fix up the
wire-up, or re-author `GamePatcher` fresh against current master and keep only
the PoC's design.** The core `GamePatcher.{h,cpp}` is largely self-contained and
likely rebases cleanly; the shell/MachineManager hunks are the ones that will
conflict. Given the file is small and the design is sound, re-authoring the
wire-up against the named-refs shell may be faster than resolving conflicts.

## What the spec adds on top of the PoC

- Per-machine AND per-signature gating as a first-class rule property (D3).
- The Choplifter rule (see `choplifter-diagnosis.md`) — a second real rule,
  proving the table is a table.
- Default-on, disclosed, defeasible behavior (D1/D2, US2/US3).
- Provenance/attribution on every rule (D4/FR-015).
- The self-checksum-timing validation (see `antim-reference.md` hazard).
- Tests reachable from `UnitTest` per the constitution's Testable-Core principle
  (signature match, wildcard if added, idempotency, bus-honoring/bank-correct
  writes, rule non-interference across titles/machines).
