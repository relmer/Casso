# Feature Specification: Disk Inspector

**Feature Branch**: `040-disk-inspector`

**Created**: 2026-10-08

**Status**: Draft

**Input**: GH #159, "WOZ disk images with type FLUX don't boot or give error"
(https://github.com/relmer/Casso/issues/159). Spec 038 made flux tracks play
and shipped in 1.30.0. The owner then commented on the issue "Todo: add this
awesomeness", with a screenshot of AppleEm's Disk Inspector showing *Bandits*
in its Timing mode. The owner later added a file and sector map, sector
editing and comparison to the first release, and preservation stories for a
later release of the same feature. The owner then added two items to the first
release, Apple Pascal and CP/M file maps and the move of Casso's other sector
writers onto the editor's in-place writer, and two to the later release,
custom track formats and the comparison of quarter tracks within one disk.

**GH ref**: GH #159, used in the CHANGELOG entry and in the commit and merge
subjects for this feature.

## Background

A Disk II track is a loop of bit cells. DOS 3.3 formats it as runs of sync
nibbles separating sectors, and each sector is an address field (volume,
track, sector and checksum between a prologue and an epilogue) followed by a
data field. Copy protection changes any part of this: marks, checksums, sync
widths, track lengths, half and quarter tracks, or, on flux tracks, the time
each cell takes. When a disk will not boot, or boots differently from real
hardware, the cause is somewhere in that structure, and Casso offers no view
of it. The `disk` command and salvage show sectors only, and the Disk ][ debug
window logs only the fields the guest reads with standard marks: address
fields with a good checksum, and data fields whose epilogue the guest reads.

AppleEm's Disk Inspector (mikedaley/web-a2e and its core,
mikedaley/applem-core, MIT license) draws the disk as a platter with one ring
per quarter track, colored by what each stretch of a track holds, and unrolls
the selected track as a strip with its sectors, their bytes and its nibbles.
On flux tracks it colors each stretch by how much faster or slower than
nominal its cells are, which is what *Bandits*' protection checks. This spec
adds a disk inspector to Casso and Casso Explorer modeled on it (its ideas,
not its code) and adds the most useful features of other Apple II disk tools.
The inspector also closes the gaps in AppleEm's: it decodes 13-sector data,
keeps unformatted tracks separate from nonstandard ones, checks the fields
AppleEm leaves unchecked, shows flux timing without rounding it to cells, and
shows every error it finds on screen.

Beyond AppleEm, the inspector maps DOS 3.3, ProDOS, Apple Pascal and CP/M
files to the sectors that hold them, edits a sector's data in place without
rebuilding its track, and compares two disks, or one disk before and after the
guest writes to it. The in-place writer the editor uses becomes the one way
Casso's tools change an existing sector, so the `disk` command and Casso
Explorer stop rebuilding the tracks they write. A later release of the same
feature adds what disk preservation needs: identifying the copy protection,
finding weak bits in flux captures that hold several revolutions, viewing a
track at another latch framing, decoding data fields with a disk's own
translate table, decoding documented custom track formats such as RW18, and
comparing two quarter tracks of one disk.

## Scope

**Image formats.** Every 5.25" disk image format Casso mounts in a Disk II
drive: DSK, DO and PO (140 KB sector images), NIB and NB2, WOZ 1, WOZ 2, and
WOZ 2.1 with bit tracks, flux tracks or both. Applies to every machine with a
Disk II controller in slot 6 (Apple ][, ][+, //e and //c). Casso does not open
DOS 3.2 sector images (.d13), which GH #164 tracks, so 13-sector disks reach
the inspector as WOZ or NIB images. In the later release, the inspector also
reads A2R 2 and A2R 3 flux captures of 5.25" disks (FR-137), which Casso does
not mount.

**Hosts.** One inspector window, opened from Casso or Casso Explorer, and a
small preview in Explorer's preview pane:

1. **Casso Explorer's preview pane**: when a disk image is selected, the
   preview adds a small still view of the disk and the summary chips.
   Clicking the small view opens the inspector window.
2. **Casso Explorer's "Inspect disk image"**: on a disk image's right-click
   menu, it opens the inspector window for that file. With exactly two disk
   images selected, "Compare disk images" on the same menu opens one window
   comparing them. Here an applied edit is saved to the file.
3. **Casso**: the same window, attached to drive 1 or drive 2. Here it also
   follows the head, shows the platter turning under the emulated head, and
   analyzes again the tracks the drive writes. An applied edit reaches the
   disk in the drive and its file, and the disk can be compared with itself as
   inserted, with its file, or with the other drive's disk.

The file and sector map, sector editing and comparison work in the inspector
window in Casso and in Explorer. In the later release, Explorer's inspector
also opens A2R flux captures, which Casso does not mount.

**Viewing and editing.** Viewing, the file and sector map, comparison and the
preservation views never change a disk image. The inspector changes a disk
only when the user applies, undoes or redoes a sector edit. An edit changes a
sector's 256 decoded bytes and rewrites only that sector's data field, under
the rules in FR-096 to FR-116. In Casso the edit reaches the disk in the drive
and its file; in Explorer, the file. A write-protected disk is never edited in
place. Editing nibbles, flux, address fields, marks or image metadata is out
of scope.

**Sector writes.** 040 adds one way for Casso's tools to change an existing
sector, the shared sector writer (FR-110 to FR-116). It replaces only a
sector's data-field nibbles inside the existing track and keeps the address
field with its volume number, the sync, the track's length, every other field
and, on a flux track, each cell's recorded time. The inspector's editor, the
`disk` command's sector, block and file writes, and Casso Explorer's writes
all use it, so this part of 040 also changes the `disk` command and Explorer
outside the inspector window. Keeping each cell's recorded time on a flux
track changes shipped spec 038 behavior, as a fix delivered with 040 (FR-113).
GH #TBD tracks the immediate fix of two defects in today's writers: they
reject WOZ images in the standard layout, and they rebuild every track they
write with volume 254 and new sync and length. 040 then moves the writers onto
the shared writer, and makes the `disk` command's and Explorer's sector, block
and file reads find each whole track through the image's map, as the writer
does (FR-111).

**Delivery.** The feature ships in two releases, each in its own merge. The
first release holds User Stories 1 to 10, everything except the preservation
stories, and no story in it ships without the others. It includes the shared
sector writer with the move of the `disk` command's and Explorer's writes onto
it, the fix to flux timing in sector writes, and the Apple Pascal and CP/M
readers the file map needs. User Stories 11 to 16, for preservationists, ship
in a later release of this same feature, with its own merge after the first
release is on master. They are specified now so that the first release's
analyzer, decode settings and views leave room for them, and the requirements
and success criteria that belong to them are marked "later release". Story
priorities give the build order only. The order is the analyzer, then the
views, then the Casso host, then the file and sector map with its Pascal and
CP/M readers, sector editing with the shared sector writer and the move of the
`disk` command's writes onto it, and comparison, then the two Explorer hosts,
and in the later release the preservation stories. Casso Explorer is spec 033,
which is not yet on master. The analyzer, the views, the Casso host, the
shared sector writer, and the map, editing and comparison in Casso do not
depend on it. The Explorer hosts, editing and comparing in Explorer, the move
of Explorer's writes onto the shared sector writer, and ProDOS subdirectories
in the map, which need 033's reading of subdirectories, are built last, after
master is merged into 033 and 040 is rebased onto 033. 040 merges to master
only after 033 is on master, so the first release's merge holds 040's work
alone. Spec 041 (disk integrity) moves the drive's head stop, keeps an empty
track for the guest to format wherever a WOZ file holds none, makes saves
durable and publishes each drive's status; 041 is built on 035 and merges
after it, and whichever of 040 and 041 merges second adapts to the other
(Assumptions).

**AppleEm coverage.** Every feature of AppleEm's Disk Inspector (the web
build at 1.8.4 and the native window added in the same release) is covered by
a requirement below, except these:

- *The data buffers between its core and its page, their size limits and its
  single shared read buffer.* These belong to a web page reading a WebAssembly
  core and have no counterpart in Casso.
- *Its entry in the View menu and the window switcher, and its help page.*
  Casso has no window switcher. The inspector opens from the Storage menu and
  each drive's right-click menu in Casso, and from the right-click menu and
  the preview pane in Explorer. Hints and tooltips on screen take the place of
  the help page, so no control is explained only in documentation.
- *Its bleed drawing,* which paints an empty quarter track as a faded copy of
  its neighbor. Casso's drive reads random bits on a quarter track the map
  leaves empty, not a copy of its neighbor, so the inspector shows such a
  quarter track as "Nothing recorded", and its tooltip shows that the drive
  reads random bits there.
- *The "13-sector data, not decoded" sector state.* The inspector decodes
  13-sector data.
- *Its rule that a valid byte spanning more than 10 cells is noise.* FR-009
  keeps a nibble's kind and counts the zero cells after it instead, because
  timing-bit protections put extra zero cells after field nibbles.
- *3.5" disk analysis,* which its core has and its web window does not use.
  Casso has no 3.5" drive.
- *The native window's text zoom.* The inspector follows Windows display
  scaling, as every other Casso window does.
- *The small disk in its Disk Drives window,* which is a separate drive
  widget, not part of the inspector. Casso's drive widgets are unchanged. The
  widget's per-track access marks are covered by the read marks in FR-047 and
  the "Reads" overlay in FR-031.

Where AppleEm has a defect that a requirement here fixes (bad sectors counted
twice, bad address arcs reported as data, mixed 13- and 16-sector tracks lost
from the summary, sectors it did not verify counted as good, decimal and hex
mixed for the same quantity, invisible errors, no settle delay on Follow
head), the requirement gives the corrected behavior.

**Additions from other tools.** These features come from Applesauce, EDD 4,
Locksmith, Bag of Tricks, Passport, nibbler, a2kit, wozardry, Virtual ][,
Copy II Plus, Nibbles Away, Disk Fixer, CiderPress II, AppleCommander,
A2FileCmd, AppleWin and HxC, and are in scope. All are in the first release
except the last four, which are in the later release:

- A survey of all 160 quarter tracks (EDD's graphic scan, nibbler, a2kit).
- A findings list for the whole disk (Applesauce's log, Passport, nibbler),
  including D5 AA pairs outside any field (a2kit).
- A table of every address and data field on a track, with the marks found
  and stored and computed checksums (Locksmith, Bag of Tricks, Virtual ][).
- Hidden timing bits and nibble validity in the nibble view (Applesauce,
  EDD, AppleWin).
- Decode settings for nonstandard marks, with wildcards and for a range of
  tracks, and for checksums (Copy II Plus, Passport, a2kit).
- A flux interval plot and histogram linked to the nibble selection
  (Applesauce, HxC).
- Track length, sync and gap measurements and the longest sync run (EDD,
  nibbler, Nibbles Away).
- The length of a selection in nibbles, cells and time (Locksmith, Nibbles
  Away).
- Random-bit regions, where the drive reads noise (the WOZ format, AppleWin).
- The image file's own structure and its checks: WOZ INFO, META, maps, track
  records and checksum (Applesauce, wozardry).
- Find with wildcards, at any bit offset, in decoded sector data and for
  timing bits (Applesauce, Locksmith).
- The DOS 3.3 logical sector and ProDOS block each physical sector holds
  (Applesauce, Disk Fixer, CiderPress II).
- The angle of each track's sector 0 and its write seam, and an overlay that
  lines them up across tracks, for track alignment (EDD, AppleWin).
- The drive's write-protect state (AppleWin).
- A file and sector map of DOS 3.3, ProDOS, Apple Pascal and CP/M volumes:
  the file or structure that owns each sector, each file's sectors in order,
  and the sectors that are cross-linked, allocated but unowned, or owned but
  marked free (CiderPress II, Copy II Plus, AppleCommander, A2FileCmd, Disk
  Fixer).
- Sector editing with pending changes, a choice of keeping or recomputing
  the checksum, and the physical, DOS 3.3 and ProDOS numbering shown together
  (Bag of Tricks, Copy II Plus, CiderPress II, Disk Fixer).
- Comparison of two disks by quarter track, sector, file, nibble and flux
  timing (Applesauce, Copy II Plus, a2kit).
- Identifying the protection techniques a disk uses and its boot loader
  (Passport, Applesauce).
- Weak-bit detection by comparing the revolutions of an A2R capture
  (Applesauce).
- Viewing a track at another latch framing, and finding bit-slip streams
  (Applesauce, Passport).
- Custom translate tables and data encodings, with a solver for swapped
  table entries (Applesauce, a2kit, CiderPress II).

The later release also adds two features of this spec's own: decoding
documented custom track formats into sectors, starting with RW18 (User Story
15), and comparing two quarter tracks of one disk (User Story 16).

**Out of scope:**

- Editing nibbles or flux, including inserting or deleting nibbles, marking
  nibbles as sync, normalizing flux timing and moving a track's start, and
  editing address fields, marks or image metadata. Sector editing (FR-096)
  changes decoded sector data only.
- File operations beyond the map, such as extracting, adding, deleting or
  renaming files, which Explorer and the `disk` command do for DOS 3.3 and
  ProDOS. The Apple Pascal and CP/M readers 040 adds are read-only.
- Laying down new tracks. The shared sector writer changes the data fields of
  existing sectors only; formatting a disk, as `disk create`, `disk init` and
  Explorer's "Format disk image..." do, and the guest formatting through the
  drive are unchanged.
- How Casso handles a change made outside Casso to a mounted image's file. An
  edit Explorer saves is such a change, sent with the reload intent as
  Explorer's other writes are (FR-106), and spec 041 reworks that handling.
- Disk breakpoints and a live trace of disk access. Disk breakpoints belong to
  the 035 debugger spec, which shares some of this spec's parts (Assumptions).
  The Disk ][ debug window keeps its event log of head steps, motor events and
  the fields the guest reads, and the inspector does not duplicate it.
- File maps of DOS 3.2 disks, which Casso has no reader for, of volumes other
  than 35 tracks of 16 sectors, and of file systems other than DOS 3.3,
  ProDOS, Apple Pascal and CP/M.
- Decoding a custom track format that has no published description, and
  editing the sectors of any custom track format. The later release decodes
  documented formats, one decoder each, starting with RW18 (FR-141), and the
  nibble and field views cover every other format.
- Mounting A2R captures in a drive, and writing a normalized copy of a disk
  or a copy with its protection removed.
- Bookmarks, a nibble-value histogram, read-head analog graphs, and saving
  the platter as a picture or the metadata as JSON.
- Screen-reader access, which follows GH #147 for all of Casso's custom
  windows.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See how a disk is recorded (Priority: P1)

A user opens the inspector on a disk image and sees the whole disk drawn as a
platter, one ring per quarter track, colored by what each stretch holds: sync,
address and data marks, address and data fields, failed checksums, other
valid nibbles, noise and random-bit regions. Chips across the top summarize
the disk. Selecting a track shows it unrolled as a strip, its sectors in the
order they pass the head, each sector's decoded bytes, and every nibble on the
track. This works for every image format Casso mounts, and for 13-sector
disks as well as 16-sector ones.

**Why this priority**: The analyzer and these views are what every other
story uses, so they are built first. Like every story in the first release,
it ships in the same merge as the others.

**Independent Test**: Open made-up 16-sector, 13-sector and mixed images, a
DSK, a NIB, and a WOZ with bit and flux tracks, and compare each view against
the known contents of the image.

**Acceptance Scenarios**:

1. **Given** a DOS 3.3 DSK, **When** it is opened, **Then** the chips show
   "16 sector", "35 tracks", "560/560 sectors good" and "Volume 254", every
   quarter track of tracks 0 to 34 shows its whole track's data, tracks 35 to
   39.75 show nothing recorded, the bytes of each sector on track 17 match the
   image at the logical sector the format maps it to, and Findings lists
   nothing.
2. **Given** a made-up 13-sector WOZ, **When** track 0 is selected, **Then**
   13 sectors are listed, each sector's 256 bytes are decoded from 5-and-3 and
   match the source, and each address and data checksum shows as good.
3. **Given** a track holding both 13-sector and 16-sector fields, **When** it
   is analyzed, **Then** both kinds of sector are decoded, the track is
   classified "13 and 16 sector", and the format chip shows "13 and 16
   sector".
4. **Given** a sector whose data checksum fails, **When** it is selected,
   **Then** its button and its strip label are marked bad, its bytes are still
   shown, and the sector header shows "data bad".
5. **Given** a sector that spans the end of the track, **When** the track is
   analyzed, **Then** the sector decodes whole and its outline on the strip is
   drawn in two parts.
6. **Given** a NIB image, **When** a track is selected, **Then** its stored
   nibbles are shown and the track header shows a note that the format
   stores no timing bits between nibbles.
7. **Given** a quarter track with nothing recorded, **When** it is selected,
   **Then** its ring is empty and the header shows "Nothing recorded on this
   quarter track".

---

### User Story 2 - Find what is unusual or damaged (Priority: P1)

A user investigating a copy-protected or damaged disk opens the Findings list
and sees every irregular thing on the disk with its location: failed
checksums, address track numbers that differ from the physical track,
duplicate or missing sectors, missing or nonstandard epilogues, orphan data
fields, track records on half and quarter tracks outside the standard layout,
unusual track lengths and damaged image records. Selecting a finding goes to
it. The Tracks table shows all 160 quarter tracks at once, the Fields table
shows every mark and checksum on a track, and the nibble view shows each
nibble's hidden timing bits. Unformatted tracks and nonstandard tracks are
kept separate. When a disk uses its own marks, the user enters them in the
decode settings and its sectors decode.

**Why this priority**: Finding the protection or the damage is the reason to
open an inspector. These views use the same analysis as User Story 1 and are
built right after it.

**Independent Test**: Open a made-up image with one of each planted anomaly
and confirm the findings, the Tracks table and the Fields table against the
list of what was planted.

**Acceptance Scenarios**:

1. **Given** a made-up image with one of each planted anomaly, **When** it is
   opened, **Then** Findings lists each anomaly once with its track and
   sector, and selecting a finding selects that track, sector and field.
2. **Given** a track on which no address field passes its checksum, **When**
   it is analyzed, **Then** the track is classified nonstandard, its fields
   count toward neither sectors good nor sectors found, the nonstandard-tracks
   chip counts the track, and the Fields table still lists the fields with
   stored and computed checksums.
3. **Given** a WOZ in which quarter tracks 1.25 to 1.75 map to one track
   record, **When** the Tracks table is shown, **Then** those three rows show
   that they share one record, and Findings lists that record once, with the
   quarter tracks it covers.
4. **Given** a track of random bits and a track of valid nibbles with no
   standard fields, **When** they are analyzed, **Then** the first is
   classified unformatted and the second nonstandard.
5. **Given** a disk whose address prologue is D4 AA 96 on every track except
   track 0, **When** the user sets the 16-sector address prologue to D4 AA 96
   in the decode settings, **Then** the disk's sectors decode, track 0, which
   uses the standard marks, still decodes, a chip shows that nonstandard
   decode settings are in use, and "Reset to standard" returns to the
   standard settings.
6. **Given** a track with 10-cell sync nibbles, **When** the Nibbles tab is
   shown, **Then** each sync nibble shows its width, and a nibble followed by
   extra zero cells shows how many.
7. **Given** a WOZ with damaged tracks, **When** it is opened, **Then** the
   damaged quarter tracks show as damaged, with the reason, in the platter,
   the Tracks table, Findings and the Image tab, and never as blank tracks.
8. **Given** the data checksum check turned off in the decode settings,
   **When** a 16-sector track is analyzed, **Then** its sectors show as "not
   checked", count as found but not as good, the sectors-good chip gives the
   number not checked, and the Fields tab still shows the stored and computed
   data checksums.

---

### User Story 3 - Read flux timing (Priority: P2)

A user looking at a flux disk such as *Bandits* switches the platter to Timing
mode and sees which stretches of each track were written fast or slow. On the
selected track, a timing line on the strip and the Flux timing tab show each
transition at its recorded time, an interval histogram, and the transitions
behind the selected nibbles.

**Why this priority**: Flux timing is what GH #159 is about, and only flux
tracks have it. It is built after the structure views it overlays.

**Independent Test**: Open a made-up flux track that mixes 3.7 µs and 4.1 µs
cells and check the colors, the tooltip values, the interval plot and the
histogram against the cell lengths written into it.

**Acceptance Scenarios**:

1. **Given** a made-up flux track with stretches of 3.7 µs and 4.1 µs cells,
   **When** Timing mode is on, **Then** the stretches are drawn as fast and
   slow, and the tooltip over each shows its cell time and its deviation from
   the 3.91 µs nominal (about -5% and +5%).
2. **Given** the same track, **When** the Flux timing tab is shown, **Then**
   every transition is plotted at its recorded interval, not rounded to
   cells, and the histogram has its peaks at the intervals written into it.
3. **Given** a selected sector and "Selection" chosen for the histogram,
   **When** the Flux timing tab is shown, **Then** the sector's transitions
   are highlighted in the plot and the histogram covers only that sector.
4. **Given** the timing range set to ±2%, **When** the platter is drawn,
   **Then** the legend scale shows ±2% and colors reach full strength at ±2%.
5. **Given** a bit track, **When** Timing mode is on, **Then** it is drawn
   dimmed, and its Flux timing tab shows a note that the track is stored as
   bit cells of one length.
6. **Given** the strip zoomed until each cell shows its own 1 or 0, **When**
   Timing mode is on, **Then** each cell shows its own timing, not an average
   over its neighbors.

---

### User Story 4 - Zoom, navigate, search and copy (Priority: P2)

A user zooms the platter from the whole disk down to single cells, zooms and
pans the strip, moves between tracks and sectors from the keyboard, goes to a
given track, sector, DOS 3.3 logical sector, ProDOS block, nibble or cell,
searches the track or the whole disk for a byte pattern, selects a range of
nibbles or bytes, and copies what they selected.

**Why this priority**: Without zoom, navigation, search and copy, each view
shows only the whole disk or the whole track. They are built right after the
P1 views they control.

**Independent Test**: Use only the keyboard to reach every view and command;
then zoom, search and copy on a made-up image and compare the results with
its known contents.

**Acceptance Scenarios**:

1. **Given** the platter at fit, **When** the user zooms in to 600× around a
   point, **Then** the point under the pointer stays put, nibble values appear
   along the rings once each fits inside its nibble's arc, then a tick for
   each 1 cell once ticks for adjacent cells no longer touch, and "Fit"
   returns to the whole disk.
2. **Given** the strip zoomed in on track 17, **When** the user selects track
   18, **Then** the strip keeps its zoom and the same fraction of the turn in
   view.
3. **Given** keyboard focus on the platter with track 17.25 selected,
   **When** the user presses each of Up, Down, Page Up and Page Down from
   there, **Then** Up selects 17, Down selects 17.5, Page Up selects 17 and
   Page Down selects 18; **When** the user presses Left and Right, **Then**
   the previous and next sector in passing order are selected.
4. **Given** a disk, **When** the user goes to sector $5 on track 17,
   **Then** that track and sector are selected and shown in every view.
5. **Given** a DOS 3.3 disk, **When** the user goes to DOS 3.3 logical sector
   $1 on track 17, **Then** the physical sector that holds it under the
   standard skew is selected, and the Sector data header shows logical sector
   $1 and the ProDOS block and half that the sector holds.
6. **Given** a disk, **When** the user searches the whole disk for D5 AA 96
   with "Any bit offset" on, **Then** the results list every match, including
   matches not aligned to the nibbles as framed.
7. **Given** a disk, **When** the user searches decoded sector data for
   20 00 BF, **Then** the results list every sector that holds those bytes,
   with the offset of each match.
8. **Given** a range of nibbles selected with Shift+drag in the strip,
   **When** the user copies, **Then** the clipboard holds them as hex text,
   and the readout shows the range's length in nibbles and cells.
9. **Given** the Nibbles tab, **When** the user clicks a nibble, **Then** the
   strip and the platter show that nibble.

---

### User Story 5 - Check the image file itself (Priority: P2)

A user who suspects the image file rather than the disk opens the Image tab
and sees what the file holds: its format and size, and for a WOZ every INFO
field, every META entry, the quarter-track maps, each track record's location
and length, its checksum result, and any damage found while loading it.

**Why this priority**: A damaged image file and a damaged disk look the same
in the track views, so the inspector shows the file's own structure
separately. It uses only the file's own data, so it can be built alongside
the other views.

**Independent Test**: Open made-up WOZ 1, WOZ 2 and WOZ 2.1 files with known
INFO, META and maps, one with a bad checksum, and one with damaged records,
and compare the Image tab with the files.

**Acceptance Scenarios**:

1. **Given** a WOZ 2.1, **When** the Image tab is shown, **Then** it lists
   every INFO field of that version, every META entry in file order, the TMAP
   and FLUX maps, and each track record's start block, block count and bit
   or byte count.
2. **Given** a WOZ 1, **When** the Image tab is shown, **Then** it lists each
   track record's bytes used, bit count, splice point, splice nibble and
   splice bit count.
3. **Given** a WOZ whose stored checksum does not match its contents,
   **When** it is opened, **Then** the Image tab and Findings both show the
   mismatch.
4. **Given** a WOZ with a track record that no map refers to, **When** the
   Image tab is shown, **Then** that record is listed as unused.
5. **Given** a WOZ whose INFO disk type is 3.5", **When** it is opened,
   **Then** the Image tab is shown and every other view shows a note that
   Casso does not analyze 3.5" disks.
6. **Given** a DSK, **When** the Image tab is shown, **Then** it shows the
   format, sector order and size, and a note that Casso builds its tracks
   from sector data with volume 254.

---

### User Story 6 - Watch the drive work in Casso (Priority: P3)

A user running a program in Casso opens the inspector on drive 1. The platter
turns as the emulated disk turns, the head marker sits over the head's
quarter track and shows whether the drive is reading or writing, and with
Follow head on the views follow the head from track to track. When the
program writes, the tracks it wrote are analyzed again and the views update,
while the rest of the disk is left alone.

**Why this priority**: It depends on the finished views. It is built before
the Explorer hosts because it does not depend on 033.

**Independent Test**: Boot a DOS 3.3 disk with the inspector open on drive 1
and Follow head on, save a file from BASIC, and confirm the head marker, the
written tracks and the emulation speed.

**Acceptance Scenarios**:

1. **Given** a running machine, **When** the user chooses "Inspect disk 1..."
   from the Storage menu, **Then** the inspector opens on drive 1; **When**
   the user then chooses "Inspect disk 2..." from drive 2's right-click menu
   in the desk scene, **Then** the same window switches to drive 2.
2. **Given** the inspector on drive 1 while the disk boots, **When** the
   drive reads, **Then** the platter turns, the head marker points at the
   head's quarter track, and the marker shows reading.
3. **Given** Follow head on, **When** the head steps and stays on a quarter
   track for about 0.3 s, **Then** that track is selected; **When** the user
   selects a track by hand, **Then** Follow head turns off; **When** the user
   presses "Go to head", **Then** the head's track is selected once.
4. **Given** the guest saves a file, **When** the drive writes, **Then** only
   the written tracks are analyzed again, the views show their new contents
   within 500 ms, the head marker shows writing, and the Tracks table marks
   those tracks as written since the disk was inserted.
5. **Given** the inspector on drive 1, **When** the disk is ejected, **Then**
   the window shows "No disk in drive 1"; **When** a disk is inserted,
   **Then** it is analyzed and shown.
6. **Given** the platter zoomed in while the disk turns, **When** the drive
   reads, **Then** the platter holds still and the head marker moves around
   it instead.
7. **Given** a write-protected disk in drive 1, **When** the guest writes to
   it, **Then** the head marker shows write blocked, the window
   shows that the disk is write-protected and why, and no track is analyzed
   again or marked as written.
8. **Given** the disk has booted, **When** the Tracks tab is shown, **Then**
   each quarter track the drive read is marked with its number of visits, and
   the "Reads" overlay marks the same quarter tracks on the platter.
9. **Given** a WOZ in drive 1 whose file holds no track record for track 20,
   **When** the inspector shows the disk, **Then** track 20 shows "Nothing
   recorded" and the Image tab lists no record for it, although Casso keeps
   an empty track there for the guest to format; **When** the guest formats
   the disk, **Then** track 20 shows the track the guest wrote, marked as
   written since the disk was inserted, and the Image tab shows it as added
   since the file was read.

---

### User Story 7 - Inspect a disk image from Casso Explorer (Priority: P7)

A user browsing disk images in Casso Explorer selects one and sees a small
picture of the disk and its summary chips in the preview pane beside the
catalog. Right-clicking the image and choosing "Inspect disk image" opens the
inspector window for that file.

**Why this priority**: It depends on 033, which is not yet on master, so it
is built last in the first release, together with the parts of User Stories
8 to 10 that need Explorer or 033.

**Independent Test**: In Casso Explorer, select DOS 3.3, ProDOS, protected
and damaged images, check the preview, open each one with "Inspect disk
image", and switch between the Light and Dark themes.

**Acceptance Scenarios**:

1. **Given** a folder of disk images, **When** the user selects one, **Then**
   the preview pane shows a small still platter and the summary chips with
   the image's catalog or details, and the catalog appears no later than it
   did before this feature.
2. **Given** a selected image, **When** the user chooses "Inspect disk image"
   from its right-click menu, **Then** the inspector window opens on that
   file with none of the Casso-only elements in FR-001.
3. **Given** an inspector window open on a file, **When** the file changes on
   disk, **Then** the window analyzes the new contents and keeps the selected
   track.
4. **Given** an image that fails to open, **When** the user chooses "Inspect
   disk image", **Then** the window shows why it could not be opened, not a
   blank disk.
5. **Given** the inspector and the preview, **When** the user switches
   Explorer between its Light and Dark themes, **Then** every view redraws in
   the new theme without being reopened.

---

### User Story 8 - See which file owns each sector (Priority: P4)

A user with a DOS 3.3, ProDOS, Apple Pascal or CP/M disk opens the File map
tab and sees a grid of every sector or block, each shown by its role: the boot
or system area, the VTOC or directory, the catalog, the bitmap, each file's
track/sector lists or index blocks and its data, free space, and the conflicts
a damaged volume has. The file list below the grid shows each file with its
sectors and whether any of them is bad. Selecting a file marks its sectors in
file order in the grid, on the platter and on the strip; selecting a sector
anywhere shows the file it belongs to. Damage in the catalog or in a file's
chain shows as findings, never as a short catalog.

**Why this priority**: It reads the sectors the analyzer decodes and is shown
in the views, so it is built after them and the Casso host. It comes before
sector editing, which shows each sector's owner, and comparison, which
compares files through it. ProDOS subdirectories are built after the rebase
onto 033 (Delivery). Casso has no Apple Pascal or CP/M reader, so 040 adds
read-only readers sufficient for the map. It ships in the first release.

**Independent Test**: Open made-up DOS 3.3, ProDOS, Apple Pascal and CP/M
images with known layouts and made-up damaged ones, and compare the grid, the
file list and Findings with the layouts and the planted damage.

**Acceptance Scenarios**:

1. **Given** a standard DOS 3.3 disk holding files, **When** the File map tab
   is shown, **Then** tracks 0 to 2 show as boot and DOS image, track 17
   sector $0 as the VTOC, the catalog sectors as catalog, each file's
   track/sector list and data sectors as that file's, the rest as free, and
   Findings lists nothing.
2. **Given** a DOS 3.3 file with three track/sector lists, **When** it is
   selected in the file list, **Then** its sectors are numbered in file order
   in the grid, each list followed by the data sectors it lists, and "Next
   sector in file" steps through them across tracks.
3. **Given** a ProDOS disk with a tree file holding a sparse hole and a
   subdirectory holding files, **When** the File map tab is shown, **Then**
   the tree file's master index, index and data blocks, the subdirectory's
   blocks and the blocks of the files in it each show their owner, the hole
   shows at its place in the tree file's order, and Findings lists nothing.
4. **Given** two files that use the same sector, **When** the disk is opened,
   **Then** the sector shows as cross-linked, its tooltip lists both files,
   and Findings lists the cross-link once with both owners.
5. **Given** a track/sector list that links back to itself, **When** the disk
   is opened, **Then** the map appears within 100 ms of the analysis finishing
   (SC-015), the file shows the sectors read before the loop and the state
   "Chain broken", and Findings gives the file, the sector where the loop
   starts and the reason.
6. **Given** a catalog sector with a bad data checksum, **When** the File map
   tab is shown, **Then** the file list shows that the catalog is incomplete
   and which sector could not be read, Findings lists it, and the files in
   the other catalog sectors are still listed.
7. **Given** a file with a bad data sector, **When** the file list is
   filtered to "Files touching bad sectors", **Then** only that file is
   listed, and its bad sector shows the bad symbol in the grid.
8. **Given** a sector selected on the platter, **When** the Sector data tab
   is shown, **Then** its header shows the sector's role and its file with
   its place in the file, for example "HELLO, data sector 3 of 5".
9. **Given** a 13-sector disk, or a disk with no DOS 3.3, ProDOS, Apple
   Pascal or CP/M file system, **When** the File map tab is shown, **Then** it
   shows why there is no map, and the grid still shows each sector's result.
10. **Given** a deleted file whose sectors are still free, **When** "Show
    deleted files" is turned on, **Then** the file is listed as deleted with
    how many of its sectors are still free, and no finding is listed for it.
11. **Given** a DOS 3.3 disk with the data checksum check turned off in the
    decode settings, **When** the File map tab is shown, **Then** the map is
    built and every file's chain is followed, no file shows "Touches bad
    sectors", and Findings lists one finding for the disk: the map was built
    from sectors not checked.
12. **Given** a disk that holds both a DOS 3.3 VTOC and a ProDOS volume
    directory, **When** the File map tab is shown, **Then** both volumes are
    mapped and the tab offers a choice between them.
13. **Given** an Apple Pascal disk holding files with a gap between two of
    them, **When** the File map tab is shown, **Then** blocks 0 and 1 show as
    boot blocks, blocks 2 to 5 as the directory, each file's run of blocks as
    that file's, the gap and the blocks after the last file as free, the file
    list gives each file's name, type and size, and Findings lists nothing.
14. **Given** a CP/M disk holding a file of two extents, a file in user area
    5 and a deleted file, **When** the File map tab is shown, **Then** tracks
    0 to 2 show as the system area, the directory's blocks as the directory,
    each file's blocks as that file's in extent order, the file in user area 5
    is listed with its user number, the deleted file is listed only once "Show
    deleted files" is on, and Findings lists nothing.
15. **Given** two Apple Pascal directory entries whose runs of blocks
    overlap, **When** the disk is opened, **Then** the shared blocks show as
    cross-linked with both files as owners, and Findings lists the overlap
    once.
16. **Given** a ProDOS disk, a DOS 3.3 disk and a disk filled with zeros,
    **When** each is opened, **Then** none of them is mapped as an Apple
    Pascal or CP/M volume.

---

### User Story 9 - Edit a sector (Priority: P5)

A user repairing a disk or changing a program selects a sector, chooses "Edit
sector" and changes its bytes in the hex or text column. The changes stay
pending, marked byte by byte, until the user applies them. Casso then rewrites
only that sector's data field, in the encoding it was read with and, unless
the user keeps the stored checksum, with a new checksum, and leaves every
other cell of the track as it was, so a flux track stays a flux track and a
protected track keeps its timing. In Casso the edit reaches the disk in the
drive and its file; in Explorer, the file. An applied edit can be undone
exactly. A write-protected disk is never edited in place, and the user can
save an edited copy instead. The `disk` command and Casso Explorer change
sectors through the same shared sector writer, so a file they write to a disk
also leaves the address fields, the sync, the track lengths and, on a flux
track, the recorded timing as they were.

**Why this priority**: It builds on the views and the map, and editing a disk
in a drive needs the Casso host, so it follows them. The shared sector writer
and the move of the `disk` command's writes onto it are built with it.
Editing in Explorer, and the move of Explorer's writes onto the shared
writer, are built with the Explorer hosts, after the rebase onto 033. It
ships in the first release.

**Independent Test**: In each image format, on bit and flux tracks, and on
made-up 16-sector and 13-sector disks, edit one sector and apply, and confirm
that the sector decodes to the new bytes while every cell or flux transition
outside its data field is unchanged; then undo and confirm the track and the
file are as they were. In Casso, boot DOS 3.3, edit a sector of a file and
read the file back in the guest. With the `disk` command and Explorer, write
sectors and files to images in the standard WOZ layout, with a volume other
than 254 and with flux tracks, and confirm the same: only the data fields
written change.

**Acceptance Scenarios**:

1. **Given** a DOS 3.3 WOZ 2 with bit tracks in the standard layout, **When**
   the user changes two bytes of sector $5 on track 17 and applies, **Then**
   the sector decodes to the new bytes with a good data checksum, every cell
   of the track outside the data field's body and checksum is unchanged, the
   track keeps its length, and the saved file holds the change.
2. **Given** a sector on a flux track, **When** it is edited and applied,
   **Then** the track is still a flux track, the time of one turn and every
   transition outside the data field are unchanged, and the sector decodes to
   the new bytes.
3. **Given** a good sector whose byte $FF is $00, on a 16-sector track of a
   WOZ or NIB image, **When** the user changes byte $FF to $FC and turns off
   "Recompute the checksum", **Then** the header shows that the sector will
   read as bad once applied, and after applying, the stored checksum nibble is
   unchanged and the sector reads as bad; **When** the user instead changes
   only byte $00, **Then** the header shows that the sector will read as good.
4. **Given** a sector whose data checksum fails, **When** the user edits it
   and applies with the checksum recomputed, **Then** the sector reads as
   good.
5. **Given** a DSK, **When** the user edits a sector, **Then** "Recompute the
   checksum" is on and unavailable, its tooltip shows that sector images store
   only sector data, and once the edit is applied the file is saved as a DSK
   holding the new bytes.
6. **Given** a sector with no data field, or whose address checksum fails,
   **When** it is selected, **Then** "Edit sector" is unavailable and the
   Sector data tab shows why.
7. **Given** a write-protected disk in drive 1, **When** the user edits a
   sector, **Then** "Apply" is unavailable, the window shows each cause, and
   "Save edited copy..." writes a new file holding the edit while the disk in
   the drive and its file stay unchanged.
8. **Given** a running machine with drive 1 turning, **When** the user
   confirms an apply, **Then** the window shows "Waiting for the drive to
   stop" and applies the edit once the drive stops turning; **When** the user
   chooses "Pause and apply" instead, **Then** the machine runs until the
   drive is not writing and the head is outside the edited field, pauses,
   applies and saves the edit, and runs again.
9. **Given** an applied edit, **When** the user chooses "Undo applied edit",
   **Then** the track record is restored exactly and the file is saved with
   it; **When** the guest has written to that track since the edit, **Then**
   "Undo applied edit" is unavailable and the window shows why.
10. **Given** an undone edit, **When** the user chooses "Redo applied edit",
    **Then** the track record is as it was after the apply and the file is
    saved with it; **When** the guest has written to that track since the
    undo, **Then** "Redo applied edit" is unavailable and the window shows
    why.
11. **Given** a disk whose file is on a full volume, **When** the user applies
    an edit and the save fails, **Then** every changed track record is
    restored to what it was before the apply while the machine is still
    paused, the edits stay pending, and the window shows "Not applied: the
    file could not be saved" with the reason.
12. **Given** a booted DOS 3.3 disk in Casso, **When** the user edits a sector
    of a text file and the guest then reads the file, **Then** the guest reads
    the new bytes.
13. **Given** an image in Explorer that a running Casso has in drive 1, with
    or without guest writes Casso has not yet saved, **When** the user applies
    an edit in Explorer's inspector, **Then** the file is saved, Casso handles
    the change exactly as it handles Explorer writing the same bytes through
    its own file commands, Casso's inspector on drive 1 shows the file the
    drive holds afterward, and when Casso's reply is anything other than a
    reload, Explorer's inspector shows it.
14. **Given** a sector whose data field spans the end of the track, **When**
    it is edited and applied, **Then** it is written whole and decodes to the
    new bytes.
15. **Given** a track with two sectors numbered $5, **When** the second is
    selected, edited and applied, **Then** only the second changes.
16. **Given** a copy of a WOZ in the standard layout, in which quarter tracks
    N-0.25, N and N+0.25 map to one record, with volume 130 in every address
    field, **When** `disk sectorwrite` writes logical sector $3 of track 20,
    **Then** the write succeeds, only that sector's data field changes, and
    track 20 keeps volume 130 in every address field, its length in cells, its
    sync runs and the place of every sector on it.
17. **Given** the same image, **When** `disk put` adds a file, **Then** the
    write succeeds, every track it changed keeps its length, its sync runs and
    volume 130, and every other track record is unchanged.
18. **Given** a sector on a flux track, **When** `disk sectorwrite` writes it,
    **Then** each cell of the new field takes the time the cell at the same
    place took and the time of one turn is unchanged; **When** the guest then
    writes that sector through the drive, **Then** the guest's field is
    written at the drive's own cell timing.
19. **Given** a WOZ whose track 13 is stored in record 14, **When** `disk
    sectorwrite` writes a sector of track 13, **Then** record 14 changes, no
    other record does, and `disk sectorread` of that sector then returns the
    new bytes.
20. **Given** a track on which one sector's data checksum fails, **When**
    `disk put` needs to change a sector on that track, **Then** the write
    fails with the track and the reason, and the file is unchanged; **When**
    the inspector edits the bad sector and applies, **Then** the edit is
    applied (FR-096).
21. **Given** a track with no standard address field, unformatted or in a
    format the decoder does not read, **When** `disk sectorwrite` writes a
    sector of it, **Then** the write fails with the track and the reason, no
    track is formatted, and the file is unchanged.
22. **Given** a WOZ whose quarter track 2.5 maps to a record of its own,
    **When** `disk sectorwrite` writes a sector of any track, **Then** the
    write fails, the message gives quarter track 2.5 and its record, and the
    file is unchanged.
23. **Given** a `disk` command write whose save fails after the new contents
    are written and before they replace the file, **When** the command ends,
    **Then** the file is byte-for-byte as it was.
24. **Given** a WOZ in the standard layout in Explorer, **When** Explorer
    copies a file onto it, **Then** the write succeeds and only the data
    fields of the sectors written change.

---

### User Story 10 - Compare two disks (Priority: P6)

A user compares the disk in the inspector with another: a second image in any
format, the same disk before and after the guest wrote to it, the disk in the
drive with its file, or the file Casso saved with the file as it was loaded.
Each whole track, and each quarter track outside the standard layouts, gets a
verdict, from identical to different, and every difference is listed, marked
on the platter and the strip, shown byte by byte in the sectors, and reached
with "Next difference". When both disks hold a volume the file map reads
(DOS 3.3, ProDOS, Apple Pascal or CP/M), their files are compared too.

**Why this priority**: It runs the analyzer on both disks and compares files
through the map, and its sources from the drives need the Casso host, so it
is built after them. Comparing in Explorer is built with the Explorer hosts.
It ships in the first release.

**Independent Test**: Compare made-up pairs with planted differences and check
every verdict and listed difference against what was planted.

**Acceptance Scenarios**:

1. **Given** two copies of one image, **When** they are compared, **Then**
   every track that holds a record shows "Identical", every quarter track with
   nothing recorded on either side shows "Nothing recorded", and the
   Differences tab shows "No differences".
2. **Given** a DSK and a WOZ in the standard layout of one made-up disk,
   **When** they are compared, **Then** each whole track is compared once, the
   quarter tracks between whole tracks show "Standard layout" and are not
   counted, no formatted track shows a verdict weaker than "Same sector data",
   and no sector differs.
3. **Given** B with one byte changed in sector $5 on track 17, **When** the
   disks are compared, **Then** track 17 shows "Sectors differ" with one
   sector, the Sector data tab marks the byte on both sides, and "Next
   difference" goes to it.
4. **Given** B whose track 3 starts 90° later and holds four more sync nibbles
   before sector $0, **When** the disks are compared, **Then** track 3 shows
   "Same nibbles" with a rotation of 90° and the length difference in cells,
   and, with the comparison options at their defaults, only the longer sync
   run is marked as different.
5. **Given** a running machine, **When** the guest saves a file and the user
   compares the disk now with the disk as inserted, **Then** only the tracks
   the guest wrote differ, and the file comparison lists the new file as only
   in A.
6. **Given** a WOZ 1 in drive 1 that Casso has saved after a guest write,
   **When** its file is compared with the disk as inserted, **Then** only the
   tracks the guest wrote differ.
7. **Given** two flux tracks with the same cells, one with a stretch written
   4% slower, **When** they are compared, **Then** the track shows "Same
   cells" with a note that the flux timing differs, and the slow stretch is
   marked.
8. **Given** B with no record at track 35, **When** the disks are compared,
   **Then** track 35 shows "Only in A".
9. **Given** a damaged record in B, **When** the disks are compared, **Then**
   its quarter tracks show "Not compared" with the reason, and the summary
   does not count them as matching.
10. **Given** two images selected in Explorer, **When** the user chooses
    "Compare disk images", **Then** one inspector window opens comparing
    them.
11. **Given** a sector that differs, **When** the user chooses "Use B's
    bytes", **Then** B's bytes become a pending edit of A's sector.
12. **Given** two copies of a disk whose address prologue is D4 AA 96, with
    the window's decode settings matching it, **When** they are compared,
    **Then** B is analyzed with the same decode settings and every track that
    holds a record shows "Identical".
13. **Given** A and B that differ only in the volume number of every address
    field, **When** they are compared, **Then** every formatted track shows
    "Same sector data" and the Differences tab lists the volume numbers;
    **When** "Ignore volume numbers" is turned on, **Then** the Differences
    tab shows "No differences" and the verdicts do not change.
14. **Given** a bit track and a flux track with the same cells from the same
    start, **When** they are compared, **Then** the track shows "Same cells"
    with a note that only one side records flux timing.

---

### User Story 11 - Decode data with a disk's own translate table (Priority: P8)

A preservationist with a disk whose data fields use their own nibble
translate table, such as one that swaps D5 and 9B, sees a finding that the
data fields hold nibbles outside the standard table, chooses "Solve table",
and gets the swap under which the checksums pass. The user can also paste a
whole table, read one from the disk's own boot code, choose another encoding
for a range of tracks, and save the settings to use on other disks of the
same title.

**Why this priority**: It ships in the later release. It extends the decode
settings, and protection identification (User Story 13) reports its
findings, so it is built first in that release.

**Independent Test**: Write made-up disks with one swapped pair, with a whole
permuted table, with 5-and-3 data and with 4-and-4 data, and check the
finding, "Solve table", a pasted table and the decoded bytes.

**Acceptance Scenarios**:

1. **Given** a made-up disk whose data fields use the standard table with D5
   and 9B swapped, **When** it is opened, **Then** Findings gives each track
   with the nibbles outside the 6-and-2 table and their counts, and its
   sectors read as bad.
2. **Given** the same disk, **When** the user chooses "Solve table", **Then**
   the result is exactly the D5 and 9B swap, every sector passes with it, and
   once the user confirms, every sector decodes to the source bytes.
3. **Given** a disk whose table is a permutation of the standard entries,
   **When** it is opened, **Then** no nibble is outside the table and the
   data checksums fail; **When** the user pastes the table, **Then** every
   sector decodes and passes.
4. **Given** a table with a repeated entry, **When** it is entered, **Then**
   the decode settings show the repeated entry and do not accept the table.
5. **Given** a disk whose boot code holds its table, **When** the user reads
   the table from its track, sector and offset, **Then** the decode settings
   use it and the sectors decode.
6. **Given** decode settings saved from one disk, **When** they are loaded
   for another disk with the same format, **Then** that disk decodes with
   them.
7. **Given** a sector decoded with a custom table, **When** it is edited and
   applied, **Then** it is encoded with that table and decodes to the new
   bytes.

---

### User Story 12 - View a track at another latch framing (Priority: P9)

A preservationist examining a bit-slip protection, such as an E7 stream,
sees a finding where slipping the framing a few cells gives a different run
of valid nibbles. Selecting it shows the slipped nibbles in a second lane
under the normal ones, lined up in time, up to the place where the two
framings rejoin. The user can also slip the framing after any nibble, or
start framing at any cell.

**Why this priority**: It ships in the later release. It adds a view and a
finding over the nibbles the analyzer already frames, and protection
identification reports its finding, so it is built before that story.

**Independent Test**: Build a made-up bit track holding D5 and E7 nibbles with
0, 1 or 2 extra zero cells, laid out as an E7 stream, and a track with the
same nibbles and no extra zero cells, and check the lane, the scan and Find
against the expected slipped nibbles.

**Acceptance Scenarios**:

1. **Given** the made-up E7 track, **When** it is analyzed, **Then** Findings
   lists one bit-slip finding with the normal nibbles D5 E7 E7 E7 and the
   slipped nibbles EE E7 FC EE E7 FC EE EE FC for a slip of 3 or 4 cells.
2. **Given** the same nibbles with no extra zero cells, **When** they are
   analyzed, **Then** no bit-slip finding is listed.
3. **Given** the finding selected, **When** the Nibbles tab is shown, **Then**
   the slipped framing shows in the second lane, lined up in time with the
   normal one, with the place where they rejoin marked.
4. **Given** any nibble, **When** the user chooses "Slip after this nibble"
   by 2 cells, **Then** the second lane shows that framing, and the sectors,
   the Fields tab and Findings do not change.
5. **Given** a standard DOS 3.3 disk, **When** it is analyzed, **Then** no
   bit-slip finding is listed.
6. **Given** "Only where the framing slips" on, **When** the user searches
   the E7 track for EE E7 FC, **Then** the match at the slipped framing is
   listed and nothing at the normal framing is.

---

### User Story 13 - Identify the copy protection (Priority: P12)

A preservationist opens a protected disk and sees a verdict chip, "Protection
found", and a Protection category in Findings giving each technique the disk
uses, with its evidence, a confidence, and what a copy must keep for the disk
to work. The boot loader is identified from its boot sector when its family is
in the pattern table, and "Trace boot" finds the disk's own read routine when
the loader is encrypted. The Image tab lists the findings that the image's
format cannot hold.

**Why this priority**: It ships in the later release. It gathers the evidence
of every other analysis except weak bits, including the translate table and
bit-slip findings, the custom track formats and the comparison of adjacent
quarter tracks, so it follows them; User Story 14 adds the weak-bit Protection
finding (FR-132).

**Independent Test**: Build made-up disks with one technique each, a standard
disk, a track of random noise, and a made-up boot loader in a made-up boot
sector, and check the Protection findings and the verdict chip against what
was planted.

**Acceptance Scenarios**:

1. **Given** a made-up disk with one technique, **When** it is opened,
   **Then** the Protection category lists that technique exactly once with
   its evidence, confidence and what a copy must keep, and the verdict
   chip shows "Protection found".
2. **Given** a standard DOS 3.3 disk, **When** it is opened, **Then** the
   Protection category is empty and the verdict chip shows "Standard".
3. **Given** a track of random noise among standard tracks, **When** the disk
   is opened, **Then** the track is classified unformatted and produces no
   Protection finding.
4. **Given** a made-up boot sector that matches a pattern in a test pattern
   table, **When** the disk is opened, **Then** the boot loader's family is
   shown with the pattern's location.
5. **Given** a DOS 3.3 disk whose read routine on track 0 reads address
   prologue D4 AA 96, **When** it is opened, **Then** a Protection finding
   gives that prologue and the place it was read from.
6. **Given** a made-up boot loader that decrypts a modified read routine into
   memory, **When** the user chooses "Trace boot", **Then** a Protection
   finding gives the marks that routine reads, and the disk, its file and any
   running machine are unchanged.
7. **Given** a WOZ with timing-bit findings, **When** the Image tab is shown,
   **Then** it lists those findings as lost in a DSK or NIB copy.

---

### User Story 14 - Find weak bits in a flux capture (Priority: P13)

A preservationist opens an A2R capture, which holds several revolutions of
each track, and sees each quarter track's revolutions, a stability lane giving
how many revolutions hold the same value at each cell, and a finding for each
region where they differ: weak bits, which read differently every time, or
unstable cells, which worn media also produce. A WOZ holds one revolution, so
for a WOZ the inspector shows the cleaned flag and where the drive reads
random bits.

**Why this priority**: It ships in the later release. It needs Casso to read
A2R captures, the largest new piece of that release, so it is built last, and
it adds the weak-bit entry to the Protection category (FR-132).

**Independent Test**: Write made-up A2R 2 and A2R 3 captures of a known
DOS 3.3 track with seeded jitter, one holding a region that each revolution
fills with different random transitions, and check the findings, the stability
lane and how the revolutions were split.

**Acceptance Scenarios**:

1. **Given** a made-up A2R 3 capture with a 40-cell weak region on track 5
   over 5 revolutions, **When** it is opened, **Then** Findings lists exactly
   one "Weak bits" region on track 5, within one cell of its place, with its
   length, the revolutions compared and whether it lies in a field or a gap.
2. **Given** the same track with jitter only, **When** it is opened, **Then**
   no region is listed.
3. **Given** a quarter track with one revolution, **When** it is opened,
   **Then** it shows "Not compared" with the reason, not as stable.
4. **Given** a capture with no index signals, **When** it is opened, **Then**
   each revolution's start is found by matching the track against itself,
   and the findings match those of the same capture with index signals.
5. **Given** the same data written as A2R 2 at 125 ns and as A2R 3 at 125 ns,
   62.5 ns and 25 ns, **When** each is opened, **Then** the findings are
   identical.
6. **Given** a capture with several revolutions, **When** the user chooses
   another revolution, **Then** every track view shows it, and the
   "Revolutions" overlay marks where the revolutions differ.
7. **Given** a capture of a 3.5" disk, **When** it is opened, **Then** the
   window shows that Casso analyzes only 5.25" captures.
8. **Given** a WOZ with its cleaned flag set, **When** the Image tab is shown,
   **Then** the flag is shown with its meaning.
9. **Given** an A2R capture, **When** the Image tab is shown, **Then** it
   lists the capture's version, its INFO fields, its resolution, its META
   entries, whether it holds an SLVD chunk, and its chunks.
10. **Given** an A2R capture with a truncated chunk, **When** it is opened,
    **Then** Findings lists the truncated chunk.

---

### User Story 15 - Decode a custom track format (Priority: P10)

A preservationist opens a disk whose tracks use a documented format of their
own, such as RW18, which Roland Gustafsson wrote for Brøderbund and which
*Prince of Persia* uses: six sectors of 768 bytes on each track, holding 18
pages of 256 bytes. In the first release those tracks show as nonstandard.
With the RW18 decoder, the inspector recognizes them, decodes each track into
its 18 pages with each sector's checksum checked, and shows the pages in the
Sector data tab, while the nibble and field views still show everything on the
track. Each documented format gets its own decoder, and a format with no
decoder is still shown by the nibble and field views.

**Why this priority**: It ships in the later release. It adds decoders beside
the analyzer's standard ones, and protection identification (User Story 13)
reports the formats it finds, so it is built before that story.

**Independent Test**: Write a made-up disk with a standard 16-sector track 0
and RW18 tracks 1 to 34, using an encoder written for the test from the
format's description, plant one fault on each of several tracks, and check
the decoded pages, the classification and the findings against what was
written and planted.

**Acceptance Scenarios**:

1. **Given** the made-up RW18 disk, **When** it is opened, **Then** track 0
   is classified "16 sector", tracks 1 to 34 "RW18", each RW18 track shows six
   sectors with good checksums, and each track's 18 pages match the source
   byte for byte.
2. **Given** an RW18 sector with one data nibble changed to another valid
   nibble, **When** its track is analyzed, **Then** the sector is marked bad
   and Findings lists the sector once with the three pages it holds.
3. **Given** an RW18 sector with one nibble outside the table, **When** its
   track is analyzed, **Then** the Sector data tab marks the bytes that nibble
   affects and Findings lists it once.
4. **Given** an RW18 address field whose check value fails, **When** its track
   is analyzed, **Then** Findings lists that address field once at its place
   and does not list its sector as missing.
5. **Given** an RW18 track with one sector missing, **When** it is analyzed,
   **Then** Findings lists the missing sector and the pages it holds, and the
   other five sectors still decode.
6. **Given** an RW18 address field whose track number differs from the
   physical track, **When** it is analyzed, **Then** the sector still decodes
   and Findings lists the difference.
7. **Given** an RW18 track, **When** the track header is shown, **Then** it
   shows the format and the ID nibble found in its data fields.
8. **Given** an RW18 track, **When** the Nibbles and Fields tabs are shown,
   **Then** they show every nibble of the track and every RW18 field with its
   marks and checksum.
9. **Given** an RW18 sector, **When** it is selected, **Then** "Edit sector"
   is unavailable and the Sector data tab shows why.
10. **Given** a standard DOS 3.3 disk, **When** it is opened, **Then** no
    track is classified RW18.

---

### User Story 16 - Compare two quarter tracks of one disk (Priority: P11)

A preservationist looking at a WOZ with a record on track 6.5 needs to know
whether the half track holds data of its own, or is a weaker, off-center read
of track 6 or track 7 saved when the disk was imaged. The user compares 6.5
with the quarter tracks beside it and sees, with the same verdicts, strips and
differences as a comparison of two disks, how much of it matches each
neighbor and where it differs. For a protection that writes data across
adjacent quarter tracks, the user compares a range of quarter tracks and sees
where the same data repeats.

**Why this priority**: It ships in the later release. It reuses the
comparison of User Story 10, and protection identification (User Story 13)
reports what it finds on adjacent quarter tracks, so it is built before that
story.

**Independent Test**: Build made-up WOZ images with a half track holding a
noisy copy of its neighbor, a half track holding sectors of its own, and one
run of data written across three adjacent quarter tracks, and check the
verdicts, the match shares and the repeated runs against what was planted.

**Acceptance Scenarios**:

1. **Given** a made-up WOZ whose record at track 6.5 holds track 6's nibbles
   with noise over a tenth of them, **When** the user compares 6.5 with its
   neighbors, **Then** the window shows the share of 6.5's nibbles that match
   track 6, marks the noisy stretches as different, and shows 6.5 as likely a
   read of track 6.
2. **Given** a made-up WOZ whose record at track 6.5 holds sectors that
   neither track 6 nor track 7 holds, **When** the user compares 6.5 with its
   neighbors, **Then** the window shows 6.5 as holding data of its own and
   lists the sectors found only on 6.5.
3. **Given** a made-up WOZ with one run of data written across tracks 5,
   5.25 and 5.5, **When** the user compares tracks 5 to 6, **Then** the
   window lists the run once for each of those quarter tracks, with its
   rotation on each.
4. **Given** two quarter tracks that map to one record, **When** the user
   compares them, **Then** the window shows that they share the record and
   compares nothing.
5. **Given** a disk in the standard layout, **When** the user compares a
   range of its quarter tracks, **Then** no repeated run is listed.
6. **Given** any comparison of quarter tracks, **When** it ends, **Then** the
   disk, its file and any running machine are unchanged.

---

### Edge Cases

- No disk in the drive: the platter shows "No disk in drive 1" (or 2) over a
  dashed outline of the disk and hub, with no chips, an empty strip and empty
  text panes.
- A disk is ejected or replaced while it is being analyzed: the analysis is
  dropped and the window shows the new state.
- Drive 2 is detached while the inspector shows it: the window shows "Drive 2
  is not attached", and "Inspect disk 2..." is unavailable until it is
  attached again.
- A machine change to a machine with no Disk II in slot 6: the window shows
  "This machine has no Disk II controller in slot 6" and keeps its other
  state, and both "Inspect disk" items are unavailable.
- A machine with a second Disk II controller in slot 5: only slot 6's drives
  are inspected, and the drive selector's tooltip shows "Slot 6 drives only".
- The head is on a quarter track with nothing recorded: the head marker sits
  on the empty ring and the strip shows "Nothing recorded".
- The drive is writing a flux track: the written stretch appears within
  500 ms of the write ending.
- The guest writes to a write-protected disk: the head marker shows write
  blocked, and no track changes or is marked as written.
- Casso saves the disk: the track views and the analysis do not change. The
  Image tab keeps describing the file as read at insert or at the last
  reload, and shows a note that the save rewrote the file.
- Casso reloads a disk because its file changed on disk: the whole disk is
  analyzed once more.
- At Maximum speed the disk turns many times per displayed frame: the platter
  shows the angle sampled once per frame, which can look still or reversed;
  the head marker's track is always correct.
- The drive the controller has not enabled: its platter does not turn,
  because only the enabled drive spins.
- While GH #135 is open, both drives share one head position: the head marker
  on the drive the controller has not enabled shows the position Casso holds
  for that drive, and it jumps to the other drive's track when that drive is
  next enabled.
- A track much longer or shorter than nominal: the platter maps the whole
  track to one turn, as Casso's drive plays it, and on a WOZ bit or flux track
  Findings reports the length.
- More than 16 sectors on a track, or sector numbers above $F: all are listed
  and the extra ones are reported in Findings.
- Several quarter tracks mapped to one track record: the record is analyzed
  once, the Tracks table shows which quarter tracks share it, and Findings
  lists it only when its quarter tracks are outside the layouts FR-048
  exempts.
- Track data past the drive's head limit, which the emulated head cannot
  reach (past track 34.75 today, past track 39.5 once spec 041 moves the
  head's stop): shown, dimmed behind a line at the head's limit, and marked as
  beyond the head's reach.
- A WOZ whose file holds no record for a whole track, where Casso keeps an
  empty track for the guest to format: the track shows "Nothing recorded",
  the Image tab lists no record for it, and the summary does not count it as
  a track with data. Once the guest writes there, the track shows what the
  guest wrote, marked as written, and the Image tab shows it as added since
  the file was read.
- A WOZ whose TMAP or FLUX entry points at a track record with a zero start
  block or a zero block count: its quarter tracks show what the drive reads
  there, "Nothing recorded" for a TMAP entry and an empty flux track, which
  the drive reads as random bits, for a FLUX entry; Findings and the Image tab
  report the entry as an image file problem, without the word damaged, and the
  image stays writable (FR-053).
- A WOZ whose INFO synchronized flag is off, so its tracks were not aligned
  when imaged: the sector 0 angles are still shown, and the Tracks table
  shows a note that the original alignment was not kept.
- Decode settings that match nothing: the disk shows no standard sectors and
  the settings chip stays visible.
- Analyzing a track again removes the selected sector: the sector nearest the
  same cell is selected.
- A search with no match: the results show "No matches".
- A damaged WOZ: damaged quarter tracks show as damaged, with the reason,
  while the drive reads them as unformatted.
- A WOZ whose disk type is 3.5" in a Casso drive: the drive plays its tracks
  as 5.25" tracks, and the inspector shows the Image tab and the note that
  Casso does not analyze 3.5" disks.
- A file that cannot be opened at all: the window shows the reason, not a
  blank disk.
- In Explorer, an image that a running Casso also has mounted: the inspector
  shows the file as last saved; Casso's own inspector shows the disk as the
  drive holds it.
- In Explorer, a file deleted or renamed while its inspector is open: the
  window keeps the last analysis and shows a note that the file is no
  longer there.
- In Explorer, the selection changes faster than images can be analyzed:
  only the image selected last gets a preview, and the others are dropped.
- In Explorer, several images are selected: "Inspect disk image" opens a
  window for each.
- The theme or the display scale changes while the window is open, or the
  window moves to a monitor with another scale: every view redraws without
  being reopened.
- A catalog or directory chain that loops or points outside the volume: the
  map stops following it there, lists the files read before that point, and
  Findings gives the place and the reason.
- A DOS 3.3 disk whose VTOC, or a ProDOS disk whose volume directory key
  block, is bad or missing: the File map tab shows that no file system was
  found and gives that sector as the likely cause, and the grid still shows
  each sector's result. A game disk whose track 17 sector 0 is bad is reported
  the same way, never as a DOS 3.3 disk with a bad VTOC.
- A ProDOS or Apple Pascal volume whose header gives a size other than 280
  blocks, or a DOS 3.3 volume whose VTOC gives other than 35 tracks of 16
  sectors: the File map tab shows that a volume of that size is not mapped,
  with the size. On a disk that holds a DOS 3.3 volume and a smaller Pascal
  volume, the DOS 3.3 volume is still mapped.
- A disk whose CP/M directory holds only free entries: it is mapped as an
  empty CP/M volume only when no DOS 3.3, ProDOS or Apple Pascal volume is
  found. A disk filled with zeros is never taken for CP/M.
- A CP/M boot disk's system entry in user area 31, which covers tracks 0 to 2:
  those tracks show as the system area, and the entry is not listed as a file
  and produces no finding.
- An Apple Pascal .BAD file: its blocks show as bad blocks the file system
  recorded, not as file data, and it produces no finding.
- A disk that holds both a DOS 3.3 VTOC and a ProDOS volume directory: both
  are mapped, with a choice between them in the File map tab.
- A DOS 3.3 catalog entry that uses no sectors, as some assemblers write for
  decoration: it is listed with no sectors and produces no finding.
- Two catalog entries with the same file name: both are listed, and a file
  comparison pairs them in catalog order.
- While the guest writes, the map is built again after each written track is
  analyzed again, so between the guest's write of a catalog sector and its
  write of the VTOC, the map can briefly show a sector as owned but marked
  free; the finding clears once the VTOC write is analyzed.
- Decode settings that make a disk with nonstandard marks decode: the map is
  built from the sectors they decode.
- Checksum checks turned off in the decode settings: the map follows chains
  through sectors not checked, no file shows "Touches bad sectors" for them,
  and Findings gives one finding for the disk.
- The guest writes to a track that holds pending edits: each pending edit on
  it follows its sector's field to the field's new place. One whose sector's
  bytes changed is marked out of date, and applying it needs the user's
  confirmation and writes only the bytes the user changed over the sector's
  new contents. One whose sector no longer decodes as editable is discarded,
  and the window shows that it was.
- The guest writes its own copy of a sector over an applied edit, such as a
  VTOC that DOS held in memory: the edit is lost, as on real hardware, the
  Tracks tab shows the guest write, and "Undo applied edit" for that track
  becomes unavailable.
- The user ejects or replaces the disk outside this window while edits are
  pending: the pending edits are discarded, and the window shows that they
  were.
- Casso, or the Explorer window, reloads the disk because its file changed
  while edits are pending: pending edits whose sectors did not change are
  kept, and the rest are handled as after a guest write.
- The user switches drives, closes the window, quits Casso or Explorer,
  changes the machine so that the disk is removed or replaced, or changes
  decode settings so that a sector with pending edits no longer decodes: the
  change needs the user's confirmation, which shows that those pending edits
  will be discarded. A machine change that keeps the same disk in the drive
  keeps the pending edits.
- Casso saves an edit and finds at save time that the file changed outside
  Casso: the save goes to a preserved copy, as every Casso save does in that
  case. The drive then holds the copy, the edit counts as applied to the copy,
  the window shows the copy's file name, and the undo history continues
  against the copy.
- Casso cannot save an applied edit, for example because the volume that holds
  the file is full, the file is locked, or a guest-written track cannot be
  saved in the image's format: every track record the apply changed is
  restored before the machine runs again, the edits stay pending, and the
  window shows "Not applied: the file could not be saved" with the reason.
- The guest writes to a track while an apply to it waits: the wait ends, the
  pending edits on that track are checked again as after any guest write, and
  the window shows the confirmation again.
- The user has paused the machine while the drive is writing or the head is
  inside a field the apply changes: "Apply" shows why it cannot apply yet and
  offers "Run to a safe point and apply".
- The user chooses "Redo applied edit" after the guest wrote to the track the
  undo restored: "Redo applied edit" is unavailable, and the window shows why.
- The window is hidden or minimized while an apply waits: the apply goes ahead
  when the drive stops, including the analysis that checks it.
- An edit on a WOZ whose checksum does not match, or that has a damaged track
  record: in either host the image counts as write-protected (FR-102), only
  "Save edited copy..." is available, and before saving, the window shows that
  the copy gets a new checksum and what it holds for each damaged record.
- In Explorer, an edit applied to an image that a running Casso has in a
  drive, with or without guest writes not yet saved: the save is a change to
  the file made outside Casso, sent with the reload intent, and Casso handles
  it as it handles Explorer's other writes to a mounted image (FR-106).
  Casso's inspector on that drive shows the file the drive holds afterward,
  and Explorer's inspector shows Casso's reply when it is anything other than
  a reload (FR-105).
- A `disk` command or Explorer write to a WOZ in the standard layout, or in a
  layout that also maps N+0.5 to a neighboring whole track's record: the
  write is accepted, and only the data fields it changes differ afterward.
- A `disk` command or Explorer write to a disk whose volume is not 254: every
  address field keeps the disk's volume.
- A `disk` command or Explorer write that needs a track that does not decode
  completely, a track with no standard address field, or a quarter track with
  nothing recorded: the write fails with the track and the reason, no track is
  formatted to make room, and the file is unchanged.
- A save of a sector write or of an applied, undone or redone edit fails
  partway, for example when the volume fills while the new contents are
  written: the file is as it was before the save.
- An edit on a track record that quarter tracks outside the standard layout
  share: the confirmation lists every quarter track that reads the record.
- An edit on a quarter track mapped in both TMAP and FLUX: only the flux
  record changes, which is the record Casso plays, and the confirmation shows
  that the bit record is left as it was.
- A comparison of tracks of different lengths: B's track is aligned to A's,
  the length difference is given in cells, and the platter draws each disk's
  track over one turn, as it does for every track.
- A comparison of a DSK with a WOZ in the standard layout: each whole track is
  compared once, and the quarter tracks between whole tracks show "Standard
  layout" and are not counted.
- A comparison of a 13-sector track with a 16-sector one: sectors pair only
  within one encoding, so every sector is listed as only in A or only in B,
  and the track shows "Sectors differ" with every sector counted.
- "Compare disk images" on an image that already has an inspector window: that
  window switches to the comparison, after the user confirms discarding any
  pending edits.
- A comparison of disks with different file systems, or where either has
  none: the file comparison shows why it is not given, and the track and
  sector comparison is unaffected.
- A file in a comparison is deleted or renamed: the window keeps the last
  comparison and shows that the file is no longer there.
- A disk in a comparison is ejected or replaced: the comparison ends, and the
  window shows why.
- A comparison with a WOZ whose disk type is 3.5": every quarter track shows
  "Not compared", with the reason.
- (Later release) An A2R capture of a drive type other than 5.25": the window
  shows that Casso analyzes only 5.25" captures.
- (Later release) An encrypted boot loader that is not in the pattern table:
  the Protection findings give only the evidence on the disk until "Trace
  boot" runs, and a trace that reaches no read of the drive within its cycle
  limit shows that none was found.
- (Later release) A capture whose revolutions do not overlap: the quarter
  track shows "Not compared", with the reason.
- (Later release) An RW18 address field whose track number differs from the
  physical track, as when a title stores a copy of one track under another
  track's number: the sector decodes, and Findings lists the difference.
- (Later release) A disk that mixes standard and RW18 tracks: each track is
  classified by its own fields, and the format chip shows both kinds.
- (Later release) A comparison of two quarter tracks that map to one record:
  the window shows that they share the record and compares nothing.

## Requirements *(mandatory)*

### Functional Requirements

**General**

- **FR-001**: The inspector window MUST present the same views whether Casso
  or Casso Explorer opens it, including the write-protect causes shown beside
  "Apply" (FR-102). Only these are specific to Casso: the drive selector,
  Follow head, "Go to head", the turning disk, the head marker, the head
  position, trail and drive state on the strip, the toolbar's write-protect
  state and the write-protect causes that belong to the drive (FR-065), the
  no-disk, not-attached and no-controller states, the refresh after the drive
  writes, the written and read marks in the Tracks tab, the "Reads" overlay,
  the out-of-date mark on Find results, the Image tab's changed-value marks
  and save note, the wait for a safe point before an edit reaches the drive
  (FR-103), saving an edit with the disk's other changes (FR-104), and the
  comparison sources taken from the drives (FR-117). Only these are specific
  to Explorer's window: the refresh when the file changes on disk, the note
  that the file is no longer there, "Compare disk images" and the "As first
  opened" comparison source (FR-117), and in the later release, A2R captures
  (FR-137). Explorer's preview pane shows only the small still platter and the
  summary chips (FR-072).
- **FR-002**: Viewing, the file and sector map, comparison and the
  preservation views MUST NOT change the disk image in memory or on disk,
  MUST NOT mark it as changed, MUST NOT cause it to be saved, and MUST NOT
  change what the guest reads or when it reads it. The inspector MUST change
  a disk only when the user applies, undoes or redoes a sector edit, and then
  only as FR-096 to FR-116 allow.
- **FR-003**: The inspector MUST open DSK, DO, PO, NIB, NB2, WOZ 1, WOZ 2 and
  WOZ 2.1 images, with bit tracks, flux tracks or both. For a sector image it
  MUST show the tracks Casso's drive plays, with a note that they are built
  from sector data and so hold no timing or copy protection. For NIB and NB2
  it MUST show a note that the format stores no timing bits between nibbles.
  The track lengths of sector, NIB and NB2 images come from the format, so
  those tracks get these notes and no track-length finding.
- **FR-004**: The inspector MUST cover all 160 quarter tracks, 0 to 39.75, and
  MUST mark the ones past the head limit as beyond the emulated head's reach
  in the Tracks tab and the track header, and by a line on the platter at that
  limit. The limit MUST be the head stop of Casso's emulated Disk II, the last
  quarter track its head can move to: the same in both hosts, read from
  Casso's drive definition, and never a constant of the inspector's own, so
  the marks and the line move when the drive's head stop moves (Assumptions).
- **FR-005**: On every quarter track with a readable record, the inspector
  MUST show what Casso's drive reads there, with flux decoded at the drive's
  own timing. A quarter track with no record MUST show as "Nothing recorded",
  and a damaged one as damaged with its reason (FR-053), never as a copy of a
  neighbor. A quarter track for which the file holds no data MUST show as
  "Nothing recorded" even where Casso keeps an empty track for the guest to
  format, and MUST NOT count as a track with data (FR-018). Once the guest
  writes such a track, its quarter tracks MUST show what the guest wrote,
  marked as written (FR-047).
- **FR-006**: Track numbers, block numbers, volumes, counts, cell positions,
  times and angles MUST be decimal, with angles in degrees clockwise from the
  index. Sector numbers, byte and nibble values, and offsets into a sector or
  into a track's nibbles MUST be hex. Hex values MUST have a "$" prefix except
  in hex dumps, the Nibbles grid, the sector buttons, the strip's sector
  labels and byte sequences such as marks, which are hex throughout. "Go to"
  MUST accept each value in the base it is shown in, and gaps (FR-017) MUST
  be given in nibbles.
- **FR-007**: The window MUST hold a toolbar with, in Casso, the drive
  selector and the write-protect state, then the file name and the chips,
  "Structure" and "Timing", the timing range, in Casso "Follow head" and "Go
  to head", and "Go to", "Find", "Compare with..." and "Decode settings...";
  while comparing, a comparison bar under the toolbar (FR-121); a platter
  column with the platter, its zoom controls, the legend, the overlays
  ("Alignment", "Files", in Casso "Reads", and while comparing "Differences"),
  the hint "Scroll to zoom, drag to pan, double-click to fit", and the disk
  tabs Tracks, Findings, File map and Image, with Differences added while
  comparing; and a track column with the track header, the strip with its
  controls and hint, the sector row, and the track tabs Sector data, Nibbles,
  Fields and Flux timing. The Sector data tab MUST hold the edit controls
  above the bytes: "Edit sector", "Recompute the checksum", the count of
  pending changes, "Apply", "Discard", "Discard sector", "Save edited
  copy...", "Undo applied edit" and "Redo applied edit" (FR-097 to FR-107),
  "Copy sector" (FR-057) and, while comparing, "Use B's bytes" (FR-123). The
  File map tab MUST hold "Show deleted files", the file filter, "Previous
  sector in file", "Next sector in file" and "Copy map" above the grid (FR-088
  to FR-095). In the later release, the toolbar MUST also hold the verdict
  chip and "Trace boot" (FR-134, FR-135), the track header the revolution
  chooser (FR-137), the Nibbles tab the framing control (FR-128), the platter
  a "Revolutions" overlay (FR-138), the decode settings "Solve table", "Search
  for tables", "Save decode settings..." and "Load decode settings..." (FR-125
  to FR-127) and a switch for each custom track format decoder (FR-141), and
  "Compare with..." a choice of quarter tracks of the window's disk (FR-145).
  The window MUST open at 980×660 and not shrink below 640×460, both scaled
  for the display, and the platter MUST stay square.

**Analysis**

- **FR-008**: The analyzer MUST frame nibbles as the drive's read latch does
  after one full turn of the track from the index, and MUST treat a track as
  a loop, so a nibble or field that spans the end of the track is read whole.
- **FR-009**: The analyzer MUST give every nibble outside a random-bit region
  one kind: sync, address prologue, address field, address epilogue, data
  prologue, data field, data epilogue, other (a valid disk byte outside any
  field), or noise (a nibble with three or more zero cells in a row within
  its eight bits). A nibble MUST keep its kind however many zero cells follow
  it, up to the random-bit limit in FR-010, and those cells are its
  extra-zero count. Field kinds MUST also record a failed checksum. A sync
  nibble is an FF followed by one or two zero cells; on NIB and NB2, which
  store no zero cells, an FF in a run of FF nibbles outside a field counts as
  sync.
- **FR-010**: The analyzer MUST mark random-bit regions: stretches with no
  transition for longer than the limit past which Casso's drive produces
  random bits, using the drive's own limit for bit and flux tracks. A
  random-bit region MUST take precedence over noise: it is shown as one
  region with no nibble values, and framing resumes at the first 1 cell
  after it.
- **FR-011**: The analyzer MUST decode 16-sector fields: address prologue
  D5 AA 96, a 4-and-4 volume, track, sector and checksum, address epilogue
  DE AA with or without EB, and a data field after prologue D5 AA AD holding
  342 6-and-2 nibbles and a checksum nibble, decoded to 256 bytes, then data
  epilogue DE AA with or without EB.
- **FR-012**: The analyzer MUST decode 13-sector fields: address prologue
  D5 AA B5, with the same 4-and-4 address field and DE AA epilogue as
  16-sector fields, and a data field after prologue D5 AA AD holding 410
  5-and-3 nibbles and a checksum nibble, decoded to 256 bytes and
  checksum-verified as 6-and-2 data is, then epilogue DE AA. A track holding
  both kinds MUST have both decoded.
- **FR-013**: A data field MUST pair with the address field before it only
  when its prologue starts within the data search window after that address
  field (Assumptions). The search MUST end at the first address prologue
  inside the window (16- or 13-sector, standard or from the decode settings),
  and the address field then has no data field. A data field MUST pair with
  at most one address field. An address field whose checksum fails MUST
  still pair with the data field after it, and its sector number MUST still
  count as present on the track, so it produces only the failed-checksum
  finding, not a missing-sector or unpaired-field finding. A data field with
  no address field and an address field with no data field MUST each be
  reported.
- **FR-014**: A data field whose checksum fails MUST still be decoded, so its
  bytes can be shown, and MUST be marked bad.
- **FR-015**: For each field the analyzer MUST keep the prologue and epilogue
  bytes actually found and the stored and computed checksums, and MUST detect:
  a failed address or data checksum; an address track number that differs from
  the physical track; a volume number that differs from the disk's most common
  one; a sector number repeated on a track; sector numbers missing from a
  standard track; a missing or nonstandard epilogue; an address field with no
  data field; a data field with no address field; data-field nibbles outside
  the encoding's translate table, reported once for each field with the nibble
  values and their counts; and a D5 AA pair outside any recognized field,
  reported with the byte that follows it. The physical track of a record is
  the whole track nearest the middle of the quarter tracks that map to it;
  when that middle is a half track N.5, an address track of N or N+1 matches.
- **FR-016**: The analyzer MUST classify every quarter track as exactly one of
  these, taking the first that applies: nothing recorded; damaged (its image
  record could not be read); 16-sector, 13-sector, or 13 and 16 sector, when
  at least one address field of that kind passes its checksum or is not
  checked (FR-020); unformatted, when more than half its length is noise or
  random-bit regions; otherwise nonstandard (valid nibbles but no standard
  sector). On a nonstandard track, the candidate fields MUST still be listed
  with their checksums. In the later release, a track that a custom track
  format decoder recognizes (FR-141) takes that format's class after the
  standard classes and before unformatted.
- **FR-017**: For each track the analyzer MUST measure: its length in cells
  against the nominal length (Assumptions); its nibble count; every sync run,
  with its nibble count and each nibble's width in cells; the gaps before and
  after each address and data field; the longest sync run, the track's likely
  write seam; the angle from the index to sector 0's address field and to the
  first field; and, on a flux track, the time of one turn, the equivalent
  RPM, the mean cell time and its deviation from nominal.
- **FR-018**: The disk summary MUST count each track record once, however many
  quarter tracks share it, and MUST give: the disk's format; the number of
  tracks with data; sectors good out of sectors found, with the number not
  checked (FR-020); bad sectors, each counted once whichever checksum failed;
  the numbers of nonstandard, unformatted, flux and damaged tracks; and the
  most common volume number. The format MUST be the first of these that
  applies: "Nothing recorded" when no quarter track holds a record; "13 and 16
  sector" when both kinds of standard sector occur anywhere on the disk; "16
  sector" or "13 sector" when only that kind occurs; "Nonstandard" when at
  least one track is nonstandard; "Unformatted" when at least one track is
  unformatted; otherwise "Damaged". In the later release, the format MUST also
  give each custom track format the disk's tracks use (FR-141), after the
  standard kinds, as in "16 sector and RW18", or alone, as in "RW18", when the
  disk has no standard sectors (FR-143). On a disk with standard sectors,
  nonstandard and unformatted tracks show only in their own chips. The summary
  MUST be shown as chips beside the image's file name, which is shortened with
  an ellipsis when it does not fit and shown in full in its tooltip. The
  sectors-good chip MUST take the bad color when any sector is bad. A chip
  whose count is zero MUST be hidden, and so MUST the volume chip on a disk
  with no standard sectors.
- **FR-019**: Through "Decode settings...", the user MUST be able to set the
  16-sector address prologue, the 13-sector address prologue, the data
  prologue, and the address and data epilogues the analyzer matches, where
  each byte accepts ?? for any value; turn off the address checksum, data
  checksum and epilogue checks; and apply the settings to the whole disk or to
  a chosen range of tracks. Custom marks MUST be matched in addition to the
  standard ones unless "Match standard marks too" is turned off. A change MUST
  analyze again every track it applies to, a chip MUST show while nonstandard
  settings are in use, and "Reset to standard" MUST restore the standard
  settings. The settings MUST last only as long as the window shows that disk;
  in the later release, the user can also save them to a file and load them
  (FR-127).
- **FR-020**: A sector whose address or data checksum check is turned off
  MUST show as "not checked", with its own color and symbol, in the sector
  row, the Sector data header, the Fields tab and the strip's sector label.
  It MUST count as found but not as good, the sectors-good chip MUST give the
  number not checked, and the Fields tab MUST still show the stored and
  computed checksums.
- **FR-021**: Analysis MUST NOT stop the window from responding to input, and
  a track not yet analyzed MUST show that it is being analyzed (FR-023).

**Platter**

- **FR-022**: The platter MUST draw one ring per quarter track with track 0 at
  the rim, a groove at each whole-track boundary, the hub, and an index mark
  where each track starts, at 12 o'clock while the platter has not turned
  (FR-064). The angle around the platter MUST be the position along the track
  as Casso's drive plays it, as a fraction of one turn, increasing clockwise
  from the index.
- **FR-023**: The platter MUST switch between "Structure" and "Timing" modes.
  In Structure mode it MUST color each stretch by kind: sync, address marks,
  address field, data marks, data field, failed checksum, other, noise,
  random-bit region, and nothing recorded. In both modes, a damaged quarter
  track MUST be drawn in its own color with a hatch, with the damage reason in
  its tooltip; a ring not yet analyzed in a pending pattern; and rings past
  the head's limit dimmed, behind the line at that limit (FR-004). A feature
  narrower than one device pixel MUST be drawn one device pixel wide in its
  own color, and where features meet in one pixel, a failed checksum MUST be
  drawn over noise, noise over a random-bit region, and those over every other
  kind. A failed address field MUST be drawn in the failed-checksum color and
  described as an address field, never as a data field, in its tooltip
  ("Address field, checksum failed") and in the Fields tab.
- **FR-024**: In Timing mode the platter MUST color flux tracks by each cell's
  deviation from the nominal 3.91 µs cell, fast toward one color, slow toward
  another and neutral at nominal, and MUST show bit tracks and sector-image
  tracks in dimmed structure colors. The timing range MUST default to ±5% and
  be adjustable from ±1% to ±25%, and the legend MUST show its numeric scale.
- **FR-025**: The platter MUST zoom from fit to at least 600×, about the
  pointer, with the mouse wheel, the "−", "+" and "Fit" buttons, and the
  plus, minus and 0 keys, and MUST pan by dragging. A drag shorter than the
  Windows drag threshold MUST count as a click. Double-click and 0 MUST return
  to fit, zooming back out to fit MUST recenter the disk, "Fit" MUST be
  unavailable at fit, a readout MUST show the zoom, panning MUST stop before
  the disk leaves the view, and the zoom controls MUST be hidden when there
  is no disk.
- **FR-026**: As rings widen with zoom, the platter MUST show each cell's own
  kind instead of averages once a cell is at least one device pixel long;
  then each nibble's hex value along its ring, upright, with a divider at
  each nibble's start, once the value fits inside the nibble's arc at the
  window's text size; then a tick for each 1 cell, once ticks for adjacent
  cells no longer touch.
- **FR-027**: Hovering the platter MUST show a tooltip with the track, the
  kind, the sector, the ring's sectors good out of found, and, on flux
  tracks, the cell time and its deviation (for example "4.10µs cells
  (+4.8%)"). Once nibble values are shown it MUST add the nibble value, the
  nibble's offset and "cell N of M". An empty ring MUST show "Nothing
  recorded", with a note that the drive reads random bits there. A tooltip
  MUST NOT appear during a drag.
- **FR-028**: The ring under the pointer MUST be outlined, and the selected
  ring outlined more strongly. A click MUST select the quarter track and the
  sector whose field is under the pointer, not the first sector with the
  same number.
- **FR-029**: With focus on the platter, Up and Page Up MUST move toward
  track 0 and Down and Page Down toward 39.75: Up and Down by one quarter
  track, and Page Up and Page Down to the next whole track in that direction
  (from 17.25, Page Down selects 18 and Page Up selects 17), stopping at 0
  and 39.75. With focus on the platter or the sector row, Left and Right MUST
  select the previous and next sector in passing order, wrapping at the
  index, and Home and End the first and last. With the platter zoomed,
  Ctrl+arrow keys MUST pan it.
- **FR-030**: A legend for the current mode MUST be shown with the platter:
  in Structure mode each structure kind, the damaged hatch, the pending
  pattern and the beyond-reach dimming; in Timing mode, "Fast cells",
  "Nominal 3.91µs" and "Slow cells" with the numeric scale and the note "Only
  flux tracks record timing; other tracks are dimmed".
- **FR-031**: The platter MUST have an "Alignment" overlay that marks each
  track's sector 0 address field and its longest sync run, so their angles
  can be compared across tracks. In Casso it MUST also have a "Reads" overlay
  that marks each quarter track the drive has read since the disk was
  inserted (FR-047).

**Track header and strip**

- **FR-032**: The track header MUST show "Track 17", "Track 17.25", "Track
  17.5" or "Track 17.75", the track's classification, and a line such as
  "51,007 cells · 6,352 nibbles · 16 sectors, 16 good · flux" ("no standard
  sectors" when there are none), or "Nothing recorded on this quarter track".
  A second line MUST show measurements such as "Turn 199.6 ms (300.6 RPM) ·
  mean cell 3.93µs (+0.5%) · longest sync 40 nibbles at cell 12,400", with
  the turn and cell items on flux tracks only.
- **FR-033**: The strip MUST show the selected track unrolled from the index,
  each nibble in its kind color, and in Timing mode on a flux track the
  timing colors. On a flux track it MUST draw a timing line in both modes,
  nominal in the middle, slow above and fast below, with the timing range
  filling the band. The strip MUST mark the longest sync run as the likely
  write seam.
- **FR-034**: The strip MUST zoom from the whole track down to about 20
  cells across, about the pointer, with the mouse wheel, the "−", "+" and
  "Whole track" buttons and the keyboard, and MUST pan by dragging, with
  Shift and the wheel, and with Left and Right. Shift+drag MUST select a
  range (FR-043). Double-click MUST return to the whole track. A readout MUST
  show the zoom and the cells in view (for example "×83 · cells
  12,400-13,016"), a hint MUST read "Scroll to zoom, drag to pan, Shift+drag
  to select, double-click for the whole track", and the zoom MUST be kept
  when another track is selected or the same track is analyzed again.
- **FR-035**: As the strip zooms in, it MUST show each nibble's hex value and
  dividers between nibbles once the value fits inside the nibble at the
  window's text size, then each cell's 1 or 0 with a pulse for each 1 once
  the digit fits inside the cell, then on flux tracks each cell's own timing,
  and a nibble's kind inside it where the kind's label fits.
- **FR-036**: The strip MUST show each sector's hex number over its address
  prologue, marked when either checksum failed or was not checked, and MUST
  outline the selected sector from its address prologue to the end of its
  data epilogue, or to the end of its address field when it has no data
  field, in two parts when it spans the end of the track.
- **FR-037**: Hovering the strip MUST show the nibble's value, its cell and
  width in cells, its kind and sector, and on flux tracks its timing.
  Clicking a nibble MUST select its sector and scroll the Nibbles tab to it.
- **FR-038**: When the selection changes from any view, the strip and, when
  zoomed, the platter MUST pan, keeping their zoom, until the selected sector
  or nibble is in view. On a new track, the strip MUST keep the same fraction
  of the turn in view unless the new selection is outside it. If analyzing a
  track again removes the selected sector, the sector nearest the same cell
  MUST be selected.

**Sectors, Sector data, Nibbles and Fields**

- **FR-039**: Under the label "Sectors, in the order they pass the head", one
  button per address field MUST show its hex sector number in passing order,
  in a state of good, bad address, bad data, not checked, or no data field.
  Each state MUST use the same color and symbol in every view. The tooltip
  MUST show the sector and its place in passing order (for example "Sector
  $5, 6th past the index"), the volume and track fields, and which checksum
  failed or was not checked. With no sectors the row MUST show "None in a
  standard format", or "No track" for an empty quarter track. The selected
  sector's button MUST be highlighted. When the selected track changes
  without a sector being chosen with it (Up, Down, Page Up, Page Down, a
  Tracks row, "Go to" with a track only, Follow head, "Go to head"), the first
  sector in passing order MUST be selected. A click on a field, a Fields or
  Findings row, "Go to" with a sector, and a Find result each select their
  own sector.
- **FR-040**: The Sector data tab MUST show a header with the sector, track
  field, volume, address result, data result (good, bad, not checked or
  missing) and encoding (6-and-2 or 5-and-3), and on a 16-sector track also
  the DOS 3.3 logical sector and the ProDOS block, and which half of it, that
  the physical sector holds under the standard skew, and, on a mapped CP/M
  volume, the CP/M sector and allocation block the physical sector holds
  (FR-085). Below the header it MUST show the 256 bytes as 16 rows of 16 with
  hex offsets and a text column (high bit masked, "." for anything not
  printable), with zero bytes and bytes of $80 or more in their own colors.
  Bad data MUST be shown and marked. With no sectors it MUST show "No standard
  sectors on this track. The Nibbles tab shows what is recorded on it." For an
  empty quarter track it MUST show "Nothing recorded on this quarter track.",
  for a damaged one the damage reason, and for a sector with no data field the
  header followed by "This sector has no data field." in place of the bytes.
- **FR-041**: The Nibbles tab MUST show every nibble on the track with hex
  offsets, rows sized to the pane in steps of 8 nibbles, kind colors, the
  selected sector's nibbles highlighted, each sync nibble's width, the count
  of extra zero cells after any nibble, and invalid nibbles and random-bit
  regions marked. Each nibble MUST have a tooltip with its offset, cell,
  kind and timing, and clicking one MUST select it in the strip and the
  platter. Selecting a sector or a strip nibble MUST scroll its row about a
  third of the way down the pane. An empty quarter track MUST show "Nothing
  recorded."
- **FR-042**: The Fields tab MUST list each address field, and each data field
  with no address field, in passing order, with its cell and angle, sector,
  volume and track fields, address prologue and epilogue bytes found,
  address checksum stored and computed, gap to the data field, data prologue
  and epilogue bytes found, data checksum stored and computed, encoding, the
  sync run before it (count and width), and every finding on it. Selecting a
  row MUST select that sector.
- **FR-043**: In the Nibbles tab and the Sector data tab, a range MUST be
  selected by dragging and extended with Shift and a click or the arrow
  keys, and Select all MUST take the whole track or the whole sector, as in
  033's hex view (its FR-012c and FR-012d). In the Sector data tab, the hex
  and text columns MUST each be a tab stop, as in 033's hex view (its
  FR-012j). In the strip, Shift+drag MUST select a range of nibbles while a
  plain drag still pans. The selection MUST show in the strip, the Nibbles
  tab and, when zoomed, the platter, with a readout of its length in nibbles
  and cells, and on a flux track in µs.

**Flux timing**

- **FR-044**: For a flux track, the Flux timing tab MUST plot each
  transition's interval against its position along the track, at its
  recorded time and not rounded to cells, with lines at one, two and three
  nominal cells and marks at the index and at each sector, following the
  strip's zoom and pan. Pairs of transitions that fall within one cell, which
  the drive reads as a single transition, MUST be marked.
- **FR-045**: The Flux timing tab MUST show a histogram of transition
  intervals, with counts, and a choice of "Whole track" and "Selection",
  defaulting to "Whole track". "Selection" covers the selected nibbles, or
  the selected sector when no nibbles are selected. The selection's
  transitions MUST be highlighted in the plot.
- **FR-046**: For a bit track the Flux timing tab MUST show a note that the
  track is stored as bit cells of one length.

**Tracks, Findings and Image**

- **FR-047**: The Tracks tab MUST list all 160 quarter tracks with: the
  quarter track, what is recorded there (nothing, bit track, flux track or
  damaged), which other quarter tracks share its record, its classification,
  sectors good out of found, its nibble count, its length against nominal,
  its longest sync run, the angle of sector 0, on flux tracks the RPM and the
  mean cell deviation, its number of findings, and marks for beyond the
  head's reach. In Casso it MUST also mark each quarter track written since
  the disk was inserted, and each quarter track read since then with its
  number of visits (each stay of the head there with the motor on). Selecting
  a row MUST select that quarter track. On a WOZ whose INFO synchronized flag
  is off, the tab MUST show a note that the original track alignment was not
  kept.
- **FR-048**: The Findings tab MUST list every finding on the disk with its
  track, sector and cell where they apply. Findings about what is recorded on
  a track MUST be worded neutrally, without the words damage or protection,
  except the Protection findings of the later release (FR-131). Findings about
  the image file MUST describe the record or the file as damaged, with the
  reason, when a record cannot be read, a map entry is out of range or the
  checksum does not match, and MUST describe the other image file problems in
  FR-051 plainly, without the word damaged. Findings MUST include those in
  FR-015 and these: on a WOZ, one finding, giving the quarter tracks it
  covers, for each track record that a quarter track strictly between whole
  tracks N and N+1 maps to when it is neither the record at whole track N nor
  the record at N+1, that no whole track maps to, or that more than one whole
  track maps to (such as a record on a half track, on a quarter track alone,
  or across two whole tracks); 13-sector fields on a disk that also has
  16-sector fields; on WOZ bit and flux tracks only, a track length more than
  2% from nominal; random-bit regions on a formatted track; damaged image
  records (FR-053); the image file problems in FR-051; and the file system
  findings in FR-093. This rule exempts, among others, the standard WOZ layout
  (N-0.25, N and N+0.25 on track N's record), the layout that also maps N+0.5
  to a neighboring whole track's record, the layout Casso writes for WOZ and
  builds for sector, NIB and NB2 images (N, N+0.25, N+0.5 and N+0.75 on track
  N's record), and a track the guest formats where the file held no record
  (FR-005). These MUST NOT produce a quarter-track finding, so a standard
  DOS 3.3 disk in any of these layouts produces no findings. Selecting a
  finding MUST go to it.
- **FR-049**: Findings MUST be ordered by quarter track and cell, MUST be
  sortable by each column and filterable by category, and MUST show a count
  for each category.
- **FR-050**: The Image tab MUST show the file name, format, size and
  read-only attribute. For a WOZ it MUST also show the checksum result, every
  INFO field the file's version has (version, disk type, write protected,
  synchronized, cleaned, creator, sides, boot sector format, optimal bit
  timing, compatible hardware, required RAM, largest track, flux block and
  largest flux track), every META entry in file order, the TMAP and FLUX maps
  as the file holds them, each track record's location and length as the
  file's version stores them (for WOZ 2 and 2.1, start block, block count and
  bit or byte count; for WOZ 1, bytes used, bit count, splice point, splice
  nibble and splice bit count), records that no map refers to, the WRIT chunk
  when present, and the ID and size of any other chunk. It MUST list the track
  records the file holds and, in Casso, the records the guest added since
  (FR-071), never an empty track Casso keeps for the guest to format (FR-005).
  For a sector image it MUST show the sector order and a note that the tracks
  are built with volume 254.
- **FR-051**: The inspector MUST check the image file for these problems and
  report each one in Findings and the Image tab: a checksum mismatch; an INFO
  largest track smaller than the largest track record; a bit or byte count
  larger than its blocks hold; a META value outside the WOZ lists (language,
  requires_ram, requires_machine); an image_date that is not RFC 3339;
  duplicate or out-of-order chunks; data past the last chunk; a track record
  that no map refers to; a TMAP or FLUX entry that points at a track record
  with a zero start block or a zero block count (FR-053); and a NIB track with
  no sync.
- **FR-052**: Casso MUST keep, from each WOZ file it reads, every INFO field
  and META entry of the file's version, the TMAP and FLUX maps as the file
  holds them, each track record's location and length fields, the records that
  no map refers to, and which map (TMAP or FLUX) each quarter track came from,
  for the inspector to show.
- **FR-053**: Damage MUST be shown per quarter track with its reason,
  including map entries out of range, never as a blank track with no
  explanation. Casso MUST record a TMAP or FLUX entry from 160 to 254 (255
  means no track) as damage, with the reason. A TMAP or FLUX entry that points
  at a track record with a zero start block or a zero block count is not
  damage: its quarter tracks MUST show what the drive reads there (FR-005),
  and the entry MUST be reported as an image file problem (FR-051), without
  the word damaged. The inspector MUST show each damaged record's reason on
  every quarter track that maps to it, not on one quarter track only.
- **FR-054**: A WOZ whose disk type is 3.5" MUST show the Image tab, and
  every other view MUST show a note that Casso does not analyze 3.5" disks.
  Casso mounts such a WOZ today and plays its tracks as 5.25" tracks, because
  it does not read the disk type when mounting. This spec leaves mounting
  unchanged, and the inspector reads the disk type from INFO itself. GH #163
  tracks 3.5" support. A file that cannot be opened at all MUST show the
  reason in the window.

**Navigation, search and copy**

- **FR-055**: "Go to" MUST select a track (with quarter fraction), a physical
  sector, a DOS 3.3 logical sector or a ProDOS block (on a 16-sector track,
  through the standard skew), a CP/M sector of a mapped CP/M volume (FR-085),
  a nibble offset or a cell.
- **FR-056**: "Find" MUST search for a hex nibble pattern, where ? matches
  any hex digit and + after a nibble requires extra zero cells after it; for
  hex bytes in decoded sector data, where ? matches any hex digit; or for
  text in sector data; on the selected track or the whole disk, with an "Any
  bit offset" option that also finds nibble patterns not aligned to the
  nibbles as framed. Results MUST be listed with their locations, "Find next"
  and "Find previous" MUST step through them, and a search with no match MUST
  show "No matches". In Casso, results on a track the drive writes MUST be
  marked out of date until Find runs again.
- **FR-057**: In the Sector data tab, Copy MUST put the selection on the
  clipboard as hex digits from the hex column and as characters from the
  text column, with one Copy command, as 033's hex view does (its FR-012f),
  and "Copy sector" MUST put the whole sector on the clipboard as a hex dump.
  In the Nibbles tab and the strip, Copy MUST put the selected nibbles on the
  clipboard as hex text. Selected rows of the Tracks, Fields, Findings and
  Image tabs, and the Flux timing histogram's bins and counts, MUST be copied
  as tab-separated text.
- **FR-058**: [NEEDS CLARIFICATION: Is export to a file in this spec, and in
  which forms: a track's nibbles as raw bytes, a track as hex text, a
  sector's 256 bytes as a binary file, or all three?] If it is, the user MUST
  be able to export the selected track or sector in the chosen forms, and
  User Story 4 gains a matching acceptance scenario. Copy (FR-057) is in
  either way.
- **FR-059**: Every command and view MUST be reachable from the keyboard,
  with visible focus. Every button MUST have a tooltip, and the mouse controls
  of the platter and the strip MUST be explained by the hints on screen
  (FR-007, FR-034).

**Casso host**

- **FR-060**: Casso's Storage menu MUST offer "Inspect disk 1..." in drive 1's
  group and "Inspect disk 2..." in drive 2's group, next to each drive's
  salvage item. The same items MUST appear on each drive's right-click menu in
  the drive band, the fullscreen strip and the desk scene. They MUST be
  available with or without a disk inserted, drive 2's only while drive 2 is
  attached, and neither when the machine has no Disk II in slot 6. The
  inspector MUST show only slot 6's drives; on a machine that also has a
  Disk II in slot 5, the drive selector's tooltip MUST show "Slot 6 drives
  only".
- **FR-061**: Casso MUST have at most one inspector window. Opening it for a
  drive MUST switch it to that drive and bring it to the front. Its "Drive
  1" and "Drive 2" selector MUST switch drives, which resets the zoom and the
  selection and shows the other drive's disk. With no disk in the drive, the
  platter MUST show "No disk in drive 1" (or 2) over a dashed outline of the
  disk and hub, with no chips, an empty strip and empty text panes.
- **FR-062**: Casso MUST make available to the inspector, for each drive, once
  per frame and as one consistent record: the head's quarter track, the track
  record under it, the angle as a fraction of one turn, whether the motor is
  on, whether the controller has the drive enabled, and whether the drive is
  reading, writing, or blocked from writing by write protection (Assumptions).
- **FR-063**: The inspector MUST NOT read or change the drive's disk itself.
  Every track record it analyzes, compares or keeps as inserted, and every
  image detail it shows (FR-052, FR-071), MUST be read from a copy that Casso
  takes at one moment between guest accesses, never from the drive's disk
  while the guest can change it, and every change it makes to the disk in a
  drive MUST be made by Casso at a safe point (FR-103). Taking the copies
  counts toward the limits in SC-005.
- **FR-064**: While the motor is on, the controller has this drive enabled
  and a disk is inserted, the head marker MUST sit at 12 o'clock and the
  platter MUST turn counterclockwise so that the drive's current position is
  under the marker; otherwise the platter MUST stay still. The head marker
  MUST point at the head's quarter track and show idle, reading, writing and
  write blocked differently. While the platter is zoomed in it MUST stay
  still and the head marker MUST move around it instead.
- **FR-065**: The window MUST show whether the disk in the drive is
  write-protected and each cause Casso reports for the drive, including the
  image's write-protect flag, a read-only file, no permission to write the
  file, Casso's write-protect setting for the drive, and a damaged image. A
  guest write to a protected disk MUST show as "write blocked", distinct from
  writing, and MUST NOT mark a track as written or cause any analysis.
- **FR-066**: When the head is on the shown track, the strip MUST show the
  drive's state (motor off, reading, writing or write blocked), and while the
  motor is on, the head's position with a short fading trail behind it.
- **FR-067**: "Follow head" (tooltip "Show the track the head is on") MUST
  select the head's quarter track after the head has stayed on it for about
  0.3 s, while the head marker itself moves at once. Selecting a track by
  hand MUST turn it off, and turning it on MUST select the head's track.
  "Go to head" MUST select the head's track once without turning Follow head
  on.
- **FR-068**: Casso MUST keep, for each track record, a count of guest writes
  to it, and for each quarter track, a count of the head's visits with the
  motor on, for bit and flux tracks alike. A save MUST NOT reset these
  counts, inserting or reloading a disk MUST start them at zero, and the
  inspector MUST be able to read them while emulation runs. The tracks to
  analyze again (FR-069) and the written and read marks (FR-047) come from
  these counts.
- **FR-069**: When the drive writes, the inspector MUST analyze again only the
  tracks written, MUST show their new contents within 500 ms of the write
  reaching the track, and during continuous writes to a bit track MUST update
  at least twice a second. On a flux track, the written stretch MUST appear
  within 500 ms of the write ending. The inspector MUST NOT analyze the whole
  disk except when the window first shows a disk (on opening, on a drive
  switch or after a machine change), on insert, on a reload after the file
  changed, on a change of decode settings, when a comparison starts (for each
  disk the window has not already analyzed), and when a compared file changes
  on disk (FR-122), and never once per frame. The selection and the zoom MUST
  be kept. An applied edit, an undo or a redo MUST analyze again only the
  tracks it changed (FR-100), and a comparison MUST compare again by the same
  rules (FR-122).
- **FR-070**: The window MUST stay open across reset, pause and machine
  changes, and after a machine change MUST show the new machine's drive. After
  a change to a machine with no Disk II in slot 6, the window MUST show "This
  machine has no Disk II controller in slot 6" and keep its other state. A
  hidden or minimized window MUST do no drawing and no analysis, and when
  shown again MUST analyze only the tracks written while it was hidden. The
  analysis that an apply, undo or redo needs (FR-100) MUST run even while the
  window is hidden or minimized.
- **FR-071**: In Casso, the Image tab MUST describe the file as read at insert
  or at the last reload. A value Casso has changed since then, such as a track
  record's length after a guest write or the write-protect flag after a change
  from Casso, MUST be marked as changed and shown with its current value; a
  track record the guest wrote where the file held none MUST be shown as added
  since the file was read; and the marks MUST refresh after a write, an
  applied edit, a save, a write-protect change and a reload. The checksum line
  MUST show "Checked when the file was read". After Casso saves the disk, the
  tab MUST show a note that the save rewrote the file, since a WOZ is saved as
  WOZ 2, or WOZ 2.1 when it has flux tracks, with new track record locations.

**Casso Explorer hosts**

- **FR-072**: When an image in a format FR-003 lists is selected, the preview
  pane MUST show a small still platter in Structure mode and the summary
  chips together with 033's catalog or details preview, without delaying it.
  Until the analysis finishes, the preview MUST show 033's preview at once
  and a placeholder where the small platter goes. Clicking the small platter
  MUST open the inspector for that image. For a WOZ whose disk type is 3.5",
  the preview MUST show the note "Casso does not analyze 3.5" disks" in place
  of the platter and chips. An image that fails to open MUST show only 033's
  error. Images in other formats, such as the 3.5-inch and hard-disk images
  that GH #163 tracks, MUST get no platter and no chips.
- **FR-073**: "Inspect disk image" MUST appear on the right-click menu of an
  image in a format FR-003 lists, in the file list and in the folder tree,
  whether or not the image is writable, and MUST open an inspector window for
  that file. It MUST also appear on the file list's background menu while the
  list shows the contents of such an image, acting on that image, whether or
  not the image is writable and at any directory depth. With several images
  selected, it MUST open a window for each. Each image MUST have at most one
  window, and choosing "Inspect disk image" again MUST bring it to the front.
  In the later release, it MUST also appear on A2R captures (FR-137).
- **FR-074**: An Explorer inspector MUST show the file as saved and MUST
  analyze it again when the file changes on disk, including changes Explorer
  itself makes, keeping the selected track when it still exists, and keeping
  or dropping pending edits as FR-108 gives. After the window saves an edit of
  its own, only the edited tracks MUST be analyzed again (FR-105). When the
  file is deleted or renamed, the window MUST keep the last analysis and show
  a note that the file is no longer there.
- **FR-075**: An Explorer inspector window MUST stay open when the Explorer
  window that opened it closes, MUST close when Explorer exits, and MUST NOT
  reopen at the next launch.
- **FR-076**: Explorer's command bar MUST NOT change, so it still matches
  File Explorer's.

**Themes and state**

- **FR-077**: Every color MUST come from the active theme, in every theme
  either host offers. No theme holds colors for the inspector's kinds, map
  roles, pending edits or differences today, so this spec adds inspector
  colors to each of Casso's three themes and to Explorer's Light and Dark
  themes, and defines the colors used for a theme that holds none. In each
  theme, all text other than unavailable controls MUST meet a 4.5:1 contrast
  ratio against its background, and every pair of Structure kind colors, and
  every pair of map role colors (FR-086), MUST differ by at least ΔE2000 10. A
  theme change MUST redraw every view without reopening it.
- **FR-078**: In Casso, the inspector window MUST be viewable in a light
  theme, so that the parts specific to Casso (FR-001) are checked in light
  and dark (SC-008). Casso's own themes are all dark, and 033 adds Light and
  Dark to Explorer only. [NEEDS CLARIFICATION: How does the inspector window
  in Casso get a light theme: by following the Windows light or dark app
  setting, by a Light and Dark choice of its own, or by Casso gaining a Light
  theme?]
- **FR-079**: Good and bad states, and address and data marks, MUST NOT
  differ by color alone; each MUST also differ in symbol or pattern.
- **FR-080**: Each host MUST save, separately from the other, the window's
  placement and size, the mode, the selected disk tab and track tab, and the
  timing range. Casso MUST also save Follow head, the last drive and whether
  the window was open, and at launch MUST reopen the window on the last drive
  if it was open at exit. "Show deleted files" and which overlays are on MUST
  be saved with the rest of this state. The zoom, the pan, the selection and
  the decode settings MUST last only while the window shows the same disk.
  "Recompute the checksum" MUST start on for each sector the user edits, and
  the comparison options ("Ignore sync widths and counts", "Ignore volume
  numbers" and "Ignore dates") MUST last only while comparing.

**Performance**

- **FR-081**: Analysis and drawing MUST NOT slow emulation beyond the limits
  in SC-005, including with the window open and following the head.
- **FR-082**: Zooming and panning the platter and the strip MUST stay smooth
  at every zoom level (see SC-004).

**File and sector map**

- **FR-083**: The inspector MUST build a file and sector map of every DOS 3.3,
  ProDOS, Apple Pascal and CP/M volume of 35 tracks of 16 sectors, from the
  sectors the analyzer decoded under the active decode settings (FR-019) and
  their results (good, bad, not checked or missing), so the map matches the
  Sector data tab and what the drive reads, and is the same in every host.
  Casso has no Apple Pascal or CP/M reader, so 040 MUST add read-only readers
  for both, sufficient for the map and the file comparison (FR-120). A disk
  with nonstandard marks MUST be mapped once the decode settings decode its
  sectors. The map MUST be built again from the analysis, without decoding any
  track again, whenever the sectors it reads change: after a guest write or an
  applied edit is analyzed (FR-069), on a reload, and on a change of decode
  settings.
- **FR-084**: A DOS 3.3 volume MUST be found when track 17 sector 0 decodes as
  a VTOC giving 256 bytes per sector, a track count, a sector count, and a
  catalog track and sector inside the volume those counts give. A ProDOS
  volume MUST be found when block 2 decodes as a volume directory key block
  (storage type $F, previous block 0, entry length $27, 13 entries per block).
  The volume's size MUST be read from the VTOC's track and sector counts or
  from the ProDOS header's total blocks, and a volume of any size other than
  35 tracks of 16 sectors or 280 blocks MUST give the not-mapped state with
  that size (FR-094). When more than one volume is found on a disk under this
  requirement and FR-085, each MUST be mapped, with a choice between them in
  the File map tab. Volume track N MUST be read from quarter track N, which
  these systems read after stepping to a whole track. Each DOS 3.3 logical
  sector, each half of a ProDOS or Pascal block, and each CP/M sector MUST
  come from the physical sector that holds it under the system's own skew,
  read from the first field in passing order with that physical sector number
  and a good or not-checked address field, and a quarter track N with no
  record makes every sector of track N missing. A repeated sector number on a
  track already produces FR-015's finding and MUST NOT produce a second one in
  the map.
- **FR-085**: An Apple Pascal volume MUST be found when block 2 begins with a
  volume header giving first block 0, next block 6 and file type 0, a volume
  name of 1 to 7 printable characters, at least 6 blocks in all and at most 77
  files. Its size MUST be read from the header's block count, and a Pascal
  volume of other than 280 blocks MUST give the not-mapped state with that
  size (FR-094). Each half of Pascal block b MUST come from the physical
  sector that holds that half of ProDOS block b. A CP/M volume MUST be looked
  for only when no DOS 3.3, ProDOS or Pascal volume is found. It MUST be found
  when the 64 directory entries in CP/M sectors $0 to $7 of track 3 include at
  least one valid entry in use and no invalid entry, or more than four valid
  entries in use whatever the invalid ones. An entry is invalid when it holds
  a user number above 31 other than $E5 (free), an extent number or record
  count CP/M 2.2 does not allow, a control character in its name, or a block
  number outside the volume other than on the system entry of user 31
  (FR-086). A directory whose entries are all free MUST count as an empty CP/M
  volume. CP/M sector n of a track MUST come from physical sector 3n mod 16,
  and allocation block b MUST be CP/M sectors 4(b mod 4) to 4(b mod 4)+3 of
  track (3 + b div 4) mod 35, so blocks 0 to 127, the volume's 128 blocks of
  1 KB, cover tracks 3 to 34, and blocks 128 to 139, which only the system
  entry of user 31 uses, cover tracks 0 to 2.
- **FR-086**: The map MUST give every sector of the volume, each DOS 3.3 track
  and logical sector, each ProDOS or Pascal block, or each CP/M track and
  sector, one role: free; boot and DOS image (DOS 3.3 tracks 0 to 2), boot
  blocks (ProDOS and Pascal blocks 0 and 1) or system area (CP/M tracks 0 to
  2); VTOC (DOS 3.3) or volume directory key block (ProDOS); catalog sector,
  volume directory block, Pascal directory block (blocks 2 to 5) or CP/M
  directory block (blocks 0 and 1); unused catalog track sector (a sector of
  the catalog track the VTOC gives that the VTOC marks used and no catalog
  sector links to); volume bitmap (ProDOS), read from the block the volume
  header gives; subdirectory block; track/sector list (DOS 3.3), or index,
  master index or extended key block (ProDOS, the last for a forked file);
  file data; bad blocks the file system recorded (a Pascal .BAD file);
  allocated but unowned; owned but marked free; or cross-linked, with every
  owner listed. A sector that one file owns MUST take that file's role, and a
  sector that more than one file owns is cross-linked. Allocated but unowned
  and owned but marked free MUST apply only to DOS 3.3 and ProDOS, whose
  volumes record free space; on a Pascal or CP/M volume, a sector no file owns
  is free. DOS 3.3 tracks 0 to 2 MUST be boot and DOS image only where the
  VTOC marks them used and no file owns them, and track 0 sector 0 MUST be
  boot whenever no file owns it. The CP/M system entry of user 31, whose
  blocks lie past the volume's 128 and fall on tracks 0 to 2, MUST give those
  tracks the system area role and MUST NOT count as a file. The boot and
  system areas MUST NOT count as owners, so they never make a sector
  cross-linked. Apart from its role, each sector MUST show its result: good,
  bad, not checked or missing. A ProDOS or Pascal block's result MUST be the
  worse of its two halves' results, in the order good, not checked, bad,
  missing, and its tooltip MUST give each half's result. Each role and each
  result MUST be shown by color and by symbol or pattern (FR-079), and every
  pair of role colors MUST differ by at least ΔE2000 10 in each theme
  (FR-077).
- **FR-087**: The map MUST follow every DOS 3.3 catalog entry and every ProDOS
  directory entry, including subdirectories at any depth and both forks of a
  forked file, and MUST give each file: its path, type, locked state, size as
  the catalog or directory records it, the sectors it uses in file order with
  the role of each, the holes of a sparse file at their places in that order,
  and whether its chain was followed to the end. A DOS 3.3 file's sectors MUST
  be given list by list, each track/sector list followed by the data sectors
  it lists. The map MUST also follow every Apple Pascal and CP/M directory
  entry and give each file the same items. A Pascal file's sectors MUST be its
  run of blocks from its first block up to, but not including, its next block,
  its size the blocks of the run less one, times 512, plus the bytes its entry
  gives for the last block, and its type the one its entry gives (such as
  .TEXT or .CODE); a Pascal file has no locked state. A CP/M file's path MUST
  be its user number and name (for example "0:PIP.COM"), its locked state its
  read-only attribute, its sectors its extents' blocks in extent order with a
  hole for each block number of 0, and its size 128 bytes for each record its
  extents give.
- **FR-088**: The map MUST also list deleted files whose entries remain in a
  catalog or directory, marked as deleted, with the sectors their entries
  still lead to, and how many of those are free and how many another file now
  uses. Deleted files MUST NOT own sectors, so they never produce a
  cross-link, allocated-but-unowned or owned-but-free finding. They MUST be
  hidden until "Show deleted files" is turned on. On a CP/M volume, a deleted
  file is a free entry ($E5) that still holds a printable name and block
  numbers. Apple Pascal removes a file's entry when it deletes the file, so a
  Pascal volume lists no deleted files.
- **FR-089**: A "File map" disk tab MUST show a grid with one row per track, 0
  to 34, and one cell per DOS 3.3 logical sector, $0 to $F, on a DOS 3.3
  volume, per ProDOS or Pascal block, eight per track, on a ProDOS or Pascal
  volume, or per CP/M sector, $0 to $F in CP/M's own order, on a CP/M volume,
  each cell showing its role and result. Below the grid, a file list MUST show
  each file's path, type, size, sectors used and state, sortable by each
  column and filterable to "Files touching bad sectors". The state MUST list
  every one that applies, in this order: "Chain broken", "Cross-linked",
  "Touches bad sectors"; a file with none of them is "Complete", and a deleted
  file shows "Deleted" alone. A file touches bad sectors when it uses a sector
  that is bad or missing, including a sector on a damaged record; a sector
  that is not checked shows only through its result (FR-093). A cell's tooltip
  MUST show the DOS 3.3 logical sector and the physical sector that holds it,
  the ProDOS or Pascal block and the physical sectors that hold its two
  halves, or the CP/M sector, its allocation block and the physical sector
  that holds it, its role and result, and each owner with the sector's place
  in it (for example "HELLO, data sector 3 of 5").
- **FR-090**: Selecting a file MUST mark its sectors in the grid, numbered in
  file order, and on the platter and the strip while the "Files" overlay is
  on, and MUST select its first sector. "Next sector in file" and "Previous
  sector in file" MUST step through its sectors in file order, across tracks.
  Selecting a grid cell MUST select the physical sector that holds it, or for
  a ProDOS or Pascal block the physical sector that holds its first half, in
  every view. For the selected sector, the Sector data header MUST show its
  role and owners and, for a file, its place in the file.
- **FR-091**: The platter MUST have a "Files" overlay that draws each
  standard sector's data field in its role's color and marks the selected
  file's sectors.
- **FR-092**: The map MUST NOT hang or fail as a whole on a damaged volume. It
  MUST stop following a chain at a loop, at a pointer outside the volume, or
  once the chain is longer than the volume can hold, MUST keep what it read
  before that point, and MUST still show the rest of the map. While following
  a chain, the map MUST read good and not-checked sectors and MUST stop at a
  bad or missing sector of the chain itself (a catalog or directory sector, a
  track/sector list or an index block), giving it as the reason (FR-093). When
  a catalog or directory sector is bad or missing, the file list MUST show
  that the catalog is incomplete and which sector could not be read, never a
  short or empty catalog with no explanation.
- **FR-093**: Findings MUST add a file system category with: cross-linked
  sectors, with every owner; sectors allocated but unowned; sectors a file
  owns that the bitmap marks free; chains that could not be followed, with the
  place and the reason (a loop, a pointer outside the volume, a bad or missing
  sector, or a chain too long); files that use bad or missing sectors; one
  finding for the disk when the map was built from sectors not checked; a
  DOS 3.3 catalog sector count or ProDOS blocks-used count that differs from
  the sectors the file uses; on an Apple Pascal volume, directory entries out
  of block order, entries that run past the volume's end or whose next block
  is not after their first, and a file count that differs from the entries;
  and on a CP/M volume, invalid directory entries (FR-085) and two entries for
  the same user number, name and extent. Blocks that two Pascal entries or
  two CP/M entries use are cross-linked sectors. The boot and system areas,
  the catalog track, sparse files, entries that use no sectors, deleted
  files, Pascal .BAD files and the CP/M system entry of user 31 MUST NOT
  produce a finding, so a standard DOS 3.3, ProDOS, Pascal or CP/M disk
  produces no file system findings.
- **FR-094**: When the disk holds no DOS 3.3, ProDOS, Apple Pascal or CP/M
  volume of 35 tracks of 16 sectors (FR-084, FR-085), the File map tab MUST
  show why: no file system the map reads was found; the disk is a 13-sector
  disk, which is not mapped; or the volume has another size, which is not
  mapped, with that size. When no file system is found and track 17 sector 0,
  block 2 or a CP/M directory sector of track 3 is bad or missing, the reason
  MUST give that sector as the likely cause. The grid MUST still show each
  sector's result by track and physical sector.
- **FR-095**: "Go to" MUST also accept a file on a mapped volume and select
  its first sector. Find results in decoded sector data MUST show the file
  that owns each sector. Copy MUST put the selected rows of the file list on
  the clipboard as tab-separated text, and "Copy map" MUST put the grid on
  the clipboard as text, one row per track with one letter per sector and a
  key to the letters.

**Sector editing**

- **FR-096**: Editing MUST change only a sector's 256 decoded data bytes. A
  sector MUST be editable when its data field pairs with an address field
  whose checksum passes or is not checked (FR-020); its data prologue, every
  nibble of its body and its checksum nibble were read with no noise or
  random-bit region among them; every body nibble is in the standard 6-and-2
  or 5-and-3 table (in the later release, the table FR-124 sets); and its
  quarter track is not damaged. A sector whose data checksum fails MUST be
  editable, so a damaged sector can be repaired. 6-and-2 and 5-and-3 sectors
  MUST both be editable, in every format FR-003 lists and on bit and flux
  tracks. 4-and-4 sectors, which the later release decodes (FR-124), MUST NOT
  be editable. For any other sector, "Edit sector" MUST be unavailable and the
  Sector data tab MUST show why ("No data field", "Address checksum failed",
  "Noise inside the data field", "Nibbles outside the translate table",
  "Damaged track record", or in the later release "4-and-4 data" or "Custom
  track format", FR-144).
- **FR-097**: "Edit sector" MUST make the Sector data tab's hex and text
  columns editable, overwriting in place so a sector always holds 256 bytes.
  Typing a hex digit MUST change the hex digit under the cursor; typing a
  character in the text column MUST change the byte under the cursor and keep
  the high bit that byte had. Pasting MUST accept hex text in the hex column
  and text in the text column, MUST overwrite from the start of the selection
  for as many bytes as were pasted, MUST stop at offset $FF, and MUST show how
  many bytes did not fit. Changes MUST be held as pending edits, which change
  nothing until applied and can span several sectors and tracks. Changed bytes
  MUST be marked by color and symbol in the hex and text columns, each sector
  with pending edits MUST be marked in the sector row, the Tracks tab and the
  File map grid, and a count MUST show the bytes and sectors changed. Ctrl+Z
  and Ctrl+Y MUST undo and redo pending changes one action at a time and MUST
  act only on pending edits; applied edits change only through "Undo applied
  edit" and "Redo applied edit" (FR-107). "Discard" MUST drop every pending
  edit, and "Discard sector" those of the selected sector.
- **FR-098**: "Recompute the checksum" MUST be on by default, so an applied
  edit gives a sector whose data checksum passes, and each sector with pending
  edits MUST have its own setting. Turned off, the stored checksum nibble MUST
  be written unchanged, so a sector whose stored checksum is part of the
  disk's format keeps it; whether the sector then reads as good or bad depends
  on the new bytes, and the header shows which (below). On DSK, DO and PO,
  which store no checksum, "Recompute the checksum" MUST be on and
  unavailable, with a tooltip that shows that sector images store only sector
  data. For a sector with pending edits, the Sector data header MUST show the
  data checksum result the sector will have once applied.
- **FR-099**: Applying an edit MUST change the sector through the shared
  sector writer (FR-110), so only the cells of the data field's body and
  checksum nibble change, and the prologue, the epilogue, every other cell of
  the track and the track's length stay as they were (FR-111). The field MUST
  be encoded as it was decoded, 6-and-2 or 5-and-3 with the standard table (in
  the later release, the table FR-124 sets). The edit MUST change the field at
  the selected sector's place on the track, not the first field with the same
  sector number. On a quarter track mapped in both TMAP and FLUX, the
  confirmation MUST show that only the flux record changes and the bit record
  is left as it was (FR-111).
- **FR-100**: "Apply" MUST apply every pending edit on the disk. It MUST first
  show what will change (the sectors, the quarter tracks that read each
  changed track record, and the file that will be saved) and MUST change
  nothing until the user confirms. A track record that several quarter tracks
  read MUST be changed once. Applying MUST be all or nothing: if any pending
  sector cannot be changed as FR-099 requires, nothing MUST change, and the
  window MUST show which sector and why. Before an edit is saved, the changed
  tracks MUST be analyzed again, and if any edited sector does not decode to
  its new bytes with the expected checksum result, or any other field on those
  tracks does not decode as it did before, every change MUST be undone and the
  window MUST show the failure. An edit MUST be shown as applied only after
  the file holds it (FR-104, FR-105), and the changed tracks MUST then be the
  only ones analyzed again.
- **FR-101**: "Save edited copy..." MUST write the disk as the window shows it
  now (in Casso, the drive's disk with its unsaved guest writes and applied
  edits), with every pending edit applied and checked as FR-100 requires, all
  or nothing, to a new file the user chooses. The file MUST be in the format
  Casso saves the image's format in, with that format's extension, NIB and NB2
  tracks MUST be written as FR-104 gives, and the file MUST appear whole or
  not at all (FR-116). The Save dialog MUST NOT accept the disk's own file or
  a file mounted in either drive. For a damaged image, the window MUST show
  before saving that the copy gets a new checksum and what it holds for each
  damaged record. Afterward the pending edits MUST stay pending, and the disk
  in the drive and its file MUST be unchanged. "Save edited copy..." MUST be
  available whenever edits are pending.
- **FR-102**: Editing in place MUST NOT be available on a write-protected
  disk. In Casso that is while any cause in FR-065 holds, or after the user
  chose to keep the disk in memory when Casso found its file changed outside
  Casso, for as long as Casso holds its saves. In Explorer it is while the
  image's write-protect flag is set, the file is read-only, the user cannot
  write the file, or the image is damaged (a checksum mismatch or a damaged
  track record, FR-053). Pending edits and "Save edited copy..." MUST still be
  available; "Apply", "Undo applied edit" and "Redo applied edit" MUST NOT be,
  and the window MUST show each cause beside "Apply". An edit MUST NOT be put
  into a disk whose changes Casso would not save.
- **FR-103**: In Casso, an edit MUST reach the disk in the drive only at a
  safe point: the drive is not writing, any guest write to the track has been
  completed, and the head is outside every data field the apply changes. While
  the drive is not turning (its spin-down after the motor turns off has
  ended), every moment is a safe point. The machine MUST be held paused from
  the moment the drive's disk changes until the apply is checked and saved, or
  rolled back (FR-100, FR-104), and MUST then run again if it was running.
  While the drive turns and the machine runs, a confirmed "Apply" MUST show
  "Waiting for the drive to stop", with "Pause and apply" and "Cancel", and
  MUST apply the edit once the drive stops turning. "Pause and apply" MUST let
  the machine run to the next safe point, pause it there, apply, and then let
  the machine run again. When the user has paused the machine away from a safe
  point, "Apply" MUST show why it cannot apply yet and offer "Run to a safe
  point and apply", which does the same and leaves the machine paused
  afterward. While an apply waits, pending edits MUST NOT change, and a guest
  write to a track the apply changes MUST end the wait, check the pending
  edits on that track again (FR-108) and show the confirmation again. Before
  the first apply on a disk, the window MUST show a note that DOS or ProDOS
  may hold the sector in memory and write its own copy back over the edit.
- **FR-104**: In Casso, applying MUST save the disk to its file at once,
  through Casso's usual save and its checks and durably (FR-116), before the
  machine runs again, and MUST show the result: saved; saved to a preserved
  copy, when Casso finds at save time that the file changed outside Casso,
  with the copy's file name; or not saved, with the reason. When the save
  fails, every track record the apply changed MUST be restored in the drive to
  what it was before the apply, as undo does, before the machine runs again;
  the pending edits MUST stay pending; and the window MUST show "Not applied:
  the file could not be saved" with the reason, so the guest never reads an
  edit the file does not hold. When the save goes to a preserved copy, the
  drive holds the copy, the edit counts as applied to the copy, the window
  MUST show the copy's file name, and the undo history continues against the
  copy. The save MUST also hold any guest writes not yet saved, as every Casso
  save does. Each track MUST keep its kind when saved, a flux track as a flux
  track and a bit track as a bit track, and a track with no edit and no guest
  write MUST be saved as it was (spec 038). A WOZ 1 is saved as WOZ 2, as
  Casso already does, and the window MUST show this before the first apply.
  This changes Casso's NIB and NB2 save: a track whose only change is an
  applied edit, an undo or a redo MUST be saved by writing the field's new
  bytes into the stored track at their place, with every other nibble
  unchanged and in place. A track with both an edit and a guest write MUST be
  rebuilt as Casso saves a guest-written track today, and the window MUST show
  that before the apply.
- **FR-105**: In Explorer, applying MUST change the image through the shared
  sector writer (FR-110), the same writer Explorer's own writes use, never by
  rebuilding a track, and MUST save it the way Explorer commits its other
  changes to an image: only when the file has not changed since the window
  read it, by replacing the file whole and durably once the new contents are
  written (FR-116), and then by notifying a running Casso, with the reload
  intent, that the file changed, as Explorer does for its other writes to a
  mounted image (033's FR-024). When Casso's reply is anything other than a
  reload, such as a conflict, the window MUST show it, as 033's FR-024
  requires, so that both Casso's and Explorer's windows show which file the
  drive holds. When the file has changed since the window read it, nothing
  MUST be saved, and the window MUST analyze the new file (FR-074) and keep or
  drop the pending edits as FR-108 gives. When the save fails, the file MUST
  be unchanged, the pending edits MUST stay pending, and the window MUST show
  "Not applied: the file could not be saved" with the reason.
- **FR-106**: An edit that Explorer's window saves to an image a running Casso
  has in a drive is a change to the file made outside Casso. Casso MUST handle
  it exactly as it handles Explorer's other writes to a mounted image
  (033's FR-024): as a change made outside Casso with the reload intent,
  through Casso's external-change handling, which spec 041 reworks. 040 MUST
  NOT add a rule of its own for it. Casso's inspector on that drive MUST show
  the file the drive holds afterward, as it does after any reload or move to a
  preserved copy.
- **FR-107**: "Undo applied edit" and "Redo applied edit" MUST undo and redo
  applied edits one apply at a time, restoring each changed track record
  exactly as it was, every cell or every flux transition, not by encoding the
  old bytes again. Each MUST need the user's confirmation, MUST reach the
  drive only as FR-103 allows, and MUST save as "Apply" does (FR-100, FR-103,
  FR-104, FR-105), with NIB and NB2 tracks saved as FR-104 gives. "Undo
  applied edit" MUST be unavailable, with the reason shown, once the guest has
  written to a track record the apply changed, or in Explorer once the file
  has changed outside the window. "Redo applied edit" MUST be unavailable,
  with the reason shown, once the guest has written to a track record the undo
  restored, or in Explorer once the file has changed outside the window. Both
  MUST be unavailable while any cause in FR-102 holds, with each cause shown.
  A new apply MUST clear the redo history. The history MUST last until the
  disk is ejected, replaced or reloaded, or the window closes.
- **FR-108**: Pending edits MUST last while the window shows the disk.
  Switching drives, closing the window, quitting Casso or Explorer, a machine
  change that removes or replaces the disk, "Compare disk images" on an image
  whose window holds pending edits (FR-117), and a change of decode settings
  after which a sector with pending edits no longer decodes MUST need the
  user's confirmation, which shows that those pending edits will be discarded.
  A machine change that keeps the same disk in the drive MUST keep the pending
  edits. When the user ejects or replaces the disk outside this window, the
  pending edits MUST be discarded, and the window MUST show that they were.
  When the guest writes to a track record that holds pending edits, when an
  undo or redo changes it, or when the disk is reloaded because its file
  changed (in Casso by Casso, in Explorer by the window, FR-074), each pending
  edit on it MUST follow the field with the same sector number and encoding
  nearest its old cell, as FR-038 does for the selection. A pending edit whose
  sector's decoded bytes now differ from its bytes when editing began MUST be
  marked out of date; applying it MUST need the user's confirmation, which
  shows that the sector changed, and MUST then write only the bytes the user
  changed over the sector's new contents. A pending edit whose sector no
  longer decodes as editable (FR-096) MUST be discarded, and the window MUST
  list each one discarded. Pending edits MUST NOT change while an apply waits
  (FR-103).
- **FR-109**: The Tracks tab MUST mark each quarter track whose record an
  applied edit changed, apart from the marks for guest writes (FR-047). An
  edit MUST NOT count as a guest write (FR-068).

**Shared sector writer**

- **FR-110**: Every direct change to an existing sector of a disk image, other
  than a guest write through the emulated drive or formatting, MUST go through
  one shared sector writer, and these MUST all use it: the inspector's editor
  (FR-099); every write of the `disk` command that changes sectors of an image
  (`sectorwrite`, `blockwrite`, `put`, `delete`, `boot` and the assemblers'
  `--disk` output); and every change Casso Explorer makes to the sectors of an
  image. The writer MUST replace only the nibbles of the sector's data field
  body and checksum inside the existing track, and MUST keep the address field
  with its volume number, the data prologue and epilogue, every sync run and
  gap, every other field, the track's length and the place of every sector on
  it. A caller MUST NOT rebuild a track to change a sector.
- **FR-111**: On a WOZ bit track, each new nibble MUST take the cells of the
  nibble at the same place in the field and MUST be followed by as many extra
  zero cells as followed that nibble, so the field keeps its length in cells
  and its timing bits. On a flux track, each cell of the new field MUST take
  the time the cell at the same place took, so the track stays a flux track
  (spec 038), every stretch keeps its speed, the time of one turn is
  unchanged, and every flux transition outside the field is unchanged. A
  cell's recorded time is its share of the interval that holds it: an interval
  the drive reads as n cells gives each of its cells the interval's time
  divided by n. The new field MUST place each of its transitions at the
  field's start plus the recorded times of the cells before it in the field,
  so the field's total time is unchanged. On NIB and NB2, the field MUST
  change in whole nibbles in place. On DSK, DO and PO, the writer MUST change
  the 256 bytes of the logical sector the physical sector holds. On a quarter
  track mapped in both TMAP and FLUX, which Casso plays as flux (spec
  038's FR-002), only the flux record MUST change. A field that spans the end
  of the track MUST be changed whole. The writer MUST find the record of whole
  track N through the image's map at quarter track N, never by taking the
  image's Nth record as track N, and MUST change a record that several quarter
  tracks read once. The `disk` command's and Explorer's sector, block and file
  reads MUST find whole track N through the map in the same way, so every byte
  a write changes comes from the record it writes.
- **FR-112**: A 6-and-2 data field of 256 bytes always holds 342 nibbles and a
  checksum nibble, a 5-and-3 field always holds 410 and a checksum nibble, and
  every valid nibble takes 8 cells, so a new field never differs in nibble
  count from the field it replaces and, with each nibble's extra zero cells
  kept, never differs in length in cells. The writer MUST NOT insert or remove
  a nibble or a cell, so no other field moves. The later release adds no other
  editable encoding: 4-and-4 data is not editable (FR-096), and a custom
  translate table (FR-124) changes nibble values, not their count. If a
  field's nibble count ever differs from what its encoding gives, the writer
  MUST change nothing on the image and MUST give the reason.
- **FR-113**: The shared sector writer, which changes the image directly
  rather than through the emulated drive, MUST keep each cell's recorded time
  on a flux track (FR-111). A guest write through the emulated drive MUST stay
  at the drive's own cell timing, as a real drive writes at its own speed.
  This changes shipped behavior and is a fix delivered with 040: spec
  038's FR-013 writes a sector to a flux track at the controller's nominal
  cell timing, which replaces the field's recorded speed, and this requirement
  replaces that rule for every direct write, the `disk` command's and
  Explorer's included.
- **FR-114**: For the `disk` command and Explorer, a quarter track between
  whole tracks N and N+1 that is unmapped, or that maps to the record of track
  N or of track N+1, MUST NOT stop a write. This accepts the standard WOZ
  layout, in which N-0.25, N and N+0.25 map to track N's record, images that
  also map N+0.5 to a neighboring whole track's record, and the layout Casso
  writes. A WOZ with a quarter track mapped to any other record, which holds
  data of its own between whole tracks, MUST stay protected as a whole: every
  write to it MUST fail, leave the file as it was, and give that quarter track
  and its record as the reason. The inspector's editor is not bound by this
  rule; its confirmation lists every quarter track that reads a changed
  record (FR-100).
- **FR-115**: For the `disk` command and Explorer, a write MUST change a track
  only when the track decodes completely: each of its 16 sectors is found
  exactly once, with standard marks and good address and data checksums. A
  write that needs any other track MUST fail, leave the file as it was, and
  give the track and the reason. This includes a track with no standard
  address field, unformatted or in a format the decoder does not read, which
  today's writers format before writing to it, and a quarter track with
  nothing recorded, including an empty track Casso keeps for the guest to
  format: the writer changes only data fields that exist and MUST NOT lay down
  a track to make room for a sector. The inspector's editor changes a sector
  under FR-096's rules instead, so a damaged sector can be repaired.
- **FR-116**: Every save of an image the shared sector writer changed, and
  every save of an applied, undone or redone edit, MUST be durable: once the
  save is reported, the file holds the old contents or the new ones whole,
  never a mix, a truncated file or zeros, and a save that fails at any step
  MUST leave the file as it was. A new file written by "Save edited copy..."
  MUST appear whole or not at all, and an existing file the user confirms
  replacing MUST then hold its old contents or the new ones whole.

**Comparison**

- **FR-117**: "Compare with..." MUST compare two disks, A and B. A is the
  window's disk unless the user chooses another, and each of A and B MUST be
  one of: an image file in any format FR-003 lists; in Casso, a drive's disk
  as the drive holds it now, that disk as it was inserted or last reloaded
  ("As inserted"), and that disk's file as saved now ("Its file"); in
  Explorer, the window's image as it is now or as the window first read it
  ("As first opened"). In Casso the user can then compare the disk now with
  the disk as inserted (what the guest wrote), the disk now with its file
  (what is not yet saved), its file with the disk as inserted (what Casso's
  saves changed), and drive 1 with drive 2. Casso MUST keep each drive's disk
  as it was inserted or last reloaded for this. Both disks MUST be analyzed
  with the window's decode settings unless the user gives B settings of its
  own, and a change of decode settings MUST analyze and compare again each
  disk it applies to (FR-069). In Explorer, "Compare disk images" on the
  right-click menu of exactly two selected disk images MUST open one window
  comparing them, with the image the user right-clicked as A. A comparison
  window counts as A's window (FR-073): when A already has a window, "Compare
  disk images" MUST switch that window to the comparison, after FR-108's
  confirmation if it holds pending edits. An image may be B in any number of
  windows and is never edited as B. "Swap A and B" and "Stop comparing" MUST
  be available while comparing, and comparing MUST NOT change either disk.
- **FR-118**: Comparison MUST compare what the drive reads on each disk. A
  quarter track at which neither disk holds a record MUST show "Nothing
  recorded", which counts as matching. Each whole track N MUST be compared
  once, A's record at N against B's. A quarter track N.25, N.5 or N.75 at
  which each disk holds either nothing or the same record that disk holds at
  whole track N or N+1, as the standard WOZ layout and the layout Casso builds
  for sector, NIB and NB2 images do (the layouts FR-048 exempts), MUST show
  "Standard layout", with each side's record in its tooltip, and MUST NOT be
  compared on its own. Every other quarter track MUST be compared with the
  other disk's record at the same quarter track. Each compared track MUST get
  one verdict, the strongest that holds: "Identical", the same cells from the
  same start, and on two flux tracks the same transitions within the timing
  tolerance (Assumptions); "Same cells", the same loop of cells, with a note
  giving the rotation in degrees, that the flux timing differs, or that only
  one side records flux timing; "Same nibbles", the same nibbles in the same
  order once aligned (FR-119), differing only in sync widths or counts, extra
  zero cells, rotation or length, with the rotation and the length difference
  in cells; "Same sector data", every sector's 256 bytes, sector number and
  checksum results the same while the nibbles differ, as for a DSK and a WOZ
  of one disk, with volume numbers taking no part (FR-120); "Sectors differ",
  with the number of sectors that differ, which is every sector when no
  sectors pair, as for a 13-sector track against a 16-sector one; "Nibbles
  differ", where neither side has standard sectors; "Only in A" or "Only in
  B"; or "Not compared", with the reason, such as a damaged record. A bit
  track compared with a flux track MUST get at most "Same cells". A track not
  compared MUST NOT be shown or counted as matching.
- **FR-119**: To compare nibbles, B's track MUST be aligned to A's on the
  first address field both hold, or, with none, on the rotation at which the
  most cells match, and the differences MUST then be found allowing for
  inserted and deleted nibbles, up to the alignment limit (Assumptions), so a
  sync run of another length marks only that run as different and not
  everything after it. Random-bit regions MUST be marked on both sides and
  never compared. Flux timing MUST be compared only between two flux tracks,
  cell by cell after alignment, within the timing tolerance (Assumptions).
  "Ignore sync widths and counts" MUST leave differences confined to sync runs
  out of the Differences tab.
- **FR-120**: Sectors MUST be paired within each compared track by sector
  number and encoding, with repeated sector numbers paired in passing order,
  and each pair MUST show its DOS 3.3 logical sector and ProDOS block, and on
  a mapped CP/M volume its CP/M sector and allocation block. A difference in
  an address field's volume number MUST be listed as its own difference unless
  "Ignore volume numbers" is on. When both disks hold a volume the map reads
  (FR-083), the comparison MUST also pair files by path, in catalog order
  where paths repeat, and give each pair: same contents; contents differ, with
  the number of bytes that differ and the first offset that differs; type,
  locked state or other attributes differ; only in A; only in B; or not
  compared, with the reason, such as a bad sector in its chain. A ProDOS
  fork's contents are its bytes up to its EOF, with sparse holes read as zero
  bytes, a DOS 3.3 file's contents are every byte of its data sectors in file
  order, with holes read as zero sectors, an Apple Pascal file's contents are
  its bytes up to the size FR-087 gives, and a CP/M file's contents are its
  records in extent order, 128 bytes each, with each hole read as a block of
  zero bytes; a difference in length MUST be reported as its own difference.
  The file comparison MUST compare A's volume shown in the File map tab
  (FR-084) with B's volume of the same file system, and when B holds none, the
  file comparison MUST show why it is not given, as for disks with different
  file systems. "Ignore dates" MUST leave ProDOS creation and modification
  dates and the Apple Pascal modification date out of the file comparison.
  "Ignore sync widths and counts", "Ignore volume numbers" and "Ignore dates"
  MUST be off by default and MUST change only the Differences tab and the file
  comparison, never a track's verdict. Selecting a file pair MUST mark the
  sectors that differ within it.
- **FR-121**: While comparing, the window MUST show: a comparison bar under
  the toolbar with A's and B's sources and file names, "Previous difference",
  "Next difference", "Swap A and B" and "Stop comparing"; a "Differences" disk
  tab listing every difference with its quarter track, sector, cell and kind,
  ordered, sortable and filterable as Findings are (FR-049); a chip with the
  result, counting each compared track once and leaving out tracks that show
  "Nothing recorded" or "Standard layout" (for example "31 identical · 3
  differ · 1 only in B", 35 tracks in all); a "Comparison" column in the
  Tracks tab with each quarter track's verdict; a "Differences" platter
  overlay marking where tracks differ; B's strip below A's, aligned, with zoom
  and pan linked and differences marked on both; in the Sector data tab, B's
  bytes beside or below A's, with the bytes that differ marked by color and
  symbol; and in the Nibbles tab, the nibbles that differ marked, with B's
  value in each one's tooltip. "Next difference" and "Previous difference"
  MUST step through the differences across the disk, and Copy MUST put the
  selected differences on the clipboard as tab-separated text. With no
  differences, the Differences tab MUST show "No differences". Pending edits
  MUST NOT be part of A, and "Edit sector" MUST be unavailable, with the
  reason, while A is not the window's disk as it is now.
- **FR-122**: When a disk in a comparison changes by a guest write or an
  applied edit, only the changed tracks MUST be compared again, as FR-069
  analyzes them, and their verdicts MUST show "Comparing" until done. When a
  file in a comparison changes on disk, it MUST be analyzed and compared again
  whole, except that when B is a disk's file ("Its file") and Casso saves that
  disk, only the tracks the save changed MUST be compared again. When a disk
  in a comparison is ejected or replaced, the comparison MUST end and the
  window MUST show why. When a file in a comparison is deleted or renamed, the
  window MUST keep the last comparison and show that the file is no longer
  there.
- **FR-123**: While comparing, "Use B's bytes" MUST put B's 256 bytes for the
  selected sector into A's matching sector as a pending edit (FR-097), when A
  is the window's disk as it is now and its sector is editable (FR-096).

**Preservation, later release**

These requirements belong to User Stories 11 to 16 and ship in the later
release (Delivery). The first release MUST NOT depend on them.

- **FR-124**: The decode settings (FR-019) MUST also set, for a range of
  tracks, the data field's encoding (6-and-2, 5-and-3, or 4-and-4, which takes
  two nibbles per byte and has the data checksum check off) and its translate
  table: the standard table; the standard table with pairs of entries swapped,
  such as D5 and 9B; or a whole 64-entry 6-and-2 or 32-entry 5-and-3 table
  pasted as hex. They MUST also set, for a range of tracks, a starting value
  other than zero for the address checksum and for the data checksum, and a
  rule for sector numbers: doubled, offset by a constant, or combined with a
  constant by exclusive OR. A table with a repeated entry, or with an entry
  whose high bit is clear, which the drive never delivers, MUST NOT be
  accepted, and the settings MUST show that entry. Entries that are mark bytes
  or are outside the standard table MUST be accepted with a warning. A sector
  decoded with a nonstandard encoding, table, checksum starting value or
  sector-number rule MUST show them in the Sector data header and the Fields
  tab, and an edit to a 6-and-2 or 5-and-3 sector decoded with them (FR-099)
  MUST be encoded with them; a 4-and-4 sector is not editable (FR-096).
- **FR-125**: A table MUST also be read from the disk, at a track, sector and
  offset the user gives, or by "Search for tables", which searches the
  decoded sectors of track 0 for runs of 64, or 32, distinct valid nibbles
  and lists each candidate with its location.
- **FR-126**: Where data fields hold nibbles outside the active table,
  Findings MUST give each track with the values and their counts (for example
  "Data fields on track 3 hold nibbles outside the 6-and-2 table: D5 ×812").
  This finding for each track MUST replace FR-015's findings for each field
  for the same nibbles, so each is listed once. "Solve table" MUST find the
  swaps of the standard table under which the most data checksums pass, and
  among those, the fewest, show them with the number of sectors that pass with
  and without them, and add them to the decode settings only when the user
  confirms. When no set of swaps within the search limit (Assumptions) makes
  the checksums pass, it MUST show that the table cannot be found from
  checksums alone.
- **FR-127**: "Save decode settings..." and "Load decode settings..." MUST
  write and read every decode setting, with its track range, as a file the
  user chooses, so settings found on one disk can be used on others. Loading
  MUST count as a change of decode settings (FR-069).
- **FR-128**: The Nibbles tab and the strip MUST have a framing control: "As
  the drive reads", the default; "Slip after this nibble", by 1 to 7 cells;
  and "Start framing at cell", at a cell the user gives. A framing other than
  the default MUST be shown as a second lane under the normal nibbles, lined
  up in time with them, with the place where the two framings rejoin marked,
  and MUST NOT change the analysis, the sectors, the map or the findings.
- **FR-129**: The analyzer MUST scan each track, outside standard fields, for
  places where slipping 1 to 4 cells after a nibble, so that the latch takes
  in extra zero cells the normal framing skips, gives a run of valid nibbles
  that differs from the normal framing for at least the minimum run
  (Assumptions) before the two framings rejoin. Each place MUST be a finding
  giving the track, the cell, the normal nibbles and the slipped nibbles for
  each slip that gives such a run (for example "Normal D5 E7 E7 E7; slipped 3
  or 4 cells: EE E7 FC EE E7 FC EE EE FC"), and selecting it MUST show that
  framing in the second lane. Sync runs MUST NOT produce this finding, so a
  standard disk produces none.
- **FR-130**: Find MUST gain "Only where the framing slips", which lists only
  the matches of a nibble pattern at bit offsets other than the drive's
  normal framing; FR-056's "Any bit offset" lists both.
- **FR-131**: Findings MUST gain a "Protection" category. Each entry MUST give
  the technique first (for example "Nibble-count track", "Modified address
  prologue D4 AA 96", "Bit-slip stream" or "Sector numbers doubled"), and a
  protection family second only when a pattern (FR-133) matched; the evidence,
  with its quarter track, cell or angle and bytes; a confidence of Certain,
  Likely or Possible; and what a copy must keep for the disk to work (for
  example "Lost in DSK and NIB, kept in WOZ" or "Kept only on a flux track").
  These findings can use the word protection (FR-048).
- **FR-132**: From the disk's data alone, without running its code, the
  inspector MUST detect and report as Protection findings: address or data
  marks other than the standard ones, with their values; address or data
  checksums that pass only when computed from another starting value; sector
  numbers transformed the same way across a track (doubled, offset or combined
  with a constant); 13-sector and mixed 13- and 16-sector layouts; RW18 and
  every other format a custom track format decoder recognizes (FR-141), and
  18-sector layouts no decoder recognizes; 4-and-4 data; data on half or
  quarter tracks; the same data on adjacent quarter tracks (wide tracks),
  found as FR-147 gives; tracks holding one repeated pattern and no fields
  (nibble-count and sync tracks); extra zero cells after field nibbles (timing
  bits); bit-slip streams (FR-129); custom translate tables (FR-126); track
  lengths far from nominal; sector 0 lined up across tracks, on a WOZ whose
  synchronized flag is on; flux speed zones; and, in an A2R capture, weak bits
  (FR-139). A standard disk MUST produce no Protection finding, and a track of
  random noise MUST be classified unformatted and MUST NOT produce one.
- **FR-133**: The inspector MUST identify the boot loader on track 0 sector 0
  against a pattern table, in which each pattern gives bytes with ??
  wildcards, where to look (the boot sector, track 0, any sector, or a
  track's nibbles) and the loader or family it matches. Casso MUST ship
  patterns for the publicly documented loaders (DOS 3.2, DOS 3.3, ProDOS and
  Pascal), and the user MUST be able to add patterns in a file. For a loader
  of DOS 3.3's family, the inspector MUST read the disk's read and write
  routine from its standard place on track 0 and report how it differs from
  DOS 3.3's: the marks it reads, the checksums and epilogues it does not
  check, and its translate tables.
- **FR-134**: "Trace boot" MUST boot a copy of the disk in a hidden machine
  (in Casso of the window's machine type, in Explorer an Apple //e) for at
  most a cycle limit (Assumptions), without changing the disk, its file or
  any running machine. It MUST stop when code the disk loaded reads the
  drive's data latch, and MUST run the checks of FR-133 on that code in
  memory, so a loader that is encrypted on disk or missing from the pattern
  table can still be read. When no such read happens within the limit, it
  MUST show that none was found.
- **FR-135**: A verdict chip MUST show the first of these that applies:
  "Protection found", when a Protection finding is Certain or Likely;
  "Damaged", when the image has damaged records or its checksum does not
  match; "Bad sectors", when a 13-sector or 16-sector track has bad sectors;
  "Nonstandard format", when a track is nonstandard or the decode settings are
  not standard; otherwise "Standard".
- **FR-136**: The Image tab MUST show what the image's format keeps and
  loses: a DSK, DO or PO keeps only sector data; a NIB or NB2 loses timing
  bits and weak bits; a WOZ bit track loses flux timing and holds weak bits
  as zero cells; a WOZ flux track keeps one turn; an A2R keeps every
  revolution captured. It MUST list the Protection findings on this disk that
  a copy in each other format would lose.
- **FR-137**: Explorer's inspector MUST open A2R 2 and A2R 3 captures of 5.25"
  disks with "Inspect disk image", which MUST also appear on the right-click
  menu of an A2R file (FR-073), and MUST show a capture of any other drive
  type with a note that Casso analyzes only 5.25" captures. For each quarter
  track it MUST list the captures, the number of revolutions in each, how each
  revolution's start was found (an index signal, the capture's loop point, or
  matching the track against itself), and for a start found by matching, the
  share of cells that matched, and MUST analyze flux at the capture's own
  resolution. The views MUST show one revolution at a time, the first by
  default, with a choice of the others. The Image tab MUST list the capture's
  version, its INFO fields (drive type, write protected, synchronized and
  hard-sector count), its resolution, its META entries, whether it holds an
  SLVD chunk, and every chunk with its size, and Findings MUST report
  truncated chunks, unknown drive types and a hard-sector count other than
  zero. A2R captures MUST get no preview platter and MUST NOT be mounted in a
  drive (Scope).
- **FR-138**: For a quarter track with at least two overlapping revolutions,
  the inspector MUST align the revolutions and compare them cell by cell and
  by flux timing, and MUST show a stability lane on the strip giving how many
  revolutions hold the same value at each cell, and a "Revolutions" platter
  overlay marking where they differ. A quarter track with one revolution, or
  with revolutions that do not overlap, MUST show "Not compared", with the
  reason, and MUST NOT show as stable.
- **FR-139**: Each region where revolutions differ by more than the alignment
  tolerance (Assumptions) MUST be a finding giving its quarter track, start
  cell and angle, its length in cells and µs, the number of revolutions
  compared, whether it lies in an address field, a data field or a gap, and
  its longest stretch with no flux transition. A region whose transitions fall
  off the cell grid (Assumptions) in every revolution MUST be reported as
  "Weak bits", with a note that a copy holding one reading of these cells
  fails a check that reads them more than once. Any other region MUST be
  reported as "Unstable cells", which worn media also produce, and MUST NOT be
  reported as protection.
- **FR-140**: For a WOZ, the Image tab MUST show the INFO cleaned flag with
  its meaning (fake bits were removed and stored as zero cells), and the
  Protection category MUST show a note that weak bits can be found only by
  comparing revolutions, which a WOZ does not hold.
- **FR-141**: The analyzer MUST decode documented custom track formats into
  sectors, each format with a decoder of its own, starting with RW18. A track
  on which a decoder finds at least one of its address fields with a good
  check value, and no standard sector, MUST be classified as that format (for
  example "RW18") instead of unformatted or nonstandard (FR-016). The Nibbles,
  Fields, Flux timing and strip views MUST still show every nibble of every
  track, so a format with no decoder is shown as in the first release. Each
  decoder MUST be on by default, and the user MUST be able to turn it off in
  the decode settings for the whole disk or a range of tracks (FR-019).
- **FR-142**: The RW18 decoder MUST read an address field of D5 9D followed by
  three nibbles holding the track, the sector ($0 to $5) and a check value,
  whose 6-bit values give zero when combined by exclusive OR, and then AA;
  and, within three nibbles after the address field, a data field of one ID
  nibble, 1,024 nibbles in 256 groups of four, a checksum nibble and D4. In
  group i, the first nibble holds the top two bits of byte i of three pages
  and the other three hold the low six bits of byte i of each page. The
  checksum is the exclusive OR of the 1,024 6-bit values. The three address
  nibbles, the 1,024 data nibbles and the checksum nibble MUST be read through
  the standard 6-and-2 translate table; the marks and the ID nibble are not.
  Sector s MUST hold pages s, s+6 and s+12, so a track holds 18 pages of 256
  bytes.
- **FR-143**: On an RW18 track, the track header MUST show the format and the
  ID nibble found in its data fields, the sector row MUST show sectors $0 to
  $5 in passing order, and the Sector data tab MUST show a sector's three
  pages with their page numbers. The Tracks tab and the summary MUST count
  RW18 sectors good and found apart from standard sectors, and the format chip
  MUST show "RW18" alone or with the standard kinds on the disk (for example
  "16 sector and RW18").
- **FR-144**: Findings MUST list, on RW18 tracks: a sector whose checksum
  fails, with the three pages it holds; a nibble outside the table, with the
  bytes it affects, which the Sector data tab MUST also mark (byte i of one
  page, or byte i of all three pages for the nibble that holds the top bits);
  an address field whose check value fails, as its own finding at its place,
  with its sector not reported again as missing; a missing or repeated sector;
  an address track number that differs from the physical track, as a finding
  and not a failure, since some titles store one track's data under another
  track's number; and an ID nibble that differs from the one on the disk's
  other RW18 tracks. A track whose six sectors are good MUST produce no
  finding. Sectors a custom track format decoder decodes MUST NOT be editable
  (FR-096), and on a disk with no other file system the File map tab MUST show
  that none was found (FR-094).
- **FR-145**: "Compare with..." MUST also compare quarter tracks of the
  window's disk: any two of them, or one quarter track with the whole tracks
  on each side of it and the quarter tracks between them that hold records of
  their own, or every quarter track in a range of tracks. Each pair MUST be
  compared by FR-118's verdicts and FR-119's alignment, with B's strip below
  A's and the differences in the Differences tab (FR-121). Quarter tracks that
  map to one record MUST show that they share it and MUST NOT be compared
  with each other. Comparing quarter tracks MUST NOT change the disk.
- **FR-146**: For a quarter track compared with its neighbors, the window
  MUST give the share of its nibbles that match each neighbor once aligned,
  the stretches where they differ, and the address fields each holds with
  their track numbers. A quarter track whose fields all give a neighbor's
  track number, and whose share of nibbles matching that neighbor reaches the
  limit set in planning (Assumptions), MUST be shown as "Likely a read of
  track N", and one holding fields or runs of nibbles that neither neighbor
  holds as "Data of its own", with the evidence for each.
- **FR-147**: Over a range of quarter tracks, the comparison MUST list each
  run of data that repeats on adjacent quarter tracks with records of their
  own, with the quarter tracks it covers and its rotation on each, as
  protections that write one stretch of data across adjacent quarter tracks
  do. Quarter tracks that share one record, as the standard WOZ layout and
  the layout Casso builds for sector, NIB and NB2 images give, MUST NOT
  produce a run, so a disk in a standard layout lists none.

### Key Entities

- **Disk analysis**: all the results for one disk image: the summary, one
  quarter-track entry per quarter track, a track analysis per track record,
  the findings, and the image details. In Casso it is built from the disk as
  the drive holds it; in Explorer, from the file as saved.
- **Quarter-track entry**: a quarter track's content (nothing, bit track,
  flux track or damaged), the record it maps to, the map it came from and the
  quarter tracks that share it, its classification, counts, measurements,
  and, in Casso, whether it was written since the disk was inserted and its
  number of read visits.
- **Track analysis**: the nibbles of one track record in passing order, each
  with its value, start cell, width in cells, extra-zero count, kind,
  failed-checksum mark and sector; its random-bit regions; on a flux track,
  each cell's time and each transition's time.
- **Field**: an address or data field: its position, the mark bytes found,
  the decoded volume, track and sector, the stored and computed checksums,
  its encoding, its result (good, bad or not checked) and the field it pairs
  with.
- **Sector**: an address field and the data field paired with it, with the 256
  decoded bytes and, on a 16-sector track, its DOS 3.3 logical sector and
  ProDOS block, and on a mapped CP/M volume, its CP/M sector and allocation
  block.
- **Finding**: a category, a location (quarter track, sector, cell) and a
  description.
- **Decode settings**: the 16-sector and 13-sector address prologues, the data
  prologue and the epilogues matched, with ?? for any byte; whether the
  standard marks are matched too; the checks turned on; and the tracks they
  apply to. In the later release, for each track range, also the data encoding
  and translate table, the checksum starting values and the sector-number rule
  (FR-124). Standard by default.
- **Image details**: the file's format, size, read-only attribute and
  checksum result, and for a WOZ its INFO fields, META entries, maps, the
  track records the file holds and their damage. In Casso, the file as read
  at insert or at the last reload, with the values Casso has changed since
  and the track records the guest added.
- **Head limit**: the last quarter track the head of Casso's emulated Disk II
  can move to, read from Casso's drive definition and the same in both hosts
  (FR-004).
- **Head state** (Casso only): for each drive, its quarter track, the track
  record under the head, its angle, whether the motor is on, whether the
  controller has it enabled, and whether it is reading, writing or blocked
  from writing (FR-062).
- **Write and visit counts** (Casso only): for each track record, the guest
  writes since the disk was inserted; for each quarter track, the head's
  visits with the motor on since then.
- **File map**: for one volume, its file system, each sector's role, result
  and owners, its files, and whether its catalog was read completely. Built
  from the disk analysis.
- **Mapped file**: a file's path (on CP/M, its user number and name), type,
  locked state and recorded size; its sectors in file order with their roles
  and its holes; whether its chain was followed to the end, and if not, where
  and why; whether it is deleted; and for a forked file, both forks.
- **Pending edit**: a sector, located by its data field's place on the track;
  its bytes when editing began; the track record's guest-write count when
  editing began; the bytes the user changed; whether the checksum is
  recomputed; and whether it is out of date.
- **Applied edit**: the track records an apply changed, each as it was before
  and after, for undo and redo; the file it was saved to; and each record's
  guest-write count after the apply, undo or redo, to show whether the guest
  has written to it since.
- **Sector write**: what the shared sector writer takes from any caller: the
  track record and the place of the data field on it, the 256 new bytes, the
  encoding and table to write them with, whether the checksum is recomputed,
  and which caller it serves, since the `disk` command and Explorer may change
  only tracks that decode completely, in an image with no record of its own
  between whole tracks (FR-114, FR-115).
- **Disk as inserted** (Casso only): each drive's disk as it was inserted or
  last reloaded, kept as copies of its track records (FR-063) for comparison.
- **Comparison**: disks A and B with their sources and the decode settings
  each is analyzed with; the options; each compared track's verdict with its
  rotation, length difference and timing note; the differences; and the file
  comparison.
- **Difference**: its kind (cells, nibbles, sector bytes, flux timing or
  file), its quarter track, sector and cell range on each side, and for a
  file, its path.
- **Translate table** (later release): an encoding, its entries, where it
  came from (standard, swaps, pasted or read from the disk) and the tracks it
  applies to.
- **Framing** (later release): a track, and the nibble after which the
  framing slips and by how many cells, or the cell it starts at.
- **Protection finding** (later release): a technique, a family when a
  pattern matched, the evidence, a confidence, and what a copy must keep.
- **Boot pattern** (later release): bytes with wildcards, where to look, and
  the loader or family it matches.
- **Capture** (later release): an A2R file's version, resolution and drive
  type, and for each quarter track its captures and revolutions, with how
  each revolution's start was found.
- **Revolution comparison** (later release): for one quarter track, how many
  revolutions hold the same value at each cell, and its weak and unstable
  regions.
- **Track format decoder** (later release): one documented custom track
  format, how its address and data fields are recognized, how a field decodes
  into sectors and pages, and the tracks it is turned on for.
- **Quarter-track comparison** (later release): two or more quarter tracks of
  one disk; for each pair its verdict, the share of nibbles that match and
  the stretches that differ; for a quarter track compared with its neighbors,
  whether it is likely a read of one of them or holds data of its own; and
  the runs of data that repeat across adjacent quarter tracks.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every made-up test image (16-sector, 13-sector, mixed,
  nonstandard marks, planted anomalies, sectors spanning the end of the
  track, an address field with no data field, an address field with no data
  field followed closely by a complete sector, epilogue variants, a data
  field just inside and just outside the search window, track records on a
  half track, on a quarter track alone and across two whole tracks, and one
  image for each image file problem in FR-051), the decoded bytes match the
  source byte for byte, every checksum result is correct, and every planted
  anomaly appears in Findings exactly once at its location.
- **SC-002**: For every test image, outside random-bit regions and the
  nibbles between each region and the first sync run after it, the nibbles
  the inspector shows for a quarter track that has a record equal the nibbles
  Casso's drive delivers on the second full turn from the index, and every
  random-bit region the inspector marks lies where the drive produces random
  bits.
- **SC-003**: Analyzing a 140 KB DOS 3.3 disk takes under 100 ms, and
  analyzing a WOZ with 160 distinct flux quarter tracks takes under 1 s, on
  the development machine; the window responds to input within 100 ms
  throughout.
- **SC-004**: Over a scripted zoom from fit to maximum and back, and a pan
  across the whole disk and the whole track, on the development machine, the
  median frame time of the platter and the strip is at most one refresh
  interval and no frame takes over 33 ms.
- **SC-005**: With the inspector open on the drive in use and following the
  head while a disk is read and written, the median of five alternating runs
  with the window open and closed, each pinned to the same cores, puts
  emulation at Maximum speed with the window open within 2% of closed. Over
  five 60 s runs at normal speed, the window-open total of audio underruns
  and dropped frames exceeds the closed total by at most one.
- **SC-006**: After the guest writes a bit track, its new contents are shown
  within 500 ms, and after a write to a flux track ends, within 500 ms; a
  test that writes one track records exactly one track analysis and no
  whole-disk analysis.
- **SC-007**: Opening, using and closing the inspector in every host without
  applying an edit, including the file and sector map, comparison and, in the
  later release, the preservation views, leaves every test image
  byte-for-byte unchanged on disk and never marks a mounted disk as changed.
- **SC-008**: Before handoff, every view is checked on screen in the Light
  and Dark themes and in each of Casso's own themes, in every host that
  offers the theme, with the parts specific to Casso checked in the light
  theme FR-078 provides. On those captures, all text other than unavailable
  controls meets a 4.5:1 contrast ratio against its background, every pair of
  Structure kind colors and every pair of map role colors differs by at least
  ΔE2000 10 in each theme, and good and bad states remain distinct in a
  grayscale capture.
- **SC-009**: In Explorer, the small platter and chips appear within 300 ms
  of selecting a 140 KB image and within 1 s for any image in the test set,
  and the catalog preview appears no later than it does without this
  feature.
- **SC-010**: Each damaged-track case from spec 038 opens in every host with
  its damaged quarter tracks shown as damaged, with their reasons, and none
  shown as a blank track.
- **SC-011**: Every command and view is reached from the keyboard alone in a
  scripted walk through the window.
- **SC-012**: Checked locally during development and never in a merged test,
  *Bandits* shows 34 tracks with data, 19 of them flux, and its flux tracks
  show their fast and slow bands in Timing mode.
- **SC-013**: A standard DOS 3.3 DSK, a NIB, a WOZ 2 in the standard layout, a
  WOZ that Casso made, and a WOZ that also maps N+0.5 to a neighboring whole
  track's record, each of the same made-up disk, open with no findings, and in
  Casso, a WOZ whose file holds no record for a track still shows no findings
  after the guest formats that track.
- **SC-014**: For made-up DOS 3.3 and ProDOS images with known layouts (a file
  with three track/sector lists, a sparse random-access text file, a tree file
  with a sparse hole, subdirectories two levels deep, a forked file, an entry
  that uses no sectors, and a deleted file), for made-up Apple Pascal images
  in DO and PO order (files with varied last-block byte counts, a gap between
  files and a .BAD file), and for made-up CP/M images in DO and PO order (a
  file of two extents, a sparse file, a deleted file, a file in another user
  area and the system entry of user 31), every sector's role and owners match
  the layout, each file's sectors are listed in file order with its holes in
  place, a standard disk of each of the four file systems produces no
  findings, and ProDOS, DOS 3.3 and all-zero disks are never mapped as Pascal
  or CP/M volumes.
- **SC-015**: For made-up damaged images (a cross-link, a loop in a
  track/sector list, a loop in the catalog chain, a loop in a ProDOS directory
  chain, a pointer outside the volume, a sector number of $10 or more in a
  track/sector list, a bad catalog sector, a bad index block, an allocated but
  unowned sector, a sector owned but marked free, a catalog sector count that
  differs from the file's sectors, overlapping Apple Pascal entries, a Pascal
  entry past the volume's end, a Pascal directory sector that is bad, a CP/M
  block two entries use, and an invalid CP/M entry among valid ones), the map
  appears within 100 ms of the analysis finishing on the development machine,
  each problem appears in Findings exactly once with its location and reason,
  and every file read before the damage is still listed.
- **SC-016**: For each format FR-003 lists, on bit and flux tracks, for a
  WOZ 2 in the standard layout with three quarter tracks to a record, and for
  a made-up 13-sector WOZ, an applied edit to one sector makes it decode to
  the new bytes under the same decode settings, with the data checksum result
  the Sector data header showed before the apply; every cell of the track
  outside the data field's body and checksum nibble is unchanged, and on a
  flux track so are every transition outside them and the time of one turn;
  every other track record is unchanged; each track keeps its kind in the
  saved file; on DSK, DO and PO the checksum is always recomputed and the
  saved file holds the new bytes; and undoing the edit restores the track
  record exactly and saves a file whose tracks are as they were before the
  edit. 13-sector edits also match test vectors worked out by hand from the
  5-and-3 description in Beneath Apple DOS, so the check does not rest on
  Casso's own encoder and decoder alone.
- **SC-017**: For the `disk` command's `sectorwrite`, `blockwrite`, `put`,
  `delete` and `boot`, an assembler's `--disk` output, and Explorer's file
  writes, on DSK, DO, PO, NIB, NB2 and WOZ images, on WOZ bit and flux tracks,
  on WOZ images in the standard layout, in a layout that also maps N+0.5, and
  in the layout Casso writes, on a disk whose volume is 130, and on a WOZ
  whose track numbers differ from its record numbers, each write succeeds;
  every address field keeps its volume; every changed track keeps its length
  in cells, its sync runs and the place of every sector; on a flux track every
  cell of a written field keeps its recorded time (FR-111) and every
  transition outside the written fields is unchanged; and every other track
  record is unchanged. On the WOZ whose track numbers differ from its record
  numbers, a `put` changes only the records of the tracks it writes, and `disk
  sectorread` of each sector written returns the new bytes. A write that needs
  a track that does not decode completely, a track with no standard address
  field, or a WOZ with a record of its own on a half or quarter track fails
  and leaves the file byte-for-byte unchanged. A save of an inspector edit, of
  a `disk` command write or of an Explorer write, made to fail at each of its
  steps in turn, leaves the file byte-for-byte as it was.
- **SC-018**: In a scenario test, Casso boots DOS 3.3, the inspector edits a
  sector of a text file, and the guest reads the file back with the new bytes.
  In a second test, the guest reads the edited sector in a loop while the edit
  is applied with "Pause and apply", and every read returns all the old bytes
  or all the new ones, with no checksum error. In a third test, the guest
  writes other bytes to the edited sector in a loop while an apply waits;
  every guest write and read completes with a good checksum, the wait ends,
  and the edit is marked out of date. In a fourth test, a 6502 5-and-3 read
  routine written for the test from public documentation reads an edited
  sector of a made-up 13-sector disk in a test machine and gets the new bytes
  with a good checksum.
- **SC-019**: For each cause in FR-065 and FR-102, in each host where it
  applies (in Explorer: the write-protect flag, a read-only file, no
  permission to write the file, and a damaged image), no edit, undo or redo
  changes the disk in memory or on disk, the causes are shown beside "Apply",
  and "Save edited copy..." writes a file that decodes with the edit while the
  original stays byte-for-byte unchanged.
- **SC-020**: In each of these cases, no guest write, applied edit or pending
  edit is lost without the window showing it, and whenever the window shows an
  edit as applied, the file the drive holds contains it: a failed save; a save
  to a preserved copy; a guest write during the apply wait; a guest write to a
  track with pending edits; "Apply" while the guest writes the edited sector;
  redo after a guest write; and eject, quit and a machine change with pending
  edits. In a scenario test, an Explorer apply on an image a running Casso
  holds, with and without unsaved guest writes, leaves Casso, its drive and
  its files in the same state as Explorer writing the same bytes to the file
  through its own file commands, and Explorer's inspector shows Casso's reply
  whenever it is anything other than a reload.
- **SC-021**: For made-up pairs with planted differences (two copies of one
  image, a DSK and a WOZ in the standard layout of one disk, one changed byte,
  a rotated track, a track with extra sync nibbles, a track only in one image,
  a flux timing change, a bit track against a flux track with the same cells,
  different volume numbers, a damaged record, a changed file, a changed Apple
  Pascal file, a changed CP/M file, a file only in one image, and a 13-sector
  track against a 16-sector one), each compared track gets the planted
  verdict, every planted difference is listed exactly once, nothing else is
  listed, no track that was not compared is counted as matching, and neither
  image changes. On the development machine, comparing two 140 KB disks takes
  under 1 s, and two WOZ images with 160 distinct flux quarter tracks under
  5 s.
- **SC-022**: In Casso, after the guest saves one file to a DOS 3.3 disk,
  comparing the disk now with the disk as inserted lists differences only on
  the tracks the guest wrote, and the file comparison lists the new file as
  only in A and no other file as different.
- **SC-023** (later release): For made-up disks with one swapped pair, a whole
  permuted table, 5-and-3 data and 4-and-4 data, "Solve table" finds exactly
  the planted swap on the swapped disk and shows that the permuted table
  cannot be found from checksums alone, a pasted or extracted table decodes
  every sector to the source bytes, and a table with a repeated entry or an
  entry whose high bit is clear is not accepted.
- **SC-024** (later release): On the made-up E7 track, the scan reports one
  finding with the expected slipped nibbles; the same nibbles without extra
  zero cells, and every standard test image, report none. A 6502 routine in a
  test machine that reads the latch, slips it as an E7 check does and then
  searches for EE finds EE within 16 tries, which also checks Casso's latch
  timing.
- **SC-025** (later release): For made-up disks with one technique each, each
  technique appears exactly once in the Protection category with the
  expected confidence; standard disks and a track of random noise produce
  none; boot-loader tests use only made-up boot sectors and a test pattern
  table; and no commercial disk's data is in a merged test. Checked locally
  and never in a merged test, *Bandits* shows its flux speed zones as a
  Protection finding.
- **SC-026** (later release): For made-up A2R 2 and A2R 3 captures, a planted
  weak region is found within one cell and nothing else is; jitter alone
  produces no region; one revolution produces "Not compared"; a capture
  without index signals gives the same findings as with them; and the same
  data as A2R 2 at 125 ns and as A2R 3 at 125 ns, 62.5 ns and 25 ns gives
  identical findings.
- **SC-027** (later release): On the development machine, opening a 35 MB A2R
  capture shows its tracks within 5 s, and the window responds to input within
  100 ms throughout.
- **SC-028** (later release): For a made-up disk with a standard 16-sector
  track 0 and RW18 tracks 1 to 34, written by an encoder written for the test
  from the format's description and not from the decoder, with pages holding
  every byte value from $00 to $FF, so the nibble that holds each group's top
  bits takes every value, every page decodes to its source bytes. Each planted
  case (a bad data nibble, a nibble outside the table, a wrong ID nibble, a
  bad address check value, a track number that differs from the physical
  track, a missing sector and a repeated sector) appears in Findings exactly
  once at its place; a rotated track start and other sync lengths produce no
  finding; and no standard test image shows an RW18 track. Checked locally and
  never in a merged test, *Prince of Persia*'s RW18 tracks decode with every
  checksum good.
- **SC-029** (later release): For made-up WOZ images with a half track
  holding its neighbor's nibbles with noise over a tenth of them, a half track
  holding sectors of its own, and one run of data written across three
  adjacent quarter tracks, the comparison shows the first as likely a read of
  its neighbor, the second as holding data of its own, and lists the run once
  for each of the three quarter tracks with its rotation; a disk in a
  standard layout lists no run; and no disk changes.

## Assumptions

- AppleEm (mikedaley/web-a2e and mikedaley/applem-core, MIT license) is the
  model for the inspector's layout and behavior; no code is copied. AppleWin
  (GPL) is a source of ideas only, as Casso's clean-room rule requires.
- *Bandits* is used for local testing during development only and is never
  checked in. Made-up images cover every test that merges to master.
- Timing deviation is measured against the drive's own cell of 4 CPU cycles
  (3.91 µs, 31.29 flux ticks), the cell Casso's drive plays and writes, not
  the format's nominal 4 µs.
- The nominal track is one turn at 300 RPM: 51,200 cells of 3.91 µs (about
  200 ms), the length Casso's drive gives a quarter track with no record. A
  flux track's nominal turn is 200 ms. FR-017 and FR-048 measure against
  these.
- The angle around the platter follows Casso's drive: on a bit track, the
  position in its bits over its bit count; on a flux track, its time over the
  time of one turn. Tracks of different lengths each fill one full turn.
- The unformatted test in FR-016, more than half a track's length in noise or
  random-bit regions, is a choice that planning may tune against real disks.
- The data search window in FR-013 is a fixed number of nibbles after the
  address field. Planning sets it, starting from AppleEm's 48 nibbles.
- A finding for track length uses ±2% from nominal. Planning may tune it
  against real disks.
- Follow head selects a track only after the head has stayed on it about
  0.3 s, as in AppleEm's native window, so a protection routine that steps
  quickly does not make the views jump from track to track.
- Copy to the clipboard is included because it is read-only and cheap.
  Whether export to a file is included is the open question in FR-058.
- In the first release the decode settings last only while the window shows
  the disk and are not saved, because they belong to one disk. The later
  release adds saving them to a file the user chooses and loading them from it
  (FR-127), and never applies settings automatically.
- The Casso labels follow the Storage menu's existing items: "disk N" for the
  disk in a drive, as in "Write-protect disk 1", and "..." because the item
  opens a window. Explorer's label is "Inspect disk image", using Explorer's
  disk-image wording, as in "Format disk image...", with no ellipsis, as
  File Explorer's Properties has none. The window's title is "Disk inspector"
  and the image's file name.
- The preview pane shows the small platter and chips above 033's catalog or
  details preview, which is otherwise unchanged.
- The views match the selection and copy behavior of 033's hex view without
  depending on it, since they are built before 040 is rebased onto 033.
  Whether the Sector data tab moves onto 033's hex view after the rebase is
  settled in planning. 033's hex view does not edit, so moving the Sector data
  tab onto it means adding editing to that view; otherwise the tab keeps its
  own editable view.
- Each host saves its inspector state in its own settings, because a running
  Casso rewrites its settings file whole and Explorer keeps its own.
- 033 adds Light and Dark to Explorer, beside Casso's three themes; Casso
  keeps only its three dark themes (Skeuomorphic, Dark modern and Retro
  terminal). FR-078 holds the open question of how the parts specific to
  Casso are checked in light.
- GH #135 (both Disk ][ drives share one head position) is open, and this
  spec does not fix it; the edge cases give what the head marker shows until
  it is fixed.
- The development machine is the owner's; the timing targets in SC-003,
  SC-004, SC-005, SC-009, SC-015 and SC-021, and in the later release SC-027,
  are measured there.
- The file and sector map reads only the sectors the analyzer decoded, so it
  matches the other views, needs no second decode of the disk, and works
  on a disk with nonstandard marks once the decode settings decode its
  sectors.
- DOS 3.3 tracks 0 to 2 count as the boot area and DOS image only where the
  VTOC marks them used and no file owns them, so a data disk that frees them
  shows them as free, and one that stores files there shows those sectors as
  the files' (FR-086).
- Deleted files are listed because their data often remains on the disk.
  They own no sectors, so they never produce findings.
- 13-sector DOS 3.2 disks are not mapped, because Casso has no reader for
  them (GH #164 tracks 13-sector images), and neither are volumes other than
  35 tracks of 16 sectors.
- Apple Pascal and CP/M disks use the standard 16-sector track format, and
  only their file systems differ, so the analyzer already decodes their
  sectors. Casso has no reader for either file system, so 040 adds read-only
  readers that find the volume, read its directory and give each file's
  blocks, which is all the map and the file comparison need. They are written
  from public descriptions of the formats: the Apple Pascal directory and its
  ProDOS-order blocks, the CP/M 2.2 directory, and the Apple II CP/M sector
  skew and allocation layout. CiderPress II (its code under Apache 2.0, its
  documentation under CC BY-SA 4.0) and cpmtools (GPL) are sources of ideas
  and facts only, and no code is copied from either.
- Some disks hold a DOS 3.3 volume beside a Pascal volume smaller than the
  disk, and 3.5" disks hold larger Pascal and CP/M volumes. The map reads the
  DOS 3.3 volume of such a disk and gives the Pascal volume the not-mapped
  state with its size, as it does for every volume of another size.
- ProDOS subdirectories in the map, and their paths in the file comparison,
  rely on 033's reading of subdirectories, so they are built after the
  rebase onto 033.
- Neither master nor 033 reads forked files or deleted entries, and today's
  readers credit DOS 3.3 tracks 0 to 2 and all of track 17, and ProDOS blocks
  0 to 6, to one volume owner. 040 adds the fork walk, the deleted-entry walk
  and the separate structure roles in FR-086. Forks inside subdirectories are
  built after the rebase onto 033.
- Editing changes decoded sector data only, because that is what DOS, ProDOS
  and programs read. Nibble and flux editing stays out of scope.
- Every sector write rewrites the data field in place instead of rebuilding
  the track, because a rebuilt track loses its length, its write seam, the
  angle of sector 0, its epilogues and volume, and its timing. Today's
  writers for WOZ bit tracks and NIB images do rebuild: a track a write
  changes gets volume 254 in every address field, 20 sync bytes before each
  address field and 6 before each data field, a length of 50,624 bits, and
  sector 0 at bit 200 with the sectors in order. A `put` always changes track
  17, so it leaves a disk whose volume is not 254 with two volume numbers.
  Today's flux writes already replace only the changed data field.
- Today's writers also reject every WOZ in the standard layout, because they
  require each quarter track to map to the record of the whole track it falls
  in, and the standard layout maps N+0.75 to track N+1's record. The error
  they print gives data at half or quarter tracks as the reason, although
  those quarter tracks hold no data of their own. Casso's own `disk create`
  maps N+0.75 to track N, so images Casso made can be written today. Today's
  sector reads and writes also take record N as track N instead of looking up
  whole track N in the map, so on an image whose records are not in track
  order they read and write the wrong record.
- GH #TBD tracks the immediate fix of the first two defects: the rejection of
  the standard WOZ layout and the rebuild with volume 254 and new sync and
  length. 040 then moves every sector writer onto the shared sector writer
  (FR-110), and also fixes the third defect: the shared sector writer and the
  `disk` command's and Explorer's sector, block and file reads find each whole
  track through the image's map (FR-111). The rules that protect tracks which
  do not decode completely and images with records of their own between whole
  tracks (FR-114, FR-115) keep today's protection, with the standard layouts
  now accepted.
- On a flux track every direct sector write keeps each cell's recorded time,
  instead of writing at the drive's cell as a guest write does, so that a disk
  whose protection checks flux speed, such as *Bandits*, still reads the same
  timing in and around the written field. Spec 038's FR-013 made sector-level
  tools write at the controller's nominal cell timing, and 1.30.0 shipped that
  behavior; FR-113 replaces it as a fix delivered with 040. A guest write
  stays at the drive's own timing, as on a real drive.
- In Casso an applied edit is saved at once, so an edit is never shown as
  applied while it exists only in memory. This follows the rule from GH #115
  that a change that could not be made must never be shown as made.
- In Casso an edit reaches the drive only at a safe point, with the machine
  held paused until the edit is saved or rolled back (FR-103), so the guest
  never reads a field while it changes and never reads an edit the file does
  not hold. The guest's DOS can still hold an old copy of a sector, such as
  the VTOC, and write it back; the window shows a warning (FR-103), and
  nothing in the inspector can prevent it.
- Undo restores the stored track record, not a new encoding of the old
  bytes, so it returns the track exactly as it was.
- "Save edited copy..." leaves the drive on the original disk, so a copy
  made from a write-protected disk never changes what the guest reads.
- An edit that Explorer saves reaches a running Casso as a change to a mounted
  image's file made outside Casso, sent with the reload intent as Explorer's
  other writes are (033's FR-024), and Casso handles it with its usual
  external-change handling (FR-106), which spec 041 reworks: 041 moves the
  matching of changed files to drives onto the emulation thread and orders a
  reload against a save of the guest's writes, keeping a preserved copy where
  they conflict. Any inconsistency in what Casso does with such a change, such
  as which file the drive holds when the guest's writes were not yet saved,
  belongs to Casso's external-change handling, not to 040. Explorer's window
  makes no request of Casso before it saves, and shows Casso's reply when it
  is anything other than a reload (FR-105); Casso's own inspector shows which
  file the drive holds afterward.
- Casso keeps each drive's disk as inserted for "As inserted" comparisons.
  This costs at most the size of the largest image per drive, a few MB.
- The flux timing tolerance for comparison starts at ±1% of a cell's time and
  must stay below the smallest timing change in the test pairs (4%). The
  nibble alignment limit starts at runs of up to 64 inserted or deleted
  nibbles. Planning tunes both against made-up tracks with jitter.
- A write seam shows in a comparison as a sync run of another length, which
  "Ignore sync widths and counts" leaves out, so no separate option is given
  for it.
- (Later release) The pattern table holds Casso's own patterns, written from
  public documentation such as Beneath Apple DOS and Beneath Apple ProDOS and
  from disks the owner holds, and copies no other tool's signature data.
  Copying, cracking and patching disks stay out of scope.
- (Later release) The bit-slip scan's minimum run is set in planning,
  starting at 6 nibbles, so that sync runs, which return to the normal
  framing within a few nibbles, never qualify.
- (Later release) "Solve table" searches swaps of the standard table up to a
  limit that starts at two swapped pairs and is set in planning; a table that
  is a whole permutation of the standard entries needs the table itself,
  pasted or read from the disk.
- (Later release) "Trace boot" uses a cycle limit set in planning, starting at
  5 seconds of emulated time. A loader that checks the machine model or its
  timing can make a trace stop early or find nothing.
- (Later release) Weak-bit detection needs at least two overlapping
  revolutions; an A2R 2 "timing" capture covers about 1.25 revolutions, so
  some tracks can be compared over only part of a turn. The alignment
  tolerance starts at one cell. A transition is off the cell grid (FR-139)
  when it falls more than 25% of a cell from every cell boundary of its
  revolution; planning tunes this against made-up captures. A capture is one
  drive's reading at one moment, so the findings give the evidence and leave
  the judgment to the user.
- (Later release) A2R captures are analyzed and not mounted. Mounting one
  needs one solved revolution per track with its weak regions cleared, which
  is a separate decision.
- (Later release) The RW18 decoder is written from public descriptions of the
  format: Roland Gustafsson's RWTS18 source as transcribed and published,
  Jordan Mechner's released *Prince of Persia* source, and Peter Ferrie's
  article on the game's boot. Neither the RWTS18 transcription nor the *Prince
  of Persia* source repository grants rights to its code, so both are read for
  the format only, the format is described in Casso's own words, and no code
  is copied. Tests use a made-up disk from an encoder written for the test
  from that description; *Prince of Persia* is used for local checks only and
  never checked in. A further format joins the same way, with a decoder of its
  own, once a public description of it exists; the RW18 variants of other
  titles, which differ only in the ID nibble, need no new decoder.
- (Later release) The share of a quarter track's nibbles that must match a
  neighbor before it is shown as likely a read of that neighbor (FR-146) is
  set in planning, starting at 75%, and tuned against made-up tracks with
  noise. A read off the center of a track gives a weaker signal and more
  noise, so a high share with no fields of its own points to a read of the
  neighbor, while fields or runs that neither neighbor holds point to data of
  its own.
- Disk breakpoints belong to the 035 debugger spec. The field definitions and
  the matching of marks with ?? wildcards (FR-011, FR-012, FR-019), the
  head-state record (FR-062) and the write counts (FR-068) are shared with
  035's disk breakpoints: they are built on the branch of whichever spec
  merges first, and the other branch uses them. The decode settings a user
  enters in the inspector stay with the inspector window (FR-019) and are not
  offered to the debugger, so 035 gives its breakpoints marks of their own.
  The Disk II controller holds one event listener today, which the Disk ][
  debug window uses, and neither spec takes it from that window.
- FR-052 builds on what Casso already keeps from a WOZ file rather than
  reading INFO again. Master keeps the INFO chunk and META whole, and its WOZ
  description already reads the version, disk type, write protect,
  synchronized, cleaned, creator, boot sector format and META entries. The
  woz-info-fields branch, not yet merged, also reads optimal bit timing,
  compatible hardware and required RAM, and checks the last two against the
  running machine. 040 uses those readings and adds what neither has: sides,
  largest track, flux block and largest flux track, each track record's
  location and length fields, records no map refers to, which map each quarter
  track came from, the checks in FR-051 and the changed-since-load marks in
  FR-071. If woz-info-fields is still unmerged when 040's plan is written, one
  place for the INFO readings is chosen in planning, and both branches use it
  before either merges.
- Spec 041 (disk integrity, branch `041-disk-integrity`, built on 035 and
  merging after it) changes four things 040 relies on. Whichever of 040 and
  041 merges second adapts its code and tests to the other, and until 041 is
  on master, 040 follows master wherever 041 has not yet changed it.
  - *The head limit.* `Disk2Controller::kMaxQuarterTrack` moves from 139
    (track 34.75) to 158 (track 39.5, the last half-track position of track
    39), so after 041 only quarter track 39.75 lies beyond the head's reach.
    040 reads the limit from that constant in both hosts, Explorer included,
    instead of keeping a constant of its own (FR-004). The drive widget's rail
    keeps its own limit of 139, which 041 leaves as it is.
  - *Empty track slots.* `WozLoader::Load` reserves an empty bit slot of 0
    bits for every whole track 0 to 39 whose position the file leaves
    unmapped, and maps that track's unmapped quarter tracks to it, so the
    guest can format the track; the first bit the guest writes sizes the slot
    to 51,200 bits. The reservation is not saved and leaves no mark: such a
    slot resolves as no track (`ResolveQuarterTrack` returns -1), exactly as
    an unmapped quarter track does. The inspector therefore shows a 0-bit slot
    that is not damaged as "Nothing recorded", lists in the Image tab only the
    records it read from the file (FR-050, FR-052), and shows a track created
    there as written and added (FR-005, FR-071). The slot's index can differ
    from the track number. 041 also unmaps and reserves in the same way every
    mapped bit record that is empty and not damaged, while a damaged record
    stays mapped. `WozLoader::ParseV2Track` already loads a bit record with a
    zero start block or a zero block count as an empty slot that is not
    damaged, and 040 does not record a map entry that points at one as damage
    (FR-053), so 041 reserves such a record as it does an unmapped track: its
    quarter tracks show "Nothing recorded", the guest can format the track,
    and Findings reports the entry as an image file problem (FR-051). A FLUX
    entry that points at such a record already loads as an empty flux track,
    which the guest can write today.
  - *Durable saves.* 041's `DurableCommit` writes a temporary file beside the
    target, copies the target's metadata to it, flushes it, and then replaces
    the target atomically, removing the temporary on any failure. Casso's
    image saves, `WriteFileAtomically`, the CLI's `DiskImageSession` and Casso
    Explorer's commits (Cassque) go through it. FR-116 relies on it for every
    save of the shared sector writer's callers and of inspector edits, and for
    "Save edited copy...", on its rename without replacing when the copy is a
    new file and on its replacing commit when the user confirms replacing an
    existing file. If 040 is ready before 041 merges, planning settles whether
    040's merge waits for 041 or 040 builds the same guarantee into the save
    paths it uses.
  - *Thread ownership.* After 041, only the emulation thread reads or changes
    disk state, and a window-thread read of a live image asserts in Debug.
    040's rule that the inspector reads copies (FR-063) already matches this:
    the emulation thread takes each track copy and each copy of the image
    details, by a posted request or as a snapshot published under a lock as
    035's debug views do, and makes every applied edit, undo and redo at a
    safe point. 041's `DriveStatus`, which the emulation thread publishes at
    the end of each service pass and frame, holds each drive's mounted file,
    write-protect causes, motor state and head quarter track. The inspector
    takes its head state (FR-062), its write-protect causes (FR-065) and the
    write and visit counts (FR-068) from it, and 040 adds to it the angle, the
    track record under the head, whether the controller has the drive enabled,
    whether the drive is reading, writing or blocked, and the counts. If 040
    is ready before 041 merges, planning settles whether 040's merge waits for
    041 or 040 publishes the same record from the emulation thread itself and
    moves it into 041's `DriveStatus` when 041 lands.
