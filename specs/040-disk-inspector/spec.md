# Feature Specification: Disk Inspector

**Feature Branch**: `040-disk-inspector`

**Created**: 2026-10-08

**Status**: Draft

**Input**: GH #159, "WOZ disk images with type FLUX don't boot or give error"
(https://github.com/relmer/Casso/issues/159). Spec 038 made flux tracks play
and shipped in 1.30.0. The owner then commented on the issue "Todo: add this
awesomeness", with a screenshot of AppleEm's Disk Inspector showing *Bandits*
in its Timing mode.

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
of it. The `disk` command and salvage show sectors only, and the Disk ][
debug window logs address marks as the head passes them.

AppleEm's Disk Inspector (mikedaley/web-a2e and its core, mikedaley/applem-core,
MIT license) draws the disk as a platter with one ring per quarter track,
colored by what each stretch of a track holds, and unrolls the selected track
as a strip with its sectors, their bytes and its nibbles. On flux tracks it
colors each stretch by how much faster or slower than nominal its cells are,
which is what *Bandits*' protection checks. This spec adds a disk inspector to
Casso and Casso Explorer modeled on it (its ideas, not its code) and adds the
most useful features of other Apple II disk tools. The inspector also closes
the gaps in AppleEm's: it decodes 13-sector data, keeps unformatted tracks
separate from nonstandard ones, checks the fields AppleEm leaves unchecked,
shows flux timing without rounding it to cells, and shows every error it
finds on screen.

## Scope

**Image formats.** Every 5.25" disk image format Casso mounts in a Disk II
drive: DSK, DO and PO (140 KB sector images), NIB and NB2, WOZ 1, WOZ 2, and
WOZ 2.1 with bit tracks, flux tracks or both. Applies to every machine with a
Disk II controller in slot 6 (Apple ][, ][+, //e and //c). Casso does not open
DOS 3.2 sector images (.d13), which GH #164 tracks, so 13-sector disks reach
the inspector as WOZ or NIB images.

**Hosts.** One inspector window, opened from Casso or Casso Explorer, and a
small preview in Explorer's preview pane:

1. **Casso Explorer's preview pane**: when a disk image is selected, the
   preview adds a small still view of the disk and the summary chips.
   Clicking the small view opens the inspector window.
2. **Casso Explorer's "Inspect disk image"**: on a disk image's right-click
   menu, it opens the inspector window for that file.
3. **Casso**: the same window, attached to drive 1 or drive 2. Here it also
   follows the head, turns the disk under the emulated head, and refreshes the
   tracks the drive writes.

**Read-only.** The inspector never changes a disk image. Editing nibbles,
flux, sectors or metadata is out of scope.

**Delivery.** This spec ships whole, in one merge. Story priorities give the
build order only, and no story ships without the others. The order is the
analyzer, then the views, then the Casso host, then the two Explorer hosts.
Casso Explorer is spec 033, which is not yet on master. The analyzer, the
views and the Casso host do not depend on it. The Explorer hosts are built
last, after 033 merges master and 040 is rebased onto 033. 040 merges to
master only after 033 is on master, so the 040 merge holds 040's work alone.

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
Copy II Plus, Nibbles Away, Disk Fixer, CiderPress II, AppleWin and HxC, and
are in scope:

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

**Out of scope, for a future spec:**

- Editing of any kind, including inserting or deleting nibbles, marking
  nibbles as sync, normalizing flux timing, and moving a track's start.
- Comparing two images, two revolutions, or a disk before and after the
  guest wrote to it.
- A file-system overlay: which file owns each sector, and which files touch
  bad sectors.
- Identifying a disk's boot loader or protection scheme.
- A live trace of disk access and disk breakpoints. The Disk ][ debug window
  keeps the event log of head steps and address marks, and the inspector
  does not duplicate it.
- Custom nibble translate tables and forcing an encoding on a track.
- Viewing a track at another latch framing (bit slip). Find at any bit offset
  covers the common case.
- Weak-bit detection by comparing revolutions. WOZ stores one revolution per
  track, and Casso does not read A2R captures.
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
story uses, so they are built first. Like every story here, it ships in the
same merge as the others.

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
7. **Given** a write-protected disk in drive 1, **When** the guest tries to
   write to it, **Then** the head marker shows write blocked, the window
   shows that the disk is write-protected and why, and no track is analyzed
   again or marked as written.
8. **Given** the disk has booted, **When** the Tracks tab is shown, **Then**
   each quarter track the drive read is marked with its number of visits, and
   the "Reads" overlay marks the same quarter tracks on the platter.

---

### User Story 7 - Inspect a disk image from Casso Explorer (Priority: P4)

A user browsing disk images in Casso Explorer selects one and sees a small
picture of the disk and its summary chips in the preview pane beside the
catalog. Right-clicking the image and choosing "Inspect disk image" opens the
inspector window for that file.

**Why this priority**: It depends on 033, which is not yet on master, so it
is built last.

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
  lists it only when its quarter tracks are outside the standard layout.
- Track data past track 34.75, which the emulated head cannot reach: shown,
  dimmed behind a line at the head's limit, and marked as beyond the head's
  reach.
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

## Requirements *(mandatory)*

### Functional Requirements

**General**

- **FR-001**: The inspector window MUST present the same views whether Casso
  or Casso Explorer opens it. Only these are specific to Casso: the drive
  selector, Follow head, "Go to head", the turning disk, the head marker, the
  head position, trail and drive state on the strip, the write-protect state
  and its causes, the no-disk, not-attached and no-controller states, the
  refresh after the drive writes, the written and read marks in the Tracks
  tab, the "Reads" overlay, the out-of-date mark on Find results, and the
  Image tab's changed-value marks and save note. Only these are specific to
  Explorer's window: the refresh when the file changes on disk and the note
  that the file is no longer there. Explorer's preview pane shows only the
  small still platter and the summary chips (FR-071).
- **FR-002**: The inspector MUST NOT change the disk image in memory or on
  disk, MUST NOT mark it as changed, MUST NOT cause it to be saved, and MUST
  NOT change what the guest reads or when it reads it.
- **FR-003**: The inspector MUST open DSK, DO, PO, NIB, NB2, WOZ 1, WOZ 2 and
  WOZ 2.1 images, with bit tracks, flux tracks or both. For a sector image it
  MUST show the tracks Casso's drive plays, with a note that they are built
  from sector data and so hold no timing or copy protection. For NIB and NB2
  it MUST show a note that the format stores no timing bits between nibbles.
  The track lengths of sector, NIB and NB2 images come from the format, so
  those tracks get these notes and no track-length finding.
- **FR-004**: The inspector MUST cover all 160 quarter tracks, 0 to 39.75,
  and MUST mark the ones past 34.75 as beyond the emulated head's reach in
  the Tracks tab and the track header, and by a line on the platter between
  34.75 and 35.
- **FR-005**: On every quarter track with a readable record, the inspector
  MUST show what Casso's drive reads there, with flux decoded at the drive's
  own timing. A quarter track with no record MUST show as "Nothing recorded",
  and a damaged one as damaged with its reason (FR-053), never as a copy of a
  neighbor.
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
  to head", and "Go to", "Find" and "Decode settings..."; a platter column
  with the platter, its zoom controls, the legend, the overlays, the hint
  "Scroll to zoom, drag to pan, double-click to fit", and the disk tabs
  Tracks, Findings and Image; and a track column with the track header, the
  strip with its controls and hint, the sector row, and the track tabs Sector
  data, Nibbles, Fields and Flux timing. The window MUST open at 980×660 and
  not shrink below 640×460, both scaled for the display, and the platter MUST
  stay square.

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
  bytes actually found and the stored and computed checksums, and MUST
  detect: a failed address or data checksum; an address track number that
  differs from the physical track; a volume number that differs from the
  disk's most common one; a sector number repeated on a track; sector numbers
  missing from a standard track; a missing or nonstandard epilogue; an
  address field with no data field; a data field with no address field; a
  data-field nibble outside its encoding's translate table; and a D5 AA pair
  outside any recognized field, reported with the byte that follows it. The
  physical track of a record is the whole track nearest the middle of the
  quarter tracks that map to it; when that middle is a half track N.5, an
  address track of N or N+1 matches.
- **FR-016**: The analyzer MUST classify every quarter track as exactly one
  of these, taking the first that applies: nothing recorded; damaged (its
  image record could not be read); 16-sector, 13-sector, or 13 and 16 sector,
  when at least one address field of that kind passes its checksum or is not
  checked (FR-020); unformatted, when more than half its length is noise or
  random-bit regions; otherwise nonstandard (valid nibbles but no standard
  sector). On a nonstandard track, the candidate fields MUST still be listed
  with their checksums.
- **FR-017**: For each track the analyzer MUST measure: its length in cells
  against the nominal length (Assumptions); its nibble count; every sync run,
  with its nibble count and each nibble's width in cells; the gaps before and
  after each address and data field; the longest sync run, the track's likely
  write seam; the angle from the index to sector 0's address field and to the
  first field; and, on a flux track, the time of one turn, the equivalent
  RPM, the mean cell time and its deviation from nominal.
- **FR-018**: The disk summary MUST count each track record once, however
  many quarter tracks share it, and MUST give: the disk's format; the number
  of tracks with data; sectors good out of sectors found, with the number not
  checked (FR-020); bad sectors, each counted once whichever checksum failed;
  the numbers of nonstandard, unformatted, flux and damaged tracks; and the
  most common volume number. The format MUST be the first of these that
  applies: "Nothing recorded" when no quarter track holds a record; "13 and
  16 sector" when both kinds of standard sector occur anywhere on the disk;
  "16 sector" or "13 sector" when only that kind occurs; "Nonstandard" when
  at least one track is nonstandard; "Unformatted" when at least one track is
  unformatted; otherwise "Damaged". On a disk with standard sectors,
  nonstandard and unformatted tracks show only in their own chips. The
  summary MUST be shown as chips beside the image's file name, which is
  shortened with an ellipsis when it does not fit and shown in full in its
  tooltip. The sectors-good chip MUST take the bad color when any sector is
  bad. A chip whose count is zero MUST be hidden, and so MUST the volume chip
  on a disk with no standard sectors.
- **FR-019**: Through "Decode settings...", the user MUST be able to set the
  16-sector address prologue, the 13-sector address prologue, the data
  prologue, and the address and data epilogues the analyzer matches, where
  each byte accepts ?? for any value; turn off the address checksum, data
  checksum and epilogue checks; and apply the settings to the whole disk or
  to a chosen range of tracks. Custom marks MUST be matched in addition to
  the standard ones unless "Match standard marks too" is turned off. A change
  MUST analyze again every track it applies to, a chip MUST show while
  nonstandard settings are in use, and "Reset to standard" MUST restore the
  standard settings. The settings MUST last only as long as the window shows
  that disk.
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
  (FR-063). The angle around the platter MUST be the position along the track
  as Casso's drive plays it, as a fraction of one turn, increasing clockwise
  from the index.
- **FR-023**: The platter MUST switch between "Structure" and "Timing"
  modes. In Structure mode it MUST color each stretch by kind: sync, address
  marks, address field, data marks, data field, failed checksum, other,
  noise, random-bit region, and nothing recorded. In both modes, a damaged
  quarter track MUST be drawn in its own color with a hatch, with the damage
  reason in its tooltip; a ring not yet analyzed in a pending pattern; and
  rings past 34.75 dimmed, behind the line at the head's limit (FR-004). A
  feature narrower than one device pixel MUST be drawn one device pixel wide
  in its own color, and where features meet in one pixel, a failed checksum
  MUST be drawn over noise, noise over a random-bit region, and those over
  every other kind. A failed address field MUST be drawn in the
  failed-checksum color and described as an address field, never as a data
  field, in its tooltip ("Address field, checksum failed") and in the Fields
  tab.
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
  the physical sector holds under the standard skew. Below the header it MUST
  show the 256 bytes as 16 rows of 16 with hex offsets and a text column
  (high bit masked, "." for anything not printable), with zero bytes and
  bytes of $80 or more in their own colors. Bad data MUST be shown and
  marked. With no sectors it MUST show "No standard sectors on this track.
  The Nibbles tab shows what is recorded on it." For an empty quarter track
  it MUST show "Nothing recorded on this quarter track.", for a damaged one
  the damage reason, and for a sector with no data field the header followed
  by "This sector has no data field." in place of the bytes.
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
  a track MUST be worded neutrally, without the words damage or protection.
  Findings about the image file MUST describe the record or the file as
  damaged, with the reason, when a record cannot be read, a map entry is out
  of range or the checksum does not match, and MUST describe the other image
  file problems in FR-051 plainly, without the word damaged. Findings MUST
  include those in FR-015 and these: on a WOZ, one finding for each track
  record whose quarter tracks are anything other than one whole track N
  together with none, either or both of N-0.25 and N+0.25, giving the
  quarter tracks it covers (such as a record on a half track, on a quarter
  track alone, or across two whole tracks); 13-sector fields on a disk that
  also has 16-sector fields; on WOZ bit and flux tracks only, a track length
  more than 2% from nominal; random-bit regions on a formatted track; damaged
  image records (FR-053); and the image file problems in FR-051. The standard
  WOZ layout and the quarter tracks Casso builds for sector, NIB and NB2
  images MUST NOT produce a quarter-track finding, so a standard DOS 3.3 disk
  in any of these formats produces no findings. Selecting a finding MUST go
  to it.
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
  when present, and the ID and size of any other chunk. For a sector image it
  MUST show the sector order and a note that the tracks are built with
  volume 254.
- **FR-051**: The inspector MUST check the image file for these problems and
  report each one in Findings and the Image tab: a checksum mismatch; an INFO
  largest track smaller than the largest track record; a bit or byte count larger than its blocks hold; a META value
  outside the WOZ lists (language, requires_ram, requires_machine); an
  image_date that is not RFC 3339; duplicate or out-of-order chunks; data
  past the last chunk; a track record that no map refers to; and a NIB track
  with no sync.
- **FR-052**: Casso MUST keep, from each WOZ file it reads, every INFO field
  and META entry of the file's version, each track record's location and
  length fields, the records that no map refers to, and which map (TMAP or
  FLUX) each quarter track came from, for the inspector to show.
- **FR-053**: Damage MUST be shown per quarter track with its reason,
  including map entries out of range and empty track records that the drive
  reads as unformatted, never as a blank track with no explanation. Casso
  MUST record these two cases as damage, with the reason: a TMAP or FLUX
  entry from 160 to 254 (255 means no track), and a referenced track record
  with a zero start block or zero block count. The inspector MUST show each
  damaged record's reason on every quarter track that maps to it, not on one
  quarter track only.
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
  through the standard skew), a nibble offset or a cell.
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
- **FR-062**: Casso MUST make available to the inspector, for each drive,
  once per frame and as one consistent record: the head's quarter track, the
  track record under it, the angle as a fraction of one turn, whether the
  motor is on, whether the controller has the drive enabled, and whether the
  drive is reading, writing, or blocked from writing by write protection.
- **FR-063**: While the motor is on, the controller has this drive enabled
  and a disk is inserted, the head marker MUST sit at 12 o'clock and the
  platter MUST turn counterclockwise so that the drive's current position is
  under the marker; otherwise the platter MUST stay still. The head marker
  MUST point at the head's quarter track and show idle, reading, writing and
  write blocked differently. While the platter is zoomed in it MUST stay
  still and the head marker MUST move around it instead.
- **FR-064**: The window MUST show whether the disk in the drive is
  write-protected and each cause: the image's write-protect flag, a
  read-only file, no permission to write the file, Casso's write-protect
  setting for the drive, or a damaged image. A write the guest attempts on a
  protected disk MUST show as "write blocked", distinct from writing, and
  MUST NOT mark a track as written or cause any analysis.
- **FR-065**: When the head is on the shown track, the strip MUST show the
  drive's state (motor off, reading, writing or write blocked), and while the
  motor is on, the head's position with a short fading trail behind it.
- **FR-066**: "Follow head" (tooltip "Show the track the head is on") MUST
  select the head's quarter track after the head has stayed on it for about
  0.3 s, while the head marker itself moves at once. Selecting a track by
  hand MUST turn it off, and turning it on MUST select the head's track.
  "Go to head" MUST select the head's track once without turning Follow head
  on.
- **FR-067**: Casso MUST keep, for each track record, a count of guest writes
  to it, and for each quarter track, a count of the head's visits with the
  motor on, for bit and flux tracks alike. A save MUST NOT reset these
  counts, inserting or reloading a disk MUST start them at zero, and the
  inspector MUST be able to read them while emulation runs. The tracks to
  analyze again (FR-068) and the written and read marks (FR-047) come from
  these counts.
- **FR-068**: When the drive writes, the inspector MUST analyze again only the
  tracks written, MUST show their new contents within 500 ms of the write
  reaching the track, and during continuous writes to a bit track MUST
  update at least twice a second. On a flux track, the written stretch MUST
  appear within 500 ms of the write ending. The inspector MUST NOT analyze
  the whole disk except when the window first shows a disk (on opening, on a
  drive switch or after a machine change), on insert, on a reload after the
  file changed, or on a change of decode settings, and never once per frame.
  The selection and the zoom MUST be kept.
- **FR-069**: The window MUST stay open across reset, pause and machine
  changes, and after a machine change MUST show the new machine's drive.
  After a change to a machine with no Disk II in slot 6, the window MUST show
  "This machine has no Disk II controller in slot 6" and keep its other
  state. A hidden or minimized window MUST do no drawing and no analysis, and
  when shown again MUST analyze only the tracks written while it was hidden.
- **FR-070**: In Casso, the Image tab MUST describe the file as read at insert
  or at the last reload. A value Casso has changed since then, such as a
  track record's length after a guest write or the write-protect flag after
  a change from Casso, MUST be marked as changed and shown with its current
  value, and the marks MUST refresh after a write, a save, a write-protect
  change and a reload. The checksum line MUST show "Checked when the file was
  read". After Casso saves the disk, the tab MUST show a note that the save
  rewrote the file, since a WOZ is saved as WOZ 2, or WOZ 2.1 when it has
  flux tracks, with new track record locations.

**Casso Explorer hosts**

- **FR-071**: When an image in a format FR-003 lists is selected, the preview
  pane MUST show a small still platter in Structure mode and the summary
  chips together with 033's catalog or details preview, without delaying it.
  Until the analysis finishes, the preview MUST show 033's preview at once
  and a placeholder where the small platter goes. Clicking the small platter
  MUST open the inspector for that image. For a WOZ whose disk type is 3.5",
  the preview MUST show the note "Casso does not analyze 3.5" disks" in place
  of the platter and chips. An image that fails to open MUST show only 033's
  error. Images in other formats, such as the 3.5-inch and hard-disk images
  that GH #163 tracks, MUST get no platter and no chips.
- **FR-072**: "Inspect disk image" MUST appear on the right-click menu of an
  image in a format FR-003 lists, in the file list and in the folder tree,
  whether or not the image is writable, and MUST open an inspector window for
  that file. It MUST also appear on the file list's background menu while the
  list shows the contents of such an image, acting on that image, whether or
  not the image is writable and at any directory depth. With several images
  selected, it MUST open a window for each. Each image MUST have at most one
  window, and choosing "Inspect disk image" again MUST bring it to the front.
- **FR-073**: An Explorer inspector MUST show the file as saved and MUST
  analyze it again when the file changes on disk, including changes Explorer
  itself makes, keeping the selected track when it still exists. When the
  file is deleted or renamed, the window MUST keep the last analysis and show
  a note that the file is no longer there.
- **FR-074**: An Explorer inspector window MUST stay open when the Explorer
  window that opened it closes, MUST close when Explorer exits, and MUST NOT
  reopen at the next launch.
- **FR-075**: Explorer's command bar MUST NOT change, so it still matches
  File Explorer's.

**Themes and state**

- **FR-076**: Every color MUST come from the active theme, in every theme
  either host offers. No theme holds colors for the inspector's kinds today,
  so this spec adds inspector colors to each of Casso's three themes and to
  Explorer's Light and Dark themes, and defines the colors used for a theme
  that holds none. In each theme, all text other than unavailable controls
  MUST meet a 4.5:1 contrast ratio against its background, and every pair of
  Structure kind colors MUST differ by at least ΔE2000 10. A theme change
  MUST redraw every view without reopening it.
- **FR-077**: In Casso, the inspector window MUST be viewable in a light
  theme, so that the parts specific to Casso (FR-001) are checked in light
  and dark (SC-008). Casso's own themes are all dark, and 033 adds Light and
  Dark to Explorer only. [NEEDS CLARIFICATION: How does the inspector window
  in Casso get a light theme: by following the Windows light or dark app
  setting, by a Light and Dark choice of its own, or by Casso gaining a Light
  theme?]
- **FR-078**: Good and bad states, and address and data marks, MUST NOT
  differ by color alone; each MUST also differ in symbol or pattern.
- **FR-079**: Each host MUST save, separately from the other, the window's
  placement and size, the mode, the selected disk tab and track tab, and the
  timing range. Casso MUST also save Follow head, the last drive and whether
  the window was open, and at launch MUST reopen the window on the last
  drive if it was open at exit. The zoom, the pan, the selection and the
  decode settings MUST last only while the window shows the same disk.

**Performance**

- **FR-080**: Analysis and drawing MUST NOT slow emulation beyond the limits
  in SC-005, including with the window open and following the head.
- **FR-081**: Zooming and panning the platter and the strip MUST stay smooth
  at every zoom level (see SC-004).

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
- **Sector**: an address field and the data field paired with it, with the
  256 decoded bytes and, on a 16-sector track, its DOS 3.3 logical sector
  and ProDOS block.
- **Finding**: a category, a location (quarter track, sector, cell) and a
  description.
- **Decode settings**: the 16-sector and 13-sector address prologues, the
  data prologue and the epilogues matched, with ?? for any byte; whether the
  standard marks are matched too; the checks turned on; and the tracks they
  apply to. Standard by default.
- **Image details**: the file's format, size, read-only attribute and
  checksum result, and for a WOZ its INFO fields, META entries, maps, track
  records and damage. In Casso, the file as read at insert or at the last
  reload, with the values Casso has changed since.
- **Head state** (Casso only): for each drive, its quarter track, the track
  record under the head, its angle, whether the motor is on, whether the
  controller has it enabled, and whether it is reading, writing or blocked
  from writing.
- **Write and visit counts** (Casso only): for each track record, the guest
  writes since the disk was inserted; for each quarter track, the head's
  visits with the motor on since then.

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
- **SC-007**: Opening, using and closing the inspector in every host leaves
  every test image byte-for-byte unchanged on disk and never marks a mounted
  disk as changed.
- **SC-008**: Before handoff, every view is checked on screen in the Light
  and Dark themes and in each of Casso's own themes, in every host that
  offers the theme, with the parts specific to Casso checked in the light
  theme FR-077 provides. On those captures, all text other than unavailable
  controls meets a 4.5:1 contrast ratio against its background, every pair of
  Structure kind colors differs by at least ΔE2000 10 in each theme, and good
  and bad states remain distinct in a grayscale capture.
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
- **SC-013**: A standard DOS 3.3 DSK, a NIB and a standard WOZ 2 of the same
  made-up disk each open with no findings.

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
- The decode settings are not saved, because they belong to one disk.
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
  settled in planning.
- Each host saves its inspector state in its own settings, because a running
  Casso rewrites its settings file whole and Explorer keeps its own.
- 033 adds Light and Dark to Explorer, beside Casso's three themes; Casso
  keeps only its three dark themes (Skeuomorphic, Dark modern and Retro
  terminal). FR-077 holds the open question of how the parts specific to
  Casso are checked in light.
- GH #135 (both Disk ][ drives share one head position) is open, and this
  spec does not fix it; the edge cases give what the head marker shows until
  it is fixed.
- The development machine is the owner's; the timing targets in SC-003,
  SC-004, SC-005 and SC-009 are measured there.
