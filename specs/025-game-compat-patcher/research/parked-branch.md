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

## RECOMMENDATION: build on it — reuse the core, re-author the wire-up

Reviewed the actual PoC source (2026-08-26). **Take `GamePatcher.{h,cpp}` as the
starting point rather than re-authoring from scratch**, for two reasons that are
about correctness, not effort:

1. **Writes go through the `MemoryBus`.** This is the bank-correctness lesson —
   the flat CPU backing store is the wrong buffer the moment the MMU repoints a
   page to aux. (Same class of bug as the double-hi-res renderer reading main
   memory through the banked bus, fixed in 1.18.1.) A rewrite could plausibly
   reach for `PokeByte` and silently miss.
2. **Idempotency is by construction**, not bookkeeping: a patched site no longer
   matches its own signature, so re-scanning is a comparison and nothing else.
   There is no "already applied" set to get wrong or to invalidate on reset.

It is also proven in the app (5 VBL sites defused in Karateka, 6 unit tests),
and `Scan` already iterates in `uint32_t` so `at++` cannot wrap a 16-bit
address. ~150 lines. Rewriting buys nothing and risks both lessons above.

**Do NOT reuse the `EmulatorShell` / `MachineManager` hunks** — see the
reconciliation hazard below. Suggested mechanics:

```
git checkout game-patch-table -- CassoEmuCore/Core/GamePatcher.h \
                                 CassoEmuCore/Core/GamePatcher.cpp
```
...then evolve the core, and re-author the ~20 lines of wire-up by hand against
the current named-refs shell.

## Gap analysis: PoC as-built vs this spec

Additive, except the last one:

| Gap in the PoC | Spec ref | Shape of work |
|---|---|---|
| `Rule` carries no machine applicability (gating is external — `MachineManager` decides whether to install) | D3 / FR-007 | add an applicability field to `Rule` |
| No provenance/attribution; `label` is an identifier, not a human reason + citation | D4 / FR-015 | add description + attribution fields |
| No wildcard/don't-care byte in signatures (anti-m's `compare.a` uses `$97`; the wider catalog will likely need it) | FR-012 | small change in `Matches` |
| No enable/disable setting; no user-facing disclosure | D1 / D2 / FR-009 / FR-010 | shell + settings work |
| **Scan cost** — see below | **FR-016** | **needs a measured strategy** |

### The one real design problem: scan cost

`Scan` calls `bus.ReadByte` per address per rule across `$0400-$BFF9`
(~48,118 addresses), **every frame**, unconditionally:

- 1 rule  @ 60 fps ≈ **2.9M** bus reads/sec
- 2 rules @ 60 fps ≈ **5.8M** bus reads/sec
- ~25 rules (the anti-m catalog ambition) ≈ **72M** bus reads/sec

Acceptable for a single PoC rule; likely not acceptable for the feature, and
squarely in tension with FR-016 ("MUST NOT measurably degrade emulation
performance"). This should be measured early and addressed. Options to weigh:

- Cheap first-byte reject before entering `Matches` (already implicit, but the
  per-address `ReadByte` call itself is the cost — consider reading through the
  bus page pointers directly for RAM rather than a call per byte).
- Scan less often than every frame, or on a trigger (e.g. after disk activity /
  a write into a candidate range) rather than unconditionally.
- Retire a rule once applied, if the title cannot legitimately re-load over it —
  but note FR-004 requires re-application when the guest DOES overwrite, so any
  retirement must be conditional, not permanent.

Note also `Matches` reads via `bus.ReadByte`; the `$0400-$BFF9` window
deliberately excludes `$C0xx` so scanning cannot toggle a soft switch. Preserve
that property in any optimization.

## Reconciliation hazard (this is the main planning risk)

The branch predates a substantial August-2026 refactor of the shell. Notably,
commit `74d6e22d` ("name the video modes and the //e devices instead of
re-deriving them") reworked `EmulatorShell` and `MachineManager`: video modes
and the //e keyboard / soft-switch bank are now named pointers in a
`MachineRefs` struct rather than positional lookups / `dynamic_cast`s. The
PoC's wire-up in those two files will not apply cleanly.

Resolved above: **take the core file, re-author the wire-up.** The core
`GamePatcher.{h,cpp}` is self-contained (it includes only `Pch.h` and
`Core/MemoryBus.h`) and should apply cleanly; the shell/`MachineManager` hunks
are the ones that will conflict, and re-doing ~20 lines of wiring against
`MachineRefs` is faster and cleaner than resolving them.

For scale: the PoC is only ~6 weeks older than the refactor it conflicts with
(`a67c4922` 2026-07-15 vs `74d6e22d` 2026-08-25) — this is a narrow, recent
conflict, not months of rot.

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
