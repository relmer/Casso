# Feature Specification: Casso Explorer as the insert disk picker

**Feature Branch**: `049-explorer-picker`

**Created**: 2026-11-14

**Status**: Draft

**Input**: User description: "Throw away the emulator's insert disk dialog and the plain file pickers for disk images. Casso Explorer in a picker mode becomes the file-open experience, scoped to the receiving drive. The dialog needs to report the form factor (3.5, 5.25, hdd, tape) and formatted size (140k, 400k, 800k, ...) and filter the list to only images matching the recipient drive. In picker mode it is scoped to the set of file masks passed in, with a contract similar to the Win32 file open and file save dialogs."

## Background

Spec 048 added 3.5-inch drives and hard disks. A machine can now have a Disk II, an Apple 3.5 Drive, a UniDisk 3.5, an HD20SC, a ProFile and a cassette recorder at once, and each accepts a different set of images. The insert disk dialog and the Windows file pickers behind it predate that. They list every image for every drive, report neither form factor nor formatted size, and become hard to use with many images. A wrong pick fails only at mount. A drop onto a drive has the same gap: every drive displays the copy cursor for any image.

Casso Explorer already lists folders of images with columns, sorting, search and a preview. This feature makes it the picker.

## Terms

- **Image**: a file that a drive or the recorder can load: a floppy disk image, a hard disk image or a tape recording.
- **Form factor**: 5.25-inch, 3.5-inch, hard disk or tape.
- **Formatted size**: the capacity of the medium the image represents: 140K, 400K, 800K, or a hard disk's size such as 32 MB. A tape has a duration instead.
- **Receiving drive**: the drive, or the recorder, that the picker was opened for or that a drop is over.
- **Picker**: Casso Explorer opened to choose one file for a caller, as opposed to Casso Explorer opened as a file manager.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Insert a disk from a drive (Priority: P1)

A user clicks a drive, chooses "Insert disk..." on its menu, or chooses the drive's entry on the Storage menu. Casso Explorer opens as a picker for that drive. It lists folders and only the images the drive accepts, with form factor and formatted size columns. The user browses or searches, picks an image, and the image is inserted in that drive. Canceling closes the picker and the drive's contents do not change.

**Why this priority**: This is the replacement for the insert disk dialog, and every other story supports it.

**Independent Test**: With a folder containing 5.25-inch, 3.5-inch, hard disk and tape images, open the picker from each drive kind and confirm that the listed images are exactly the ones that drive accepts, and that a pick is inserted in the drive the picker was opened for.

**Acceptance Scenarios**:

1. **Given** a machine with a Disk II and a folder containing 140K, 800K and 32 MB images, **When** the user opens the picker from the Disk II, **Then** the list has the folder's subfolders and the 140K images, and it has neither the 800K nor the 32 MB images.
2. **Given** the same folder, **When** the user opens the picker from an Apple 3.5 Drive, **Then** the list has the 400K and 800K images, including 3.5-inch WOZ images, and no 140K or hard disk images.
3. **Given** the same folder, **When** the user opens the picker from an HD20SC or a ProFile, **Then** the list has the hard disk images that fit the drive, and no 140K, 400K or 800K images.
4. **Given** the picker is open for a drive and not pinned, **When** the user picks an image and confirms, **Then** the picker closes and the image is inserted in that drive, with the drive door animation the old dialog had.
5. **Given** pickers are open for two drives, **When** the user confirms an image in one, **Then** the image is inserted in that picker's drive and the other picker and its drive do not change.
6. **Given** a picker is open for a drive, **When** the user clicks that drive again, **Then** the open picker comes to the front and no second picker opens for it.
7. **Given** a pinned picker, **When** the user confirms an image, **Then** the image is inserted and the picker stays open for the same drive.
8. **Given** the picker is open for a drive, **When** the user cancels with the button, Esc or the close box, **Then** the picker closes and the drive's contents do not change.
9. **Given** a `.dsk` file whose length is not a known image length, **When** the picker lists its folder, **Then** the file is not in the list for any drive.
10. **Given** the picker is opened from the recorder, **When** the list is drawn, **Then** it has only tape recordings.

---

### User Story 2 - Form factor and formatted size in every listing (Priority: P2)

A user browsing a folder of images in Casso Explorer, as a file manager or as a picker, can read each image's form factor and formatted size in two columns, and can sort and filter by them. A collection of several hundred images can be sorted by formatted size to find the hard disk volumes.

**Why this priority**: The columns are what make a large collection manageable, and the picker's default view depends on them.

**Independent Test**: Open a folder of mixed images in Casso Explorer and confirm that each row's form factor and formatted size match the image, and that sorting by either column orders the rows correctly.

**Acceptance Scenarios**:

1. **Given** a folder with a 140K `.dsk`, an 800K `.2mg`, a DiskCopy 4.2 image, a 5.25-inch WOZ, a 3.5-inch WOZ, a 32 MB `.hdv` and a tape recording, **When** the folder is listed, **Then** the rows report 5.25-inch 140K, 3.5-inch 800K, the DiskCopy image's form factor and size from its header, 5.25-inch 140K, 3.5-inch 800K or 400K as the WOZ specifies, Hard disk 32 MB, and Tape.
2. **Given** a file that is not an image, **When** the folder is listed, **Then** both columns are empty for that row.
3. **Given** a folder of several hundred images on a local drive, **When** the folder is opened, **Then** the file list appears immediately and the two columns fill in without blocking scrolling, sorting or selection.
4. **Given** a listing sorted by formatted size, **When** the user reads down the column, **Then** sizes ascend or descend by capacity, not by the text of the label.

---

### User Story 3 - A drop follows the same rule (Priority: P2)

A user drags an image from File Explorer or Casso Explorer over the emulator. Over a drive that accepts the image, the cursor is the copy cursor. Over a drive that does not, the cursor is the no-drop cursor, and releasing there does nothing.

**Why this priority**: The picker and a drop are two ways to do the same thing. A rule that applies to one and not the other would be reported as a bug.

**Independent Test**: Drag one image of each form factor over each drive kind and over the recorder, and confirm the cursor and the result of releasing match the drive rule in every pairing.

**Acceptance Scenarios**:

1. **Given** an 800K image is dragged, **When** the pointer is over a Disk II, **Then** the no-drop cursor is displayed and releasing does not change the drive.
2. **Given** an 800K image is dragged, **When** the pointer moves from a Disk II to an Apple 3.5 Drive, **Then** the cursor changes to the copy cursor and releasing inserts the image there.
3. **Given** a tape recording is dragged, **When** the pointer is over any disk drive, **Then** the no-drop cursor is displayed.
4. **Given** a file that is not an image is dragged, **When** the pointer is over any drive, **Then** the no-drop cursor is displayed.
5. **Given** an image that fails the rule is released over a drive by a route that bypasses the cursor check, **When** the drop is handled, **Then** the drive's contents do not change and no mount is attempted.

---

### User Story 4 - A general file-open picker with filter entries (Priority: P3)

A caller that requires a file from the user passes a title, a starting folder and a list of filter entries. Each entry is a label and a set of file masks, such as "Disk images" with `*.po;*.2mg`. The picker lists folders and only the files matching the active entry's masks, offers the entries as a choice when there are several, and returns the chosen path or a cancel. A picker opened for a drive applies the masks and the drive rule together.

**Why this priority**: The drive-scoped picker is one use of this contract. Stating the contract separately lets other callers use it and lets tests exercise mask scoping without a drive.

**Independent Test**: Open the picker with two filter entries over a folder of mixed files, switch between the entries, and confirm that the list has only folders and files matching the active entry, and that the returned path is the picked file or a cancel.

**Acceptance Scenarios**:

1. **Given** filter entries "ProDOS images" (`*.po;*.2mg`) and "All images" (`*.po;*.2mg;*.dsk;*.woz`), **When** the picker opens, **Then** the first entry is active and only folders and `.po` and `.2mg` files are listed.
2. **Given** the same picker, **When** the user chooses the second entry, **Then** `.dsk` and `.woz` files are listed as well.
3. **Given** a single filter entry, **When** the picker opens, **Then** no filter choice is offered.
4. **Given** a starting folder that does not exist, **When** the picker opens, **Then** it opens in a fallback folder without an error.
5. **Given** the picker is open, **When** the user looks for commands that change files, start transfers, open connections or open other windows, **Then** none is reachable by menu, toolbar, keyboard shortcut or context menu.
6. **Given** a picker opened for a drive with the mask `*.po`, **When** a folder has a 140K `.po` and an 800K `.po` and the drive is a Disk II, **Then** only the 140K `.po` is listed.

---

### User Story 5 - Recent images, new disks and stock disks in the picker (Priority: P2)

The insert disk dialog lists recently used images, has a "Create new disk..." row, and offers downloads of stock system disks. The recorder's dialog lists recent tapes and offers a blank tape. All of these move into the picker.

Casso Explorer's tree already has a Casso node. Its children are the folders Casso has opened images from, most recently used first. Selecting the Casso node itself lists every image in those folders as one list, most recently used first. A picker opened for a drive starts on that list, filtered to the drive, so the images a user inserted lately are the first rows, as in the old dialog. The same list is available in the file manager.

**Why this priority**: Without it, removing the dialog takes away the fastest route to a disk used yesterday, and the routes to a new disk and to the stock system disks.

**Independent Test**: Insert images from two folders, open the picker from a drive, and confirm that the Casso node's list has the images of both folders that the drive accepts, with the ones just inserted first. Create a new disk and pick a stock disk from the picker and confirm each is inserted.

**Acceptance Scenarios**:

1. **Given** images were inserted from two folders, **When** the user selects the Casso node, **Then** one list has the images of both folders, without the folders as rows, and each row has its folder in a column.
2. **Given** that list, **When** it is drawn with its default sort, **Then** images Casso has loaded come first, newest load first, and the images never loaded follow, ordered by name.
3. **Given** the picker is opened from a drive for the first time, **When** it opens, **Then** it is on the Casso node's list, filtered to the images the drive accepts.
4. **Given** the picker is opened from the recorder, **When** it opens, **Then** the list has the recent tapes first.
5. **Given** the picker is open for a floppy drive, **When** the user chooses "Create new disk...", **Then** the create disk flow runs for that drive's form factor, and on completion the picker closes and the new disk is inserted.
6. **Given** the picker is open for the recorder, **When** the user chooses "New blank tape", **Then** a blank tape is created and loaded, and the picker closes.
7. **Given** the picker is open for a drive that a stock system disk fits, **When** the user picks a stock disk that is not downloaded yet, **Then** it is downloaded, after the consent step the old dialog had, and inserted.
8. **Given** a folder under the Casso node no longer exists, **When** the list is built, **Then** the folder is skipped and the rest of the list is complete.
9. **Given** the Casso node is selected in the file manager, outside picker mode, **When** the list is drawn, **Then** it is the same list with every image, not filtered to a drive.

---

### Edge Cases

- A raw 800K block image with a `.po` or `.dsk` extension is classified as a 3.5-inch 800K disk, never as a hard disk volume. The hard disks do not accept it.
- A raw 140K image is classified as 5.25-inch. A UniDisk 3.5 or a hard disk does not list it.
- A WOZ image's form factor comes from its header, not its length. A 3.5-inch WOZ and a 5.25-inch WOZ of similar length get different form factors.
- A 2IMG or DiskCopy 4.2 image has a header, so its formatted size comes from the header and not from the file length.
- A 2IMG image in nibble or DOS sector order is a 5.25-inch image and is not a block image for the hard disks.
- A file with an image extension that is truncated, zero bytes long, or has a damaged header has no classification. It is listed in the file manager with empty columns and is absent from every drive's picker.
- An image that is locked by another program or unreadable is treated the same way and does not stop the listing.
- An image already inserted in another drive of the running machine is listed and marked as in use; picking it follows the rule the emulator has today for mounting one image twice.
- A folder on a slow or disconnected network share does not freeze the picker; classification of its files can be abandoned on navigation to another folder.
- A tape has no formatted size. Its size column is empty or reports its duration.
- The receiving drive is removed, or the machine is switched, while the picker is open: the picker closes without inserting.
- An image in the Casso node's list is deleted or moved after the list is built: picking it reports the failure and the picker stays open.
- A picker is open for one drive and the user clicks a second drive: a second picker opens for the second drive, and the first is not changed.
- A picker is open for a drive and the user inserts or ejects a disk in that drive by another route (a drop, the menu, the command line): the picker stays open and its drive is the same.
- Casso Explorer is already running as a file manager, possibly with a serial session open. Opening a picker neither disturbs that window nor uses its session.
- A hard disk image larger than the receiving drive's capacity is not listed for that drive.
- A hidden or system file matching the masks follows the file manager's existing setting for hidden files.

## Requirements *(mandatory)*

### Functional Requirements

#### Image classification

- **FR-001**: One classification call MUST return, for a file, whether it is an image and, when it is, its form factor (5.25-inch, 3.5-inch, hard disk or tape) and its formatted size.
- **FR-002**: Classification MUST read only the file's extension, length and header. It MUST NOT read the whole image, decode tracks or parse the file system.
- **FR-003**: Classification MUST cover WOZ images (form factor and side count from the header), `.dsk`, `.do`, `.po`, `.nib` and `.d13` sector and nibble images, 2IMG, DiskCopy 4.2, raw block images including `.hdv`, and tape recordings.
- **FR-004**: A block image's form factor MUST follow from its block count: 280 blocks is 5.25-inch 140K, 800 blocks is 3.5-inch 400K, 1,600 blocks is 3.5-inch 800K, and any other count is a hard disk whose formatted size is its capacity.
- **FR-005**: A tape MUST be classified from its extension alone.
- **FR-006**: A file that has an image extension and fails the checks in FR-003 and FR-004 MUST be reported as not classified, distinct from a file that is not an image, so a damaged image is never listed as a healthy one.
- **FR-007**: Formatted size MUST be available both as a number, for sorting and comparison, and as a label: "140K", "400K", "800K", and megabytes for hard disks.

#### The drive rule

- **FR-008**: One rule, next to the classifier, MUST answer whether a given drive kind accepts a given classified image. Every caller (the picker, drops, and the mount commands) MUST use this rule and no other.
- **FR-009**: A Disk II and the //c's 5.25-inch drives MUST accept 5.25-inch images only.
- **FR-010**: An Apple 3.5 Drive and the //c Plus's built-in drive MUST accept 400K and 800K images, including 3.5-inch WOZ images.
- **FR-011**: A UniDisk 3.5 MUST accept 400K and 800K images that can be read as blocks.
- **FR-012**: An HD20SC and a ProFile MUST accept hard disk images up to the drive's capacity, and MUST NOT accept a 140K, 400K or 800K image. The rule is final: the picker has no way to list an image that fails it, and a mount by any other route fails the same way.
- **FR-013**: The recorder MUST accept tapes only, and no disk drive accepts a tape.
- **FR-014**: The rule MUST be total over the drive kinds: a drive kind added later without an entry MUST fail a test instead of accepting everything or nothing.
- **FR-015**: The rule and the mount code MUST be consistent. An image that passes the rule for a drive MUST NOT then fail at mount for its form factor or size, and the reverse.

#### Drops

- **FR-016**: While an image is dragged over the emulator, the cursor over each drive and the recorder MUST be the copy cursor when the rule accepts the image there and the no-drop cursor when it does not.
- **FR-017**: A drop on a drive that does not accept the image MUST do nothing: no mount attempt, no eject of the current disk, no error dialog.
- **FR-018**: A drop that lands on no drive follows the emulator's existing behavior for that case, limited to the drives that accept the image.
- **FR-019**: A drag of several files or of a file that is not an image MUST display the no-drop cursor over every drive.

#### The picker contract

- **FR-020**: A caller MUST be able to open the picker with a title, a starting folder, a list of filter entries (each a label and a set of file masks), and optionally a receiving drive, and MUST receive one of three results: a chosen path, a cancel, or a failure.
- **FR-021**: The picker MUST be available through the same request that Casso's callers already make for a file to open, so a caller that passes filter entries gets the picker without changing. Requests to open a disk or tape image in the emulator go to the picker. Other requests (serial files, printer output, screenshots, and Casso Explorer's dialogs), and all file-save and folder requests, go to the Windows dialogs.
- **FR-022**: With no receiving drive, the picker MUST list folders and the files matching the active filter entry's masks, and nothing else.
- **FR-023**: With a receiving drive, the picker MUST list folders and the files that both match the active entry's masks and are accepted by that drive under the drive rule.
- **FR-024**: Masks MUST match by file name, case-insensitively, with `*` and `?` wildcards and `;` between masks, as the Windows dialogs do. `*.*` matches every file.
- **FR-025**: When there are several filter entries, the picker MUST offer them as a choice, with the first active by default; when there is one, no choice is offered.
- **FR-026**: The picker has two forms. Opened for a drive of the running machine, it is a window of the emulator that is not modal: the emulator stays usable while it is open, and a confirm inserts the image in the drive. Opened by a caller that waits for a path, such as the Hardware page of Settings, it is modal to the caller's window and returns the path. Both forms confirm on the button, Enter or a double-click on a file, and cancel on the button, Esc or the close box.
- **FR-050**: A picker opened for a drive MUST stay associated with that drive until it is closed. A click on another drive MUST NOT change the drive of an open picker; it opens a picker for the other drive, so several pickers can be open, one per drive.
- **FR-051**: Opening the picker for a drive whose picker is already open MUST bring that picker to the front instead of opening a second one.
- **FR-052**: After a confirm, a picker opened for a drive MUST close, unless the user has pinned it open; a pinned picker stays open for the next insert in the same drive. The pin is saved with the picker's view.
- **FR-053**: A picker opened for a drive MUST close without inserting when the drive is removed, the machine is switched or the emulator window closes.
- **FR-054**: While a picker is open for a drive, the drive's door MUST be open if the drive is empty, and MUST follow the old dialog's door animation on insert and on close.
- **FR-027**: Confirm MUST be unavailable while the selection is empty or is a folder; Enter or a double-click on a folder opens the folder.
- **FR-028**: A test MUST be able to replace the picker with a scripted answer through the existing seam, and the mask and drive scoping MUST be testable without a window.

#### Picker mode in Casso Explorer

- **FR-029**: In picker mode, Casso Explorer MUST offer navigation (folder tree or breadcrumb, back, forward, up, typing a path), sorting, column choice, search within the listing, and the preview of the selected image.
- **FR-030**: In picker mode, no command that changes files, images or settings outside the picker MUST be reachable: no copy, move, delete, rename, new folder, format, transfer, serial link or image editing. A row can be dragged from the picker onto a drive of the emulator, where the drive rule applies as for any drop; a drag to any other target is not offered.
- **FR-031**: Picker mode MUST NOT restore Casso Explorer's saved session, MUST NOT open a serial port or a network connection, and MUST NOT write to the saved session on close.
- **FR-032**: Picker mode MUST save, per kind of request, the last folder and the view (columns, sort, window size) between uses, separately from the file manager's saved state.
- **FR-033**: A picker opened for a drive MUST use, the first time, a details view with file name, form factor, formatted size, file system and volume name columns, sorted by name, at a density suited to several hundred rows.
- **FR-034**: A picker opened for a drive MUST state the receiving drive in its title or header, for example "Insert disk - Apple 3.5 Drive, slot 5 drive 1", and MUST state what is being listed, for example "400K and 800K images".
- **FR-035**: The picker MUST start in the caller's starting folder. Without one, a picker opened for a drive starts on the Casso node's list (FR-043), and any other picker starts in the last folder used for that kind of request, or the emulator's disk folder the first time.

#### Columns

- **FR-036**: Casso Explorer MUST have a form factor column and a formatted size column, available in every folder listing, in the file manager and in the picker.
- **FR-037**: The columns MUST be filled without blocking the listing: the file list first, classification after, with results cached so that a return to a folder classifies only new and modified files.
- **FR-038**: Both columns MUST be sortable, with formatted size ordered by capacity, and MUST take part in the file manager's existing filtering and search where columns do.
- **FR-039**: The file system and volume name columns, filled today only in collection results, MUST also be filled for images in ordinary folder listings where the picker's default view includes them.

#### Replaced surfaces

- **FR-040**: The insert disk dialog MUST be removed, and every place that opened it MUST open the picker for its drive: a click on a Disk II, the Disk menu's insert commands, the drive bar, and the keyboard shortcuts.
- **FR-041**: The Windows file pickers for 3.5-inch images (the drive bar, the desk drives and the drive context menu), for hard disk images in Settings, and for tapes MUST be replaced by the picker for the corresponding drive kind.
- **FR-042**: The drive door animation and the confirmation before replacing a disk with unsaved inspector edits MUST work around the picker the way they work around the dialog today.

#### The Casso node's list, new disks and stock disks

- **FR-043**: Selecting the Casso node MUST list every image in the folders under it as one list, with a folder column, in the file manager and in the picker.
- **FR-044**: That list's default sort MUST be most recently used first: images with a load time from the recent list, newest first, then the images never loaded, by name. A "Last used" column MUST report the load time and be sortable.
- **FR-055**: The recent list MUST have load times for the 1,000 most recently loaded images, up from 16. A menu or list elsewhere that displays recent images limits what it displays separately.
- **FR-045**: The list MUST be built without blocking the window, MUST skip folders that are missing or unreadable, and MUST list an image once when two listed folders overlap.
- **FR-046**: In a picker opened for a floppy drive or a hard disk, a "Create new disk..." command MUST run the existing create flow, limited to the kinds of disk the drive accepts, and MUST return the new image as the pick.
- **FR-047**: In a picker opened for the recorder, a "New blank tape" command MUST create a blank tape and return it as the pick.
- **FR-048**: In a picker opened for a drive, the stock system disks that the drive accepts MUST be offered, marked as stock disks, whether downloaded or not. Picking one that is not downloaded MUST download it with the consent step the old dialog had, then return it as the pick.
- **FR-049**: An insert through the picker MUST update the recent list and the Casso node's folders exactly as an insert through the old dialog did.

### Key Entities
- **Image classification**: what one file is: not an image, an unclassified image, or an image with a form factor and a formatted size.
- **Drive kind**: the kinds of receiving drive: Disk II and //c 5.25-inch drives, Apple 3.5 Drive, //c Plus drive, UniDisk 3.5, HD20SC, ProFile, and the recorder.
- **Drive rule**: the table from drive kind and classification to accepted or not.
- **Filter entry**: a label and a set of file masks.
- **Picker request**: title, starting folder, filter entries and an optional receiving drive.
- **Picker result**: a chosen path, a cancel, or a failure.
- **Picker view state**: the last folder, columns, sort and window size, saved per kind of request and separate from the file manager's session.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every pairing of drive kind and image kind, the picker's listing, the drop cursor, the drop result and the mount result are the same.
- **SC-002**: Opened for a drive, the picker lists zero images that the drive then fails to mount for form factor or size.
- **SC-003**: A folder of 500 images on a local drive is listed in under one second and has both columns filled in under two seconds on first visit, and in under half a second on a return visit.
- **SC-004**: A user can go from a click on a drive to an inserted image in three actions when the image is in the last-used folder: click the drive, click the image, confirm.
- **SC-005**: No insert disk dialog and no Windows file picker for a disk or tape image remains reachable in the emulator.
- **SC-006**: Opening and closing the picker any number of times does not open a serial port, does not restore a saved session and does not change the file manager's saved session.
- **SC-007**: In picker mode, zero file-changing or connection commands are reachable by any input route.
- **SC-008**: Every image in the test collection that the emulator can mount is classified, and every classification matches what the mount reports for that image.

## Assumptions

- The picker opens a file. A file-save variant and a folder variant are out of scope. The contract is modeled on the Windows open dialog so a save variant can be added later without changing callers.
- One file is picked at a time; multi-select is not offered.
- The picker lists folders and files. It does not browse into an image or an archive to pick something inside, although the preview of the selected image is available.
- An "All files" filter entry passed by a caller for a drive-scoped request widens the masks only. The drive rule applies regardless.
- Hidden files, natural sort order and locale formatting follow the file manager's existing behavior.
- The picker runs inside the emulator's process as a window of the emulator, unless the plan finds a reason against it. It is not the user's running Casso Explorer and has no access to that window or its session. FR-031 applies either way.
- This work is cut from `048-smartport-drives` and merges 048 again before it lands, so 048's drive kinds and Casso Explorer from 033 are both present.
- The hard disk limit in FR-012 covers the HD20SC and the ProFile only. Other block devices, such as CFFA and A2Pico cards, are not drive kinds on the desk or the drive bar and accept 140K and 800K block images; the drive rule does not apply to them.
- 048 makes the mount-side check for FR-012 in its hard disk wiring. This work merges it and does not duplicate it.
- Command-line mounting (`--disk1` and related options) is outside the picker, but uses the drive rule through the mount commands (FR-008).
