# Contract: The Dialect Profile Seam

**Feature**: `019-assembler-dialects`

This is the contract `023-ca65-dialect` consumes. Its SC-006 requires that adding
ca65 change nothing in the mechanism, so what follows is deliberately written as
obligations on the *mechanism*, not on Merlin.

## What a profile supplies

A profile is **mostly data**. The declarative surface covers comment introducers
and their column rules, the label rule, the field model, the local-label and
variable sigils, the directive spelling table, the string-encoding table, and
where the CPU target comes from.

**As built the seam has one virtual**: parse a line into a `ParsedLine`.
Everything downstream consumes that without knowing which profile produced it,
which is precisely what keeps the engine shared.

Earlier drafts of this contract listed four virtuals — field segmentation, local
labels, variable symbols, macro parameters. Extracting the AS65 grammar showed
those are *internal* to a profile that needs them rather than obligations on
every profile, and AS65 needs none of them. They are not missing; they were never
required.

**Virtuals are added when a dialect proves it needs one.** Merlin will likely add
some. That is the narrow-seam decision working: the alternative was making every
future dialect implement behavior it may not have.

What that does *not* license is widening the seam to avoid a hard problem. The
test is whether the behavior is genuinely per-dialect syntax. If a proposed
virtual would let a profile reach into how the assembly *runs* rather than how
source is *read*, the seam is being used to smuggle engine changes, and that is
the thing to raise.

## What the mechanism guarantees

1. **The engine is shared and untouched.** The two-pass driver, the expression
   evaluator, and the opcode tables are not parameterized by dialect and must not
   become so. A profile that needs engine changes has found a real gap; it is a
   spec amendment, not a local edit.

2. **The token vocabulary is shared.** `Directive` is one enum across all
   dialects. A profile maps its own spellings onto it. A new token is added only
   for an operation the assembler cannot already perform — never merely because a
   dialect spells something differently.

3. **Selection is uniform.** The dialect is carried in `AssemblerOptions`, so
   every entry point selects it identically. Command-line parsers only populate
   that field; they never branch on dialect themselves.

4. **The CPU axis is independent.** A profile never implies a CPU, and the
   mechanism never assumes a profile has only one available. *Where* a profile
   takes its CPU from is part of the profile — command line, in-source directive,
   or both.

5. **Strictness is per profile.** No lenient union across dialects. For a profile
   modeling an assembler Casso already implements, "authentic" means what Casso
   accepts today: the rule forbids admitting another dialect's constructs, and
   does not license tightening against source that currently assembles.

6. **The subset boundary is data.** Constructs a profile refuses live in one
   table with a reason class, exposed by an enumerating accessor, with help text
   generated from it.

## Verification

SC-009 is proved by a **synthetic, test-only third profile** in the unit tests.
It exists to fail if the mechanism is secretly built for exactly two dialects — a
condition every Merlin test would pass while 023 discovers it the expensive way.

The synthetic profile must be addable without editing the engine, the evaluator,
or the opcode tables. If adding it requires touching any of those three, SC-009
has failed regardless of how the Merlin suite looks.

## Notes for `023-ca65-dialect`

Known ca65 pressure points on this seam, recorded now so 023 can judge fit early
rather than after implementing:

- ca65's `::` scoping and `.proc`/`.scope` are symbol-table behavior, not line
  syntax. They will press on the engine, not on this seam — which is where the
  boundary between the two gets tested for real.
- ca65's bare `:` unnamed labels are a third meaning for a character that already
  means two things. The field model and label rule should absorb it; if they
  cannot, that is the seam's first genuine defect.
- ca65's `.import`/`.export` belong on the subset-boundary table with reason
  `NeedsLinker`, the same row shape Merlin's relocatable constructs use.
