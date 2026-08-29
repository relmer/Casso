# Feature Specification: Assembler-to-Disk Output

**Feature Branch**: `026-assembler-to-disk`

**Created**: 2026-08-29

**Status**: Draft

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

Six constructs sit outside Casso's Merlin subset. This feature closes three of
them. The other three are `REL`/`ENT`/`EXT`, which need the relocating linker
(GitHub #112), and a second `XC`, which needs a 65816 core and is out of scope
while Casso declares itself a 6502 / 65C02 assembler.

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
equates and macros, and produces both as separate files on the volume in a
single assembly.

**Why this priority**: It is the least common workflow and the one with the most
design surface. The feature is valuable without it.

**Independent Test**: Assemble a source containing two origin-and-save sequences
against an image and confirm both files exist with their own names, types and
load addresses.

**Acceptance Scenarios**:

1. **Given** a source with two `ORG`/`SAV` sequences naming different files,
   **When** assembled to an image, **Then** both files exist on the volume, each
   with its own recorded load address.
2. **Given** the same source, **When** the assembly fails after the first save
   would have been written, **Then** the image contains neither file and is
   byte-for-byte unchanged.

---

### Edge Cases

- The named image does not exist.
- The image exists but holds no filesystem, or one that is not recognized.
- The volume has no room for the object, or no free directory entry.
- A file of that name already exists on the volume.
- The name is legal on the host but not on the target filesystem — too long, or
  using characters the filesystem forbids.
- The source declares no origin at all, so there is no load address to record.
- The assembly produces zero bytes.
- The image is open in a running emulator, or held by another program.
- A save is requested when no image target was given.
- The same file is named twice in one assembly.

## Requirements *(mandatory)*

### Functional Requirements

**The target**

- **FR-001**: The assembler MUST accept a disk image as the destination for its
  object output, for every dialect it supports, specified on the command line.
- **FR-002**: The assembler MUST accept a file name for the object on that
  volume.
- **FR-003**: The capability MUST behave identically across dialects. A dialect
  MUST NOT be required to have directives for a developer to reach it.

**What goes where**

- **FR-004**: Only the object MUST be written into the image. The listing, symbol
  table and debug info MUST continue to be written as host files when their own
  flags request them.
- **FR-005**: The assembler MUST record the object's load address on the volume,
  derived from the origin the source declared, without the developer restating
  it.
- **FR-006**: The assembler MUST assign the object a filesystem type, defaulting
  to the target filesystem's binary type when the source and command line say
  nothing.

**Precedence**

- **FR-007**: Where a source directive and a command-line flag both supply a
  name or a type, the command-line value MUST win and the directive MUST supply
  the default. This follows the precedence the tool already applies to the object
  file name, which is settled by the assembler because only it sees both.

**Merlin directives**

- **FR-008**: `DSK` MUST name the object on the volume when an image target is
  given, and MUST continue to name a host file when one is not.
- **FR-009**: `TYP` MUST set the object's filesystem type when an image target is
  given.
- **FR-010**: A type with no counterpart on the target filesystem MUST be refused
  by name, identifying both the type and the filesystem, rather than mapped to
  an approximation. A ProDOS system file has no DOS 3.3 equivalent, because
  DOS 3.3 has no system-program concept.
- **FR-011**: A type value outside the set the tool recognizes MUST be refused,
  naming the value.
- **FR-012**: `SAV` MUST write the object accumulated so far to the volume and
  allow assembly to continue, so one source can produce several complete files.
- **FR-013**: The published Merlin subset boundary MUST be updated so the
  constructs this feature implements are no longer listed as unsupported. The
  boundary table is the single authority every refusal and published list is
  composed from, so this MUST be a change to that table and not a special case
  elsewhere.

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
  produces one by default and may produce several.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can go from source to a file on a bootable disk in one
  fewer command than before, and the documented build loop shrinks from three
  steps to two.
- **SC-002**: The load address recorded on the volume matches the source's origin
  in 100% of assemblies, with no opportunity for a developer to state a
  conflicting one.
- **SC-003**: A Merlin source using `DSK`, `TYP` and `SAV`, within the supported
  subset, assembles unmodified and produces the same files a period assembler
  would have produced.
- **SC-004**: The count of constructs listed as outside the Merlin subset falls
  from six to three, and every remaining one is attributable to the linker or to
  a processor the emulator does not model.
- **SC-005**: No failure path leaves a modified image. Every refusal and every
  failed assembly leaves the target byte-for-byte as it was.
- **SC-006**: Assembling without an image target produces byte-for-byte the same
  host files as before this feature.

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

## Open Questions

These affect scope and are best settled before planning.

- **Q1**: Must the image already exist, or should the assembler be able to create
  one? Creating implies deciding format, filesystem and volume name, which the
  existing disk-creation command already handles.
- **Q2**: When no image target is given, does `SAV` stay refused, or does it gain
  a meaning against host files — several host files from one assembly?
- **Q3**: Should assembling to an image be able to set the volume's startup
  program, so a single command can produce a bootable disk?

## Dependencies

- Existing disk read/write support for the sector and bit-stream formats.
- The existing filesystem-type mapping between ProDOS and DOS 3.3.
- The Merlin subset boundary table, which must be updated rather than bypassed.
