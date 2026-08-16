# Feature Specification: Disk File Access for the Build Loop

**Feature Branch**: `020-disk-file-access`

**Created**: 2026-08-15

**Status**: Clarified (2026-08-15) — ready for planning

**Input**: User description: "Developer-focused features so a 6502 developer can edit source on Windows, assemble it, get the result onto an Apple II disk image, and boot it — without leaving Casso or hand-assembling on a 128K machine."

## Overview

Casso today assembles 6502 source and boots disk images, but nothing connects the
two. A developer who assembles a program has no way to put it on a disk, and no
way to read a file back off one. The gap is closed by every other Apple II
toolchain with a second and third tool (AppleCommander for file placement, c2d
for bootable images), each with its own conventions and runtime.

This feature makes the loop **edit → assemble → place on disk → boot** a single
toolchain, so a developer types one command and watches their program run.

### Minimum viable path

Stories 1, 2, and 3 together — produce a loadable artifact, put it on a disk, take
files off a disk — are the smallest set that lets a developer **migrate an
existing project onto a modern host**, which is the situation that motivated this
work. All three are P1 for that reason.

Note the order a migrating developer actually experiences: extraction (Story 3)
comes *first*, because their source is currently on Apple II disks and has to come
off before anything else can happen. It is listed third because it is the least
coupled to the others, not because it is needed last.

Stories 4 through 6 are refinements. Each is independently valuable and none gates
the migration.

## Clarifications

### Session 2026-08-15

- Q: When writing into a bit-stream image, what happens to tracks the operation never touched? → A: Re-encode only the tracks whose sectors changed; copy every other track's original bit stream verbatim. Representability is judged per touched track, not per image.
- Q: How is an unwritable track identified — by detecting copy protection? → A: No. Detect standard-ness and refuse anything not provably standard; never enumerate protection schemes. The posture is fail-safe.
- Q: How far does the all-or-nothing guarantee in FR-013 extend? → A: To crash safety. Build the complete image in memory or fail, then commit via a uniquely named temp file and an atomic replace.
- Q: How should the tool detect that a target image is in use by a running emulator? → A: It should not — that scenario is already out of scope per the Assumptions, and the emulator holds no handle to detect. Document the hazard, keep a best-effort probe for other holders, and re-verify the file has not changed between read and write.
- Q: What second-level verbs does the `disk` subcommand use? → A: Descriptive canonical verbs (`list`, `put`, `get`, `delete`, `boot`) with terse aliases (`ls`, `rm`). No `cat`.
- Q: What does the tool return when a volume is damaged but partly readable? → A: Three states on the tool's existing exit-status vocabulary — 0 clean, 1 succeeded with complaints, 2 produced no output.
- Q: What does delete do when a file's sector chain is damaged — refuse, or free what is readable and leak the rest? → A: Neither blindly. Free only what the catalog proves the file uniquely owns, report the rest as leaked, and keep the file removable so a damaged file cannot strand the volume.
- Q: Are files addressed by bare name or by path? → A: By path from the first pass, so subdirectory support is a filled-in capability rather than a later signature change.
- Q: Is the volume reference map built per consumer or once? → A: Once, as a first-class integrity pass with four consumers — delete, listing, allocation, and the pre-commit check on every computed write.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Assemble to a loadable binary (Priority: P1) — DELIVERED

> Already implemented and shipped. `--raw` and `--dos-bin` live in the core's
> output-format writers alongside the existing full-image writer, with tests.
> Retained here for the acceptance record; no further work is planned against it.

A developer assembles a source file and gets a binary that an Apple II can
actually load: just the assembled bytes, optionally carrying the load address and
length the target operating system expects.

Today the only binary output is a full 64 KB memory image padded with fill bytes.
That shape is correct for ROM burning and reference comparison, but a developer
who wants to load a 2 KB routine at `$6000` has to slice 64 KB down by hand.

**Why this priority**: Every other story depends on having a loadable artifact.
Without it there is nothing to place on a disk. It is also the smallest change in
the feature and unblocks developers immediately, even before disk access exists —
they can pair it with an external disk tool the same day.

**Independent Test**: Assemble a source file with the new output selection and
confirm the file contains exactly the assembled bytes (and, when requested, the
4-byte address/length prefix), with no padding.

**Acceptance Scenarios**:

1. **Given** a source file that assembles to 512 bytes starting at `$6000`,
   **When** the developer requests raw binary output, **Then** the output file is
   exactly 512 bytes long and its first byte is the byte assembled at `$6000`.
2. **Given** the same source, **When** the developer requests binary output with a
   DOS 3.3 header, **Then** the output file is 516 bytes: a 2-byte load address of
   `$6000`, a 2-byte length of 512, then the assembled bytes.
3. **Given** the same source, **When** the developer requests the existing full-image
   output, **Then** the output is byte-for-byte identical to what Casso produces
   today.
4. **Given** source that assembles to a non-contiguous layout, **When** raw output
   is requested, **Then** the assembler reports what span was written so the
   developer can tell whether gaps were filled.

---

### User Story 2 - Put a file onto a disk image (Priority: P1)

A developer places an assembled binary onto a disk image, naming it and telling
the disk what kind of file it is and where it loads. The disk is then mounted in
Casso and the program is loaded from the guest.

**Why this priority**: This is the missing step the entire feature exists to
provide. Paired with Story 1 it completes the minimum viable loop: assemble,
place, boot, run.

**Independent Test**: Put a known binary onto a freshly created disk image, mount
that image in Casso, and confirm the guest's catalog lists the file and that
loading it produces the expected bytes in memory.

**Acceptance Scenarios**:

1. **Given** a formatted DOS 3.3 disk image and a 512-byte binary, **When** the
   developer places it as a binary file named `PROG` loading at `$6000`, **Then**
   booting the image and running `CATALOG` lists `B 002 PROG`, and `BLOAD PROG`
   places the bytes at `$6000`.
2. **Given** a formatted ProDOS disk image, **When** the developer places the same
   binary, **Then** the guest catalog lists it as type `BIN` with an auxiliary
   type of `$6000`.
3. **Given** a disk image that already contains a file of the same name, **When**
   the developer places a new file under that name, **Then** the existing file is
   replaced and the freed space is returned to the volume.
4. **Given** a disk image with insufficient free space, **When** the developer
   attempts to place a file, **Then** the operation fails with a message naming
   the shortfall and the image is left byte-for-byte unchanged.
5. **Given** a write-protected image, **When** the developer attempts to place a
   file, **Then** the operation is refused and the image is unchanged.

---

### User Story 3 - Read a disk image's contents (Priority: P1)

A developer lists what is on a disk image and extracts a file from it to the host
— to recover source stored on a disk, to inspect what a program actually wrote, or
to confirm that a placement worked.

**Why this priority**: For a developer migrating an existing project, this is
step one — their source lives on Apple II disks today and cannot be edited on a
modern host until it can be extracted. Nothing else in this feature is reachable
for them until it exists. It also makes the loop debuggable, since listing is how
a developer verifies Story 2 without booting the emulator.

**Independent Test**: List a disk image with known contents and confirm every file
is reported with the correct name, type, size, and lock state; extract one and
compare it byte-for-byte against the original.

**Acceptance Scenarios**:

1. **Given** a disk image containing several files, **When** the developer lists
   it, **Then** each file's name, type, size, and lock state is shown, along with
   the volume's free space.
2. **Given** a disk image containing a binary file, **When** the developer extracts
   it, **Then** the extracted bytes match what was originally placed, and the load
   address is reported.
3. **Given** a disk image containing a text file, **When** the developer extracts
   it, **Then** the high-bit character encoding is converted to host text and line
   endings are normalized.
4. **Given** a disk image whose catalog is damaged, **When** the developer lists it,
   **Then** the readable entries are reported on the output stream, the damage is
   described on the error stream, and the exit status is 1 — succeeded with
   complaints — rather than the operation failing outright or reporting a clean
   success.
5. **Given** a disk image with a track that cannot be decoded into standard
   sectors, **When** the developer lists or extracts from it, **Then** the
   unrecovered sectors are identified as unrecovered rather than reported as
   zero bytes.

---

### User Story 4 - Make the disk boot the program (Priority: P2)

A developer configures a bootable disk so that inserting it and powering on runs
their program, with no typing at the guest prompt.

Casso can already create bootable DOS 3.3 and ProDOS disks, but they always boot
to a stock greeting. There is no way to say "boot into *my* program."

**Why this priority**: It removes the last manual step from the loop. Without it
every iteration ends with the developer typing `BRUN PROG` by hand — tolerable
once, tedious fifty times a day. It is P2 rather than P1 because the loop is
already usable without it.

**Independent Test**: Create a bootable disk, place a program, set it as the boot
program, then boot the image in Casso and confirm the program runs without input.

**Acceptance Scenarios**:

1. **Given** a bootable DOS 3.3 image containing a binary file `PROG`, **When** the
   developer sets `PROG` as the boot program and boots the image, **Then** the
   program runs automatically after DOS loads.
2. **Given** a bootable ProDOS image, **When** the developer sets a system program
   as the boot program and boots the image, **Then** that program runs after
   ProDOS loads.
3. **Given** an image whose boot program names a file that is not present, **When**
   the developer sets it, **Then** the operation is refused with a message naming
   the missing file.

---

### User Story 5 - Boot straight into a program with no operating system (Priority: P3)

A developer produces a disk image that boots directly into a binary with no DOS or
ProDOS present at all — the program owns the machine from power-on and gets the
memory the operating system would otherwise occupy.

**Why this priority**: For demos, test harnesses, and "does this even run," waiting
for DOS to load is pure overhead, and the memory DOS occupies is often exactly
what the program needs. It is P3 because it serves a narrower audience than the
filesystem stories and is independently valuable — it does not depend on any of
them.

**Independent Test**: Produce a no-operating-system boot image from a binary, boot
it in Casso, and confirm the program begins executing without any filesystem
present on the disk.

**Acceptance Scenarios**:

1. **Given** a binary that loads at `$0800`, **When** the developer produces a
   direct-boot image from it and boots that image, **Then** the program is running
   within a noticeably shorter time than an equivalent DOS 3.3 boot.
2. **Given** a binary larger than the space a direct-boot image can carry, **When**
   the developer attempts to produce one, **Then** the operation is refused with a
   message stating the available capacity.
3. **Given** a direct-boot image, **When** the developer asks the program to be
   entered at an address other than its load address, **Then** execution begins at
   the named address.

---

### User Story 6 - Put BASIC source on a disk as a runnable program (Priority: P3)

A developer writes Applesoft BASIC as ordinary text on the host and places it on a
disk as a program the guest can `RUN` directly.

**Why this priority**: Hand-producing tokenized Applesoft is impractical, so
without this a developer who wants a BASIC loader or test harness must type it at
the guest prompt and save it there — which breaks the "edit on the host" premise.
It is P3 because assembly, not BASIC, is the primary audience.

**Independent Test**: Place a known BASIC listing on a disk, boot it, `LIST` the
program in the guest, and confirm the listing matches the source text.

**Acceptance Scenarios**:

1. **Given** a text file containing a numbered Applesoft listing, **When** the
   developer places it as a BASIC program, **Then** booting the disk and running
   `LIST` reproduces the original listing.
2. **Given** a listing containing a syntax error or an out-of-order line number,
   **When** the developer places it, **Then** the operation is refused with the
   offending line quoted and its line number reported.

---

### Edge Cases

- What happens when the target image is currently mounted in a running copy of
  Casso? The emulator holds the disk in memory and will write its own copy back
  when the drive flushes, silently discarding host-side edits. The emulator holds
  no operating-system handle on the file while it is mounted — it reads the bytes
  and closes — so no probe the tool can make detects this, and inventing a
  cross-process protocol to detect it would contradict the scope this feature
  declares. The hazard MUST therefore be documented rather than claimed to be
  prevented, and the tool MUST refuse only what the platform can actually
  observe: another holder with the file open, and a target whose size or
  modification time changed between read and commit.
- What happens when a track cannot be decoded into standard sectors? Every sector
  the decoder could not recover MUST be reported as unrecovered. It MUST NOT be
  presented as a run of zero bytes, which is indistinguishable from genuinely
  zeroed data and, on a write path, silently overwrites what could not be read.
- What happens when a file name is not legal on the target filesystem — too long,
  lowercase, or containing characters the catalog cannot store? The name MUST be
  reported as rejected rather than silently truncated or transliterated.
- What happens when a placement fails partway through — out of space discovered
  after some sectors are written? The image MUST be left exactly as it was; a
  partially written file is worse than a failed operation.
- What happens when the volume's free-space map disagrees with what the catalog
  actually references? The inconsistency MUST be reported rather than compounded
  by allocating from a map that is already wrong.
- What happens when a file is placed on a bit-stream image that carries data the
  sector layer cannot represent (a copy-protected disk)? The operation MUST be
  refused with the reason, rather than rewriting the track and destroying the
  protection.
- What happens when a DOS 3.3 file is locked? Placement over it MUST be refused
  until it is unlocked, matching how the guest behaves.
- What happens when the assembled output is empty (source assembled no bytes)?
  Raw output MUST produce an empty file and say so, rather than producing a header
  claiming zero length with no data.

## Requirements *(mandatory)*

### Functional Requirements

#### Assembler output

- **FR-001**: The assembler MUST be able to emit only the assembled bytes, with no
  padding before the start address or after the end address.
- **FR-002**: The assembler MUST be able to emit the assembled bytes prefixed with
  a 2-byte load address and 2-byte length, in the layout DOS 3.3 binary files use.
- **FR-003**: The existing full-64 KB image, S-record, and Intel HEX outputs MUST
  remain available and byte-for-byte unchanged.
- **FR-004**: The assembler MUST report the start address, end address, and byte
  count of what it wrote, so a build script can place the result without
  re-deriving them.

#### Volume access

- **FR-005**: The system MUST read the file catalog of a DOS 3.3 volume, reporting
  each file's name, type, size in sectors, and lock state, plus the volume's free
  space.
- **FR-006**: The system MUST read the file catalog of a ProDOS volume, reporting
  each file's name, type, auxiliary type, size, and creation/modification stamps
  where present, plus the volume's free space.
- **FR-007**: The system MUST write a file into a DOS 3.3 volume, allocating from
  the volume's free-space map, creating the catalog entry and sector list, and
  leaving the map consistent with what was allocated.
- **FR-008**: The system MUST write a file into a ProDOS volume, allocating from
  the volume bitmap and growing the file's block structure as its size requires.
- **FR-009**: The system MUST extract a file's contents from either volume type.
  Files MUST be addressed by path rather than by a bare name, so a file inside a
  subdirectory is reachable on the filesystem that has them. A volume without
  subdirectories simply has paths one component long.
- **FR-010**: The system MUST delete a file from either volume type, returning its
  space to the volume.
- **FR-011**: Delete MUST NOT free a sector or block that anything else still
  references. Before returning space, the system MUST determine what the volume's
  catalog actually references, and free only what the deleted file uniquely owns.
  Space the file referenced but that cannot be safely freed MUST be reported as
  leaked rather than freed, and a file whose sector chain is damaged MUST still be
  removable on those terms — refusing to delete it would strand the volume, and
  the migrating developer of US3 is working with exactly the degraded disks where
  that arises.
- **FR-012**: Writing a file whose name already exists MUST replace the existing
  file, not create a duplicate entry or fail. Replacement MUST be computed as a
  whole — the complete post-replacement image produced before anything is
  committed — never as a delete applied to the target followed by a write. A
  failure between the two steps of an in-place replacement would leave the old
  file freed and the new one absent, which loses the file outright and is worse
  than a refused write.
- **FR-013**: Every write operation MUST be all-or-nothing: on any failure the
  image MUST be left byte-for-byte as it was. The complete new image MUST be
  built in memory and validated before anything is committed, and the commit
  MUST be crash-safe — written to a uniquely named temporary file alongside the
  target, then atomically replacing it — so an interrupted write cannot truncate
  or corrupt the original. The temporary file MUST be removed on any failure,
  and its name MUST NOT collide when two invocations target the same image.
- **FR-014**: The system MUST refuse to write to a volume that is write-protected,
  and MUST refuse to overwrite a file that is locked. The refusal MUST be
  reported in intelligible terms naming the image and the reason, not as a raw
  platform error code — including when write protection is enforced by the host
  file's read-only attribute and surfaces as an access denial at commit time.
- **FR-015**: Volume access MUST work on every disk image format Casso can already
  mount, including bit-stream images, not only sector-order images.
- **FR-016**: The system MUST refuse to write to an image whose track data cannot
  be losslessly represented as standard sectors, reporting why. Representability
  is judged **per track**: a track that cannot round-trip blocks the write only
  when the operation needs to write to that track.
- **FR-017**: Writing to a bit-stream image MUST re-encode only the tracks whose
  sector contents changed. Every other track's original bit stream MUST be
  preserved verbatim, so timing, sync patterns, weak bits, and data held at
  half/quarter-track positions survive a write elsewhere on the disk.
- **FR-018**: Reading a bit-stream image MUST report, per track, whether that
  track decoded to a complete set of standard sectors, so a caller can tell a
  clean track from an undecodable one rather than silently seeing an undecodable
  track as zeros. This report is what FR-016 and FR-017 judge, and what US3's
  damage description draws on. It is a precondition for every write path, not a
  refinement of one: without it a write silently destroys what it cannot decode.
- **FR-019**: The standard-ness test MUST be a positive proof, not a search for
  known protection schemes. A track qualifies only when it decodes to sixteen
  distinct, valid, standard sectors; anything else is refused. The system MUST
  additionally refuse, before examining any track, an image whose quarter-track
  map resolves any position to something other than its whole track, since that
  disk holds data at half- or quarter-track positions that a sector-level
  rewrite cannot represent, and an image whose own metadata declares it was
  captured with cross-track synchronization or with drive-level fake bits
  preserved, since both say the image carries timing the sector layer discards.
  These whole-image checks are cheap and MUST be evaluated before any track is
  decoded.

#### File types and encoding

- **FR-020**: The system MUST accept a file type and, where the filesystem stores
  one, a load address when placing a file.
- **FR-021**: The system MUST convert host text to the target's character encoding
  and line-ending convention when placing a text file, and reverse the conversion
  when extracting one.
- **FR-022**: The system MUST convert an Applesoft BASIC listing in host text into
  the tokenized on-disk form when placing it as a BASIC program, and reverse the
  conversion when extracting one.
- **FR-023**: Placing a BASIC listing that cannot be tokenized MUST be refused with
  the offending line number and text reported.

#### Boot configuration

- **FR-024**: The system MUST set which program a bootable volume runs after its
  operating system loads.
- **FR-025**: Setting a boot program that is not present on the volume MUST be
  refused.
- **FR-026**: The system MUST be able to produce a disk image that boots directly
  into a supplied binary with no operating system present.
- **FR-027**: A direct-boot image MUST reject a payload that exceeds the memory the
  boot path can load, reporting the available capacity.
- **FR-028**: A direct-boot image MUST support entering the payload at an address
  other than its load address.

#### Interface

- **FR-029**: Every capability above MUST be reachable from the command-line tool
  using subcommand-style invocation consistent with the existing tool: a single
  `disk` subcommand carrying second-level verbs.
- **FR-030**: The second-level verbs MUST be descriptive words — `list`, `put`,
  `get`, `delete`, `boot` — and MUST additionally accept the terse aliases `ls`
  for `list` and `rm` for `delete`. Help output MUST display the descriptive
  form. `put` and `get` are named from the disk's perspective, which is what
  makes their direction unambiguous; the help text MUST say so. `cat` MUST NOT
  be used for the catalog listing, because it collides with the established
  meaning of printing a file's contents — which this tool does under `get`.
- **FR-031**: Every operation MUST report its outcome through an exit status.
  The values **0**, **1**, and **2** are reserved and mean the same thing in
  every subcommand of this tool: **0** the operation completed cleanly, **1** the
  operation succeeded but had complaints (a partial read, damage described on
  the error stream, with the usable result still on the output stream), **2** the
  operation produced no output. This is the meaning the assembler and run
  subcommands already assign, so a script needs no per-subcommand knowledge to
  branch on those three.
- **FR-032**: Exit statuses of **3** and above are scoped to the subcommand that
  returns them and MUST be documented in that subcommand's own help. They carry
  no cross-subcommand meaning and MUST NOT be coordinated against other
  subcommands' values — a caller already knows which subcommand it invoked, so
  requiring global uniqueness above 2 would couple independent subcommands
  without making any script more correct.
- **FR-033**: Failure messages MUST name the image, the file, and the reason, and
  MUST go to the error stream so they do not contaminate piped output.
- **FR-034**: Every capability MUST be documented in the tool's help output.
- **FR-035**: Because modifying an image mounted in a running emulator is out of
  scope (see Assumptions), and because the emulator holds no handle on the file
  to detect, the system MUST document the hazard rather than claim to prevent
  it. It MUST additionally make a best-effort exclusive-open probe and refuse
  when some *other* holder has the file open — which the platform can detect —
  without implying that a clean probe means no emulator is running.
- **FR-036**: The system MUST record the target image's size and modification
  time when it reads the image, and MUST re-verify both immediately before
  committing a write, refusing if either changed. This closes the window in
  which another writer landed between read and commit. It cannot detect a write
  that lands after the commit, and the documentation MUST say so.

#### Volume integrity

These requirements describe one mechanism, not one feature's helper. The same
pass answers what delete may safely free (FR-011), what a listing reports as
damage (US3), whether allocation may trust the free map (Edge Cases), and
whether a computed write is correct before it is committed (FR-013). It is
specified once here because building it three or four times approximately is
how the versions drift apart.

- **FR-037**: The system MUST be able to determine, for a whole volume, what its
  catalog actually references: which sectors or blocks each file claims, which
  are claimed by more than one file, which the free map marks allocated but no
  readable file claims, and which files have a chain the walk could not follow
  to its end.
- **FR-038**: That determination MUST terminate on any input. A corrupted chain
  pointer can form a cycle, and this pass runs by design on volumes selected for
  being damaged, so traversal MUST be bounded — by a visited set, an iteration
  ceiling derived from the volume's own capacity, or both — and a chain that hits
  the bound MUST be reported as unfollowable rather than followed further.
- **FR-039**: Every write MUST run this pass over the **computed result**, before
  anything is committed, and MUST refuse to commit a result that fails it: a
  sector claimed twice, a free map disagreeing with what the catalog references,
  or a file whose chain the edit broke. Verifying its own output is what keeps a
  write path from reporting success over a volume it has quietly damaged.
- **FR-040**: The guarantee in FR-011 that delete frees only what a file uniquely
  owns is bounded by what the catalog could be read. Where the catalog itself did
  not fully parse, a file that could not be read claims nothing observable, so a
  sector it shares with the file being deleted would be freed. This requires
  catalog damage and cross-linking together and is the one case the rule can
  still lose data; the system MUST NOT claim otherwise, and MUST warn distinctly
  when deleting from a volume whose catalog did not fully parse.

Note the asymmetry that keeps the rest safe: delete only ever frees sectors the
deleted file itself claims, so the large ambiguous set — allocated in the free
map, claimed by no readable file — is never touched. That set is either
already-leaked space or an invisible file's data, and refusing to guess between
them is the correct behavior, not a gap.

#### Recovery from a refused write

- **FR-041**: When a write is refused because the image could not be read
  losslessly, the system MUST preserve the rejected state rather than discard it.
  It MUST produce a recovery artifact beside the target in a form that loses
  nothing — including the very content that caused the refusal — leave the
  original untouched, and name that artifact's location in the message. A refusal
  that leaves the user no way to recover their work is only half a fix: the point
  is to keep the original intact **and** the work retrievable, and to let the user
  choose between them.
- **FR-042**: Producing the recovery artifact MUST NOT overwrite an existing
  file, and MUST NOT change what is currently in use or where the system will look
  on next start. Preserving the work and adopting it are different operations, and
  only the first is in scope here.

### Key Entities

- **Volume**: A formatted filesystem on a disk image — DOS 3.3 or ProDOS. Knows its
  total and free capacity, its name or number, and the files it contains.
- **File Entry**: One catalog record — name, type, size, lock state, and (per
  filesystem) load address, auxiliary type, and timestamps.
- **File Payload**: The bytes of one file, plus the metadata needed to place it
  correctly (its type and, where applicable, its load address).
- **Boot Configuration**: What a bootable volume runs after its operating system
  loads, or — for a direct-boot image — the payload, its load address, and its
  entry point.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can go from an edited source file to their program
  running in the emulator with a single command invocation per step and no
  third-party tool.
- **SC-002**: A developer new to Casso can complete that loop for the first time
  using only the tool's own help output, without consulting external documentation.
  Mechanically checkable form: the help output contains a copy-pasteable worked
  example covering assemble → place → boot, and every verb and option used in
  that example appears in the same help output. Whether a newcomer *succeeds* is a
  review gate, not a test — the checkable part is that the material is there.
- **SC-003**: Files placed by Casso are read correctly by the guest operating
  system in 100% of cases for the supported file types — verified by listing and
  loading each type from a booted machine.
- **SC-004**: Files extracted by Casso match the bytes originally placed exactly,
  for every supported file type, across every image format Casso can mount.
- **SC-005**: No failed operation ever leaves a disk image in a state the guest
  operating system cannot read — verified by attempting every documented failure
  mode and booting the image afterwards. This holds for an interruption as well
  as a refusal: a write killed mid-commit leaves the original image intact and
  leaves no temporary file behind.
- **SC-006**: A full iteration — assemble, place, launch, program running — takes
  under 10 seconds on a typical development machine for a program of a few
  kilobytes.
- **SC-007**: A direct-boot image reaches the developer's code in **under 25% of
  the emulated CPU cycles** the equivalent DOS 3.3 boot of the same program takes.
  Emulated cycles rather than wall clock, so the measurement is deterministic and
  independent of host speed and emulation throttling.
- **SC-008**: No write ever destroys data the tool could not read. Verified
  against an image carrying a track that does not decode to standard sectors:
  the write is refused, and the image is byte-for-byte unchanged afterwards.
- **SC-009**: No write is ever committed without having been checked. Every
  committed image passed the volume integrity pass over its computed result, and
  a deliberately corrupted result is refused rather than written — verified by
  the pass rejecting a result whose free map and catalog disagree.
- **SC-010**: Every operation over a deliberately damaged volume terminates.
  Verified against volumes carrying cyclic and self-referential chains, with no
  operation failing to return.
- **SC-011**: A write refused for unreadable content always leaves the user both
  options. Verified by refusing a write on an image with a damaged track and
  confirming: the original is byte-for-byte unchanged, a recovery artifact exists
  beside it, that artifact reproduces every track including the damaged one, no
  pre-existing file was overwritten, and what is currently in use is unchanged.

## Assumptions

- The primary audience is a developer assembling 6502 code on Windows for a
  35-track, 140 KB 5.25-inch disk. Larger media and additional filesystems are out
  of scope here and are covered by a later feature.
- DOS 3.3 and ProDOS are the only filesystems in scope. Pascal, CP/M, and RDOS
  volumes are not addressed.
- The command-line tool is the only interface in this feature. A graphical disk
  manager is a separate feature and does not gate this one.
- Read-side support for damaged or unusual volumes is best-effort: the goal is a
  useful report, not recovery.
- Casso's existing blank-disk creation covers producing the formatted, optionally
  bootable images this feature writes into; no new formatting capability is
  required beyond setting the boot program.
- The tool operates on image files on disk. Modifying an image while it is mounted
  in a running emulator is explicitly not supported by this feature; doing so
  safely is a concern of the graphical disk manager, which can coordinate with the
  running machine because it runs inside that process. A separate command-line
  process cannot, without a coordination protocol this feature deliberately does
  not introduce.
- File placement targets standard-format volumes. Copy-protected bit-stream images
  are readable but not writable, and this is expected rather than a defect. The
  refusal is reached by requiring proof of standard-ness, never by recognizing
  protection schemes.
- Most copy-protected disks are refused by the filesystem layer before the track
  layer is consulted, because they carry no readable DOS 3.3 or ProDOS volume at
  all. The per-track check is the backstop for the awkward middle case — a
  mostly-standard disk with one or two protected tracks — which is precisely
  where a silent failure does the most damage.
- Crash-safe commit is introduced for this tool's writes only. The emulator's own
  flush path writes non-atomically, so after this feature command-line writes are
  crash-safe and emulator flushes are not. That asymmetry is deliberate: one is a
  deliberate one-shot operation on a file the user may have no other copy of, the
  other happens continuously during emulation where the cost would be paid on
  every flush. It is recorded here so it is not rediscovered as an oversight.

## Dependencies and Known Defects

- **The sector decoder discards what it cannot read, and this is a pre-existing
  defect on a live path.** Denibblization stops at the first sector it cannot
  decode on a track and leaves that sector and every later one on the track as
  zero bytes, while reporting overall success. Because the emulator serializes
  sector-format images through that same path on flush, a guest that leaves a
  track partially written can already lose the rest of that track on eject today,
  with no error surfaced. This feature does not introduce the defect, but it does
  make it reachable from a second direction and would build a write path on top
  of it. FR-018 is the fix. It MUST land before any write path that consumes
  denibblized output, and the pre-existing emulator-side exposure SHOULD be
  tracked as its own defect rather than folded silently into this feature.
