# Implementation Plan: Casso Explorer

**Branch**: `033-cassque` | **Date**: 2026-09-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/033-cassque/spec.md`

## Summary

A second GUI executable, `CassoExplorer.exe`, holding no code, whose entry point
and shell live in `CassoEmuCore/CassoExplorer/`. It browses host folders and
Apple II disk images in an Explorer layout, previews files, performs every
verb the command-line disk tool offers through the same core classes,
drags files in and out with conversion only when crossing into the host,
and hands disks to a running or newly launched Casso over the existing
intent channel. It shares Casso's known folders through a new small file
and keeps its own preferences in another. Every decision is a pure model
class under test; the window is thin. Research and corrections to the
spec's assumptions are in [research.md](research.md).

Section H adds CassoLink: a protocol engine that serves ADTPro's
clients and Casso's own polling client (R15, R16), a 6502 shim and program
assembled from source in the core (R17, R22), a paced bridge from an
emulated 6551 to the PC (R18), COM and TCP streams behind one seam (R19),
sparse sector images the volume classes fault in over the line (R20), and
an Apple root in Casso Explorer's tree (R21). The wire contract is
[contracts/cassolink.md](contracts/cassolink.md).

## Technical Context

**Language/Version**: C++ under stdcpplatest, MSVC v145+

**Primary Dependencies**: Dxui plus the 032 command widgets; CassoEmuCore
disk classes (`DiskImageSession`, `IVolume`, `DiskCommandRunner`,
`VolumeImage`); CassoCore `ApplesoftTokenizer` and the 6502 tables; the
Apple II video modes; OLE drag and drop; WIC via `PngCodec`

**Storage**: `KnownFolders.json` and `CassoExplorerPrefs.json` beside
`UserPrefs.json`, atomic replace, read on demand

**Testing**: CppUnitTestFramework; `FakeDiskFileIo`, `InMemoryFileSystem`,
the Dxui paint mocks, a new `FakeProcessLauncher`

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: desktop app, second executable over the shared core

**Performance Goals**: a folder of 200 images lists in under one second; a
catalog appears within 200 ms of selection (SC-003); tree children are
loaded lazily on first expand; catalogs are cached per tab until a write

**Constraints**: `CassoExplorer.exe` holds zero functions; `Dxui/` includes
nothing from CassoEmuCore; every CLI verb is byte identical through
Casso Explorer; no change to `GlobalUserPrefs` semantics; Casso's own picker
keeps offering everything it offered before

**Scale/Scope**: one new executable project, one new core folder with
about ten model classes and a shell, two new CassoCore codecs, six Dxui
additions, two intent-channel intents plus a reply, one process seam, two
prefs files, and their tests

**Section H additions**: Winsock and the Win32 COM-port API (Windows SDK,
no new dependency); CassoCore's assembler at run time; `Acia6551`,
`IVolume`, `NibblizationLayer`, `DirectBootBuilder`; ADTPro 2.1.0's own
host and client as a captured reference, never its source. Throughput:
115,200 baud is about 11.5 KB/s, so a 140 KB disk in under 30 s (SC-027)
allows about half the time for framing and retries.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Code Quality | PASS | New code; EHM, VerbNoun, class-static helpers, `UnicodeSymbols.h` for glyphs. |
| II. Testing Discipline | PASS | Every decision in a model class on mocks; no test touches a real file, window or process. OLE calls sit behind `DxuiDragDropSource` and are exercised through a fake consumer. |
| III. UX Consistency | PASS | CLI behavior unchanged; Casso Explorer's menus follow Casso's conventions via the 032 widgets. |
| IV. Performance | PASS | Lazy tree, virtual list, cached catalogs and framebuffers. |
| V. Simplicity | PASS | No plugin layer; two JSON files instead of a locking scheme; the preview reuses the list view instead of a new text control. |
| VI. Thin Executable | PASS | `CassoExplorer.vcxproj` is a linker target per the 031 contract; the entry point is in core. |

Post-design re-check: unchanged. No Complexity Tracking entries.

Section H re-check: II, every transport sits behind `IByteStream` and the
COM-port list behind the same seam, so no unit test opens a port or a
socket; scenario tests run the Apple program in a `TestMachine` against
the in-process stream. V, the Apple side stays a shim and a sector server,
and file systems stay in `IVolume` (R16, R20). VI, the link lives in
`CassoEmuCore/CassoLink/`; no executable changes. PASS.

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
│   ├── dxui-additions.md
│   └── cassolink.md        # section H wire contract and session interface
├── tasks.md              # /speckit-tasks output
└── validation.md         # quickstart outcomes, written during the last phase
```

### Source Code (repository root)

```text
CassoExplorer/
├── CassoExplorer.vcxproj                    # NEW: comment-only Main.cpp, Casso Explorer.rc, wWinMainCRTStartup
├── Main.cpp                           # NEW: comment naming CassoEmuCore
└── Casso Explorer.rc                         # NEW: icon, cassowary PNG as RCDATA
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
├── CassoExplorer/
│   ├── CassoExplorerMain.cpp                # NEW: wWinMain, modeled on Gui/GuiMain.cpp
│   ├── CassoExplorerShell.h/.cpp            # NEW: window, layout, routing; thin
│   ├── CassoExplorerCommands.h/.cpp         # NEW: DxuiCommand table, menu and toolbar placement
│   ├── CassoExplorerTheme.h/.cpp            # NEW: the six choices, follow-system switching
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
│       ├── CassoExplorerPrefs.h/.cpp
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
│   ├── EmulatorShell.cpp              # Casso Explorer menu launch
│   └── WindowCommandManager.cpp       # picker reads KnownFolderStore
├── Ui/Dialogs/
│   ├── DialogDefinition.h             # optional image
│   └── DialogBodyContent.cpp          # paints it
└── resource.h                         # Casso Explorer icon and PNG ids
CassoCore/ExternalChangeIntent.h       # two intents

CassoEmuCore/CassoLink/                # section H
├── CassoLinkCodec.h/.cpp              # framing, EOR, CRC, RLE (ADTPro's, R15)
├── CassoLinkHost.h/.cpp               # serves ADTPro's clients and Casso's (R16)
├── CassoLinkSession.h/.cpp            # thread, queue, LinkedDisks, states
├── LinkedDisk.h/.cpp                  # sparse sector image, fault-in (R20)
├── LinkProgram.h/.cpp                 # assembles the shim and program (R22)
├── LinkBootstrap.h/.cpp               # monitor lines for IN#2 (R23)
├── CassoLinkSearch.h/.cpp             # the COM search (FR-083a)
└── Apple/                             # AS65 source, compiled in as text
    ├── Shim.s
    └── Program.s
CassoEmuCore/Devices/AciaEndpoints.h/.cpp   # AciaBridgeEndpoint, paced (R18)
CassoEmuCore/Seams/
├── IByteStream.h                      # NEW
├── IByteStreamFactory.h               # NEW: lists COM ports, opens each kind of stream
├── ByteStreamSettings.h/.cpp          # NEW: COM settings and TCP bind address, pure
├── InProcessByteStream.h/.cpp         # NEW
├── Win32ComPort.h/.cpp                 # NEW
└── Win32TcpStream.h/.cpp              # NEW
CassoEmuCore/Machines/Apple2/Common/   # Dos33Volume, ProDosVolume read sectors through one accessor
CassoEmuCore/CassoExplorer/CassoExplorerLinkDialog.h/.cpp   # add and edit a link
CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp   # each serial port's endpoint

UnitTest/
├── CassoExplorer/                           # NEW folder, one test file per model class
├── Dxui/                              # extended and new widget tests
├── UiTests/IntentChannelTests.cpp     # extended
└── UnitTest.vcxproj
```

**Structure Decision**: Casso Explorer's code sits under `CassoEmuCore/CassoExplorer/`,
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
   `TreeModel`, `DragPayload`, `KnownFolderStore`, `CassoExplorerPrefs`,
   `CassoTargeting`, all pure, all with tests. Byte-identity tests
   against `DiskCommandRunner` on `FakeDiskFileIo` for every verb.
3. **Casso side**: `KnownFolderStore` read in Casso's two picker call
   sites and seeded from the MRU; the two intents and the reply in
   `Win32IntentChannel` and `OnCopyData` in `EmulatorWindow.cpp`, which
   also appends every mount's folder to the known-folder store; `IProcessLauncher`
   and the Casso Explorer menu item; `DialogDefinition` image. Casso ships this
   even if Casso Explorer slips.
4. **Dxui additions**: tree extensions, list extensions, splitter, status
   bar, framebuffer view, drag source, light and dark palettes, each with
   headless tests. The capability checklist's tree tests must pass
   unchanged.
5. **Executable and entry point**: `CassoExplorer.vcxproj`, `Main.cpp`,
   `CassoExplorer.rc`, solution entries, `CassoExplorerMain.cpp` opening an empty
   `CassoExplorerShell` window. Proves the thin-exe shape early.
6. **Shell, gated on 032 merged**: `CassoExplorerCommands` over `DxuiCommand`,
   menu bar, toolbar, context menus via `DxuiPopupMenu`; layout of tree,
   list, preview, tabs, status bar with splitters; routing to the models;
   drag and drop wiring; theme switching; About with the picture.
7. **The preview's bytes**: `DxuiHexView` replaces the hex dump's list of
   strings with a byte-range selection drawn in both the hex and the text
   column, grouping at 1, 2, 4 and 8 bytes, copy either way, and go to
   offset. Text, listing, hex and disassembly previews all draw in a
   fixed-width face; an Apple II face was built from the character
   generator table and dropped, because that table is substantially
   Apple's ROM and every free reproduction is either personal-use or not
   the machine's shapes. The hex view reads its bytes through a host-supplied
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
by calling `Append`; Casso Explorer calls it on Insert, Open in new Casso and a
drag onto Casso, and on Add to Casso.

### Theme

`CassoExplorerTheme` owns six `IDxuiTheme` instances and a current pointer. The
window's `SetTheme` is called with the pointer and invalidated. Follow
system reads `DxuiWindowsThemeColors` on the settings-change notification
`DxuiHwndSource` already raises and swaps between Light and Dark.

### Apple text previews

FR-082, research R14. A disk text file whose bytes are text becomes
`PreviewContent::Kind::Text` lines through `PreviewDecoder::SplitAppleText`
instead of the hex view. A host file whose name `HostFileNaming::Parse`
matches to an Applesoft, Integer or Text suffix is read as text even in
plain ASCII, which otherwise opens in the hex view. Both go to
`DxuiTextView` in the theme's monospaced face, as other text is.

Two richer looks were built and dropped at the owner's request: the text
drawn through the CRT pass a band at a time, and a TrueType font made at
run time from the video ROM. Neither's code remains.

The monitor for pictures is chosen on the options dialog's Display page;
the command bar's Monitor drop-down was removed (FR-068).

### CassoLink

Section H, research R15 to R23, contract
[contracts/cassolink.md](contracts/cassolink.md). Phases, numbered as in
tasks.md, whose phases 10 to 15 are gap-fixing passes over sections A to G:

16. **Bridge and streams**: `IByteStream` with the in-process pair,
    `AciaBridgeEndpoint` with cycle-counted pacing, the TCP stream, and the
    endpoint as a machine setting. An emulated //c's port is reachable over
    TCP.
17. **Recordings**: ADTPro 2.1.0's host joined to its own client in an
    emulated //c over the TCP bridge; disk send, receive and a virtual
    drive captured as fixtures; the contract's *captured* values filled in.
18. **Codec and ADTPro host**: `CassoLinkCodec` and `CassoLinkHost`
    serving ADTPro's clients, tested against the recordings, then against
    ADTPro's client in an emulated //c (SC-032).
19. **Apple program**: the shim and program in AS65, `LinkProgram`, and a
    scenario test reading and writing sectors on an emulated //c.
20. **Sparse disks**: the volume classes' sector accessor, `LinkedDisk`'s
    fault-in, and the session's operations, all on the in-process stream.
21. **Casso Explorer**: the link roots, settings, the COM search,
    copy in and out, Read disk to image and Write image to disk.
21a. **The Apple's screen, and finding Casso** (R24 to R26, Session
     2026-10-16): the shim at $3D00 with its screen and its speed byte; the
     program's PC, error and label lines and the Label command; the
     running-Casso files and their roots; the dialog's rows by transport;
     the search at both speeds; the Wi-Fi modem's dial address.
21b. **Connections and the ADTPro server** (R27 to R31, Session
     2026-10-22): the Add connection dialog with the transport first, a name
     made from the settings, no duplicate added, a COM drop-down and no
     Apple-side port or slot; the Emulated transport removed and its saved
     links dropped; the Wi-Fi modem on fixed port 6400; the shim taking its
     slot from the input hook or a card scan; the CTS check; the
     ADTPro-compatible server type with its shared folder, its requests
     confined to that folder, and its help as a virtual item; the Options
     dialog's CassoLink page and `emulateAdtPro` removed; the boot disk's MAC
     salt and the W5100 read-first detection.
21c. **Settings** (R32, Session 2026-10-22), built: live serial connectors
     and the ImageWriter II's connector, the Super Serial Card's printer mode
     saved and restored, the cross-tab notices, the Settings tab strip's
     scrolling and the pinned info tips.
22. **Hardware**: `Win32ComPort`, bootstrap over `IN#2`, and the owner's
    //c walkthrough (SC-027, SC-028, SC-033).
23. **Cards, after their merges**: the Super Serial Card driver in the shim
    with the 042 merge, Uthernet II with the 043 merge, and audio bootstrap
    for a ][+ or //e with whichever lands first.
24. **Validation**: quickstart §7b's emulated steps on Debug and Release,
    with SC-029, SC-030 and SC-032 written to validation.md.

## Risks

- **032 timing**: phase 6 is the only gate; everything before it merges
  on its own if 032 runs long.
- **OLE drag on the UI thread**: `DoDragDrop` blocks; the preview timer
  and tooltip must tolerate a paused loop, as the caption-drag stopgap
  already does.
- **Prefs races**: avoided by the two-file design; the residual race is
  two Casso Explorer windows appending known folders at once, closed by
  read-merge-replace under a named mutex in `KnownFolderStore`.
- **Video modes' `MemoryBus` constructor argument**: a default-constructed
  bus is never read when `videoRam` is non-null; a test asserts that with
  a bus that fails on read.
- **Master renames mid-branch**: merge master after each phase.
- **Polled serial at 115,200 baud on a //c**: about 89 cycles per byte. The
  receive loop is cycle-counted in the emulator before it runs on the
  owner's //c; 57,600 baud is the fallback, and SC-027 is met at it with a
  smaller margin.
- **ADTPro's undocumented details**: the CRC, RLE and timeouts come from
  recordings (R15). A recording that does not settle one blocks phase 18,
  not the link: Casso's own client uses neither ADTPro's RLE nor its CRC.
- **The shim's new address**: $3D00 is in hi-res page 1. The program draws
  only text, and a program that grows past $3DFF fails the build's size
  check rather than overwriting the shim.
- **Card features slipping**: phase 23 waits on 042 and 043; everything
  before it runs on the //c.
- **Saved links from older builds**: an `Emulated` link is dropped on load,
  and any other link's name is made again from its settings; of two saved
  links with the same name, only the first is listed. The old
  `Documents\ADTPro` folder and its help file are left where they are,
  since the folder can contain the user's own disks.
- **A path that escapes the shared folder**: `..`, a drive letter, a UNC
  path or a junction. Every ADTPro path is resolved to its full form and
  compared with the folder's, after the folder's own junctions are
  resolved, and anything outside fails with ADTPro's error reply.

## Phase 2 Preview

`/speckit-tasks` generates tasks.md following the seven phases, one
commit per phase, with the 032 merge as the first task of phase 6.
