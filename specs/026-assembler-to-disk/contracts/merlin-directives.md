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

### With no image target

**Refused**, naming the flag that supplies one (FR-041), which is what the
`--type` flag does in the same situation (FR-040).

`DSK` and `SAV` degrade to host meanings because they have one — a name and a
write. `TYP` has none: a host file has no filesystem type. So the reason the
boundary table gave for refusing `TYP` in the first place, that it "means
nothing without a filesystem that has types", still holds exactly when no image
is targeted. Accepting it there would be accepting a directive precisely where
its own stated precondition is absent.

The remedy is a flag rather than a source edit, so this does not fail a
developer for source they did not write, and it regresses nothing — `TYP` does
not assemble today under any invocation.

**What decides is whether the construct has anything to mean without a volume**,
not whether it arrived as a flag or as a directive. That is why `DSK` and `SAV`
go one way and `TYP` goes the other.

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

## How a save point gets its name

`DSK` and `SAV` name in **opposite directions**, and both manual quotes say so
plainly:

| Directive | Manual | Names |
|---|---|---|
| `DSK` | "assemble the **following** code directly to disk" | the span that **starts** here |
| `SAV` | "save the **current** object code under the specified name" | the span that **ends** here |

So a span's name may be fixed before its first byte exists or after its last
one. Both are ordinary; neither is an error.

**Where both name the same span**, `SAV` wins. It is the later statement and the
one made with the bytes in hand, and it is the directive whose whole purpose is
to write this output — `DSK` merely said where the following code was headed.

**A span nothing named** takes the command-line name, then the default
(`<source>.bin`). This is the ordinary single-output assembly: no directive
names anything and the object is `<source>.bin`.

**Bytes emitted after the last save** are a span like any other and are written
under the rule above, rather than discarded. Real Merlin leaves such bytes
unsaved in the object area, but silently dropping assembled bytes is the failure
mode this tree has been bitten by five times, and a trailing span is
indistinguishable from a whole ordinary assembly when nothing above it saved.

## Naming collisions

Two rules, and they are not the same rule.

**A command-line name plus several outputs is refused** (FR-026). `--as` and
`-o` supply one name. Applying it to each output in turn means each replaces the
last, and the tool reports success having written one file where the source
asked for three. Refuse, naming the flag and the count.

This can only be known once the assembly has run, since nothing before pass 2
knows how many outputs there are. That is fine: it is a post-assembly refusal,
the image is untouched, and the status is the same 2 every other failure to
produce output earns.

**A command-line TYPE is not limited this way.** One type applies to every
output without ambiguity, so `--type` with several saves is ordinary.

**Two outputs of one assembly under one name is refused** (FR-027), naming the
file. This is deliberately NOT the same as FR-019, which replaces a file left by
an earlier run:

| | |
|---|---|
| Name already on the volume from an **earlier run** | **Replace.** A build loop reassembles constantly; refusing would fail every build after the first. |
| Two save points in **one assembly** under one name | **Refuse.** The source just asked for two files and one of them would be thrown away. |

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
