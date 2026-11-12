# Research: Casso Explorer

**Feature**: 033-casso-explorer | **Date**: 2026-09-10

Every question here is settled by reading the tree. Where the tree
contradicts an assumption the spec was written under, the correction is
stated and the spec's requirement is kept unless the correction makes it
impossible.

## File Explorer's command bar and list, measured

Measured 2026-09-23 on Windows 11, both windows 1600x900 device pixels at
125%, dark theme, the same folder of three files open in each, nothing
selected. Figures are device pixels with the dip equivalent beside them,
read from the captures rather than from any documentation.

### The command bar

| | File Explorer | Casso Explorer |
|---|---|---|
| Bar items, left to right | New (chevron), cut, copy, paste, rename, share, delete, Sort (chevron), View (chevron), overflow | New, cut, copy, paste, rename, delete, Sort, View, overflow |
| First icon center, x | 41 | 32 |
| Icon-only button pitch | 60 px = 48 dip | 46 px = 37 dip |
| Icon ink height | 16 px = 12.8 dip | 15 px = 12 dip |
| Label cap height | 11 px = 8.8 dip | 13 px = 10.4 dip |
| Dropdown chevron | 6x4 px, #B7B7B7, after New, Sort and View | none |
| Disabled icon ink | #616060 | #7B7B7B |
| Right end | Details pane toggle | Preview toggle, Theme |

Both dim the clipboard and file buttons with nothing selected, so the
disabled state itself matches; the ink does not.

### The list

| | File Explorer | Casso Explorer |
|---|---|---|
| List background | #191919 | #191919 |
| Header background | #191919 | #191919 |
| Row pitch | 37 px = 29.6 dip | 38 px = 30.4 dip |
| Row text cap height | 11 px = 8.8 dip | 12 px = 9.6 dip |

### What this asks of Casso Explorer

1. The icon-only buttons sit 11 dip closer together than Explorer's.
2. Nothing marks the buttons that open a menu; Explorer draws a chevron on
   each of the three.
3. Disabled ink is lighter than Explorer's, so a disabled button reads as
   more available than it is.
4. Row pitch is 0.8 dip taller and the text about 1 dip larger.

The departures the spec already allows -- Preview and Theme where Explorer
has its Details toggle, and no Share button, Share being a context-menu verb
here -- are not in that list.

### At 100%, 125%, 150% and 200%

Measured 2026-09-23 with both windows on the portrait monitor, its scale set
to each value in turn and then restored to 150%; 125% is the primary
display. Device pixels.

| | 100% | 125% | 150% | 200% | Rule |
|---|---|---|---|---|---|
| Address strip, fill | 48 | 60 | 72 | 96 | 48 dip, rounded down |
| Command bar, fill and line | 47 | 58 | 70 | 94 | 47 dip, rounded down |
| Line under each strip | 1 | 1 | 2 | 2 | 1 dip, rounded |
| Address box height | 32 | 40 | 48 | 64 | 32 dip |
| Icon-only button pitch | 48 | 60 | 72 | 96 | 48 dip |
| Group separator, width | 1 | 1 | 1 | 2 | 1 dip, rounded down |
| Group separator, height | 32 | 39 | 48 | 62 | 8 dip below the bar's top, 7 above its bottom |
| Label cap height | 8 | 11 | 13 | 17 | a 12 dip font |
| Tree row pitch | 32 | 40 | 48 | 64 | 32 dip |
| List row pitch | 28 | 37 | 43 | 56 | see below |

Casso Explorer now matches every row to within 2 pixels. Its label cap height
is 9 pixels at 100% against Explorer's 8, the same 12 dip font drawn with
different hinting. The command bar starts 5 dip in, and a group gap is 8 dip,
the separator in its middle.

The list row is not one size scaled: 28 dip at 100% and 200%, but taller at
125% and 150%. Five more scales on the portrait monitor gave:

| Scale | 100 | 125 | 150 | 175 | 200 | 225 | 250 | 300 | 350 |
|---|---|---|---|---|---|---|---|---|---|
| Row, px | 28 | 37 | 43 | 51 | 56 | 65 | 71 | 84 | 99 |

Every one is twice 14 dip rounded up, plus one pixel at a scale that is not a
whole multiple of 100%. Casso Explorer's list computes its rows that way.

### The overflow

Measured 2026-09-23 at 125%, narrowing Explorer from 1000 to 200 pixels in
50-pixel steps. Explorer never drops a label. It moves whole buttons into
See more, rightmost first -- View, Sort, Delete, Share, Rename, Paste, Copy,
Cut, then New -- and Details keeps its label at every width. The line before
See more shows only while nothing has moved into it.

Every Dxui toolbar dropped labels first, right to left, and moved nothing.
Every one now overflows as Explorer does, Casso's own included. A trailing
entry, such as Preview or Theme, follows the leading ones once they are all
in See more, so a very narrow window does not cut it off.

The panes shrink in turn as the window narrows: the list first, down to its
minimum; then the preview; then the tree; then all three together. Explorer's
navigation pane holds its width until it reaches about half the window, so
its list gives way first as well. The window narrows to 200 dip, where it
stopped at the three panes' minimums added together, 640 dip. Where
Explorer's Details pane falls in that order was not measured.

Explorer's list shows a horizontal scrollbar once a column is cut off, as
Casso Explorer's does; its navigation pane never shows one, so Casso
Explorer's tree no longer does either.

### The light theme

Measured 2026-09-25 with Windows' apps set to light for the capture and set
back after, both windows 1601x900 on the 125% display and 2100x1300 on the
portrait display at 200%, the session locked, so no pointer and no hover.

| | File Explorer | Casso Explorer before |
|---|---|---|
| Caption and tab strip | #E8E8E8 | #E8E8E8 |
| Line under the tab strip, broken under the selected tab | #DADADA | none |
| Address strip, box | #F8F8F8, #FDFDFD | the same |
| Command bar, and the lines above and below it | #FFFFFF, #D6D6D6 | the same |
| List | #FFFFFF | #FFFFFF |
| Lines between column titles | #E5E5E5 | #D6D6D6 |
| Selected row, pane focused | #CCE8FF, outlined #000000 | #CCE4F7, outlined #5D5D5D |
| Selected row, pane not focused (the tree) | #D9D9D9, outlined #949494 | #CCE4F7 |

The selected row's box is the same in either theme and at either scale: 2 dip
shorter than the row at top and bottom (a 24 dip box in a 28 dip row at 200%,
31 pixels in 37 at 125%), ending 4 dip short of the last column's right edge
rather than running across the list's empty width. A row under the pointer is
#E5F3FF, measured 2026-09-23. A pressed row needs a click and was not
measured.

## R1. Executable shape

**Finding**: `Casso.vcxproj` holds one comment-only `Main.cpp`, one `.rc`,
`EntryPointSymbol=wWinMainCRTStartup`, and references to CassoCore,
CassoEmuCore and Dxui. The real `wWinMain` is `CassoEmuCore/Gui/GuiMain.cpp`.
Its order: DPI awareness, EHM notify and breakpoint hooks, command line,
asset bootstrap, prefs, machine selection, shell initialize, message loop.
COM is initialized inside the shell, not the entry point. Resources are
per module, so a second executable needs its own `.rc`.

**Decision**: `CassoExplorer/CassoExplorer.vcxproj` copies Casso's layout minus the
mesh bake, with `CassoExplorer.rc` holding the icon and the cassowary PNG. The
entry point is `CassoEmuCore/CassoExplorer/CassoExplorerMain.cpp`, following GuiMain's
order with a `CassoExplorerShell` in place of `EmulatorShell`. The solution gains
one project block and twelve configuration lines.

## R2. The intent channel is one-way

**Finding**: `Win32IntentChannel` sends `WM_COPYDATA` by
`SendMessageTimeoutW` to every window of class `CassoWindow`, payload one
intent byte plus a UTF-8 absolute path, at most 4096 bytes. Intents are
`Unstated`, `ReloadInPlace`, `Restart`. No reply exists; `StateIntent`
returns void by contract. The emulator's only identity is the window class.
There is no mutex and no title protocol.

**Decision**: the channel grows two intents and one reply path.

- `InsertDisk` carries the path plus a drive number; the emulator mounts
  it through the same eject-then-mount path the drive widget uses, so the
  flush guarantee holds.
- `DescribeMachine` carries no path; the emulator answers with a second
  `WM_COPYDATA` to the sender's window carrying the machine display name
  and its attached Disk II drive count from
  `MachineConfig::AttachedDiskIiDriveCount`.
- The reply uses a second registered message id so the two directions
  never confuse each other, and the sender identifies itself by passing
  its own HWND as `wParam`, which `WM_COPYDATA` already does.
- Targeting: Casso passes its own HWND on Casso Explorer's command line as
  `--owner <hwnd>`. Casso Explorer sends to that window when it is alive, else
  enumerates `CassoWindow` and picks the most recently active, else
  launches. A second launch from the same Casso is detected by Casso Explorer
  enumerating its own class `CassoExplorerWindow` for a window whose owner
  matches, and fronting it.

**Alternatives considered**: a named pipe. Rejected; the existing channel
is already the protocol both sides speak, and one more message id costs
nothing.

## R3. The prefs file cannot be shared for writes

**Finding**: `GlobalUserPrefs::Save` is a read-modify-write with no lock,
no temp-and-rename, and no external-edit detection. A running Casso
rewrites the whole `global` section from memory on its next save, so
anything Casso Explorer wrote there is lost. The spec's "shared prefs file"
assumption is unsafe for known folders, Casso Explorer's theme, and everything
else Casso Explorer persists.

**Decision**: two small files beside `UserPrefs.json`, each written by
atomic replace and read on demand rather than cached.

- `KnownFolders.json`: the list, with last-used time per entry. Both
  executables read it when they need it and append through one helper in
  core that reads, merges and replaces atomically. Casso's picker reads it
  at open time in place of `DiskMru::DistinctFolders`, then still appends
  the MRU's folders as today so nothing the picker showed before goes
  missing. First-run seeding is the MRU's distinct folders.
- `CassoExplorerPrefs.json`: theme, preview-pane state, host-naming setting,
  placement, open tabs. Casso never touches it. First-run theme seeding
  reads Casso's `activeTheme` under `global` in `UserPrefs.json` read-only;
the top-level `activeTheme` in `GlobalUserPrefs.json` is a decoy the 032
session lost four capture runs to.

**Alternatives considered**: teaching `GlobalUserPrefs` a file lock and a
change watcher. Rejected as a change to Casso's persistence for a
consumer that needs three keys; the spec's own requirement is "shared
preferences", and two files in the same directory satisfy it.

## R4. In-process disk access

**Finding**: `DiskCommandRunner::Run` takes a parsed command line and
returns text plus a payload; `RunList` formats text, not data. Beneath it
`DiskImageSession::OpenImage` yields an `OpenedImage` with the sector
buffer and its kind, and `IVolume` (`Dos33Volume`, `ProDosVolume`) gives
`Enumerate` into a `VolumeListing` of `FileEntry` structs with name, type,
size units, EOF, load address, aux type, locked and directory flags. Every
mutator is const and returns a new sector buffer; `SaveAndCommit` writes it
back atomically with a staleness stamp and an in-use refusal. `FileEntry`
carries no modified date; neither file system's date is surfaced.

**Decision**: the browser model uses `DiskImageSession` and `IVolume`
directly for catalog, read, write and delete, and calls
`DiskCommandRunner` for the verbs that only exist there: create, init,
boot, sector and block read and write, building a `CommandLineOptions`
in-process exactly as the CLI parser would. Byte identity with the CLI
(SC-001) follows because both paths end in the same `IVolume` and
`VolumeImage::Save`. The modified column is host-file-only; catalog rows
leave it blank, which the spec's "where the file system records it"
already allows.

## R5. Decoders the tree lacks

**Finding**: `ApplesoftTokenizer` has `Tokenize` and `Detokenize` with a
structured error. There is no Integer BASIC codec and no disassembler.
The 6502 tables expose mnemonic and addressing mode per opcode and
`OpcodeTable::GetOperandSize`, so a disassembler is a short loop over
them.

**Decision**: add `IntegerBasicDetokenizer` beside `ApplesoftTokenizer` in
CassoCore, and `Disassembler` in CassoCore over the existing tables, both
pure and tested. No Integer tokenizer, per the clarification.

## R6. Picture preview

**Finding**: `AppleHiResMode`, `AppleDoubleHiResMode` and `AppleLoResMode`
render from a caller-supplied `videoRam` span into a BGRA framebuffer and
touch their `MemoryBus` only when the span is null. `DxuiTextRenderer::
DrawFramebuffer` paints such a buffer. `IDxuiPainter` has no bitmap call.

**Decision**: `PicturePreview` places the file's bytes at its load address
inside a 64 KB scratch span, picks the mode from the spec's address and
length rule, renders once into a cached framebuffer, and a small
`DxuiFramebufferView` control paints it through `DrawFramebuffer`. Double
hi-res splits the 16 KB file into main and aux halves per the mode's
setters.

## R7. Dxui widget gaps, corrected

**Finding**: `DxuiTreeView` is recursive to any depth, contrary to the
spec's assumption, but it draws a checkbox on every row, identifies rows
by label, and flattens the whole tree eagerly with no lazy children.
`DxuiListView` is virtual-capable with columns and a sort indicator, but
single-select and no sorting. `DxuiTabStrip` exists. There is no splitter,
no status bar, no multi-line text view, no image control, and drag is
drop-only.

**Decision**: library additions, each general:

- `DxuiTreeView`: an id per node, a lazy-children callback fired on first
  expand, and a per-tree flag that hides checkboxes. The capability
  checklist keeps its behavior with the flag off.
- `DxuiListView`: multi-select with Ctrl and Shift, and an on-sort callback
  so the consumer reorders rows; the widget keeps indicator-only.
- `DxuiSplitter`: a sash between two docked controls, keyboard-resizable.
- `DxuiStatusBar`: fixed and stretch fields with text.
- `DxuiDragDropSource`: `IDropSource` plus a data object over a list of
  formats, beside the existing `DxuiDragDropTarget`.
- `DxuiFramebufferView`: paints a BGRA buffer.
- Listing and hex previews use a virtual `DxuiListView`, one row per line,
  rather than a new text view.

## R8. Theme following

**Finding**: `DxuiHwndSource` already handles `WM_SETTINGCHANGE` for
`ImmersiveColorSet` and refreshes `DxuiWindowsThemeColors`. A window's
theme is a non-owning pointer swapped by `SetTheme` plus invalidate.
`CassoTheme::MakeByName` yields any of the three Casso palettes without
the theme manager.

**Decision**: Casso Explorer owns a `CassoExplorerTheme` object per choice: Light and
Dark are two new palettes in Dxui built from the Fluent tokens
`DxuiWindowsThemeColors` already carries; Follow system picks one on the
settings-change notification; the three Casso choices come from
`CassoTheme::MakeByName`, with Skeuomorphic used as colors only because
Casso Explorer has no desk scene to draw.

## R9. About dialog image

**Finding**: `DialogDefinition` supports a fixed icon enum, text runs and
hyperlinks, no arbitrary bitmap. `PngCodec::DecodeRgba` exists.

**Decision**: `DialogDefinition` gains an optional image, RGBA bytes plus
size, painted by `DialogBodyContent` via `DrawIconBitmap`. The cassowary
ships as RCDATA in `CassoExplorer.rc`. Casso's About is untouched.

## R10. Launching processes

**Finding**: core has no `CreateProcess` seam; the only launches are
`ShellExecuteW` for folders and URLs. `CommandLineParser` accepts
`--disk1`, `--disk2` and `--title` with both prefixes.

**Decision**: `Seams/IProcessLauncher` with a Win32 implementation and a
fake for tests. Casso's Casso Explorer menu item launches `CassoExplorer.exe` from its
own module directory with `--owner <hwnd>`. Casso Explorer's Open in new Casso
launches `Casso.exe` with `--disk1 <quoted path>`.

## R11. Host-side drag formats

**Decision**, no prior art in the tree:

- Private format `CassoExplorerCatalogEntries`: source image path, file-system
  kind, and the entries' paths, so an Apple-to-Apple drop copies through
  `IVolume::Read` and `Write` with no conversion.
- `CFSTR_FILEDESCRIPTORW` and `CFSTR_FILECONTENTS` with delayed rendering,
  names per the host naming rule, so Explorer and Casso Explorer host folders
  receive converted files on demand.
- `CF_HDROP` accepted on disk-image targets; each path goes through the
  content rule from the clarifications.
- `CF_HDROP` offered for a dragged disk image so Casso's existing
  `DxuiDragDropTarget` accepts it unchanged.

## R12. Testing shape

**Finding**: the suite tests decisions extracted into pure classes and
leaves `EmulatorShell` nearly untested. Mocks exist for `IDiskFileIo`,
`IFileSystem`, and the three Dxui paint interfaces.

**Decision**: every decision in Casso Explorer lives in a model class under
`CassoEmuCore/CassoExplorer/Model/` with no window: `BrowserModel` (locations,
tabs, selection), `CatalogModel`, `PreviewDecoder`, `HostFileNaming`,
`ContentSniffer`, `KnownFolderStore`, `CassoTargeting`, `DragPayload`.
`CassoExplorerShell` is a thin window over them. New Dxui widgets get headless
tests on the mocks.

## R13. File Explorer's tabs

**Finding**: measured 2026-09-14 from two Explorer windows on Windows 11 at
120 dpi in the dark theme, one with a single tab and one with eight, whose
tabs had shrunk until the scroll arrows showed. Figures are pixels at 1.25x
and the DIP they come to.

- A lone tab is 303 px, 240 DIP, wide. Crowded tabs repeat every 125 px, so
  100 DIP is the minimum before the strip scrolls.
- Tabs begin 11 px, 9 DIP, below the top of the strip and run 41 px, 33 DIP,
  down to the row below, which starts at 52 px.
- The selected tab is filled with the row below's color (`#2C2C2C` against a
  `#202020` strip) and joins it: its top corners are rounded, and its bottom
  corners curve outward into the row below. Unselected tabs have no fill and
  a 1 px divider (`#323232`) between neighbors.
- The icon is 16 DIP, 10 DIP in from the tab's left edge. The label starts
  38 DIP in, left-aligned: `#FFFFFF` and semibold on the selected tab,
  `#CCCCCC` and regular on the rest.
- The close glyph is about 8 DIP wide, centered 22 DIP from the tab's right
  edge, on every tab.
- Scroll arrows are solid triangles at each end of the tabs; the + glyph,
  about 9 DIP wide, follows the last tab, or the right arrow when they
  overflow.
- The row below, holding navigation and the address bar, is 59 px, 47 DIP,
  tall, with a 40 px, 32 DIP, address box. A 1 px line then separates the
  46 DIP command bar.

**Decision**: `DxuiTabStrip` takes these metrics and shape, the selected
tab's fill supplied by the host as the color of the row it joins.

## R14. Apple text previews

**Finding**: a disk text file previewed in the hex view's character mode
reads as a block of characters rather than lines, and a listing exported
to the host in plain ASCII opens in the hex view like any ASCII file.

**Decision**: a disk text file whose bytes are text is split into lines,
the high bit cleared, a return ending each line and zero bytes dropped;
one whose bytes are not text stays in the hex view. A host file whose name
ends in an export suffix is read as text whatever its character set. Both
are drawn in `DxuiTextView` in the theme's monospaced face.

**Alternatives considered**, both built on 2026-10-13 and dropped at the
owner's request:

- The text drawn through Casso's CRT pass on the picture monitor, a band
  of rows at a time in `DxuiFramebufferView`, since a program drawn whole
  is taller than a texture can be. It could not be selected.
- A TrueType font made at run time from the installed video ROM, each lit
  dot a square, registered for the process alone, drawn in the monitor's
  color. It was selectable and redistributed nothing, but the owner chose
  the theme's face.

A face built from the built-in table was dropped in the 2026-09 pass, since
the table is a partial copy of Apple's ROM.

## R15. ADTPro's protocol, from its published document

**Finding**: adtpro.com/protocol.html, version 2.1.0, defines:

- **The envelope**: `'A'` ($C1), a two-byte payload length, a command byte,
  and an EOR of those four bytes; then the payload and, when the payload is
  not empty, an EOR of the payload.
- **The command bytes**: CD $C3, Directory $C4, Get $C7, Put $D0, Batch
  $C2, Nibble $CD, Multiple $CC, Size $DA, Send Image Data $D3,
  Acknowledgement $CB, Summary $D9, Home $D8, Send File $C6.
- **Virtual drives**: their own `'E'` ($C5) header, with mode, block number
  and EOR, over 512-byte blocks.
- **Acknowledgements**: ACK $06, NAK $15 (with a block number in Get), and
  CAN $18 and ENQ $05 for nibble analysis.

Every exchange starts at the client.

**Not in the document**:

- the byte order of the envelope's length field (the Summary example
  implies LSB first);
- the CRC's polynomial, initial value and bit order;
- the RLE encoding of image pages;
- the scope of the virtual drive's block EOR;
- timeouts and retry counts;
- the Nibble packet layout.

**Decision**: implement from the document, and settle each gap by recording
traffic between ADTPro 2.1.0's own host and its own client, both
unmodified:

- the host from the owner's install (`Downloads\ADTPro-2.1.0`);
- the client from its distribution disk, booted in an emulated //c;
- the two joined by Casso's TCP bridge (R18), ADTPro's serial-over-IP
  transport being the client end for emulators.

The recordings become test fixtures. ADTPro's GPL source is never read
(FR-096); its documentation and its observable behavior are fair inputs.

**Alternatives considered**: reading ADTPro's source to fill the gaps,
which the clean-room rule forbids; guessing the CRC from common 16-bit
variants, which a recording settles in minutes and without risk.

**Settled (2026-10-15)**: captured with ADTPro 2.1.0's server in its
serial-over-IP mode and its client booted in an emulated //c, the captures
in `UnitTest/Fixtures/CassoLink/`. Each gap above is now settled in the
contract:

- **Length**: LSB first; in Send Image Data it is the length after decoding.
- **CRC**: CRC-16/XMODEM (polynomial $1021, initial value 0, not
  reflected) over the decoded data, LSB first. It matches all 280 packets in
  the send and receive captures.
- **Encoding**: each byte is the difference from the previous decoded byte,
  and a 0 is followed by an offset within the page up to which the previous
  byte repeats. Every packet decodes to the source image's blocks, and the
  image ADTPro's server wrote from the //c was identical to its source.
- **Virtual drives**: the EOR covers the 512 block bytes alone; the driver
  reads with Read3 only; and the server's reply to a write ends with the
  block's check, not the header's EOR as the document has it.
- **Timeouts and retries**: the client waits about 2 s; the server resends a
  NAKed packet twice, after 0.26 s and 0.52 s, then ends with Home. With the
  server's ACKs dropped, the client resends with no limit, under the same
  block number but with later blocks' data, until its memory is overwritten.
- **Replies**: a host receiving data replies with one unframed byte, a
  client with an Acknowledgement packet.

The Nibble packet layout remains open: it is Phase 23's, after the protected-
disk follow-on, and none of the captures contains it.


## R16. The PC drives inside a framing where the client speaks first

**Finding**: every ADTPro exchange begins at the Apple, and the host only
replies. In a PC-driven link the PC chooses the next operation, and must do so without
breaking that rule, so that ADTPro's clients and Casso's client share one
host.

**Decision**: Casso's Apple program polls. It sends a new command, Ready
(`'R'`, $D2, outside ADTPro's set), with its version and the drives it has.
The host's reply is the next operation, or Idle; on Idle the client sends
Ready again after a short pause. The operations are new commands in the
same envelope, also outside ADTPro's set:

- list drives;
- read sectors and write sectors (a track and a list of sectors);
- read and write a whole track (formatting it);
- report status;
- load program.

Data moves in a Sector data packet of Casso's own: 256 bytes unencoded and a
CRC-16/XMODEM, acknowledged with ADTPro's ACK and NAK. Casso's client so
depends on none of ADTPro's undocumented details. The
Apple program does no file-system work: catalogs, files, deletes and
renames are all sector reads and writes the PC computes (R20).

The host distinguishes the two kinds of client by the first command:

- Ready means Casso's client;
- anything in ADTPro's set means ADTPro's client, served as ADTPro's host
  serves it.

The contract is in [contracts/cassolink.md](contracts/cassolink.md).

**Alternatives considered**:

- **A new protocol with the PC speaking first.** It breaks ADTPro
  compatibility, which the owner chose.
- **File-system commands on the Apple.** They require DOS 3.3 and ProDOS code
  in 6502, against the owner's choice of a thin Apple side. They also duplicate `IVolume`,
  which Casso already has.

## R17. The Apple side: shim, program and disk access

**Decision**:

- **Two pieces.** A shim of under one page, resident at $0300, contains the
  6551 driver and a loader for `load program`. The program it loads,
  linked at $0800, contains:
  - the poll loop;
  - the status screen on the text page;
  - Casso's own 5.25-inch disk routines: read and write a 6-and-2 sector,
    format a track, seek with the arm's phase sequence. They are written
    from Apple's published Disk II documentation and the 6-and-2 encoding
    `NibblizationLayer` already implements on the PC side.
- **No DOS or ProDOS in memory**, so all 64 KB but the shim and the text
  page is free for track buffers.
- **Updates.** The shim sends Ready with the program's version; the host
  replies `load program` when it differs (FR-091). Save link to disk writes
  a boot disk: a boot sector that loads the shim and calls it, so even the
  disk copy fetches the program from the PC.
- **Speed.** The //c's 6551 is polled, not run on interrupts. At 115,200
  baud a byte arrives every 87 microseconds, about 89 cycles at 1.023 MHz.
  That is enough for a tight polled loop that stores to a buffer, which
  ADTPro's client demonstrates on this machine.

**Alternatives considered**: the program resident in full, with no shim,
which requires a disk update on every Casso release; a ProDOS SYS program,
which brings ProDOS into memory and a ProDOS disk to start from.

## R18. Joining an emulated serial port to the PC

**Finding**: `Acia6551` delivers a received byte through `ReceiveByte` with
no FIFO. A second byte before the guest reads the first sets Overrun.
Transmit calls `IAciaEndpoint::OnByteTransmitted` at once, ignoring the
baud rate. The //c's ports are wired to `AciaLoopbackEndpoint` in
`MachineBuilder.cpp`. There is no host serial or socket code in the tree.

**Decision**:

- **A bridge endpoint.** A new `AciaBridgeEndpoint` implements
  `IAciaEndpoint` and owns a pacer. It passes the 6551 one byte at a time,
  never sooner than one character time at the baud rate set in the chip after
  the last. A byte the guest has not read when the next arrives is overrun,
  so a guest that reads each byte in time receives every byte and one that
  falls behind overruns, as on a real //c (FR-095).
- **Paced transmit.** Bytes the guest transmits go out at the same rate, so
  the transmit-empty flag clears and sets at real timing for the guest.
- **Clock.** The pacer counts machine cycles, not wall time, so its timing
  is correct under the debugger's pauses and at any emulation speed.
- **What the bridge talks to.** The bridge's other side is an
  `IByteStream`, with three implementations:
  - in-process, a pair of queues, used when the emulated machine and the
    link run in one process, as in every test;
  - TCP, listening on a port for Casso Explorer's link, ADTPro's
    serial-over-IP transport or a second Casso (FR-097: this PC only, except
    for the Wi-Fi modem transport). Casso and Casso Explorer are separate
    processes, so they always join this way, on 127.0.0.1;
  - a COM port, joining an emulated machine to real hardware.
- **Settings.** The endpoint for each port is a machine setting in
  `MachineBuilder`; loopback stays the default.

**Alternatives considered**: a receive FIFO in `Acia6551`. The real chip
has none, and with a FIFO a guest that falls behind gets no overrun,
hiding bugs that appear on a real //c.

## R19. Host transports

**Decision**: one seam, `IByteStream` (open, close, write, read with a
timeout, readable count, a "closed" signal), with the three
implementations above:

- `Win32ComPort` uses `CreateFile` on `\\.\COMn`, `SetCommState` for speed
  and handshaking, and `SetCommTimeouts`.
- `Win32TcpStream` uses Winsock. It binds to loopback unless the Wi-Fi
  modem transport is chosen (FR-097).
- The in-process pair is a pair of queues.

`IByteStreamFactory` lists COM ports and opens each kind of stream, so the
COM search runs against a fake factory in tests. The decisions the Win32
streams make are pure functions in `ByteStreamSettings`, tested without a
port or a socket (constitution II): a speed, data format and handshake to
the COM port's settings, and the transport to the address a TCP port binds
to.

All three are in `CassoEmuCore/Seams/`. The protocol engine uses only
`IByteStream`, so unit tests drive it with an in-memory pair and touch no
port or socket (constitution II).

The COM search (FR-083a) lists ports with `GetCommPorts` behind the same
seam. It opens only ports that open exclusively, sends the Ready probe's
reply-request at the default speed, and closes each port it has opened
before trying the next.

**Alternatives considered**: a third-party serial library, which the
dependency rules rule out; overlapped I/O with completion ports, which is
more than one link at a time requires.

## R20. Browsing an Apple disk without reading all of it

**Finding**: `Dos33Volume` and `ProDosVolume` work on a whole image in
memory, and a whole 140 KB disk takes about 12 seconds at 115,200 baud.
That is too long before a catalog appears.

**Decision**:

- **A sparse disk.** `LinkedDisk` is a 140 KB image in memory with a map of
  which sectors are present.
- **Reads fault in.** The volume classes read sectors through an accessor.
  For a `LinkedDisk`, a read of an absent sector adds it to a miss list and
  returns zeros. The operation (enumerate, read a file, write a file) runs;
  if the miss list is not empty, the PC fetches those sectors in one request
  and runs it again. It is done when a run adds nothing to the list.
- **Prefetch.** A catalog is the VTOC and track 17 for DOS 3.3, or blocks 2
  to 5 and the directory chain for ProDOS. Each is fetched in one or two
  rounds; a file is its T/S list or index blocks, then its data.
- **Writes.** A write runs the volume's own write on the sparse image,
  fetches what it read first, and sends only the sectors it changed. The
  catalog is read again before every write, so a disk changed since it was
  listed is reported, not written over (edge cases).

This changes the volume classes in one place: sector access goes through
one accessor instead of indexing the buffer, a behavior-preserving
refactor the existing volume tests cover.

**Alternatives considered**: reading the whole disk on first open, which is
too slow (SC-028); file-system code on the Apple (R16); a separate
file-system walker on the PC, which duplicates `IVolume`.

## R21. Casso Explorer integration

**Decision**:

- **Tree and locations.** A new `Location::Kind::AppleDrive` and
  `TreeNode::Kind::CassoLink` / `AppleDrive`, with ids `link:<name>` and
  `link:<name>|<slot>,<drive>`. The links are listed under This PC's
  siblings, as a root per link (FR-083).
- **Settings.** Links are stored in `CassoExplorerPrefs.json` (name,
  transport, port or address, speed, handshaking).
- **Background work.** An `CassoLinkSession` per open link runs on its own
  thread, and owns the `IByteStream`, the protocol engine and the
  operation queue (FR-098). It posts progress and results to the window,
  as the shell listings do. The window never waits on the line.
- **Copies.** Copying out and in reuses `DragPayload`, `HostFileNaming` and
  the put rules, over a `LinkedDisk` instead of an image. Read
  disk to image and Write image to disk are verbs on an Apple drive's node
  and its menu.

**Alternatives considered**: an Apple drive presented as a disk image in a temp
folder, which hides the live connection and its errors; a separate
transfer window, which the owner's File Explorer model rules out.

## R22. Building the Apple-side program

**Finding**: CassoCore's assembler is linked into CassoEmuCore, and
`ImageArtifactSink` and `DirectBootBuilder` write assembled code onto a
disk image. A build step running `CassoCli` to assemble the program cannot
feed CassoEmuCore, since `CassoCli` itself links CassoEmuCore.

**Decision**:

- **Source in the tree.** The shim and the program are AS65 source under
  `CassoEmuCore/CassoLink/Apple/`, compiled into the core as text
  resources. The core assembles them with CassoCore's assembler when a link
  starts.
- **The build check.** A unit test assembles both on every build and fails
  on any error, and checks that the program's version matches the core's.
  That makes the build the check FR-096 requires, with no build-order
  cycle.
- **Versions and boot disks.** The version is a constant both sides read,
  so a Casso release bumps it once. Save link to disk uses
  `DirectBootBuilder` for the boot sector and shim.

**Alternatives considered**: checking in assembled binaries, which drift
from their source; a prebuild step with a separately built assembler,
which adds a project and a build order for no gain.

## R23. Bootstrap over serial

**Finding**: ADTPro bootstraps a //c this way. The user types `IN#2` and
then Ctrl-A `14B`, which sets 9,600 baud (on a //c, with no Return).
The host then sends text at 9,600 baud, pausing 250 ms after each line.
ADTPro's page does not document the text.

**Decision**: Casso types into the Apple's monitor:

1. `CALL -151`;
2. the shim as hex lines (`300:` followed by 8 bytes per line);
3. `300G`.

After 250 ms per line, the shim runs, sends Ready at 9,600 baud, receives
the program at 9,600 baud, and the program switches both ends to the
chosen speed with a speed-change operation the host acknowledges. The
//c's serial firmware commands come from the //c Technical Reference
(already transcribed under the session's reference texts) and are
confirmed on an emulated //c running its own firmware before the real
one.

Audio bootstrap for a ][+ or //e waits for the Super Serial Card or
Uthernet II feature to merge, since a ][+ or //e has no link without one.
Tests send the loader through Casso's cassette emulation (spec 037, on master).

**Alternatives considered**: typing an Applesoft loader with POKEs, which
is several times longer than hex at 9,600 baud.

## R24. The Apple's screen while the link runs

**Finding**: The shim fills page 3 from $0300 to $03EF; above it are the
monitor's vectors, so it cannot grow there. The program occupies $0800 to
about $11FF and its buffers start at $4000, leaving $1200 to $3FFF unused.
The program's status screen has the connection, drive, track, operation and
progress, and the program has no clock: time on the Apple is counted in
polling loops.

**Decision**:

- **The shim moves to $3D00**, with up to 768 bytes, below the program's
  first table at $4000. It writes its own lines on the text page: a title,
  `WAITING FOR THE PC ON PORT 2 AT <speed>`, `LOADING PROGRAM <n>%` as each
  page arrives, and `STARTING VERSION <n>`. The typed bootstrap grows to
  about 14 seconds at 9,600 baud, within SC-033.

  **Update (2026-10-26)**: the 6551 shim moved again, to $3B00 beside the
  W5100 shim, for room to name its interface and the target a modem dials.
  Its copy of the zero page is at $3A00 and the slot finder at $3AA0
  (spec Clarifications, Session 2026-10-26).
- **The shim's speed is a data byte** at a fixed offset, the 6551 control
  byte. Save link to disk and the emulated link's boot disk write it at the
  link's speed; for the typed bootstrap it is 9,600 (FR-094).
- **The program counts its polls.** It sends Ready and waits about a second
  for a reply, sending Ready again on silence. A reply sets the PC line to
  `CONNECTED`; each unanswered second adds one to `NO REPLY FOR <n> S`.
- **The last error stays** on its own line, as text from the Status error
  code, until the next operation succeeds.
- **A label from the host.** A new command, Label ($CE), has up to 38
  characters of text, such as `COPYING HELLO TO DRIVE 2`; the program
  prints it on the operation's line and sends Ready. The host sends one ahead
  of each operation the user starts. An ADTPro client never receives it.

**Alternatives considered**: the shim's text fit into page 3 by shortening
its code, with no room for a percentage; a label inside each Read
and Write packet, which changes commands the scenario tests already cover.

## R25. Finding a running Casso

**Finding**: Casso connects an emulated serial port to TCP in
`MachineHost::ConnectSerialPort`, listening on 127.0.0.1 at the port the
machine's setting specifies. Casso Explorer already finds running Cassos for
Insert into drive, by their windows, but no file indicates which of them has
a serial port on TCP.

**Decision**:

- **A file per connected port.** When `ConnectSerialPort` opens a TCP
  listener, Casso writes `%LOCALAPPDATA%\Casso\Links\<pid>-<port>.json`
  with the process id, the process's start time, the machine's name, the
  serial port's number and the address. Casso deletes it when the port is
  connected elsewhere and when it exits.
- **Casso Explorer reads the folder** at start and when it changes, through
  a change notification on that folder. A file whose process is gone, or
  whose process started at another time, is ignored and deleted, so a crash
  results in no root.
- **Each record is a root**, "Casso //c (port 6502)", with an Emulated
  link's settings and no entry in the preferences. A record for port 1 is
  listed with the reason, since the link program uses port 2.
- **An Emulated link's address defaults to 127.0.0.1:6502** in the dialog,
  for an emulator that is not Casso.

**Alternatives considered**: probing 127.0.0.1 ports, which finds only
what is already set up and connects to programs that are not Casso; a
pipe per Casso, which every Casso Explorer would poll.

## R26. The link dialog and the Wi-Fi modem's address

**Decision**:

- **Only the transport's settings.** Serial: COM port (empty to search),
  speed, bootstrap speed, handshaking. TCP: address. Emulated: address.
  Wi-Fi modem: port, speed, handshaking. Changing the transport hides and
  reveals the rows and resizes the dialog.
- **The search opens each port at both speeds**, the bootstrap speed and then the link's,
  on each port (FR-083a), about two seconds a port.
- **The dial address.** A Wi-Fi modem link's root lists
  `ATDT <pcname>.local:<port>`. Windows resolves a `.local` host by mDNS from
  Windows 10 on.

## R27. The Add connection dialog and a connection's name

**Decision**:

- **The transport first**: Serial, TCP, Wi-Fi modem or ADTPro-compatible
  server, then only the rows it uses (FR-083c). Serial: COM port, bootstrap
  speed, speed, handshake. TCP: address. Wi-Fi modem: speed, handshake, and
  a description of the modem with no port field. ADTPro-compatible server:
  COM port or TCP address, speed, handshake for serial, and the folder it
  shares.
- **No name field.** The name is made from the settings: `Serial (any
  port)` or the COM port, `TCP <host>:<port>`, `Wi-Fi modem on port 6400`,
  `ADTPro server on COM3` or `ADTPro server at <host>:<port>`. An Uthernet
  II's announcement is listed as `Uthernet II at <address>:<port>`, never
  saved. A connection with an existing one's name is not added, since the
  name is made from every field in which two connections differ.
- **The COM port** is a drop-down of `Any` and the ports Windows lists.
- **No Apple-side port or slot** for any transport (R29).
- **Info tips**: the bootstrap speed's indicates it is only for the loader typed
  in through `IN#`; the handshake's recommends a first attempt without it
  and notes that many null-modem cables do not connect RTS and CTS, or loop
  each end's back, and that a //c's 6551 has CTS tied to ground.
- **An empty TCP address** has no error displayed; Add is disabled until it parses.
- **The Wi-Fi modem** listens on TCP port 6400. A port in use is the root's
  error, and the root lists the address the boot disk dials.
- **No Emulated transport.** Running Cassos are listed with no setup
  (R25), and a Casso on another PC is a TCP connection. A saved `Emulated`
  link is dropped when the links are read.

**Rationale**: every field of the old dialog that the PC or the
Apple can find for itself is gone, and a name made from the settings
cannot disagree with them.

**Alternatives considered**: a name field defaulting to the made name,
not chosen, at the owner's request; an editable modem port,
not chosen, because the boot disk has the port built in, so a change breaks
every disk made before it.

## R28. The ADTPro-compatible server

**Decision**: a connection type of its own, on a COM port or a TCP address,
with the folder it shares. `CassoLinkHost` serves ADTPro's requests only
on a connection of this type, and every other connection serves only
Casso's own client. `CassoLinkFolder` resolves each path a request has (CD,
Directory, Size, Get, Put, Batch, virtual drives) to its full form with
`GetFinalPathNameByHandle` on the folder and `GetFullPathName` on the
request, and fails a request whose result is not the folder or inside it,
with ADTPro's error reply. The connection's root has a virtual help item,
previewed from text in the program; opening it or dragging it out writes it
to `%TEMP%\Casso\` first. `CassoExplorerPrefs::emulateAdtPro`, the Options
dialog's CassoLink page, `ApplyAdtProSettings`, `StartLinksForAdtPro` and
`GetAdtProFolder` are removed; `emulateAdtPro` in an existing file is
ignored.

**Rationale**: serving ADTPro becomes a connection the user adds, as any
other, instead of a global mode that opened every port; and a server that
writes the files an Apple sends has to be confined to one folder.

**Alternatives considered**: keeping the global mode with a folder picker,
not chosen, at the owner's request; confining by string prefix, not chosen because `..`
and junctions defeat it.

## R29. The Apple side finds its own port

**Decision**: the shim takes its slot from the header when the boot disk's
stage started it, else from the input hook's high byte at $39 when it is
$C1 to $C7, which `IN#n` set before the loader was typed in, else from the
first slot whose firmware has the Super Serial Card's identification bytes,
which is the case after the cassette loader's `3D00G`. The 6551's registers
are at $C088 plus 16 times the slot on a card and on a //c alike, port 1
being slot 1 and port 2 slot 2.

**Rationale**: the user typed the slot already, as `IN#2`; a second field for it is
a second chance to disagree.

**Alternatives considered**: the PC typing the slot into the header,
not chosen, because it requires the dialog's slot field.

## R30. A cable that does not carry CTS

**Decision**: a serial connection with hardware handshake on watches
`GetCommModemStatus` when a write does not finish within its 2-second timeout. CTS low then fails
the connection with `The PC's CTS line is off, so the cable may not
connect it. Turn off hardware handshake.`

**Rationale**: with output flow control on, Windows suspends every write while
CTS is low, so such a cable produces a link that hangs with no reason.

**Alternatives considered**: turning handshake off automatically, not chosen
because the user chose it.

## R31. The Uthernet II boot disk's MAC address and detection

**Decision**: the stage adds into the MAC address's last three bytes the
monitor's RNDL and RNDH ($4E and $4F), which count while the monitor waits
for a key, and a 16-bit sum of $4000-$BFFF, which differs between machines
and power-ons. A slot's W5100 is read first: its mode register at $C084
plus 16 times the slot must read with bits 6, 5 and 2 clear; only then is
the address pointer written and read back.

**Rationale**: two Apples booted from copies of one disk on one network
would otherwise share a MAC address; and a write to a card's registers is
not harmless on every card a slot can have.

**Alternatives considered**: a MAC address chosen by Casso Explorer per
disk, not chosen, because copies of one disk share it.

## R32. Live serial connectors and the Super Serial Card's printer mode

**Decision**: a connector's attachment change goes to the CPU thread as
`IDM_SERIAL_CONNECT` (a //c port or a card's slot, the device, and a file's
path) or `IDM_PRINTER_PARALLEL`, and `MachineHost` replaces the endpoint
between instructions. The connectors whose new device is not the printer are sent
before the one that takes it, and the printer's drain is started before
it. Choosing the printer on a card sets SW1-5 off and SW1-6 on and saves the
card's SW1 bank as `sw1BeforePrinter`; moving it away restores SW1-5 and
SW1-6 from that bank if the card is in printer mode. Each change to another
tab's setting is reported in a message box.

**Rationale**: none of these requires a rebuild of the machine, and a reboot ends the
user's program.

**Alternatives considered**: keeping the reboot for the printer, not chosen
by the owner.

## R33. Splitting the large classes

**Decision**: each class past about 2,000 lines of .cpp is split into
component classes, built as Casso's shell components are: a class with a
reference to the class it comes from, a friend of it, which has the state
it uses. The class map is in plan.md's Component split.

**Rationale**: `CassoExplorerWindow.cpp` is 17,455 lines, and its header
declares about 155 members. With the .cpp split alone, every member is in
one class, and a change in one area means reading the whole header.
Components have only their area's state, and unit tests can call the logic
in them. The split comes before the final manual passes, so they are run
once.

**Alternatives considered**: one class across several .cpp files, as
`EmulatorWindow.cpp` and `EmulatorWindowInput.cpp` are, faster and with
less risk, but rejected by the owner (Session 2026-10-26). Leaving the
files as they are fails SC-037.

## R34. Full Send

**Finding**: the PC starts every link operation, and the Apple-side program runs one at a time, so one drive transfers while the other is idle and can be swapped. No DOS 3.3, ProDOS or Disk II signal reports a disk change.

**Decision**:

- **Start and end**: F on the Apple starts a session and Q ends it, as the user is at the Apple feeding disks. The folder and format are set in Options. Esc is ignored in a session, as a second press would end CassoLink. F during another operation starts the session when that operation ends, which is simpler than a refusal and loses nothing.
- **New disk**: a SHA-256 of track 0 and the catalog track (track 17 for DOS 3.3; ProDOS blocks 2 to 5, which are on track 0). A disk that matches on two reads and is absent from the manifest is new. A read with no address field is an empty drive.
- **Other disks**: track 0 and the DOS 3.3 catalog track are the fingerprint for Pascal, CP/M and unknown disks. Space forces a send on a false match.
- **Duplicate**: a SHA-256 of the whole image. Use a SHA-256 already in the core if one exists, and Windows CNG otherwise.
- **Order**: the other drive, then the drive just read, then Wait for key. No drive spins during the wait.
- **Reads**: each fingerprint track is read twice before the arm moves. Tracks read whole are not read again for the image, and the catalog track, or track 0 on ProDOS, is read again after the image to catch a swap.
- **Manifest**: `full-send.csv` in the destination folder, in UTF-8, one row per disk, rewritten through a temporary file after each disk.
- **Image files**: `NNNN NAME.ext`, with four digits and the title cleaned by the host file name rules. A DOS 3.3 name is the first program in the catalog, as the boot sector's startup name is not stored at a standard place across DOS versions.
- **Sound**: the program clicks $C030 with fixed patterns between requests, never during a disk read.

**Rationale**: fingerprints cost a fraction of a disk read, and rereading both tracks catches a disk that is not seated. The manifest is plain text so that a user can read it and it survives a crash.

**Alternatives considered**:

- **The write-protect line as a swap signal.** It changes as a disk slides past the sensor, but only a polled, selected drive reports it, and polling would spin a drive during a swap. It is a possible later refinement after a test on real drives.
- **Labels typed on the Apple.** The keyboard has a one-character latch that cannot be polled during a disk read. The owner ruled this out.

## R35. File type decoders and converters (User Story 18)

**Finding**: CiderPress II's format notes (`ndocs/formatdoc` in fadden/CiderPress2, Apache License 2.0) cover every format in FR-156 except VisiCalc. Each note cites the primary documents it was written from: Apple's File Type Notes and IIgs Technical Notes, the programs' manuals, and disassemblies. LZ4FH ($08/$8066) is documented by fhpack, its author's packer. Gutenberg documents are on disks in Gutenberg's file system, which Casso has no volume class for. Business BASIC is the Apple ///'s, found on ProDOS volumes as type $09.

**Decision**:

- **Sources**: the formats are written from the File Type Notes, the IIgs Technical Notes and the manuals first, and from CiderPress II's notes where those are silent. Its code is Apache-licensed and may be read; it is not copied, so Casso has no license notice to add. GPL code, such as ADTPro's and the original CiderPress's converters, may be read for a format's behavior and is never copied or paraphrased.
- **VisiCalc**: the storage chapter of the 1982 VisiCalc User's Guide on bitsavers, and files saved by VisiCalc run in Casso as the fixtures. A saved sheet is a DOS text file of entries, as replayed by a load. Each entry's coordinate and contents are read, and formulas are written as entered, since the file has no computed values.
- **Gutenberg**: a read-only `GutenbergVolume` behind `IVolume`, from CiderPress II's Gutenberg notes. Writing to a Gutenberg disk is out of scope.
- **Identification**: one table of formats, matched on the volume kind, ProDOS type and aux type, DOS 3.3 type, name prefix or suffix and size, followed by each format's content check. This is the table in [contracts/file-types.md](contracts/file-types.md).
- **P-code**: Apple Pascal's p-machine opcodes, from the Apple Pascal Operating System Reference Manual and the p-System's published instruction set, for the p-code version a codefile's segment dictionary has. A segment of another version is listed with its bytes in hex and the version reported.
- **OMF**: versions 1, 2.0 and 2.1, from the Apple IIgs Programmer's Workshop Reference and its OMF appendix.
- **Super hi-res**: $C1/0000 unpacked, $C0/0001 PackBytes, $C0/0002 Apple Preferred, $C0/0000 Paintworks, and 3200-color as $C1/0002, DreamGrafix $C0/8005 and `.3201`, drawn with each line's palette and 320 or 640 mode bit, and IIgs TN #63's color values.
- **Outputs**: RTF 1.5 with only `\b`, `\i`, `\ul`, `\ql`, `\qc`, `\qr`, `\qj`, `\tab` and `\par`, which Word and WordPad both open; CSV as RFC 4180 specifies, in UTF-8 with a byte order mark so Excel reads it as UTF-8; PNG through `PngCodec`.
- **Character sets**: Apple text, Mac OS Roman and ISO 8859-1, as CiderPress II offers. A IIgs format defaults to Mac OS Roman, the IIgs's character set. and the IIgs's Mac-style characters are mapped to Unicode by one table in `UnicodeSymbols.h`.

**Rationale**: with one table of formats, identification is in one place and tested alone, and the decoders are pure functions over bytes, as `PreviewDecoder`'s are. RTF is the smallest styled format both PC programs read.

**Alternatives considered**:

- **DOCX or XLSX output**: a zip and XML writer for formatting that RTF and CSV already cover. Not adopted.
- **A plugin interface per format**: a plugin layer is contrary to the constitution's simplicity principle, and a table of function pointers is enough.
- **Leaving Gutenberg out**: FR-156 lists it, and with no volume class its documents never appear.

## R36. Volumes larger than 140K (Story 19)

**Finding**: every volume class addresses one 143,360-byte buffer in DOS logical sector order. ProDOS, Pascal and CP/M access a block through `ProDosSkeleton::GetBlockByteOffset`, which pairs two DOS sectors per block. `DiskImage::kDos33ImageSize`, `BlankDiskBuilder`, `MountDiagnosis` and the ADTPro server also test for that size. Writes return the whole buffer, which `VolumeImage::Save` renders back into the container.

**Decision**:

- **Layout**: a `VolumeLayout` value is passed with each buffer: `Dos140K`, the order of today's buffers, or `Blocks`, a linear run of 512-byte blocks with its count. `GetBlockByteOffset` takes the layout, and returns today's pairing for `Dos140K` and `block * 512 + offset` for `Blocks`. A 140K image stays `Dos140K`, so the sector editor, the nibble layer and every test of today's volumes see the bytes they see now.
- **Size**: a ProDOS volume of up to 65,535 blocks is loaded whole, at most 32 MB, and written back whole, as a 140K volume is. Copying 32 MB on each write is about 10 ms.
- **Paged store**: HFS volumes and physical cards can be larger than is reasonable to copy on each write, so they are read through an `IBlockStore` with `ReadBlocks`, `WriteBlocks` and `Commit`. `FileBlockStore` reads pages of 64 KB from the file on demand and writes dirty pages on `Commit`. `MemoryBlockStore` serves the unit tests. A partition of 32 MB or less is copied out of the store into a `Blocks` buffer and opened with the volume classes as they are, so CFFA, DOS.MASTER and the other layouts use the ProDOS classes with no paging.
- **Size checks**: the checks for 143,360 bytes become checks of the layout, and the ADTPro server serves any `Blocks` image for its virtual drives.

**Rationale**: with one value passed beside the buffer, every reader of today's 140K images reads the same bytes, and a 32 MB copy costs less than reading the file.

**Alternatives considered**:

- **Converting 140K images to block order too.** The sector editor, the nibble layer and Casso's mount all work in DOS order, and the change would alter every test. Not adopted.
- **Paging every volume.** It changes the write path of every volume class for a size that fits in memory. Not adopted.

## R37. 2IMG and Disk Copy 4.2 (Story 19)

**Finding**: 2IMG has a 64-byte header with a creator, a format of DOS order, ProDOS order or nibbles, offsets and lengths of the data, the comment and the creator data, and flags with a write-protect bit and a DOS volume number. Disk Copy 4.2 has an 84-byte header with a Pascal-string name, data and tag sizes, a checksum of each, a disk format and a format byte, followed by the data and then the tags. Both are documented in Apple's and CiderPress II's format notes.

**Decision**: `TwoImgContainer` and `DiskCopy42Container`, each with `Parse` into a header value and a payload, and `Build` from a header and a changed payload. `DiskFormat` gains `TwoImg`, `DiskCopy42` and `Blocks` (`.hdv`, `.img`, `.iso`, and `.po` larger than 140K). The checksum is the rotate-and-add sum Disk Copy 4.2 defines, over 16-bit big-endian words. A 2IMG nibble image is passed to the nibble layer as a `.nib` is.

**Rationale**: a container is a header and a payload, and the payload is an image the layer reads today.

**Alternatives considered**: reading the containers in `VolumeImage::Load` with no class for each. The header is written back on every save, so it is better as a value with separate tests.

## R38. Extended files and AppleDouble (Story 20)

**Finding**: a ProDOS extended file, storage type 5, has a key block with a mini-entry for the data fork and one for the resource fork, each with its storage type, key block, blocks used and EOF, and two 18-byte Finder information entries. AppleSingle and AppleDouble share one format: entry 1 is the data fork, entry 2 the resource fork, entry 9 Finder information, entry 11 ProDOS file information. `AppleSingleCodec` reads and writes data-fork files today.

**Decision**: `FilePayload` gains `resourceFork` and `finderInfo`, and `FileEntry` gains `resourceForkBytes`. `ProDosVolume` reads and writes storage type 5 through its writer, with the forks' trees written as for any file. `AppleSingleCodec` writes entries 2 and 9, and gains an AppleDouble form with no entry 1. Which is written follows the host-name style, as FR-181 sets. A copy to a volume with no forks writes the data fork and returns `S_FALSE` with the reason FR-182 reports.

**Rationale**: the forks are two files' trees under one entry, so the writer that writes a tree writes both.

**Alternatives considered**: NTFS alternate data streams for the resource fork. They are lost when a file is copied to FAT, zipped or uploaded. Not adopted.

## R39. NuFX, Binary II and nested containers (Story 21)

**Finding**: NuFX (Apple File Type Note $E0/8002) is a master header and records, each with threads: data, resource and disk forks, a file name, comments. Thread formats are uncompressed, Squeeze, LZW/1, LZW/2, LZC 12 and 16, Deflate and bzip2. Each thread has a CRC. Binary II (File Type Note $E0/0000) is 128-byte headers before each file's data. Each method is documented in CiderPress II's NuFX, LZW, Squeeze and LZC notes. `Update/Inflate.cpp` decodes Deflate; Casso has no bzip2 decoder.

**Decision**:

- **Codecs**: `NuFxArchive` (read, build), `NuLzw` (LZW/1 and LZW/2, both ways), `SqueezeDecoder`, `LzcDecoder`, `BinaryTwoArchive`, `GzipFile` over `Inflate`. bzip2 threads use the reference bzip2 library, vendored under `External/`. Its license is BSD-style, which the clean-room rule allows, as it forbids GPL code only.
- **Browsing**: `ArchiveVolume` implements `IVolume` over a parsed archive, so the list, preview, Get, put, rename and delete paths are the ones an image has. A write returns the rebuilt archive's bytes, written as an image's are.
- **Nesting**: a location's path is a list of steps, each a container: a host file, a ZIP member, a gzip file, a disk image's file or an archive's record. Opening a step reads its bytes from the step before. A write goes back up the list, rebuilding each container in turn, and the host file is replaced last, through a temporary file.
- **Disk records**: a disk record is a volume, opened by `VolumeImage` from its bytes. An 800K record is a `Blocks` buffer (R36).

**Before ContainerPath** (T547, read from the code on 2026-11-09): a ZIP is a shell folder, listed by the Windows ZIP folder. Its members have no file system path, since `SIGDN_FILESYSPATH` fails for them, so a disk image in a ZIP is listed with an empty `hostPath`, and `isDiskImage` is false. The image is listed as an ordinary file with the shell's type text, and the preview pane has no catalog for it. A double-click calls `OpenShellItem`: the ZIP folder extracts the image to a temporary folder and starts the program associated with its extension, and a change made there isn't written back into the ZIP. A file on the image can't be copied out and one can't be put in, because the image never opens as a volume. Dragging the image out of the ZIP copies it, through the shell.

**Rationale**: an archive as an `IVolume` gets every operation the window has without a second set of paths, and the step list handles any depth with one rule.

**Alternatives considered**: NufxLib, now BSD-licensed. It is C with separate file I/O and allocation, and wrapping it is not smaller than LZW/2 and the record format. It is the test reference beside CiderPress II.

## R40. Link modules for new devices (Stories 23 to 29)

**Finding**: the link program loads at $0800 and must end before $3A00, below the shim's copy of the zero page; the program is several thousand lines. Block devices, raw capture, cassette transfers, the IIgs's SCC, Pascal firmware cards, the CS8900A and the LAN91C96 do not all fit there beside today's code. Each Apple has at most a few of them.

**Decision**: modules. A module is a separately assembled 6502 block that loads at $4000 to $7FFF, with a header of its version, its entry table and its size. After Ready, the PC sends the modules a connection uses with a new Load module request, verified by CRC as the program is. The program's request table dispatches the new requests to the module's entry table. The program's version and each module's are compared on each connection, as the program's is today. Modules: block devices, raw capture, cassette, SCC, Pascal firmware, CS8900A, LAN91C96, and the virtual drive driver's installer.

**Rationale**: memory above $4000 is free while the link runs, and loading over the link already exists for the program. A module that a machine does not use costs nothing.

**Alternatives considered**: one larger program. It overruns hi-res page 1 and the space under BASIC on every machine. Not adopted.

**Revised (2026-11-11, T566)**: $4000 to $7FFF is not free: the program's CRC and nibble tables are at $4000 to $42FF and its request payload at $4900, and DOS or BASIC.SYSTEM, which the program returns to, is above $9600. A module loads at $5000 to $8FFF, on the 5.25" disk buffers. A request that uses those buffers marks the module overwritten, and before each module request the PC sends a query of the module the program has, and loads it again if that isn't the module. A module is assembled against the program's build, given each of its symbols with a `p_` prefix, and its header has the PROGVER it was built for, which the program checks with the CRC. The header: `CM`, id, version, PROGVER, length, CRC-16/XMODEM of the bytes from offset 10, the request count and each request's command and entry.

## R41. Block devices through firmware (Story 23)

**Finding**: a ProDOS block device's firmware has $20, $00 and $03 at $Cn01, $Cn03 and $Cn05, and its entry at $Cn00 plus the byte at $CnFF. A SmartPort device also has $00 at $Cn07, and its dispatch is three bytes past the ProDOS entry. SmartPort STATUS lists the units and each unit's size and type. The //c's 3.5" port, the //c Plus, a UniDisk 3.5 and hard disk cards all answer this way, and the IIgs's slot 5 is a SmartPort.

**Decision**: the block module scans slots 1 to 7 for the signature, calls SmartPort STATUS for each unit where the firmware has it, and reads and writes 512-byte blocks through READ BLOCK and WRITE BLOCK. Each read is sent with a CRC, as sectors are. A whole-volume transfer uses Read disk to image's queue and progress. The partial image and its resume record are written as in R42. Verified writes read each block back and compare CRCs.

**Rationale**: the firmware drives each device the Apple has, so one module covers them all.

## R42. Resuming a long transfer (Story 23, Session 2026-11-09)

**Decision**: a transfer of a block device writes `NAME.partial` and `NAME.partial.json`, the device's slot, unit, block count, volume name, a SHA-256 of its volume directory blocks and the last block verified, after every 64 blocks. A later transfer of a device with the same values offers to continue. The image is renamed to its final name when the last block is verified.

## R43. Raw capture (Story 24)

**Finding**: the Disk II's data latch has a byte each 32 cycles, and a sync byte with its two zero bits takes 40. The Apple can count cycles between bytes in a timed loop. The head moves in quarter tracks by phase steps, so the link can read at quarter-track positions. A revolution is about 200 ms. WOZ 2 has a quarter-track map (TMAP) and bit streams (TRKS); a long run of zero bits is read by emulators as random bits.

**Decision**: the capture module reads at each quarter-track position from 0 to 34, and to 35 where the drive steps there, two revolutions per read, timing each byte. A position with no valid byte on its first read is reported empty and not read again. Reads two and three are compared on the Apple with the first, and only the differing ranges are sent. The PC builds each stream from the bytes and timings, inserting two zero bits after each sync byte, cuts one revolution where the stream repeats, and writes WOZ 2 with TMAP pointing the neighboring quarter tracks at a stream, as WOZ's reference specifies. A range that differs between reads is written as zero bits.

**Rationale**: comparisons on the Apple send most tracks once, which is how SC-055's five minutes is met: 141 positions times two revolutions is about 60 s of reading, and most positions are empty.

**Revised (2026-11-11)**: the module has $5000 to $8FFF (R40's revision), so two revolutions fit once, and there's no room for a second read beside the first to compare on the Apple. The timing is read from the data latch rather than counted: by Sather's Table 9.5, a finished byte is in the latch for about 8 cycles, and 4 more for each zero bit after it. The capture loop reads the latch again 8 cycles after it finds a byte; when the latch has the same byte then, zero bits followed it, and the byte is stored with its top bit clear, which a disk byte never has, so one byte of the buffer is one byte of the disk. The PC writes two zero bits after each byte both revolutions of the read mark, since a mark is now and then set on a byte that isn't a sync byte. The loop is one copy per page of the buffer, so no page crossing adds cycles to a byte. The buffer is sent page by page, each page with its CRC. The PC compares each whole track's reads aligned by rotation, writes a range that differs as zero bits, and maps a quarter track whose first read matches a neighbor's stream to that stream in TMAP without reading it again.

**Measured (2026-11-11)**: in Casso's drive, a mark at 8 cycles is on a byte followed by no zero bit about 1% of the time and by one about 45% of the time, so a 13-sector disk's 9-bit sync bytes, FF and one zero bit, can't be counted from two revolutions. A whole track is read at 8, 12 and 8 cycles, and up to four more times at 8 while an FF byte is uncertain; each run of FF is split into stretches of one count by the likeliest sequence of counts. Captures of CiderPress II's DOS 3.2 master and an RDOS disk decode to their sources' sectors, each track within a few sync bits of its source. Casso's drive steps by half tracks, so a quarter track a real drive steps to with two phases on can't be tested in Casso.

**Measured on a real //c (2026-11-12)**, from the raw reads of Wizardry I (Apple Pascal, 16 sectors):

- A revolution of a real track isn't byte for byte the same on each pass. The partial bytes of each write splice, between a data field's epilogue and the sync bytes after it, are framed differently each time, so one pass has a byte more or fewer than the next until the sync bytes realign it. A read whose revolution is found by an exact repeat fails, or finds two revolutions. The PC now starts a revolution where 64 bytes with eight or more values repeat exactly, once in the revolution and in three windows further on. It aligns every other revolution of every read to that one with bytes more or fewer allowed (a banded edit distance, 24 bytes either way, at most one byte in 50 differing), taking each revolution's best alignment. A byte is weak only where two alignments put it in a stretch of differences longer than 8 bytes.
- A byte stays in the //c's IWM data register longer than in a Disk II's latch. At 8 cycles a byte with no zero bits after it is marked about one time in four, where Casso's drive marks it about one time in a hundred; at 12 cycles it is never marked, and a run of sync bytes is marked now and then. With the fixed chances measured in Casso, sync bytes were counted wrongly and nearly every track was read the most times. The PC now measures each track's baseline on its bytes that aren't FF and counts each run of FF against it, and the extra reads are gone.
- A track between two standard tracks isn't read. With the two changes above, 34 of the 35 whole tracks are standard, every sector decodes, and a capture is 105 reads plus the gaps beside the one other track.
- The first encoding, with a count before each run of literal bytes, cost about 275 cycles a literal byte on the Apple, 3.8 KB/s, slower than the 130 cycles of sending bytes raw. The encoding is now escaped (0 begins a token), with the CRC from the program's tables inline.

**Alternatives considered**: flux capture. The Disk II has no access to flux, and Applesauce exists for it. Out of scope.

## R44. Virtual drives (Story 25)

**Finding**: ProDOS's device table at $BF10 has an entry per slot and drive, and a slot's entries are free when no card is in the slot. BASIC.SYSTEM's GETBUFR call ($BEF5) allocates pages below HIMEM and lowers HIMEM by that amount. ProDOS starts the first `.SYSTEM` file on the boot volume.

**Decision**: Make CassoLink ProDOS disk copies the user's `PRODOS` and `BASIC.SYSTEM` onto a new 140K ProDOS image with `CASSOLINK.SYSTEM` before them. `CASSOLINK.SYSTEM` loads the shim's serial code and the driver, installs the driver for drive 1 and 2 of the first free slot from 7 down, allocates its pages through GETBUFR (four pages, FR-200), and starts `BASIC.SYSTEM`. The driver's requests are new link requests, read block and write block; the PC writes each block to a one-block journal beside the image, then at its offset in the image, then clears the journal, each flushed before the reply. A journal left by a crash is applied when the image is next opened, so a block is in the image whole or not at all (FR-201).

**Rationale**: GETBUFR is the documented way to take memory beside BASIC.SYSTEM, and a free slot's entries are where ProDOS looks for a device.

## R45. Disks over cassette audio (Story 26)

**Finding**: the ][+ and //e toggle the cassette output at $C020 and read the input at $C060. Casso Explorer bootstraps the link through the cassette input today (`LinkAudioBootstrap`). ADTPro's audio client sends and receives disks with a separate encoding, published on adtpro.com. `WasapiAudio` renders audio; Casso has no capture from the PC's input.

**Decision**: the cassette module uses ADTPro's published audio encoding, so one decoder on the PC reads both clients. Each track is a packet with a CRC; a failed track is requested again. The PC captures its input through WASAPI shared-mode capture, added beside `WasapiAudio`, and decodes with the same edge timing `TapeSignalDecoder` uses. The connection's settings pick the devices and draw a level meter. In the scenario tests, Casso's cassette input is fed from the PC side's audio and its output is captured, through `CassettePort`.

**Rationale**: one encoding and one decoder for both clients.

## R46. Tape recordings as files (Story 27)

**Finding**: Casso decodes WAV and, through Media Foundation, MP3 (`MfTapeAudioDecoder`), and Ogg through stb_vorbis, which decodes only. Windows' Media Foundation has an MP3 encoder. `TapeRecordScanner` finds records in a recording.

**Decision**: `TapeRecordingWriter` writes the monitor's format: a 10-second header tone, the sync bit, the bytes and the checksum, with a BASIC program's length record ahead of it, as SAVE writes. WAV through `WavCodec`, MP3 through Media Foundation's encoder, Ogg Vorbis through libvorbis and libogg, vendored under `External/`, both BSD-licensed. `TapeRecordingVolume` implements `IVolume` read-only over `TapeRecordScanner`'s records. Disk audio is the cassette module's stream written to a file.

## R47. More interfaces (Story 28)

**Finding**: the IIgs's ports are a Zilog 8530 SCC at $C038 to $C03B. Bit 7 of $C036 sets the IIgs's speed. A Pascal 1.1 firmware card has $38, $18 and $01 at $Cn05, $Cn07 and $Cn0B, and its init, read, write and status offsets at $Cn0D to $Cn10. The original Uthernet is a CS8900A accessed through its PacketPage registers in the slot's I/O space; the LANceGS is an SMSC LAN91C96 with banked registers, at $C0x0 with x = 8 + slot. ADTPro's ip65 drives both, as the Contiki port does.

**Decision**: an SCC module, a Pascal firmware module and two Ethernet modules, each with the transport entries the program calls today: open, send, receive and close. The Ethernet modules implement in software the UDP and TCP the Uthernet II implements in hardware, as a minimal stack of ARP, IPv4, UDP and TCP with one connection. The program sets the IIgs to normal speed around each 5.25" read and write and restores $C036 after. Test targets are scripted: KEGS or GSplus for the SCC, MAME for a IIgs with a Super Serial Card, the Laser 128 and the LANceGS, AppleWin for the Uthernet.

## R48. The Apple /// client (Story 29)

**Finding**: SOS boots `SOS.KERNEL`, `SOS.DRIVER` and `SOS.INTERP` from a disk. Drivers are configured into `SOS.DRIVER` with the System Configuration Program. `.RS232` is the serial driver and `.D1` to `.D4` and `.PROFILE` the block drivers. An interpreter calls SOS through BRK with a call number.

**Decision**: the link's SOS program is assembled as `SOS.INTERP`, from the shared 6502 source with an SOS transport and SOS block calls in place of the Apple II's. Make CassoLink SOS disk copies the user's `SOS.KERNEL` and `SOS.DRIVER` onto a new disk with it. The PC side treats the Apple /// as an Apple with block devices, as Story 23 does.

## R49. DOS 3.2 and large DOS 3.3 (Story 30)

**Finding**: 13-sector disks use 5-and-3 encoding: the address prolog is D5 AA B5, a data field is 410 nibbles plus a checksum. A `.d13` file is 35 tracks of 13 sectors, 116,480 bytes. CiderPress II's DOS notes document 40- and 50-track and 32-sector volumes, and the Basis 108's 80-track layout with two bitmap bytes per track.

**Decision**: `NibblizationLayer` gains the 5-and-3 codec, and `VolumeImage` loads `.d13` and 13-sector nibble and WOZ images into a `Dos13` layout. `Dos33Volume` reads the track and sector counts from the VTOC, so 40-, 50- and 80-track and 32-sector volumes are one class. Over the link, a 13-sector track is read by the capture module's track read and decoded on the PC, and written as nibbles the PC encodes, as Sector nibbles writes 6-and-2 today.

## R50. Partitions and physical cards (Story 31)

**Finding**: CFFA cards are consecutive 32 MB slices (FR-218), with the HFS partitions of firmware 1.2's GS/OS driver after the fourth. FocusDrive and MicroDrive have partition maps in block 0. DOS.MASTER puts DOS 3.3 volumes in the last blocks of a ProDOS volume. AmDOS, OzDOS and UniDOS split an 800K disk into two 400K DOS volumes, interleaved by block or by half. Hybrids put DOS 3.3 tracks beside a ProDOS or Pascal volume on a 140K disk, each file system marking the other's blocks used. Each is documented in CiderPress II's notes. Windows opens a disk as `\\.\PhysicalDriveN` for administrators only.

**Decision**: one `PartitionLayout` class per layout, each with `Detect` over an `IBlockStore` and a list of `Partition` values. Hybrids open each file system on the same buffer, and a write fails when it would allocate a block the other's map has as used. Physical cards: Casso Explorer starts CassoExplorer.exe again, elevated, with `/rawdevice` and a pipe name, through ShellExecuteEx with `runas`; that process opens the drive and serves block reads and writes over the pipe, and exits when the pipe closes. Removable drives are those `IOCTL_STORAGE_QUERY_PROPERTY` reports as removable media or on a USB or SD bus.

**Rationale**: with elevation for that operation alone, the window runs unelevated, and drag and drop with File Explorer works.

## R51. HFS (Story 32)

**Finding**: HFS has a master directory block at block 2, a volume bitmap, an extents overflow B-tree and a catalog B-tree, names of up to 31 Mac OS Roman characters, and two forks per file. Apple's File Type Note for ProDOS files on HFS maps a ProDOS type to the HFS type `p` + type + aux type, creator `pdos`, and lists the pairs for TEXT, BINA and others.

**Decision**: `HfsVolume` implements `IVolume` over an `IBlockStore`, with `HfsBTree` for both trees, read and write, splitting and merging nodes as Inside Macintosh: Files specifies. The type mapping is a table from the File Type Note. Formatting writes the structures for the volume's size, as Inside Macintosh specifies for an initialized volume.

## R52. SOS, Gutenberg writes and RDOS (Stories 33 and 34)

**Decision**:

- **SOS**: `SosVolume` beside `ProDosVolume`, sharing its reader and writer with an SOS type table, since SOS's directory format is ProDOS's ancestor and matches it.
- **Gutenberg**: the free-space map is worked out from disks Gutenberg saves in Casso: blank, after one document, after a delete, after a long document. The result is written into [contracts/file-types.md](contracts/file-types.md) before `GutenbergVolume` gains writes.
- **RDOS**: `RdosVolume`, read-only, for RDOS 2.1, 3.2 and 3.3, from CiderPress II's RDOS notes.

## R53. The expanded preview (Story 35)

**Decision**: An expanded state of the preview pane, per tab, in place of a separate viewer window. Expanded, `ExplorerLayout` lays the preview over the command bar, address bar, body and status bar, below the tab strip. The tree, list and splitter widths stay in the preferences for the collapse. Previous and next move the list's selection, so the list, the status bar and a collapse all match the file on screen. A tab torn off into a window, with its preview expanded, is a viewer beside the main window.

View as, Save as and Copy are on the preview pane, docked or expanded. View as resets to the default view on each file.

**Alternatives considered**: A separate top-level viewer window, one per Casso Explorer window, reused for each file. It duplicated the preview pane, and its file had to follow the list.

CiderPress II's file viewer, from its GUI manual, was compared feature by feature. Taken from it: previous and next through the selected files, or the whole list when one file is selected; a list of the conversions that apply, the best first; export of the current conversion, a picture at its original resolution; Copy, with formatted text as plain and formatted; and the resource fork as a view. Left out: saving the conversion options as defaults, control characters drawn as symbols, and opening PDF and RTF files in their Windows applications. Its character sets, hex dump, text search and DOS 3.3 sector view are in the preview and the disk inspector already.

## R54. Collection tools (Story 36, Session 2026-11-09)

**Decision**:

- **Results**: a results location, `results:<id>`, as a search's is, so the list, groups and context menu are the ones a search has. Save as CSV writes the columns in [data-model.md](data-model.md).
- **Walk**: a worker thread walks the folder and descends into every container the step list of R39 opens, reporting rows as they are found. An image with partitions is listed as a disk too, unless the partitions fill it, as on a CFFA card or a MicroDrive. A file set has members on disks whose data differs, so two copies of one disk don't form file sets.
- **Identity**: a disk's identity is the SHA-256 of its decoded volume buffer, so `.dsk`, `.do`, `.po`, 2IMG and a WOZ of the same disk match. The CRC-32 is of the same buffer. A WOZ track with data its sectors don't decode, such as a protection track, sets `hasMore`. A file's identity is the SHA-256 of its data fork and its type.
- **Content search**: the search walker passes each file inside an image through `FileFormatTable::Export` and searches the text.

## R55. BASIC tools (Story 37)

**Decision**: `IntegerBasicTokenizer` beside `ApplesoftTokenizer` in CassoCore, from the Integer BASIC token table its detokenizer has. `BasicRenumber` and `BasicCrossReference` work on token lists of either BASIC. `casso basic` gains `tokenize`, `list`, `renumber` and `xref`.

## R56. Game data (Story 38)

**Finding**: Wizardry I to III scenarios are Pascal files on Pascal disks; Infocom story files are Z-machine images, specified by the Z-Machine Standards Document 1.1.

**Decision**: `WizardryScenario` and `ZMachineStory` decoders in `FileTypes/`, which return Lines and Picture. The Wizardry layout is taken from community format notes published for it, checked against the game run in Casso. DiskBrowser's source is read for behavior only, under FR-171.

## R57. VisiCalc recalculation (Story 22)

**Decision**: `VisiCalcEvaluator` parses each formula into a tree and evaluates the sheet in the file's order, one pass per recalculation, with the functions in the VisiCalc User's Guide's reference section. Values are formatted as VisiCalc formats them: general, integer, dollars and cents, left, right and the graph format.
