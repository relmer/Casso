# Feature Specification: Casso Explorer

**Feature Branch**: `033-cassque`

**Created**: 2026-09-10

**Status**: Draft

**Input**: User description: "Casso Explorer: a GUI file browser for Apple II
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

Casso Explorer is the window onto all of that. It is a second executable beside
Casso, sharing its core library and its preferences, and it does for disk
images what File Explorer does for folders: browse, inspect, preview, copy,
delete, create, and hand a disk to the emulator. Its name is the
cassowary's casque, and the About dialog says so with a picture of the
bird.

The command surfaces it is built on, one command model, one dropdown, one
toolbar, are delivered by the preceding feature. This one adds the widgets
the browser layout needs and the browser itself.

Casso Explorer began with moving files between a real Apple //c and the PC
with ADTPro over a null modem cable. In ADTPro the Apple starts every
exchange and the PC only replies; only whole disks or raw blocks move,
never single files; and neither end's interface is pleasant to use. The
CassoLink turns that around: a connected Apple's drives are a root in
Casso Explorer's tree, and files and whole disks move either way, started
from the PC. The same link joins Casso's emulated machines to the PC, so
the Apple side is written, debugged and tested in Casso before it runs on
hardware.

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
  system? → A: Descriptive suffixes Casso Explorer writes, since catalog names
  carry no extension: converted files as `NAME.Applesoft BASIC.txt`,
  `NAME.Integer BASIC.txt`, `NAME.Text.txt`; raw copies as
  `NAME.Binary.$AAAA.bin` for binary on either file system,
  `NAME.ProDOS.$TT.bin` with `.$AAAA` before `.bin` when aux is nonzero
  for other ProDOS types, and `NAME.DOS.X.bin` for other DOS 3.3 types.
  The trailing `.txt` or `.bin` gives Windows a real extension for file
  associations. On the way in Casso Explorer understands these, the
  CiderPress `#TTAAAA` form, and no suffix at all (content rule), and
  strips the suffix from the catalog name. A setting, Host file names:
  Descriptive or CiderPress, default Descriptive, chooses what Casso Explorer
  writes. Only a raw copy round-trips byte for byte; a converted listing
  is a different file from the tokenized original.
- Q: Is the theme choice shared with Casso or Casso Explorer's own? → A:
  Casso Explorer's own, stored beside Casso's preferences in Casso Explorer's own file.
  On first run it starts from Casso's current theme mapped onto Casso Explorer's
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
  Casso Explorer shows that refusal. Casso Explorer's own writes to a mounted image go
  with the reload intent and land in Casso's existing external-change
  policy, which never discards guest writes.

### Session 2026-09-14

- Q: Does Casso Explorer understand AppleSingle files? → A: Yes. An AppleSingle
  file (magic $00051600) is one Apple II file carrying its data, real
  name, dates, and ProDOS type and aux type inside it. It is a file, not a
  disk image, and Casso Explorer never lists or opens one as a volume. Put
  unwraps it with its type and aux type; Get can write one; its preview
  shows the file it holds; Casso does nothing with one. The codec is
  `AppleSingleCodec` in CassoEmuCore, shared with 035-debugger's BLOAD.
- Q: How is the host-name style chosen on the way out? → A: The Host file
  names setting offers AppleSingle beside Descriptive and CiderPress as the
  default, and a right-drag out of an image opens a menu choosing the
  style for that drop.
- Q: Does Casso Explorer keep a menu bar? → A: No. File Explorer's command bar
  replaces it, duplicated as closely as possible. New tab, Close tab and
  Exit are gone from it, since the tab strip and the window already offer
  them. Of Edit, only Cut, Copy, Paste and Delete survive, as buttons with
  Explorer's icons. Casso Explorer adds a Theme dropdown, as Casso's toolbar has,
  and keeps only its Preview toggle where Explorer offers Details and
  Preview, because Casso Explorer's file list already shows details.
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

### Session 2026-10-13

- Q: Are Apple listings and text files drawn in the Apple II character set?
  → A: No. They are text in the theme's monospaced face, as other text
  previews are. A font made at run time from the video ROM, and a CRT pass
  over the text before that, were both built and dropped.
- Q: How do a disk text file and an exported Apple file preview? → A: As
  lines of text: a text file inside a disk image, and a host file Casso
  Explorer exported, with a file name ending in `.Applesoft BASIC.txt`,
  `.Integer BASIC.txt` or `.Text.txt`, even when it is plain ASCII.
- Q: Where is the picture monitor chosen? → A: On the options dialog's
  Display page alone; the command bar has no monitor drop-down.

### Session 2026-10-14

- Q: Is capturing copy-protected disks part of CassoLink? → A: No.
  A follow-on feature, built on this link, reads raw nibbles or bit streams
  and writes WOZ images.
- Q: Which machines and transports does the link cover? → A: The //c's
  built-in serial ports; Casso's emulated machines, over TCP on the same PC;
  a serial Wi-Fi modem on a //c, connected over TCP; and, for a ][+ or //e, a
  Super Serial Card, an Uthernet II and audio bootstrap. Casso's emulation
  of the two cards, which the owner lacks, is two features of their own that
  land in Casso first and are merged here. The //c has no slots and no cassette
  port, so on a //c the link is serial only.
- Q: ADTPro's protocol or a new one? → A: ADTPro's, so an existing ADTPro
  setup works against Casso, extended for file and disk operations the PC
  starts.
- Q: How much does the Apple side do on its own? → A: It is a server with a
  status screen; an Apple-side menu is a later feature. It is a thin shim
  that loads the current program from the PC at every launch, so a Casso
  release updates the Apple side with no step by the user.
- Q: Is the link usable from the command line too? → A: No. It is used
  from Casso Explorer only; there is no `casso link` command.
- Q: Which drives on the Apple does the link use? → A: 5.25-inch floppy
  drives, with DOS 3.3 and ProDOS disks. ProDOS block devices such as a
  UniDisk 3.5 or a hard drive are a later feature.
- Q: Which PC folder does an ADTPro client's image list and transfers use?
  → A: The folder open in Casso Explorer's current tab.
- Q: Does the link's TCP port accept connections from other machines? → A:
  Only while the Wi-Fi modem transport is chosen; otherwise it accepts only
  programs on the same PC.
- Q: How does Casso Explorer connect to an Apple, and does its root stay in
  the tree? → A: The link is set up once and listed as a root that connects
  when opened and lists its state when no Apple replies; Casso Explorer also
  finds a running link on any free COM port by itself.
- Q: Does writing an image to a real disk write .nib and .woz tracks exactly?
  → A: No. Only standard sectors are written; raw-track writes are part of
  the protected-disk follow-on.

### Session 2026-10-16

- Q: What is on the Apple's screen while the link runs? → A: The shim displays a
  title and its state: waiting for the PC on port 2 at its speed, loading
  the program with a percentage, and starting the program's version. The
  program's status screen adds whether the PC is connected, or how long since
  its last packet; the last error, until the next operation succeeds; and a
  short label of the user's operation, which the PC sends.
- Q: Is a link to an emulated Apple set up in the dialog? → A: No. Each
  running Casso whose serial port is connected to a TCP port writes that
  port where Casso Explorer finds it, and Casso Explorer lists each such
  machine as a link root with no dialog. A machine connected through port 1
  rather than port 2 is listed with that reason.
- Q: Which speeds does the link start at? → A: The shim starts at the link's
  speed, 115,200 baud by default, for an emulated machine and for a link boot
  disk. The 9,600-baud bootstrap speed is only for the loader typed in
  through the Apple's serial input.
- Q: Which settings are in the link dialog? → A: Only those that apply
  to the chosen transport: a COM port, bootstrap speed and handshaking for
  serial; an address for TCP and an emulated machine, defaulting to
  127.0.0.1:6502 for an emulated machine; a port and handshaking for a Wi-Fi
  modem.
- Q: At which speeds is each port opened in the COM port search? → A: The
  bootstrap speed and the link's speed.
- Q: How does the user find the address a Wi-Fi modem dials? → A: The
  address is listed under the link's root, as the PC's name with .local and
  the port.

### Session 2026-10-18

- Q: Does a new disk image take the selected files? → A: No. New > Disk
  image creates an empty image in the listed folder whatever is selected,
  as File Explorer's New does. This replaces the 2026-09-14 answer, under
  which the selected host files were copied onto the new image.
- Q: Does put on the command line take a file's type and entry from the
  suffixes Casso Explorer adds to a host file?
  → A: Yes. With no `--as`, a host name in one of Casso Explorer's forms,
  such as `HELLO.Applesoft BASIC.txt`, `PROG.Binary.$6000.bin` or
  `PROG#062000`, is stored under the part before the suffix, with the type,
  load address and conversion in the suffix unless `--type`, `--load`,
  `--text` or `--basic` is given. Any other host name is stored as written,
  so `MEMO.txt` is stored as `MEMO.TXT`.

### Session 2026-10-19

- Q: Which buttons do the Delete and Format confirmations have? → A: Delete
  and Cancel, and Format and Cancel, not Yes and No.
- Q: How are all of a disk image's files copied out at once? → A: Extract
  all files, on a disk image's menu in the list and in the tree, copies every
  file into a new folder beside the image, called after the image without its
  extension, with each ProDOS directory as a folder inside it. The files take
  the host file style the Options dialog sets, as a Copy does. When that
  folder exists, the files go into `name (2)`, `name (3)` and so on.
- Q: What is on the Apple-side program's screen besides the status? → A: Below
  the status, a history: one line per operation the user ran, and one when
  the PC connects, the newest at the top and older lines moving down. Each
  line starts with the PC's time, HH:MM, since an Apple may have no clock,
  then OK or ERR and the operation's label.
- Q: How is a drive that is not attached distinguished from an empty one? → A:
  The program reads the drive's data for about a quarter second at start.
  A drive reads nibbles from a disk, and noise with none, so its data
  changes; with no drive at the //c's port it reads `` throughout, as
  measured on a //c. A drive that is not attached is left out of the drive
  map, and Casso Explorer lists only the drives in the map.
- Q: Can a disk in an Apple's drive be formatted, or made bootable? → A:
  Format disk, on an Apple drive's menu in the tree, opens the format dialog:
  DOS 3.3 or ProDOS, its volume, and Bootable, which copies the stock system
  files as for an image. The disk is built as an image on the PC and written
  to the drive as Write image to disk writes one, after a confirmation that
  everything on it is erased.

### Session 2026-10-22

- Q: Is there one link boot disk for every interface, or one per link? → A:
  One universal disk, as ADTPro's: it detects a //c's ports 1 and 2, a Super
  Serial Card in any slot by its firmware's identification bytes, and an
  Uthernet II in any other slot but a Disk II's, and waits for the PC on all
  of them at once. The first one the PC connects through is used.
- Q: How does the PC find an Uthernet II booted from the universal disk? →
  A: The disk gets its address by DHCP, or uses the address built into it
  when no DHCP reply comes, and broadcasts an announcement to UDP port 6510
  about every two seconds while it waits: CASSOLINK, a version, its address,
  its port and its MAC address. Casso Explorer listens on that port and
  lists each announcement's Apple as an Uthernet II link root, never saved.
  An announcement from the PC itself is listed at 127.0.0.1, where Casso's
  emulated card listens.
- Q: What does a Wi-Fi modem link's disk do, since the PC cannot connect
  first? → A: It dials the PC at once, on a //c's port 2, where
  the modem is, or on the Super Serial Card in the highest slot.
- Q: Where can the emulated ImageWriter II be attached? → A: To a parallel
  card, a Super Serial Card, or a //c serial port, one connector at a time,
  chosen on Settings > Printing in Connected to, and only there. A parallel
  card with no ports listed has the printer. On a serial connector the
  printer's handshake drops CTS, DSR and DCD together near a full buffer. A
  change of connector applies to the running machine.
- Q: How does a user serve ADTPro's own Apple client? → A: Through a
  CassoLink connection of the type ADTPro-compatible server, added in the
  same Add connection dialog as any other, on a COM port or a TCP port, with
  the folder it shares. There is none until the user adds one. The Options
  dialog's CassoLink page, its option to serve ADTPro's client, and the
  answering of ADTPro's requests on other CassoLink connections are
  removed. The server's help is a virtual item under its root, written to a
  temporary file only when it is opened or dragged out, never to the user's
  Documents folder. A request for a path outside the shared folder's subtree fails.
- Q: Which settings does the Add connection dialog have? → A: The
  transport first, then only the settings it uses. There is no name field:
  the name is made from the settings, such as "Serial (any port)", "COM3",
  "TCP 192.168.1.5:6502", "Wi-Fi modem on port 6400" or "ADTPro server on
  COM3", and a connection identical to an existing one cannot be added. The COM
  port is a drop-down of Any and the ports found. No transport has an
  Apple-side port or slot setting, since the Apple side detects it. The
  bootstrap speed is above the transfer speed, and both it and hardware
  handshake have info tips. No error is displayed for an empty TCP address.
  The Wi-Fi modem transport has a description in the dialog and listens on TCP
  port 6400, which has no field. The Emulated transport is removed: running
  Cassos are listed by themselves, and a Casso on another PC is a TCP
  connection. Saved Emulated connections are dropped.
- Q: What happens when hardware handshake is on and the cable does not
  connect CTS? → A: Many null-modem cables do not connect RTS and CTS, or loop
  each end's back on itself, and a //c's 6551 has CTS tied to ground. A
  connection with handshake on whose CTS never rises within a timeout
  reports that the cable may not connect CTS and suggests turning handshake
  off.
- Q: What does a change to a serial connector on the Settings sheet do? →
  A: It applies to the running machine with no reset. The ImageWriter II is
  not offered in a serial connector's drop-down. On a Super Serial Card it
  sets the card to printer mode and saves the previous mode in the machine's
  configuration. Moving the printer away restores that mode only if the
  card is in printer mode then, and switching the card out of printer mode
  disconnects the printer. Each such change to another tab's setting is
  reported in a message box.
- Q: How does the Uthernet II boot disk pick its address and find the
  card? → A: Its built-in MAC address is salted with the monitor's RNDL and
  RNDH ($4E and $4F) and a hash of $4000-$BFFF. The card is detected by
  reading its mode register first, and its address pointer is written only
  when that read is plausible.
- Q: How are the Settings sheet's tabs scrolled and its info tips closed? → A: When
  the tabs do not fit, the strip scrolls with an arrow at each end, as
  Casso Explorer's does. An info tip is up until a click elsewhere, a
  change of focus, Escape, or a second click on its icon, and Escape
  closes an open info tip before it cancels a dialog.

### Session 2026-10-23

- Q: What does a copy out of a disk image with sectors that cannot be read
  do? → A: It completes, as a salvage copy. What cannot be read is written
  as zeros, and a message lists each file affected and how many sectors
  were lost. A move from such an image does not delete the originals, and
  neither does a move dragged to File Explorer unless every file was read
  whole.
- Q: What happens when a file copied out of an image has the name of a file
  in the host folder? → A: File Explorer's prompt: `Replace`, `Skip` or
  `Keep both`, which numbers the copy. A move deletes neither a file skipped there
  nor one skipped at the load address prompt of a copy into an image.
- Q: What does a copy between images preserve? → A: The type, the address
  or aux type, the locked flag and the dates. A locked directory counts as
  a locked entry in a delete's plan, a folder cannot be copied into itself
  or a folder inside it, and a move does not delete a locked entry.
- Q: How does a virtual drive on the ADTPro-compatible server write to its
  image? → A: Each block in place, at its offset in the file, rather than by
  writing the whole image again.
- Q: Should CassoLink have virtual drives for a ProDOS program on the Apple,
  like ADTPro's VSDrive? → A: Not in this spec, and at low priority.
  VSDrive works against the ADTPro-compatible server today. A CassoLink
  version is a bootable ProDOS link disk plus a resident block driver, and
  is relmer/Casso#178.
- Q: How do several Casso Explorer windows share the typed path history? →
  A: Every window adds to the one list in the preferences, so a path typed
  in one window is offered in all of them.

### Session 2026-10-24

- Q: Which connection opens the announcement listener, given that the
  listener is what lists Uthernet IIs automatically? → A: An Uthernet II
  added with its address left empty, whose name is "Uthernet II (any
  address)". It connects to no Apple; each Apple whose announcement arrives
  is listed as a separate connection. An Uthernet II with an address dials
  that Apple and opens nothing, so no inbound firewall rule applies to it.
  With no such connection saved, nothing listens and no Apple from an
  announcement is listed.
- Q: Which network profiles do the package's firewall rules cover? → A: All
  three, public included, each rule narrow to one protocol and port. Nothing
  binds those ports until the user turns on a network link, and TCP 6400 is
  the only one where a stranger can open a session, which serves nothing the
  PC did not start. Every other TCP transport, the ADTPro-compatible server
  included, dials out.

### Session 2026-10-30

- Q: How does a Full Send start and end? -> A: From the Apple. The folder and image format are set ahead of time on a Full Send page in Casso Explorer's Options, with `Casso Full Send` under Downloads as the default folder. F on the Apple's CassoLink screen starts a session, and Q ends it, discards a partial image and returns to the CassoLink screen. Esc is ignored during a session, as a second press would end CassoLink. No menu or toolbar command on the PC starts one, and Cancel on the PC ends one. F pressed during another operation starts the session when that operation ends.
- Q: How does Casso Explorer detect and install new versions? -> A: Casso and Casso Explorer share one implementation, ShellUpdater, behind a small host interface that each app supplies. Whichever app notices an update first shows it. The schedule, the skipped version and the check record are shared in Casso's prefs file, and only the update fields are written from Casso Explorer. Applying an update while the other app runs stops and asks the user to close the other app; the other app is never closed silently. Casso Explorer has the same title bar indicator and update dialog as Casso, in one open window, plus Help > Check for updates, an automatic check at startup, --updated and --cleanup-old, and the pending update applied at exit.

- Q: What does Home show, and where does it read it? -> A: File Explorer's Home page: Quick access tiles, then Recent, Favorites and Shared tabs over two-line rows. Recent and Quick access come from the Home shell folder; Favorites are the pinned items of the MS Graph Recent File Folder and Shared is the MS Graph Shared File Folder, so no sign-in is needed. Those folder ids are undocumented, so a tab is empty if Windows drops them.

- Q: How does a box of floppies get archived from a //c? -> A: Full Send, a CassoLink session that alternates between the Apple's two drives with no interaction on the PC. Disk II hardware has no usable disk-change signal, so a new disk is recognized by a fingerprint of track 0 and the catalog track. With no new fingerprint in either drive, the session pauses until Space on the Apple, and Space also forces a send, since two freshly formatted disks can match. Each image's name comes from its catalog where the disk has one. Labels are not typed on the Apple, whose keyboard has a one-character latch that a disk read can't poll; the user renames images on the PC afterward. Raw capture of copy-protected disks is a follow-on; the manifest flags those disks.

- Q: When a disk finishes and the other drive's disk is not new, is the drive just read checked before the pause for Space? -> A: Only when its disk changed during the read, so a swap during the transfer continues with no key press. See Session 2026-11-01.

- Q: Does a new Full Send recognize disks an earlier session sent to the same folder? -> A: Yes, from the folder's manifest, which also has each disk's fingerprint. Sequence numbers continue from it.

### Session 2026-10-26

- Q: How is a sector Read disk to image could not read distinguished, in a
  later merge of several reads of one disk, from one read as zeros? → A: Read disk to
  image saves the list of the sectors it wrote as zeros beside the image, as
  NAME.unreadable.txt for an image NAME: lines starting with # are comments,
  and each other line is a sector's track and DOS 3.3 sector in decimal,
  whatever the image's format. A read with every sector removes a list of
  that name from an earlier read, so a list beside an image is always that
  image's. Verified transfers (Session 2026-11-01) add repeated reads and the vote, after https://norsewanderer.com/blog/BlogDSK3.html.
- Q: What happens when an emulated Uthernet II's guest listens on a TCP port
  the PC already has, as when the Super Serial Card's connector is on TCP port
  6502, the port the link boot disk listens on? → A: Casso reports it in an
  infobar over the machine that is up until the port opens or the user
  closes it with the X at its right, and in a warning on Settings' Network and
  Serial card pages: the Super Serial Card or serial port on that port when one
  is, and otherwise another program on the PC. A different problem raises the
  infobar again. Before this the listen failed with no word, and the link over
  the card did not connect either.
- Q: How does a notice strip offer its choices? → A: Every one is a
  DxuiInfoBanner, set up by flags: buttons for the actions it offers (Download,
  Restart, Save as) and an X at its right edge for putting it away, never a
  Dismiss button.
- Q: How do the Apple's screens name a connection? → A: By what it is, then
  its slot or port, then its speed or network port: `SSC, SLOT 2, 115200`,
  `SERIAL PORT 2, 115200` on a //c, `WI-FI MODEM, SLOT 2, 115200` when the
  shim dials a modem, and `UTHERNET II, SLOT 3, 6502`. The universal boot
  disk's waiting screen gives an Uthernet II's address as well, `UTHERNET II,
  SLOT 3, 10.0.65.15:6502`, and while it has none, `GETTING AN ADDRESS`. The
  6551 shim's screens follow in the next answer.
- Q: Where does the 6551 shim find room for its interface's name, and what
  is on its screens and the program's for a link with an address? → A: The
  6551 shim moves to $3B00, where the W5100 shim loads, with its copy of the
  zero page at $3A00 and the slot finder at $3AA0, so both shims have $3B00 to
  $3FFF. The universal boot disk has the 6551 shim at $2C00 and the W5100 shim
  at $3100. The dialing screen has the target, as `DIALING
  CASSO.LOCAL:6400`, in capitals for a ][+. The waiting screen has
  `WAITING FOR THE PC ON:` and, below a blank row, the interface as the
  universal disk lists it, such as `WI-FI MODEM, PORT 2, 115200`. The
  program's status screen has a row below CONNECTION for a link with an
  address: `DIALED:` and the modem's target, or `ADDRESS:` and the Uthernet
  II's address and port, with the PC's row below it; the history then starts
  a row lower and is a row shorter. A boot disk written before the move has its
  shim at $3D00, where the program no longer jumps back to, so it must be
  written again. The typed bootstrap is 105 bytes longer.
- Q: How large may a source file the feature adds or grows be, and how is
  one past that split? → A: About 2,000 lines for a .cpp in most cases.
  A class past it is split into component classes with separate state,
  each with a reference to the class it was split from, as Casso's shell
  components are; spreading one class over several .cpp files does not
  count. Every .cpp
  the feature added past the limit is split: CassoExplorerWindow (17,455
  lines), CassoExplorerBrowser (5,171), CassoExplorerActions (4,217),
  DxuiListView with DxuiListViewItems (6,932 and 3,904; the feature took
  DxuiListView from 3,610), Win32ShellItemVerbs (3,763), DxuiHexView
  (3,131), CassoLinkHost (2,393) and CassoLinkSession (2,374). Files the
  feature grew past it are split where a logical seam exists: ProDosVolume
  (3,048, from 1,763), SettingsPanelState (3,480, from 2,771),
  DxuiTextRenderer (3,011, from 2,265), DxuiPopupMenu (2,997, from 2,134),
  DxuiTabStrip (2,183, from 491) and DxuiTreeView (2,002, from 834).
  CassoExplorerCommands stays, well under the limit. The refactor changes no
  behavior and lands before the owner's final manual passes, with the //c
  walkthrough moved to 2026-10-28, so no pass is repeated after it.

### Session 2026-10-31

- Q: Which rows does Group by offer? -> A: File Explorer's for the folder's type, read from its menus: generic, Documents and Downloads, OneDrive, search results, This PC, Pictures, Music, Videos, Gallery, Recycle Bin, Network, Zip and Home. Any extra column on display is offered too. (None) is offered only while the folder is grouped. More opens Choose Details.
- Q: How are property groups labeled? -> A: As File Explorer labels them: ranges from the property's description, date groups for a date, drive ranges from Windows' table, percentages in tens. A value of several parts lists its item under each part.
- Q: What does an item listed under several groups do when more than one of its rows is selected? -> A: A command acts on it once.
- Q: Does sorting or grouping by a property wait for its values? -> A: No. Rows take their place as values arrive, at most once a second and again when the last arrive, so a slow device or a large folder never stalls the window.
- Q: What groupings do folders open with? -> A: File Explorer's defaults until the user chooses otherwise: Network in Tiles grouped by Category, Downloads by Date modified newest first, This PC by Type.
- Q: Which Search options are there? -> A: File Explorer's: All subfolders or Current folder, Date modified, Kind and Size, System files, Zipped (compressed) folders, File contents, and Change indexed locations. A date, kind or size goes in the search box as a word of its syntax, and choosing it again removes it. Where the index can't answer, these words are tested against each file. File contents has no size limit.
- Q: How does a slow folder such as Network load? -> A: Rows appear as the read finds them, because Network has its devices in seconds and its read runs up to half a minute more. While a folder is read again, its rows stay on screen until the new read finishes. Which devices are listed varies with network discovery.
- Q: Which rows does the background menu of a folder have? -> A: File Explorer's: This PC has Add a network location and Properties, the Recycle Bin has Empty Recycle Bin and Properties, a zip has Extract All, Libraries has New. Each then has the installed apps' commands and the row for more options. On a folder not on the disk, a command that is also listed with Network as its item is omitted, which omits Beyond Compare as File Explorer's menus do. This rule is inferred from those menus.
- Q: What happens when a shell command faults? -> A: The command fails with a message, and the window stays open. Commands from the command store are initialized from their registry key, as File Explorer does, and have a site and a selection when invoked.
- Q: How does a search result's Folder and Path read? -> A: As the shell displays them, so Recent reads Recent Items. An item inside a zip has its path.
- Q: Does a typed path open a zip? -> A: Yes, and a typed shell name for This PC opens the This PC root.

### Session 2026-11-01

- Q: Can a Full Send's images have a file name prefix, as ADTPro's batch mode does? -> A: Yes. It is typed on the Apple, as ADTPro takes a file name there: P on the entry screen or in a pause opens a line for it, and each session begins with none. It goes before the sequence number exactly as typed, so the user chooses any separator.
- Q: Why was a Full Send slower per disk than ADTPro's batch mode, and what changed? -> A: One drive's disk took 42 track reads against ADTPro's 35: a check of two tracks read twice each before the disk, the catalog track read again after it, and another check of the same drive. A track read whole now counts as one read, and a drive whose disk was just sent is not checked again before the pause, which is 36 reads. The check is required to separate a new disk from a repeat, and the whole-disk read skips its tracks.
- Q: Where is the link to a Full Send's images? -> A: A Full Send row directly under CassoLink, a peer of the connections, in the list and the tree, once the folder exists, because every connection writes to the same folder. It opens the folder.
- Q: Which connections start at launch, and what happens when two use one port? -> A: An ADTPro-compatible server doesn't start by default. Its list has its folder and help, with a floating Start server button over the list, as Bootstrap the Apple has. Other connections start at launch, except one the user disconnected, an any-port serial connection with no port that has connected to an Apple, and a Wi-Fi listener the firewall doesn't allow. On a shared port the most recent choice takes the port: starting or creating a connection disconnects the others on that port, each with its reason and a Connect button. A running transfer is never interrupted.
- Q: How is the Apple's screen arranged? -> A: The status row is merged into each drive's row, as D1: SENDING 13 GAMES DISK, with no leading zeros on screen. The zeros are in image file names only. Each drive has a 35-cell bar, one cell per track, filled in track order, with its track and sector overlaid in inverse video as the bar passes and the percentage at the right edge. A second map row, used once a track has a problem, marks R recovered, U unreliable and X unreadable. The connection and PC rows are merged, and a divider follows the counts. A prompt line follows ON DECK-- only after INSERT A DISK TO SEND NEXT has appeared once for that drive.
- Q: How does a Full Send begin? -> A: F enters Full Send without reading a disk. The screen has an explanation of V, then REMOVE THE CASSOLINK DISK unless this session bootstrapped the Apple, then INSERT DISKS TO BE SENT INTO THE DRIVES, a blank line, and PRESS <SPACE> TO BEGIN. A CassoLink disk left in a drive is sent as any other disk.
- Q: What happens on Space when a drive has a disk already sent? -> A: The disk isn't read and DUP doesn't change. The Apple lists the drives that may already have been sent, with CHECK CASSO EXPLORER FOR DETAILS and PRESS 1 OR 2 TO SEND D1 OR D2 ANYWAY, for those drives only. A disk sent this way is written even when it is an exact copy, and counts as SENT. A repeated drive's row reads DUPE OF and the matching file, with a ?, until it is sent. Casso Explorer's status lists the matching file, and its help has the reason: a fingerprint compares only a few tracks.
- Q: Does the Apple's screen use 80 columns? -> A: The program starts in 40 columns, and on connecting the PC selects 80 where the Apple detects them, unless the connection's Use 40 columns setting is on, for a monitor too fuzzy for 80 or a wrong detection. No boot disk option is needed, as 80 columns never appear before the PC connects.
- Q: How are transfers verified? -> A: Verified transfers apply to Full Send, Create image and Write image. Reads, off by default: three reads per track compared per sector, then up to nine and a strict majority when they disagree. A sector with read errors is accepted at two matching good reads, one good read is saved and listed as unreliable in NAME.unreadable.txt, and none is saved as zeros and listed in the same file. Full Send also counts unreliable sectors in its manifest. Failed reads don't vote, the head moves away and back before a retry, and re-reads return per-sector CRCs unless reads disagree. Writes, off by default: a failed track is rewritten up to twice more, and the whole disk is compared with the image by CRC at the end. Verify automatically when a read fails, on by default, verifies only the failing track. The setting is global, can be overridden per connection, and per transfer by commands on a drive's menu for Create verified image and Write image and verify, and by V on the Apple at any time in Full Send, effective at the next track. The manifest lists a disk verified for only some tracks as partly verified. This replaces the plan for a later multi-read spec.
- Q: How does a copy-protected disk differ from a damaged one? -> A: It can't be known from sectors, so the labels are Likely copy-protected, Likely damaged and Unclear, from whether a file system is recognized, then the failure type, its consistency and its spread. With no recognized file system and whole tracks with no sector found the same way on each read, a disk is likely copy-protected. After about 3 such tracks the rest is read once, with no retries or verification. The partial image and its list are saved, a PROT count and tone are added, a flux tool is suggested, and the session goes on. A clean result doesn't mean an image runs.
- Q: What is the manifest's file? -> A: manifest.fullsend, CSV inside, with a # header with instructions not to edit it, so that it doesn't preview or open in a spreadsheet. A full-send.csv is renamed on first use.
- Q: What does Casso Explorer's tree add for transfers? -> A: An animation on the connection's icon and the receiving folder during any transfer, and a count badge on the Full Send and ADTPro folders for files received but not yet rendered in the list, which survives a restart.
- Q: Are Apple Pascal and CP/M in this spec? -> A: Yes, full read and write in Casso Explorer, on images and over CassoLink, on 140K disks. CP/M has no reliable signature, so detection is heuristic with a manual choice. A fingerprint adds track 3 for a disk with no recognized file system. Booting CP/M in Casso requires a Z80, which is a separate spec.
- Q: Where does a catalog listing indicate its file system? -> A: Every listing starts with a heading line, `CP/M disk`, `Apple Pascal disk`, `ProDOS disk` or `DOS 3.3 disk`, in the preview pane and in `disk list`. DOS 3.2 follows when Casso can open those images.
- Q: How is a catalog listing's heading set apart in the preview pane? -> A: It is drawn in the theme's muted text color above a blank row, from a heading field on the preview rather than from the first line. `disk list` prints the plain text heading.
- Q: What is the format of Apple Pascal and CP/M listings? -> A: What the Apple displays. Pascal is the Filer's Extended list in a single column, with a `< UNUSED >` row for each free run in disk order. CP/M is the layout of `STAT *.*` for user 0, sorted by name, with system files in parentheses and `Bytes Remaining On A:` after it. Each other user follows in a separate block under a `USER n` line with the same columns, and a user 0 with no files prints `File Not Found`. The list view rows are not changed.
- Q: Can Casso create Apple Pascal and CP/M disks? -> A: Yes, with `--format pascal` and `--format cpm` on `disk create` and `disk init`, in the New disk dialog and over the link, in every container. `--volume` is the Pascal volume name, 1 to 7 characters by the rule of the Filer's Zero, with NEWDISK as the default, and is an error for CP/M. A Pascal blank has a directory in blocks 2 to 5, 280 blocks and today's date. A CP/M blank has $E5 in every byte.
- Q: How is a CP/M disk detected? -> A: By heuristics, with no manual step, when no other format matches. A directory area that is all $E5 is an empty CP/M disk, as is one where every slot is free and the used ones are entries ERA erased. An all-zero disk is not recognized. Read as CP/M and `--filesystem cpm` are the fallback for a damaged directory that fails the score.
- Q: How many directory entries does a CP/M disk have? -> A: CP/AM 5.1's disk parameter block, measured on 0007.dsk, has 48 entries and 140 blocks, with blocks 128 to 139 wrapping onto tracks 0 to 2. Writes use slots 0 to 47, and the directory is full at 48, unless a valid entry is past slot 47, which indicates a 64-entry disk. Reads accept valid entries in all 64 slots. Invalid bytes in slots 48 to 63 are not damage when no valid entry is there. Writes never allocate blocks 128 to 139. Listing totals are 128 plus any blocks that entries list past 127.
- Q: What does Casso Explorer indicate for a damaged directory? -> A: The note that `disk list` prints also appears in the preview pane and the status text. Blocks listed only by invalid entries with a user byte of 0 to $1F count as used, since real CP/M reads those as files. Slots with invalid bytes are never reused.
- Q: How does a Pascal or CP/M disk become bootable? -> A: As DOS 3.3 and ProDOS disks do: `--bootable` copies a stock boot disk from the cache, the New disk dialog offers to download it, and no system bytes are in the repository. `--bootable <image>` overrides it. The Pascal copy is blocks 0 and 1 and the SYSTEM files, and an image with no boot loader, SYSTEM.APPLE or SYSTEM.PASCAL is an error. The CP/M copy is tracks 0 to 2 and any user 31 entry whose blocks wrap onto them. CP/M's default is CP/AM 5.1, and `--bootable softcard` or the dialog's CP/M system list selects CP/M 2.23 for a Microsoft SoftCard.
- Q: Is krunch offered? -> A: `disk krunch <image>` and Krunch disk on a Pascal image's menu move files toward the volume start in directory order and combine free space, all or nothing through the staged image. A Pascal put that fails because free space is in pieces has a message that points to krunch. Over the link it is not offered: `LinkedDisk` sends sectors one group at a time, moved data overwrites old locations before the directory changes, and files are unreadable after a failure partway.
- Q: What happens to a file copied between file systems? -> A: Text is converted among Pascal TEXT, DOS T, ProDOS TXT and CP/M text, including Pascal's two-block header and indent codes, the high bit, CR versus CR LF, and the CP/M $1A end. A CP/M file is text with a .TXT name or bytes that are all text. Text is ProDOS type $04 and DOS T, with .TEXT on Pascal and .TXT on CP/M when the name fits. Pascal CODE is ProDOS $02, a CP/M file is ProDOS $00, and on DOS 3.3 both are type S, which is not runnable, rather than type B at 0. A Pascal FOTO is DOS B at $2000. A load address is preserved on DOS 3.3 and ProDOS. A copy to Pascal or CP/M, which have no load addresses, has a warning with the address. A CP/M copy to the host converts text only for a .TXT name, since the choice is made before the file is read.
- Q: Does each `disk` command apply to Apple Pascal and CP/M? -> A: Yes, or it fails with a reason. `list`, `get`, `put`, `delete`, `create`, `init`, `sectorread`, `sectorwrite`, `blockread` and `blockwrite` work on both. `mkdir` and `rmdir` report that the file system has no directories, and `boot` reports that it has no startup setting. `--type` takes Pascal kinds, and on CP/M the attributes RO and SYS. `--user` selects a CP/M user on `put`, `get` and `delete`, and is an error on any other file system. `get --basic` and `--load` report that these file systems have no such thing.
- Q: Which disk does the stock Apple Pascal download fetch? -> A: Apple Pascal 1.3's APPLE1:, part 680-0283-A. APPLE0: has no SYSTEM.APPLE, and APPLE1: boots. A disk made from it boots to the Pascal command line in the emulator, and the real Filer's Extended list sets the listing's format.
- Q: Do other CP/M disks have 48 directory entries too? -> A: Yes. The disk parameter blocks on the stock CP/AM 5.1 and CP/M 2.23 disks both specify 48 entries. CP/AM has 140 blocks and CP/M 2.23 has 128.
- Q: How would krunch work over the link? -> A: The Apple-side program runs the moves and rewrites the directory after each file, so a link failure doesn't stop it partway. A power failure can damage the file being moved. The listing's `unused` and `in largest` totals already report how fragmented free space is, so no analysis command is added.
- Q: What is on the top row of the Apple's screen, and what moves a drive's bar? -> A: The top row is CASSOLINK: and the PC's state. In 80 columns the port and speed are at its right edge, and in 40 columns they are on the row below, where both don't fit. Only a whole-disk transfer moves a bar, as the bar has a cell for each track, and a new connection starts with empty bars.
- Q: How does a catalog fit a narrow preview pane? -> A: A catalog is a table, so its rows don't wrap. A row wider than the pane scrolls sideways under a horizontal scrollbar, by the bar, Shift with the wheel, a horizontal wheel and the left and right arrow keys.
- Q: What is on a drive's row when it has no disk? -> A: EMPTY OR NO RECOGNIZED SECTORS, after an operation on that drive found no address field for eight revolutions in a row, until an operation on it succeeds. A Disk II has no disk sensor, so an empty drive and an unformatted disk look the same. The Full Send screen has the same words.
- Q: Are catalog reads and protocol commands added to the Apple's history? -> A: No. The history has the user's operations only, as a whole-disk read alone sends at least 35 requests and the history has 8 rows.

### Session 2026-11-02

- Q: Which disk image formats have metadata to edit? -> A: Only WOZ, among the formats Casso writes: its INFO chunk and its optional META chunk. .dsk, .do, .po, .nib and .nb2 have no header at all, and .2mg, which Casso does not write, has a creator and a comment. A WOZ image's Properties get a page that edits INFO's write protection, boot sector format, compatible hardware and required RAM, and META's standard keys (title, subtitle, publisher, developer, copyright, version, language, requires_ram, requires_machine, notes, side, side_name, contributor, image_date), saved into the image in place. INFO's creator is set by the program that writes the image and is shown, not edited.
- Q: Which order does an automatic Full Send use for Apple Pascal and CP/M disks? -> A: .po for Apple Pascal, whose blocks are in ProDOS's interleave, and .dsk for CP/M, which applies its own skew on top of DOS order.

### Session 2026-11-03

- Q: Where is a Full Send's file name prefix set? -> A: On the Apple, as ADTPro takes a file name there, not in Casso Explorer's Options. P on the entry screen or in a pause opens a line; the prefix lasts for that session only.
- Q: What is on the Apple's screen for a disk that may already have been sent? -> A: DUPE OF and the matching file, with a ?, as the fingerprint compares only a few tracks. A drive's number sends it anyway, and the image is written with the next sequence number even when it is an exact copy. A disk whose whole-disk hash matches an earlier image, with no key, is written too, and its row adds DUPE OF and that file with no ?.
- Q: Is there a log of what crossed the wire? -> A: Yes: one CSV file under Casso's Logs folder with each connection's activity and every packet in plain English, its rows past 30 days dropped, so a session across midnight stays in one file.
- Q: Do Details columns narrow as well as widen? -> A: Yes, in a folder with no widths the user set; a folder with any opens with them.
- Q: Does View have File Explorer's panes and Show? -> A: Preview pane and Show, yes; the Details pane, which repeats the preview's facts, no. Show's Navigation pane is Casso Explorer's setting; Compact view, Item check boxes, File name extensions and Hidden items are Windows' and change File Explorer's windows too, as its menu does. Check boxes are drawn in Details.
- Q: How do menu icons follow the theme? -> A: File Explorer's icons are read for the theme in use; one with no light file is mapped from the dark one by one palette transform (#E0DFDF to #555, #4CC2FF to #0078D4), and Casso's own art takes the same colors through tokens.
- Q: Does a search's background have Save search? -> A: Yes. It saves a .search-ms file, in Saved Searches unless the user picks another folder, that File Explorer opens as the same search.

### Session 2026-11-04

- Q: How does a search over a folder the index doesn't cover compare with File Explorer's? -> A: Over C:\Windows, a search with File contents off took about 10 seconds in Casso Explorer and about 62 in the Windows search folder File Explorer uses, with the same results. Casso Explorer continues to walk folders and parse the query itself.
- Q: What query syntax works where a folder is walked? -> A: A subset of AQS: AND, implied between terms; OR and NOT in capitals; `-` for NOT; parentheses; quoted phrases; and `*` and `?` wildcards. After a property: `=`, `<>`, `<`, `>`, `<=`, `>=`, `a..b` for a range, `~<` (starts with), `~=` (contains) and `~!` (doesn't contain). An indexed folder gets the query as typed.
- Q: Which properties? -> A: `name:` and `filename:`; `ext:` and `fileext:`; `folder:` and `path:`; `size:` with File Explorer's keywords or a number in KB, MB or GB; `datemodified:`, `datecreated:` and `dateaccessed:` with File Explorer's keywords, a date in the user's format or ISO 8601, comparisons and ranges; `kind:` and `type:`; `attributes:` with hidden, system, readonly or compressed; and `content:`. Any other property is looked up by its display name in the Windows property system and is read only for a file whose other terms match. A property word that isn't recognized is matched as text, and a query that doesn't parse is searched as plain words, as in File Explorer.
- Q: What is left out? -> A: Natural-language queries, keywords in other languages, stemming and fuzzy matching, relevance ranks for walked results, mail and contact properties, and matching a bare word against any property but the name and contents.
- Q: Are regular expressions supported? -> A: Yes, as a term between slashes, such as `/^kb\d+/`, alone or after `name:` or `path:`, matched anywhere in the value and ignoring case. A term with a regular expression is always walked. One that isn't valid is reported in the infobar instead of being searched as text. `content:` doesn't take a regular expression.
- Q: Are contents searched by default? -> A: Yes. File contents is on by default. A bare word or phrase matches when it appears in the name or the contents, word by word, so a file with one word in its name and another in its text matches both. `name:` and `filename:` limit a term to the name, and `content:` to the contents, whatever the option. A file's contents are read only when they can change whether it matches.
- Q: Which files' contents are read, and how? -> A: It depends on the IFilter registered for the extension. The null filter means the file isn't opened. The plain-text filter means its bytes are read directly. Any other filter extracts its text, including the metadata text of images, except for .zip, which the Compressed files option covers. A file with no filter is read when its first 4 KB are ANSI, UTF-8 or UTF-16 text. Filters run in a separate process, so one that crashes or hangs costs that file and not Casso Explorer.
- Q: When do results appear? -> A: As each is found, without waiting for the walk to finish.
- Q: Is the preview pane open with nothing selected? -> A: No. File Explorer's pane reads "Select a file to preview". Casso Explorer's closes when no file is selected and opens again when one is, so an empty pane never takes up space. With View's Preview pane off, it never opens.
- Q: Where is a WOZ image's metadata edited? -> A: In Casso Explorer's Disk properties window, which has an INFO tab and a META tab, opened from a .woz file's menu and from Edit metadata on its preview's toolbar. Properties opens Windows' Properties, as for any file. The preview lists the image's WOZ info and WOZ metadata above the catalog, under those headings, as label and value with each of several values on a line of its own, as an image with no file system lists them. It isn't edited.
- Q: How are the edits saved? -> A: In place: INFO's fields are changed in the file, META is replaced, and the header CRC is computed again. No other byte changes. A WOZ 1 image has no boot sector format, compatible hardware or required RAM, so those fields are disabled. An image whose CRC doesn't match its bytes isn't changed, since saving would hide the damage.
- Q: How are META's lists and illegal characters handled? -> A: In the reference, | separates a list's values, and no value can have a |, a tab or a line break. Developer, and a custom key whose value is a list, have a field for each value; Developer has a blank one after them. Requires is the machine boxes, a family a row. A field with a |, a tab or a line break, typed or pasted, has a message under it about that character, and OK and Apply are disabled until every field can be saved. Every standard key is written, empty when it has no value, and none can be deleted. The Language list has English first and the rest alphabetically. The image date's day is typed or chosen from a calendar, and its time of day chosen from hour, minute and second lists, with AM and PM on a 12-hour clock, in local time; it is saved as RFC 3339 in UTC. Custom keys, the ones the reference doesn't define, are listed with their keys as the file has them, can be renamed, deleted and added, and no two can have the same key.

- Q: What does a link do when a reply arrives damaged? -> A: It sends the request again, as it does for a request the Apple didn't take, up to five times. Every link operation can be repeated. Once the resends run out, the error is that replies from the Apple arrived damaged on every attempt, and the log row has the error code.
- Q: How are a link's recovery paths tested? -> A: With faults injected in Debug builds alone, on emulated machines in the scenario tests and on real hardware from Casso Explorer. A fault from the PC flips or drops a byte the PC sends: over serial, over the network, in the serial bootstrap, or in the cassette audio, where the byte is changed before it is modulated. A fault from the Apple is armed by a Fault command, which the Apple's program has only when it is assembled for a Debug build. It flips, drops or stalls a byte of a packet the Apple sends, reports a sector as unreadable, sends a sector's data changed with a CRC computed for the change, or sends a wrong CRC in reply to Read CRCs. Each injected fault is logged.
- Q: How is a fault armed on real hardware? -> A: From a Debug-only menu in Casso Explorer, for the next operation, or with `--link-faults` at launch, for a list of them. Release builds have neither, and their program has no Fault command.
- Q: What is on the Apple's screen after an operation fails? -> A: The drive's bar stays where it stopped, with FAILED over it, until the next operation on that drive.

### Session 2026-11-08

- Q: Are the file type conversions a separate feature? -> A: No. They are User Story 18 of this feature, on the same branch, after Casso Explorer's merge to master.
- Q: Are the IIgs formats in scope? -> A: Yes: AppleWorks GS, OMF, super hi-res in each form, IIgs icons and IIgs bitmap fonts. A 65816 disassembly is not.
- Q: Is a Pascal codefile's p-code disassembled? -> A: Yes, into p-code mnemonics with their operands, beside the segment and procedure dictionaries.
- Q: Does the list go past the owner's first one? -> A: Yes, to every converter CiderPress II has except the Macintosh formats: Applesoft shape tables, Paintworks, DreamGrafix and `.3201` pictures, IIgs animation files, Fontrix fonts, Apple /// hi-res fonts, Print Shop GS clip art in color, a record length chosen in the preview, and text character sets.
- Q: In what order is the rest built? -> A: The Roadmap at the end of this spec. ShrinkIt is read and written in one story, since one without the other is half a feature. 040-disk-inspector merges into this branch first, for its sector editor. Devices the owner has no real Apple for are tested in other emulators: MAME, AppleWin, KEGS and GSplus.
- Q: Are Gutenberg disks written? -> A: Not yet. A Gutenberg volume is read-only, as in CiderPress II, since its free-space map's format is undocumented. Writing is a later item, after the map is worked out from disks Gutenberg saves in Casso.

### Session 2026-11-09

- Q: Is the rest of the Roadmap specified now? -> A: Yes. Every item not yet a story is specified as Stories 22 to 38, before planning starts. Language servers are cut, as editor support rather than disk management. The emulator work in the Roadmap's Elsewhere list is part of other specs.
- Q: Are CassoLink virtual drives in this spec now? -> A: Yes, as Story 25, which replaces the Session 2026-10-23 answer that left them out.
- Q: When the link drops partway through a block device's read or write, does it resume? -> A: Yes. The partial image and the last verified block are saved beside it, and the next read or write of that device offers to continue from that block.
- Q: Where do Disk list's and Find duplicates' results appear? -> A: In a tab, as a results folder: a row for each disk or file, duplicates grouped by set, with the usual context menu. Save as CSV is on its toolbar.
- Q: Do the collection tools look inside archives? -> A: Yes, at any depth Story 21 opens, with the archive and the image inside it in the row's path.
- Q: How long may a raw capture of a 5.25" disk take on a //c at 115,200 baud? -> A: Under 5 minutes, so a quarter track with no data is read once, and a repeated read is sent only where it differs from the first.

## User Scenarios & Testing *(mandatory)*
### User Story 1 - Browse and inspect a disk (Priority: P1)

A user opens Casso Explorer, expands the Casso root in the tree, sees the folders
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
6. **Given** the preview pane hidden with Alt+P, **When** Casso Explorer is
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
from a disk image into a host folder, in Casso Explorer or in File Explorer, and
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

A user right-clicks a disk image and chooses Insert into Casso drive 1 or
Drive 2, or Open in new Casso. If a Casso is running, the insert goes to
it. If none is, one is launched with the disk in the drive. Drive 2 is
offered only when the machine that would receive the disk has a second
drive. When Casso Explorer was launched from Casso's menu, it talks to that Casso;
when started on its own, it picks a running one or launches one.

**Why this priority**: The round trip is the reason Casso Explorer lives beside
the emulator rather than as a standalone tool.

**Independent Test**: With Casso running, insert into each drive and see
the drive widget change; with Casso closed, Insert launches one with the
disk mounted; Open in new Casso always launches a new instance.

**Acceptance Scenarios**:

1. **Given** Casso Explorer launched from Casso's menu, **When** Insert into
   Drive 1 is chosen, **Then** that Casso instance mounts the disk.
2. **Given** the target machine has one drive, **When** the context menu
   opens, **Then** Insert into Casso drive 2 is disabled.
3. **Given** no Casso running, **When** Insert is chosen, **Then** a Casso
   starts with the disk in that drive.
4. **Given** any state, **When** Open in new Casso is chosen, **Then** a
   new Casso starts with the disk in drive 1, leaving any running one
   untouched.
5. **Given** the disk is mounted in the running Casso, **When** Casso Explorer
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

### User Story 7 - Find anything as File Explorer finds it (Priority: P3)

A user who knows File Explorer reaches the same places from Casso Explorer's
navigation pane, its address bar and its search box: Home, Gallery, OneDrive,
Quick access, cloud drives, This PC, Libraries, Network and Linux, and the
shell's other folders through the address bar's drop-down; and a search
finds files by File Explorer's query syntax, including files inside disk
images.

**Why this priority**: Casso Explorer stands in for File Explorer when
browsing disks; a user who cannot reach a folder the way File Explorer
reaches it goes back to File Explorer for that part of the job.

**Independent Test**: With File Explorer open beside Casso Explorer on the
same machine, the navigation pane's sections match in order and content;
pinning a folder in either shows it in both; the address bar's drop-down
lists the same entries; a query typed in each search box over the same
folder returns the same host files, and Casso Explorer adds the matching
files inside the disk images there.

**Acceptance Scenarios**:

1. **Given** File Explorer's navigation pane shows Home, Gallery, OneDrive,
   Quick access, iCloud Drive, This PC, Libraries, Network and Linux,
   **When** Casso Explorer opens, **Then** its navigation pane shows the
   same sections in the same order, Casso's own above them, each opening
   to what File Explorer shows for it.
2. **Given** a folder in the list, **When** the user chooses Pin to Quick
   access, **Then** it appears under Quick access in both Casso Explorer
   and File Explorer, and Unpin from Quick access removes it from both.
3. **Given** a disk image on a network share, reached by a UNC path or a
   mapped drive, **When** the user opens it, **Then** it lists, previews,
   copies and inserts as one on a local disk does.
4. **Given** the navigation pane, **When** the user right-clicks its empty
   area, **Then** Show This PC, Show Network, Show libraries, Show all
   folders and Expand to current folder are offered, each showing whether
   it is on, and each choice holds after a restart.
5. **Given** any location, **When** the user opens the address bar's
   chevron at its left, **Then** the shell's desktop roots are listed,
   then the folders on the user's desktop, each with its icon, and
   choosing one goes there; the address bar shows the location's own icon.
6. **Given** a folder holding host files and disk images, **When** the user
   types `kind:document modified:this week` or a plain word in the search
   box, **Then** the matching files from the folder and its subfolders
   list with the folder each is in, as File Explorer lists them, and files
   inside the disk images there that match by name are listed too.

### User Story 8 - Disk images belong to Casso on the desktop (Priority: P3)

A user who registers Casso with Windows sees Casso's icons on disk images in
File Explorer, opens one there to boot it in Casso, and reaches Casso
Explorer from Open with. Inside Casso Explorer, each file in an image shows
an icon for its Apple file type, and the navigation pane matches File
Explorer's own options until the user sets one differently.

**Why this priority**: A disk image is a document like any other on the
host; a user who has to start Casso and browse to it every time has a
worse path than double-clicking it, and a file type nobody can see at a
glance makes a long catalog slow to read.

**Independent Test**: Register from Casso; File Explorer shows a Casso icon
on each of the six formats, Open boots the image in Casso, and Open with
lists Casso Explorer. Unregister; the icons and verbs are gone and nothing
Casso wrote is left. Separately, a ProDOS disk with system, BASIC, binary
and text files shows four different icons in every view.

**Acceptance Scenarios**:

1. **Given** Casso is not registered, **When** the user chooses to register
   it, **Then** files ending in .dsk, .do, .po, .woz, .nib and .nb2 show
   Casso's disk image icon in File Explorer, for this user alone, with no
   administrator rights asked for.
2. **Given** Casso is registered, **When** the user opens a disk image from
   File Explorer, **Then** Casso starts with that image in its first drive
   and boots it; **When** the user picks Open with, **Then** Casso Explorer
   is offered and opens the image as a location.
3. **Given** Casso is registered, **When** the user chooses to unregister,
   **Then** every association Casso wrote is removed, and a format another
   program owned before Casso was registered returns to that program.
4. **Given** File Explorer shows libraries and hides Network, **When** Casso
   Explorer opens with no navigation pane option set in it, **Then** its
   pane shows libraries and hides Network; **When** the user turns Network
   on in Casso Explorer, **Then** Network shows in Casso Explorer alone,
   File Explorer is unchanged, and Casso Explorer keeps following File
   Explorer's other four options.
5. **Given** a disk image holding files of several DOS 3.3 and ProDOS
   types, **When** it is listed in any view, **Then** each file shows the
   icon for its type, and a type with no icon of its own shows the generic
   file icon.

### User Story 9 - Pictures on the monitor they were made for (Priority: P3)

A user previewing a picture inside a disk image sees it as Casso would show
it: on a color, green, amber, white or custom monitor, with the same
display effects Casso offers, chosen and tuned on a Display page in Casso
Explorer's options.

**Why this priority**: An Apple II picture was made to be seen on a
monitor, and a green or amber screen changes how it reads; a user who has
chosen a look in Casso expects the same look when browsing the same
pictures, without starting the emulator.

**Independent Test**: Preview a hi-res, a double hi-res and a lo-res
picture; switch the Display page's monitor through each style and see each
preview redrawn in it; change an effect on the Display page and see the
preview follow; restart and find the choice kept. Switch a double hi-res
picture between color and monochrome and see it redrawn each way.

**Acceptance Scenarios**:

1. **Given** a picture is previewed, **When** the user picks green, amber,
   white, color or custom on the options dialog's Display page, **Then**
   the preview is redrawn on that monitor at once, and the choice is kept
   for the next start.
2. **Given** the options dialog is open, **When** the user changes an effect
   on its Display page, **Then** the preview shows the change, with the
   same controls and the same results as Casso's Display settings page.
3. **Given** no monitor has been chosen in Casso Explorer, **When** a
   picture is previewed under a Casso theme, **Then** it uses that theme's
   own default; **When** under Light, Dark or Follow Windows, **Then** it
   uses green with the retro theme's effects.
4. **Given** a double hi-res picture is previewed, **When** the user
   switches it between color and monochrome, **Then** it is redrawn the
   other way, whatever the automatic choice was.
5. **Given** a monitor is chosen in Casso Explorer, **When** Casso's own
   display settings change, **Then** Casso Explorer's are unchanged, and
   the other way around.
### User Story 10 - Previews of everything File Explorer previews (Priority: P3)

A user selecting a Word document, a PDF, a picture or any other file
Windows can preview sees the same preview File Explorer would show, in
Casso Explorer's Preview pane, so there is no reason to switch programs to
glance at a file.

**Why this priority**: Casso Explorer is meant to be usable as a file
manager in its own right; hex in the Preview pane where File Explorer draws
the document reads as broken.

**Independent Test**: Select a .docx, a .pdf, a .png and a .txt file, and
see each previewed as File Explorer previews it; select an Apple file in a
disk image and a disk image itself and see Casso Explorer's own previews;
select a file of a type nothing previews and see its bytes.

**Acceptance Scenarios**:

1. **Given** a file on the host of a type Windows has a preview for,
   **When** it is selected, **Then** that preview is drawn in the Preview
   pane, as in File Explorer's.
2. **Given** an Apple file or a disk image, **When** it is selected,
   **Then** Casso Explorer's own preview is shown, whatever Windows offers.
3. **Given** a file nothing previews, **When** it is selected, **Then** its
   bytes are shown, as now.
4. **Given** a shell preview is showing, **When** the pane is resized,
   hidden, or the selection moves, **Then** the preview follows at once and
   never outlives the selection.

---

### User Story 11 - The menus File Explorer has (Priority: P3)

A user right-clicking a file, a folder, or a node in the navigation tree
finds the same commands, in the same order and with the same icons, as
File Explorer offers for the same item, including what other programs add.

**Why this priority**: A user's habits are built on File Explorer's menus;
a command missing or moved sends them back to File Explorer.

**Independent Test**: Right-click the same file, folder and tree node in
both programs, with the same programs installed, and compare the menus
item for item.

**Acceptance Scenarios**:

1. **Given** a file or folder in the list, **When** it is right-clicked,
   **Then** the menu's first level matches File Explorer's for it: the
   clipboard row, the same commands in the same order, the same icons, and
   the items other programs add.
2. **Given** a node in the tree, **When** it is right-clicked, **Then** the
   menu matches File Explorer's for that node, and the node is neither
   selected nor opened, nor does the tree take the keyboard.
3. **Given** a selection of several items, **When** one of them is
   right-clicked, **Then** the menu applies to the whole selection, as File
   Explorer's does.

---

### User Story 12 - Archives are folders (Priority: P3)

A user browses into a zip, or any archive Windows opens, as into a folder:
in the tree, the list and the address bar; sees what one holds by selecting
it; and copies, drags out or extracts its files.

**Why this priority**: Disk images and their companion files often arrive
in zips; opening each in another program to see what it holds breaks the
flow of finding and inserting a disk.

**Independent Test**: Find a zip in the tree and expand it; open it in the
list; select it and see its contents in the preview; copy a file from it,
drag one out to the desktop, and extract it all.

**Acceptance Scenarios**:

1. **Given** a folder with an archive in it, **When** it is listed, **Then**
   the archive appears in the tree after the folders and can be expanded,
   and in the list it opens as a folder.
2. **Given** an archive is open, **When** the address bar is read, **Then**
   it reads as the folders on the disk around the archive, then the archive
   and any folders inside it.
3. **Given** an archive is selected, **When** the Preview pane is shown,
   **Then** it lists the archive's contents as the list would show them
   opened: the same columns, font, icons and selection.
4. **Given** files inside an archive are selected, in the list or the
   preview, **When** the user copies, drags them out, or chooses Extract All,
   **Then** they arrive where they are pasted or dropped, and a copy stays on
   the clipboard after the archive is closed.

---

### User Story 13 - Drop onto what File Explorer drops onto (Priority: P3)

A user dragging files sees each place they could land lit as they pass
over it, with words under the pointer for what a drop there would do,
and drops onto folders, drives, disk images, archives and programs as in
File Explorer.

**Why this priority**: Drag and drop is how many users move files; a target
that does nothing, or does something unannounced, costs trust.

**Independent Test**: Drag a file over a folder, a drive, a disk image, a
zip, a program and the folder it came from, reading the highlight and the
words at each, and drop on each.

**Acceptance Scenarios**:

1. **Given** a drag over a valid target, **When** the pointer is over it,
   **Then** it is drawn as a selected item other than the focused one, and
   the words under the pointer give what a drop would do: Move to, Copy to or
   Open with, and the target's name.
2. **Given** a drag over an archive, **When** dropped, **Then** the files are
   copied into it.
3. **Given** a drag over a program, **When** dropped, **Then** the program
   starts with each dropped file's full path as an argument.
4. **Given** a drag over the folder the files came from, or over a plain
   file in it, **When** the pointer is there, **Then** it is the no-drop
   pointer, and a drop does nothing.
5. **Given** a press on empty space in any view, **When** the pointer is
   dragged, **Then** a rectangle selects every item it touches, as File
   Explorer's does.

---

### User Story 14 - Files and disks to and from a real Apple II (Priority: P2)

A user connects an Apple II to the PC with a null modem cable, starts the
link on both ends, and finds the Apple's drives as a root in Casso
Explorer's tree. From the PC they browse a disk's catalog, preview a file,
copy files from the Apple to the PC and from the PC onto the Apple's disk,
read a whole disk into an image file, and write an image back onto a disk.

**Why this priority**: Moving files and disks off a real Apple II is why
Casso Explorer exists. Doing it a whole disk at a time, started from the
Apple, is the workflow this feature replaces.

**Independent Test**: With a //c on a null modem cable, list a DOS 3.3 disk
and a ProDOS disk in its drives; copy one file each way; read a disk into a
.dsk and compare it with one ADTPro reads from the same disk; write an image
onto a blank disk and boot it.

**Acceptance Scenarios**:

1. **Given** the link is running on the Apple, **When** Casso Explorer
   connects, **Then** a root for the Apple appears in the tree with one node
   per drive, labeled with the drive and the volume in it.
2. **Given** a drive has a DOS 3.3 or ProDOS disk in it, **When** its node is
   selected, **Then** its catalog is listed as an image's is, and selecting a
   file previews it.
3. **Given** files on the Apple are selected, **When** they are copied or
   dragged to a host folder, **Then** they arrive in the chosen host file-name
   style, as files copied out of an image do.
4. **Given** host files are pasted or dropped onto an Apple drive or a
   directory on one, **When** the copy runs, **Then** they are written onto
   the Apple's disk by the conversion rules of a put into an image.
5. **Given** a drive is selected, **When** Read disk to image is chosen,
   **Then** the whole disk is written to an image file in the chosen format;
   **When** Write image to disk is chosen and confirmed, **Then** every track
   is written, formatting as it goes, and read back to verify.
6. **Given** a transfer is under way, **When** either end is looked at,
   **Then** the PC displays its progress, and the Apple's status screen
   displays the drive, the track or block, the operation and the progress.
7. **Given** an unmodified ADTPro client on the Apple, **When** it sends or
   receives a disk or uses a virtual drive, **Then** Casso serves it as
   ADTPro's own host does.

---

### User Story 15 - Start from a bare Apple and stay current (Priority: P2)

A user with an Apple II and no copy of the link program on any disk starts
it from the PC. Once it is running, each launch loads the current program
from Casso, so a new Casso release updates the Apple side without the user
copying anything.

**Why this priority**: A program that has to be copied to the Apple on a
disk first is a program most users never get running; one that drifts out of
date behind the PC side breaks the link.

**Independent Test**: On a //c with no disk in either drive, bootstrap the
link over serial; save it to a blank disk; boot that disk. Raise the link
program's version in Casso, boot the disk again, and confirm the newer
program runs.

**Acceptance Scenarios**:

1. **Given** an Apple at the BASIC prompt, taking input from its serial port,
   **When** the user chooses Bootstrap in Casso Explorer, **Then** the PC
   types a loader into the Apple and the link program starts.
2. **Given** a ][+ or //e with its cassette input connected to the PC's
   audio output, **When** the user chooses audio bootstrap, **Then** the PC
   sends a loader into the Apple as audio and the link program starts.
3. **Given** the link starts from a disk, **When** it connects, **Then** its
   version is compared with Casso's and, when they differ, the current
   program is loaded from the PC before any transfer.
4. **Given** the link is running, **When** the user chooses Save link to
   disk, **Then** the shim and the current program are written onto the disk
   in a chosen drive, which then boots into the link.

---

### User Story 16 - The link inside Casso (Priority: P2)

A developer runs the Apple side of the link in Casso's emulated machines,
joined to Casso Explorer on the same PC, steps through it in Casso's
debugger, and runs every transfer with no hardware: on an emulated //c's
serial port, and on a ][+ or //e with a Super Serial Card or an Uthernet II
in a slot.

**Why this priority**: The Apple side is new 6502 code. Writing and fixing
it on real hardware alone is slow and cannot be tested automatically, and
the owner has neither card.

**Independent Test**: Boot the link disk in an emulated //c and connect
Casso Explorer to it over TCP on the same PC; copy files both ways, and read and write
whole disks. Repeat on an emulated //e with a Super Serial Card and with an
Uthernet II. Boot ADTPro's own client disk in an emulated //c and send a disk
to Casso over TCP.

**Acceptance Scenarios**:

1. **Given** an emulated //c running the link, **When** Casso Explorer
   connects to it, **Then** every scenario of story 14 works as with a real
   //c.
2. **Given** an emulated ][+ or //e with a Super Serial Card, **When** the
   link runs over the card, **Then** transfers work.
3. **Given** an emulated ][+ or //e with an Uthernet II, **When** the link
   runs over the network, **Then** transfers work, and Casso Explorer
   connects to it at its address.
4. **Given** an emulated serial port offered over TCP, **When** another
   program connects to it, such as ADTPro's host or a second Casso, **Then**
   the serial traffic passes both ways.
5. **Given** a transfer is under way, **When** the developer breaks into
   the debugger and resumes, **Then** the transfer completes, any packets
   dropped during the pause being sent again.
6. **Given** Casso is running a //c whose serial port is connected to a TCP
   port, **When** Casso Explorer starts, **Then** that machine is a root in
   the tree with no setup, and opening it connects; when that Casso exits,
   the root is removed.
7. **Given** the link disk boots in an emulated //c, **When** nothing is yet
   connected, **Then** the shim displays that it is waiting for the PC,
   with the port and speed, and then the program loading.

---
### User Story 17 - Full Send a box of floppies (Priority: P2)

A user sits at a //c with a box of floppies and archives them to the PC.
They press F on the Apple's CassoLink screen, then only feed disks: while one
drive transfers, they put a new disk in the other. Each drive follows the
other as fast as the Apple can read and the link can send.

**Why this priority**: Archiving a collection is a main reason to connect an
Apple to a PC, and starting each disk from the PC makes a user walk between
two machines for every floppy.

**Independent Test**: In an emulated //c, press F with a different
disk in each drive, swap in new disks during each transfer, and include one
duplicate and one disk with an unreadable sector. Compare each image with its
source and read the manifest. Repeat on a real //c with real floppies.

**Acceptance Scenarios**:

1. **Given** a linked Apple with two drives and a disk in drive 1, **When**
   the user presses F and then Space on the Apple, **Then** the disk's image is written to the
   folder set in Options and drive 2 is checked next.
2. **Given** drive 1 is transferring, **When** the user swaps the disk in
   drive 2, **Then** drive 2's disk is sent as soon as drive 1 finishes.
3. **Given** a transfer finishes, **When** the other drive's disk has a new
   fingerprint, **Then** it is sent with no key press.
4. **Given** neither drive has a disk with a new fingerprint, **When** the
   check finishes, **Then** a prompt for a new disk appears on the Apple's
   screen, and both drives are checked again after Space.
5. **Given** a disk whose fingerprint matches one already sent, **When** the
   user presses Space with no other new disk present, **Then** the disk isn't
   read, the prompt lists its drive, and pressing that drive's number sends it.
6. **Given** a sent disk's whole-image hash matches an earlier image in the
   session, **When** the read finishes, **Then** the second image is written too, the
   manifest lists it as a duplicate, and the duplicate tone sounds.
7. **Given** a ProDOS, DOS 3.3 or other disk, **When** its image is written,
   **Then** its file name is the session's sequence number and a name from
   the catalog where one exists, and the sequence number alone otherwise.
8. **Given** a disk with unreadable sectors, **When** its transfer ends,
   **Then** its image and unreadable list are written as by Read disk to
   image, the manifest has its assessment, and the bad
   sectors tone sounds.
9. **Given** a session is running, **When** the user presses Q on the Apple,
   **Then** any partial image is discarded and the CassoLink screen returns, and
   standard operations work. Esc is ignored during a session.
10. **Given** another operation is running, **When** the user presses F,
   **Then** the session starts when that operation ends.

---

### User Story 18 - Read and convert every common Apple II file type (Priority: P2)

Each document, program source, picture and font on a disk is drawn as it
was meant to be read: an AppleWorks letter as
formatted text, a spreadsheet as a grid, Merlin source in its columns, a
super hi-res picture in color, a font as a sheet of its characters. Dragging
the file to a PC folder writes a file a PC program opens: rich text, CSV,
plain text or PNG. Pascal text and Merlin source go back onto a disk from
the PC as files the Apple programs open.

**Why this priority**: These are the files a collection consists of. A hex
dump of an AppleWorks file, or a raw binary of a picture, means opening
another program for every file, and Casso Explorer exists to remove that
step.

**Independent Test**: For each type in FR-156, preview a sample file on a
disk image and compare it with the program that made it, run in Casso. Drag
it out and open the result in a PC program: Word or WordPad for rich text,
Excel for CSV, Notepad for text, Photos for PNG. Put a Pascal text file and
a Merlin source file back onto an image, and open each in Apple Pascal's
editor and in Merlin, run in Casso.

**Acceptance Scenarios**:

1. **Given** an AppleWorks or AppleWorks GS word processor document, or a
   Teach document, **When** it is selected, **Then** its text is drawn with
   its paragraphs, tabs, bold, italic, underline and alignment; **When** it
   is dragged out, **Then** it is written as rich text with the same styles.
2. **Given** a Gutenberg or Magic Window document, **When** it is selected or
   dragged out, **Then** its text is drawn or written as plain text, with
   the program's formatting commands removed.
3. **Given** an AppleWorks database, an AppleWorks spreadsheet or a VisiCalc
   file, **When** it is selected, **Then** it is drawn as a grid of rows and
   columns; **When** it is dragged out, **Then** it is written as CSV with
   one row of the grid on each line.
4. **Given** a random-access text file, **When** it is selected or dragged
   out, **Then** each record is one row, drawn as a grid or written as CSV.
5. **Given** a Pascal textfile, **When** it is selected or dragged out,
   **Then** its text is drawn or written with its indentation expanded;
   **When** a text file from the PC is dropped onto a Pascal volume,
   **Then** it is put as a Pascal textfile that Apple Pascal's editor opens.
6. **Given** a Merlin source file, **When** it is selected, **Then** its
   lines are drawn in label, opcode, operand and comment columns; **When**
   it is dragged out and later dropped back onto an image, **Then** the file
   on the image is identical to the original.
7. **Given** an S-C Assembler, LISA or Business BASIC file, **When** it is
   selected or dragged out, **Then** its listing is drawn or written as text,
   for each version of LISA's tokens.
8. **Given** a Pascal codefile or an OMF file, **When** it is selected or
   dragged out, **Then** its segments are listed with their headers and
   contents.
9. **Given** a super hi-res, 3200-color, IIgs animation, LZ4FH or Print
   Shop picture, a IIgs icon file or an Applesoft shape table, **When** it is selected, **Then** the
   picture or each icon is drawn; **When** it is dragged out, **Then** it is
   written as PNG.
10. **Given** an Apple II or Apple /// hi-res font, a Fontrix font, a Print
    Shop font or a IIgs bitmap font, **When** it is selected, **Then** every character it has is drawn
    on a sheet with the font's name and size; **When** it is dragged out,
    **Then** the sheet is written as PNG.
11. **Given** any of these files, **When** the user right-drags it out,
    **Then** the menu offers the conversion and a raw copy.
12. **Given** a file whose type matches one of these formats but whose
    contents don't decode, **When** it is selected, **Then** the reason is
    drawn above its bytes; **When** it is copied out, **Then** that file is
    skipped with the reason reported, and the others in the copy complete.
13. **Given** a multiple selection, **When** the user picks Convert to
    folder from its context menu, **Then** each file is written to the
    chosen folder with its conversion, and the files that fail are listed
    after the rest are written.

---

### User Story 19 - 800K and hard disk images (Priority: P1)

A user opens an 800K 3.5" ProDOS disk, a 32 MB hard disk image or a Disk
Copy 4.2 image in Casso Explorer and works with it as with a 140K floppy:
browsing, previews, copies in and out, renames, deletes and new folders. They
create a new 800K or hard disk image from the New disk dialog or the
command line.

**Why this priority**: IIgs software, hard disk collections and most
downloads from the archives are larger than 140K, and ShrinkIt disk
archives (Story 21) usually contain 800K disks. Nothing larger than a floppy
opens today.

**Independent Test**: Open an 800K `.po`, a 32 MB `.hdv`, an 800K 2IMG and a
Disk Copy 4.2 image made by CiderPress II. Copy a file into each and out
again, and create a folder. Open each image in CiderPress II and run its
Test; open the 800K image in MAME's //c Plus and catalog it from ProDOS.

**Acceptance Scenarios**:

1. **Given** a ProDOS image of any size up to 32 MB, **When** it is opened,
   **Then** it is browsed and changed as a 140K ProDOS image is.
2. **Given** a 2IMG image, **When** it is opened, **Then** its DOS-order,
   ProDOS-order or nibble data is read by its header, and the header's
   comment, creator and flags are listed in Properties and written back
   as read on every write.
3. **Given** a Disk Copy 4.2 image of 400K or 800K, **When** it is opened,
   **Then** its data checksum is checked, and **When** it is written,
   **Then** the checksum is updated and its tag bytes are written back.
4. **Given** the New disk dialog or `casso disk create`, **When** an 800K or
   hard disk size is picked, **Then** a formatted ProDOS image of that size
   is created.

---

### User Story 20 - Files with resource forks (Priority: P1)

On a IIgs disk, each file with a resource fork is listed with both fork
lengths. The user copies one to another ProDOS disk with both forks, and to
the PC without losing the resource fork.

**Why this priority**: Most IIgs files have a resource fork: AppleWorks GS
and Teach documents, icons, applications. A file read without its fork
breaks, and the Teach decoder in Story 18 reads its styles from the fork.

**Independent Test**: On an 800K IIgs disk from CiderPress II, list a Teach
document, copy it to another ProDOS image and to a PC folder in each host
name style, and put the PC copy back. Compare both forks and the Finder
information with the original, and run CiderPress II's Test on the image.

**Acceptance Scenarios**:

1. **Given** a ProDOS file with a resource fork, **When** its folder is
   listed, **Then** its data fork length, resource fork length and total
   size have columns, as do its full access flags.
2. **Given** such a file, **When** it is copied between ProDOS volumes,
   **Then** both forks and its Finder information are copied.
3. **Given** such a file, **When** it is copied to the PC, **Then** the
   resource fork and Finder information are written with it, and **When**
   that copy is put back, **Then** the file is identical to the original.
4. **Given** such a file, **When** it is copied to a DOS 3.3, Pascal or CP/M
   volume, **Then** only its data fork is written, and a message reports that the
   resource fork was not copied.

---

### User Story 21 - ShrinkIt and Binary II archives (Priority: P1)

A user opens a ShrinkIt archive from a download as a folder, copies files out
of it, opens a disk inside it as a disk image, adds and removes files, and
creates new archives, including one from a disk image. Binary II files, and
archives inside disk images, ZIP files and gzip files, open the same way.

**Why this priority**: ShrinkIt and Binary II are how Apple II software is
distributed, and Story 18's sample files are in a ShrinkIt disk archive. An
archive that opens but can't be changed or made is half a feature.

**Independent Test**: Open CiderPress II's `test-files.sdk` and
`dos-files.do.gz`, copy every file out, and compare them with CiderPress II's
extraction. Create an archive of several files with resource forks, and
an `.sdk` of an 800K disk, and run CiderPress II's Test and NuLib2's test on
each. Open an archive stored on a ProDOS disk inside a ZIP file.

**Acceptance Scenarios**:

1. **Given** a `.shk`, `.bxy`, `.sea` or `.sdk` file, **When** it is opened,
   **Then** its records are listed with name, file type, aux type, dates,
   access and fork lengths, and a disk record opens as a disk image.
2. **Given** an archive, **When** a file is added, deleted or renamed,
   **Then** the archive is written with the change, and **When** a new
   archive is created from files or from a disk image, **Then** it opens in
   CiderPress II with no errors.
3. **Given** a Binary II file, `.bny` or `.bqy`, **When** it is opened,
   **Then** its files are listed and copied as an archive's are, and **When**
   files are added, **Then** it is written as Binary II.
4. **Given** an archive or disk image inside a ZIP file, a gzip file, a disk
   image or another archive, **When** it is opened, **Then** it opens in
   place, at any depth.
5. **Given** an archive whose record fails its CRC, **When** it is opened or
   tested, **Then** that record is reported, and the rest are readable.

---
### User Story 22 - VisiCalc sheets with their values (Priority: P3)

A user selects a VisiCalc file and finds every cell with its value, as
VisiCalc draws it after a recalculation, and drags it out as CSV with those
values.

**Why this priority**: VisiCalc saves each formula without its result, so
Story 18 draws a sheet of formulas. A sheet of formulas is not what its
author read.

**Independent Test**: Load each sample VisiCalc file in VisiCalc, run in
Casso, recalculate it and note every cell. Preview the same file in Casso
Explorer and compare each cell, and compare the CSV a drag out writes.

**Acceptance Scenarios**:

1. **Given** a VisiCalc file, **When** it is selected, **Then** each formula
   cell is drawn with the value VisiCalc computes for it, in the file's
   recalculation order and with its column widths and formats.
2. **Given** a formula that refers to itself through other cells, or one
   VisiCalc evaluates to ERROR or NA, **When** the sheet is drawn, **Then**
   the cell has ERROR or NA as VisiCalc draws it.
3. **Given** a recalculated sheet, **When** it is dragged out, **Then** the
   CSV has each cell's value, and a choice in the preview writes formulas
   instead.

---

### User Story 23 - Block devices on a real Apple (Priority: P2)

A user with a UniDisk 3.5, a //c Plus or a //c's 3.5" port, or a hard drive
on a card, connects the Apple and finds each of those drives in Casso
Explorer's tree beside its 5.25" drives. They browse an 800K disk or a hard
drive's volume, copy files both ways, and read and write whole volumes as
images.

**Why this priority**: IIgs and late Apple II collections are on 3.5" disks
and hard drives, and the link reads and writes only 5.25" drives today. ADTPro reads
any device ProDOS can, so this closes ADTPro's largest lead.

**Independent Test**: The owner has no 3.5" drive or hard drive, so this is
tested in emulators run as the link's target: in Casso once it has a
SmartPort hard disk card, in AppleWin with a hard disk card and a Super
Serial Card on TCP until then, and in MAME, scripted, for a UniDisk 3.5 on a
//c with ROM 4 and for a //c Plus. List each device, copy a file each way,
and read each volume to an image and compare it with the image the emulator
mounted.

**Acceptance Scenarios**:

1. **Given** the link is running on an Apple with ProDOS block devices,
   **When** Casso Explorer connects, **Then** each device is a node under
   the Apple with its slot, unit and size, and the volume on it.
2. **Given** an 800K or hard drive volume on the Apple, **When** it is
   browsed or files are copied either way, **Then** it works as in Story 14 for a 5.25" disk.
3. **Given** a block device, **When** Read disk to image is chosen, **Then**
   every block is read into a `.po`, `.hdv` or 2IMG image; **When** Write
   image to disk is chosen and confirmed, **Then** every block is written
   and read back to verify.
4. **Given** a drive with no disk, or a device that reports an error,
   **When** it is listed or read, **Then** the node or the operation reports
   the device's error in words.
5. **Given** a transfer to or from a block device, **When** the Apple's
   screen is looked at, **Then** it displays the device, the block and the
   progress.

---

### User Story 24 - Capture copy-protected disks (Priority: P2)

A user archiving a box of floppies finds one that Full Send flags as likely
copy-protected. They read it again as a raw capture, and the WOZ image that
results boots in Casso as the floppy boots on the Apple.

**Why this priority**: Full Send flags a protected disk but saves only its
standard sectors, which don't boot. Commercial software is most of a typical
collection, and most of it is protected.

**Independent Test**: On the owner's //c, capture Archon, whose WOZ from
4am's collection is the reference, and boot the capture in Casso. Capture
each protected test disk in an emulated //c in Casso from WOZ images, and
compare each capture's tracks with the source image's.

**Acceptance Scenarios**:

1. **Given** a drive is selected on a linked Apple, **When** Capture raw
   disk is chosen, **Then** every quarter track from 0 to 35 is read as its
   nibble stream with the timing of its sync bytes, and written as a WOZ 2
   image.
2. **Given** a track read several times, **When** the reads differ in the
   same region, **Then** the region is written as WOZ's random bits, and the
   capture's report lists the track.
3. **Given** Full Send flags a disk as likely protected, **When** Capture
   raw on protected disks is on in Options, **Then** that disk is captured
   raw instead of read by sectors.
4. **Given** a capture, **When** it finishes, **Then** the WOZ image's INFO
   chunk lists Casso Explorer as its creator and the drive it was read
   from, and it is edited in the Disk properties window FR-133 sets.

---

### User Story 25 - Virtual drives over CassoLink (Priority: P3)

A user boots a CassoLink ProDOS disk on the Apple and finds disk images from
a folder on the PC as ProDOS volumes, in a slot with no card. Programs on
the Apple read and write them as if they were real drives, and each block
written lands in the image on the PC at once.

**Why this priority**: ADTPro's VSDrive does this today against Casso's
ADTPro-compatible server, with ADTPro's software on the Apple. A CassoLink
version uses the link already running, with nothing of ADTPro's.

**Independent Test**: In an emulated //c in Casso, boot the CassoLink
ProDOS disk made from the user's ProDOS, catalog both virtual drives from
BASIC.SYSTEM, save a file to each, and compare the images on the PC. Repeat
on the owner's //c over serial.

**Acceptance Scenarios**:

1. **Given** the user's ProDOS disk, **When** Make CassoLink ProDOS disk is
   chosen, **Then** a disk is written with the user's ProDOS and the link's
   block driver. No part of ProDOS is shipped with Casso.
2. **Given** that disk boots with the link connected, **When** ProDOS lists
   its volumes, **Then** the two images set in the connection's virtual
   drives are volumes in a free slot.
3. **Given** a program writes a block to a virtual drive, **When** the
   write returns, **Then** the block is in the image on the PC, at its
   offset in the file.
4. **Given** the link drops during a block write, **When** ProDOS gets the
   result, **Then** it is an I/O error, and the image has the block whole
   or not at all.

---

### User Story 26 - Whole disks over cassette audio (Priority: P3)

A user with a ][+ or //e and no serial card connects the Apple's cassette
ports to the PC's audio, and sends a disk from the Apple to the PC, or an
image from the PC onto a disk, as audio.

**Why this priority**: A cassette port is all some Apples have for talking
to a PC. Casso Explorer already starts the link through the cassette input,
and ADTPro moves whole disks that way.

**Independent Test**: The owner's //c has no cassette port, so this is
tested in Casso, on an emulated ][+ and //e with their cassette input fed
from Casso Explorer's audio and their output recorded. Send a 140K disk
each way and compare the images. Serve ADTPro's audio client the same way.

**Acceptance Scenarios**:

1. **Given** the link started over cassette, **When** Read disk to image is
   chosen, **Then** each track is sent from the Apple's cassette output,
   and Casso Explorer decodes the PC's audio input into the image.
2. **Given** a damaged stretch of audio, **When** a track's checksum fails,
   **Then** that track is requested again, as a serial reply is.
3. **Given** an image on the PC, **When** Write image to disk is chosen,
   **Then** each track is played into the Apple's cassette input, written,
   and read back to verify.
4. **Given** ADTPro's audio client on the Apple, **When** it sends or
   receives a disk, **Then** Casso Explorer serves it as ADTPro's host does.

---

### User Story 27 - Files and disks as tape audio (Priority: P3)

A user turns Apple files into tape audio, WAV, MP3 or Ogg, to play into an
Apple's cassette input from any player, and turns a tape recording back into
files. A disk image becomes a recording too, played into the link's
cassette disk write.

**Why this priority**: Tape recordings are part of the earliest Apple II
collections, and an audio file is the only way some users can reach a
machine with a cassette port.

**Independent Test**: Convert a BASIC program and a binary file to each
audio format, load each in an emulated ][+ in Casso from the file, and run
them. Convert a recording with several files back, and compare each file.
Play a disk's recording into the link's cassette write in Casso, and compare
the disk with the image.

**Acceptance Scenarios**:

1. **Given** Applesoft, Integer BASIC or binary files, **When** Convert to
   tape audio is chosen, **Then** one recording is written with each file
   in the monitor's tape format and a gap between them, with a sidecar text
   file that lists each file's start time, load address and the command
   that loads it.
2. **Given** a tape recording in WAV, MP3 or Ogg, **When** it is opened,
   **Then** each file in it is listed as a folder entry, with its checksum
   checked, and copied out as a file.
3. **Given** a disk image, **When** Convert to disk audio is chosen,
   **Then** a recording is written that the link's cassette write in Story
   26 takes as it takes Casso Explorer's live audio.

---

### User Story 28 - More Apples and interfaces (Priority: P3)

A user with a IIgs, a serial card other than a Super Serial Card, or an
Ethernet card other than an Uthernet II starts the link and works as a //c
user does.

**Why this priority**: ADTPro runs on these machines and cards, and the link runs only on the //c's ports, the Super Serial Card and the Uthernet II.

**Independent Test**: The owner doesn't have any of these, so each is tested in an
emulator run as the link's target: the IIgs's ports in KEGS, GSplus or MAME,
whichever connects over TCP; a IIgs with a Super Serial Card and a Laser 128
in MAME, scripted; Pascal-firmware cards in Casso's Super Serial Card, used through its Pascal entry points; the original Uthernet in AppleWin; and the LANceGS in MAME, scripted.
On each, run Story 14's scenarios.

**Acceptance Scenarios**:

1. **Given** a IIgs, **When** the link runs on its modem or printer port,
   **Then** Story 14's scenarios work, with the machine at 1 MHz while a
   5.25" drive is read or written.
2. **Given** a serial card with Pascal 1.1 firmware entry points that the
   link doesn't match by its ID bytes, **When** the link starts, **Then** it
   uses the card through those entry points.
3. **Given** an original Uthernet, or a LANceGS, **When** the link runs over
   it, **Then** transfers work as over an Uthernet II, and its announcement
   is listed.
4. **Given** a IIgs with a Super Serial Card, or a compatible such as a
   Laser 128, **When** the link runs, **Then** Story 14's scenarios work.

---

### User Story 29 - An Apple /// client (Priority: P3)

A user with an Apple /// boots a CassoLink SOS disk and works with the
Apple ///'s drives from Casso Explorer as with an Apple II's.

**Why this priority**: ADTPro has an Apple /// client, and Apple /// software
is the rarest to find on a PC.

**Independent Test**: The owner has no Apple ///, so this is tested in
MAME's Apple ///, scripted, with its serial port on TCP. MAME's Apple ///
socket connection has a reported bug, so it is checked first. Boot the
CassoLink SOS disk made from the user's SOS disk, list the built-in and
external drives, copy files both ways and read and write each disk.

**Acceptance Scenarios**:

1. **Given** the user's SOS boot disk, **When** Make CassoLink SOS disk is
   chosen, **Then** a disk is written with the user's SOS kernel and the
   link's SOS program. No part of SOS is shipped with Casso.
2. **Given** that disk boots, **When** Casso Explorer connects over the
   Apple ///'s serial port, **Then** the Apple /// is a root with its
   built-in drive, its external drives and any block device SOS lists.
3. **Given** an SOS or ProDOS disk on the Apple ///, **When** it is browsed,
   copied or read and written whole, **Then** it works as in Story 14.

---

### User Story 30 - DOS 3.2 and large DOS 3.3 disks (Priority: P3)

A user opens a 13-sector DOS 3.2 disk, from an image or over the link, and a DOS 3.3 disk of 40, 50 or 80 tracks, and works with each as with a 140K DOS 3.3
disk.

**Why this priority**: The earliest Apple II software is on 13-sector disks,
and preservation collections have them as they were made.

**Independent Test**: Open CiderPress II's DOS 3.2 sample images and a 40-
and an 80-track DOS 3.3 image, copy files each way, and run CiderPress II's
Test on each. On the owner's //c, read a 13-sector disk written in Casso to
an image and compare it.

**Acceptance Scenarios**:

1. **Given** a `.d13` image, or a `.nib` or `.woz` image of a 13-sector
   disk, **When** it is opened, **Then** its DOS 3.2 catalog is listed and
   its files are read and written.
2. **Given** a 13-sector disk in a linked Apple's drive, **When** it is
   read, written or browsed, **Then** it works as a 16-sector disk does.
3. **Given** a DOS 3.3 image of 40, 50 or 80 tracks, or of 32 sectors a track, **When** it is opened, **Then** it is
   browsed and changed as a 140K image is.
4. **Given** the New disk dialog, **When** DOS 3.2 is chosen, **Then** a
   13-sector image is created, in `.d13`, `.nib` or `.woz`.

---

### User Story 31 - Partitions and hybrid disks (Priority: P3)

A user opens a CFFA card's image, or the card itself plugged into the PC,
and finds each partition as a volume. A ProDOS hard disk with DOS.MASTER
volumes, an 800K disk split by AmDOS, OzDOS or UniDOS, and a floppy with
DOS 3.3 beside ProDOS or Pascal all open as their volumes.

**Why this priority**: CFFA and similar cards are how most Apple II users
keep their software today, and a hard disk image of one is a set of
volumes, not one.

**Independent Test**: Open sample CFFA, FocusDrive, MicroDrive, DOS.MASTER,
AmDOS, OzDOS, UniDOS, hybrid and Pascal ProFile Manager images, browse each
volume, and copy a file into each and out. Copy a partition out and replace
one, and run CiderPress II's Test. Write a CFFA image to a CF card in a
USB reader and read it back.

**Acceptance Scenarios**:

1. **Given** an image with a partition layout, **When** it is opened,
   **Then** each partition is a node with its number, size and volume, and
   each volume is browsed and changed as an image is.
2. **Given** a partition, **When** Copy partition to image is chosen,
   **Then** its blocks are written as an image; **When** Replace partition
   is chosen with an image no larger than it and confirmed, **Then** its
   blocks are replaced.
3. **Given** a floppy with DOS 3.3 and ProDOS or Pascal on it, **When** it
   is opened, **Then** each file system is a volume, and neither write
   touches the other's blocks.
4. **Given** a CF or SD card in a reader on the PC, **When** Open physical
   card is chosen and Windows grants administrator access, **Then** the
   card opens as an image does; only removable drives are offered, and
   every write is confirmed with the drive's model and size.

---

### User Story 32 - HFS volumes (Priority: P3)

A user opens a IIgs hard disk or 3.5" disk formatted with HFS and works with
it as with a ProDOS volume, both forks of each file included.

**Why this priority**: GS/OS reads and writes HFS, and many IIgs hard disks
and CD-ROMs are formatted with it.

**Independent Test**: Open HFS images made by GS/OS in KEGS or GSplus,
copy files with resource forks each way, create folders, and run CiderPress
II's Test; boot GS/OS on the changed image and open each file.

**Acceptance Scenarios**:

1. **Given** an HFS image of any size the containers in FR-175 allow, **When**
   it is opened, **Then** its folders and files are listed with their Mac
   OS Roman names, types, creators, both fork lengths and dates.
2. **Given** an HFS volume, **When** files are copied in or out, renamed,
   deleted or put in new folders, **Then** both forks and the Finder
   information are written, and GS/OS opens the volume after the change.
3. **Given** a file copied between HFS and ProDOS, **When** it arrives,
   **Then** its type is mapped as GS/OS maps ProDOS types to HFS types and
   creators.
4. **Given** the New disk dialog, **When** HFS is chosen, **Then** a
   formatted HFS image is created at an 800K or hard disk size.

---

### User Story 33 - SOS volumes (Priority: P3)

A user opens an Apple /// disk or ProFile image and works with it as with a
ProDOS volume.

**Why this priority**: Apple /// software is on SOS volumes, and Story 29's
client sends them whole.

**Independent Test**: Open SOS images from MAME's Apple /// software lists
and from CiderPress II's samples, copy files each way, and run CiderPress
II's Test; boot the changed disk in MAME's Apple ///, scripted.

**Acceptance Scenarios**:

1. **Given** an SOS image, **When** it is opened, **Then** its directories
   and files are listed with their SOS file types, and its files are read
   and written.
2. **Given** a file copied between SOS and ProDOS, **When** it arrives,
   **Then** its type and aux type are copied where both file systems define
   them, and a type ProDOS lacks is reported.
3. **Given** the New disk dialog, **When** SOS is chosen, **Then** a
   formatted 140K SOS image is created.

---

### User Story 34 - Gutenberg writes and RDOS (Priority: P3)

A user copies documents onto a Gutenberg disk, and pulls files off an SSI
game's RDOS disk.

**Why this priority**: Gutenberg volumes open read-only today, and RDOS is
the last Apple II file system CiderPress II reads that Casso Explorer
doesn't.

**Independent Test**: Write a document to a Gutenberg disk in Casso
Explorer, open it in Gutenberg run in Casso, and edit and save it. Open SSI
disks with RDOS 2.1 and 3.3 in CiderPress II's samples and compare each file
copied out with CiderPress II's.

**Acceptance Scenarios**:

1. **Given** a Gutenberg volume, **When** a file is copied in, renamed or
   deleted, **Then** the volume's free-space map is updated as Gutenberg
   updates it, and Gutenberg opens the file.
2. **Given** an RDOS disk, **When** it is opened, **Then** its catalog is
   listed and each file is copied out; the volume is read-only.

---

### User Story 35 - A viewer window (Priority: P3)

A user opens a file large, in a separate window, and steps through the
folder with the previous and next arrows, as Windows' Photos steps through a
folder of pictures.

**Why this priority**: The preview pane is narrow. Looking through a disk of
pictures or documents one file at a time is what a viewer is for.

**Independent Test**: Open a picture in the viewer from a disk of pictures,
step forward past a text file and a BASIC program to the end and back to
the start, and save one as PNG.

**Acceptance Scenarios**:

1. **Given** a selected file, **When** Open in viewer is chosen or Enter is
   pressed with Open in viewer set in Options, **Then** a viewer window
   opens with the file's preview at the window's size.
2. **Given** the viewer, **When** previous or next is clicked, or Left or
   Right is pressed, **Then** the folder's previous or next file opens, in
   the list's sort order, folders skipped.
3. **Given** the viewer, **When** Save as is chosen, **Then** the file is
   written with its conversion, or raw, as Get offers.
4. **Given** the file's folder changes while the viewer is open, **When**
   next is pressed, **Then** the next file is taken from the folder as it
   is now.

---

### User Story 36 - Collection tools (Priority: P3)

A user with thousands of disk images finds what they have: a list of every
disk with its checksums, the disks and files duplicated across the
collection, and every file inside the images that contains a word,
detokenized BASIC included.

**Why this priority**: A collection read from shoeboxes and downloads is
full of copies, and the files inside images are invisible to Windows'
search.

**Independent Test**: Run each tool over a folder of a few thousand images
with known duplicates, in several containers, and a known word inside a
BASIC program and a text file, and compare the results with the known
list.

**Acceptance Scenarios**:

1. **Given** a folder, **When** Disk list is chosen, **Then** every disk
   image in its subtree is listed with its container, file system, volume
   name, file count, free space, CRC-32 and SHA-256, in a results tab that
   saves as CSV.
2. **Given** a folder, **When** Find duplicates is chosen, **Then** disks
   with the same volume data are grouped, whatever their containers, and
   files with the same data and type across different disks are grouped.
3. **Given** a search with File contents on, **When** its folder has a disk image, **Then** each file inside it is searched as its conversion
   writes it: BASIC detokenized, Apple text as text.
4. **Given** each tool, **When** it is run from `casso disk`, **Then** it
   writes the same results.

---

### User Story 37 - BASIC language tools (Priority: P3)

A developer puts an Applesoft or Integer BASIC listing from the PC onto a
disk as a program, and renumbers and cross-references a BASIC program from
Casso Explorer or the command line.

**Why this priority**: An Integer BASIC listing put onto a disk is text
today, and renumbering a program on the Apple is slow and breaks easily.

**Independent Test**: Tokenize a listing of each BASIC and compare it with
the program BASIC saves from the same lines in Casso. Renumber a program
with GOTO, GOSUB, ON and THEN targets, run both in Casso, and compare.

**Acceptance Scenarios**:

1. **Given** an Applesoft or Integer BASIC listing, **When** it is put onto
   an image or tokenized from `casso basic`, **Then** the program is the
   one BASIC saves from those lines.
2. **Given** a BASIC program, **When** Renumber is chosen with a start and
   a step, **Then** every line and every line reference is renumbered.
3. **Given** a BASIC program, **When** Cross-reference is chosen, **Then**
   each variable, line reference, CALL, PEEK and POKE address is listed
   with the lines it is on.

---

### User Story 38 - Game data viewers (Priority: P3)

A user selects a Wizardry scenario disk's files and finds its monsters,
items, characters and maze maps, and an Infocom story file's header,
dictionary, objects and text.

**Why this priority**: These two are the game data collectors open most,
and DiskBrowser is the only tool that reads them.

**Independent Test**: Preview the Wizardry I to III scenario disks and
compare each list and map with the game, run in Casso. Preview Infocom
story files of each version and compare the dictionary and text with an
interpreter's.

**Acceptance Scenarios**:

1. **Given** a Wizardry scenario file, **When** it is selected, **Then**
   its monsters, items, characters and each maze level are drawn: lists for
   the first three, and a map with walls, doors and special squares for each
   level.
2. **Given** an Infocom story file, **When** it is selected, **Then** its
   header, abbreviations, dictionary, object tree and decoded strings are
   listed.
3. **Given** either, **When** it is dragged out, **Then** the lists are
   written as text and each map as PNG.

---

### User Story 39 - Volumes read and written by the block (Priority: P2)

A user opens a 32 MB ProDOS volume on a storage card, a hard drive image or
a CassoLink drive, and its catalog appears at once, however full the
volume is. Copying one file onto it writes that file's blocks, not the
volume.

**Why this priority**: Every volume is read whole into memory before
anything is listed, and every write builds a new image of the whole
volume. That costs nothing for a floppy, seconds for a full volume on a
card, minutes over a CassoLink connection, and rules out HFS volumes of a
gigabyte or more.

**Independent Test**: Open a full 32 MB volume on a card, a hard drive
image and a CassoLink drive, and count the blocks read to list the root:
about the directory and bitmap, not the volume. Copy one file to each and
count the blocks written. Repeat on a 1 GB HFS volume.

**Acceptance Scenarios**:

1. **Given** a full 32 MB ProDOS volume on a storage card, **When** it is
   opened, **Then** the root lists after reading its directory and bitmap
   blocks only, and a subdirectory's blocks are read when it is opened.
2. **Given** a file copied to that volume, **When** the copy ends, **Then**
   only the blocks of the file, its directory entry and the bitmap are
   written, in one commit. After a failure before the commit, the volume
   is as it was.
3. **Given** a 1 GB HFS volume, **When** it is opened, **Then** its root
   lists without reading the volume whole.
4. **Given** a DOS 3.3, Pascal or CP/M disk, **When** it is opened,
   **Then** it reads as it does today.

---

### User Story 40 - Block mode in the disk inspector (Priority: P2)

A user opens a ProDOS, HFS, Pascal or SOS block image, an 800K disk, a hard
drive image or a partition on a card in the disk inspector, and steps
through its blocks: each one in hex and text, its structure annotated, the
blocks colored by what uses them, and a block number in an index block, a
directory link or a B-tree link followed by a click.

**Why this priority**: The inspector reads Disk II tracks, so a block image
can't be opened in it at all, and a damaged hard drive image or card has no
tool in Casso to look inside it.

**Independent Test**: Open CiderPress II's 800K ProDOS, 2IMG, HFS and
partitioned images and a CFFA card's partition in the inspector. Step to
the volume directory, an index block and an HFS catalog leaf node, check
the annotations against the volume's catalog, follow a pointer and come
back, and edit a byte and see it written.

**Acceptance Scenarios**:

1. **Given** a block image, **When** it is opened in the inspector,
   **Then** its blocks are listed by number, with a block's 512 bytes in
   hex and text, and a byte can be edited and written as a sector can.
2. **Given** a ProDOS volume, **When** a block is selected, **Then** it is
   annotated as what it is: boot loader, volume or subdirectory header and
   entries, bitmap, seedling, sapling or tree index block, an extended
   file's fork block, or a file's data, with the owning file's path.
3. **Given** an HFS volume, **When** a block is selected, **Then** the boot
   blocks, master directory block and its copy, volume bitmap and B-tree
   header, index, leaf and map nodes are annotated, and the allocation
   blocks are marked where they start.
4. **Given** any block volume, **When** the block map is open, **Then**
   each block is colored as free, a file's data, an index block, a
   directory, the bitmap or boot, and blocks marked used with no file,
   and blocks in two files, each have a separate color.
5. **Given** a block number in an index block, a directory link or a
   B-tree link, **When** it is clicked, **Then** the inspector steps to
   that block, and Back and Forward return as in a browser. Highlight file's
   blocks marks every block of the file under the cursor.
6. **Given** an image with partitions, **When** it is opened, **Then** each
   partition is a band over its blocks: an Apple Partition Map's driver
   descriptor and entries, CFFA slices, MicroDrive and FocusDrive.
7. **Given** a 2MG or Disk Copy 4.2 container, **When** it is opened,
   **Then** its header is annotated, and Disk Copy's checksums and tag
   bytes are listed.
8. **Given** two block images, **When** they are compared, **Then** the
   blocks that differ are listed and stepped through, as Compare does for
   tracks.
9. **Given** a free block with text or code in it, **When** the block map
   is open, **Then** it is marked as leftover data, and a run of such
   blocks can be saved as a file on the PC.
10. **Given** a volume inside another, as the two DOS 3.3 volumes of a
    UniDOS, AmDOS or OzDOS 800K disk, or a ProDOS volume inside a DOS 3.3
    hybrid, **When** a block of it is selected, **Then** the block is also
    addressed in the inner volume's terms, track and sector for DOS 3.3, and
    annotated by the inner volume's structure.

---
### Edge Cases

- **A picture on a monochrome monitor**: green, amber and white show a
  color picture as Casso's monochrome monitor shows it, in shades of the
  one color, and a monochrome double hi-res picture in the one color on
  black.
- **Listings, text and hex**: these previews are not pictures, and are
  drawn in the theme's font and colors whatever monitor is chosen.
- **A text file that is not text**: a disk text file whose bytes are not
  text, such as a random-access file with zeros between its entries, is
  opened in the hex view, as a binary is.
- **Registration is never silent**: installing, starting or updating Casso
  writes no association; only the user's own choice in Casso does.
- **A format another program already owns**: registering takes it for
  Casso and remembers the program it had, so unregistering gives it back.
- **Casso moved or reinstalled after registering**: the associations point
  at the old location; registering again repairs them, and Casso offers to
  when it finds them stale.
- **File Explorer's options change while Casso Explorer is open**: an
  option Casso Explorer still follows changes with them at the next
  refresh of the pane.

- A navigation pane section whose provider is absent (no OneDrive signed
  in, no iCloud, no WSL distribution) does not show, as File Explorer
  leaves it out.
- A shell folder that is not a disk folder (Gallery, Network, a library)
  lists what the shell lists in it; the disk-image verbs are not offered
  on items that are not files.
- A network share that cannot be reached says so in the list, as a folder
  that cannot be read does, rather than hanging the window.
- A search over a folder the index does not cover still finds its files,
  by walking the folders, and says it is searching until done; a search
  can be stopped by clearing the box or leaving the location.
- A search finds nothing: the list says so, as File Explorer's does.

- **A disk image that fails to parse**: it appears in the tree with no
  expand affordance and a tooltip carrying the error; selecting it shows
  the error in the file list area.
- **A write-protected or read-only image**: put, delete and format are
  disabled in its menus, and a drop onto it shows no-drop.
- **The image is open in a running Casso**: writes go through, and Casso
  is told to reload, as story 4 scenario 5 says. If the guest also has
  unwritten changes, Casso keeps both versions and Casso Explorer reports the
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
- **Known folder no longer exists**: one deleted from a drive that is present
  is dropped from Casso's folder list by whichever of Casso and Casso
  Explorer notices first: Casso when it lists folders for a disk picker,
  Casso Explorer when it lists its Casso root, at start and on Refresh. One
  whose drive is not present, or that is on a network share, stays listed and
  grayed, since it is likely to come back, and Remove from Casso works on it
  as on any known folder. A recent disk's folder that is gone is left out of
  the disk picker.
- **Two Casso Explorer windows edit the same image**: last write wins, and the
  other window refreshes on focus; no locking.
- **Casso launched Casso Explorer and then exited**: Insert falls back to the
  standalone behavior.
- **Casso Explorer menu item clicked while Casso Explorer is already open**: the
  existing window launched by that Casso is brought to the front. A
  standalone Casso Explorer is a separate instance from one Casso launched.

- **Shell preview unavailable**: a file whose preview program fails or takes
  too long is previewed as its bytes instead, and Casso Explorer never stops
  responding while one loads.
- **Archive on a network share**: an archive on an unreachable share is
  listed as unavailable, never read on the window's own thread.
- **Archive that cannot be read**: a damaged or password-protected archive
  is reported with the reason in the preview and the list, rather than as
  an empty folder.
- **Drop onto a read-only target**: the pointer over it is the no-drop one.

- **The cable is pulled or the Apple reset mid-transfer**: the transfer
  fails with the reason; no partial host file and no half-written image is
  left on the PC. A disk being written on the Apple is left partly written,
  and the message reports the last track written.
- **A packet arrives damaged**: it is sent again, up to a limit, after which
  the transfer fails with the reason.
- **The disk is changed after its catalog was listed**: the catalog is read
  again before any write, and a different volume is reported rather than
  written over.
- **A disk with unreadable sectors**: Read disk to image lists them and
  offers to write the image with them blank, and then saves their list beside
  the image as NAME.unreadable.txt.
- **An image with non-standard tracks**: Write image to disk fails with the
  reason and lists the tracks that do not decode, since only standard
  sectors are written.
- **An empty drive or a write-protected disk**: the drive's node is listed
  as empty; a write to a protected disk fails with the reason.
- **The COM port is in use by another program**: opening it fails with the
  reason, and nothing else is changed; a search for a running link skips it.
- **An emulated Uthernet II's port already in use on the PC**: Casso reports
  it in an infobar the user can close and on the Settings pages it concerns,
  with the Super Serial Card in the slot that has the port when it is the
  card's connector.
- **Another device on a searched COM port**: a modem or printer on a
  searched port receives only the link's short greeting, and the search
  moves to the next port when no link replies.
- **A running Casso with no link program on it**: its root connects and
  waits, the Apple showing nothing of the link until it boots a link disk;
  the root lists that state. A Casso that exits while connected drops its
  root, and a transfer in progress fails as on a pulled cable.
- **An ADTPro client against Casso**: it is served as ADTPro's host serves
  it; operations the PC starts require Casso's own Apple program, and Casso
  Explorer reports that rather than offering them.
- **Speeds differ at the two ends**: bootstrap runs at the slower bootstrap
  speed, and the link then moves both ends to the chosen speed.
- **No DHCP server on an Uthernet II's network**: the boot disk uses the
  address built into it, and its announcements and screen have that address.
- **A device other than the PC on one of the Apple's serial ports**: bytes
  from it that are not an envelope's start byte are dropped, and the boot
  disk goes on waiting on every interface.
- **Two connectors listing the ImageWriter II, as in a configuration file
  edited in a text editor**: the first in build order has it, the slots in their
  listed order and then the //c's ports, and the others are empty.
- **Two operations at once**: one runs at a time per Apple; others queue,
  and when the running one is canceled the Apple is ready for the next.
- **A disk swapped during its transfer, or not fully seated**: a disk
  with a track not read whole that reads differently on a second read is not sent yet, and a
  disk that changes during a transfer fails that image with no file left on
  the PC, and the session continues.
- **An Apple with one drive**: a Full Send uses that drive alone, and pauses
  until Space after each disk.
- **A Full Send when the link drops**: the session ends with the reason, the
  images already written and the manifest remain on the PC, and the partial
  image is discarded.
- **An AppleWorks file from a later version**: a document from AppleWorks
  4 or 5, with features 3.0 lacks, is drawn with what the 3.0 format
  defines, and the rest is listed as unsupported rather than dropped
  silently.
- **A spreadsheet formula with no stored value**: the cell's formula is
  written as entered, as VisiCalc saves formulas with no results.
- **A CSV field with a comma, a quote or a line break**: the field is quoted,
  and a quote inside it is doubled.
- **A super hi-res picture shorter than a full screen, or a packed one that
  ends early**: the lines read are drawn and the rest is black, and the preview
  reports how many lines are missing.
- **A Pascal text file dropped onto a ProDOS or DOS 3.3 image**: on ProDOS it
  is put as a Pascal textfile, type $03, and on DOS 3.3, which has no such
  type, as plain text.
- **A Merlin source line longer than Merlin accepts**: the put fails
  with the line number.
- **A font with characters missing from its range**: the missing ones are
  left blank on the sheet.
- **A random-access text file on DOS 3.3**: the record length isn't stored,
  so it is taken from the spacing of its entries; a file with one record is
  drawn as sequential text. A length found wrongly is replaced in the
  preview.
- **An animation file**: only its first frame is drawn, and the preview
  reports the number of frames.
- **A shape table with a shape of no vectors**: its cell is left blank,
  with its number.
- **A VisiCalc function the manual doesn't list, from a later VisiCalc**:
  the cell is drawn as ERROR, and the preview lists the function as
  unsupported.
- **A block device that reports a size larger than ProDOS allows**: it is
  listed with its size, and only its first 65,535 blocks are read, with a
  message.
- **A SmartPort unit that is a printer or a modem**: it isn't listed as a
  drive.
- **A resumed transfer whose device has changed**: when the device's volume directory blocks differ from the partial image's, the
  offer to continue is withdrawn and the transfer starts over.
- **A capture of an unprotected disk**: it is written as WOZ as any capture
  is, and its report doesn't list a differing track.
- **A track with no sync bytes at all**: it is written as it read, and the
  report lists it.
- **A virtual drive's image that is opened in Casso Explorer while the
  Apple writes it**: the write is applied, and the open view is refreshed,
  as for any image changed outside the window.
- **Audio input too quiet or clipped**: the level is drawn in red, and the
  operation doesn't start until it is in range or the user starts it
  anyway.
- **A tape recording with no file found**: it opens as an empty folder,
  with a message that no tape header was found.
- **A IIgs with an accelerator, or a //c Plus at 4 MHz**: the link sets
  the machine to its standard speed while a 5.25" drive is read or written,
  where the firmware allows.
- **A 13-sector disk with 16-sector tracks on it**: each track is read in
  the encoding its address fields have.
- **A partition marker that points past the end of the image**: the
  partition is listed as damaged and not opened.
- **A CF card removed while open**: the operation fails with the reason, and
  the card's node is removed.
- **An HFS name with a slash or a colon**: it is copied to the PC with the
  character replaced as FR-017 sets for any host-illegal character.
- **An HFS volume larger than 2 GB**: it opens read-only, with the reason.
- **A Gutenberg volume before the free-space map is worked out**: it stays
  read-only as FR-156 sets.
- **The viewer at the last file when Right is pressed**: the file stays, and
  the next arrow is disabled.
- **Two disks of the same data in different containers, one with a WOZ
  track the other lacks**: they are listed as duplicates by their sector
  data, and the WOZ one is marked as having more.
- **A BASIC line too long for BASIC's input buffer**: tokenizing writes it
  as BASIC would store it, with a warning that BASIC can't enter it.
- **A Wizardry or Infocom file from a version the decoder doesn't decode**: it
  falls to hex, with the reason.
## Requirements *(mandatory)*

### Functional Requirements

**Shell**

- **FR-001**: Casso Explorer MUST ship as a separate executable holding no code,
  with all logic in the core library that Casso and the unit tests link.
- **FR-002**: Casso Explorer MUST be launchable from Casso's menu and standalone.
  A second launch from the same Casso MUST bring that Casso's existing
  Casso Explorer window to the front rather than open another.
- **FR-003**: Casso Explorer MUST have an About dialog that explains the name
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
  1024 bytes. A double hi-res picture is monochrome when its name ends
  .A2FM and color when it ends .A2FC; with neither, it is color when its
  dots repeat every four far more strongly than at nearby spacings, as
  color's four-dot cells make them, and monochrome otherwise.
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
  fully read, refuses the entire delete. A locked directory MUST count as a
  locked entry, as a locked file does. The volume directory MUST NOT be
  removable. Casso Explorer's delete of a directory MUST use the same plan, and
  its confirmation MUST show the same list and totals.
- **FR-013f**: `disk delete` MUST remove files only and refuse a directory,
  pointing to `rmdir`.
- **FR-013g**: ProDOS names MUST match without regard to case. Where an
  entry carries GS/OS's lowercase flags, Casso Explorer and `disk list` MUST show
  the name in that case. A name created or renamed MUST be stored in
  uppercase, with lowercase flags keeping the case the user typed.
- **FR-014**: Operations MUST call the core disk runner directly, never
  spawn the command-line tool.
- **FR-015**: Destructive operations, meaning delete, format and
  overwriting put, MUST confirm before acting.

**Drag and drop**

- **FR-016**: A drag between two Apple disk images MUST copy raw with no
  conversion, preserving type, address, locked flag and dates. A folder
  MUST NOT be copied or moved into itself or into a folder inside it, and a
  move MUST NOT delete a locked entry; the failure is reported.
- **FR-017**: A drag or Get from a disk image to a host folder, in Casso Explorer
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
  AppleSingle, default Descriptive, MUST switch what Casso Explorer writes for raw
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
- **FR-018b**: A copy from a disk image with sectors that cannot be read
  MUST complete, with zeros in place of what could not be read, and MUST
  report each file affected and the number of sectors lost. A copy from a
  directory whose catalog cannot be read whole MUST report that entries may
  be missing.
- **FR-018c**: A move MUST delete its originals only when every item was
  written whole. An item copied from a damaged image, skipped at a prompt or
  at a name clash, or that failed, MUST NOT be deleted. Host originals MUST
  go to the Recycle Bin through the shell, which confirms before deleting a
  file the bin cannot take. A move dragged to File Explorer MUST delete only
  when the target read every file whole.
- **FR-018d**: A copy out of an image into a host folder that has a file of
  the same name MUST offer File Explorer's choices for each such file:
  `Replace`, `Skip` and `Keep both`, which writes the copy under a
  numbered name.
- **FR-019**: A drag of a disk image onto a running Casso window MUST be
  accepted by Casso's existing drop handling.

**Casso integration**

- **FR-020**: A disk image's context menu MUST offer Insert into Casso drive 1,
  Insert into Casso drive 2, and Open in new Casso.
- **FR-021**: Insert into Casso drive 2 MUST be enabled only when the receiving
  machine declares a second drive, determined in-process from the default
  machine's layout when no Casso is running and by asking the running
  Casso otherwise.
- **FR-022**: Insert MUST target the Casso that launched Casso Explorer when
  there is one, otherwise a running Casso, otherwise a newly launched one.
- **FR-023**: Open in new Casso MUST always launch a new instance with the
  disk in drive 1.
- **FR-024**: A write to an image mounted in a running Casso MUST tell
  that Casso to reload the image, and MUST show Casso's answer when it is
  a conflict or a refusal rather than a reload.
- **FR-024a**: Insert into an occupied drive MUST replace the disk without
  a confirmation in Casso Explorer. Casso flushes the outgoing image's unwritten
  changes before the swap; Casso Explorer MUST show Casso's refusal when the
  drive is mid-write or the flush fails.

**Tabs, keyboard, themes**

- **FR-025**: Casso Explorer MUST support multiple tabs, each with its own
  location, history, selection, sort, and preview scroll and
  disassembly toggle, with Ctrl+T, Ctrl+W and Ctrl+Tab, and Alt+Left and
  Alt+Right for back and forward. Preview-pane visibility is one setting
  for the window, not per tab. Open tabs are restored on the next launch.
- **FR-026**: Every operation MUST be reachable from the keyboard, and
  every context menu MUST open from the application key and Shift+F10.
- **FR-027**: The toolbar's Theme dropdown MUST offer Casso's three themes in
  Casso's order, the skeuomorphic entry marked as colors only, then Dark,
  Light and Follow system; Follow system MUST track the Windows setting live.
- **FR-028**: Theme choice, preview-pane state, host file naming and
  window placement MUST persist in Casso Explorer's own preferences file beside
  Casso's, since Casso's file is rewritten whole by a running Casso and
  would lose them. The theme key MUST be separate from Casso's; on
  first run it MUST be seeded from Casso's current theme mapped onto
  Casso Explorer's palette for it, after which the two are independent.
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
  with one toolbar does. Casso Explorer MUST take the browser's arrangement.
- **FR-040**: The address bar MUST remember the last ten paths typed or
  pasted into it, most recent first, and offer them from a dropdown. A
  path entered again MUST move to the top rather than appear twice.
  A path MUST be remembered only once it navigates successfully; one that
  cannot be reached MUST NOT be added. Reaching a place by browsing MUST
  NOT add it. The list MUST persist.

**Toolbar and options**

- **FR-041**: Casso Explorer MUST have no menu bar. A command bar duplicating
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
  Casso Explorer can create. A ProDOS image or a directory in one MUST offer New
  folder, making a ProDOS subdirectory. A DOS 3.3 image, which has no
  folders, MUST show New disabled. A new item MUST take an unused default
  name and open for renaming at once, as Explorer's does. A new disk image
  MUST be created empty, whatever is selected.
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
  replaced by Casso Explorer's Preview toggle, and a Theme dropdown MUST follow
  it.
- **FR-044**: The file list MUST offer Explorer's eight views: Extra large
  icons, Large icons, Medium icons, Small icons, List, Details, Tiles and
  Content, each keeping the selection, keyboard navigation, drag and
  context menus that Details has. The choice MUST persist.
- **FR-045**: An Options dialog MUST hold Casso Explorer's settings, Host file
  names among them. Commands the preview pane's context menu offers MUST
  NOT also appear at the top level of the window.

**Shipping**

- **FR-050**: Every release channel that ships Casso MUST ship Casso
  Explorer with it: the zip archive and the installer package alike, each
  with the runtime libraries it needs. A user who installs Casso MUST have
  Casso Explorer already.
- **FR-051**: The installer MUST offer Casso Explorer as an application of
  its own, startable without Casso, under its own name, its own icon and
  its own tile pictures rather than Casso's, and reachable by typing its
  executable's name.
- **FR-052**: Casso's Browse disks command MUST find the installed
  executable in an installed copy, not only beside a build's output.

**Testability**

- **FR-030**: Browsing, cataloging, preview decoding, conversion, the
  drag data object's contents, and the Casso-targeting decision MUST be
  driven by unit tests with no window, no real file system and no running
  emulator.
- **FR-030a**: The hex preview's own decisions -- which bytes a point
  selects, which cells a selection lights in each column, what a
  regrouping does to it, and what a copy yields -- MUST be driven by unit
  tests with no window.

- **FR-053**: The navigation pane MUST show File Explorer's sections in
  File Explorer's order, below Casso's own root: Home, Gallery, the
  OneDrive root, a divider, Quick access, the cloud provider roots the
  shell lists in its navigation pane, This PC, Libraries, Network and the
  Linux root, each present only where File Explorer shows it, and each
  opening to what File Explorer lists for it.
- **FR-054**: A location MUST be able to be any shell folder, not only a
  folder on a disk: its items list, sort, group and preview as File
  Explorer shows them, and the address bar, tabs, history and the tree
  follow it as they follow a disk folder.
- **FR-055**: Quick access MUST list the pinned folders first, then the
  frequent folders, as File Explorer's does, from the list File Explorer
  keeps; Pin to Quick access and Unpin from Quick access MUST be offered in
  Casso Explorer's menus for a folder, and a change made in either program
  MUST show in the other.
- **FR-056**: Every disk-image verb MUST work on an image reached through a
  network path, by UNC path or by mapped drive letter, as on one on a local
  disk.
- **FR-057**: The navigation pane's empty-area menu MUST offer File
  Explorer's Show This PC, Show Network, Show libraries, Show all folders
  and Expand to current folder; the four shows MUST be checked when on and
  MUST hold across runs.
- **FR-058**: The address bar MUST show the current location's icon at its
  left, and a chevron there MUST drop a list of the shell's desktop roots
  followed by the folders on the user's desktop, each with its icon, as
  File Explorer's does; choosing one MUST go there.
- **FR-059**: The search box MUST take File Explorer's query syntax and
  search the current location and everything below it, answering from the
  Windows search index where the location is indexed and by walking the
  folders where it is not; results MUST list in the file list with the
  folder each is in, sortable and groupable as any listing, and the search
  MUST be able to be stopped.
- **FR-060**: A search MUST also match files inside the disk images in the
  searched location by name, and the files in Casso's own folders when the
  search starts from Casso's root; each such result MUST open, preview and
  copy as it does in its image.
- **FR-061**: Casso MUST offer to register itself, for the current user,
  as the program for .dsk, .do, .po, .woz, .nib and .nb2 files, and to
  unregister; nothing MUST be registered except by that choice, and no
  administrator rights MUST be needed.
- **FR-062**: A registered format MUST show Casso's disk image icon in File
  Explorer, MUST open in Casso with the image in its first drive, and MUST
  list Casso Explorer under Open with.
- **FR-063**: Unregistering MUST remove everything registering wrote and
  MUST give each format back to the program that had it before.
- **FR-064**: Each of the navigation pane's five options MUST follow File
  Explorer's own setting until the user sets it in Casso Explorer, and
  from then MUST keep Casso Explorer's value for that option alone; Casso
  Explorer MUST NOT change File Explorer's settings.
- **FR-065**: A file inside a disk image MUST show an icon for its DOS 3.3
  or ProDOS file type in every view, drawn for Casso, at least for text,
  Applesoft, Integer BASIC, binary, system, directory and relocatable
  files; any other type MUST show the generic file icon.
- **FR-066**: Automated tests that register Casso MUST remove what they
  wrote, leaving the user's associations as they found them.
- **FR-067**: A picture preview MUST be drawn on the chosen monitor --
  color, green, amber, white or custom -- with Casso's display effects,
  drawn as Casso draws them; listing, text, hex and catalog previews MUST
  be unaffected.
- **FR-068**: The monitor MUST be chosen on the options dialog's Display
  page, listing Casso's styles and a custom one; the command bar MUST NOT
  have a monitor drop-down.
- **FR-069**: The options dialog MUST have a Display page holding Casso's
  own Display settings page, with the same controls and the same effect on
  the preview as on Casso's screen.
- **FR-070**: With no monitor chosen in Casso Explorer, the preview MUST use
  the theme's default: each Casso theme's own, and green with the retro
  theme's effects under Light, Dark and Follow Windows.
- **FR-071**: Casso Explorer's monitor and effect choices MUST be kept
  across starts and MUST be its own, neither read from nor written to
  Casso's.
- **FR-072**: A double hi-res picture's preview MUST offer a switch between
  color and monochrome, overriding the automatic choice for that picture
  while it is shown.
- **FR-073**: A file of a type Windows has a preview for MUST be previewed
  in the Preview pane as File Explorer previews it, unless Casso Explorer
  has a preview of its own for it, which MUST take priority for Apple files
  and disk images.
- **FR-074**: A shell preview MUST follow the pane's size, visibility and
  the selection, MUST be shown within the same time File Explorer takes,
  and MUST fall back to the bytes when it fails or does not arrive.
- **FR-075**: The context menu of a file, a folder, a selection, or a tree
  node MUST match File Explorer's for the same item: its clipboard row, the
  same commands in the same order, the same icons, and the commands other
  programs add to its first level.
- **FR-076**: A right-click on a tree node MUST NOT select it, open it or
  move keyboard focus to the tree.
- **FR-077**: Any file the shell opens as a folder MUST be browsable as one
  in the tree, the list and the address bar, read-only unless the shell
  allows writing to it.
- **FR-078**: A selected archive MUST be previewed as its contents, drawn as
  the list would draw them opened; its files MUST support selection,
  select all, copy, drag out, open, and Extract All.
- **FR-079**: Folders, drives, disk images, archives and programs MUST be
  drop targets; while a drag is over one it MUST be drawn as a selected item
  other than the focused one, with words under the pointer for what a drop
  would do.
- **FR-080**: A drop on an archive MUST copy into it; a drop on a program
  MUST start it with each dropped file's full path, quoted, as an argument;
  a drop on the folder the files came from MUST do nothing, under the
  no-drop pointer.
- **FR-081**: A press on empty space in every view, Details included, MUST
  start a selection rectangle that selects every item it touches.
- **FR-082**: A text file inside a disk image whose bytes are text, and a
  host file Casso Explorer exported, whose file name ends in
  `.Applesoft BASIC.txt`, `.Integer BASIC.txt` or `.Text.txt`, MUST be
  previewed as lines of text in the theme's monospaced face, selectable
  and copyable as other text previews are, the exported file even when it
  is plain ASCII.

**CassoLink**

- **FR-083**: Casso Explorer MUST list a connected Apple II as a root in the
  tree, with a node per 5.25-inch drive the Apple has, labeled with the
  drive and the volume in it. A link MUST be set up once, with its port or
  address and speed, and listed as a root across runs; it MUST connect when
  opened, and with no Apple replying MUST list its state and the reason in
  place of drives.
- **FR-083a**: Casso Explorer MUST find a running link by itself, by opening
  each COM port that no other program has open, only when an Apple root with
  no port set is opened or when the user chooses to search; it MUST NOT open
  ports in the background, and MUST stop at the first port where a link
  replies. Each port MUST be opened at the bootstrap speed and at the link's speed.
- **FR-083b**: Casso Explorer MUST list each running Casso whose emulated
  serial port is connected to a TCP port as a link root, with no setup, and
  MUST remove the root when that Casso exits. Each running Casso MUST record
  its machine, which serial port is connected and the TCP address where
  Casso Explorer reads them. A machine connected through a //c's port 1 or a
  Super Serial Card's slot MUST be listed and usable, its shim given that
  port or slot.
- **FR-083c**: The Add connection dialog MUST begin with the transport, and
  then contain only the settings that apply to it. It MUST NOT have a name
  field: the connection's name MUST be made from its settings, and a
  connection identical to an existing one MUST NOT be added. The COM port MUST
  be a drop-down of Any and the ports found. No transport MUST have an
  Apple-side port or slot setting. The bootstrap speed MUST come above the
  transfer speed, and it and hardware handshake MUST each have an info tip,
  the handshake's recommending a first attempt without it. No error MUST be
  displayed for an empty TCP address. The Wi-Fi modem transport MUST have a
  description in the dialog and MUST listen on TCP port 6400, with no port
  field; a port already in use MUST be reported on its root. A Wi-Fi modem
  link's root MUST list the address the boot disk dials: the PC's name with
  .local, and the port. There MUST be no Emulated transport, and saved
  Emulated connections MUST be dropped.
- **FR-083d**: A serial connection with hardware handshake on whose CTS does
  not rise within a timeout MUST report that the cable may not connect CTS
  and suggest turning handshake off.
- **FR-084**: An Apple drive MUST be browsable as an image is: DOS 3.3 and
  ProDOS catalogs, ProDOS directories, previews and Properties.
- **FR-085**: Files MUST copy from an Apple drive to the host, and from the
  host onto an Apple drive, by copy and paste and by drag and drop, in the
  host file-name styles and by the conversion rules of a get or put on an
  image. Delete, rename and new folder MUST work on an Apple drive as on an
  image of the same file system.
- **FR-086**: Read disk to image MUST read a whole Apple disk's standard
  sectors and write them to an image file in any format Casso writes, a
  .nib or .woz being built from those sectors. An image written with
  unreadable sectors as zeros MUST have their list saved beside it as
  NAME.unreadable.txt, each sector a line of its track and DOS 3.3 sector in
  decimal after comment lines starting with #, and a read with every sector
  MUST remove a list of that name. Write image to disk MUST
  write every track's standard sectors, formatting as it goes, after a
  confirmation that identifies the drive and the volume, and MUST read the
  disk back to verify it. A .dsk, .do or .po is written as it is, and a .nib
  or .woz by its standard sectors; an image with tracks that do not decode
  to standard sectors MUST fail with the reason. Format disk MUST write a
  new DOS 3.3 or ProDOS disk to an Apple drive in the same way, bootable
  when Bootable is checked, with the stock system files.
- **FR-087**: Every packet MUST include a checksum and every page of data a
  CRC; a damaged packet MUST be sent again, up to five times. A transfer that
  cannot complete MUST fail with the reason, and MUST NOT result in a partial
  host file or a half-written image on the PC.
- **FR-088**: Casso MUST serve ADTPro's published protocol, so that an
  unmodified ADTPro client sends and receives disks, and uses virtual
  drives, against Casso as against ADTPro's own host, through a CassoLink
  connection of the type ADTPro-compatible server, on a COM port or a TCP
  port. The image files it lists, sends and receives MUST be in the folder
  the connection shares, and a request for a path outside that folder's
  subtree MUST fail. Other CassoLink connections MUST NOT answer ADTPro's requests.
  The server's help MUST be a virtual item under its root, written to a
  temporary file only when it is opened or dragged out, and nothing MUST be
  written to the user's Documents folder for it. A virtual drive's block
  write MUST change only that block of the image file.
- **FR-089**: Casso's own Apple-side program MUST use ADTPro's framing,
  extended with operations the PC starts: list drives, read a catalog, read
  and write a file, delete, rename, make a directory, read and write a disk
  by track and sector or by block, and report status. Since the Apple starts
  every exchange in that framing, the Apple-side program MUST poll the PC for
  the next operation.
- **FR-090**: The Apple-side program MUST display a status screen with the
  connection, named by what it is, its slot or port and its speed or network
  port (Session 2026-10-26), and for a Wi-Fi modem or an Uthernet II the
  target dialed or the card's address and port; the drive, the track or block, the operation and its progress;
  whether the PC is connected, or how long since its last packet; the last
  error, until the next operation succeeds; and a short label of the user's
  operation, sent by the PC. The shim MUST display a title and its state:
  dialing a Wi-Fi modem, with the target; waiting for the PC, with the
  interface and speed; loading the program, with a percentage; and starting
  the program's version. Below the status, the
  program's screen MUST have a history of the user's operations and the PC's
  connections, newest at the top, each with the PC's time.
- **FR-091**: The Apple-side program MUST be a resident shim that, at each
  launch, compares its version with Casso's and loads the current program
  from the PC when they differ. Casso MUST offer to write the current shim
  and program onto a disk in an Apple drive, which then boots into the link.
- **FR-091a**: The link boot disk MUST be one disk for every interface: it
  MUST detect a //c's ports 1 and 2, a Super Serial Card in any slot by its
  firmware's identification bytes, and an Uthernet II in any other slot but
  a Disk II's, list each on its screen, and wait for the PC on all of them
  at once, using the first one the PC connects through. A disk made for a
  Wi-Fi modem link MUST dial the PC at once on a //c's port 2, or on the
  Super Serial Card in the highest slot. With
  no interface found, the disk MUST draw NO SERIAL PORT OR CARD FOUND.
- **FR-091b**: On an Uthernet II the boot disk MUST get its address by DHCP,
  using the address built into the disk when no DHCP reply comes, and MUST
  broadcast an announcement to UDP port 6510 about every two seconds while it
  waits: CASSOLINK, a version, its address, its TCP port and its MAC address.
  Casso Explorer MUST listen on that port and list each announcement's Apple
  as an Uthernet II link root, never saved, at the announcement's address, or at
  127.0.0.1 for an announcement from the PC itself. When an emulated
  Uthernet II cannot open on the PC the TCP port its guest listens on, Casso
  MUST report it in an infobar that is up until the port opens or the user
  closes it, and on Settings' Network and Serial card pages, and MUST
  identify the machine's serial connector when that connector is on the port.
- **FR-091c**: The boot disk's built-in MAC address MUST be salted with the
  monitor's RNDL and RNDH ($4E and $4F) and a hash of $4000-$BFFF. The disk
  MUST detect an Uthernet II by reading its mode register first, and MUST
  write its address pointer only when that read is plausible.
- **FR-092**: Casso MUST start the link on an Apple with no copy of it: over
  serial, by typing a loader into the Apple's serial input, on a //c and on
  a ][+ or //e with a Super Serial Card; and on a ][+ or //e, by playing a
  loader into the cassette input. The typed loader MUST take its port or
  slot from the input hook that IN# set, and the cassette loader MUST find
  a Super Serial Card by its firmware's identification bytes.
- **FR-093**: The link MUST run over a //c's built-in serial ports to a PC
  COM port or USB-serial adapter; over a Super Serial Card; over an
  Uthernet II; over a serial Wi-Fi modem connected over TCP; and between
  Casso's emulated machines and Casso Explorer over TCP on the same PC.
  Within one process, as in the automated tests, the emulated machine and
  the link MUST also connect directly, with no port.
- **FR-094**: Serial speed and handshaking MUST be settable, defaulting to
  ADTPro's: 115,200 baud, no hardware handshake, and 9,600 baud for
  bootstrap. The bootstrap speed applies only to the loader typed in over
  serial; a shim on a link boot disk or in an emulated machine MUST start at
  the link's speed. Handshaking applies only to serial and the Wi-Fi modem.
- **FR-095**: An emulated serial port, the //c's or a Super Serial Card's,
  MUST be connectable to a TCP port, which Casso Explorer's link or another
  program connects to, or to a PC COM port. Bytes MUST arrive at the emulated machine no faster than
  the speed set in its serial chip, so a program that runs at the speed of
  real hardware drops none.
- **FR-095a**: The emulated ImageWriter II MUST be attachable to a parallel
  card, a Super Serial Card, or a //c serial port, one connector at a time,
  chosen on Settings > Printing in Connected to, and choosing it on one
  connector MUST empty the connector that had it. A parallel card with no
  ports listed MUST have the printer. On a serial connector the printer's
  handshake MUST drop CTS, DSR and DCD together near a full buffer and raise
  them once it has room. A change of connector MUST apply to the running
  machine. The printer MUST be chosen only on the Printing tab, and MUST NOT
  be offered in a serial connector's drop-down. Chosen on a Super Serial
  Card, it MUST set the card to printer mode and save the previous mode in
  the machine's configuration; moved away, it MUST restore that mode only if
  the card is in printer mode then; and switching the card out of printer
  mode MUST disconnect it. Each such change to another tab's setting MUST be
  reported in a message box.
- **FR-095b**: A change to any serial connector's attachment on the Settings
  sheet MUST apply to the running machine with no reset. When the sheet's
  tabs do not fit, its tab strip MUST scroll with an arrow at each end, as
  Casso Explorer's does. An info tip MUST be up until a click elsewhere, a
  change of focus, Escape, or a second click on its icon, and Escape MUST
  close an open info tip before it cancels a dialog.
- **FR-096**: The Apple-side program MUST be assembled from source in the
  repository by Casso's own assembler, and a test MUST assemble it on every
  build, failing the build on any error. The protocol
  MUST be implemented from ADTPro's published protocol document and from
  observed traffic, never from ADTPro's source.
- **FR-097**: Casso MUST open a COM port or listen on a TCP port only while
  the user has the link started, and MUST release it when the link stops.
  The TCP port MUST accept connections from the same PC only, except while
  the Wi-Fi modem transport is chosen, when it MUST accept them from the
  network.
- **FR-098**: One operation MUST run at a time per Apple; others MUST queue,
  and the running one MUST be cancelable, leaving the Apple-side program
  ready for the next.
- **FR-099**: The installer package MUST declare the inbound firewall rules
  both programs need, each narrow to one protocol and port rather than
  permitting the program as a whole, so that an installed Casso raises no
  Windows Firewall prompt for them.
- **FR-100**: A socket that must bind wider than the loopback address to do
  its job MUST refuse what it is not meant to serve, and MUST say at the bind
  why it is wide. The Uthernet II's address translation binds every port the
  guest opens, since that is how the guest reaches the network at all, and
  refuses unsolicited senders while connections from other computers are not
  allowed. It MUST NOT be narrowed to the loopback address, which would cut
  the Apple off from the network.
- **FR-101**: Casso Explorer MUST NOT open the announcement listener before a
  connection that needs it exists, so a user who never links to an Apple over
  the network never binds a port beyond the loopback address. The connection
  that opens it is an Uthernet II with no address, which lists each Apple
  whose announcement arrives; the listener closes when the last one is
  removed.
- **FR-102**: Both programs MUST determine whether the firewall admits their
  inbound sockets on the network's present category, and MUST determine it
  again when the network changes.
- **FR-103**: A connection whose transport needs to be reached from outside
  the PC, and which the firewall is refusing, MUST carry a warning on its row
  in the tree, a pointer tip naming the cause, and an action in the pane that
  puts it right. A connection that needs nothing inbound, such as one over a
  serial cable, MUST carry neither.
- **FR-104**: Casso MUST show the same warning and the same action on the
  Network settings page, where the setting that widens the binding lives.
- **FR-105**: The warning MUST name the program and every port the firewall
  blocks for it, in one sentence: `Windows Firewall is blocking Casso from
  listening on TCP port 6502 and UDP port 6511.` A connection's pane MUST have
  only the warning while its port is blocked, since none of its instructions
  can work until then. On a public network the warning MUST offer both
  admitting the program there and the alternative of marking the network
  private.
- **FR-106**: A port the package cannot name MUST be admitted when it is
  opened: a port forward's, which the user chooses, and any other port the
  guest opens that the address translation mirrors onto the PC. Only the
  link's own ports are fixed enough to declare.
- **FR-152**: Neither program MUST open a port on the PC's addresses that the
  firewall does not admit on the categories of the networks the PC is
  connected to, so no Windows Firewall prompt appears for it. The port MUST
  wait, with the program's warning and its Allow, and MUST open once the
  port is admitted. An interface with no network identified, such as Hyper-V's
  Default Switch, MUST NOT add its category. The guest's outgoing UDP, from
  the address translation or an Uthernet II socket with LAN access off, MUST
  NOT be deferred: a port the firewall doesn't admit, or one the PC has in
  use, MUST be sent from a socket with no explicit bind, which the first send
  binds to a temporary port, since an explicit bind to the PC's addresses
  raises the prompt and an unbound send doesn't.
- **FR-153**: When the PC has an Uthernet II listener's port in use, such as
  6502 in a second Casso on the PC, the listener MUST move to the next port
  the PC has free, up to the seventh port after the guest's. The guest's
  socket MUST remain on the guest's port. A CassoLink announcement MUST
  contain the port the listener moved to, so Casso Explorer connects to each
  Casso. The card's infobar MUST report the move. Allow MUST admit the whole
  range with one rule, 6502-6509 for the link, so a later Casso that moves
  doesn't require another Allow.
- **FR-154**: An Apple booted from the universal disk MUST go on sending an
  announcement each second while its program listens with no PC connected,
  so Casso Explorer lists it again after the link drops or Casso Explorer
  restarts. Casso Explorer MUST connect to each Apple an announcement lists
  as soon as it is listed, unless the user disconnected it in that session.
  An Apple booted from another link disk MUST NOT send
  announcements. The program's status screen MUST have an Uthernet II's
  connection without its port, as `UTHERNET II, SLOT 3`, and the address and
  port, with no label, below it: at the right edge of the row below the top
  row in 80 columns, and on the row below the connection in 40. This replaces
  the port in the connection's name and the `ADDRESS:` row in the
  clarifications above.
- **FR-155**: An image renamed in Casso Explorer in a folder with a Full Send
  manifest MUST be renamed in the manifest as well: each row with the old
  file name, and each duplicate's reference to it, MUST be set to the new
  name.
  Undoing the rename MUST put the old name back in the manifest. The
  manifest MUST be written under the folder's lock, which a session sending
  to the folder takes, and a manifest that can't be read MUST be left as it
  is. An image deleted in Casso Explorer MUST be removed from the manifest.
  The earliest row with a file that repeats it MUST take its
  place, with its duplicateOf and, for a duplicate, its status, and the
  other rows that repeat it MUST then repeat that row. With no such row, the
  rows that repeat it MUST be removed. Every row MUST be put back, in order
  of their sequence, when the image is restored by Undo or from the Recycle
  Bin in the same Casso Explorer window. The manifest MUST NOT be changed for
  a file Windows didn't delete, as when its confirmation was canceled.

**Full Send**

- **FR-107**: A Full Send MUST start from the Apple. F (either case) on the
  program's CassoLink screen, which MUST show `F: FULL SEND`, MUST enter Full Send
  without reading a disk, and Space MUST begin. The screen MUST explain V, and MUST
  have REMOVE THE CASSOLINK DISK unless this session bootstrapped the Apple.
  Casso Explorer's Options MUST have a Full Send page with the destination folder,
  and the image format: any format Casso writes, defaulting to
  .po for a ProDOS or Apple Pascal disk and .dsk for any other, CP/M included. The default folder MUST be
  `Casso Full Send` under the user's Downloads, made when a session needs it. No
  command on the PC starts a session. F during another operation MUST start the
  session when that operation ends.
- **FR-108**: A Full Send MUST read one drive at a time, and when a disk
  finishes MUST check the other drive first, alternating for as long as the
  session runs. The drive just read MUST be checked as well only when its disk
  changed during the read.
- **FR-109**: A disk's fingerprint MUST be a hash of track 0 and the catalog
  track. A track not read whole MUST read the same on two reads before the disk
  is sent. A disk whose fingerprint is not among those sent to the folder MUST be sent
  with no key press. A drive with no readable disk MUST count as having no
  new disk. A disk with no catalog track that Casso recognizes MUST use tracks 0 and 3
  and the DOS 3.3 catalog track.
- **FR-110**: With no new disk in either drive, the session MUST pause until
  a key on the Apple, without spinning a drive while it waits. The status row MUST read WAITING FOR A DISK during the pause. Space MUST
  check both drives. A disk already sent MUST NOT be read and MUST NOT change the duplicate count. The Apple MUST list the drives that
  may already have been sent, including a drive found in the round before the pause, and pressing a listed drive's number MUST send that drive's disk.
- **FR-111**: An image whose whole-disk hash matches an earlier image in the
  folder's manifest MUST still be written, and unless it was sent with its drive's number, the manifest MUST list it as a duplicate of
  that image and the drive's row MUST add DUPE OF and that image's file. A duplicate MUST have a manifest row and a sequence number, so the numbers match the disks fed in.
- **FR-112**: An image's file name MUST begin with the prefix typed on the Apple, as
  typed and with characters a file name cannot have replaced, then the session's sequence
  number, followed by the volume name on ProDOS or Apple Pascal, or the
  first program's name on DOS 3.3. With no name in the catalog,
  the prefix and sequence number MUST be the whole name. An existing file MUST NOT be
  overwritten.
- **FR-113**: A Full Send MUST accept no label typed on the Apple but the file name
  prefix. P on its entry screen or in a pause MUST open a line for it, where Return takes
  it, Esc puts back the prefix before, and Left arrow or Delete erases a letter. Each session
  MUST begin with no prefix. Images MUST be renamed on the PC.
- **FR-114**: The session MUST write a manifest to the destination folder,
  updated after each disk: each image's file name, drive, fingerprint, hash, time,
  unreadable sector count, whether it is a duplicate, its verification and its
  assessment: Likely copy-protected, Likely damaged or Unclear. It MUST be
  `manifest.fullsend`, CSV with a # header with instructions not to edit it, and a
  `full-send.csv` MUST be renamed on first use. A session started on a folder with a manifest MUST treat
  its disks as already sent and continue its sequence numbers.
- **FR-115**: The session MUST sound one tone on the Apple's speaker for a finished disk,
  another for a duplicate and another for a disk with bad sectors. The
  Apple's screen MUST contain each drive's state, the current image's name and
  progress, and the session's counts of disks sent, duplicates and disks with
  bad sectors.
- **FR-116**: A disk with unreadable sectors MUST NOT stop the session. Q
  (either case) on the Apple or Cancel on the PC MUST end the session after
  discarding any partial image, and return the Apple to the CassoLink screen.
  Esc MUST be ignored during a session. The screen MUST show
  `Q: BACK TO CASSOLINK`, and the session's rows MUST NOT be under the history's
  heading.
- **FR-117**: Group by MUST offer File Explorer's rows for the folder's type, any extra column on display, (None) only while grouped, and More, with File Explorer's group labels. An item with a value of several parts is listed under each part, and a command acts on it once.
- **FR-118**: Sorting or grouping by a property MUST NOT wait on the window's thread for its values.
- **FR-119**: Network, Downloads and This PC MUST open with File Explorer's default grouping until the user chooses otherwise, and Network in Tiles.
- **FR-120**: Search results MUST have File Explorer's Search options, with date, kind and size filters applied with or without the index.
- **FR-121**: A slow folder's rows MUST appear as they are read, and while a folder is read again its rows MUST stay on screen until the read finishes.
- **FR-122**: The list's background menu MUST have File Explorer's rows for This PC, the Recycle Bin, Home, Network, Libraries and a zip.
- **FR-123**: A shell command MUST be initialized and hosted as File Explorer hosts it, and one that faults MUST NOT close the window.
- **FR-124**: CassoLink's root MUST list a Full Send row, a peer of the connections, that opens the Full Send folder, once the folder exists.
- **FR-125**: The Full Send folder and an ADTPro server's folder MUST open grouped by Date modified, newest first, until the user chooses otherwise.
- **FR-126**: An ADTPro-compatible server MUST NOT start by default, and its list MUST have a floating Start server button. Other connections MUST start at launch, except one the user disconnected, an any-port serial connection with no port that has connected to an Apple, and a Wi-Fi listener the firewall doesn't allow.
- **FR-127**: Starting or creating a connection MUST disconnect others on the same port, each with its reason and a Connect button, and MUST NOT interrupt a running transfer.
- **FR-128**: The Apple's screen MUST merge the status into each drive's row, with a 35-cell per-track bar filled in track order, track and sector overlaid in inverse video, the percentage at the right edge, and a second map row marking R, U and X once a track has a problem. It MUST use 80 columns where detected, 40 otherwise, and a connection's Use 40 columns setting MUST force 40.
- **FR-129**: Verified transfers MUST apply to Full Send, Create image and Write image, as in Session 2026-11-01, with a global setting on an Options page for them, its toggles in one column after their labels, a per-connection override, a command per Create image and Write image on a drive's menu, and V on the Apple in Full Send at any time. The Apple's screen MUST indicate whether verification is on, whichever setting enabled it. Other reads and writes MUST NOT be affected.
- **FR-130**: A disk assessed as likely copy-protected MUST have its remaining tracks read once, with no retries or verification, MUST save its partial image and list, MUST add to a PROT count with a distinct tone, and MUST NOT stop the session.
- **FR-131**: The tree MUST animate a connection's icon and the receiving folder during a transfer, and MUST badge the Full Send and ADTPro folders with a count of received files not yet rendered in the list, across restarts.
- **FR-132**: Casso Explorer MUST read and write Apple Pascal and CP/M 140K disks, on images and over CassoLink. CP/M detection MAY be heuristic, with a manual choice.
- **FR-133**: A WOZ image MUST have a Disk properties window, opened from its menu and from Edit metadata on its preview's toolbar, that edits INFO's write protection, boot sector format, compatible hardware and required RAM, and META's standard keys and any others the image has, as in Session 2026-11-02 and Session 2026-11-04. The edits MUST be saved into the image in place, changing no byte but those fields, META and the header CRC. Its preview MUST list its WOZ info and WOZ metadata above the catalog, as an image with no file system lists them.
- **FR-134**: Casso Explorer MUST append every CassoLink connection's activity and each packet sent and received to one `CassoLink Log.csv` under Casso's Logs folder in local app data, a row each with the date and time, the connection, Activity, Sent or Received, and the message in plain English, and MUST drop the rows older than 30 days.
- **FR-135**: A folder in Details view with no column width the user set MUST fit each column but Name to its values, wider or narrower, measuring every row up to 500 and the rows on screen past that. A width the user set by dragging or fitting a column MUST be the folder's from then on, and an automatic fit MUST NOT be saved as one.
- **FR-136**: The CassoLink program MUST compute a CRC-16/XMODEM over itself as loaded, before it changes any of its code, and MUST send it in Ready. The host MUST load the program again when the CRC differs from the image it sent, and MUST fail the connection after two loads in a row that fail the check.
- **FR-137**: Where a folder is walked, the search box MUST take the AQS subset in Session 2026-11-04, with each file tested against the parsed query. An indexed folder MUST be given the query as typed, unless it has a wildcard or a regular expression, which is walked.
- **FR-138**: A walked search MUST support the properties in Session 2026-11-04, testing a file's name, size, dates and attributes before reading any other property or its contents, and MUST match an unrecognized property word as text.
- **FR-139**: A term between slashes MUST be matched as a regular expression, ignoring case, against the name or, after `path:`, the path. One that isn't valid MUST be reported in the infobar.
- **FR-140**: File contents MUST be on by default. A bare word or phrase MUST match the name or the contents, word by word; `name:` and `filename:` MUST limit a term to the name, and `content:` to the contents.
- **FR-141**: A file's contents MUST be read according to its registered IFilter as in Session 2026-11-04, with filters run in a separate process. A filter that fails or takes longer than 10 seconds MUST cost that file only.
- **FR-142**: Search results MUST be listed as they are found. Only terms that must appear in a name MUST be highlighted.
- **FR-143**: The preview pane MUST close when no file is selected and MUST open again when one is, while View's Preview pane is on.
- **FR-144**: A request whose reply arrives damaged MUST be sent again, up to five times. A failure after the resends MUST have an error message in words, and its log row MUST have its error code.
- **FR-145**: Idle polls MUST NOT be written to the CassoLink log, and a Ready MUST be written only when it differs from the last one written. Tests MUST NOT write to the user's log.
- **FR-146**: A Debug build MUST be able to flip or drop the Nth byte the PC sends over serial, over TCP, in the serial bootstrap and in the cassette audio. A Release build MUST NOT.
- **FR-147**: The Apple's program, assembled for a Debug build, MUST take a Fault command that arms one fault in what the Apple sends next: a flipped, dropped or stalled byte at an offset in the Nth packet of a command, a sector reported as unreadable, a sector's data changed with a CRC that matches the change, or a wrong CRC in a Read CRCs reply. A Release build's program MUST NOT have the command.
- **FR-148**: A Debug build of Casso Explorer MUST arm a fault for the next operation from a Debug menu, and a list of them from `--link-faults` at launch. Each injected fault MUST be logged.
- **FR-149**: Each fault kind MUST have a scenario test on an emulated machine that checks the recovery, over the //c's serial port, a ][+'s Super Serial Card, a //e's Uthernet II and a ][+'s cassette input.
- **FR-150**: After an operation fails, FAILED MUST be drawn over that drive's bar on the Apple's screen until the next operation on the drive.
- **FR-151**: A cassette bootstrap read that fails the monitor's checksum MUST NOT run. The monitor's command reads the tape and runs it on one line, so the monitor alone prints ERR and runs what it read. The Apple MUST read the tape again with nothing typed, and Casso Explorer MUST send the audio again while the shim hasn't sent Ready, up to five times, then report that the Apple didn't start.

**File types**

- **FR-156**: Casso Explorer MUST preview and convert each of these formats:
  Pascal textfiles; random-access text; Merlin source; S-C Assembler source;
  LISA source, in each version of its tokens; Business BASIC; Pascal
  codefiles; OMF; AppleWorks word processor, database and spreadsheet
  files; AppleWorks GS word processor files; VisiCalc; Teach; Gutenberg, from a
  read-only Gutenberg volume; Magic Window; super hi-res pictures, plain,
  packed, Apple Preferred and Paintworks; 3200-color pictures, plain,
  DreamGrafix and `.3201`; the first frame of IIgs animation files; LZ4FH
  pictures; Print Shop and Print Shop GS clip art, in color and monochrome;
  Applesoft shape tables; IIgs icons; Apple II and Apple /// hi-res fonts;
  Fontrix fonts; Print Shop fonts; IIgs bitmap fonts. A 65816 disassembly and
  the Macintosh formats are out of scope.
- **FR-157**: A format MUST be identified by its ProDOS file type and aux
  type where it has them, and on DOS 3.3 and Pascal volumes by the file
  type, name prefix and size it is saved with. The contents MUST pass the
  format's checks before the file is converted; a file that fails them falls
  to the existing rules, as hex and a raw copy.
- **FR-158**: AppleWorks and AppleWorks GS word processor files and Teach
  files MUST be drawn as styled text: paragraphs, tabs, bold, italic,
  underline and alignment. A copy out MUST write rich text,
  `NAME.AppleWorks WP.rtf`, `NAME.AppleWorks GS WP.rtf` or `NAME.Teach.rtf`.
  Gutenberg and Magic Window files MUST be drawn and written as plain text,
  `NAME.Gutenberg.txt` or `NAME.Magic Window.txt`, with their formatting
  commands removed.
- **FR-159**: AppleWorks databases, AppleWorks spreadsheets and VisiCalc
  files MUST be drawn as a grid, and a copy out MUST write CSV in UTF-8,
  quoted as RFC 4180 specifies: `NAME.AppleWorks DB.csv`,
  `NAME.AppleWorks SS.csv` or `NAME.VisiCalc.csv`. The first row MUST
  be a database's categories. A spreadsheet cell MUST be written as its
  stored value, or its formula as entered when the file has no value for it.
- **FR-160**: A random-access text file MUST be drawn as a grid and written as
  CSV, `NAME.Random text.csv`, one record on each row and one field of the
  record, split at its carriage returns, in each column. The record length
  MUST come from the aux type on ProDOS and from the spacing of its entries
  on DOS 3.3. The preview MUST offer a record length to replace the one
  found, which then applies to the preview and to a copy out of that file.
- **FR-161**: A Pascal textfile MUST be drawn and written as text,
  `NAME.Pascal text.txt`, with each indentation code expanded to spaces. A
  text file put onto a Pascal volume MUST be written as a textfile Apple
  Pascal's editor opens, with `.TEXT` added to its catalog name when it has
  no such suffix. A file with the `.Pascal text.txt` suffix put onto a
  ProDOS volume MUST be written as type $03.
- **FR-162**: Merlin source MUST be drawn in label, opcode, operand and
  comment columns, and written as text, `NAME.Merlin.txt`, with the fields
  separated by spaces. A file with that suffix, or one the put dialog sets to
  Merlin source, MUST be put as the file Merlin saves on that file system:
  a TXT file ending `.S` on ProDOS, a binary file whose name starts with
  `T.` on DOS 3.3. A Merlin file copied out and put back MUST be identical
  to the original.
- **FR-163**: S-C Assembler source, LISA source and Business BASIC MUST be
  drawn and written as listings, `NAME.S-C Assembler.txt`, `NAME.LISA.txt`
  and `NAME.Business BASIC.txt`. LISA MUST be decoded in each version of its
  tokens. These three convert out only.
- **FR-164**: A Pascal codefile MUST list its segment dictionary: each
  segment's name, kind, size and machine type, and each segment's procedure
  dictionary. Each p-code procedure MUST be disassembled into p-code
  mnemonics with their operands, and a segment of 6502 code MUST be
  disassembled. A copy out MUST
  write the listing, `NAME.Pascal code.txt`.
- **FR-165**: An OMF file MUST list each segment's header: its name, load
  name, kind, sizes, alignment and bank. Each record MUST be listed by its
  opcode, with code and data bytes in hex. A copy out MUST write the
  listing, `NAME.OMF.txt`.
- **FR-166**: Super hi-res pictures in each form in FR-156, 3200-color
  pictures in each form, the first frame of an animation file, LZ4FH
  pictures, Print Shop and Print Shop GS clip art and IIgs icons MUST be
  drawn, and a copy out MUST write PNG: `NAME.Super Hi-Res.png`,
  `NAME.3200 Color.png`, `NAME.Animation.png`, `NAME.LZ4FH.png`,
  `NAME.Print Shop.png` or `NAME.IIgs Icons.png`. Color clip art MUST be
  drawn in its colors, and monochrome clip art black on white. A super
  hi-res picture MUST be drawn in the colors of its palettes, at 320 or 640
  pixels as each line's mode sets, and MUST NOT take the Display page's
  monitor, since a IIgs's monitor is RGB. An LZ4FH picture is a hi-res
  picture and MUST be drawn as US9 draws one. An icon file MUST be drawn
  with each icon and its mask beside its name.
- **FR-167**: Apple II and Apple /// hi-res fonts, of 96 or 128
  characters, Fontrix fonts, Print Shop fonts and IIgs bitmap fonts MUST be
  drawn as a sheet of every character the font has, in rows of
  sixteen, with the font's name, family, style and size where the format
  stores them. A copy out MUST write the sheet as PNG, `NAME.Font.png`.
- **FR-172**: An Applesoft shape table MUST be drawn as a sheet of its
  shapes, in rows of eight, each at scale 1 and rotation 0 with its number
  under it, and a copy out MUST write the sheet as PNG,
  `NAME.Shape table.png`. A shape whose vectors extend past the sheet's cell MUST
  be clipped to the cell.
- **FR-173**: The preview of text, and a text copy out, MUST offer the
  character set the bytes are read in: Apple text, in which high and low
  ASCII are one character set, Mac OS Roman, or ISO 8859-1. The default MUST
  be Mac OS Roman for a IIgs format's text and Apple text otherwise. The
  choice MUST apply to that file's preview and copy out.
- **FR-168**: Each of these conversions MUST be the default for a drag or
  Get out, as the BASIC and text conversions are. A right-drag out MUST
  offer the conversion and a raw copy, and Get MUST offer the same choice.
  A raw copy's name MUST follow FR-017 and FR-017a.
- **FR-169**: A file that matches a format's type but fails to decode MUST
  be previewed with the decoder's reason above its bytes. A copy out MUST
  skip that file and report the reason, as FR-018a does, and complete the rest.
- **FR-170**: The command-line tool's get and put MUST apply the same
  conversions.
- **FR-171**: Each format MUST be implemented from published documentation:
  Apple's File Type Notes, the programs' manuals and format descriptions
  published under a license that allows it. Source code under the GPL MAY
  be read for a format's behavior, and MUST NOT be copied or
  paraphrased into Casso.
- **FR-174**: A multiple selection's context menu MUST offer Convert to
  folder, which writes each file with its conversion to a folder the user
  picks. A file that fails MUST be skipped, and every failure listed when
  the rest are written.

**Large volumes**

- **FR-175**: ProDOS volumes of any size up to 65,535 blocks MUST be read
  and written in raw block images, `.po`, `.hdv`, `.img` and `.iso`, in
  2IMG, `.2mg` and `.2img`, and in Disk Copy 4.2, `.dc`, `.dc42` and
  `.image`. Every operation available on a 140K ProDOS image MUST be
  available on them, the raw block dialogs included.
  Apple Pascal volumes of 800K, as Apple Pascal 1.3 writes them, MUST be
  read and written in the same containers.
- **FR-176**: A 2IMG image MUST be read by its header: DOS order, ProDOS
  order or nibbles, at any size the format allows. Its comment, creator,
  flags and creator data MUST be listed in Properties and written back on
  every write.
- **FR-177**: A Disk Copy 4.2 image's data checksum MUST be checked when it
  is opened and updated when it is written. Its tag bytes MUST be written
  back with it.
- **FR-178**: The New disk dialog and `casso disk create` MUST offer 800K and
  hard disk sizes for ProDOS, up to 32 MB, in each container FR-175 lists.

**Resource forks**

- **FR-179**: ProDOS extended files, storage type 5, MUST be listed, read,
  written, renamed and deleted, and a copy between ProDOS volumes MUST copy
  both forks and the Finder information.
- **FR-180**: Details MUST offer data fork length, resource fork length,
  total size and access flags columns. Access flags MUST list each of
  destroy, rename, backup, invisible, write and read.
- **FR-181**: A copy to the PC MUST write the resource fork and Finder
  information: inside the file in the AppleSingle style, and in an
  AppleDouble file, `._NAME`, beside the data fork in the Descriptive and
  CiderPress styles. A put of a data fork with its AppleDouble file, or of an
  AppleSingle file with a resource fork, MUST create an extended file
  identical to the original.
- **FR-182**: A copy of a file with a resource fork to a DOS 3.3, Pascal or
  CP/M volume MUST write its data fork alone and report that the resource
  fork was not copied.

**Archives**

- **FR-183**: NuFX archives, `.shk`, `.sdk`, `.bxy` and `.sea`, MUST be
  browsed as folders. Each record MUST be listed with its name, file type,
  aux type, dates, access and fork lengths, and a disk record MUST open as
  a disk image of its volume, a 140K disk's block order converted for the
  140K volume classes.
- **FR-184**: Every NuFX compression MUST be read: none, Squeeze, LZW/1,
  LZW/2, LZC 12 and 16, Deflate and bzip2. Each thread's CRC MUST be
  checked, and a record that fails MUST be reported while the rest are
  readable.
- **FR-185**: NuFX archives MUST be written: created from files or from a
  disk image, and records added, deleted and renamed. New records MUST be
  compressed with LZW/2, the compression ShrinkIt reads in every version.
- **FR-186**: Binary II files, `.bny` and `.bqy`, MUST be read and written,
  and a Binary II file wrapping a NuFX archive, `.bxy`, MUST open as the
  archive.
- **FR-187**: An archive or disk image MUST open in place inside a ZIP file,
  a gzip file, a disk image or another archive, at any depth. A ZIP member
  stored with Apple file type information in an AppleDouble entry, as
  CiderPress II writes them, MUST be read with its type, aux type and
  resource fork.
- **FR-188**: Test MUST check a disk image's structure with
  `VolumeIntegrityReport` and an archive's CRCs, from Casso Explorer's
  context menu and `casso disk test`, and report each problem found.
- **FR-189**: The archive formats MUST be implemented from published
  documentation, by the rule FR-171 sets: CiderPress II's NuFX, LZW,
  Squeeze and LZC notes, and the NuFX and Binary II specifications from
  Apple's File Type Notes.

**VisiCalc recalculation**

- **FR-190**: A VisiCalc file's formulas MUST be evaluated as VisiCalc
  evaluates them: every function and operator its manual lists, in the
  file's recalculation order, by rows or by columns, with one pass for each
  recalculation, so that a circular reference has the value VisiCalc
  draws. Each cell MUST be drawn with its format and column width, and as
  ERROR or NA where VisiCalc has them. For VisiCalc, this replaces FR-159's
  rule: a copy out MUST write each cell's computed value, and the preview
  MUST offer to write formulas instead.

**Block devices on the Apple**

- **FR-191**: The link program MUST find each ProDOS block device and each
  SmartPort unit by its slot firmware's signature bytes, on a ][+, //e,
  //c, //c Plus and IIgs, and MUST read and write blocks through the
  firmware's entry points. Each unit MUST be a node under the Apple with
  its slot, unit, size and volume.
- **FR-192**: Every operation of Story 14 MUST work on a block device: a
  ProDOS volume of any size FR-175 allows is browsed and changed, Read disk
  to image writes `.po`, `.hdv` or 2IMG, and Write image to disk writes and
  reads back every block. Verified reads and writes MUST apply to blocks as they apply to sectors.
  When the link drops during either, the partial image and the last
  verified block MUST be saved beside it, in `NAME.partial`, and the next
  read or write of the same device, matched by its slot, unit, size and
  volume, MUST offer to continue from that block.
- **FR-193**: A device error MUST be reported in words: no disk, write
  protected, device offline and I/O error, with the block number.
- **FR-194**: Block devices MUST be tested in scenario tests on Casso's
  emulated machines once Casso has a SmartPort card, and until then on
  AppleWin with a hard disk card and on MAME, launched from a script, with
  a UniDisk 3.5 on a //c with ROM 4 and on a //c Plus.

**Raw capture**

- **FR-195**: Capture raw disk MUST read each quarter track from track 0 to 34, and to 35 when the drive can step to it, as its nibble stream over at least two
  revolutions, with the time between bytes measured so that sync bytes are
  written with their extra zero bits. The capture MUST be written as a WOZ 2 image with a bit stream for each quarter track that has data, and its TMAP MUST map each quarter track to the stream read there.
- **FR-196**: Each track MUST be read at least three times. A region that
  differs between reads MUST be written as a run of zero bits that WOZ
  readers read as random bits, and the capture's report MUST list each such track. A quarter track with
  no data on its first read MUST NOT be read again, and a later read MUST
  be sent only where it differs from the first.
- **FR-197**: Options MUST offer Capture raw on protected disks for Full
  Send. With it on, a disk assessed as likely copy-protected MUST be
  captured raw, and its manifest row MUST indicate a raw capture.
- **FR-198**: A capture's INFO chunk MUST list Casso Explorer and its
  version as the creator, and its META chunk the capture's date and the
  drive. The image MUST open in the Disk properties window of FR-133.

**Virtual drives**

- **FR-199**: Make CassoLink ProDOS disk MUST write a bootable disk from the
  ProDOS on a disk image the user picks, with the link program and its
  block driver. No part of ProDOS MUST ship with Casso.
- **FR-200**: On that disk's boot, the block driver MUST be installed for
  two drives in a free slot's entries in ProDOS's device table, and each
  block read or written MUST go to the image set for that drive in the
  connection's settings, written in place at its offset in the file. The
  driver MUST lower HIMEM by no more than 1 KB beside BASIC.SYSTEM.
- **FR-201**: A block write the link doesn't complete MUST return ProDOS's
  I/O error, and the image MUST have that block whole or not at all.

**Cassette disks**

- **FR-202**: On a ][+ or //e whose link started over cassette, Read disk to
  image MUST receive each track from the Apple's cassette output through the
  PC's audio input, and Write image to disk MUST send each track as audio into the
  Apple's cassette input, write it and read it back to verify. The
  connection's settings MUST pick the audio input and output devices and
  draw the input level.
- **FR-203**: Each track sent as audio MUST have a checksum, and a track that
  fails MUST be sent again, up to five times, as FR-144 sets for serial.
- **FR-204**: ADTPro's audio client MUST be served by its published
  protocol, by the rule the Assumptions set for ADTPro compatibility.

**Tape audio**

- **FR-205**: Convert to tape audio MUST write one recording of the
  selected Applesoft, Integer BASIC and binary files in the monitor's tape
  format, a BASIC program with the length record BASIC's SAVE writes, a gap
  of two seconds between files, as WAV, MP3 or Ogg Vorbis, at 44.1 kHz and
  in mono. A sidecar, `NAME.tape.txt`, MUST list each file's start time,
  length, load address and the command that loads it.
- **FR-206**: A recording in WAV, MP3 or Ogg MUST open as a folder of the
  files found in it, each with its checksum checked, a BASIC program
  recognized by its length record, and each copied out by the conversion
  rules of its type. A file that fails its checksum MUST be listed with
  that reason.
- **FR-207**: Convert to disk audio MUST write an image's tracks as the audio FR-202 sends, so that the recording played from any player is
  written to disk by the link.

**More Apples and interfaces**

- **FR-208**: The link MUST run on a IIgs's modem and printer ports, and
  MUST set the IIgs to 1 MHz while a 5.25" drive is read or written and
  restore its speed after.
- **FR-209**: A serial card with Pascal 1.1 firmware signature bytes and no
  driver matched by its ID bytes MUST be used through its init, read, write
  and status entry points.
- **FR-210**: The link MUST run over an original Uthernet and a LANceGS as
  over an Uthernet II, with the same announcement.
- **FR-211**: The link program MUST run on a IIgs with a Super Serial Card
  and on compatibles whose serial and disk hardware match an Apple's, such
  as the Laser 128. These and FR-208 to FR-210 MUST be tested in KEGS,
  GSplus, MAME or AppleWin as Story 28 lists, MAME launched from a script.

**Apple ///**

- **FR-212**: Make CassoLink SOS disk MUST write a bootable disk from the
  SOS kernel and drivers on a disk image the user picks, with the link's
  SOS program as its interpreter. No part of SOS MUST ship with Casso.
- **FR-213**: The SOS program MUST connect over the Apple ///'s built-in
  serial port, MUST list the built-in drive, the external drives and each
  block device SOS has a driver for, and MUST read and write them through
  SOS's block calls.
- **FR-214**: Every operation of Story 14 MUST work on the Apple ///, and its
  SOS volumes MUST be browsed by Story 33's file system.

**DOS 3.2 and large DOS 3.3 disks**

- **FR-215**: 13-sector disks MUST be read and written in `.d13`, `.nib` and
  `.woz`, in the 5-and-3 encoding, with DOS 3.2's catalog, and created from
  the New disk dialog and `casso disk create`.
- **FR-216**: The link MUST read, write and browse 13-sector disks, and Read
  disk to image MUST offer `.d13` and `.woz` for them.
- **FR-217**: DOS 3.3 volumes MUST be read and written with any track and
  sector count the VTOC allows, up to 50 tracks of 32 sectors, and 80-track disks of 16 sectors in the Basis 108's layout, whose VTOC has two bytes for each track. CiderPress II's DOS notes document the layout.

**Partitions and hybrid disks**

- **FR-218**: These layouts MUST be found by their published markers and
  opened as a node per partition, with its number, size and volume:
  FocusDrive and MicroDrive partition maps; DOS.MASTER volumes inside a
  ProDOS volume; AmDOS, OzDOS and UniDOS 800K disks; DOS 3.3 hybrids with
  ProDOS or Pascal; and Pascal ProFile Manager volumes. A CFFA card has no
  map: its partitions are consecutive 32 MB slices, found by the volume
  directory at the start of each, as firmware 1.2 and 2.x both read them.
  After the fourth slice, the HFS partitions of up to 1 GB that the CFFA's
  GS/OS driver for firmware 1.2 adds MUST also be opened.
- **FR-219**: Copy partition to image MUST write a partition's blocks as an
  image, and Replace partition MUST write an image no larger than the
  partition over it, after a confirmation that lists both.
- **FR-220**: On a hybrid disk, a write to one file system MUST NOT
  allocate or change a block of another's.
- **FR-221**: A storage card MUST be detected when it is inserted, and its
  partition table read without administrator access. A partition Windows
  mounts MUST appear under This PC by its drive letter, and every other
  partition, or a card with no table, MUST appear as a storage card node.
  Administrator access MUST be requested only when the user first opens a
  storage card node, and once per Casso Explorer session. A tab restored at
  launch MUST NOT request it. Each command that writes to a card MUST be
  confirmed once, with the drive's model and size.
- **FR-243**: Raw access MUST run in a separate elevated broker program that
  opens only storage cards: removable media on a USB, SD or MMC bus, with no
  Windows, boot, page file, hibernation or BitLocker volume. Hard disks,
  SSDs and NVMe drives MUST NOT be opened. The broker MUST make each check
  itself at every open, MUST accept a write only inside an Apple partition
  it detects, and MUST fail a write that overlaps a mounted Windows volume.
  The pipe MUST admit elevated administrators only. Casso Explorer MUST
  accept only the broker process it started, and the broker MUST accept only
  the Casso Explorer in its folder, signed with the broker's certificate
  when the broker is signed. The broker MUST exit when the pipe closes.
- **FR-244**: Initialize card for Apple MUST erase a storage card and write
  a new Apple layout, after a confirmation that lists the card's model,
  size and each volume erased with its drive letter and label. It MUST be
  the only command that writes outside an Apple partition.
- **FR-245**: Initialize card for Apple MUST offer Quick format, checked by
  default, which writes each ProDOS volume's boot blocks, directory and
  bitmap only. Unchecked, every block of each volume MUST be zeroed.
- **FR-246**: The storage card broker MUST log, for each card it opens, the
  bytes read and written and the time spent in the device, so a slow card
  and a slow connection can be distinguished.

**Volumes read by the block**

- **FR-247**: ProDOS and HFS volumes MUST read blocks on demand from the
  file, card, hard drive image or CassoLink drive they are on, through a
  cache, rather than from a copy of the whole volume in memory.
- **FR-248**: A write to a volume on a storage card MUST send only the
  blocks it changes, committed together. An image file MUST be rewritten
  whole through a temporary file that replaces it, so that after a failure
  partway through, the image is as it was.
- **FR-249**: DOS 3.3, Pascal and CP/M volumes MAY be read whole through
  the same block interface.

**Block mode in the disk inspector**

- **FR-250**: The disk inspector MUST open block images, 800K disks, hard
  drive images and card partitions in a block mode: block number, hex and
  text panes of one 512-byte block, and editing written as the sector
  editor writes.
- **FR-251**: Block mode MUST annotate ProDOS, SOS, Pascal and HFS blocks by
  their structure, and 2MG and Disk Copy 4.2 headers, from the formats'
  published documentation.
- **FR-252**: Block mode MUST color each block by its use from the volume's
  integrity report, with distinct colors for blocks marked used with no
  file and for blocks in two files, and the owning file's path MUST appear
  on hover.
- **FR-253**: A block number in an index block, directory link or B-tree
  link MUST step to that block when clicked, with Back and Forward.
- **FR-254**: Each partition MUST be drawn as a band over its blocks.
- **FR-255**: Block mode MUST compare two block images block by block, and
  mark free blocks with text or code in them, which can be saved as a file.
- **FR-256**: A volume inside another MUST be addressed and annotated by
  its file system over the outer volume's blocks: the DOS 3.3 volumes of
  UniDOS, AmDOS and OzDOS disks by track and sector, and a hybrid disk's
  second file system.

**HFS**

- **FR-222**: HFS volumes MUST be read and written in every container
  FR-175 lists, up to 2 GB: both forks, Finder information, Mac OS Roman
  names of up to 31 characters, and dates. A file copied to the PC MUST
  follow FR-181.
- **FR-223**: A copy between HFS and ProDOS MUST map types and creators as
  Apple's File Type Notes specify for GS/OS, and a name with a character
  the target can't store MUST be reported, as FR-017 does.
- **FR-224**: The New disk dialog and `casso disk create` MUST create HFS
  images at 800K and hard disk sizes.

**SOS**

- **FR-225**: SOS volumes MUST be read and written on 140K disks and ProFile
  images, with their file types, and created from the New disk dialog and
  `casso disk create`.
- **FR-226**: A copy between SOS and ProDOS MUST copy the type and aux type
  where both file systems define them, and report a type the target lacks.

**Gutenberg writes and RDOS**

- **FR-227**: Gutenberg volumes MUST be written once the free-space map's
  format is worked out from disks Gutenberg saves in Casso, documented in the
  feature's contracts. A file copied in, renamed or deleted MUST update the
  map as Gutenberg does.
- **FR-228**: RDOS volumes MUST be read, read-only, with each file listed
  and copied out.

**Viewer window**

- **FR-229**: Open in viewer MUST open a window with the file's preview at
  the window's size, from the context menu, and from Enter when Options sets
  Enter to open the viewer. Each Casso Explorer window MUST have at most one
  viewer, reused for the next file opened.
- **FR-230**: The viewer MUST step to the previous and next file of its
  folder, in the list's sort order as it is when the step is taken, by
  arrows on the window and by Left and Right, skipping folders and stopping
  at each end.
- **FR-231**: The viewer MUST offer Save as, with the conversion and raw
  choices Get offers, and pictures MUST be zoomed with Ctrl and the mouse
  wheel.

**Collection tools**

- **FR-232**: Disk list MUST list each disk image in a folder's subtree with
  its path, container, file system, volume name, file count, free space,
  CRC-32 and SHA-256. Images inside ZIP, gzip, NuFX and Binary II files
  and inside other images MUST be listed too, at any depth FR-187 opens,
  with the archive and the image inside it in the path. Find duplicates
  MUST search the same set.
- **FR-233**: Find duplicates MUST group disks whose volume data is the same,
  whatever their containers, and files whose data and type are the same
  across different disks.
- **FR-233a**: Both tools MUST open their results in a tab as a results
  folder, as a search does: a row for each disk or file, each duplicate set
  a group, and the context menu a row has in any folder. The results
  toolbar MUST offer Save as CSV.
- **FR-234**: A search with File contents on MUST search each file inside a
  disk image as its conversion writes it: BASIC detokenized, Apple text as
  text, and binary files not at all.
- **FR-235**: `casso disk list`, `casso disk dupes` and `casso disk search`
  MUST write the same results as the window.

**Language tools**

- **FR-236**: Applesoft and Integer BASIC listings MUST be tokenized into the
  program BASIC saves from the same lines, on a put and from `casso basic
  tokenize`, and detokenized from `casso basic list`.
- **FR-237**: Renumber MUST renumber a BASIC program's lines from a start and
  a step, with each GOTO, GOSUB, ON, THEN, LIST, RUN and DEL target, and
  MUST fail with the line when a number would pass 63999 in Applesoft or
  32767 in Integer BASIC.
- **FR-238**: Cross-reference MUST list each variable, line reference, and
  CALL, PEEK and POKE address of a BASIC program with the lines it is on,
  in Casso Explorer's preview and from `casso basic xref`.
- **FR-239**: Removed: language servers are cut.

**Game data**

- **FR-240**: Wizardry I, II and III scenario files MUST be drawn as lists
  of monsters, items and characters, and as a map of each maze level with
  walls, doors and special squares. A copy out MUST write the lists as text
  and each map as PNG.
- **FR-241**: Infocom story files of each Z-machine version MUST list their
  header, abbreviations, dictionary, object tree and decoded strings, and a
  copy out MUST write the listing as text.
- **FR-242**: Every format and protocol in FR-190 to FR-241 MUST be
  implemented by the rule FR-171 sets.

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
  address bar, most recent first, with no duplicates; persisted. Every
  window adds to the one list, so a path typed in one is offered in all.
- **List view mode**: how the file list draws its entries, one of the
  views FR-044 names; persisted.
- **Registration**: the formats Casso is the program for, and for each the
  program that had it before; written and removed only by the user's
  choice.
- **Navigation pane option**: one of the pane's five options, either
  following File Explorer's setting or holding Casso Explorer's own value;
  persisted.
- **CassoLink**: a connection to one Apple II: its transport (serial port,
  TCP address, Wi-Fi modem, or ADTPro-compatible server), speed and
  handshaking, the machine at the other end, and its drives; its settings
  persisted, and its name made from them. A running Casso or an Uthernet
  II's announcement is listed as one with no settings saved.
- **ADTPro-compatible server**: a CassoLink connection that serves an
  unmodified ADTPro client on a COM port or a TCP port, with the folder it
  shares; requests are confined to that folder's subtree.
- **Apple drive**: one 5.25-inch drive on a linked Apple: its slot and
  drive, and the volume in it, with its file system.
- **Link program**: the Apple-side shim and the program it loads, each with
  a version; the program's copy on the PC is the current one.
- **Link operation**: one request from the PC to the Apple, with its
  progress; queued, running, done, failed or canceled.
- **Running Casso**: a Casso instance whose emulated serial port is
  connected to a TCP port: its process, machine, serial port and address;
  written to a file while it runs, for Casso Explorer to list as a link.
- **Link announcement**: a UDP broadcast from an Uthernet II booted from
  the link boot disk: a version, the card's address, TCP port and MAC
  address. Casso Explorer lists each Apple once, by its MAC address, at the
  address of its latest announcement.
- **Printer connection**: the one connector the emulated ImageWriter II is
  attached to: a parallel card's slot, a Super Serial Card's slot, a //c
  serial port, or none.
- **Full Send session**: one run of alternating disk reads from a linked
  Apple: its destination folder, image format, sequence number, the
  fingerprints and hashes of its disks, and its manifest.
- **Block device**: one ProDOS block device or SmartPort unit on a linked
  Apple: its slot, unit, size and volume.
- **Raw capture**: a WOZ 2 image read as nibble streams from a linked
  Apple's drive, with the tracks whose reads differ.
- **Virtual drive**: one of the two drives the CassoLink ProDOS disk's block
  driver installs, and the image on the PC it reads and writes.
- **Tape recording**: an audio file of one or more files in the monitor's
  tape format, with its sidecar.
- **Partition**: one volume inside an image or card with a partition
  layout: its number, its first block, its size and its file system.
- **Disk list**: one row for each disk image in a folder's subtree, with its
  checksums, saved as CSV.
- **Viewer**: a window with one file's preview, its folder and the list's
  sort order.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every verb of the command-line disk tool, the result
  produced from Casso Explorer is byte identical to the command-line result on
  the same input.
- **SC-002**: A user with no command-line experience can list a disk,
  preview a file, and copy it to a host folder within two minutes of
  first launch, with no documentation.
- **SC-003**: A folder of 200 disk images lists in under one second, and
  a disk's catalog appears within 200 ms of selection.
- **SC-004**: Every acceptance scenario in stories 1 through 4 completes
  with the mouse disconnected.
- **SC-005**: The Casso Explorer executable holds zero functions, and the unit
  test project exercises browsing, catalog, preview, conversion and
  targeting with no window and no real disk.
- **SC-006**: The Windows theme switching to dark changes the window
  within one second with no restart.
- **SC-007**: A range selected in either column of a hex preview covers
  exactly the same bytes in the other column, at every grouping width,
  including a range that starts and ends mid-row.
- **SC-009**: Captures of Casso Explorer and File Explorer at the same scale,
  set side by side, differ in no element of the chrome a reviewer can
  name -- text, row metrics, headers, hover, chevrons, splitters, edges
  -- outside the palette of Casso's three themes.
- **SC-010**: With twenty tabs open in a window 900 pixels wide, every
  tab can be reached and every tab can be reordered.
- **SC-011**: A user can reach any folder on the host, and any directory
  inside an image, by typing its path into the address bar.
- **SC-012**: Captures of Casso Explorer's command bar and File Explorer's at the
  same scale, set side by side, differ only in the departures FR-042 and
  FR-043 name.
- **SC-013**: After eleven paths are typed, the address bar offers the
  latest ten, newest first; typing the oldest of them again puts it at the
  top with ten entries still listed. A path that fails to navigate leaves
  the list unchanged.
- **SC-014**: A fresh install of the package, on a machine with no build
  of Casso on it, starts Casso Explorer from the Start menu, from its
  executable's name typed at a prompt, and from Casso's Browse disks
  command, with no message about a missing program.
- **SC-015**: The zip archive holds the Casso Explorer executable beside
  Casso's, and unpacking it onto a machine with no build tools gives a
  window that opens a disk image.
- **SC-008a**: The hex view shows a 64 KB run, addressed from an origin
  of the host's choosing, with no copy of those bytes held by the widget
  and no pause a user can see when scrolling through it.
- **SC-008**: A hex row, a disassembly line and a text file's columns all
  line up down the pane at every window width and scale.

- **SC-016**: Side by side with File Explorer on the same machine, the
  navigation pane's sections match File Explorer's in order and in content
  for every section present, and a pin made in either program shows in
  the other within one refresh.
- **SC-017**: A search for a word over a folder of 1,000 indexed files
  shows its first results within 2 seconds, and returns the same host
  files File Explorer's search returns for the same query.
- **SC-018**: After registering, all six formats show Casso's icon in File
  Explorer and open in Casso with one double-click; after unregistering,
  a comparison of the user's associations before and after shows no
  difference.
- **SC-019**: In a catalog of 50 files of mixed types, a user tells system,
  BASIC, binary and text files apart by icon alone, in every view.
- **SC-020**: A picture shown in Casso Explorer's preview and on Casso's
  screen, with the same monitor and effects, matches pixel for pixel at the
  same size.
- **SC-021**: Switching the monitor redraws the preview within a quarter
  second, and the choice survives a restart.
- **SC-022**: For a .docx, a .pdf and a .png, the Preview pane draws the
  same preview as File Explorer's, within a second of selection.
- **SC-023**: For the same file, folder and tree node on the same machine,
  the context menus of the two programs list the same first-level items in
  the same order.
- **SC-024**: A user opens a zip, previews a file in it, and extracts it,
  without leaving Casso Explorer, in under a minute.
- **SC-025**: Over a folder, a drive, a disk image, an archive and a program, the highlight and the
  words under the pointer match File Explorer's for the same drag.
- **SC-026**: An Applesoft program exported to the host previews as the
  same lines of text as the program inside its disk image.
- **SC-027**: A 140 KB disk in a real //c reads into an image over serial at
  115,200 baud in under 30 seconds, and the image matches one ADTPro reads
  from the same disk byte for byte.
- **SC-028**: A 10 KB file copies from a real //c's disk to the host, or
  from the host onto the disk, in under 5 seconds from the drop.
- **SC-029**: Every acceptance scenario in stories 14 and 15 that does not
  name real hardware runs against an emulated //c in the automated test
  suite, and every file and image it moves compares byte for byte.
- **SC-030**: With the cable pulled mid-transfer, nothing on the PC is left
  half-written, and the first transfer after reconnecting succeeds with
  neither end restarted.
- **SC-031**: After a Casso release with a newer link program, the Apple
  side is the new program at its next launch, with no step by the user.
- **SC-032**: ADTPro 2.1.0's own client sends a disk to Casso and receives
  one from it, and each image matches the one ADTPro's own host makes
  byte for byte.
- **SC-033**: From a //c at the BASIC prompt with no disk, the link is
  running within 60 seconds of choosing Bootstrap.
- **SC-034**: On a PC that has never run Casso, installing the package and
  turning on the Uthernet II raises no firewall prompt, and an Apple on the
  network is listed.
- **SC-035**: With the firewall refusing, the connection's row carries a
  warning, its tip names the firewall as the cause, and the action in the pane
  restores the listing without restarting either program.
- **SC-036**: On a network Windows calls public, the warning says so and
  offers both remedies, while a connection over a serial cable on the same PC
  carries no warning at all.
- **SC-037**: No .cpp the feature adds is over about 2,000 lines, and each
  class split to get there is a set of component classes with separate
  state rather than one class across several files. The unit and scenario
  suites pass unchanged in what they check before and after each split.
- **SC-038**: On a //c at 115,200 baud, a Full Send of two 140K disks with a
  swap during each transfer runs with no PC interaction and no key press after Space, at
  under 30 seconds a disk, and each image matches one read by Read disk to
  image from the same disk.
- **SC-039**: In a Full Send that includes a duplicate and a disk with an
  unreadable sector, the manifest lists both correctly, no duplicate image is
  on the PC, and the session continues past each.
- **SC-040**: The window's thread doesn't stall for more than 100 ms when Network opens or when a folder of thousands of files is grouped by a property, and Network's first rows appear within 5 seconds.
- **SC-041**: The Group by and background menus of each folder type match File Explorer's rows, except Gallery's background menu, which couldn't be read from File Explorer.
- **SC-042**: With verified reads on, a track whose reads disagree is saved with the majority of up to nine reads, and a one-drive //c disk with no disagreement takes under twice the time of a standard read.
- **SC-043**: A likely copy-protected disk doesn't take more than one and a half times a standard read, and with verified writes on, a track damaged after it was written is reported by the final comparison.
- **SC-044**: A search of C:\Windows with File contents off returns the same files as the Windows search folder for the same name condition, in no more than a quarter of its time.
- **SC-045**: A search with File contents on lists its first result within 2 seconds of its folder being walked, and finds a word in a .txt, a .md, a UTF-16 .log and a .docx, but not in a .dll.
- **SC-046**: Each fault kind recovers, or fails with a message in words, in its scenario test, and the same fault armed on a real //c over serial has the same result.
- **SC-047**: For each format in FR-156, a sample file previews with the same
  text, values or pixels as the program that made it draws in Casso, and its
  converted copy opens in Word, Excel, Notepad or Photos with the same
  content.
- **SC-048**: A Merlin source file copied out and put back is identical byte
  for byte, and a Pascal text file put onto a Pascal volume opens in Apple
  Pascal's editor with the same text.
- **SC-049**: A file of any of these formats, up to 64 KB, is previewed
  within half a second of being selected.
- **SC-050**: An 800K and a 32 MB image changed in Casso Explorer pass
  CiderPress II's Test, and the 800K image catalogs in ProDOS on MAME's //c
  Plus.
- **SC-051**: A file with a resource fork copied to the PC and put back is
  identical in both forks and its Finder information.
- **SC-052**: Every file in CiderPress II's `test-files.sdk` copied out of
  Casso Explorer is identical to CiderPress II's extraction of it, and an
  archive Casso Explorer creates passes CiderPress II's Test and NuLib2's.
- **SC-053**: Every cell of each sample VisiCalc sheet has the value
  VisiCalc draws after a recalculation in Casso.
- **SC-054**: On each emulator in FR-194, an 800K and a 32 MB volume read
  through the link are identical to the images the emulator mounted, and a
  file put through the link opens on the Apple.
- **SC-055**: A raw capture of Archon on the owner's //c boots in Casso, and
  a capture of each protected test image in an emulated //c boots as its
  source does. A capture on a //c at 115,200 baud takes under 5 minutes.
- **SC-056**: A file saved on a CassoLink virtual drive from BASIC.SYSTEM is
  in the image on the PC when the save returns, on an emulated //c and on
  the owner's //c.
- **SC-057**: A 140K disk sent each way over cassette audio in Casso is
  identical to its source.
- **SC-058**: Each file in a tape recording Casso Explorer writes loads and
  runs in an emulated ][+, and each file read back from it is identical to
  the original.
- **SC-059**: Story 14's scenarios pass on each machine and interface in
  Story 28's test list, and on MAME's Apple ///.
- **SC-060**: Images in DOS 3.2, 40-, 50- and 80-track DOS 3.3, each
  partition layout in FR-218, HFS, SOS and RDOS, changed in Casso Explorer
  where they can be written, pass CiderPress II's Test, and each file copied
  out is identical to CiderPress II's extraction.
- **SC-061**: An HFS image changed in Casso Explorer boots GS/OS in KEGS or
  GSplus and opens each changed file.
- **SC-062**: A Gutenberg document written by Casso Explorer opens in
  Gutenberg in Casso and saves again.
- **SC-063**: The viewer steps to the next file within half a second for
  files up to 64 KB.
- **SC-064**: Disk list and Find duplicates over 5,000 images finish within
  five minutes, and find every known duplicate in the test set with no
  false matches.
- **SC-065**: A tokenized listing of each sample BASIC program is identical
  to the program BASIC saves in Casso, and a renumbered program runs with
  the same output.
- **SC-066**: Each sample Wizardry and Infocom file lists the same
  monsters, items, words and text as the game or an interpreter has.
- **SC-067**: Listing the root of a full 32 MB ProDOS volume reads fewer
  than 64 blocks, and copying a 10 KB file onto it writes fewer than 64.
- **SC-068**: Quick format initializes a 1 GB card with 13 volumes in
  under 5 seconds.
- **SC-069**: Every block of CiderPress II's ProDOS and HFS sample images is
  annotated with its use in the catalog, and a step to a block on a 32 MB
  volume or a card's partition takes under 100 ms.

## Assumptions

- The command widgets, meaning command, dropdown, toolbar, menu bar and
  context menu, are delivered by the preceding feature and are not
  re-specified here. Casso Explorer uses all of them but the menu bar.
- Widgets this feature adds to the UI library: lazy expansion, stable ids
  and hidden checkboxes on the existing tree (which already recurses to
  any depth), multi-select on the list, a splitter, a status bar, a
  framebuffer view, drag-out support beside the existing drop-in
  support, a hex view, and light and dark palettes. Each is general, not
  Casso Explorer-only. The tab strip exists already.
- Supported image formats are the ones the command-line tool supports
  today. The one container added is AppleSingle, which holds a single file
  rather than a volume.
- Conversion rules on get, put and drag are the command-line tool's rules,
  plus an Integer BASIC detokenizer this feature adds for preview and Get.
  The tree detokenizes Applesoft only today. No Integer BASIC tokenizer:
  an Integer listing dragged in is put as text until Story 37 adds one.
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
  from the user's own video ROM at run time redistributes nothing; one was
  built on 2026-10-13 and dropped at the owner's request, so Apple text is
  in the theme's face.
- The hex preview is a widget the UI library gains, general rather than
  Casso Explorer-only: the emulator's debugger wants the same view of memory,
  which is why the bytes arrive through a source and an origin instead of
  a buffer the widget owns. Editing bytes in place is not part of this
  feature; the debugger's write path is a follow-on, and the widget's
  selection is the ground it will stand on.
- Sector and block operations live under an Advanced submenu and confirm
  before writing.
- Known folders are seeded once from the recent-disks list and thereafter
  maintained independently of it; the recent-disks list keeps its current
  role for Casso's own picker ordering.
- The emulator exposes what Casso Explorer needs over its existing intent
  channel, which gains an insert intent and a describe-machine reply.
  Casso Explorer is told the channel name on its command line when Casso
  launches it.
- Screen-reader announcement is out of scope. Dxui draws every control
  itself, so unlike native Windows controls nothing is announced until
  the library has an automation provider; that provider is one generic
  walk of the existing control tree plus a few patterns, serves Casso's
  chrome as well, and is its own follow-on feature. Casso Explorer sets names
  and roles now so that provider needs no per-control work here.
- Registration covers the six formats Casso reads today. 3.5-inch,
  hard-disk and DOS 3.2 images are tracked separately (GitHub issues 163
  and 164) and join the list when Casso can open them.
- Apple file-type icons are drawn for Casso, not taken from Apple's or any
  other program's artwork, and are used inside Casso Explorer only.

- File Explorer parity for the navigation pane, the address bar's drop-down
  and search means what File Explorer shows on the same machine at the
  same time; a section, root or result File Explorer does not show is not
  expected of Casso Explorer either.
- Quick access is read from and written through the shell, not kept by
  Casso Explorer, so the two programs share one list.
- Search inside disk images matches by entry name only, and the files'
  contents are not indexed, until Story 36 searches them.

- ADTPro compatibility means its published protocol as of release 2.1.0,
  the version on the owner's machine.
- The Apple-side program runs on a ][+, //e or //c with 64 KB, with no DOS
  in memory, and reads DOS 3.3 and ProDOS disks itself. 13-sector DOS 3.2
  disks are Story 30.
- The link reads and writes 5.25-inch drives. ProDOS block devices, such as a
  UniDisk 3.5 or a hard drive, are Story 23.
- Capturing copy-protected disks as WOZ images from nibble reads is Story
  24, and flux capture is out of scope. Every other link operation reads a
  protected disk only as far as its standard sectors allow.
- Casso's emulation of the Super Serial Card and of the Uthernet II are two
  separate features. Each lands in Casso on its own and is merged into this
  branch when the link's work on that card starts; the link's scenarios on
  a ][+ or //e depend on them.
- A serial Wi-Fi modem is any that can make a plain TCP connection from
  Hayes-style commands; Casso Explorer accepts the connection as it accepts
  one from an emulated machine.
- An Apple-side menu, from which the Apple starts transfers itself, is a
  later feature; here the Apple side is a server with a status screen.
- The link is used from Casso Explorer only; there is no command-line
  entry for it beside `casso disk`.

## Roadmap

The order in which Casso Explorer's remaining work is built, from GitHub
issue #181 checked against CiderPress II, ADTPro, a2kit, AppleCommander,
DiskBrowser and DiskM8 (Session 2026-11-08). Each step is specified in full
before it is built. VisiCalc recalculation in 2.3 is Story 22; 3.1 to 3.5
are Stories 23 to 27; 3.6, 3.7, 3.9 and 3.10 are Story 28; 3.8 is Story
29; 4.1 to 4.4 are Stories 30 to 33; 4.5 and 4.6 are Story 34; and 5.1 to
5.4 are Stories 35 to 38. Effort is for agent-driven work:
S is a day or two, M up to a week, L several weeks.

**Done on this branch**: DOS 3.3, ProDOS, Apple Pascal and CP/M read and
write; Full Send; verified reads and writes; the cassette bootstrap.

**1. Foundations**

1. Merge `040-disk-inspector` into this branch, with its session: the sector
   and block editor, and the Pascal and CP/M file maps. It changes the
   volume layer, which step 2 changes too, so it goes first.
2. 800K and hard disk images, 2IMG and Disk Copy 4.2: Story 19 (M).
3. Files with resource forks, AppleDouble, and the fork and access columns:
   Story 20 (M).
4. ShrinkIt and Binary II, reading and writing, nested archives, gzip, ZIP
   with Apple file types, and Test: Story 21 (M to L). First, check what
   Casso Explorer does today with a disk image inside a ZIP file.

**2. File types**: Story 18, in this order, the most used first in each group.

1. RTF and CSV writers, and Convert to folder (FR-174).
2. Text: Pascal textfiles, random-access text.
3. Business: AppleWorks Classic, then AppleWorks GS, Teach, VisiCalc,
   Gutenberg and Magic Window. VisiCalc recalculation, as DiskBrowser does
   it, is added here as a later step (M).
4. Code: Merlin and S-C first, then LISA, Business BASIC, Pascal codefiles
   and OMF.
5. Pictures, then fonts and shape tables.

**3. CassoLink**

The owner's only real Apple is a //c, so the devices it lacks are tested in
emulators run as targets of the link, never as sources of code:

- Casso first: once Casso has its SmartPort hard disk card, #101, and 3.5"
  drives, #163, block devices are tested as the //c's serial port is now,
  in the scenario tests on Casso's emulated machines.
- AppleWin until then, for a hard drive through its hard disk card, with its
  Super Serial Card over TCP.
- MAME only where nothing else emulates the machine: the Apple ///, and the
  UniDisk 3.5 on a //c with ROM 4 or a //c Plus, for any test that
  comes before #163. MAME is launched from a script with the disk and its null modem on
  TCP, and its Lua scripting types at the machine, so its UI is never used.
  Its Apple /// socket connection has a reported bug, so it is checked
  first.
- KEGS, GSplus or MAME for the IIgs serial ports, whichever connects over
  TCP.

1. Block devices on the Apple: a UniDisk 3.5, a //c's 3.5" port and a hard
   drive, through their firmware (M), after step 1.2.
2. Raw capture of copy-protected disks, with Archon as the test disk (L).
3. Virtual drives, #178.
4. Whole disks over cassette audio, as ADTPro sends them (M).
5. Files and disks to tape audio and back, in WAV, MP3 and Ogg (S to M).
6. Serial cards through Pascal firmware entry points, for third-party cards
   (M).
7. The IIgs serial ports (M).
8. An Apple /// client under SOS (L).
9. LANceGS and the original Uthernet, the Ethernet cards ADTPro supports
   besides the Uthernet II (S to M each).
10. Tests on machines the link program may already run on: a IIgs with a
    Super Serial Card, and compatibles such as the Laser 128 (S).

**4. File systems and layouts**

1. DOS 3.2 13-sector disks, and 40- and 80-track DOS 3.3 disks (M), with
   #164.
2. Partition layouts, partition copy and replace, and raw access to a CF
   card: CFFA, DOS.MASTER, DOS hybrids, AmDOS, OzDOS, UniDOS, FocusDrive,
   MicroDrive and Pascal ProFile Manager (M to L), after step 1.2.
3. HFS, the Mac's Hierarchical File System, which GS/OS reads and writes on
   IIgs 3.5" and hard disks, read and write (M to L).
4. SOS, the Apple ///'s file system, read and write (M), with step 3.8.
5. Gutenberg writes, from `feedback-backlog.md`.
6. RDOS, read-only (S).

**5. Tools**

1. A viewer window with previous and next arrows (S to M).
2. Collection tools, from DiskBrowser and DiskM8: a disk list with
   checksums, duplicates across many disks by whole disk and by file, and a
   search inside disk images that includes detokenized BASIC (M).
3. Language tools, from a2kit: tokenizing, detokenizing and renumbering
   Applesoft and Integer BASIC, a BASIC cross-reference of variables, calls
   and jumps as DiskBrowser lists them (S to M).
4. Game data viewers, from DiskBrowser: Wizardry scenarios and Infocom story
   files (M each).

**Elsewhere**: Casso's hard disk card, #101, built beside step 1.2 so that
Casso boots what Casso Explorer writes; 3.5" drive emulation, #163; the
UniDisk 3.5 on the //c's SmartPort bus, #93, which shares #101's block core;
the Z-RAM card, #65, so that Casso runs the CP/M disks Casso Explorer reads
and writes.

**Cut**: the Macintosh formats (MFS, DART, MOOF, MacBinary, Mac TS, Apple
Partition Map, MacPaint), Trackstar, AppleLink PE, ADTPro's DOS ADT client
and half tracks, floppy drives attached to the PC, language servers, and
65816 disassembly.