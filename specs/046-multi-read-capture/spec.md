# Feature Specification: Multi-Read Consensus Capture

**Feature Branch**: `046-multi-read-capture`

**Created**: 2026-10-25

**Status**: Draft

**Input**: User description: "Multi-read consensus capture of real floppy
disks: read each disk several times, on two different drives where possible,
and merge the reads by majority vote per sector. A read that failed a sector
does not vote on it. Builds on 033 Casso Explorer's CassoLink and on 040
Disk inspector's sector pairing, comparison and shared sector writer.
Repeated capture over CassoLink with targeted re-reads; a consensus merge as
a core engine, a `disk` command and an Explorer action; an inspector view of
a set of reads. Flux capture and raw nibble capture are out of scope."

## Background

A 5.25-inch floppy is analog media. A weak spot, a dirty head or a drive
slightly out of alignment can make a sector read correctly one time and
wrongly the next. One read cannot separate a marginal sector from a sound
one, because the drive returns 256 bytes either way. Archivists working
through a box of disks therefore read each disk several times, on two
drives where they can, and use the value most reads have in common. The
workflow for this feature comes from
<https://norsewanderer.com/blog/BlogDSK3.html>: five standard reads of every
disk, and seven or nine for a disk with any trouble. The count is odd so a
vote cannot tie, and a read in a second drive is independent of the first drive's faults.

Casso already has the pieces. 033's CassoLink reads a real Apple's disks
from the PC, a whole disk or a single sector at a time. 040's Disk
inspector compares two disks sector by sector and edits a sector through
one shared sector writer. This feature adds the part between them: taking
many reads of one disk, settling each sector by vote, and marking where the
reads differ.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Merge existing reads of a disk (Priority: P1)

A user has several image files of one disk, such as five ADT Pro captures,
`Zork1 0001.dsk` to `Zork1 0005.dsk`. The user chooses the files, or the
folder, and Casso merges them into one image by majority vote per sector,
with a report of the sectors where the reads differ.

**Why this priority**: The merge is useful by itself with reads made by any
tool, and every other story feeds it or builds on it.

**Independent Test**: Make five copies of a disk image, change one sector
in two of them and a different sector in three of them, merge, and compare
the result and the report with the vote worked out manually.

**Acceptance Scenarios**:

1. **Given** five reads of a disk that match on every sector but one, where
   three reads have one value and two another, **When** the user merges
   them, **Then** the merged image has the three-read value and the report
   lists that sector as settled by a majority of 3 to 2.
2. **Given** a folder of ADT Pro batch captures of several disks, **When**
   the user merges the folder, **Then** the reads are grouped by disk after
   the trailing capture counter and punctuation are removed from each file
   name, each group is ordered by modification time, and each group is
   merged into a separate image.
3. **Given** a read whose failure list includes sector $5 of track 3,
   **When** the reads are merged, **Then** that read does not vote on that
   sector, and the report lists the votes cast and the reads that
   abstained.
4. **Given** reads that tie on a sector, **When** they are merged, **Then**
   the merged image has the earliest read's value among the tied ones and
   the report lists the sector as tied.
5. **Given** a merge through the `disk` command, **When** it runs, **Then**
   the result and the report match those of the Explorer action for the
   same reads.

---

### User Story 2 - Inspect disagreements and settle sectors manually (Priority: P2)

A user opens a set of reads in the Disk inspector. The platter, the file
map and the Tracks tab mark each sector by its outcome. In Sector data,
each byte that differs between reads is marked, and its tooltip lists each
value and how many reads have it. For a sector the vote did not settle,
the user picks one read's bytes.

**Why this priority**: Tied sectors and sectors that failed in every read
require a person's judgment, and the merge by itself cannot settle them.

**Independent Test**: Open the set of reads from Story 1's test in the
inspector, check the outcome marks against the known changes, and pick a
read for the tied sector.

**Acceptance Scenarios**:

1. **Given** a set of reads open in the inspector, **When** the user looks
   at the platter, **Then** each sector has one of four outcome marks
   (unanimous, majority, tied, failed), and a sector is also marked
   marginal when any read failed it or any vote differed from the winning
   value.
2. **Given** a sector whose reads differ, **When** the user selects it,
   **Then** Sector data has the merged bytes with each differing byte
   marked, and the byte's tooltip lists every value with its count of
   reads.
3. **Given** a tied sector, **When** the user chooses one read's bytes for
   it, **Then** those bytes go into the merged image through the shared
   sector writer as a pending edit, and the next report lists the sector
   as settled manually, with the read chosen.
4. **Given** a set of reads open in the inspector, **When** the user opens
   the Tracks tab, **Then** each track's row has its counts of unanimous,
   majority, tied, failed and marginal sectors.

---

### User Story 3 - Capture several reads of a disk over CassoLink (Priority: P3)

A user with a real Apple connected over CassoLink puts a disk in the drive,
types its label, and requests five reads. Casso reads the whole disk five
times and saves each read with its failure list. Where any read failed a
sector or the reads differ, Casso re-reads only those sectors, up to a
count the user chooses, and offers a read in the Apple's other drive.

**Why this priority**: It replaces the manual passes of the archiving
workflow and is faster than whole-disk re-reads, but it depends on 033's
link and on real hardware or an emulated Apple.

**Independent Test**: In Casso, connect an emulated Apple over CassoLink
with a disk whose drive is made to fail one sector on some reads, capture
five reads, and check the saved reads, the failure lists and the targeted
re-reads.

**Acceptance Scenarios**:

1. **Given** a connected Apple with a disk in drive 1, **When** the user
   requests five reads with the label "Zork I", **Then** five whole-disk
   reads are saved as `Zork I 0001` to `Zork I 0005` in the chosen folder,
   each with its failure list.
2. **Given** a first pass in which two reads failed sector $5 of track 3,
   **When** targeted re-reads are on with a count of 4, **Then** Casso
   reads that sector four more times, saves each re-read with the set, and
   the merge counts each re-read as a vote for that sector only.
3. **Given** a disk with failed or differing sectors after its reads,
   **When** the capture ends, **Then** Casso offers "Read again in drive 2",
   and a read made there is saved with the set and labeled with its drive.
4. **Given** a disk whose reads match on every sector, **When** the capture
   ends, **Then** Casso merges the reads and reports the disk as done with
   no prompt.
5. **Given** a batch of disks, **When** the user finishes one disk and
   types the next label, **Then** the capture counter starts again at 0001
   for the new label.

---

### User Story 4 - Read a whole stack of disks in a batch (Priority: P4)

A user works through a box of disks: insert, type the label, capture,
repeat. At the end, Casso lists every disk with its outcome: done, settled
by majority with marginal sectors, or unsettled. The list identifies the
disks that require another drive or other hardware.

**Why this priority**: It turns the per-disk capture into the triage of the
archiving workflow, but Story 3 already covers each disk.

**Independent Test**: Capture three disks in one batch, one clean, one
with a marginal sector and one with a sector that fails in every read, and
check the batch summary.

**Acceptance Scenarios**:

1. **Given** a batch of three disks as in the test, **When** the batch
   ends, **Then** the summary lists them as done, marginal and unsettled,
   and lists the unsettled sectors of the third.
2. **Given** a batch summary, **When** the user chooses a disk in it,
   **Then** that disk's set of reads opens in the Disk inspector.

---

### Edge Cases

- **A read of a different disk**: a read whose format, size or volume
  structure differs from the set's earliest read is left out of the merge,
  never counted as a vote, and listed in the report with the reason.
- **Reads in different formats of one disk**: a .dsk, a .po, a .nib and a
  .woz of the same disk vote together; each is compared by its standard
  sectors in DOS 3.3 order, as 040's comparison pairs them.
- **A .nib or .woz read**: a sector whose data checksum fails in that read
  counts as failed, and so does a sector missing from the read.
- **A single read**: merging one read is allowed; the report lists every
  sector with one vote, and none as settled by majority.
- **An even number of reads**: allowed; ties are reported as in Story 1.
- **Every read failed a sector**: the merged sector is 256 zero bytes, and
  the report lists it as failed.
- **A non-standard track**: when every read is a .nib or .woz, a track with
  no standard sectors in any read is copied from the earliest read with a
  record there; otherwise the report lists it as not compared.
- **The link drops during a capture**: the reads already saved remain, the
  read in progress is discarded, and the capture can resume at the next
  read.
- **A disk removed during a targeted re-read**: the re-read fails with the
  reason, and no re-read of a different disk joins the set.
- **A label with characters not allowed in a file name**: those characters
  become underscores in the file name, and the report has the label as
  typed.

## Requirements *(mandatory)*

### Functional Requirements

**Sets of reads**

- **FR-001**: A set of reads MUST be one or more disk images of one disk,
  in any format Casso reads, each with its failure list when one exists.
- **FR-002**: Casso MUST form sets from a folder by removing from each file
  name a trailing run of up to four digits and the punctuation and spaces
  around it, grouping files whose remaining names match, and ordering each
  group by modification time.
- **FR-003**: Casso MUST exclude from a set any read whose format, sector
  count or volume structure differs from the set's earliest read, and MUST
  list it in the report with the reason.
- **FR-004**: Casso MUST skip files from ADT Pro's virtual drive server
  (`Virtual.po`, `Virtual2.po`) when forming sets from a folder.

**Failed sectors**

- **FR-005**: A sector MUST count as failed in a read when the read's
  failure list includes it; when the read is a .nib or .woz and the
  sector's data checksum fails or the sector is missing; or, for a plain
  sector image with no failure list, when its bytes are all zero and
  another read of the same sector has bytes that are not all zero.
- **FR-006**: The user MUST be able to turn the all-zero rule of FR-005 off
  for a set, after which every read of a plain sector image votes on every
  sector.
- **FR-007**: A failed sector MUST NOT vote.

**The vote**

- **FR-008**: Casso MUST settle each sector by comparing the 256 bytes of
  every read that did not fail it; the value with the most reads is chosen.
- **FR-009**: Each sector MUST get exactly one outcome: unanimous (every
  vote cast has one value), majority (one value has more votes than any
  other), tied (two or more values share the most votes), or failed (no
  read voted).
- **FR-010**: A unanimous or majority sector MUST also be marked marginal
  when any read failed it or any vote differed from the winning value.
- **FR-011**: The merged image MUST have, for each sector, the winning
  value; for a tied sector, the earliest read's value among the tied ones;
  and for a failed sector, 256 zero bytes.
- **FR-012**: The merged image MUST be written in the format of the set's
  earliest read, or a format the user chooses, through 033's image
  writing, and MUST NOT overwrite any read in the set.

**The report**

- **FR-013**: Every merge MUST produce a report with the reads in order and
  their drive when known; the count of sectors for each outcome; and every
  majority, tied, failed and marginal sector by track and sector, with its
  DOS 3.3 logical sector and ProDOS block, the votes for each value and
  the reads that abstained.
- **FR-014**: The report MUST be saved as a text file beside the merged
  image, and the `disk` command MUST also print it.
- **FR-015**: A report written after a sector is settled manually (FR-024)
  MUST list that sector as settled manually, with the read chosen.

**Command and Explorer**

- **FR-016**: The `disk` command MUST merge a list of image files, or a
  folder, into merged images and reports, with options for the output
  folder, the output format, and FR-006's rule.
- **FR-017**: Casso Explorer MUST offer the same merge as an action on a
  selection of images or a folder, with the same options and results.

**Inspector**

- **FR-018**: The Disk inspector MUST open a set of reads, with the merged
  image as the disk.
- **FR-019**: The platter, the file map and the Tracks tab MUST mark each
  sector by its outcome, and MUST mark a marginal sector in addition to its
  outcome.
- **FR-020**: Each outcome and marginal MUST have a separate color, a Legend
  entry, and a tooltip line with that color's swatch, in the same way 040
  marks find hits.
- **FR-021**: Each row of the Tracks tab MUST have the track's counts of
  unanimous, majority, tied, failed and marginal sectors.
- **FR-022**: Sector data MUST mark each byte whose value differs between
  voting reads, and the byte's tooltip MUST list each value with its count
  of reads.
- **FR-023**: The inspector MUST offer "Previous unsettled" and "Next
  unsettled" to step through tied and failed sectors in disk order.
- **FR-024**: For any sector, the user MUST be able to choose one read's
  bytes, which MUST go into the merged image through 040's shared sector
  writer as a pending edit.

**Capture over CassoLink**

- **FR-025**: Casso Explorer MUST capture a disk in a connected Apple's
  drive a chosen number of times, from 1 to 15, default 5, and MUST save
  each whole-disk read as a separate image with its failure list.
- **FR-026**: Each read's file name MUST be the user's label and a
  four-digit capture counter, starting at 0001 for each label, so the
  reads form a set under FR-002.
- **FR-027**: After the whole-disk reads, when targeted re-reads are on,
  Casso MUST re-read each sector that any read failed or that the reads
  differ on, a chosen number of times, from 1 to 15, default 4, through
  CassoLink's single-sector read, and MUST save each re-read so it votes on
  its sector only.
- **FR-028**: Targeted re-reads MUST be available only when the Apple runs
  Casso's link client. With ADTPro's client, only whole-disk reads MUST be
  offered, with the reason in the capture dialog.
- **FR-029**: When a disk ends its reads with any failed, tied or marginal
  sector and the Apple has a second drive, Casso MUST offer a read in the
  other drive. Every read MUST record the drive it came from.
- **FR-030**: A disk whose reads are unanimous on every sector MUST be
  merged and reported as done without a prompt.
- **FR-031**: A batch MUST continue disk after disk until the user ends it,
  and MUST end with a summary of each disk's outcome (done, marginal,
  unsettled), from which a disk's set opens in the inspector.
- **FR-032**: When the link drops, every read already saved MUST remain,
  the read in progress MUST be discarded, and the capture MUST be able to
  resume at the next read.

### Key Entities

- **Read**: one disk image of the disk, with its source (a file, or a
  CassoLink drive), its order in the set, and its failure list.
- **Failure list**: the file 033's Read disk to image saves beside an image
  written with unreadable sectors, `NAME.unreadable.txt` (for example
  `game.dsk.unreadable.txt`). Each line that does not start with `#` is one
  sector, `track sector` in decimal, by DOS 3.3 logical sector number in
  every image format. A Casso read with no unreadable sectors has no list,
  since a clean read removes a list of the same name.
- **Set of reads**: the reads of one disk, in order, and the reads excluded
  from it with the reason for each.
- **Sector outcome**: for one sector, its votes by value, the reads that
  abstained, its outcome (unanimous, majority, tied, failed), whether it
  is marginal, and the read chosen manually, if any.
- **Merge report**: the set's reads and every sector outcome other than a
  unanimous one with no failed reads.
- **Batch**: the disks captured in one session, each with its label, set
  and outcome.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For a set of five 140 KB reads, the merge and its report are
  ready in under 2 seconds.
- **SC-002**: On test sets built with known changes, every sector's outcome
  and winning value match the vote worked out manually, for every set size
  from 1 to 9 reads.
- **SC-003**: A user steps to every unsettled sector of a disk in the
  inspector with "Next unsettled" alone, without leaving the window.
- **SC-004**: Capturing a disk with one failing sector by five whole reads
  and four targeted re-reads takes less time than seven whole-disk reads
  of the same disk over the same link.
- **SC-005**: A folder of ADT Pro batch captures of 20 disks forms exactly
  20 sets, with no read in the wrong set.

## Assumptions

- 033 Casso Explorer and 040 Disk inspector are merged before this feature
  starts. Its capture stories use 033's CassoLink, sector reads and image
  writing; its inspector story uses 040's comparison pairing, its marks
  and Legend, and its shared sector writer.
- 033 saves the failure list (033 FR-086, Session 2026-10-26, commit
  `876f2f4ec`), and this feature reads and writes the same format for its
  reads and re-reads.
- Only 16-sector disks are captured, as with 033's link; 13-sector images
  can be merged as files.
- Raw nibble, bit stream and flux capture are out of scope. Capturing
  copy-protected disks is the subject of 033's follow-on feature; this
  feature merges .nib and .woz reads when they exist but does not make
  them.
- Specialized capture hardware (Applesauce, KryoFlux, Greaseweazle) is out
  of scope.
- The capture counter and grouping match ADT Pro's batch file names, so
  reads made by ADT Pro and reads made by Casso can share a folder.

Defaults in this draft, for review in `/speckit-clarify`:

- The vote is per sector, not per byte. A sector's 256 bytes are chosen
  together, so the merged image never has a sector that no read produced.
  The inspector marks per-byte differences as an aid to a person's
  judgment, but the vote does not use them.
- A plain sector image marks a failed sector through the failure list saved
  with the read. Without a list, a sector of all zero bytes counts as failed
  when another read of the same sector has bytes that are not all zero, and
  the user can turn that rule off for a set.
- A tied sector takes the bytes of the earliest read among the tied values,
  and a sector that failed in every read is written as zero bytes. The
  report lists both, and either can be settled manually in the inspector.
