# Feature Specification: Assembler-to-Disk Output

**Feature Branch**: `026-assembler-to-disk`

**Created**: 2026-08-29

**Status**: Clarified

**Input**: Let the assembler write its object directly into a disk image instead of only to host files, closing the Merlin `TYP`/`DSK`/`SAV` gap and collapsing the assemble-then-place build loop into one step.

## Why this exists

Casso's assembler writes host files only. `ArtifactWriter` produces an object, a
listing, a symbol table and debug info, and none of them can land inside a disk
image. Two consequences follow.

**The build loop takes a step it should not.** Getting assembled code onto a disk
means assembling, then placing:

```
CassoCli disk create mydisk.dsk --bootable
CassoCli as65 prog.a65 -oprog.bin
CassoCli disk put mydisk.dsk prog.bin --as PROG --type B --load $6000
```

The second and third commands are one operation split in half. Worse, `--load
$6000` restates the origin the source already declared, and nothing checks the
two agree: a source whose `ORG` moves silently produces a file ProDOS loads at
the wrong address.

**Three Merlin directives have nowhere to land.** Merlin assumes the assembler
writes onto a ProDOS volume, so it has `DSK` to name the output file, `TYP` to
set its filesystem type, and `SAV` to write one and carry on. Casso refuses
`TYP` and `SAV` by name, and honors `DSK` only by redirecting it to a host file.
A Windows file has no ProDOS file type, so `TYP` cannot mean anything today.

Six constructs are recognized and refused by name: `REL`, `ENT`, `EXT`, `XC`,
`TYP` and `SAV`. This feature closes two of them, `TYP` and `SAV`, leaving four.
The four that remain are `REL`/`ENT`/`EXT`, which need the relocating linker
(GitHub #112), and a second `XC`, which needs a 65816 core and is out of scope
while Casso declares itself a 6502 / 65C02 assembler.

`DSK` is a third gap this feature closes, and a different kind. It is already
accepted and already honored, so it never appeared on the refusal list at all —
what it lacks is its real meaning. Naming a file that lands on a volume is what
Merlin's `DSK` does; naming a host file is the nearest thing Casso could offer
without one.

## Clarifications

### Session 2026-08-29

- Q: Must the target disk image already exist, or should the assembler be able
  to create one? → A: It must already exist. A missing image is refused, naming
  the command that creates one. Creation implies choosing container type,
  filesystem, volume name and bootability, and the existing disk-creation
  command already owns all four; a second route to them would be two ways to
  make a disk with different rules.
- Q: When no image target is given, does `SAV` stay refused, or gain a meaning
  against host files? → A: It gains one. `SAV` writes several host files from
  one assembly when no image is targeted, so the construct leaves the refused
  list outright instead of trading a boundary refusal for a conditional one,
  and so a directive does not behave differently depending on the target.
- Q: Should assembling to an image be able to set the volume's startup program?
  → A: Yes, behind a flag. One command then produces a disk that boots what was
  just assembled. The rules deciding whether a file is runnable as a startup
  program are the ones the existing boot command already applies, shared rather
  than restated, so the two routes cannot disagree about what they accept.
- Q: What happens when a file of that name already exists on the volume? → A:
  It is replaced. A build loop reassembles constantly, so refusing would fail
  every build after the first, and this is what the existing file-placement
  command already does.
- Q: Does a second `SAV` write the whole object again, or only the bytes since
  the previous one? → A: Only the bytes since the previous one, and this was
  settled by reading Merlin's own manual rather than by choosing. Bredon states
  that a save may be done several times during an assembly and that "after a
  save, the MERLIN object area is 'empty'". The saved file's address follows
  from the same rule: the first save takes the initial origin, and each later
  one continues from where the previous save ended. See
  [research.md](research.md) for the quotations and for the one case the manual
  does not settle.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Assemble straight onto a disk (Priority: P1)

A developer writing 6502 code for the emulator assembles a source file and has
the object land on a disk image, named and typed, ready to boot or `BRUN`,
without a separate placement step and without restating the load address.

**Why this priority**: It is the whole feature in one action, it serves every
dialect, and it delivers the correctness win on its own. Everything else builds
on the target existing.

**Independent Test**: Assemble a source with a stated origin directly to an
existing image, then read the file back and confirm its contents, its type, and
that its recorded load address matches the source's origin rather than anything
typed on the command line.

**Acceptance Scenarios**:

1. **Given** an existing disk image and a source with an origin of `$6000`,
   **When** the developer assembles to that image under a chosen file name,
   **Then** the file appears on the volume with the assembled bytes, and its
   recorded load address is `$6000` without the developer stating it.
2. **Given** the same invocation, **When** the assembly succeeds, **Then** the
   listing, symbol table and debug info requested by their own flags are written
   as host files, not into the image.
3. **Given** a source that fails to assemble, **When** the developer assembles to
   an image, **Then** the image is byte-for-byte unchanged.
4. **Given** any supported dialect, **When** the developer assembles to an image,
   **Then** the behavior is the same, because the capability belongs to the
   assembler and not to one dialect.

---

### User Story 2 - Merlin source that names its own output (Priority: P2)

A developer brings existing Merlin source containing `DSK` and `TYP` and
assembles it against a disk image. The directives mean what Merlin meant: the
file is created on the volume under the name `DSK` gives, with the type `TYP`
gives.

**Why this priority**: It closes the documented subset gap and makes period
source assemble unmodified, but it depends on User Story 1 having built the
target.

**Independent Test**: Assemble a Merlin source containing `DSK` and `TYP`
against an image, with no naming or typing flags on the command line, and
confirm the volume shows exactly the name and type the source asked for.

**Acceptance Scenarios**:

1. **Given** Merlin source with `DSK PROG` and `TYP $06`, **When** assembled to a
   ProDOS image with no naming flags, **Then** a binary file named `PROG` exists
   on the volume.
2. **Given** the same source, **When** assembled with a command-line name that
   differs from `DSK`, **Then** the command-line name is used, because the flag
   overrides the directive.
3. **Given** Merlin source with `TYP` naming a type the target filesystem has no
   equivalent for, **When** assembled, **Then** the assembly is refused with a
   message identifying the type and the filesystem, and the image is unchanged.

---

### User Story 3 - One source, several output files (Priority: P3)

A developer keeps a loader and a main program in one source file, sharing its
equates and macros, and produces both as separate outputs in a single assembly —
onto the volume when an image is targeted, and as host files when one is not.

**Why this priority**: It is the least common workflow and the one with the most
design surface. The feature is valuable without it.

**Independent Test**: Assemble a source containing two origin-and-save sequences
against an image and confirm both files exist with their own names, types and
load addresses; assemble the same source with no image target and confirm two
host files appear instead.

**Acceptance Scenarios**:

1. **Given** a source with two `ORG`/`SAV` sequences naming different files,
   **When** assembled to an image, **Then** both files exist on the volume, each
   with its own recorded load address, and the second file holds only the bytes
   assembled after the first save — not the first file's bytes as well.
2. **Given** the same source, **When** the assembly fails after the first save
   would have been written, **Then** the image contains neither file and is
   byte-for-byte unchanged.
3. **Given** the same source, **When** assembled with no image target, **Then**
   two host files are written, one per save, and the directive is not refused.
4. **Given** the same source, **When** the assembly fails after the first save
   would have been written and no image target was given, **Then** no host
   file from this assembly is left behind.

---

### User Story 4 - A disk that boots what was just assembled (Priority: P3)

A developer assembling to an image asks, on the same command line, that the
object become the program the volume runs when it boots. One command turns
source into a disk that starts it.

**Why this priority**: It completes the build loop the feature exists to
shorten, but it is an addition to the target rather than part of it, and every
other story stands without it.

**Independent Test**: Assemble to a bootable image with the startup flag given,
then read the volume back and confirm it names the assembled file as its
startup program.

**Acceptance Scenarios**:

1. **Given** a bootable image and a source that assembles cleanly, **When** the
   developer assembles to that image asking for the object to be the startup
   program, **Then** the volume records it as the program it runs at boot.
2. **Given** the same invocation against a target filesystem that would not
   actually run a file of that type at boot, **When** assembled, **Then** the
   request is refused on the same terms the tool's existing boot command uses,
   and the image is unchanged.
3. **Given** the flag with no image target, **When** assembled, **Then** the
   command line is refused, because there is no volume whose startup program
   could be set.

---

### Edge Cases

- The named image does not exist. **Refused**, naming the command that creates
  one; the assembler does not create disks.
- The image exists but holds no filesystem, or one that is not recognized.
- The volume has no room for the object, or no free directory entry.
- A file of that name already exists on the volume. **Replaced**, as the
  existing file-placement command does.
- The name is legal on the host but not on the target filesystem — too long, or
  using characters the filesystem forbids.
- The source declares no origin at all, so there is no load address to record.
- The assembly produces zero bytes.
- The image is open in a running emulator, or held by another program.
- A save is requested when no image target was given. **Writes a host file**;
  the directive is not refused for want of a disk.
- The startup-program flag is given with no image target.
- The same file is named twice in one assembly. **Refused**, naming the file —
  unlike a name left by an earlier run, which is replaced.
- A command-line name is given and the source produces several outputs.
  **Refused**, naming the flag; one name cannot serve several files.
- The source names an output before the bytes exist (`DSK`) and again after they
  do (`SAV`), for the same span.
- Bytes are emitted after the last save, so a span ends with nothing having
  named it.

## Requirements *(mandatory)*

### Functional Requirements

**The target**

- **FR-001**: The assembler MUST accept a disk image as the destination for its
  object output, for every dialect it supports, specified on the command line.
- **FR-002**: The assembler MUST accept a file name for the object on that
  volume.
- **FR-003**: The capability MUST behave identically across dialects. A dialect
  MUST NOT be required to have directives for a developer to reach it.
- **FR-018**: The image MUST already exist. A named image that is not there MUST
  be refused, naming the command that creates one, and the assembler MUST NOT
  create, format or name a disk itself.

**What goes where**

- **FR-004**: Only the object MUST be written into the image. The listing, symbol
  table and debug info MUST continue to be written as host files when their own
  flags request them.
- **FR-028**: An assembly producing several outputs MUST produce a listing per
  output, each covering the source lines that became that output. A single
  listing spanning all of them makes a reader searching for one program's code
  hunt through the others, which is the same objection that applies to the
  symbol and debug artifacts.

  **The listing file is this tool's own, not the period assembler's.** Merlin
  sent its listing to a screen or a printer and had no way to write one to
  disk, so there is no period behavior to be faithful to here and SC-003 does
  not reach it. Merlin's listing stream did run continuously across a save, but
  a stream watched as it scrolls and a file opened later to find something are
  not the same artifact, and only the second is being specified here.
- **FR-036**: The source lines above the first output — the equates and macro
  definitions every output shares — MUST appear in each per-output listing.
  Same reason as FR-035: each file has to stand alone.
- **FR-029**: Symbol and debug output MUST be scoped per output. Independent
  outputs may occupy overlapping addresses and are never in memory together, so
  an index spanning all of them cannot answer "what is at this address" — it
  reports symbols from programs that are not loaded. This applies to the debug
  file's address index, to its by-name index, and to the symbol table, all of
  which are organized by address today.

  **This is an improvement on the period assembler, not a match to it, and the
  distinction is recorded so it is not "corrected" later.** Merlin printed one
  flat symbol table per assembly even for multi-output sources, and it had no
  equivalent of the machine-read debug file at all — no debugger was reading its
  screen. So SC-003 does not reach these artifacts: a period assembler produced
  no file to compare against. The requirement stands on the ambiguity argument
  alone, which is sufficient.
- **FR-030**: Symbols MUST be scoped by where they are DEFINED in the source,
  using the same cuts that divide the object, so scoping cannot disagree with
  the bytes. Symbols defined above the first output — equates naming hardware
  addresses and the like — belong to no output, MUST be reported once, and MUST
  NOT be repeated into each. Scoping by address instead would be ambiguous
  exactly where outputs overlap, which is the case this requirement exists for.
- **FR-031**: Where an assembly produces several outputs, its listing, symbol
  and debug artifacts MUST be written as separate files, one set per output,
  rather than as one file holding several sections. Each output already has a
  name, so each set has a filename stem without anything having to be invented,
  and a reader looking for one program's code opens that program's file instead
  of finding its section inside a larger one.
- **FR-032**: Those filenames MUST derive from the OUTPUT's name, not the
  source's. This is the existing behavior of the debug flag generalized: it
  takes no filename today and derives one, so derivation is the established
  pattern and only its stem changes.
- **FR-033**: A single-output assembly MUST keep its present artifact names and
  destinations exactly. Deriving from the output name instead would change
  shipped behavior for the common case and buy nothing, since with one output
  there is nothing to disambiguate.
- **FR-034**: Under Merlin, the listing flag MUST take no filename. Listings are
  named after the output they describe, so a filename can name at most the
  single-output case and cannot express the general one. Withdrawing the value
  removes the possibility of the mismatch rather than adding a refusal for it.
  A filename supplied anyway MUST earn a diagnostic saying listings are named
  after each output, NOT a generic unknown-flag message.
- **FR-037**: Under Merlin, the listing flag MUST write files rather than
  standard output. A listing is read later to find something, which is what a
  file is for; the console stream that Merlin itself produced was a thing to
  watch scroll past, and it cannot be split per output in any case.
- **FR-038**: The as65 grammar's listing flag MUST NOT change. It keeps its
  filename and its standard-output default, because that is an as65
  compatibility obligation rather than a choice, and because an as65 assembly
  always produces exactly one output — the dialect has no directive that could
  produce a second — so none of the multi-output reasoning reaches it.
- **FR-035**: Symbols belonging to no output — the equates above the first —
  MUST be repeated into every per-output artifact. Each file has to stand alone
  for a reader or a debugger holding only that one program, and a hardware
  address it cannot resolve is exactly what such a reader came for. This
  reverses the economy that a single combined file would have allowed.
- **FR-005**: The assembler MUST record the object's load address on the volume,
  derived from the origin the source declared, without the developer restating
  it.
- **FR-006**: The assembler MUST assign the object a filesystem type, defaulting
  to the target filesystem's binary type when the source and command line say
  nothing.
- **FR-019**: A file already on the volume under the name the object takes MUST
  be replaced, and the replacement MUST be subject to FR-014, so a failure
  cannot leave the volume holding neither the old file nor the new one.

**Precedence**

- **FR-007**: Where a source directive and a command-line flag both supply a
  name or a type, the command-line value MUST win and the directive MUST supply
  the default. This follows the precedence the tool already applies to the object
  file name, which is settled by the assembler because only it sees both.
- **FR-026**: A command-line name supplies ONE name, so an assembly that
  produces more than one output while a command-line name is in force MUST be
  refused, naming the flag and how many outputs the source asked for. It MUST
  NOT apply that name to each output in turn: every output but the last would be
  replaced by the next, and the tool would report success having written one
  file where the source asked for several. A command-line TYPE has no such
  limit, because one type applies to every output without ambiguity.
- **FR-027**: Two outputs of a single assembly MUST NOT be written under the
  same name. This MUST be refused, naming the file. It is a different case from
  FR-019, which replaces a file left by an EARLIER run: replacing across runs is
  what a build loop needs, and replacing within one run discards an output the
  source just asked for.

**Merlin directives**

- **FR-008**: `DSK` MUST name the object on the volume when an image target is
  given, and MUST continue to name a host file when one is not.
- **FR-025**: A second `DSK` in one source MUST close the output the first
  named and begin another, rather than renaming a single output. Merlin's
  manual is explicit that a `DSK` arriving while one is in effect closes the
  old file and begins a new one, so a source with two of them produces two
  files. Today the tool keeps only the last name, which is indistinguishable
  from Merlin for one occurrence and wrong for two.
- **FR-009**: `TYP` MUST set the object's filesystem type when an image target is
  given.
- **FR-010**: A type with no counterpart on the target filesystem MUST be refused
  by name, identifying both the type and the filesystem, rather than mapped to
  an approximation. A ProDOS system file has no DOS 3.3 equivalent, because
  DOS 3.3 has no system-program concept.
- **FR-011**: A type value outside the set the tool recognizes MUST be refused,
  naming the value.
- **FR-012**: `SAV` MUST write the object accumulated since the previous save —
  or since the start of the assembly, for the first — to the current target, and
  MUST then allow assembly to continue with that accumulation emptied. Bytes
  already saved MUST NOT appear in a later file. This is Merlin's own behavior,
  not a choice: its manual states that the object area is empty after a save.
- **FR-024**: Each save point's recorded load address MUST be the address its
  own first byte assembles to, so several saves from one source land where the
  source put them. With no intervening origin this continues from the previous
  save's last address plus one, which is the rule Merlin's manual states; where
  the source does state a new origin, that origin governs.
- **FR-020**: `SAV` MUST NOT depend on an image target. With one, it writes to
  the volume; without one, it writes a host file, so one assembly produces
  several host files. It MUST NOT be refused for want of a disk.
- **FR-013**: The published Merlin subset boundary MUST be updated so the
  constructs this feature implements are no longer listed as unsupported. The
  boundary table is the single authority every refusal and published list is
  composed from, so this MUST be a change to that table and not a special case
  elsewhere.

**Booting what was assembled**

- **FR-021**: The assembler MUST be able to set the target volume's startup
  program to the object it just wrote, requested on the command line. This MUST
  be off unless asked for.
- **FR-022**: That request MUST be judged by the same rules the tool's existing
  boot command applies, shared rather than restated, so the two routes cannot
  accept different things. A volume whose operating system would not actually
  run the file MUST be refused on those terms.
- **FR-023**: The request MUST be refused when no image target was given, since
  there is no volume whose startup program could be set.

**Integrity**

- **FR-014**: A write to an image MUST be all-or-nothing. An assembly that fails
  at any point MUST leave the image byte-for-byte as it was, including when an
  earlier save in the same assembly had already produced a file.
- **FR-015**: Any refusal MUST leave the image unchanged and MUST state which
  condition it hit, so the developer knows whether to change the source, the
  command line, or the disk.

**Compatibility**

- **FR-016**: Assembling to host files MUST be unchanged when no image target is
  given.
- **FR-017**: The feature MUST be documented in the tool's own help output.

### Key Entities

- **Image target**: The disk image the object is written into, together with the
  name the object takes on that volume.
- **Object placement**: What the volume records about the file — its name, its
  filesystem type, its load address, and its contents.
- **Save point**: One complete output produced during an assembly. An assembly
  produces one by default and may produce several. A save point is written to
  whichever target the invocation named — the volume, or the host — so the
  concept does not belong to the image.
- **Startup request**: The optional ask that the object just written become the
  program the volume runs at boot. It is a property of the invocation, not of
  the assembly, and it is meaningless without an image target.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can go from source to a file on a disk in one fewer
  command than before, and the documented build loop shrinks from three steps to
  two. Reaching a disk that BOOTS the assembled program shrinks from four steps
  to the same two, because setting the startup program stops being a step.
- **SC-002**: The load address recorded on the volume matches the source's origin
  in 100% of assemblies, with no opportunity for a developer to state a
  conflicting one.
- **SC-003**: A Merlin source using `DSK`, `TYP` and `SAV`, within the supported
  subset, assembles unmodified and produces the same files a period assembler
  would have produced.
- **SC-004**: The count of constructs listed as outside the Merlin subset falls
  from six to four, and every remaining one — `REL`, `ENT`, `EXT` and a second
  `XC` — is attributable to the linker or to a processor the emulator does not
  model. No construct is merely reworded into a different refusal: `TYP` and
  `SAV` leave the list outright.
- **SC-005**: No failure path leaves a modified image. Every refusal and every
  failed assembly leaves the target byte-for-byte as it was.
- **SC-006**: Assembling without an image target produces byte-for-byte the same
  host files as before this feature, for every source that assembled before this
  feature.
- **SC-007**: A single assembly can produce a disk that boots straight into the
  program it just assembled, with no further command.

## Assumptions

- The disk formats and filesystems already supported for reading and writing are
  the ones this feature writes into. No new container or filesystem support is in
  scope.
- The existing command that places a host file onto an image remains, and is the
  right tool for placing files the assembler did not produce. This feature is not
  a replacement for it.
- Listing, symbol table and debug info remain host artifacts because host tools
  and any future debugger read them from the host filesystem while the program
  under test runs from the image.
- A future in-emulator debugging experience is a consideration behind FR-004 but
  is not part of this feature.
- The relocating linker (GitHub #112) is out of scope. `SAV` produces several
  complete, independent outputs, which is the opposite of what a linker does, so
  it does not depend on one.
- Creating a disk stays with the existing disk-creation command, so the two
  steps that remain in the build loop are "make a disk" and "assemble onto it".
  Setting the startup program is folded into the second rather than being a
  third, and the existing command that sets it separately remains, for a disk
  whose startup program is not something this assembly produced.

## Dependencies

- Existing disk read/write support for the sector and bit-stream formats.
- The existing filesystem-type mapping between ProDOS and DOS 3.3.
- The Merlin subset boundary table, which must be updated rather than bypassed.
- The existing startup-program mechanism and the rules deciding what a booting
  volume will actually run, which FR-022 shares rather than reimplements.
