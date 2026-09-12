# Tasks: Cassque

**Input**: Design documents from `/specs/033-cassque/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. FR-030 and SC-005 demand that every decision is driven headlessly, and SC-001 is a byte-identity claim only a test can make for every verb.

**Organization**: Phases follow the plan's dependency order. Phases 1 through 6 need only master. Phase 7, the shell, is gated on merging `032-dxui-command-widgets`, and that merge is its first task. Each task carries the stories it serves.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 browse and inspect, US2 every disk operation, US3 drag and drop, US4 hand a disk to Casso, US5 tabs, keyboard and themes

## Path Conventions

- Executable: `Cassque/`; entry and shell: `CassoEmuCore/Cassque/`; models: `CassoEmuCore/Cassque/Model/`
- Codecs: `CassoCore/`; seams: `CassoEmuCore/Seams/`; widgets: `Dxui/Widgets/`, `Dxui/Window/`, `Dxui/Theme/`
- Tests: `UnitTest/Cassque/` (new), `UnitTest/Dxui/`, `UnitTest/UiTests/`, `UnitTest/EmuTests/`
- Mocks: `UnitTest/EmuTests/FakeDiskFileIo.h`, `UnitTest/UiTests/InMemoryFileSystem.h`, `UnitTest/Dxui/Mock*.h`
- Widget skeleton and headless-test pattern: `specs/013-dxui-framework-extraction/quickstart.md` §2 and §3
- Executable contract: `specs/031-thin-exe-shim/contracts/executable-project.md`

## Standing rules for every task

- Build through `Casso.sln` with `scripts/Build.ps1`; `-Target Rebuild` after any virtual is added or removed.
- EHM in every function that can fail; VerbNoun names, `OnXxx` exempt; helpers are class statics; non-ASCII via `Dxui/Core/UnicodeSymbols.h`; a new function spliced ahead of another goes ahead of its `////` banner.
- No spec, FR or task references in code comments or identifiers.
- Commit per phase; subjects in the form `feat(cassque): ...`, `feat(dxui): ...`, `feat(core): ...`, `feat(shell): ...`.
- Merge `origin/master` into the branch after each phase commit.

---

## Phase 1: Setup

**Purpose**: fixtures the byte-identity tests need, and a CLI baseline to compare against.

- [x] T001 Add to `UnitTest/Fixtures/Disks/` one DOS 3.3 and one ProDOS scratch image (with subdirectories) holding an Applesoft program, an Integer BASIC program, a text file, an 8192-byte hi-res binary at $2000, a 1024-byte lo-res binary at $400, a 16384-byte double hi-res binary, and an odd-length binary at $803; add a `LICENSE` note if any content is not repo-original
- [x] T002 [P] Add `UnitTest/Fixtures/Cassque/` host-side fixtures: an Applesoft listing, an implicit-LET listing, a numbered README that is not BASIC, an Integer BASIC listing, a printable text file, and a binary blob; plus expected outputs produced by `CassoCli disk get` with `--basic` and `--text` on the T001 images, recorded with the command lines used in `UnitTest/Fixtures/Cassque/README.md`
- [x] T003 Add `UnitTest/Cassque/` to `UnitTest/UnitTest.vcxproj` with a placeholder `CassqueFixtureTests.cpp` that loads each T001 image through `VolumeImage::Load` and asserts the expected entry count; build and run `scripts/RunTests.ps1 -Configuration Debug -Filter CassqueFixture`

---

## Phase 2: Foundational codecs (US1, US2)

**Purpose**: the two decoders the tree lacks. Blocks preview and Get for Integer BASIC and the hex pane's disassembly.

- [x] T004 [P] [US1] Create `CassoCore/IntegerBasicDetokenizer.h/.cpp`: `static HRESULT Detokenize (const std::vector<Byte> & programBytes, std::string & outListing, BasicListingError & outError)` over Integer BASIC's length-prefixed line chain and token set; refuse with the offset on a malformed line rather than emit a partial listing
- [x] T005 [P] [US1] Create `CassoCore/Disassembler.h/.cpp`: `static void Disassemble (std::span<const Byte> bytes, Word origin, bool cmos, std::vector<DisassembledLine> & out)` over `Cpu::GetInstructionSet` and `OpcodeTable::GetOperandSize`, one line per instruction with address, bytes, mnemonic and operand text; undefined opcodes render as `???` with one byte
- [x] T006 [P] [US1] Create `UnitTest/Cassque/IntegerBasicDetokenizerTests.cpp`: the T001 Integer program detokenizes to the expected listing; a truncated line reports its offset; an empty program yields an empty listing
- [x] T007 [P] [US1] Create `UnitTest/Cassque/DisassemblerTests.cpp`: every addressing mode on both NMOS and CMOS tables, an undefined opcode, and an operand cut off by the end of the buffer
- [x] T083 [P] [US2] Add `Rename (const FilePath & from, const std::string & to, std::vector<Byte> & outBuffer) const` to `CassoEmuCore/Machines/Apple2/Common/IVolume.h`, implemented in `Dos33Volume.cpp` and `ProDosVolume.cpp` as an in-place catalog edit applying each file system's name rules and refusing a collision; tests in `UnitTest/EmuTests/VolumeRenameTests.cpp` on the T001 images
- [x] T084 [P] [US1] Add `modifiedUnix` and `hasModified` to `FileEntry` in `CassoEmuCore/Machines/Apple2/Common/VolumeTypes.h`, populated by `ProDosVolume::Enumerate` from the directory entry's modification date and left clear by `Dos33Volume`; tests in `UnitTest/EmuTests/ProDosModifiedDateTests.cpp`
- [x] T008 [US1] Add the Phase 2 files to `CassoCore/CassoCore.vcxproj`, `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`; build; run `scripts/RunTests.ps1 -Configuration Debug -Filter "IntegerBasic|Disassembler|VolumeRename|ProDosModified"`
- [x] T009 Commit: `feat(core): Integer BASIC detokenizer, 6502 disassembler, volume rename, ProDOS modified date`

**Checkpoint**: both codecs exist with no consumer; tree green.

---

## Phase 3: Model classes (US1, US2, US3, US4)

**Purpose**: every decision Cassque makes, as pure classes with tests and no window. This phase is the bulk of the feature and proves SC-001.

### Naming and sniffing (US2, US3)

- [x] T010 [P] [US3] Create `CassoEmuCore/Cassque/Model/HostFileNaming.h/.cpp` per `contracts/host-file-names.md`: `ForConverted`, `ForRaw` in both styles, `Parse`, `MakeHostLegal`
- [x] T011 [P] [US3] Create `CassoEmuCore/Cassque/Model/ContentSniffer.h/.cpp` per the contract: the Applesoft line test using `ApplesoftTokenizer::GetKeyword` for the keyword check, the printable-text test, and the binary verdict with the suggested address
- [x] T012 [P] [US3] Create `UnitTest/Cassque/HostFileNamingTests.cpp`: every row of the naming table in both styles, `Parse` round-trips each, CiderPress `#TTAAAA` parses, a bare name parses to undecided, illegal characters are substituted and reported
- [x] T013 [P] [US3] Create `UnitTest/Cassque/ContentSnifferTests.cpp` on the T002 fixtures: Applesoft, implicit LET, the numbered README rejected on its first non-keyword line, the Integer listing falling to Text, printable text, binary at $2000 for 8192 bytes and $803 otherwise

### Catalog, preview, operations (US1, US2)

- [x] T014 [P] [US1] Create `CassoEmuCore/Cassque/Model/CatalogModel.h/.cpp`: `FileEntry` and host directory entries to `CatalogRow` per data-model.md, type text from DOS letters and ProDOS mnemonics, size from `eofBytes` when present else units times the unit size, address text only when the `has*` flag is set, sort by any column
- [x] T015 [P] [US1] Create `CassoEmuCore/Cassque/Model/PicturePreview.h/.cpp`: place bytes at the load address in a 64 KB span, choose `AppleHiResMode`, `AppleDoubleHiResMode` (main and aux halves) or `AppleLoResMode` by the spec's address and length rule, render into a BGRA framebuffer with a `MemoryBus` that is never read
- [x] T016 [US1] Create `CassoEmuCore/Cassque/Model/PreviewDecoder.h/.cpp`: `Decide (FileEntry, bytes)` to a `PreviewContent` kind, then render listing via `ApplesoftTokenizer::Detokenize` or `IntegerBasicDetokenizer`, text via the runner's text conversion, picture via T015, hex rows with the `Disassembler` toggle, catalog via T014, and the error kind with the decoder's offset (depends on T014, T015)
- [x] T017 [US2] Create `CassoEmuCore/Cassque/Model/DiskOperations.h/.cpp`: a facade over `DiskImageSession`, `IVolume` and `DiskCommandRunner` taking `IDiskFileIo &`; `List`, `Get`, `Put`, `Delete`, `Rename` through the session and volume; `Create`, `Init`, `Boot`, `SectorRead`, `SectorWrite`, `BlockRead`, `BlockWrite` by building a `CommandLineOptions` and calling `DiskCommandRunner::Run`; every result carries the runner's message text on failure
- [x] T018 [P] [US1] Create `UnitTest/Cassque/CatalogModelTests.cpp`: rows for each T001 entry, blank address when the flag is clear, ProDOS directory rows, host rows with modified dates, sort by each column both directions
- [x] T019 [P] [US1] Create `UnitTest/Cassque/PreviewDecoderTests.cpp`: kind per fixture, the graphics rule for each address and length pair and one near miss, hex and disassembly rows, catalog rows for an image, the error kind on a damaged program, and a `MemoryBus` stub that fails on any read to prove the picture path never touches it
- [x] T020 [US2] Create `UnitTest/Cassque/DiskOperationsTests.cpp` on `FakeDiskFileIo`: for every verb, run `DiskCommandRunner::Run` with the CLI's options and run `DiskOperations` on a second copy, then assert the resulting image bytes or payload are identical; refusal text matches the runner's

### Browser state (US1, US5)

- [x] T021 [P] [US1] Create `CassoEmuCore/Cassque/Model/TreeModel.h/.cpp`: the two roots, node ids `casso:`, `pc:`, `img:`, `dir:` per plan, children on demand through an `IFileSystem` listing and `IVolume::Enumerate`, supported-extension filtering, dimmed missing known folders, and a `loadError` node state for an image that fails to parse (no expand affordance, error text as tooltip and list-area message)
- [x] T022 [P] [US5] Create `CassoEmuCore/Cassque/Model/BrowserModel.h/.cpp`: tabs with `Location`, history, selection ids, sort state; open, close, switch, navigate, back, forward; catalog cache per tab invalidated on a write
- [x] T023 [P] [US1] Create `UnitTest/Cassque/TreeModelTests.cpp` on `InMemoryFileSystem` and the T001 images: roots, lazy children fetched once, a ProDOS image with directory children, a DOS 3.3 image with none, a missing known folder dimmed, a file with a disk extension that is not an image carrying `loadError` and no children
- [x] T024 [P] [US5] Create `UnitTest/Cassque/BrowserModelTests.cpp`: tab lifecycle, history, selection survives a sort, cache invalidation on write

### Persistence and Casso targeting (US1, US4)

- [x] T025 [P] [US1] Create `CassoEmuCore/Cassque/Model/KnownFolderStore.h/.cpp`: `Load`, `Append`, `Remove`, `SeedFromMru` over `IFileSystem`, JSON per data-model.md, read-merge-replace under a named mutex, atomic replace via a temp file
- [x] T026 [P] [US5] Create `CassoEmuCore/Cassque/Model/CassquePrefs.h/.cpp`: the fields in data-model.md, `Load` and `Save` over `IFileSystem`, first-run theme seeded read-only from `activeTheme` under `global` in `UserPrefs.json` (the effective key; the top-level one in `GlobalUserPrefs.json` is inert)
- [x] T027 [P] [US4] Create `CassoEmuCore/Cassque/Model/CassoTargeting.h/.cpp`: `Choose (owner, ownerAlive, running, defaultMachine)` per `contracts/intent-channel.md`, liveness supplied by the caller so the class stays pure, drive count from `MachineConfig::AttachedDiskIiDriveCount`
- [x] T028 [P] [US3] Create `CassoEmuCore/Cassque/Model/DragPayload.h/.cpp`: formats offered per source kind, the private `CassqueCatalogEntries` encoding, file descriptor names via `HostFileNaming`, a ProDOS directory entry expanding to nested descriptors recursively, and the delayed-render plan per descriptor index
- [x] T029 [P] [US1] Create `UnitTest/Cassque/KnownFolderStoreTests.cpp`: seed from MRU distinct folders, append dedupes case-insensitively, remove, missing folder retained, write is a temp-and-replace on the mock
- [x] T030 [P] [US5] Create `UnitTest/Cassque/CassquePrefsTests.cpp`: defaults, round-trip, theme seeded from Casso's file on first run only
- [x] T031 [P] [US4] Create `UnitTest/Cassque/CassoTargetingTests.cpp`: owner with `ownerAlive` true, owner with it false and one running, several running picks the most recent, none running yields Launch with the default machine's drive count
- [x] T032 [P] [US3] Create `UnitTest/Cassque/DragPayloadTests.cpp`: Apple-to-Apple offers the private format and `CF_HDROP` for images only; host-bound offers descriptors with the naming rule; a directory entry yields nested descriptors with relative paths; a DOS 3.3 target refuses folders
- [x] T033 Add every Phase 3 file to `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`; build; run `scripts/RunTests.ps1 -Configuration Debug -Filter Cassque`
- [x] T034 Commit: `feat(cassque): browser model classes with byte-identity tests against the disk runner`

**Checkpoint**: every decision in the feature is implemented and tested; no window exists.

---

## Phase 4: Casso side (US1, US4)

**Purpose**: what the emulator needs so Cassque can talk to it and share folders. Ships on its own if Cassque slips.

- [x] T035 [US4] Add `InsertDisk` and `DescribeMachine` to `CassoCore/ExternalChangeIntent.h`; extend `CassoEmuCore/Seams/Win32IntentChannel.h/.cpp` with the drive byte on `InsertDisk`, a `CassoIntentReply` message id, `EncodeReply` and `DecodeReply` for `MachineDescription`, `InsertDone`, `InsertRefused`, `ReloadDone`, `ReloadConflict` and `ReloadRefused`, and `SendTo (HWND, payload)` for a targeted send; keep `StateIntent` void
- [x] T036 [US4] In `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` `OnCopyData`, handle `InsertDisk` by routing to the same eject-then-mount path the drive widget uses and replying `InsertDone` or `InsertRefused` with the store's reason, `DescribeMachine` by replying the display name and `AttachedDiskIiDriveCount`, and `ReloadInPlace` with a sender window by replying `ReloadDone`, `ReloadConflict` or `ReloadRefused` from the external-change policy's decision; in the same file's OLE drop path and the insert path, call `KnownFolderStore::Append` with the mounted image's folder, so every hand-off from any source is recorded by Casso; message filter already allows `WM_COPYDATA`
- [x] T037 [P] [US4] Create `CassoEmuCore/Seams/IProcessLauncher.h`, `Win32ProcessLauncher.h/.cpp`, and `UnitTest/Cassque/FakeProcessLauncher.h`
- [x] T038 [US4] Add a Cassque command to the emulator's command table and the Disk menu, launching `Cassque.exe` from the module directory with `--owner <hwnd>` through `IProcessLauncher`; when `Cassque.exe` is absent the command shows the runner-style message and does nothing else
- [x] T039 [US1] In `CassoEmuCore/Gui/GuiMain.cpp` and `CassoEmuCore/Shell/WindowCommandManager.cpp` picker call sites, load `KnownFolderStore`, seed it from the MRU when the file is absent, and append its folders to the picker before the existing `AppendSiblingDisksFromMruFolders`; the `Append` on every mount is T036's job, so the picker only reads
- [x] T040 [P] [US5] Add the optional `DialogImage` to `CassoEmuCore/Ui/Dialogs/DialogDefinition.h` and paint it in `DialogBodyContent.cpp` above the body via `DrawIconBitmap`; Casso's About is unchanged
- [x] T041 [P] [US4] Extend `UnitTest/UiTests/IntentChannelTests.cpp`: encode and decode of the two intents and six replies, size cap, unknown-byte refusal preserved
- [x] T042 [P] [US1] Add to `UnitTest/UiTests/` a test that the picker's folder list is the union of `KnownFolderStore` and the MRU's distinct folders, in that order, on `InMemoryFileSystem`
- [x] T043 Build; run the full suite Debug; launch Casso in the background with `--title 033-cassque`, insert a disk, confirm `KnownFolders.json` gains its folder; close Casso
- [x] T044 Commit: `feat(shell): insert and describe intents, known-folder store, process launcher, dialog image`

**Checkpoint**: Casso answers `DescribeMachine` and accepts `InsertDisk`; its picker reads the shared file.

---

## Phase 5: Dxui additions (US1, US3, US5)

**Purpose**: the general widgets the layout needs, per `contracts/dxui-additions.md`, each tested headlessly.

- [x] T045 [P] [US1] Extend `Dxui/Widgets/DxuiTreeView.h/.cpp`: node `id`, `childrenLoaded`, `dimmed`; `SetShowCheckboxes`, `SetChildProvider`, `SetOnSelect`, `SetOnExpand`; Right expands, Left collapses or moves to parent; existing `SetOnToggle` untouched
- [x] T046 [P] [US1] Extend `Dxui/Widgets/DxuiListView.h/.cpp`: `SetMultiSelect`, `GetSelectedRows`, `SetSelectedRows`, Ctrl and Shift semantics, Ctrl+A, `SetOnSort` on header click, `SetOnActivate` on Enter and double-click
- [x] T047 [P] [US1] Create `Dxui/Widgets/DxuiSplitter.h/.cpp` per contract
- [x] T048 [P] [US1] Create `Dxui/Widgets/DxuiStatusBar.h/.cpp` per contract
- [x] T049 [P] [US1] Create `Dxui/Widgets/DxuiFramebufferView.h/.cpp` painting through `DxuiTextRenderer::DrawFramebuffer` with integer scaling
- [x] T050 [P] [US3] Create `Dxui/Window/DxuiDragDropSource.h/.cpp`: `IDropSource` and `IDataObject` over a format list with on-demand rendering of `FILECONTENTS` streams; `Begin` wraps `DoDragDrop`
- [x] T051 [P] [US5] Create `Dxui/Theme/DxuiLightTheme.h/.cpp` and `DxuiDarkTheme.h/.cpp` from the Fluent tokens in `DxuiWindowsThemeColors`
- [x] T052 [P] [US1] Extend `UnitTest/UiTests/DxuiTreeViewTests.cpp` for ids, lazy children fetched once, checkbox hiding, arrow keys; existing checklist tests unchanged
- [x] T053 [P] [US1] Extend `UnitTest/Dxui/DxuiListViewTests.cpp` (or create it) for multi-select semantics, sort callback, activate callback
- [x] T054 [P] [US1] Create `UnitTest/Dxui/DxuiSplitterTests.cpp`, `DxuiStatusBarTests.cpp`, `DxuiFramebufferViewTests.cpp` on the mocks
- [x] T055 [P] [US3] Create `UnitTest/Dxui/DxuiDragDropSourceTests.cpp`: a fake consumer enumerates formats, pulls a `FILECONTENTS` stream by index and observes the render callback called once per index
- [x] T056 Add all Phase 5 files to `Dxui/Dxui.vcxproj` and `UnitTest/UnitTest.vcxproj`; build with `-Target Rebuild`; run `scripts/RunTests.ps1 -Configuration Debug -Filter Dxui`; grep `Dxui/` for `Cassque` and `CassoEmuCore` and confirm no hits
- [x] T057 Commit: `feat(dxui): lazy tree, multi-select list, splitter, status bar, framebuffer view, drag source, light and dark palettes`

**Checkpoint**: every widget the layout needs exists with tests; Casso's chrome is untouched.

---

## Phase 6: Executable and entry point (US1)

**Purpose**: prove the thin-executable shape before the shell has content.

- [x] T058 [US1] Create `Cassque/Cassque.vcxproj` from `Casso/Casso.vcxproj` minus the mesh bake: one `ClCompile` for a comment-only `Cassque/Main.cpp`, one `ResourceCompile` for `Cassque/Cassque.rc`, `EntryPointSymbol=wWinMainCRTStartup` in all six configurations, references to CassoCore, CassoEmuCore and Dxui, same OutDir and IntDir
- [x] T059 [P] [US1] Create `Cassque/Cassque.rc` with an application icon and the cassowary PNG as RCDATA; add their ids to `CassoEmuCore/resource.h`; add the PNG under `Cassque/Assets/` with a note on its source and license
- [x] T060 [US1] Add the project to `Casso.sln` with its twelve configuration lines
- [x] T061 [US1] Create `CassoEmuCore/Cassque/CassqueMain.cpp` with `wWinMain` in GuiMain's order: DPI awareness, EHM hooks, command line (`--owner`, `--title`), asset bootstrap for themes, `CassquePrefs::Load`, then a `CassqueShell` that opens an empty `CassqueWindow` of class `CassqueWindow` and runs the message loop; a second launch with a matching `--owner` fronts the existing window and exits
- [x] T062 [US1] Create `CassoEmuCore/Cassque/CassqueShell.h/.cpp` with window creation, `OleInitialize`, theme pointer, placement from prefs, and nothing else yet
- [x] T063 [US1] Build Debug and Release x64; run `Cassque.exe --title 033-cassque` in the background and confirm a window appears; confirm `UnitTest.vcxproj` gained no reference to `Cassque.vcxproj`; count functions in `Cassque/Main.cpp` and assert zero
- [x] T064 Commit: `feat(cassque): thin executable and entry point`

**Checkpoint**: an empty Cassque window launches from a zero-function executable.

---

## Phase 7: Shell (US1, US2, US3, US4, US5), gated on 032

**Purpose**: the window over the models and widgets. Nothing here is a decision; every branch in this phase forwards to a model.

- [x] T065 Merge `origin/032-dxui-command-widgets` (or master once it has shipped) into the branch; rebuild with `-Target Rebuild`; run the full suite
- [ ] T066 [US1] Create `CassoEmuCore/Cassque/CassqueCommands.h/.cpp`: the `DxuiCommand` table for File, Edit, View, Disk, Casso and Help menus and the toolbar placement; `isEnabled` functors read `BrowserModel` selection and image write-protect state
- [x] T067 [US1] In `CassqueShell`, lay out the menu bar, toolbar, tab strip, tree, splitter, list, splitter, preview, status bar with `DxuiDockLayout`; persist splitter positions and preview visibility through `CassquePrefs`
- [x] T068 [US1] Wire `TreeModel` to the tree widget through the child provider and `OnSelect`; wire `CatalogModel` rows to the list with sort and multi-select; wire `PreviewDecoder` to the preview list or framebuffer view; wire the status bar fields from the selection; invalidate the current tab's catalog cache and refresh on window activation so a second window's edits show on focus
- [ ] T069 [US2] Wire context menus via `DxuiPopupMenu::Show` for files, images, directories and folders: Get, Put, Delete, Rename (also F2, inline in the list), New Disk, Format, Boot, Advanced submenu with sector and block verbs, Add to Casso, Remove from Casso, Insert into Drive 1 and 2, Open in new Casso; confirmations on delete, format, overwriting put, and sector and block writes; every operation through `DiskOperations`
- [x] T070 [US2] Add the put dialog (name, type, address) and the new-disk dialog (container, format, volume, bootable) as `DialogDefinition` forms, prefilled per `ContentSniffer` and the runner's defaults
- [ ] T071 [US3] Wire drag-out through `DxuiDragDropSource` from `DragPayload`, and drag-in through `DxuiDragDropTarget` on the list and tree with the `CF_HDROP` and private-format paths, folder recursion and the DOS 3.3 folder refusal, no-drop on write-protected targets
- [ ] T072 [US4] Wire `CassoTargeting` with `ownerAlive` from `IsWindow`, the intent sends and replies, `InsertRefused`, `ReloadConflict` and `ReloadRefused` as dialogs, `Append` to `KnownFolderStore` only for Open in new Casso (the running-instance paths are recorded by Casso in T036), and `Open in new Casso` through `IProcessLauncher`; the reload intent with the sender window after every write to an image
- [ ] T073 [US5] Wire tabs (Ctrl+T, Ctrl+W, Ctrl+Tab), back and forward (Alt+Left, Alt+Right, toolbar buttons), open-tab restore from `CassquePrefs` on launch, per-tab preview scroll and disassembly toggle, full keyboard navigation, Shift+F10 and the application key, Alt+P; `CassqueTheme` with the six choices and Follow system on the settings-change notification; Help, About with the cassowary image and the name explanation
- [x] T074 [US1] Set an accessible name and role on every control in the shell
- [ ] T075 Build Debug and Release x64; run the full suite; run Cassque in the background with `--title 033-cassque` and capture the window with `scripts/CaptureScreenshotMatrix.ps1`'s capture step for a first visual check in each theme
- [ ] T076 Commit: `feat(cassque): browser shell over the models and command widgets`

**Checkpoint**: every story is reachable in the window.

---

## Phase 7a: The preview's bytes (US6)

**Purpose**: a hex view a user can select in, as a widget the library gains. An Apple II face was built from the character generator table and dropped: that table is substantially Apple's ROM, and every free reproduction is either personal-use or not the machine's shapes. Text, listing, hex and disassembly all draw in a fixed-width face instead.

- [x] T088 [P] [US6] `Dxui/Widgets/DxuiHexView.h/.cpp`: the offset, hex and text columns, grouping at 1, 2, 4 and 8 bytes, the point-to-byte hit test, the byte-range selection, scrolling and `EnsureByteVisible` -- geometry and selection only, no painting. The bytes arrive through a source the host supplies, addressed from an origin, with a per-byte mark for anything the host draws differently; the widget holds no copy and reads only the rows it draws, so a machine's whole memory costs what a screenful does
- [x] T089 [US6] `DxuiHexView` input: drag in either column, Shift with a click or an arrow key to extend, arrows, Home, End, Page keys and Ctrl+A, and the wheel
- [x] T090 [US6] `DxuiHexView` painting: both columns from the theme, the selection lit in both at once, the offsets down the left, both columns in the fixed-width face
- [x] T091 [US6] `DxuiHexView` copy and go to offset: one Copy that yields digits or characters by the column the caret is in, on Ctrl+C and through a context menu the host raises, with `DxuiClipboard` extracted from `DxuiTextInput` so there is one clipboard path in the library; `GoToOffset` puts the caret on an offset and scrolls to it
- [ ] T092 [US6] Cassque's hex preview becomes a `DxuiHexView` instead of a list of strings, with the per-tab preview scroll kept
- [ ] T093 [P] [US6] `UnitTest/Dxui/DxuiHexViewTests.cpp`: which bytes a point selects, what a selection lights in each column, what a regrouping does to it, what each copy yields, the ends of the file, and a 64 KB source at a non-zero origin that counts the bytes it is asked for, proving only the drawn rows are read
- [ ] T094 [US6] Extend `quickstart.md` with the story 6 walk-through

- [ ] T091a [US6] Cassque wiring for the above: Edit > Copy and a context menu on the selection, Go to offset, and the grouping choice in the View menu persisted through `CassquePrefs`

**Checkpoint**: a hex preview can be selected in, regrouped and copied from, in either column.

---

## Phase 7b: Native chrome, tabs and the address bar (US1, US5)

**Purpose**: what walking the built window turned up. The parity work belongs to Dxui, so Casso inherits every fix; the tab and address-bar work is chrome Cassque needs and the library does not have yet.

- [ ] T095 [US5] Measure Dxui's Windows light and dark themes against File Explorer at 100% and 200%: text faces and sizes, list background and edge grays, header rendering, row hover, tree chevrons. Capture both windows and correct the palettes and metrics in Dxui until a side-by-side names no difference (FR-031, SC-009)
- [ ] T096 [P] [US5] `DxuiSplitter`: a hairline to the eye over a grab band far wider than the line, Explorer's proportions; the grab must not follow the drawn width (FR-032)
- [ ] T097 [P] [US1] Explorer's iconography for Back, Forward, Up and Refresh, through `UnicodeSymbols.h` (FR-033)
- [ ] T098 [US1] The preview toggle moves to the toolbar's trailing end, which `DxuiToolbar` needs a trailing group for (FR-034)
- [ ] T099 [P] [US1] Cut a long name off rather than wrapping it, in tree rows, list cells and tabs (FR-035)
- [ ] T100 [US5] `DxuiTabStrip`: reach every tab past the point where they stop fitting, and reorder by dragging (FR-036, FR-037, SC-010)
- [ ] T101 [US5] A new tab opens from the strip's own affordance, as a browser opens one, with the toolbar button retired (FR-037)
- [ ] T102 [US1] `DxuiAddressBar`: the location as navigable segments, editable into a typed path, reaching a directory inside an image as readily as a host folder; `BrowserModel` parses and formats both (FR-038, SC-011)
- [ ] T103 [US5] Let a window put its tab strip outside the menu bar, toolbar and address bar or inside them, and take the browser's arrangement in Cassque (FR-039)

**Checkpoint**: the window reads as a native Windows browser, and its tabs and address bar behave as one.

---

## Phase 8: Validation and gates

- [ ] T077 Walk `quickstart.md` §1 through §6 on a Release x64 build and record outcomes in `specs/033-cassque/validation.md`, including every `fc /b` result
- [ ] T078 [P] Run `quickstart.md` §7 headless tests Debug and Release and §8 structural checks; record in `validation.md`
- [ ] T079 [P] Add one line under `[Unreleased]` in `CHANGELOG.md` for the net effect, and a headline line in `README.md`; both shown for approval before push
- [ ] T080 `git add -A` then `scripts/CheckStyle.ps1 -Mode Tree`; fix every hit
- [ ] T081 `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for all four configurations; zero warnings
- [ ] T082 Merge `origin/master`, rebuild, rerun the suite; present every commit subject and the CHANGELOG and README lines for approval before the master merge

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 first. Phase 2 needs Phase 1 fixtures. Phase 3 needs Phase 2.
- Phases 4 and 5 need only Phase 3 and are independent of each other.
- Phase 6 needs Phase 5 (theme palettes) and Phase 3 (prefs).
- Phase 7 needs Phases 4, 5, 6 and the 032 merge.
- Phase 8 needs Phase 7.

### Story Dependencies

- US1 spans codecs, model, tree and list widgets, executable and shell.
- US2 is Phase 3's `DiskOperations` plus Phase 7's menus and dialogs.
- US3 is Phase 3's `DragPayload`, Phase 5's drag source, Phase 7's wiring.
- US4 is Phase 3's targeting, Phase 4's Casso side, Phase 7's wiring.
- US5 is Phase 3's prefs and browser model, Phase 5's palettes, Phase 7's wiring.

### Parallel Opportunities

- Phase 2: T004 through T007 together.
- Phase 3: every `[P]` model and its test together; T016 after T014 and T015; T020 after T017.
- Phase 4 and Phase 5 as a whole, on two worktrees.
- Phase 5: all seven widget tasks and their tests together.

---

## Implementation Strategy

### Merge after any checkpoint

Phases 1 through 6 each leave master's behavior unchanged and can merge on their own. Phase 4 in particular is worth merging early: it gives Casso the shared known-folder file and the intents, which other tools can use before Cassque exists.

### Minimum useful slice

Phases 1 through 6 plus a reduced Phase 7 of T065 through T068: a read-only catalog browser with preview. That is Story 1 alone, and it is already the tool the spec's background describes as missing.

### Master merges

After each phase commit, merge `origin/master`; the tree takes sweeping renames and the cost compounds.

---

## Notes

- `DiskOperationsTests` is the proof of SC-001; a verb without a byte-identity test is not done.
- No adapter keeps an old type alive for a test's sake; existing tests are retyped at construction sites only.
- The 032 merge is T065, not earlier; everything before it must build against master alone.
- T083 and T084 execute inside Phase 2 alongside T004 through T007 despite their numbers.
- Casso is the one writer of hand-offs to `KnownFolders.json` (T036); Cassque appends only for Open in new Casso and Add to Casso.
