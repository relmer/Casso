# Contract: `TYP`, `SAV` and `DSK`

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

What the three directives mean once this feature lands. Behavior here is
Merlin's, quoted in [research.md](../research.md) where the manual settles it and
flagged as a synthesis where it does not.

This extends [019's directive contract](../../019-assembler-dialects/contracts/merlin-directives.md),
whose refusal table lists `TYP` and `SAV`. Those two rows go away.

## `TYP` — the object's filesystem type

`TYP <expr>` sets the ProDOS file type the object is filed under. It is a
ProDOS-era directive; Merlin 8's DOS 3.3 manual has no entry for it, which is
consistent, since DOS 3.3 has no ProDOS types to set.

**Recognized types**, matching what the tool already publishes for
`disk put --type`:

| Value | ProDOS | DOS 3.3 | Result |
|---|---|---|---|
| `$04` | TXT | `$00` T | Filed as text. |
| `$06` | BIN | `$04` B | Filed as binary. **The default when nothing states a type.** |
| `$FC` | BAS | `$02` A | Filed as Applesoft. |
| `$FF` | SYS | *none* | **On ProDOS: filed as a system file. On DOS 3.3: refused.** |
| anything else | — | — | **Refused, naming the value.** |

**Why `SYS` is refused rather than approximated (FR-010).** The ProDOS kernel
boots by scanning the volume directory for a `SYS`-typed entry — the tree's own
`ProDosVolume::SetStartupProgram` puts it bluntly, "the ORDERING IS THE
MECHANISM" — and DOS 3.3 has no system-program concept at all. There is no
DOS 3.3 type that means what `SYS` means, so any mapping would be a guess filed
under a type the source did not ask for, surfacing much later as a program that
will not run. The refusal names both the type and the filesystem.

**Not the GS/OS range.** Merlin 32 documents a `TYP` taking `$B2`–`$BD` or a
three-letter alias, but that is the OMF range for 65816 IIgs program files, out
of scope on the same boundary that refuses a second `XC`. See research finding
4.

**Last one wins**, as `DSK` already does, and `--type` beats all of them.

## `SAV` — write an output and carry on

`SAV <name>` writes everything accumulated **since the previous save** — or
since the start of the assembly, for the first — and empties the accumulation.
Assembly continues.

This is Merlin's, not ours. From Bredon's manual: a save "can be done several
times during assembly", and "after a save, the MERLIN object area is 'empty'".

### What each file gets

| | |
|---|---|
| **Bytes** | The span since the last save. Never cumulative; a byte appears in exactly one file. |
| **Name** | `SAV`'s operand, unless `--as` overrode it. |
| **Type** | The `TYP` in effect, unless `--type` overrode it; binary otherwise. |
| **Load address** | The address this span's first byte assembles to. |

The load address rule is the one place the manual stops short. It states the
run-on case — "the first file will have an origin of the initial ORG command,
the second will have the last address of the first+1" — and says nothing about
an `ORG` landing between two saves. "The address of this span's first byte"
reduces to exactly the manual's rule when nothing moves the program counter, and
gives the sensible answer when something does. Recorded as a synthesis, not as
quoted behavior.

### With no image target

Writes a host file (FR-020). `SAV` is not refused for want of a disk, which is
what lets its boundary row be deleted outright rather than reworded into a
conditional refusal.

### Integrity

An assembly that fails **after** a save has been composed must leave the target
exactly as it was (FR-014) — the image byte-for-byte, and no host file from this
assembly left behind. Every output buffers until the whole assembly succeeds.

## `DSK` — name the output, and close the previous one

`DSK <name>` names the object. With `--disk`, that is a name on the volume; with
no image target it names a host file, as today (FR-008).

**A second `DSK` closes the first output and begins another** (FR-025). The
manual: "IF DSK is already in effect, the old file will be closed and a new one
begun." The tree currently keeps only the last name — indistinguishable from
Merlin for one occurrence, wrong for two.

**Merlin's streaming is not adopted.** `DSK` in Merlin means "assemble the
following code directly to disk", for objects too large to hold in memory, and
the manual says `SAV` is preferred and 30% faster for anything moderate. Casso
assembles into a `std::vector<Byte>` with no such ceiling, so streaming buys
nothing — and it would defeat FR-014 outright, since a directive that writes as
it goes cannot promise a failed assembly leaves the image untouched. What is
adopted is where the file boundaries fall.

## The boundary table

`CassoCore/MerlinSubsetBoundary.cpp` is the single authority every refusal and
published list is composed from, so this is a change to that table and not a
special case elsewhere (FR-013).

| Before (6 rows) | After (4 rows) |
|---|---|
| `REL`, `ENT`, `EXT` — need the linker (GH #112) | unchanged |
| `XC` (second) — needs a 65816 core | unchanged |
| `TYP` — "owned by another feature" | **row deleted** |
| `SAV` — "needs its own decision" | **row deleted** |

`DSK` is not in this table and never was: it is accepted today and appears in
the *supported* directive table in `docs/merlin-subset.md`. So the count goes
six to **four**, and `DSK`'s improvement is a directive gaining its real meaning
rather than a refusal being lifted.

**Deleting a row is not the whole change.** `AssemblySession` routes a directive
that matches a boundary row to `HandleSubsetBoundary`, which claims the line.
Remove the row and the directive falls through to the ordinary dispatch table,
where it needs a handler or it becomes an unknown directive. `Directive::FileType`
and `Directive::SaveObject` already exist and are already spelled in
`MerlinDialect.cpp`; the handling behind them is what is missing. `SAV`'s must
run in **pass 2**, unlike `DSK`'s, because a save point is a span of emitted
bytes.

`MerlinSubsetBoundary::GetHelpText` needs no edit — it composes from the rows.
`docs/merlin-subset.md`'s prose does: it says "Six constructs are recognized and
refused by name."

## Test coverage note

`UnitTest/Fixtures/Merlin/CLOCK.S` is the only committed period source with two
`SAV` lines, and they are **mutually exclusive** — `DO HOURS-12 / ELSE / FIN`,
inside an outer `DO SAVOBJ`. Exactly one can execute per assembly, so the vendor
corpus cannot cover multi-save behavior. A fixture has to be authored for it,
and an authored fixture is a weaker authority than a vendor source. Say so where
it is used rather than letting a hand-written file read as period evidence.
