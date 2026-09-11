# Feature Specification: Cassque

**Feature Branch**: `033-cassque`

**Created**: 2026-09-10

**Status**: Draft

**Input**: User description: "Cassque: a GUI file browser for Apple II
disk images, shipped as a separate executable holding no code, with
everything in the core library. Explorer-style tree, file list and preview
pane; every operation the command-line disk tool offers; drag and drop as
source and sink with conversion only when crossing into the host file
system; launched from Casso's menus and able to insert images into a
running Casso; Windows light and dark plus Casso's three themes."

## Background

Casso can read and write Apple II disk images, but only from the command
line. Getting a file off a disk, putting one on, or seeing what a disk
holds means typing a command per step, and comparing two disks means two
terminal windows. Casso's disk picker knows which folders hold disks, but
that knowledge is derived from a short recent-disks list and lives in the
emulator alone.

Cassque is the window onto all of that. It is a second executable beside
Casso, sharing its core library and its preferences, and it does for disk
images what File Explorer does for folders: browse, inspect, preview, copy,
delete, create, and hand a disk to the emulator. Its name is the
cassowary's casque, and the About dialog says so with a picture of the
bird.

The command surfaces it is built on, one command model, one dropdown, one
toolbar, are delivered by the preceding feature. This one adds the widgets
the browser layout needs and the browser itself.

## Clarifications

### Session 2026-09-10

- Q: How is a host file's conversion chosen on drag-in? → A: By content,
  never by extension. Applesoft when every non-blank line is an ascending
  line number at most 63999 followed by a keyword or an identifier and
  `=`; otherwise text when entirely printable; otherwise binary, put raw,
  with the put dialog opened once for the load address, prefilled $2000
  for 8192-byte files and $803 otherwise. Integer BASIC listings are not
  tokenized on the way in and fall to the text rule. No grammar
  validation for either BASIC, since the Apple never required one.
- Q: Is Integer BASIC covered? → A: Preview and Get need a new Integer
  BASIC detokenizer; the tree detokenizes Applesoft only today. No
  Integer BASIC tokenizer.
- Q: How do type and address survive a trip through the host file
  system? → A: Descriptive suffixes Cassque writes, since catalog names
  carry no extension: converted files as `NAME.Applesoft BASIC.txt`,
  `NAME.Integer BASIC.txt`, `NAME.Text.txt`; raw copies as
  `NAME.Binary.$AAAA.bin` for binary on either file system,
  `NAME.ProDOS.$TT.bin` with `.$AAAA` before `.bin` when aux is nonzero
  for other ProDOS types, and `NAME.DOS.X.bin` for other DOS 3.3 types.
  The trailing `.txt` or `.bin` gives Windows a real extension for file
  associations. On the way in Cassque understands these, the
  CiderPress `#TTAAAA` form, and no suffix at all (content rule), and
  strips the suffix from the catalog name. A setting, Host file names:
  Descriptive or CiderPress, default Descriptive, chooses what Cassque
  writes. Only a raw copy round-trips byte for byte; a converted listing
  is a different file from the tokenized original.
- Q: Is the theme choice shared with Casso or Cassque's own? → A:
  Cassque's own, stored in the shared preferences file under its own key.
  On first run it starts from Casso's current theme mapped onto Cassque's
  palette for it, then the two diverge independently.
- Q: What adds a folder to the Casso root, and can the user manage the
  list by hand? → A: A folder becomes known when a disk image in it is
  handed to Casso: Insert into a drive, Open in new Casso, or a drag onto
  a Casso window. Browsing, previewing, copying out or editing do not.
  Any host folder's context menu offers Add to Casso, and any known
  folder's offers Remove from Casso, whether or not the folder still
  exists.
- Q: Insert into a drive that already holds a disk? → A: Replace without
  asking, as Casso's own picker does. Casso's eject flushes the old
  image's unwritten changes before the swap, so nothing is lost; Casso
  refuses only when the drive is mid-write or the flush fails, and
  Cassque shows that refusal. Cassque's own writes to a mounted image go
  with the reload intent and land in Casso's existing external-change
  policy, which never discards guest writes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Browse and inspect a disk (Priority: P1)

A user opens Cassque, expands the Casso root in the tree, sees the folders
Casso knows, expands one, and sees its disk images. Expanding a disk shows
its directories; selecting the disk fills the file list with its catalog:
name, type, size, load address or aux type, locked flag, and modified date
where the file system records one. Selecting a file shows it in the
preview pane. The status bar shows how many entries are selected, the
selected entry's type, size and address, and the volume's free space.
Under the This PC root the user navigates the whole host file system the
same way, and any folder that holds a disk image the user opens is added
to the Casso root for next time.

**Why this priority**: A catalog viewer with preview is already useful on
its own and is the surface every other story acts on.

**Independent Test**: Open a folder of known images under This PC, expand a
ProDOS image and a DOS 3.3 image, select files of each type, and confirm
the list columns and the preview match what the command-line list and get
commands report.

**Acceptance Scenarios**:

1. **Given** a folder with ProDOS and DOS 3.3 images, **When** each is
   expanded in the tree, **Then** the ProDOS image shows its
   subdirectories as child nodes and the DOS 3.3 image shows none.
2. **Given** a selected disk image in the tree, **When** the file list
   fills, **Then** every entry the command-line list reports appears with
   the same name, type and size.
3. **Given** an Applesoft file selected, **When** the preview pane is
   visible, **Then** it shows the same listing the command-line basic
   conversion produces.
4. **Given** a binary file whose load address and length match a
   graphics buffer, **When** selected, **Then** the preview shows it as a
   picture; **Given** any other binary, **Then** a hex dump with a
   disassembly toggle.
5. **Given** a disk image selected in the file list rather than the tree,
   **When** the preview pane is visible, **Then** it shows the disk's
   catalog.
6. **Given** the preview pane hidden with Alt+P, **When** Cassque is
   closed and reopened, **Then** it stays hidden.
7. **Given** a folder under This PC not in the Casso root, **When** the
   user inserts a disk image from it into Casso, **Then** the folder
   appears under the Casso root, and Casso's own disk picker offers it
   next time; **When** the user only browses or previews, **Then** it
   does not.
8. **Given** any host folder, **When** Add to Casso is chosen, **Then** it
   appears under the Casso root; **Given** any known folder, **When**
   Remove from Casso is chosen, **Then** it leaves the root and the
   picker.

---

### User Story 2 - Every disk operation from the window (Priority: P1)

A user right-clicks a file and gets, copies it out, deletes it, or opens
it. They right-click a disk image and put files onto it, delete files, set
it as the boot disk for a new emulator instance, or format it. They
right-click a folder and create a new blank disk with a chosen format,
volume name and bootable option. For the debug-minded, a sector or block
can be read into a file or written from one. Text and BASIC conversions
happen on get and put exactly as the command line does them.

**Why this priority**: Parity with the command-line disk tool is the
functional contract; without it the browser is a viewer.

**Independent Test**: For each command-line disk verb, perform the same
operation from a context menu and compare the resulting image or file
byte for byte with the command-line result.

**Acceptance Scenarios**:

1. **Given** a file selected, **When** Get is chosen with a target folder,
   **Then** the host file matches the command-line get with the same
   conversion option.
2. **Given** a host file, **When** Put is chosen onto a disk with a type
   and load address, **Then** the image matches the command-line put.
3. **Given** a file selected, **When** Delete is chosen and confirmed,
   **Then** it is gone from the catalog and the free space grows.
4. **Given** a folder, **When** New Disk is chosen with format, volume
   and bootable settings, **Then** the resulting image matches the
   command-line create.
5. **Given** an image, **When** Format is chosen and confirmed, **Then**
   the image matches the command-line init.
6. **Given** a sector or block address, **When** Read or Write is chosen,
   **Then** the bytes match the command-line sector and block commands.
7. **Given** any operation that fails, **When** it fails, **Then** the
   user sees the same message text the command line prints, in a dialog.

---

### User Story 3 - Drag and drop (Priority: P2)

A user drags files between two disk images in two tabs or two windows,
and they are copied byte for byte with no conversion. They drag a file
from a disk image into a host folder, in Cassque or in File Explorer, and
it arrives converted for its type: BASIC as a listing, text as text,
everything else raw. They drag a host file into a disk image and it is
put with the conversion its content calls for. They drag a disk image onto a running
Casso window and it is inserted, as dropping from File Explorer does
today.

**Why this priority**: This is the interaction that makes a browser feel
like one, and it depends on the operations from story 2.

**Independent Test**: Perform each of the four drags and compare the
result to the equivalent story 2 operation.

**Acceptance Scenarios**:

1. **Given** two Apple disk images, **When** a file is dragged between
   them, **Then** the copy is byte identical and keeps its type, address
   and locked flag.
2. **Given** a BASIC file dragged to File Explorer, **When** dropped,
   **Then** a text file with the listing appears in the target folder.
3. **Given** a host text file dragged onto a disk image, **When**
   dropped, **Then** it is put as a text file with the text conversion.
4. **Given** a disk image dragged onto a running Casso window, **When**
   dropped, **Then** Casso inserts it exactly as it does for a drop from
   File Explorer.
5. **Given** a drag over a target that cannot accept it, **When**
   hovering, **Then** the cursor shows no-drop and nothing happens on
   release.

---

### User Story 4 - Hand a disk to Casso (Priority: P2)

A user right-clicks a disk image and chooses Insert into Drive 1 or
Drive 2, or Open in new Casso. If a Casso is running, the insert goes to
it. If none is, one is launched with the disk in the drive. Drive 2 is
offered only when the machine that would receive the disk has a second
drive. When Cassque was launched from Casso's menu, it talks to that Casso;
when started on its own, it picks a running one or launches one.

**Why this priority**: The round trip is the reason Cassque lives beside
the emulator rather than as a standalone tool.

**Independent Test**: With Casso running, insert into each drive and see
the drive widget change; with Casso closed, Insert launches one with the
disk mounted; Open in new Casso always launches a new instance.

**Acceptance Scenarios**:

1. **Given** Cassque launched from Casso's menu, **When** Insert into
   Drive 1 is chosen, **Then** that Casso instance mounts the disk.
2. **Given** the target machine has one drive, **When** the context menu
   opens, **Then** Insert into Drive 2 is disabled.
3. **Given** no Casso running, **When** Insert is chosen, **Then** a Casso
   starts with the disk in that drive.
4. **Given** any state, **When** Open in new Casso is chosen, **Then** a
   new Casso starts with the disk in drive 1, leaving any running one
   untouched.
5. **Given** the disk is mounted in the running Casso, **When** Cassque
   writes to it, **Then** Casso reloads the image, as the command line's
   on-change option does.

---

### User Story 5 - Tabs, keyboard, themes and accessibility (Priority: P3)

A user opens several disks in tabs and switches between them. They drive
the whole window from the keyboard: Tab between panes, arrows in tree and
list, Enter to open, Delete to delete, F2 to rename where the file system
allows it, the application key for the context menu, Ctrl+T and Ctrl+W
for tabs. The window follows the Windows light or dark setting as it
changes, or a manual override, or one of Casso's three themes, with the
skeuomorphic one offered as colors only. Every control carries an
accessible name and role, ready for the automation provider a later
feature adds.

**Why this priority**: Polish that makes the tool complete, none of it
blocking the stories above.

**Independent Test**: Complete story 1 and story 2 flows with the mouse
unplugged; toggle the Windows theme and watch the window follow; walk the
window with a screen reader.

**Acceptance Scenarios**:

1. **Given** two tabs open, **When** Ctrl+Tab is pressed, **Then** the
   other tab is shown with its own tree selection and list.
2. **Given** focus in the tree, **When** Right is pressed on a disk image
   node, **Then** it expands; **When** Enter is pressed, **Then** the file
   list fills.
3. **Given** the Windows theme set to follow system, **When** the system
   switches to dark, **Then** the window switches without restart.
4. **Given** the theme menu, **When** opened, **Then** it lists Light,
   Dark, Follow system, and Casso's three themes with the skeuomorphic
   entry marked as colors only.
5. **Given** any control in the window, **When** it is inspected, **Then**
   it carries an accessible name and role. Announcement to a screen
   reader is out of scope: the UI library has no automation provider,
   and adding one is a follow-on feature that serves Casso's chrome too.

---

### Edge Cases

- **A disk image that fails to parse**: it appears in the tree with no
  expand affordance and a tooltip carrying the error; selecting it shows
  the error in the file list area.
- **A write-protected or read-only image**: put, delete and format are
  disabled in its menus, and a drop onto it shows no-drop.
- **The image is open in a running Casso**: writes go through, and Casso
  is told to reload, as story 4 scenario 5 says. If the guest also has
  unwritten changes, Casso keeps both versions and Cassque reports the
  conflict; if the drive is mid-write, the user sees the refusal.
- **Insert into an occupied drive**: the old disk is flushed and ejected,
  the new one mounted, no prompt. A failed flush is shown as Casso's
  refusal.
- **A file name that is illegal on the host**: get and drag-out substitute
  host-legal characters and tell the user the resulting name.
- **A host file name that is illegal on the target file system**: put and
  drag-in offer the truncated or corrected name before proceeding.
- **A put that does not fit**: refused with the free space and the file's
  size in the message; nothing is written.
- **Both roots show the same folder**: the Casso root lists it as known;
  This PC lists it in place. Selecting either fills the same list.
- **Known folder no longer exists**: it stays listed, grayed, and Remove
  from Casso works on it as on any known folder.
- **Two Cassque windows edit the same image**: last write wins, and the
  other window refreshes on focus; no locking.
- **Casso launched Cassque and then exited**: Insert falls back to the
  standalone behavior.
- **Cassque menu item clicked while Cassque is already open**: the
  existing window launched by that Casso is brought to the front. A
  standalone Cassque is a separate instance from one Casso launched.

## Requirements *(mandatory)*

### Functional Requirements

**Shell**

- **FR-001**: Cassque MUST ship as a separate executable holding no code,
  with all logic in the core library that Casso and the unit tests link.
- **FR-002**: Cassque MUST be launchable from Casso's menu and standalone.
  A second launch from the same Casso MUST bring that Casso's existing
  Cassque window to the front rather than open another.
- **FR-003**: Cassque MUST have an About dialog that explains the name
  with a picture of a cassowary.

**Tree and roots**

- **FR-004**: The tree MUST have a Casso root listing known folders and a
  This PC root listing the host's drives, each navigable to any depth.
- **FR-005**: Known folders MUST be a persisted preference shared with
  Casso, seeded on first run from the folders of Casso's recent disks,
  and extended with the folder of any disk image handed to Casso from
  Cassque: Insert into a drive, Open in new Casso, or a drag onto a Casso
  window. Browsing, previewing, copying out or editing MUST NOT add a
  folder.
- **FR-005a**: Every host folder's context menu MUST offer Add to Casso,
  and every known folder's MUST offer Remove from Casso, whether or not
  the folder still exists.
- **FR-006**: Casso's disk picker MUST read the same known-folder list.
- **FR-007**: Disk images with a supported extension MUST appear in the
  tree as expandable nodes, ProDOS subdirectories as children, DOS 3.3
  with no children.
- **FR-008**: Selecting a folder or image in the tree MUST fill the file
  list; selecting an image in the file list MUST show its catalog in the
  preview.

**File list and status**

- **FR-009**: The file list MUST show name, type, size, load address or
  aux type, locked, and modified where the file system records it, with
  sortable columns and multiple selection.
- **FR-010**: The status bar MUST show the selection count, the selected
  entry's type, size and address, and the volume's free space.

**Preview**

- **FR-011**: The preview pane MUST be toggleable from the View menu and
  Alt+P, and its state MUST persist.
- **FR-012**: The preview MUST show BASIC files as listings, text files as
  text, graphics-sized binaries as pictures, other binaries as a hex dump
  with a disassembly toggle, and disk images as their catalog. The
  graphics rule is: hi-res at $2000 or $4000 with 8192 or $1FF8 bytes;
  double hi-res at $2000 with 16384 bytes; lo-res at $400 or $800 with
  1024 bytes.

**Operations**

- **FR-013**: Every verb of the command-line disk tool MUST be available
  from context menus and produce byte-identical results: list, get, put,
  delete, boot, create, init, sector read and write, block read and
  write, with the text and BASIC conversions.
- **FR-014**: Operations MUST call the core disk runner directly, never
  spawn the command-line tool.
- **FR-015**: Destructive operations, meaning delete, format and
  overwriting put, MUST confirm before acting.

**Drag and drop**

- **FR-016**: A drag between two Apple disk images MUST copy raw with no
  conversion, preserving type, address and locked flag.
- **FR-017**: A drag or Get from a disk image to a host folder, in Cassque
  or another application, MUST convert per file type: BASIC to a listing,
  text to text, everything else raw, with the file materialized only when
  the target asks for it, and MUST give the host file a descriptive
  suffix, since catalog names carry none: `NAME.Applesoft BASIC.txt`,
  `NAME.Integer BASIC.txt` and `NAME.Text.txt` for converted files;
  `NAME.Binary.$AAAA.bin` for binary on either file system;
  `NAME.ProDOS.$TT.bin`, with `.$AAAA` before `.bin` when aux is nonzero,
  for other ProDOS types; `NAME.DOS.X.bin` for other DOS 3.3 types.
  Host-illegal characters are substituted and the user told the
  resulting name.
- **FR-017a**: A setting, Host file names: Descriptive or CiderPress,
  default Descriptive, MUST switch what Cassque writes to the CiderPress
  `NAME#TTAAAA` form for raw copies. Reading MUST accept both forms and
  bare names regardless of the setting.
- **FR-017b**: A ProDOS directory dragged out MUST become a host folder
  recursively; a host folder dragged onto a ProDOS image MUST become a
  directory recursively; a folder dropped on a DOS 3.3 image MUST be
  refused with a message, since DOS 3.3 is flat.
- **FR-018**: A drag from the host into a disk image MUST choose its
  conversion by content: Applesoft when every non-blank line is an
  ascending line number at most 63999 followed by a keyword or an
  identifier and `=`; otherwise text when entirely printable; otherwise
  binary put raw, with the put dialog opened once for the load address,
  prefilled $2000 for 8192-byte files and $803 otherwise. A file carrying
  a descriptive suffix or a CiderPress `#TTAAAA` suffix MUST take its
  type and aux from the suffix with no dialog, and the suffix MUST be
  stripped from the catalog name. Otherwise extension MUST NOT decide.
  Integer BASIC listings are not tokenized and fall to the text rule.
- **FR-018a**: Get and preview with the BASIC conversion MUST refuse with
  the decoder's error rather than write or show a partial listing.
- **FR-019**: A drag of a disk image onto a running Casso window MUST be
  accepted by Casso's existing drop handling.

**Casso integration**

- **FR-020**: A disk image's context menu MUST offer Insert into Drive 1,
  Insert into Drive 2, and Open in new Casso.
- **FR-021**: Insert into Drive 2 MUST be enabled only when the receiving
  machine declares a second drive, determined in-process from the default
  machine's layout when no Casso is running and by asking the running
  Casso otherwise.
- **FR-022**: Insert MUST target the Casso that launched Cassque when
  there is one, otherwise a running Casso, otherwise a newly launched one.
- **FR-023**: Open in new Casso MUST always launch a new instance with the
  disk in drive 1.
- **FR-024**: A write to an image mounted in a running Casso MUST tell
  that Casso to reload the image, and MUST show Casso's answer when it is
  a conflict or a refusal rather than a reload.
- **FR-024a**: Insert into an occupied drive MUST replace the disk without
  a confirmation in Cassque. Casso flushes the outgoing image's unwritten
  changes before the swap; Cassque MUST show Casso's refusal when the
  drive is mid-write or the flush fails.

**Tabs, keyboard, themes**

- **FR-025**: Cassque MUST support multiple tabs, each with its own tree
  selection, list and preview state, with Ctrl+T, Ctrl+W and Ctrl+Tab.
- **FR-026**: Every operation MUST be reachable from the keyboard, and
  every context menu MUST open from the application key and Shift+F10.
- **FR-027**: The theme menu MUST offer Light, Dark, Follow system, and
  Casso's three themes, with the skeuomorphic entry marked as colors
  only; Follow system MUST track the Windows setting live.
- **FR-028**: Theme choice, preview-pane state, host file naming and
  window placement MUST persist in the shared preferences file under
  Cassque's own keys. The theme key MUST be separate from Casso's; on
  first run it MUST be seeded from Casso's current theme mapped onto
  Cassque's palette for it, after which the two are independent.
- **FR-029**: Every control MUST carry an accessible name and role.
  Announcement to assistive technology is out of scope; the UI library's
  automation provider is a follow-on feature.

**Testability**

- **FR-030**: Browsing, cataloging, preview decoding, conversion, the
  drag data object's contents, and the Casso-targeting decision MUST be
  driven by unit tests with no window, no real file system and no running
  emulator.

### Key Entities

- **Known folder**: a host folder the user has opened a disk from; path,
  last-used time; persisted, shared with Casso.
- **Location**: what a tab is looking at: a host folder, a disk image, or
  a directory inside an image.
- **Catalog entry**: one file in an image: name, type, size, address or
  aux type, locked, modified; plus the file system it came from.
- **Preview**: a rendering of a catalog entry by kind: listing, text,
  picture, hex, catalog.
- **Drag payload**: the dragged entries with their source image, and the
  conversions the host side would need.
- **Casso target**: the instance an insert goes to: launcher, a running
  instance, or a new one; with its machine's drive count.
- **Tab**: a location, its selection, and its preview state.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every verb of the command-line disk tool, the result
  produced from Cassque is byte identical to the command-line result on
  the same input.
- **SC-002**: A user with no command-line experience can list a disk,
  preview a file, and copy it to a host folder within two minutes of
  first launch, with no documentation.
- **SC-003**: A folder of 200 disk images lists in under one second, and
  a disk's catalog appears within 200 ms of selection.
- **SC-004**: Every acceptance scenario in stories 1 through 4 completes
  with the mouse disconnected.
- **SC-005**: The Cassque executable holds zero functions, and the unit
  test project exercises browsing, catalog, preview, conversion and
  targeting with no window and no real disk.
- **SC-006**: The Windows theme switching to dark changes the window
  within one second with no restart.

## Assumptions

- The command widgets, meaning command, dropdown, toolbar, menu bar and
  context menu, are delivered by the preceding feature and are not
  re-specified here.
- Widgets this feature adds to the UI library: a tree of any depth with
  lazy expansion, a splitter, a status bar, drag-out support beside the
  existing drop-in support, and a tab host. Each is a general widget, not
  a Cassque-only one.
- Supported image formats are the ones the command-line tool supports
  today. No new container formats.
- Conversion rules on get, put and drag are the command-line tool's rules,
  plus an Integer BASIC detokenizer this feature adds for preview and Get.
  The tree detokenizes Applesoft only today. No Integer BASIC tokenizer:
  an Integer listing dragged in is put as text.
- Preview and Get show what a decoder can decode and mark where it
  stopped, since the Apple saves programs without validating them and a
  type A or I file may hold anything.
- Rename is offered only where the file system supports it in place;
  otherwise it is absent from the menu rather than emulated by copy and
  delete.
- Executable detection is not attempted; hex dump is the default for
  binaries and the graphics rule is the only exception.
- Sector and block operations live under an Advanced submenu and confirm
  before writing.
- Known folders are seeded once from the recent-disks list and thereafter
  maintained independently of it; the recent-disks list keeps its current
  role for Casso's own picker ordering.
- The emulator exposes what Cassque needs over its existing intent
  channel, which gains an insert intent and a describe-machine reply.
  Cassque is told the channel name on its command line when Casso
  launches it.
- Screen-reader announcement is out of scope. Dxui draws every control
  itself, so unlike native Windows controls nothing is announced until
  the library has an automation provider; that provider is one generic
  walk of the existing control tree plus a few patterns, serves Casso's
  chrome as well, and is its own follow-on feature. Cassque sets names
  and roles now so that provider needs no per-control work here.
