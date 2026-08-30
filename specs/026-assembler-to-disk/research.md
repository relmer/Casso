# Phase 0 Research: Assembler-to-Disk Output

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

This resolves the unknowns the plan's Technical Context raised. Every finding
below is either read off this tree or quoted from a primary source; where
neither settles a point, it says so rather than picking quietly.

## 1. Merlin's `SAV` saves a span, not the whole object

**Decision**: A save point holds the bytes emitted since the previous save, or
since the start of the assembly for the first. After a save, the accumulation is
empty and assembly continues into a fresh one.

**Rationale**: This is not a design choice. Glen Bredon's manual for Merlin
states it directly:

> "SAV will save the current object code under the specified name. This acts
> exactly as does the MERLIN EXEC mode object saving command, except it can be
> done several times during assembly. After a save, the MERLIN object area is
> 'empty' and the object address is set to the last specification of OBJ, or if
> it is not present, MERLIN HIMEM by default."

"After a save, the MERLIN object area is 'empty'" is the whole answer. Several
saves in one assembly are explicitly supported, and each one carries only what
accumulated since the last.

**How this was settled**: The question was raised because the drafted FR-012
said "the object accumulated so far", which reads as cumulative, and because
this tree documents `ORG` as relocating without splitting the output stream, so
the object is one contiguous blob and the two readings genuinely differ. The
in-tree Merlin corpus could not decide it: `CLOCK.S` is the only committed
source with two `SAV` lines, and they are mutually exclusive, guarded by
`DO HOURS-12 / ELSE / FIN` inside an outer `DO SAVOBJ`. Exactly one can execute
per assembly, so the corpus never exercises the difference. The manual did.

**Alternatives considered**:

- *Cumulative, each save writing the whole object.* Rejected: contradicted by
  the manual. It was the reading the draft's wording implied, and it would have
  put a loader inside the main program's file.
- *Delta only where an origin intervened.* Rejected: two rules where the source
  has one, and nothing in Merlin suggests it.

## 2. The load address a save point records

**Decision**: Each save point records the address its own first byte assembles
to. With no intervening `ORG` this is the previous save's last address plus one;
where the source states a new origin, that origin governs.

**Rationale**: The manual gives the no-intervening-origin case exactly:

> "The SAV command sets the address of the saved file to the 'correct' value.
> For example, the first file will have an origin of the initial ORG command,
> the second will have the last address of the first+1, and the third will have
> the address of the second+1,... When BLOADed later, they will go to the
> correct location(s)."

The manual describes only the run-on case and does not say what an `ORG` between
two saves does. **That gap was closed by measurement rather than reasoning** —
see finding 2a. Merlin puts the second file at the stated origin.

`AssemblySession` already computes the value this needs.
`AssemblyResult::startAddress` is set from the first `ORG` and later overwritten
with the lowest address actually used (`AssemblySession.cpp:3109`, `:8011`),
which is this derivation applied to a whole assembly. A save point needs it
applied to a span.

## 2a. Measured against Merlin Pro 2.23: delta saves, and the origin governs

**Both open questions above were settled by running real Merlin under Casso**,
rather than by reading the manual or reasoning from it. Source assembled:

```
* ORG BETWEEN TWO SAVES
 ORG $300
 LDA #$11
 RTS
 SAV SPAN1A
 ORG $6000
 LDA #$22
 RTS
 SAV SPAN1B
```

Merlin's own listing reported `Object saved as SPAN1B,A$6000,L$0003` and
`--End assembly, 6 bytes, Errors: 0`. Reading the two objects back off the disk
and decoding their DOS 3.3 binary headers:

| File | Load | Length | Bytes |
|---|---|---|---|
| `SPAN1A` | `$0300` | `$0003` | `A9 11 60` |
| `SPAN1B` | `$6000` | `$0003` | `A9 22 60` |

**`SAV` is delta, confirmed.** Each file holds three bytes — only its own span.
A cumulative implementation would have made `SPAN1B` six bytes containing
`SPAN1A`'s as well, and the whole assembly reported six bytes across two files
rather than six in the second.

**A stated origin governs the saved address, confirmed.** `SPAN1B` loads at
`$6000`, the `ORG` in effect. Had the addresses kept running consecutively from
the previous save, as the manual's own example implies for the run-on case, it
would have been `$0303`. FR-024's rule was recorded as a synthesis because the
manual is silent here; it is now measured behavior.

### `DSK` cuts spans with no `SAV` present — confirmed

```
* TWO DSK, NO SAV
 DSK SPAN2A
 ORG $300
 LDA #$11
 RTS
 DSK SPAN2B
 LDA #$22
 RTS
```

| File | Load | Length | Bytes |
|---|---|---|---|
| `SPAN2A` | `$0300` | `$0003` | `A9 11 60` |
| `SPAN2B` | `$0303` | `$0003` | `A9 22 60` |

**Two files, from two `DSK` directives, with no `SAV` anywhere** — FR-025 and
FR-043 confirmed. Each holds only its own span, so `DSK` cuts exactly as `SAV`
does. Merlin printed no "Object saved as" line for either, consistent with `DSK`
streaming rather than saving a buffer.

`SPAN2B` at `$0303` is the previous save's last address plus one, which is the
manual's run-on rule. Taken with `SPAN1B` at `$6000`, both halves of FR-024 are
now measured: addresses run on when nothing moves the program counter, and a
stated origin governs when one does.

### Bytes after the last save are DROPPED — and this contradicted the spec

```
* BYTES AFTER THE LAST SAV
 ORG $300
 LDA #$11
 RTS
 SAV SPAN3A
 LDA #$22
 RTS
```

Merlin reported `Object saved as SPAN3A,A$0300,L$0003`, then listed
`0303: A9 22` and `0305: 60` and ended with `6 bytes, Errors: 0`. **Only
`SPAN3A` reached the disk.** The trailing two instructions were assembled,
counted in the total, and written nowhere.

**The spec said the opposite**, on the reasoning that silently dropping
assembled bytes is the failure mode this tree has been bitten by five times.
That reasoning is sound and the conclusion was still wrong, because it is not
this tool's decision to make: SC-003 promises the same files a period assembler
would have produced, and writing a file Merlin does not write breaks it.

**Resolved by separating the two concerns.** Casso drops the span, matching
Merlin exactly, AND warns that bytes were assembled after the last save and not
written. Fidelity is preserved in the files; the degraded-state doctrine is
satisfied by the diagnostic rather than by inventing an output. A build that
wants the warning fatal already has the flag for it.

**The rule that fits all three measurements**, and preserves Casso's own CLI
behavior: a span is written when a directive named it, or when it is the only
span and takes the command-line or default name. `SPAN2`'s trailing span was
written because the `DSK` still named it; `SPAN3`'s was not because nothing did;
and an ordinary assembly with no directives at all is one span under the
command-line name, which is what `CassoCli merlin` already does and FR-016
protects.

### The last three, measured: one confirmed, two wrong

These were implemented on reasoning and then run. **Two of the three were
wrong**, which is the second time in this feature that a rule reasoned out from
the manual did not survive contact.

**A bare `SAV` — confirmed.** Merlin does not fall back to any other name. It
prints `Object saved as ,A$0300,L$0003` — the name empty — and the operating
system then answers `SYNTAX ERROR`. So it is an error and there is no fallback,
which is what FR-042 says. The difference is only that Casso raises it at the
line missing the name rather than letting DOS raise it later and less clearly.

**Two saves under one name — WRONG, it is not an error.**

```
* TWO SAVES, ONE NAME
 ORG $300
 LDA #$11
 SAV SAME
 LDA #$22
 SAV SAME
```

Merlin reported `Object saved as SAME,A$0302,L$0002` and `--End assembly, 4
bytes, Errors: 0`. Reading the disk back leaves exactly one `SAME`, holding the
SECOND save's bytes: the first was overwritten with no complaint. FR-027 refused
this outright, which would produce no files where Merlin produces one. Corrected
to the shape the trailing-span rule already uses — the files match Merlin, and a
warning carries what was lost.

**A `DSK` name persisting past a `SAV` — WRONG, the combination is refused.**

```
* DSK, THEN SAV, THEN MORE
 DSK OUTER
 ORG $300
 LDA #$11
 RTS
 SAV INNER
```

Merlin answered **`Bad "SAV" in line: 6`**. The disk afterwards holds `OUTER`
(`$0300`, 3 bytes, the code above the save) and no `INNER` at all.

The two directives are **mutually exclusive output mechanisms**: `DSK` streams
the following code straight to disk, `SAV` writes the object held in memory, and
Merlin will not have both in play. FR-044 had invented a rule for combining
them — the name staying in effect past a save and governing the next span —
and the contract even flagged those rows as "Casso's composition rather than
recovered behavior". They were composition of a case that does not exist.

**The lesson, since it now has three instances.** Every rule in this feature
reasoned out rather than measured has had roughly even odds: the trailing span
was wrong, the file type's scope was wrong, two of these three were wrong. What
the manual states plainly has held every time. What it is silent about should be
run, not derived.

### The procedure, since 019 asked for exactly this

019's research stopped at "it needs someone who can drive Merlin interactively
and report the exact working sequence", blocked on leaving the editor's **Add
mode**. That blocker is gone, and not by solving it: with `disk put` the source
never goes through the editor at all.

1. Copy the vendor disk. Never assemble against `UnitTest/Fixtures/Disks/`.
2. `CassoCli disk put work.dsk src.txt --as T.NAME --type T --text`. This
   produces exactly the on-disk form Merlin's own sources use — verified byte
   for byte against `T.PI.MACS`: high-ASCII throughout, `$8D` terminators,
   `$A0`-padded 30-byte catalog name, type `$00`.
3. Launch Casso on the work copy and wait for the `%` prompt. **Do not send
   keys during boot**; they are silently lost.
4. `R` for Read text file, then the name **without** its `T.` prefix — Merlin
   prepends it. `SPAN1` reaches `T.SPAN1`.
5. `ASM`, then `N` to "Update source (Y/N)?".
6. Close Casso to flush, then read the objects with `disk get` and decode the
   DOS 3.3 four-byte header for the load address.

Three traps cost most of the time spent finding this, all of them in the
driving rather than in Casso:

- **`SendCassoKeys.ps1` needs `-DelayMs 250`.** Its own documentation warns that
  the emulated keyboard has a single-byte latch, and the 60 ms default silently
  drops characters: `T.PI.MACS` arrived as `T.P.MAC`. Every `FILE NOT FOUND` in
  this session was a mangled name, not a missing file.
- **The filename prompt is pre-filled with the previous name**, and typing
  overwrites from the cursor rather than replacing. A shorter name leaves the
  old tail behind. `Ctrl-X` (`$18`) cancels the line, exactly as Apple's GETLN
  does.
- **"Read text file" appends to the editor buffer rather than replacing it.**
  Reading a second source without clearing splices the two. Rebooting between
  captures is the cheap way to guarantee an empty buffer.

`disk put` was suspected twice during this and exonerated twice: the guest's own
`CATALOG` lists the written files with a matching free-sector count, the raw
catalog bytes match vendor entries exactly, and DOS's own
`PRINT CHR$(4)"VERIFY T.SPAN1"` returns no error.

## 3. `DSK` is a streaming multi-output directive, and the tree implements it as a name

**Decision**: A second `DSK` closes the output the first named and begins
another, matching the manual. Recorded as FR-025.

**Rationale**: The manual again:

> "DSK instructs the assembler to assemble the following code directly to disk.
> IF DSK is already in effect, the old file will be closed and a new one begun.
> DSK is used primarily for extremely large files. For moderately sized
> programs, SAV is preferred since it is 30% faster and theoretically more
> reliable."

`AssemblySession::HandlePass1ObjectFile` currently documents the opposite: "A
later directive replaces an earlier one, the way a later origin does. The name
in effect is the last one the source stated." For **one** `DSK` the two are
indistinguishable, which is why this has never mattered. For two they differ:
Merlin writes two files, the tree writes one.

**The streaming half is deliberately not adopted.** Merlin's reason for `DSK`
was an object too large to hold in memory, and it is the slower path by its own
manual. Casso assembles into a `std::vector<Byte>` with no such ceiling, so
streaming buys nothing, and it would defeat FR-014: a directive that writes as
it goes cannot promise that a failed assembly leaves the image untouched. What
is adopted is the observable behavior, which is where the file boundaries fall.

**Alternatives considered**:

- *Leave `DSK` as last-one-wins.* Rejected: it is wrong against the authority
  this feature is measured by, and once save points exist a second `DSK`
  closing the current one is nearly free.

## 4. `TYP`'s operand is a ProDOS type byte, and the 16-bit range does not apply

**Decision**: `TYP` takes an 8-bit ProDOS file type, written as Merlin writes
numbers (`TYP $06`). The recognized set is the one the tool already publishes:
`$04` text, `$06` binary, `$FC` Applesoft, `$FF` system. Anything else is
refused naming the byte.

**Rationale**: `TYP` is absent from the Merlin 8 manual, which is consistent,
since DOS 3.3 has no ProDOS types to set. The Merlin Pro manual lists it as
"TYP (ProDOS only)", though the archived OCR is truncated at the entry itself.
Merlin 32 documents a `TYP` taking `$B2`-`$BD` or a three-letter alias, but that
is the **GS/OS OMF** range for 65816 IIgs program files, and is out of scope
while Casso declares itself 6502/65C02, the same boundary that refuses a second
`XC`. The 8-bit ProDOS types are the right set and are already in the tree as
`ProDosVolume::kTypeText` / `kTypeBinary` / `kTypeBasic` / `kTypeSystem`.

**Type mapping and the refusal FR-010 requires**, derived from the constants
already present in `ProDosVolume.h` and `Dos33Volume.h`:

| Merlin `TYP` | ProDOS | DOS 3.3 | Note |
|---|---|---|---|
| `$04` | `$04` TXT | `$00` T | maps |
| `$06` | `$06` BIN | `$04` B | maps; the default |
| `$FC` | `$FC` BAS | `$02` A | maps |
| `$FF` | `$FF` SYS | none | **refused on DOS 3.3** |
| anything else | none | none | **refused, naming the byte** |

`SYS` has no DOS 3.3 counterpart because the ProDOS kernel boots by scanning the
volume directory for a `SYS`-typed entry, and DOS 3.3 has no system-program
concept to approximate it with. `Dos33Volume::kTypeInteger` and
`kTypeRelocatable` have no ProDOS counterpart in the recognized set either, but
nothing needs one: `TYP` is a ProDOS directive naming a ProDOS type.

## 4a. Merlin has no listing FILE, so it cannot settle how one is segmented

**Finding**: Merlin sends its listing to the screen and/or printer and never to
disk. There is no listing file, and no pseudo-op that would make one.

> "LST controls whether the assembly listing is to be sent to the Apple screen
> and/or other output device."

`LST` controls *whether*, not *where*. `PAG` "sends a form feed ($8C) to the
printer. It has no effect at any time on the screen", which says plainly what
the destination is. `TR`, `EXP`, `AST` and `SKP` shape what is printed and
nothing more.

**Why this matters.** `-l<file>` is **as65's** flag, not Merlin's. So the
question "how should a listing file be split across several outputs" has no
Merlin answer to be faithful to, and SC-003 does not reach it: a period
assembler produced no such file, so it cannot have produced a different one.
FR-028 is a Casso engineering decision and should not be defended, or later
"corrected", on fidelity grounds.

**The one relevant piece of Merlin evidence** is that the listing stream is
continuous across a save — the manual documents no relationship between `SAV`
or `DSK` and listing output; they manage object storage and the listing is
unaffected. That weakly favors one listing over several, which is the shape
FR-028 takes.

**And one finding that cuts the other way, recorded because it is inconvenient
rather than despite it**: Merlin's symbol table "appears per assembly in both
alphabetical and numerical order, displayed after object code completes."
Per assembly, unsegmented, *even though* multi-`SAV` assemblies were supported.
So period behavior for the human-readable dump is one flat table.

That does not rescue the machine-read debug file. Merlin has no equivalent of
`-g`'s by-address index — no debugger was reading Merlin's screen — so the
ambiguity argument in finding 4b stands on its own. But it does mean scoping the
**stdout symbol table** is our improvement on Merlin rather than a match to it,
and the spec says so rather than implying otherwise.

## 4b. Why the debug file's address index cannot span several outputs

**Decision**: Scope symbol and debug output per output (FR-029), by source
position (FR-030), written as a separate set of files per output and named from
each output's name (FR-031, FR-032).

**One combined file per flag was the first answer here and it was wrong.** The
argument for it was that `-g` names one file, so several would hit FR-026's
refusal. That confuses two things: FR-026 refuses a name the USER supplied for
several outputs, and these names are DERIVED, one per output, so there is
nothing to collide. `-g` already derives `<source>.dbg` from a flag that takes
no filename at all, so derivation is the established pattern and only the stem
changes. And a combined file makes a reader looking for one program's code walk
past the others, which is the whole complaint.

**Rationale**: `Assembler::FormatDebugInfo` builds both indexes from a single
flat `std::unordered_map<std::string, Word>` covering the whole assembly, and
its own comment states the question the file exists to answer:

> "Reading a debug file is two different questions -- 'what is at $0310' and
> 'where did FOO go'"

Independent outputs may occupy overlapping addresses and are never in memory
together, so "what is at $0310" has no single answer across them. A flat index
answers with every candidate, including symbols from a program that is not
loaded, which is worse than not answering.

Scoping is by **source position** rather than address range, because an address
test is ambiguous exactly where outputs overlap — the case that motivates the
requirement. Symbols above the first output are equates and belong to none.

One file per flag, because `-g` names one file and several would hit the
refusal FR-026 already imposes on object names.

## 5. The transaction seam already exists and is exactly the right shape

**Decision**: Reuse `DiskImageSession` and `IVolume` unchanged. Do not build a
second write path.

**Rationale**: FR-014's all-or-nothing guarantee, including across several save
points, falls out of what 020 already built rather than needing anything new.

- `IVolume`'s header states it outright: "NOTHING MUTATES IN PLACE. The volume
  holds its buffer as an immutable input and every mutating call produces a
  COMPLETE new buffer, or fails and produces nothing." Several save points
  therefore compose by feeding each `Write` the buffer the previous one
  returned, and nothing reaches the host until the last has succeeded.
- `IVolume::Write` is documented as "Adds or replaces", which is FR-019 for
  free, and the same header explains why replacement computed whole is the safe
  form.
- `DiskImageSession` is "ONE IMAGE, OPENED AND COMMITTED AS A TRANSACTION". It
  records a freshness stamp at read time, probes for the image being held by
  another program, and replaces atomically. That covers three of the spec's edge
  cases, image held open, image changed underneath, and refusal leaving the
  target byte-for-byte, with no new code.
- `IVolume::SetStartupProgram` exists, so FR-021 is a call rather than a
  mechanism.

**Consequence for FR-018**: `DiskImageSession::OpenedImage` carries an `isNew`
flag used by `disk create`, and the freshness check is skipped for it. The
assembler path must NOT set it, because a missing image is a refusal here. The
`requireFilesystem` argument already makes an unrecognized disk a refusal too.

## 6. Where the code goes, and what already lets it

**Decision**: A second `ArtifactSink` implementation, in `CassoEmuCore`.

**Rationale**: `ArtifactWriter.h` already declares `ArtifactSink` as an
abstraction over "where a successful assembly's two files go", with
`FileArtifactSink` as the production implementation, and `AssemblerMode::Run`
already accepts a sink by pointer for exactly this purpose. An image target is a
second implementation of an existing seam, not a new seam.

Placement is settled by what links what. `CassoCli` and `UnitTest` both
reference `CassoCore` **and** `CassoEmuCore` (checked in the two `.vcxproj`
files), the disk layer is `CassoEmuCore/Devices/Disk`, and `ArtifactWriter` is
already `CassoEmuCore/Cli`. So a sink that writes into an image sits beside the
one that writes host files, and `UnitTest` reaches it. Nothing moves, and
nothing lands in an exe.

**One header hazard to respect.** `DiskCommandRunner.h` forward-declares
`VolumeKind`, `DiskFormat`, `BlankDiskContents`, `BlankDiskSpec` and
`BootPayload` rather than including their headers, and says why: including
`VolumeImage.h` "would drag DiskImage.h through this header and into the console
project". Its comment records that doing so "built the library fine and broke
the console project". The new sink's header must follow that discipline.

## 7. What the assembler already reports, and what it must start reporting

**Decision**: Extend `AssemblyResult` with the save points, following the
pattern `outputFileName` already set.

**Rationale**: `AssemblyResult::outputFileName` is documented as "REPORTED
rather than acted on. Nothing here writes a file, so this says what the name is
and leaves the writing to whoever asked for the assembly, which keeps the
precedence rule in one place instead of repeated at every entry point that
produces output." A file type and a list of save points are the same kind of
fact and belong the same way. This also keeps `CassoCore` free of any knowledge
that disks exist, which is what lets FR-003 hold: the capability is the
assembler's, and the directives merely feed it.

The precedence rule FR-007 needs is likewise already built and already
explained. `CommandLineParser::ApplyMerlinDefaults` deliberately does not
default the output name, because "the precedence between the flag and the
directive is settled by the assembler, which sees both, rather than guessed by a
parser that sees one", and `HandlePass1ObjectFile` implements it by declining to
overwrite a caller-supplied name. The type and the on-volume name take the same
route.

## 8. Removing a boundary row is a real change, not a deletion

**Decision**: Delete the `TYP` and `SAV` rows from `s_kMerlinBoundary`, and add
the handlers those directives then need.

**Rationale**: `AssemblySession` resolves a directive against
`m_dialect.GetSubsetBoundary()` and, on a hit, routes it to
`HandleSubsetBoundary`, which claims the line and records an offense. Removing a
row does not merely stop the refusal, it makes the directive fall through to the
ordinary dispatch table at `AssemblySession.cpp:5111`, where it needs an entry
or it becomes an unknown directive. `Directive::FileType` and
`Directive::SaveObject` already exist and are already spelled in
`MerlinDialect.cpp`, so the tokens are recognized; the handling behind them is
what is missing.

`SAV` needs a **pass 2** handler, unlike `DSK`, which is pass 1 only
(`{ Directive::ObjectFile, &HandlePass1ObjectFile, nullptr }`). A save point is
a span of emitted bytes, and bytes are emitted in pass 2.

**The published list is generated, so most of it updates itself.**
`MerlinSubsetBoundary::GetHelpText` composes from the same rows and
`UnitTest/MerlinSubsetBoundaryTests.cpp` sweeps the accessor. The prose in
`docs/merlin-subset.md` is the part that needs a human edit: it says "Six
constructs are recognized and refused by name" and then lists them.

## 9. Counting the boundary, correctly

**Finding**: The drafted spec said this feature takes the refused count from six
to three. It takes it from six to **four**.

`s_kMerlinBoundary` holds six rows: `REL`, `ENT`, `EXT`, `XC`, `TYP`, `SAV`.
`DSK` is not among them and never was. It is accepted today and honored by
redirection to a host file, so it appears in `docs/merlin-subset.md`'s
*supported* directive table, not its refused list. Closing `TYP` and `SAV`
removes two rows and leaves four: the three linker rows, and the second
`XC`.

`DSK` is still a gap this feature closes, but of a different kind: not a refusal
lifted, but a directive finally meaning what it means. Corrected in the spec and
in the checklist notes.

## Sources

- [Merlin: Macro Assembler for the Apple II Family, Glen Bredon](https://gswv.apple2.org.za/a2zine/Docs/MerlinManual.txt) — the `SAV` and `DSK` quotations above.
- [Merlin Pro Macro Assembler manual](https://archive.org/details/MerlinProMacroAssembler) — lists `TYP` as "(ProDOS only)"; the OCR is truncated at the entry itself.
- [Merlin 32 syntax reference, Brutal Deluxe](http://www.brutaldeluxe.fr/products/crossdevtools/merlin/index.html) — the GS/OS `TYP` range, recorded to explain why it does **not** apply here.
- In-tree: `UnitTest/Fixtures/Merlin/CLOCK.S`, `CassoCore/MerlinSubsetBoundary.cpp`, `CassoCore/AssemblySession.cpp`, `CassoEmuCore/Cli/ArtifactWriter.h`, and `CassoEmuCore/Devices/Disk/{IVolume,DiskImageSession,ProDosVolume,Dos33Volume}.h`.
