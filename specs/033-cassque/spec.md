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

### Session 2026-09-11 (walking the built window)

- Q: How close to native should the chrome sit? → A: Indistinguishable
  from File Explorer at the same scale, palette aside. The three Casso
  themes carry their own colors and nothing else of their own. Fixes
  belong to the UI library, so the emulator inherits them.
- Q: What does a splitter look like? → A: Explorer's: a hairline to the
  eye, a wide band to the pointer. The two are not the same measurement.
- Q: Where do tabs sit? → A: Outermost, as a browser puts them, with the
  menu bar, toolbar and address bar inside the tab. The library must
  support both arrangements, since a window with one toolbar wants the
  other.

### Session 2026-09-11

- Q: What face shows an Apple II file's text? → A: A fixed-width one, not
  the machine's. Building a face from the character generator table in the
  tree was tried and dropped: 47 of its 64 glyphs are bit-identical to
  Apple's own character ROM, so shipping a font built from it redistributes
  those shapes, and the seventeen that differ mean it was not faithful
  anyway. Every free reproduction is either licensed for personal use only
  or drawn from memory, and the 2513 datasheet prints one example glyph
  rather than the font. Reading the user's own video ROM at runtime stays
  open as a feature of its own, since nothing would be redistributed.
- Q: How does a hex dump let a user select bytes? → A: One selection over
  a range of bytes, drawn in both columns at once. A drag in either
  column selects the same bytes, and the other column highlights them, so
  the two readings of a byte are never selected apart.
- Q: What does the text column of a hex dump decode? → A: Apple text: the
  high bit is ignored, so $C1 reads as A. A byte that is not printable
  either way shows as a period.
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
  Cassque's own, stored beside Casso's preferences in Cassque's own file.
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

### Session 2026-09-14

- Q: Does Cassque understand AppleSingle files? → A: Yes. An AppleSingle
  file (magic $00051600) is one Apple II file carrying its data, real
  name, dates, and ProDOS type and aux type inside it. It is a file, not a
  disk image, and Cassque never lists or opens one as a volume. Put
  unwraps it with its type and aux type; Get can write one; its preview
  shows the file it holds; Casso does nothing with one. The codec is
  `AppleSingleCodec` in CassoEmuCore, shared with 035-debugger's BLOAD.
- Q: How is the host-name style chosen on the way out? → A: The Host file
  names setting offers AppleSingle beside Descriptive and CiderPress as the
  default, and a right-drag out of an image opens a menu choosing the
  style for that drop.
- Q: Does Cassque keep a menu bar? → A: No. File Explorer's command bar
  replaces it, duplicated as closely as possible. New tab, Close tab and
  Exit are gone from it, since the tab strip and the window already offer
  them. Of Edit, only Cut, Copy, Paste and Delete survive, as buttons with
  Explorer's icons. Cassque adds a Theme dropdown, as Casso's toolbar has,
  and keeps only its Preview toggle where Explorer offers Details and
  Preview, because Cassque's file list already shows details.
- Q: Where do the commands the menus held go? → A: Commands the preview
  pane's context menu offers leave the top level and stay on that menu.
  Settings such as Host file names move to an Options dialog, opened from
  the View dropdown, as Explorer opens its folder options.
- Q: What do Sort and View act on? → A: The file list, which gains the
  views a Win32 list view has rather than details alone.
- Q: What does the address bar remember? → A: Paths typed or pasted into
  it, not places reached by browsing, the last ten, most recent first; a
  path entered again moves to the top. A path that fails to navigate is
  not remembered.
- Q: Which of Explorer's New, Rename, Share and See more stay? → A: New,
  offering only what the listed location can hold; Rename, under the
  holding file system's rules; See more, which takes buttons that do not
  fit and can hold items permanently. Share is dropped. Help and About go
  in See more.
- Q: Which list views? → A: Explorer's eight.
- Q: How are tabs laid out and drawn? → A: Above the toolbars and address
  bar, below the system title bar, which stays. Explorer's tab shape, a
  fixed width that shrinks toward Explorer's minimum, left-aligned label,
  icon and close button, and a + button after the tabs and scroll arrows.
  Navigation buttons sit left of the address bar on one row, and the
  command bar spans the window below it.
- Q: Does a new disk image take the selected files? → A: Yes. New > Disk
  image with host files selected copies them into the new image, folders
  becoming ProDOS directories.
- Q: How does the command-line tool reach ProDOS directories? → A: Every
  file verb takes a path, relative to the volume directory or a full
  ProDOS path from the volume name. `list` gains `--recurse` (`-r`, `-s`),
  and `mkdir`/`md` and `rmdir`/`rd` are added; `mkdir` creates missing
  directories along its path with no flag. A bare `/word` that names an
  option stays an option; any other form of the path reaches the entry, so
  no separate escape is needed.
- Q: How does a recursive delete behave? → A: PowerShell's `Remove-Item`
  meanings for `--recurse` and `--force` (locked entries), with a listed
  plan and a confirmation `--yes` skips, applied all or nothing.
- Q: How is ProDOS name case handled? → A: Matched without regard to case,
  shown with GS/OS lowercase flags where present, and written in uppercase
  with those flags keeping the typed case.

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
4. **Given** the Theme dropdown, **When** opened, **Then** it lists Light,
   Dark, Follow system, and Casso's three themes with the skeuomorphic
   entry marked as colors only.
5. **Given** any control in the window, **When** it is inspected, **Then**
   it carries an accessible name and role. Announcement to a screen
   reader is out of scope: the UI library has no automation provider,
   and adding one is a follow-on feature that serves Casso's chrome too.

---

### User Story 6 - Read a file's bytes (Priority: P3)

A user looking at a binary wants to read it as bytes: to see a range of
them, to know which characters they stand for, and to copy what they
found into something else.

**Why this priority**: The hex dump already shows the bytes; this makes
them usable. It is the last of the preview work and nothing else waits
on it.

**Independent Test**: Open a binary in the preview, select a run of
bytes, change the grouping, and copy the selection both ways.

**Acceptance Scenarios**:

1. **Given** a text file previewed from an Apple II disk, **When** the
   preview shows it, **Then** its columns line up in a fixed-width face,
   and the text can be selected and copied as text.
2. **Given** a binary previewed as a hex dump, **When** the user drags
   across bytes in the hex column, **Then** those bytes highlight in the
   hex column and the same bytes highlight in the text column.
3. **Given** a selection made in the text column, **When** the user looks
   at the hex column, **Then** the same bytes are highlighted there,
   however many rows they span.
4. **Given** a selection, **When** the user changes the grouping to one,
   two, four or eight bytes, **Then** the same bytes stay selected and
   the columns regroup around them.
5. **Given** a selection, **When** the user copies it, **Then** they can
   take it either as hex digits or as the characters the text column
   shows.
6. **Given** a file larger than the pane, **When** the user asks to go to
   an offset, **Then** the view scrolls to that offset and puts the
   caret there.

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
- **A hex selection dragged past the last byte**: it stops at the last
  byte; a file shorter than one row still selects and copies.
- **A copy with nothing selected**: the whole preview is copied, so the
  command never does nothing without saying why.
- **A byte the character generator has no shape for**: control codes and
  MouseText show as the period the text column uses for anything it
  cannot print.
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
  and extended with the folder of any disk image handed to Casso: Insert
  into a drive, Open in new Casso, or a drop onto a Casso window from any
  source, including File Explorer. Casso records the hand-off, since
  only it sees every mount. Browsing, previewing, copying out or editing
  MUST NOT add a folder.
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
  aux type, locked, and modified, with sortable columns and multiple
  selection. Modified is shown for ProDOS entries and host files; DOS 3.3
  records no date and its rows leave the column blank.
- **FR-010**: The status bar MUST show the selection count, the selected
  entry's type, size and address, and the volume's free space.

**Preview**

- **FR-011**: The preview pane MUST be toggleable from the toolbar's
  Preview button and Alt+P, and its state MUST persist.
- **FR-012**: The preview MUST show BASIC files as listings, text files as
  text, graphics-sized binaries as pictures, other binaries as a hex dump
  with a disassembly toggle, and disk images as their catalog. The
  graphics rule is: hi-res at $2000 or $4000 with 8192 or $1FF8 bytes;
  double hi-res at $2000 with 16384 bytes; lo-res at $400 or $800 with
  1024 bytes.
- **FR-012a**: A text, listing, hex or disassembly preview MUST draw in a
  fixed-width face, on rows at the line's height, since every one of them
  is columns of characters the reader lines up by eye.
- **FR-012c**: A hex preview MUST carry one selection over a range of
  bytes, drawn at the same time in both the hex column and the text
  column, whatever rows the range spans.
- **FR-012d**: The selection MUST be made by dragging in either column,
  extended with Shift and a click or the arrow keys, and moved with the
  keyboard alone.
- **FR-012j**: The hex view's two columns MUST each be a tab stop inside
  the one widget: Tab into the view reaches the hex column, Tab again the
  characters, and a third Tab leaves for the next control, with Shift+Tab
  running the same stops backwards and entering at the last of them. A
  user MUST be able to change which column they are working in without
  the mouse.
- **FR-012e**: The hex preview MUST group its bytes one, two, four or
  eight at a time at the user's choice, keep the selection across a
  change of grouping, and persist the choice.
- **FR-012f**: The selection MUST be copyable, as hex digits when the user
  is working in the hex column and as the characters the text column shows
  when they are working in that one. There MUST be one Copy command --
  reached through the toolbar, the context menu on the selection, and Ctrl+C --
  and not a pair of copy-as-hex and copy-as-text commands: the column the
  caret is in already says which form is meant. Select all MUST follow the
  same rule, taking every byte and meaning the column the user is in.
- **FR-012g**: The hex preview MUST offer go to offset, which scrolls to
  that offset and puts the caret there.
- **FR-012h**: The text column MUST read a byte as Apple text, ignoring
  the high bit, and show a period for a byte that is printable neither
  way.
- **FR-012i**: The hex view MUST serve any run of bytes, not a previewed
  file alone: it MUST read them through a supplied source rather than
  hold a copy, address them from a supplied origin rather than from zero,
  take a per-byte mark from its host for bytes worth showing differently,
  and stay responsive over a run as large as a machine's whole memory
  with a source whose bytes change under it. The emulator's debugger is
  the second caller and MUST need no change to the widget to use it.

**Operations**

- **FR-013**: Every verb of the command-line disk tool MUST be available
  from context menus and produce byte-identical results: list, get, put,
  delete, boot, create, init, sector read and write, block read and
  write, with the text and BASIC conversions.
- **FR-013a**: Rename MUST be offered on files in both DOS 3.3 and ProDOS
  images and on host files, from the context menu and F2, applying the
  target file system's name rules and refusing a collision.
- **FR-013b**: Every file verb of the command-line disk tool MUST take a
  path into a ProDOS image's directories. A path without a leading slash
  MUST be read from the volume directory (`GAMES/CHESS`); one with a leading
  slash MUST be a full ProDOS path beginning with the volume's name
  (`/MYDISK/GAMES/CHESS`), refused when that name is not the image's. A
  DOS 3.3 path MUST stay one name, slashes included. A missing directory
  along a path MUST be refused with a message naming it. Where a bare
  `/word` names an option, it MUST be read as the option; a path of two or
  more parts, or one without a leading slash, reaches the entry.
- **FR-013c**: `disk list <image> [<dir>]` MUST list one directory, and
  with `--recurse` (also `-r` and `-s`) that directory and everything below
  it and nothing above, one full path per row.
- **FR-013d**: `disk mkdir <image> <path>` (also `md`) MUST create a ProDOS
  directory and any missing directories along the path, as cmd's `mkdir`
  does: `mkdir A/B/C` creates A and B where absent, then C. A path that
  already exists in full MUST be refused.
  On a DOS 3.3 image it MUST be refused, since DOS 3.3 has no directories.
- **FR-013e**: `disk rmdir <image> <path>` (also `rd`) MUST follow
  PowerShell's `Remove-Item` meanings: without `--recurse` (`-r`, `-s`) a
  directory with contents MUST be refused with a message naming the flag;
  with it the whole subtree MUST go; `--force` MUST also remove locked
  entries, which are otherwise refused. Before deleting a subtree it MUST
  list every entry that would go, mark the locked ones, total the blocks
  freed, and ask for confirmation, which `--yes` (`-y`) skips; with no
  terminal to answer and no `--yes` it MUST refuse rather than wait. The
  whole plan MUST be made before anything changes and applied all or
  nothing: a locked entry without `--force`, or a directory that cannot be
  fully read, refuses the entire delete. The volume directory MUST NOT be
  removable. Cassque's delete of a directory MUST use the same plan, and
  its confirmation MUST show the same list and totals.
- **FR-013f**: `disk delete` MUST remove files only and refuse a directory,
  pointing to `rmdir`.
- **FR-013g**: ProDOS names MUST match without regard to case. Where an
  entry carries GS/OS's lowercase flags, Cassque and `disk list` MUST show
  the name in that case. A name created or renamed MUST be stored in
  uppercase, with lowercase flags keeping the case the user typed.
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
- **FR-017a**: A setting in the Options dialog, Host file names: Descriptive, CiderPress or
  AppleSingle, default Descriptive, MUST switch what Cassque writes for raw
  copies to the CiderPress `NAME#TTAAAA` form or to an AppleSingle file
  `NAME.as`. Reading MUST accept every form and bare names regardless of
  the setting.
- **FR-017b**: A ProDOS directory dragged out MUST become a host folder
  recursively; a host folder dragged onto a ProDOS image MUST become a
  directory recursively; a folder dropped on a DOS 3.3 image MUST be
  refused with a message, since DOS 3.3 is flat.
- **FR-017c**: A host file that is an AppleSingle container MUST put as
  the file it holds, with no dialog: the data fork as the contents, the
  real name as the catalog name (sanitized when it is not legal), and the
  ProDOS type and aux type, mapped to a DOS 3.3 type and load address on a
  DOS 3.3 image. It MUST NOT be listed or opened as a disk image. Get and
  drag-out in the AppleSingle style MUST write `NAME.as` carrying the name,
  type, aux type and dates. A right-drag out of an image MUST open a menu
  offering Descriptive, CiderPress and AppleSingle for that drop. The
  preview of an AppleSingle host file MUST show the name, type, aux type
  and dates of the file it holds.
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

- **FR-025**: Cassque MUST support multiple tabs, each with its own
  location, history, selection, sort, and preview scroll and
  disassembly toggle, with Ctrl+T, Ctrl+W and Ctrl+Tab, and Alt+Left and
  Alt+Right for back and forward. Preview-pane visibility is one setting
  for the window, not per tab. Open tabs are restored on the next launch.
- **FR-026**: Every operation MUST be reachable from the keyboard, and
  every context menu MUST open from the application key and Shift+F10.
- **FR-027**: The toolbar's Theme dropdown MUST offer Light, Dark, Follow system, and
  Casso's three themes, with the skeuomorphic entry marked as colors
  only; Follow system MUST track the Windows setting live.
- **FR-028**: Theme choice, preview-pane state, host file naming and
  window placement MUST persist in Cassque's own preferences file beside
  Casso's, since Casso's file is rewritten whole by a running Casso and
  would lose them. The theme key MUST be separate from Casso's; on
  first run it MUST be seeded from Casso's current theme mapped onto
  Cassque's palette for it, after which the two are independent.
- **FR-029**: Every control MUST carry an accessible name and role.
  Announcement to assistive technology is out of scope; the UI library's
  automation provider is a follow-on feature.

**Native chrome**

- **FR-031**: The Windows light and dark themes MUST read as native at a
  glance beside File Explorer at the same scale: the text faces and
  sizes, the list's background and its edge grays, the column headers,
  the hover the pointer leaves on a row, and the tree's expand and
  collapse chevrons. Casso's three themes MAY depart in palette, and in
  nothing else.
- **FR-032**: A splitter MUST draw as a hairline and take the pointer
  over a band far wider than that line, as Explorer's do. The grab MUST
  NOT shrink to the drawn width.
- **FR-033**: Back, Forward, Up and Refresh MUST carry the iconography
  Explorer uses for them.
- **FR-034**: The preview toggle MUST sit at the toolbar's trailing end,
  where Explorer keeps the control that shows and hides its pane.
- **FR-035**: A long name MUST be cut off, never wrapped, in a tree row,
  a list cell or a tab.

**Tabs and the address bar**

- **FR-036**: The tab strip MUST stay usable past the point where the
  tabs no longer fit: every open tab MUST remain reachable without
  resizing the window.
- **FR-037**: Tabs MUST be reorderable by dragging, and a new tab MUST be
  openable from the strip itself, the way a browser and Explorer open
  one, rather than from a toolbar button alone.
- **FR-038**: The window MUST carry an address bar: the current location
  as navigable segments, editable into a typed path, and reaching a
  location inside a disk image as readily as one on the host.
- **FR-039**: The chrome arrangement MUST be the library's choice, not
  the window's: a window MUST be able to put its tab strip outside the
  toolbar and address bar, as a browser does, or inside them, as a window
  with one toolbar does. Cassque MUST take the browser's arrangement.
- **FR-040**: The address bar MUST remember the last ten paths typed or
  pasted into it, most recent first, and offer them from a dropdown. A
  path entered again MUST move to the top rather than appear twice.
  A path MUST be remembered only once it navigates successfully; one that
  cannot be reached MUST NOT be added. Reaching a place by browsing MUST
  NOT add it. The list MUST persist.

**Toolbar and options**

- **FR-041**: Cassque MUST have no menu bar. A command bar duplicating
  File Explorer's MUST take its place: its buttons, their order, icons,
  labels, spacing and the overflow of buttons that no longer fit, with
  every departure below and none other.
- **FR-042**: The command bar MUST offer New, Cut, Copy, Paste, Rename and
  Delete as buttons with Explorer's icons, acting on the file list's
  selection, and MUST NOT offer Share, New tab, Close tab or Exit. Rename
  MUST apply the name rules of the file system holding the item: DOS 3.3's
  or ProDOS's inside an image, the host's outside one (FR-013a). Help and
  About MUST live in the See more menu.
- **FR-046**: New MUST offer only what the listed location can hold. A host
  folder MUST offer New folder and a new disk image of every container
  Cassque can create. A ProDOS image or a directory in one MUST offer New
  folder, making a ProDOS subdirectory. A DOS 3.3 image, which has no
  folders, MUST show New disabled. A new item MUST take an unused default
  name and open for renaming at once, as Explorer's does. A new disk image
  made while host files are selected MUST receive copies of them, by Put's
  conversion rules: selected folders become ProDOS directories, a DOS 3.3
  image refuses folders with a message rather than flattening them, and a
  selected disk image goes in as a file, not unpacked. A selection too
  large for the container MUST be refused before the image is created.
- **FR-047**: The command bar MUST end in a See more menu. Buttons that no
  longer fit MUST move into it as the window narrows and come back as it
  widens. An item MUST be able to live in See more permanently, never
  shown on the bar whatever the room.
- **FR-048**: Tabs MUST take File Explorer's form: its tab shape, a fixed
  width that shrinks toward Explorer's minimum as tabs are added before the
  strip scrolls, the label left-aligned and cut off, the location's icon
  before it and a close button after it. A + button MUST follow the tabs and
  the scroll arrows and open a new tab.
- **FR-049**: The chrome MUST run, top to bottom: the tab strip; a row
  holding the navigation toolbar (Back, Forward, Up, Refresh) at the left
  and the address bar filling the rest; the full-width command bar of
  FR-041. The window keeps the system title bar.
- **FR-043**: The command bar MUST offer Sort and View dropdowns acting
  on the file list: Sort by any column, ascending or descending; View
  choosing how the list draws its entries. View MUST end with Options,
  which opens the Options dialog. Explorer's Details pane button MUST be
  replaced by Cassque's Preview toggle, and a Theme dropdown MUST follow
  it.
- **FR-044**: The file list MUST offer Explorer's eight views: Extra large
  icons, Large icons, Medium icons, Small icons, List, Details, Tiles and
  Content, each keeping the selection, keyboard navigation, drag and
  context menus that Details has. The choice MUST persist.
- **FR-045**: An Options dialog MUST hold Cassque's settings, Host file
  names among them. Commands the preview pane's context menu offers MUST
  NOT also appear at the top level of the window.

**Testability**

- **FR-030**: Browsing, cataloging, preview decoding, conversion, the
  drag data object's contents, and the Casso-targeting decision MUST be
  driven by unit tests with no window, no real file system and no running
  emulator.
- **FR-030a**: The hex preview's own decisions -- which bytes a point
  selects, which cells a selection lights in each column, what a
  regrouping does to it, and what a copy yields -- MUST be driven by unit
  tests with no window.

### Key Entities

- **Known folder**: a host folder the user has opened a disk from; path,
  last-used time; persisted, shared with Casso.
- **Location**: what a tab is looking at: a host folder, a disk image, or
  a directory inside an image.
- **Catalog entry**: one file in an image: name, type, size, address or
  aux type, locked, modified; plus the file system it came from.
- **Preview**: a rendering of a catalog entry by kind: listing, text,
  picture, hex, catalog.
- **Byte selection**: an offset into the previewed bytes and a count,
  drawn in both columns of a hex preview and carried across a change of
  grouping.
- **Drag payload**: the dragged entries with their source image, and the
  conversions the host side would need.
- **Casso target**: the instance an insert goes to: launcher, a running
  instance, or a new one; with its machine's drive count.
- **Tab**: a location, its history, selection, sort, and the preview's
  scroll position and disassembly toggle.
- **Typed path history**: the last ten paths typed or pasted into the
  address bar, most recent first, with no duplicates; persisted.
- **List view mode**: how the file list draws its entries, one of the
  views FR-044 names; persisted.

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
- **SC-007**: A range selected in either column of a hex preview covers
  exactly the same bytes in the other column, at every grouping width,
  including a range that starts and ends mid-row.
- **SC-009**: Captures of Cassque and File Explorer at the same scale,
  set side by side, differ in no element of the chrome a reviewer can
  name -- text, row metrics, headers, hover, chevrons, splitters, edges
  -- outside the palette of Casso's three themes.
- **SC-010**: With twenty tabs open in a window 900 pixels wide, every
  tab can be reached and every tab can be reordered.
- **SC-011**: A user can reach any folder on the host, and any directory
  inside an image, by typing its path into the address bar.
- **SC-012**: Captures of Cassque's command bar and File Explorer's at the
  same scale, set side by side, differ only in the departures FR-042 and
  FR-043 name.
- **SC-013**: After eleven paths are typed, the address bar offers the
  latest ten, newest first; typing the oldest of them again puts it at the
  top with ten entries still listed. A path that fails to navigate leaves
  the list unchanged.
- **SC-008a**: The hex view shows a 64 KB run, addressed from an origin
  of the host's choosing, with no copy of those bytes held by the widget
  and no pause a user can see when scrolling through it.
- **SC-008**: A hex row, a disassembly line and a text file's columns all
  line up down the pane at every window width and scale.

## Assumptions

- The command widgets, meaning command, dropdown, toolbar, menu bar and
  context menu, are delivered by the preceding feature and are not
  re-specified here. Cassque uses all of them but the menu bar.
- Widgets this feature adds to the UI library: lazy expansion, stable ids
  and hidden checkboxes on the existing tree (which already recurses to
  any depth), multi-select on the list, a splitter, a status bar, a
  framebuffer view, drag-out support beside the existing drop-in
  support, a hex view, and light and dark palettes. Each is general, not
  Cassque-only. The tab strip exists already.
- Supported image formats are the ones the command-line tool supports
  today. The one container added is AppleSingle, which holds a single file
  rather than a volume.
- Conversion rules on get, put and drag are the command-line tool's rules,
  plus an Integer BASIC detokenizer this feature adds for preview and Get.
  The tree detokenizes Applesoft only today. No Integer BASIC tokenizer:
  an Integer listing dragged in is put as text.
- Preview and Get show what a decoder can decode and mark where it
  stopped, since the Apple saves programs without validating them and a
  type A or I file may hold anything.
- Rename is added to both disk file systems as an in-place catalog edit;
  it is never emulated by copy and delete.
- Executable detection is not attempted; hex dump is the default for
  binaries and the graphics rule is the only exception.
- No Apple II face ships. The character generator table in the tree is
  substantially Apple's ROM -- 47 of its 64 glyphs match bit for bit --
  so a font built from it would redistribute those shapes, and the
  seventeen that differ mean it would not even be faithful. A face read
  from the user's own video ROM at runtime redistributes nothing and is
  a feature of its own, not part of this one.
- The hex preview is a widget the UI library gains, general rather than
  Cassque-only: the emulator's debugger wants the same view of memory,
  which is why the bytes arrive through a source and an origin instead of
  a buffer the widget owns. Editing bytes in place is not part of this
  feature; the debugger's write path is a follow-on, and the widget's
  selection is the ground it will stand on.
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
