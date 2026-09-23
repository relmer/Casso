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
