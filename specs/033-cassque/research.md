# Research: Cassque

**Feature**: 033-cassque | **Date**: 2026-09-10

Every question here is settled by reading the tree. Where the tree
contradicts an assumption the spec was written under, the correction is
stated and the spec's requirement is kept unless the correction makes it
impossible.

## R1. Executable shape

**Finding**: `Casso.vcxproj` holds one comment-only `Main.cpp`, one `.rc`,
`EntryPointSymbol=wWinMainCRTStartup`, and references to CassoCore,
CassoEmuCore and Dxui. The real `wWinMain` is `CassoEmuCore/Gui/GuiMain.cpp`.
Its order: DPI awareness, EHM notify and breakpoint hooks, command line,
asset bootstrap, prefs, machine selection, shell initialize, message loop.
COM is initialized inside the shell, not the entry point. Resources are
per module, so a second executable needs its own `.rc`.

**Decision**: `Cassque/Cassque.vcxproj` copies Casso's layout minus the
mesh bake, with `Cassque.rc` holding the icon and the cassowary PNG. The
entry point is `CassoEmuCore/Cassque/CassqueMain.cpp`, following GuiMain's
order with a `CassqueShell` in place of `EmulatorShell`. The solution gains
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
- Targeting: Casso passes its own HWND on Cassque's command line as
  `--owner <hwnd>`. Cassque sends to that window when it is alive, else
  enumerates `CassoWindow` and picks the most recently active, else
  launches. A second launch from the same Casso is detected by Cassque
  enumerating its own class `CassqueWindow` for a window whose owner
  matches, and fronting it.

**Alternatives considered**: a named pipe. Rejected; the existing channel
is already the protocol both sides speak, and one more message id costs
nothing.

## R3. The prefs file cannot be shared for writes

**Finding**: `GlobalUserPrefs::Save` is a read-modify-write with no lock,
no temp-and-rename, and no external-edit detection. A running Casso
rewrites the whole `global` section from memory on its next save, so
anything Cassque wrote there is lost. The spec's "shared prefs file"
assumption is unsafe for known folders, Cassque's theme, and everything
else Cassque persists.

**Decision**: two small files beside `UserPrefs.json`, each written by
atomic replace and read on demand rather than cached.

- `KnownFolders.json`: the list, with last-used time per entry. Both
  executables read it when they need it and append through one helper in
  core that reads, merges and replaces atomically. Casso's picker reads it
  at open time in place of `DiskMru::DistinctFolders`, then still appends
  the MRU's folders as today so nothing the picker showed before goes
  missing. First-run seeding is the MRU's distinct folders.
- `CassquePrefs.json`: theme, preview-pane state, host-naming setting,
  placement, open tabs. Casso never touches it. First-run theme seeding
  reads Casso's `activeTheme` from `UserPrefs.json` read-only.

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

**Decision**: Cassque owns a `CassqueTheme` object per choice: Light and
Dark are two new palettes in Dxui built from the Fluent tokens
`DxuiWindowsThemeColors` already carries; Follow system picks one on the
settings-change notification; the three Casso choices come from
`CassoTheme::MakeByName`, with Skeuomorphic used as colors only because
Cassque has no desk scene to draw.

## R9. About dialog image

**Finding**: `DialogDefinition` supports a fixed icon enum, text runs and
hyperlinks, no arbitrary bitmap. `PngCodec::DecodeRgba` exists.

**Decision**: `DialogDefinition` gains an optional image, RGBA bytes plus
size, painted by `DialogBodyContent` via `DrawIconBitmap`. The cassowary
ships as RCDATA in `Cassque.rc`. Casso's About is untouched.

## R10. Launching processes

**Finding**: core has no `CreateProcess` seam; the only launches are
`ShellExecuteW` for folders and URLs. `CommandLineParser` accepts
`--disk1`, `--disk2` and `--title` with both prefixes.

**Decision**: `Seams/IProcessLauncher` with a Win32 implementation and a
fake for tests. Casso's Cassque menu item launches `Cassque.exe` from its
own module directory with `--owner <hwnd>`. Cassque's Open in new Casso
launches `Casso.exe` with `--disk1 <quoted path>`.

## R11. Host-side drag formats

**Decision**, no prior art in the tree:

- Private format `CassqueCatalogEntries`: source image path, file-system
  kind, and the entries' paths, so an Apple-to-Apple drop copies through
  `IVolume::Read` and `Write` with no conversion.
- `CFSTR_FILEDESCRIPTORW` and `CFSTR_FILECONTENTS` with delayed rendering,
  names per the host naming rule, so Explorer and Cassque host folders
  receive converted files on demand.
- `CF_HDROP` accepted on disk-image targets; each path goes through the
  content rule from the clarifications.
- `CF_HDROP` offered for a dragged disk image so Casso's existing
  `DxuiDragDropTarget` accepts it unchanged.

## R12. Testing shape

**Finding**: the suite tests decisions extracted into pure classes and
leaves `EmulatorShell` nearly untested. Mocks exist for `IDiskFileIo`,
`IFileSystem`, and the three Dxui paint interfaces.

**Decision**: every decision in Cassque lives in a model class under
`CassoEmuCore/Cassque/Model/` with no window: `BrowserModel` (locations,
tabs, selection), `CatalogModel`, `PreviewDecoder`, `HostFileNaming`,
`ContentSniffer`, `KnownFolderStore`, `CassoTargeting`, `DragPayload`.
`CassqueShell` is a thin window over them. New Dxui widgets get headless
tests on the mocks.
