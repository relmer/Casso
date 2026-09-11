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
   user opens a disk image inside it, **Then** the folder appears under
   the Casso root, and Casso's own disk picker offers it next time.

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
put with the inverse conversion. They drag a disk image onto a running
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
skeuomorphic one offered as colors only. A screen reader announces the
tree, list and preview with their names and roles.

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
5. **Given** a screen reader running, **When** focus moves to the file
   list, **Then** the list's name and the focused row's text are
   announced. [NEEDS CLARIFICATION: Dxui exposes accessible names and
   roles on controls but has no UI Automation provider, so nothing is
   announced to a screen reader today. Is a UI Automation provider in
   scope for this feature, or is this feature keyboard-complete with
   names and roles set, and the provider a follow-on?]

---

### Edge Cases

- **A disk image that fails to parse**: it appears in the tree with no
  expand affordance and a tooltip carrying the error; selecting it shows
  the error in the file list area.
- **A write-protected or read-only image**: put, delete and format are
  disabled in its menus, and a drop onto it shows no-drop.
- **The image is open in a running Casso**: writes go through, and Casso
  is told to reload, as story 4 scenario 5 says. If Casso refuses because
  the drive is mid-write, the user sees the refusal.
- **A file name that is illegal on the host**: get and drag-out substitute
  host-legal characters and tell the user the resulting name.
- **A host file name that is illegal on the target file system**: put and
  drag-in offer the truncated or corrected name before proceeding.
- **A put that does not fit**: refused with the free space and the file's
  size in the message; nothing is written.
- **Both roots show the same folder**: the Casso root lists it as known;
  This PC lists it in place. Selecting either fills the same list.
- **Known folder no longer exists**: it stays listed, grayed, with a
  Remove item in its context menu.
- **Two Cassque windows edit the same image**: last write wins, and the
  other window refreshes on focus; no locking.
- **Casso launched Cassque and then exited**: Insert falls back to the
  standalone behavior.
- **Cassque menu item clicked while Cassque is already open**: [NEEDS
  CLARIFICATION: bring the existing window to front, open a second
  window, or open a new tab in the existing window?]

## Requirements *(mandatory)*

### Functional Requirements

**Shell**

- **FR-001**: Cassque MUST ship as a separate executable holding no code,
  with all logic in the core library that Casso and the unit tests link.
- **FR-002**: Cassque MUST be launchable from Casso's menu and standalone.
- **FR-003**: Cassque MUST have an About dialog that explains the name
  with a picture of a cassowary.

**Tree and roots**

- **FR-004**: The tree MUST have a Casso root listing known folders and a
  This PC root listing the host's drives, each navigable to any depth.
- **FR-005**: Known folders MUST be a persisted preference shared with
  Casso, seeded on first run from the folders of Casso's recent disks,
  and extended with any host folder in which the user opens a disk image.
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
- **FR-017**: A drag from a disk image to a host folder, in Cassque or
  another application, MUST convert per file type: BASIC to a listing,
  text to text, everything else raw, with the file materialized only when
  the target asks for it.
- **FR-018**: A drag from the host into a disk image MUST put with the
  inverse conversion.
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
  that Casso to reload the image.

**Tabs, keyboard, themes**

- **FR-025**: Cassque MUST support multiple tabs, each with its own tree
  selection, list and preview state, with Ctrl+T, Ctrl+W and Ctrl+Tab.
- **FR-026**: Every operation MUST be reachable from the keyboard, and
  every context menu MUST open from the application key and Shift+F10.
- **FR-027**: The theme menu MUST offer Light, Dark, Follow system, and
  Casso's three themes, with the skeuomorphic entry marked as colors
  only; Follow system MUST track the Windows setting live.
- **FR-028**: Theme choice, preview-pane state and window placement MUST
  persist in the shared preferences.
- **FR-029**: Every control MUST carry an accessible name and role.
  Announcement to assistive technology depends on the clarification in
  story 5.

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
- Conversion rules on get, put and drag are the command-line tool's rules
  and nothing more. A host-side file type table decides which conversion
  a drag applies: BASIC types to a listing, text types to text, all else
  raw.
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
- Dxui's screen-reader support is the one open scope question and is
  marked in story 5.
