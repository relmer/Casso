# Implementation Plan: Cassque

**Branch**: `033-cassque` | **Date**: 2026-09-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/033-cassque/spec.md`

## Summary

A second GUI executable, `Cassque.exe`, holding no code, whose entry point
and shell live in `CassoEmuCore/Cassque/`. It browses host folders and
Apple II disk images in an Explorer layout, previews files, performs every
verb the command-line disk tool offers through the same core classes,
drags files in and out with conversion only when crossing into the host,
and hands disks to a running or newly launched Casso over the existing
intent channel. It shares Casso's known folders through a new small file
and keeps its own preferences in another. Every decision is a pure model
class under test; the window is thin. Research and corrections to the
spec's assumptions are in [research.md](research.md).

## Technical Context

**Language/Version**: C++ under stdcpplatest, MSVC v145+

**Primary Dependencies**: Dxui plus the 032 command widgets; CassoEmuCore
disk classes (`DiskImageSession`, `IVolume`, `DiskCommandRunner`,
`VolumeImage`); CassoCore `ApplesoftTokenizer` and the 6502 tables; the
Apple II video modes; OLE drag and drop; WIC via `PngCodec`

**Storage**: `KnownFolders.json` and `CassquePrefs.json` beside
`UserPrefs.json`, atomic replace, read on demand

**Testing**: CppUnitTestFramework; `FakeDiskFileIo`, `InMemoryFileSystem`,
the Dxui paint mocks, a new `FakeProcessLauncher`

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: desktop app, second executable over the shared core

**Performance Goals**: a folder of 200 images lists in under one second; a
catalog appears within 200 ms of selection (SC-003); tree children are
loaded lazily on first expand; catalogs are cached per tab until a write

**Constraints**: `Cassque.exe` holds zero functions; `Dxui/` includes
nothing from CassoEmuCore; every CLI verb is byte identical through
Cassque; no change to `GlobalUserPrefs` semantics; Casso's own picker
keeps offering everything it offered before

**Scale/Scope**: one new executable project, one new core folder with
about ten model classes and a shell, two new CassoCore codecs, six Dxui
additions, two intent-channel intents plus a reply, one process seam, two
prefs files, and their tests

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Code Quality | PASS | New code; EHM, VerbNoun, class-static helpers, `UnicodeSymbols.h` for glyphs. |
| II. Testing Discipline | PASS | Every decision in a model class on mocks; no test touches a real file, window or process. OLE calls sit behind `DxuiDragDropSource` and are exercised through a fake consumer. |
| III. UX Consistency | PASS | CLI behavior unchanged; Cassque's menus follow Casso's conventions via the 032 widgets. |
| IV. Performance | PASS | Lazy tree, virtual list, cached catalogs and framebuffers. |
| V. Simplicity | PASS | No plugin layer; two JSON files instead of a locking scheme; the preview reuses the list view instead of a new text control. |
| VI. Thin Executable | PASS | `Cassque.vcxproj` is a linker target per the 031 contract; the entry point is in core. |

Post-design re-check: unchanged. No Complexity Tracking entries.

## Project Structure

### Documentation (this feature)

```text
specs/033-cassque/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── intent-channel.md
│   ├── host-file-names.md
│   └── dxui-additions.md
├── tasks.md              # /speckit-tasks output
└── validation.md         # quickstart outcomes, written during the last phase
```

### Source Code (repository root)

```text
Cassque/
├── Cassque.vcxproj                    # NEW: comment-only Main.cpp, Cassque.rc, wWinMainCRTStartup
├── Main.cpp                           # NEW: comment naming CassoEmuCore
└── Cassque.rc                         # NEW: icon, cassowary PNG as RCDATA
Casso.sln                              # one project block, twelve configuration lines

CassoCore/
├── IntegerBasicDetokenizer.h/.cpp     # NEW
└── Disassembler.h/.cpp                # NEW, over Cpu tables and OpcodeTable::GetOperandSize

Dxui/
├── Widgets/
│   ├── DxuiTreeView.h/.cpp            # ids, lazy children, checkbox toggle, arrow keys
│   ├── DxuiListView.h/.cpp            # multi-select, sort and activate callbacks
│   ├── DxuiSplitter.h/.cpp            # NEW
│   ├── DxuiStatusBar.h/.cpp           # NEW
│   ├── DxuiFramebufferView.h/.cpp     # NEW
│   ├── DxuiHexView.h/.cpp             # NEW: offset, hex and text columns over one byte selection,
│   │                                  #      bytes from a host source at a host origin (debugger too)
│   ├── DxuiAddressBar.h/.cpp          # NEW: segments, typed path, host folders and image directories
│   └── DxuiTabStrip.h/.cpp            # overflow, drag to reorder, new-tab affordance
├── Window/
│   └── DxuiDragDropSource.h/.cpp      # NEW: IDropSource + IDataObject with delayed rendering
└── Theme/
    ├── DxuiLightTheme.h/.cpp          # NEW
    └── DxuiDarkTheme.h/.cpp           # NEW

scripts/
└── GenApple2Font.py                   # NEW: CharacterRom.h's glyphs to Resources/Fonts/CassoApple2.ttf

CassoEmuCore/
├── Cassque/
│   ├── CassqueMain.cpp                # NEW: wWinMain, modeled on Gui/GuiMain.cpp
│   ├── CassqueShell.h/.cpp            # NEW: window, layout, routing; thin
│   ├── CassqueCommands.h/.cpp         # NEW: DxuiCommand table, menu and toolbar placement
│   ├── CassqueTheme.h/.cpp            # NEW: the six choices, follow-system switching
│   └── Model/
│       ├── BrowserModel.h/.cpp        # tabs, locations, history, selection, sort
│       ├── TreeModel.h/.cpp           # roots, lazy children, known-folder rows
│       ├── CatalogModel.h/.cpp        # FileEntry and host entries to CatalogRow
│       ├── PreviewDecoder.h/.cpp      # kind selection and rendering to PreviewContent
│       ├── PicturePreview.h/.cpp      # video-mode render into a framebuffer
│       ├── HostFileNaming.h/.cpp
│       ├── ContentSniffer.h/.cpp
│       ├── DiskOperations.h/.cpp      # IVolume + DiskImageSession + DiskCommandRunner facade
│       ├── DragPayload.h/.cpp
│       ├── KnownFolderStore.h/.cpp    # shared with Casso's picker
│       ├── CassquePrefs.h/.cpp
│       └── CassoTargeting.h/.cpp
├── Seams/
│   ├── IProcessLauncher.h             # NEW
│   ├── Win32ProcessLauncher.h/.cpp    # NEW
│   └── Win32IntentChannel.h/.cpp      # InsertDisk, DescribeMachine, reply path
├── Machines/Apple2/Common/
│   ├── IVolume.h                      # Rename added
│   ├── Dos33Volume.h/.cpp             # Rename
│   ├── ProDosVolume.h/.cpp            # Rename; modified date surfaced on FileEntry
│   └── VolumeTypes.h                  # FileEntry gains modifiedUnix and hasModified
├── Shell/
│   ├── Window/EmulatorWindow.cpp      # OnCopyData: insert, describe, reload replies; drop path appends known folder
│   ├── EmulatorShell.cpp              # Cassque menu launch
│   └── WindowCommandManager.cpp       # picker reads KnownFolderStore
├── Ui/Dialogs/
│   ├── DialogDefinition.h             # optional image
│   └── DialogBodyContent.cpp          # paints it
└── resource.h                         # Cassque icon and PNG ids
CassoCore/ExternalChangeIntent.h       # two intents

UnitTest/
├── Cassque/                           # NEW folder, one test file per model class
├── Dxui/                              # extended and new widget tests
├── UiTests/IntentChannelTests.cpp     # extended
└── UnitTest.vcxproj
```

**Structure Decision**: Cassque's code sits under `CassoEmuCore/Cassque/`,
a sibling of `Gui/` and `Cli/`, per the 031 machine-layout contract's
placement rule (code that assumes no machine lives outside `Machines/`).
Model classes are the unit of testing; the shell is the only file that
knows about HWNDs.

## Design

### Phases, in dependency order

Each phase leaves the tree building and every existing test green.
tasks.md prepends a fixtures phase, so its numbering runs one ahead of
this list; phases here through the executable need nothing from 032.

1. **Codecs**: `IntegerBasicDetokenizer`, `Disassembler`, plus
   `IVolume::Rename` on both file systems and the ProDOS modified date
   on `FileEntry`, with tests.
2. **Model**: `HostFileNaming`, `ContentSniffer`, `PreviewDecoder`,
   `PicturePreview`, `CatalogModel`, `DiskOperations`, `BrowserModel`,
   `TreeModel`, `DragPayload`, `KnownFolderStore`, `CassquePrefs`,
   `CassoTargeting`, all pure, all with tests. Byte-identity tests
   against `DiskCommandRunner` on `FakeDiskFileIo` for every verb.
3. **Casso side**: `KnownFolderStore` read in Casso's two picker call
   sites and seeded from the MRU; the two intents and the reply in
   `Win32IntentChannel` and `OnCopyData` in `EmulatorWindow.cpp`, which
   also appends every mount's folder to the known-folder store; `IProcessLauncher`
   and the Cassque menu item; `DialogDefinition` image. Casso ships this
   even if Cassque slips.
4. **Dxui additions**: tree extensions, list extensions, splitter, status
   bar, framebuffer view, drag source, light and dark palettes, each with
   headless tests. The capability checklist's tree tests must pass
   unchanged.
5. **Executable and entry point**: `Cassque.vcxproj`, `Main.cpp`,
   `Cassque.rc`, solution entries, `CassqueMain.cpp` opening an empty
   `CassqueShell` window. Proves the thin-exe shape early.
6. **Shell, gated on 032 merged**: `CassqueCommands` over `DxuiCommand`,
   menu bar, toolbar, context menus via `DxuiPopupMenu`; layout of tree,
   list, preview, tabs, status bar with splitters; routing to the models;
   drag and drop wiring; theme switching; About with the picture.
7. **The preview's text and bytes**: `scripts/GenApple2Font.py` builds
   `Resources/Fonts/CassoApple2.ttf` from the character generator table
   in `CharacterRom.h`, embedded and registered the way
   `CassoSymbols.ttf` already is, so a text preview draws in the
   machine's own shapes as selectable text; `DxuiHexView` replaces the
   hex dump's list of strings with a byte-range selection drawn in both
   the hex and the text column, grouping at 1, 2, 4 and 8 bytes, copy
   either way, and go to offset. The font's glyphs come from the table
   at build time rather than a second copy of the dots, so the two
   cannot drift. The hex view reads its bytes through a host-supplied
   source at a host-supplied origin and holds no copy, because the
   debugger's caller is live machine memory addressed from $0000, not a
   file the widget could own.
8. **Native chrome, tabs and the address bar**: the parity pass over
   Dxui's Windows themes and metrics against File Explorer, a splitter
   whose grab is wider than its line, Explorer's navigation icons, a
   toolbar with a trailing group, names cut off rather than wrapped,
   a tab strip that survives more tabs than fit and reorders by drag,
   `DxuiAddressBar`, and a chrome arrangement that lets the tab strip sit
   outside the toolbars. Every piece lands in Dxui, so Casso inherits it.
9. **Validation and gates**: quickstart §1 through §9, `validation.md`,
   CHANGELOG, README headline, style sweep, four-configuration analysis
   rebuild, master merge.

### Byte identity

`DiskOperations` builds a `CommandLineOptions` for create, init, boot and
the sector and block verbs exactly as `CommandLineParser` would, and calls
`DiskCommandRunner::Run` on the real `Win32DiskFileIo`. For list, get, put
and delete it uses `DiskImageSession` and `IVolume` directly, which is the
same code the runner calls. The tests in phase 2 run both paths on the
same fake image and compare buffers.

### The tree

`TreeModel` supplies nodes with ids of the form `casso:<path>`,
`pc:<path>`, `img:<path>`, `dir:<path>|<inner>`. Children are produced on
first expand: drives from the host, folders and images from a directory
listing filtered by the supported-extension set, directories from
`IVolume::Enumerate`. A known folder that is missing is a dimmed node
with children disabled.

### Preview

`PreviewDecoder::Decide (FileEntry, bytes)` returns the kind by the spec's
rule, then renders: listing through the detokenizers, text through the
existing text conversion, picture through `PicturePreview`, hex through
`Disassembler` when toggled, catalog through `CatalogModel`. The shell
shows listing, text, hex and catalog in a virtual `DxuiListView` and
pictures in `DxuiFramebufferView`.

### Known folders, both sides

`KnownFolderStore` is the one reader and writer. Casso's picker at
`GuiMain.cpp` and `WindowCommandManager.cpp` calls `Load`, appends its
folders to the picker as `AppendSiblingDisksFromMruFolders` does today,
and still appends the MRU's own folders so the picker never loses a
folder it used to show. Casso records a hand-off from the intent channel
by calling `Append`; Cassque calls it on Insert, Open in new Casso and a
drag onto Casso, and on Add to Casso.

### Theme

`CassqueTheme` owns six `IDxuiTheme` instances and a current pointer. The
window's `SetTheme` is called with the pointer and invalidated. Follow
system reads `DxuiWindowsThemeColors` on the settings-change notification
`DxuiHwndSource` already raises and swaps between Light and Dark.

## Risks

- **032 timing**: phase 6 is the only gate; everything before it merges
  on its own if 032 runs long.
- **OLE drag on the UI thread**: `DoDragDrop` blocks; the preview timer
  and tooltip must tolerate a paused loop, as the caption-drag stopgap
  already does.
- **Prefs races**: avoided by the two-file design; the residual race is
  two Cassque windows appending known folders at once, closed by
  read-merge-replace under a named mutex in `KnownFolderStore`.
- **Video modes' `MemoryBus` constructor argument**: a default-constructed
  bus is never read when `videoRam` is non-null; a test asserts that with
  a bus that fails on read.
- **Master renames mid-branch**: merge master after each phase.

## Phase 2 Preview

`/speckit-tasks` generates tasks.md following the seven phases, one
commit per phase, with the 032 merge as the first task of phase 6.
