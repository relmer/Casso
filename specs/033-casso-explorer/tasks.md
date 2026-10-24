# Tasks: Casso Explorer

**Input**: Design documents from `/specs/033-cassque/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. FR-030 and SC-005 demand that every decision is driven headlessly, and SC-001 is a byte-identity claim only a test can make for every verb.

**Organization**: Phases follow the plan's dependency order. Phases 1 through 6 need only master. Phase 7, the shell, is gated on merging `032-dxui-command-widgets`, and that merge is its first task. Each task carries the stories it serves.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 browse and inspect, US2 every disk operation, US3 drag and drop, US4 hand a disk to Casso, US5 tabs, keyboard and themes

## Path Conventions

- Executable: `CassoExplorer/`; entry and shell: `CassoEmuCore/CassoExplorer/`; models: `CassoEmuCore/CassoExplorer/Model/`
- Codecs: `CassoCore/`; seams: `CassoEmuCore/Seams/`; widgets: `Dxui/Widgets/`, `Dxui/Window/`, `Dxui/Theme/`
- Tests: `UnitTest/CassoExplorer/` (new), `UnitTest/Dxui/`, `UnitTest/UiTests/`, `UnitTest/EmuTests/`
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
- [x] T002 [P] Add `UnitTest/Fixtures/CassoExplorer/` host-side fixtures: an Applesoft listing, an implicit-LET listing, a numbered README that is not BASIC, an Integer BASIC listing, a printable text file, and a binary blob; plus expected outputs produced by `CassoCli disk get` with `--basic` and `--text` on the T001 images, recorded with the command lines used in `UnitTest/Fixtures/CassoExplorer/README.md`
- [x] T003 Add `UnitTest/CassoExplorer/` to `UnitTest/UnitTest.vcxproj` with a placeholder `CassoExplorerFixtureTests.cpp` that loads each T001 image through `VolumeImage::Load` and asserts the expected entry count; build and run `scripts/RunTests.ps1 -Configuration Debug -Filter CassoExplorerFixture`

---

## Phase 2: Foundational codecs (US1, US2)

**Purpose**: the two decoders the tree lacks. Blocks preview and Get for Integer BASIC and the hex pane's disassembly.

- [x] T004 [P] [US1] Create `CassoCore/IntegerBasicDetokenizer.h/.cpp`: `static HRESULT Detokenize (const std::vector<Byte> & programBytes, std::string & outListing, BasicListingError & outError)` over Integer BASIC's length-prefixed line chain and token set; refuse with the offset on a malformed line rather than emit a partial listing
- [x] T005 [P] [US1] Create `CassoCore/Disassembler.h/.cpp`: `static void Disassemble (std::span<const Byte> bytes, Word origin, bool cmos, std::vector<DisassembledLine> & out)` over `Cpu::GetInstructionSet` and `OpcodeTable::GetOperandSize`, one line per instruction with address, bytes, mnemonic and operand text; undefined opcodes render as `???` with one byte
- [x] T006 [P] [US1] Create `UnitTest/CassoExplorer/IntegerBasicDetokenizerTests.cpp`: the T001 Integer program detokenizes to the expected listing; a truncated line reports its offset; an empty program yields an empty listing
- [x] T007 [P] [US1] Create `UnitTest/CassoExplorer/DisassemblerTests.cpp`: every addressing mode on both NMOS and CMOS tables, an undefined opcode, and an operand cut off by the end of the buffer
- [x] T083 [P] [US2] Add `Rename (const FilePath & from, const std::string & to, std::vector<Byte> & outBuffer) const` to `CassoEmuCore/Machines/Apple2/Common/IVolume.h`, implemented in `Dos33Volume.cpp` and `ProDosVolume.cpp` as an in-place catalog edit applying each file system's name rules and refusing a collision; tests in `UnitTest/EmuTests/VolumeRenameTests.cpp` on the T001 images
- [x] T084 [P] [US1] Add `modifiedUnix` and `hasModified` to `FileEntry` in `CassoEmuCore/Machines/Apple2/Common/VolumeTypes.h`, populated by `ProDosVolume::Enumerate` from the directory entry's modification date and left clear by `Dos33Volume`; tests in `UnitTest/EmuTests/ProDosModifiedDateTests.cpp`
- [x] T008 [US1] Add the Phase 2 files to `CassoCore/CassoCore.vcxproj`, `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`; build; run `scripts/RunTests.ps1 -Configuration Debug -Filter "IntegerBasic|Disassembler|VolumeRename|ProDosModified"`
- [x] T009 Commit: `feat(core): Integer BASIC detokenizer, 6502 disassembler, volume rename, ProDOS modified date`

**Checkpoint**: both codecs exist with no consumer; tree green.

---

## Phase 3: Model classes (US1, US2, US3, US4)

**Purpose**: every decision Casso Explorer makes, as pure classes with tests and no window. This phase is the bulk of the feature and proves SC-001.

### Naming and sniffing (US2, US3)

- [x] T010 [P] [US3] Create `CassoEmuCore/CassoExplorer/Model/HostFileNaming.h/.cpp` per `contracts/host-file-names.md`: `ForConverted`, `ForRaw` in both styles, `Parse`, `MakeHostLegal`
- [x] T011 [P] [US3] Create `CassoEmuCore/CassoExplorer/Model/ContentSniffer.h/.cpp` per the contract: the Applesoft line test using `ApplesoftTokenizer::GetKeyword` for the keyword check, the printable-text test, and the binary verdict with the suggested address
- [x] T012 [P] [US3] Create `UnitTest/CassoExplorer/HostFileNamingTests.cpp`: every row of the naming table in both styles, `Parse` round-trips each, CiderPress `#TTAAAA` parses, a bare name parses to undecided, illegal characters are substituted and reported
- [x] T013 [P] [US3] Create `UnitTest/CassoExplorer/ContentSnifferTests.cpp` on the T002 fixtures: Applesoft, implicit LET, the numbered README rejected on its first non-keyword line, the Integer listing falling to Text, printable text, binary at $2000 for 8192 bytes and $803 otherwise

### Catalog, preview, operations (US1, US2)

- [x] T014 [P] [US1] Create `CassoEmuCore/CassoExplorer/Model/CatalogModel.h/.cpp`: `FileEntry` and host directory entries to `CatalogRow` per data-model.md, type text from DOS letters and ProDOS mnemonics, size from `eofBytes` when present else units times the unit size, address text only when the `has*` flag is set, sort by any column
- [x] T015 [P] [US1] Create `CassoEmuCore/CassoExplorer/Model/PicturePreview.h/.cpp`: place bytes at the load address in a 64 KB span, choose `AppleHiResMode`, `AppleDoubleHiResMode` (main and aux halves) or `AppleLoResMode` by the spec's address and length rule, render into a BGRA framebuffer with a `MemoryBus` that is never read
- [x] T016 [US1] Create `CassoEmuCore/CassoExplorer/Model/PreviewDecoder.h/.cpp`: `Decide (FileEntry, bytes)` to a `PreviewContent` kind, then render listing via `ApplesoftTokenizer::Detokenize` or `IntegerBasicDetokenizer`, text via the runner's text conversion, picture via T015, hex rows with the `Disassembler` toggle, catalog via T014, and the error kind with the decoder's offset (depends on T014, T015)
- [x] T017 [US2] Create `CassoEmuCore/CassoExplorer/Model/DiskOperations.h/.cpp`: a facade over `DiskImageSession`, `IVolume` and `DiskCommandRunner` taking `IDiskFileIo &`; `List`, `Get`, `Put`, `Delete`, `Rename` through the session and volume; `Create`, `Init`, `Boot`, `SectorRead`, `SectorWrite`, `BlockRead`, `BlockWrite` by building a `CommandLineOptions` and calling `DiskCommandRunner::Run`; every result carries the runner's message text on failure
- [x] T018 [P] [US1] Create `UnitTest/CassoExplorer/CatalogModelTests.cpp`: rows for each T001 entry, blank address when the flag is clear, ProDOS directory rows, host rows with modified dates, sort by each column both directions
- [x] T019 [P] [US1] Create `UnitTest/CassoExplorer/PreviewDecoderTests.cpp`: kind per fixture, the graphics rule for each address and length pair and one near miss, hex and disassembly rows, catalog rows for an image, the error kind on a damaged program, and a `MemoryBus` stub that fails on any read to prove the picture path never touches it
- [x] T020 [US2] Create `UnitTest/CassoExplorer/DiskOperationsTests.cpp` on `FakeDiskFileIo`: for every verb, run `DiskCommandRunner::Run` with the CLI's options and run `DiskOperations` on a second copy, then assert the resulting image bytes or payload are identical; refusal text matches the runner's

### Browser state (US1, US5)

- [x] T021 [P] [US1] Create `CassoEmuCore/CassoExplorer/Model/TreeModel.h/.cpp`: the two roots, node ids `casso:`, `pc:`, `img:`, `dir:` per plan, children on demand through an `IFileSystem` listing and `IVolume::Enumerate`, supported-extension filtering, dimmed missing known folders, and a `loadError` node state for an image that fails to parse (no expand affordance, error text as tooltip and list-area message)
- [x] T022 [P] [US5] Create `CassoEmuCore/CassoExplorer/Model/BrowserModel.h/.cpp`: tabs with `Location`, history, selection ids, sort state; open, close, switch, navigate, back, forward; catalog cache per tab invalidated on a write
- [x] T023 [P] [US1] Create `UnitTest/CassoExplorer/TreeModelTests.cpp` on `InMemoryFileSystem` and the T001 images: roots, lazy children fetched once, a ProDOS image with directory children, a DOS 3.3 image with none, a missing known folder dimmed, a file with a disk extension that is not an image carrying `loadError` and no children
- [x] T024 [P] [US5] Create `UnitTest/CassoExplorer/BrowserModelTests.cpp`: tab lifecycle, history, selection survives a sort, cache invalidation on write

### Persistence and Casso targeting (US1, US4)

- [x] T025 [P] [US1] Create `CassoEmuCore/CassoExplorer/Model/KnownFolderStore.h/.cpp`: `Load`, `Append`, `Remove`, `SeedFromMru` over `IFileSystem`, JSON per data-model.md, read-merge-replace under a named mutex, atomic replace via a temp file
- [x] T026 [P] [US5] Create `CassoEmuCore/CassoExplorer/Model/CassoExplorerPrefs.h/.cpp`: the fields in data-model.md, `Load` and `Save` over `IFileSystem`, first-run theme seeded read-only from `activeTheme` under `global` in `UserPrefs.json` (the effective key; the top-level one in `GlobalUserPrefs.json` is inert)
- [x] T027 [P] [US4] Create `CassoEmuCore/CassoExplorer/Model/CassoTargeting.h/.cpp`: `Choose (owner, ownerAlive, running, defaultMachine)` per `contracts/intent-channel.md`, liveness supplied by the caller so the class stays pure, drive count from `MachineConfig::AttachedDiskIiDriveCount`
- [x] T028 [P] [US3] Create `CassoEmuCore/CassoExplorer/Model/DragPayload.h/.cpp`: formats offered per source kind, the private `CassoExplorerCatalogEntries` encoding, file descriptor names via `HostFileNaming`, a ProDOS directory entry expanding to nested descriptors recursively, and the delayed-render plan per descriptor index
- [x] T029 [P] [US1] Create `UnitTest/CassoExplorer/KnownFolderStoreTests.cpp`: seed from MRU distinct folders, append dedupes case-insensitively, remove, missing folder retained, write is a temp-and-replace on the mock
- [x] T030 [P] [US5] Create `UnitTest/CassoExplorer/CassoExplorerPrefsTests.cpp`: defaults, round-trip, theme seeded from Casso's file on first run only
- [x] T031 [P] [US4] Create `UnitTest/CassoExplorer/CassoTargetingTests.cpp`: owner with `ownerAlive` true, owner with it false and one running, several running picks the most recent, none running yields Launch with the default machine's drive count
- [x] T032 [P] [US3] Create `UnitTest/CassoExplorer/DragPayloadTests.cpp`: Apple-to-Apple offers the private format and `CF_HDROP` for images only; host-bound offers descriptors with the naming rule; a directory entry yields nested descriptors with relative paths; a DOS 3.3 target refuses folders
- [x] T033 Add every Phase 3 file to `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`; build; run `scripts/RunTests.ps1 -Configuration Debug -Filter Casso Explorer`
- [x] T034 Commit: `feat(cassque): browser model classes with byte-identity tests against the disk runner`

**Checkpoint**: every decision in the feature is implemented and tested; no window exists.

---

## Phase 4: Casso side (US1, US4)

**Purpose**: what the emulator needs so Casso Explorer can talk to it and share folders. Ships on its own if Casso Explorer slips.

- [x] T035 [US4] Add `InsertDisk` and `DescribeMachine` to `CassoCore/ExternalChangeIntent.h`; extend `CassoEmuCore/Seams/Win32IntentChannel.h/.cpp` with the drive byte on `InsertDisk`, a `CassoIntentReply` message id, `EncodeReply` and `DecodeReply` for `MachineDescription`, `InsertDone`, `InsertRefused`, `ReloadDone`, `ReloadConflict` and `ReloadRefused`, and `SendTo (HWND, payload)` for a targeted send; keep `StateIntent` void
- [x] T036 [US4] In `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` `OnCopyData`, handle `InsertDisk` by routing to the same eject-then-mount path the drive widget uses and replying `InsertDone` or `InsertRefused` with the store's reason, `DescribeMachine` by replying the display name and `AttachedDiskIiDriveCount`, and `ReloadInPlace` with a sender window by replying `ReloadDone`, `ReloadConflict` or `ReloadRefused` from the external-change policy's decision; in the same file's OLE drop path and the insert path, call `KnownFolderStore::Append` with the mounted image's folder, so every hand-off from any source is recorded by Casso; message filter already allows `WM_COPYDATA`
- [x] T037 [P] [US4] Create `CassoEmuCore/Seams/IProcessLauncher.h`, `Win32ProcessLauncher.h/.cpp`, and `UnitTest/CassoExplorer/FakeProcessLauncher.h`
- [x] T038 [US4] Add a Casso Explorer command to the emulator's command table and the Disk menu, launching `CassoExplorer.exe` from the module directory with `--owner <hwnd>` through `IProcessLauncher`; when `CassoExplorer.exe` is absent the command shows the runner-style message and does nothing else
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
- [x] T056 Add all Phase 5 files to `Dxui/Dxui.vcxproj` and `UnitTest/UnitTest.vcxproj`; build with `-Target Rebuild`; run `scripts/RunTests.ps1 -Configuration Debug -Filter Dxui`; grep `Dxui/` for `CassoExplorer` and `CassoEmuCore` and confirm no hits
- [x] T057 Commit: `feat(dxui): lazy tree, multi-select list, splitter, status bar, framebuffer view, drag source, light and dark palettes`

**Checkpoint**: every widget the layout needs exists with tests; Casso's chrome is untouched.

---

## Phase 6: Executable and entry point (US1)

**Purpose**: prove the thin-executable shape before the shell has content.

- [x] T058 [US1] Create `CassoExplorer/CassoExplorer.vcxproj` from `Casso/Casso.vcxproj` minus the mesh bake: one `ClCompile` for a comment-only `CassoExplorer/Main.cpp`, one `ResourceCompile` for `CassoExplorer/CassoExplorer.rc`, `EntryPointSymbol=wWinMainCRTStartup` in all six configurations, references to CassoCore, CassoEmuCore and Dxui, same OutDir and IntDir
- [x] T059 [P] [US1] Create `CassoExplorer/CassoExplorer.rc` with an application icon and the cassowary PNG as RCDATA; add their ids to `CassoEmuCore/resource.h`; add the PNG under `CassoExplorer/Assets/` with a note on its source and license
- [x] T060 [US1] Add the project to `Casso.sln` with its twelve configuration lines
- [x] T061 [US1] Create `CassoEmuCore/CassoExplorer/CassoExplorerMain.cpp` with `wWinMain` in GuiMain's order: DPI awareness, EHM hooks, command line (`--owner`, `--title`), asset bootstrap for themes, `CassoExplorerPrefs::Load`, then a `CassoExplorerShell` that opens an empty `CassoExplorerWindow` of class `CassoExplorerWindow` and runs the message loop; a second launch with a matching `--owner` fronts the existing window and exits
- [x] T062 [US1] Create `CassoEmuCore/CassoExplorer/CassoExplorerShell.h/.cpp` with window creation, `OleInitialize`, theme pointer, placement from prefs, and nothing else yet
- [x] T063 [US1] Build Debug and Release x64; run `CassoExplorer.exe --title 033-cassque` in the background and confirm a window appears; confirm `UnitTest.vcxproj` gained no reference to `CassoExplorer.vcxproj`; count functions in `CassoExplorer/Main.cpp` and assert zero
- [x] T064 Commit: `feat(cassque): thin executable and entry point`

**Checkpoint**: an empty Casso Explorer window launches from a zero-function executable.

---

## Phase 7: Shell (US1, US2, US3, US4, US5), gated on 032

**Purpose**: the window over the models and widgets. Nothing here is a decision; every branch in this phase forwards to a model.

- [x] T065 Merge `origin/032-dxui-command-widgets` (or master once it has shipped) into the branch; rebuild with `-Target Rebuild`; run the full suite
- [x] T066 [US1] Create `CassoEmuCore/CassoExplorer/CassoExplorerCommands.h/.cpp`: the `DxuiCommand` table for the command bar, context menus and keyboard; `isEnabled` functors read `BrowserModel` selection and image write-protect state. The menu bar it first fed is retired by T109
- [x] T067 [US1] In `CassoExplorerShell`, lay out the menu bar, toolbar, tab strip, tree, splitter, list, splitter, preview, status bar with `DxuiDockLayout`; persist splitter positions and preview visibility through `CassoExplorerPrefs`
- [x] T068 [US1] Wire `TreeModel` to the tree widget through the child provider and `OnSelect`; wire `CatalogModel` rows to the list with sort and multi-select; wire `PreviewDecoder` to the preview list or framebuffer view; wire the status bar fields from the selection; invalidate the current tab's catalog cache and refresh on window activation so a second window's edits show on focus
- [x] T069 [US2] Wire context menus via `DxuiPopupMenu::Show` for files, images, directories and folders: Get, Put, Delete, Rename (also F2, inline in the list), New Disk, Format, Boot, Advanced submenu with sector and block verbs, Add to Casso, Remove from Casso, Insert into Drive 1 and 2, Open in new Casso; confirmations on delete, format, overwriting put, and sector and block writes; every operation through `DiskOperations` -- done 2026-09-17: Explorer-style icon row, inline rename, host Cut/Copy/Paste/Delete/Rename through IFileOperation, Open with, Share, Show more options
- [x] T070 [US2] Add the put dialog (name, type, address) and the new-disk dialog (container, format, volume, bootable) as `DialogDefinition` forms, prefilled per `ContentSniffer` and the runner's defaults
- [x] T071 [US3] Wire drag-out through `DxuiDragDropSource` from `DragPayload`, and drag-in through `DxuiDragDropTarget` on the list and tree with the `CF_HDROP` and private-format paths, folder recursion and the DOS 3.3 folder refusal, no-drop on write-protected targets -- done 2026-09-17: drops on list and tree, private format with types, folder recursion, DOS 3.3 refusal, no-drop on read-only images
- [x] T072 [US4] Wire `CassoTargeting` with `ownerAlive` from `IsWindow`, the intent sends and replies, `InsertRefused`, `ReloadConflict` and `ReloadRefused` as dialogs, `Append` to `KnownFolderStore` only for Open in new Casso (the running-instance paths are recorded by Casso in T036), and `Open in new Casso` through `IProcessLauncher`; the reload intent with the sender window after every write to an image -- done 2026-09-17: CassoTargeting chooses the target; every write states ReloadInPlace
- [x] T073 [US5] Wire tabs (Ctrl+T, Ctrl+W, Ctrl+Tab), back and forward (Alt+Left, Alt+Right, toolbar buttons), open-tab restore from `CassoExplorerPrefs` on launch, per-tab preview scroll and disassembly toggle, full keyboard navigation, Shift+F10 and the application key, Alt+P; `CassoExplorerTheme` with the six choices and Follow system on the settings-change notification; Help, About with the cassowary image and the name explanation -- done 2026-09-17: Follow system reacts to WM_SETTINGCHANGE; Alt+P and per-tab disassembly were already in
- [x] T074 [US1] Set an accessible name and role on every control in the shell
- [x] T075 Build Debug and Release x64; run the full suite; run Casso Explorer in the background with `--title 033-cassque` and capture the window with `scripts/CaptureScreenshotMatrix.ps1`'s capture step for a first visual check in each theme
- [x] T076 Commit: `feat(cassque): browser shell over the models and command widgets`

**Checkpoint**: every story is reachable in the window.

---

## Phase 7a: The preview's bytes (US6)

**Purpose**: a hex view a user can select in, as a widget the library gains. An Apple II face was built from the character generator table and dropped: that table is substantially Apple's ROM, and every free reproduction is either personal-use or not the machine's shapes. Text, listing, hex and disassembly all draw in a fixed-width face instead.

- [x] T088 [P] [US6] `Dxui/Widgets/DxuiHexView.h/.cpp`: the offset, hex and text columns, grouping at 1, 2, 4 and 8 bytes, the point-to-byte hit test, the byte-range selection, scrolling and `EnsureByteVisible` -- geometry and selection only, no painting. The bytes arrive through a source the host supplies, addressed from an origin, with a per-byte mark for anything the host draws differently; the widget holds no copy and reads only the rows it draws, so a machine's whole memory costs what a screenful does
- [x] T089 [US6] `DxuiHexView` input: drag in either column, Shift with a click or an arrow key to extend, arrows, Home, End, Page keys and Ctrl+A, and the wheel
- [x] T090 [US6] `DxuiHexView` painting: both columns from the theme, the selection lit in both at once, the offsets down the left, both columns in the fixed-width face
- [x] T091 [US6] `DxuiHexView` copy and go to offset: one Copy that yields digits or characters by the column the caret is in, on Ctrl+C and through a context menu the host raises, with `DxuiClipboard` extracted from `DxuiTextInput` so there is one clipboard path in the library; `GoToOffset` puts the caret on an offset and scrolls to it
- [x] T091a [US6] Casso Explorer wiring for the above: Edit > Copy and a context menu on the selection, Go to offset, and the grouping choice in the View menu persisted through `CassoExplorerPrefs`
- [x] T092 [US6] Casso Explorer's hex preview becomes a `DxuiHexView` instead of a list of strings, with the per-tab preview scroll kept
- [x] T093 [P] [US6] `UnitTest/Dxui/DxuiHexViewTests.cpp`: which bytes a point selects, what a selection lights in each column, what a regrouping does to it, what each copy yields, the ends of the file, and a 64 KB source at a non-zero origin that counts the bytes it is asked for, proving only the drawn rows are read
- [x] T094 [US6] Extend `quickstart.md` with the story 6 walk-through

**Checkpoint**: a hex preview can be selected in, regrouped and copied from, in either column.

---

## Phase 7b: Native chrome, tabs and the address bar (US1, US5)

**Purpose**: what walking the built window turned up. The parity work belongs to Dxui, so Casso inherits every fix; the tab and address-bar work is chrome Casso Explorer needs and the library does not have yet.

- [x] T095 [US5] Measure Dxui's Windows light and dark themes against File Explorer at 100% and 200%: text faces and sizes, list background and edge grays, header rendering, row hover, tree chevrons. Capture both windows and correct the palettes and metrics in Dxui until a side-by-side shows no difference (FR-031, SC-009) -- status 2026-09-25: the dark theme is measured at 100% to 200% (research.md), including row selection, focus and hover; the light theme measured on 2026-09-25 at 125% and 200% (research.md, The light theme), and its selection, inactive selection, column title lines and tab strip line matched; a pressed row needs a click and was not measured
- [x] T096 [P] [US5] `DxuiSplitter`: a hairline to the eye over a grab band far wider than the line, Explorer's proportions; the grab must not follow the drawn width (FR-032)
- [x] T097 [P] [US1] Explorer's iconography for Back, Forward, Up and Refresh, through `UnicodeSymbols.h` (FR-033)
- [x] T098 [US1] The preview toggle moves to the toolbar's trailing end, which `DxuiToolbar` needs a trailing group for (FR-034)
- [x] T099 [P] [US1] Cut a long name off rather than wrapping it, in tree rows, list cells and tabs (FR-035)
- [x] T100 [US5] `DxuiTabStrip`: reach every tab past the point where they stop fitting, and reorder by dragging (FR-036, FR-037, SC-010)
- [x] T101 [US5] A new tab opens from the strip's own affordance, as a browser opens one, with the toolbar button retired (FR-037)
- [x] T102 [US1] `DxuiAddressBar`: the location as navigable segments, editable into a typed path, reaching a directory inside an image as readily as a host folder; `BrowserModel` parses and formats both (FR-038, SC-011)
- [x] T103 [US5] Let a window put its tab strip outside the toolbar and address bar or inside them, and take the browser's arrangement in Casso Explorer: tabs on top, then the navigation toolbar and address bar on one row, then the command bar (FR-039, FR-049)
- [x] T125 [US5] Measure File Explorer's tabs: shape, full and minimum width, icon, label inset, close button, + button, and the selected tab joining the row below; recorded in `research.md` R13 at 120 dpi in the dark theme, light and 200% still to measure
- [x] T126 [US5] `DxuiTabStrip` in Explorer's form from T125: fixed width shrinking to the minimum before scrolling, left-aligned cut-off label, icon, close button, and a + button after the scroll arrows (FR-048)
- [x] T127 [US1] Tab icons and labels from the location: the folder's or drive's shell icon and display name, an image's icon and name (FR-048)

**Checkpoint**: the window reads as a native Windows browser, and its tabs and address bar behave as one.

---

## Phase 7c: AppleSingle (US2, US3, US6)

- [x] T104 [US2] `AppleSingleCodec` in CassoEmuCore: read and write the container (data fork, real name, ProDOS type and aux type, dates), with no knowledge of memory or disks; round-trip and malformed-header tests. Shared with 035-debugger (research R-016): whichever spec implements first builds it, and tells the other -- built by 035-debugger with the dates entry; on master as 325bfbb5
- [x] T105 [US2] Put and drag-in: an AppleSingle host file unwraps through `CassoExplorerActions::PlanPut` with its type and aux type and no dialog, and is never taken for a disk image (FR-017c)
- [x] T106 [US2] Get and drag-out in the AppleSingle style: `HostFileNaming` gains it and the Host file names setting offers it (FR-017a, FR-017c)
- [x] T107 [US3] A right-drag out of an image opens a menu choosing the style for that drop (FR-017c)
- [x] T108 [US6] The preview of an AppleSingle host file shows the name, type, aux type and dates of the file it holds (FR-017c)

---

## Phase 7d: Explorer's command bar (US1, US2, US5)

**Purpose**: the menu bar gives way to File Explorer's command bar, the file list gains Explorer's eight views, and settings move to an Options dialog.

- [x] T109 [US1] Measure File Explorer's command bar at 100% and 200%: buttons, order, icons, labels, spacing, dropdown arrows, and the overflow when the window narrows; record it in `research.md` -- done 2026-09-23: 100%, 125%, 150% and 200% in research.md, File Explorer's command bar and list, measured
- [x] T110 [US1] Retire Casso Explorer's menu bar; the command bar takes its row, built on `DxuiToolbar` with Explorer's metrics from T109, including its overflow (FR-041, SC-012) -- status 2026-09-17: the menu bar is gone and the command bar has its own row under the address row, with See more; its spacing is the toolbar's own until Explorer can be measured (T109), which needs an unlocked screen -- done 2026-09-23: every strip, button pitch, separator and row matches Explorer to within 2 pixels at each scale, and buttons that do not fit move whole into See more
- [x] T111 [US2] Cut, Copy, Paste and Delete buttons with Explorer's icons over the file list's selection; New tab, Close tab and Exit removed (FR-042)
- [x] T112 [P] [US1] `DxuiListView` views beyond details -- Explorer's extra large, large, medium and small icons, list, tiles and content -- each with selection, keyboard navigation, drag and context menus; headless tests for hit testing and arrow-key movement in each (FR-044)
- [x] T113 [US1] Sort and View dropdowns over the list, the view choice persisted through `CassoExplorerPrefs`; Preview toggle and Theme dropdown at the bar's trailing end (FR-043, FR-044, FR-027)
- [x] T114 [US2] Options dialog opened from View > Options, holding Host file names; commands the preview's context menu offers leave the top level (FR-045, FR-017a)
- [x] T115 [P] [US5] Typed path history: the model in CassoEmuCore (ten entries, newest first, a repeat moves to the top, added only after a successful navigation, browsing adds nothing) with headless tests, persisted through `CassoExplorerPrefs` (FR-040, SC-013)
- [x] T116 [US5] `DxuiAddressBar` dropdown offering the typed path history (FR-040)
- [x] T117 [P] [US2] `ProDosVolume` walks directories: every file operation takes a multi-part `FilePath` (today each refuses anything but one part), creates a subdirectory, grows a subdirectory's blocks as entries are added, and keeps the parent's file count; headless tests on the T001 ProDOS fixture. FR-017b's folder drop onto a ProDOS image needs it too (FR-013b, FR-013d)
- [x] T121 [US2] Command-line grammar: paths on every file verb with the relative and full-path rules, `list --recurse`/`-r`/`-s`, `mkdir`/`md` creating missing directories along the path, `rmdir`/`rd`, `delete` refusing directories; usage text and `docs` updated; parser and runner tests (FR-013b, FR-013c, FR-013d, FR-013f) — done but for the full-path rule: a leading slash is parsed and a relative path works, and nothing yet checks `/VOLNAME/...` against the image's own volume name. `docs` is not updated either
- [x] T122 [US2] Recursive delete plan in the core: list, locked marks, blocks freed, refusal on a locked entry without `--force` or an unreadable directory, applied all or nothing on a staged image; `rmdir --recurse --force --yes` and the no-terminal refusal in the runner; tests (FR-013e)
- [x] T123 [US2] Casso Explorer's directory delete through the same plan, its confirmation showing the list and totals (FR-013e, FR-015)
- [x] T124 [P] [US1] ProDOS name case: case-free matching, GS/OS lowercase flags read for display and written on create and rename, after confirming the field layout against Apple's IIgs technical note; tests (FR-013g) — entry names only. GS/OS Technical Note #8 puts a volume header's own case bits at +$1A, and CiderPress2's format notes put them at +$16; the two disagree, nothing here reads a volume name's case, so that field is left alone
- [x] T118 [US2] Context-sensitive New: New folder on host folders and ProDOS locations, a new disk image of each supported container on host folders, disabled on DOS 3.3; an unused default name that opens for renaming; a new image made with host files selected receives them, refused up front when they will not fit (FR-046) -- done 2026-09-17: New submenu, default names opening for rename, a new image receives the selection with a fit check first; revised 2026-10-18 per the spec's Session 2026-10-18: a new image is created empty, whatever is selected
- [x] T119 [US2] Rename on the command bar under the holding file system's rules, host names included (FR-042, FR-013a)
- [x] T120 [US1] `DxuiToolbar` See more menu: overflow as the window narrows and items pinned there permanently (FR-047)

---

## Phase 8: Validation and gates

- [X] T077 Walk `quickstart.md` §1 through §6 on a Release x64 build and record outcomes in `specs/033-casso-explorer/validation.md`, including every `fc /b` result -- walked 2026-10-18 on x64 Release, and the last three steps on 2026-10-20: §5 with no Casso running, §5 with a single-drive default machine, and §6 Windows dark mode
- [x] T078 [P] Run `quickstart.md` §7 headless tests Debug and Release and §8 structural checks; record in `validation.md`
- [ ] T079 [P] Add one line under `[Unreleased]` in `CHANGELOG.md` for the net effect, and a headline line in `README.md`; both shown for approval before push
- [x] T080 `git add -A` then `scripts/CheckStyle.ps1 -Mode Tree`; fix every hit
- [x] T081 `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for all four configurations; zero warnings
- [ ] T082 Merge `origin/master`, rebuild, rerun the suite; present every commit subject and the CHANGELOG and README lines for approval before the master merge

---

## Phase 9: Shipping

**Goal**: An installed Casso has Casso Explorer with it. These precede T082, since the master merge is what releases them.

- [x] T128 `scripts/StageRelease.ps1` stages `CassoExplorer.exe` beside `Casso.exe` and `CassoCli.exe`, and reads its dependencies so the CRT libraries it needs are staged too (FR-050)
- [x] T129 [P] `scripts/GenerateMsixAssets.ps1` builds a second set of tile pictures from `Resources/Icons/CassoExplorer.png`, under their own names, checked in beside Casso's (FR-051)
- [x] T130 `Installer/Package.appxmanifest` declares Casso Explorer as an application of its own: its display name, the tile pictures from T129, and an execution alias for `CassoExplorer.exe` (FR-051)
- [ ] T131 Install the built package on a machine with no build of Casso on it; start Casso Explorer from the Start menu, from its executable's name at a prompt, and from Casso's Browse disks command; unpack the zip on the same machine and open an image. Record both in `validation.md` (SC-014, SC-015)

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

Phases 1 through 6 each leave master's behavior unchanged and can merge on their own. Phase 4 in particular is worth merging early: it gives Casso the shared known-folder file and the intents, which other tools can use before Casso Explorer exists.

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
- Casso is the one writer of hand-offs to `KnownFolders.json` (T036); Casso Explorer appends only for Open in new Casso and Add to Casso.

## Phase 10: Convergence

- [x] T132 Add a shell-folder `Location` kind (parsing name plus display path) that lists any shell folder's items through the shell namespace in `CassoExplorerBrowser`, with address, tabs, history and prefs following it, and the disk-image verbs offered only on file items, per FR-054 (missing)
- [x] T133 Build File Explorer's navigation sections under Casso's root in `TreeModel` and the tree: Home, Gallery, the OneDrive root, divider, Quick access, the shell's cloud roots, This PC, Libraries, Network, Linux, each only where present, per FR-053 and US7/AC1 (missing)
- [x] T134 List Quick access from the shell (pinned, then frequent) and add Pin to Quick access and Unpin from Quick access to the list's and tree's menus through the shell's own verbs, per FR-055 and US7/AC2 (missing)
- [x] T135 Make every disk-image verb work on UNC and mapped-drive paths (address parsing, tree reveal, watchers, writes), and show a share that cannot be reached as an error in the list without blocking the window, per FR-056, US7/AC3 and Edge Cases (partial)
- [x] T136 Search box per File Explorer: query syntax over the location and below, from the Windows search index where indexed and a background folder walk where not, results with their folders in the list, sortable and groupable, stoppable, and an empty-result message, per FR-059, US7/AC6 and Edge Cases (missing)
- [x] T137 Add the navigation pane's empty-area menu: Show This PC, Show Network, Show libraries, Show all folders (checked, kept in prefs) and Expand to current folder, per FR-057 and US7/AC4 (missing)
- [x] T138 Show the current location's icon at the address bar's left with a chevron that drops the shell's desktop roots, then the user's desktop folders, each with its icon, navigating on choice, per FR-058 and US7/AC5 (missing)
- [X] T139 Extend search results with name matches inside disk images in the searched location, and in Casso's own folders from Casso's root, each opening, previewing and copying as in its image, per FR-060 (missing)
- [x] T140 Record SC-016 (navigation sections and pins side by side with File Explorer) and SC-017 (first results within 2 s over 1,000 indexed files, same host files as File Explorer) in `validation.md`, per SC-016 and SC-017 (missing)


## Phase 11: Convergence

- [x] T141 Offer Register and Unregister in Casso for .dsk, .do, .po, .woz, .nib and .nb2, per user with no administrator rights, writing Casso's icon, Open in Casso with the image in its first drive, and Casso Explorer under Open with, and recording each format's previous program so Unregister gives it back, per FR-061, FR-062, FR-063 and US8/AC1-3 (missing)
- [x] T142 Make each of the navigation pane's five options follow File Explorer's own setting until it is set in Casso Explorer, then keep Casso Explorer's value for that option alone, never writing File Explorer's settings, per FR-064 and US8/AC4 (missing)
- [x] T143 Draw a sample set of Apple file-type icons (text, Applesoft, Integer BASIC, binary, system, directory, relocatable, generic) and show them for entries inside images in every view, per FR-065, US8/AC5 and SC-019 (missing)
- [x] T144 Test registration against a scratch registry root, or remove everything the test wrote and compare associations before and after, per FR-066 (missing)
- [x] T145 Read disk folders on a network path off the UI thread so that an unreachable share never hangs the window, per FR-056 and Edge Cases (partial)
- [x] T146 Complete Show all folders: show the Desktop root holding the pane's other roots, and Control Panel, as File Explorer does, per FR-057 (partial)
- [x] T147 Show the OneDrive root with the shell's cloud icon, per FR-053 (partial)
- [x] T148 Show the pin icon on pinned folders in the navigation pane, per FR-055 (partial)
- [x] T149 Match File Explorer's Content view type line and right column position, per FR-044 and SC-009 (partial)
- [x] T150 Match File Explorer's wrap width for names without spaces in Medium icons, per SC-009 (partial)
- [x] T151 Record SC-018 and SC-019 in `validation.md`, per SC-018 and SC-019 (missing)
- [x] T152 Give the drop-down and context menus File Explorer's acrylic backdrop (blurred, tinted desktop behind the card; the popup sized to the card with the system's own rounded corners and shadow), and any other surface where File Explorer uses acrylic, matched by capture in light and dark, per SC-009 (missing)

## Phase 12: Convergence

- [x] T153 Draw picture previews (hi-res, double hi-res, lo-res) through Casso's CRT post-process (CassoEmuCore/CrtPostProcess.h) with the chosen monitor and effects, so a preview matches Casso's screen; listing, text, hex and catalog previews stay as they are, per FR-067 and SC-020 (missing)
- [x] T154 Give Casso Explorer its own monitor type and CRT effect settings in CassoExplorerPrefs, kept across starts and never read from or written to Casso's, defaulting to each Casso theme's own and to green with the retro theme's effects under Light, Dark and Follow Windows, per FR-070, FR-071 and US9/AC3, US9/AC5 (missing)
- [x] T155 Let Casso's DisplayPage be hosted outside Casso: put SettingsDisplayCrtBridge's bindings (prefs, theme, panel state, emulator shell) behind what a host supplies, and add a Display page to Casso Explorer's options dialog that edits T154's settings and redraws the preview, per FR-069 and US9/AC2 (missing)
- [x] T156 Add a monitor drop-down beside Theme on Casso Explorer's command bar listing Casso's monitor styles, as Casso's toolbar has one, switching the preview at once, per FR-068 and US9/AC1 (missing)
- [x] T157 Add a color/monochrome switch to a double hi-res picture's preview, overriding the automatic choice while that picture is shown, per FR-072 and US9/AC4 (missing)
- [x] T158 Measure SC-020 (a picture in the preview and on Casso's screen, same monitor and effects, compared pixel for pixel at the same size) and SC-021 (redraw within a quarter second on a monitor switch; the choice survives a restart), and record both in validation.md, per SC-020 and SC-021 (missing)

## Phase 13: Convergence

- [X] T159 Host Windows' preview handler for a host file of a type that has one, in the Preview pane where File Explorer draws it, with Casso Explorer's own previews first for Apple files and disk images and the bytes for a file nothing previews, per FR-073 and US10/AC1-3 (missing)
- [X] T160 Resize a shell preview with the pane, hide it with the pane, unload it when the selection moves, load it off the window's thread, and draw the bytes instead when it fails or takes longer than File Explorer allows, per FR-074, US10/AC4 and Edge Cases (missing)
- [X] T161 Match File Explorer's first-level context menu for files, folders and selections: the clipboard row, the same commands in the same order with the same icons, Open with as a submenu, and the commands other programs add, per FR-075, US11/AC1 and US11/AC3 (partial)
- [X] T162 Match File Explorer's context menu for a navigation tree node, item for item, per FR-075 and US11/AC2 (partial)
- [X] T163 Offer Extract All on the first level of an archive's menu and on the command bar when an archive is selected or open, as File Explorer does, per FR-078, US12/AC4 and SC-024 (partial)
- [X] T164 Move the selection in an archive's preview with the arrow keys, Home, End and Page Up and Down, with Shift extending it, as in the file list, per FR-078 (partial)
- [X] T165 Report a damaged or password-protected archive with the reason, in the preview and in the list, rather than as an empty folder, per Edge Cases (partial)
- [X] T166 List an archive on a network path for its preview off the window's thread, listing it as unavailable when the share is offline, per Edge Cases (partial)
- [X] T167 Measure SC-022 (a .docx, a .pdf and a .png previewed as File Explorer previews them, within a second), SC-023 (the same first-level menu items in the same order for a file, a folder and a tree node), SC-024 (open a zip, preview a file in it and extract it in under a minute) and SC-025 (highlight and words under the pointer over each target kind), and record each in validation.md, per SC-022, SC-023, SC-024 and SC-025 (missing)

## Phase 14: Convergence

- [X] T168 Add `AppleTextFont` in `CassoEmuCore/CassoExplorer/Model/`, loading the first of `Apple2eEnhanced_Video.rom`, `Apple2e_Video.rom`, `Apple2c_Video.rom` and `Apple2_Video.rom` through `PathResolver`, else the built-in table, mapping a character to a primary-set code, folding lowercase to uppercase for a ROM without it and drawing anything outside printable ASCII as `?`, with tests, per FR-083 and research R14 (missing)
- [X] T169 Add `AppleTextLayout::Wrap` in `CassoEmuCore/CassoExplorer/Model/`, wrapping lines at a column count with an empty line as one empty row, with tests, per FR-082 and the data model (missing)
- [X] T170 Set `PreviewContent::appleText` on Applesoft and Integer BASIC listings, on a disk text file in place of its hex view (its bytes on the hex toggle), and on a host file whose name `HostFileNaming::Parse` matches to an Applesoft, Integer or Text suffix, with tests, per FR-082 and the plan's Apple text section (missing)
- [X] T171 Add `SetVirtualHeight`, `SetOnScroll` and `GetScrollTop` to `DxuiFramebufferView`, with the scrollbar and wheel ranging over the virtual height, with headless tests, per contracts/dxui-additions.md (missing)
- [X] T172 Draw `appleText` content in `CassoExplorerWindow` through the framebuffer view: columns from the pane width and a cell scaled to the theme's text line height, the band in view drawn white on black from `AppleTextFont`, tinted by `PicturePreview::Tint` and passed through `CrtPictureRenderer` with the picture's monitor, effects and glow margin, redrawn on layout, scroll and monitor or effect change, per FR-082, US9/AC6 and SC-026 (missing)
- [X] T173 Test that hex views, catalogs and host text files other than exported Apple files stay in the theme's font on every monitor, that a 2 KB ROM draws lowercase as uppercase, and that no ROM falls back to the built-in table without an error, per FR-067 and Edge Cases (missing)
- [X] T174 Copy the whole Apple text with Ctrl+C while it is drawn on the monitor, with a test, per the plan's Risks (missing)
- [X] T175 Walk quickstart section 7a on x64 Debug and Release and record SC-026 in validation.md, per SC-026 (missing)

## Phase 15: Convergence

- [X] T176 Draw Apple text in `DxuiTextView` in a font made at run time from the video ROM (`AppleFontFile`, `DxuiTextRenderer::AddPrivateFont`, `DxuiTextView::SetFace` and `SetColors`), in the monitor's color on black without the display effects, selectable and copyable, in place of the CRT band of T172, per FR-082 and the Session 2026-10-13 clarifications (contradicts)
- [X] T177 Remove `DxuiFramebufferView`'s virtual height, unused once Apple text left the CRT band, at the owner's request, and apply the text view's zoom after the face is rounded to its pixel grid, per FR-082 (unrequested)
- [X] T178 Remove the ROM font and its Dxui face, color and private-font support, and draw Apple text in the theme's monospaced face, per FR-082 as revised on 2026-10-13 (contradicts)
- [X] T179 Remove the command bar's Monitor drop-down, leaving the monitor on the options dialog's Display page, per FR-068 as revised on 2026-10-13 (contradicts)

---

# Section H: CassoLink

**Input**: spec.md section H (User Stories 14 to 16, FR-083 to FR-098 with
FR-083a, SC-027 to SC-033), plan.md's section H phases 16 to 24, research.md R15 to
R23, [contracts/cassolink.md](contracts/cassolink.md), data-model.md's
CassoLink section, quickstart.md §7b.

**Stories**:

- US14: files and disks to and from a real Apple II;
- US15: start from a bare Apple and stay current;
- US16: the link inside Casso.

**Tests**: required, as for sections A to G. No unit test opens a COM port
or a socket. Every transport sits behind `IByteStream`, and scenario tests
run the Apple program in a `TestMachine` over the in-process stream.

**Order**:

- Phases 16 to 22 run on this branch alone.
- Phase 23 waits on the 042 (Super Serial Card) and 043 (Uthernet II)
  merges.
- Phase 24 closes the section.
- Commit per phase, with subjects such as `feat(explorer): ...`,
  `feat(emu): ...` and `test(link): ...`.

## Phase 16: Bridge and streams (US16)

**Purpose**: an emulated //c's serial port reachable from the PC side,
in-process and over TCP, at real line timing.

- [X] T180 Add `UnitTest/CassoLink/` to `UnitTest/UnitTest.vcxproj`. `ScenarioTests/CassoLink*.cpp` and `CassoEmuCore/CassoLink/` enter their projects with their first files, so neither has a placeholder
- [X] T181 [US16] Add the stream seam:
  - `IByteStream` in `CassoEmuCore/Seams/IByteStream.h`: open, close, write, read with a timeout in milliseconds, readable count, closed;
  - `IByteStreamFactory` in `CassoEmuCore/Seams/IByteStreamFactory.h`: list COM ports, open a COM port, listen on or connect to a TCP port;
  - `InProcessByteStream` in `CassoEmuCore/Seams/InProcessByteStream.h/.cpp`: two thread-safe linked ends over queues;
  - tests in `UnitTest/CassoLink/InProcessByteStreamTests.cpp`.

  Per R19 and FR-093.
- [X] T182 [US16] Add `AciaBridgeEndpoint` to `CassoEmuCore/Devices/AciaEndpoints.h/.cpp`, over an `IByteStream`:
  - **Receive**: pass `Acia6551::ReceiveByte` one byte at a time, never sooner than one character time at the baud rate and word length set in the 6551 after the last. A byte the guest has not read when the next arrives is overrun, as on a real line.
  - **Transmit**: send the guest's bytes out at the same rate, through a shift register and the data register, with the transmit-empty flag clearing and setting at that timing.
  - **Clock**: count machine cycles, not wall time.
  - **Byte log**: optional, with a direction mark on each byte, for captures.
  - **Acia6551**: add only what it lacks, a paced transmit-empty flag for an endpoint that paces; `MachineHost` ticks the bridge with the machine's cycles.
  - **Tests** in `UnitTest/EmuTests/AciaBridgeEndpointTests.cpp`: pacing at 9,600 and 115,200 baud, no overrun for a guest that reads each byte in time, an overrun for one that does not, and transmit pacing.

  Per FR-095 and R18.
- [X] T183 [US16] Make each //c serial port's endpoint a machine setting in `CassoEmuCore/Shell/MachineBuilder.cpp` and `CassoEmuCore/Shell/MachineHost.h`:
  - loopback by default;
  - a bridge to an in-process stream, a TCP listen port, or a COM port.

  Extend `UnitTest/EmuTests/Apple2cBootTests.cpp` with a //c whose port 2 bridges to an `InProcessByteStream` end and exchanges bytes with the test through the guest's own 6551 code. Per FR-095.
- [X] T184 [P] [US16] Add `Win32TcpStream` in `CassoEmuCore/Seams/Win32TcpStream.h/.cpp`:
  - Winsock listen and connect, one connection at a time;
  - bound to 127.0.0.1 unless the caller selects the network;
  - the port released on close;
  - the bind address chosen by `ByteStreamSettings::GetBindAddress` in `CassoEmuCore/Seams/ByteStreamSettings.h/.cpp`, a pure function from the transport, so the choice is tested without a socket.

  Tests of `GetBindAddress` in `UnitTest/CassoLink/ByteStreamSettingsTests.cpp`: 127.0.0.1 for every transport but `WifiModem`, and the network for `WifiModem`. Per FR-097, R19 and constitution II.
- [X] T185 [P] [US16] Add `Win32ComPort` in `CassoEmuCore/Seams/Win32ComPort.h/.cpp`:
  - `CreateFile` on `\\.\COMn`, opened exclusively;
  - `SetCommState` for speed, data format and hardware handshake;
  - `SetCommTimeouts` for the read timeout;
  - the port list from `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM`, through `IByteStreamFactory`;
  - the port's settings built by `ByteStreamSettings::ToDcb`, a pure function from the speed, data format and handshake, so they are tested without a port.

  Tests of `ToDcb` in `UnitTest/CassoLink/ByteStreamSettingsTests.cpp`: 9,600 and 115,200 baud, eight data bits, no parity, one stop bit, with and without hardware handshake. Per FR-093, FR-094, R19 and constitution II.
- [X] T186 [US16] Offer each emulated serial port's endpoint (loopback, a TCP listen port, or a PC COM port) on Casso's Hardware settings page, in `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp`, saved with the machine's settings and connected while the machine runs, with no reset, as a cable is plugged in. A port is opened only while the machine runs and is released when it stops or its device changes. Per FR-095 and FR-097.

## Phase 17: ADTPro captures (US16)

**Purpose**: settle what ADTPro's protocol document omits, from its
own host and client, never from its source (FR-096).

- [X] T187 [US16] Capture traffic between ADTPro 2.1.0's host (`C:\Users\relmer\Downloads\ADTPro-2.1.0`, its serial-over-IP transport) and its own client disk booted in an emulated //c on T186's TCP port, through T182's byte log, written to a file by Casso's undocumented `--serial-log [file]` (by default a timestamped file in `%LOCALAPPDATA%\Casso\SerialLogs\`) (`CassoEmuCore/Devices/SerialTrafficLog.h/.cpp`):
  - a disk sent to the host and one received from it;
  - a Directory and a Size query;
  - virtual-drive block reads, timestamped reads and writes.

  Save each capture with its direction marks in `UnitTest/Fixtures/CassoLink/`, with a `LICENSE` note stating that ADTPro is GPL and the captures are test fixtures that ship in nothing. The owner runs this once, manually, on a machine which has ADTPro's Java host; the captures are checked in and CI only reads them. Per R15.
- [X] T188 [US16] From T187's captures, settle each value in `specs/033-casso-explorer/contracts/cassolink.md` marked *captured*, replacing the mark:
  - the length field's byte order;
  - the CRC's polynomial, initial value and bit order;
  - the RLE encoding of image pages;
  - the virtual drive's block EOR scope;
  - the timeout and retry counts.

  Per R15.

## Phase 18: Codec and ADTPro host (US14)

**Purpose**: Casso serves an unmodified ADTPro client (FR-088).

- [X] T189 [P] [US14] Add `CassoLinkCodec` in `CassoEmuCore/CassoLink/CassoLinkCodec.h/.cpp`:
  - the envelope: start `$C1`, a two-byte length, the command, and an EOR of those four bytes;
  - the payload EOR;
  - Send Image Data with T188's RLE and CRC;
  - Sector data (`$CA`): 256 bytes unencoded with CRC-16/XMODEM (polynomial `$1021`, initial value 0);
  - Acknowledgement (`$CB`) with ACK `$06` and NAK `$15`.

  Tests in `UnitTest/CassoLink/CassoLinkCodecTests.cpp` decode every T187 capture, round-trip every packet kind, and fail on damaged envelopes, damaged payloads and bad CRCs. Per FR-087 and the contract.
- [X] T190 [US14] Add `CassoLinkHost` in `CassoEmuCore/CassoLink/CassoLinkHost.h/.cpp`, serving ADTPro's clients:
  - CD, Directory, Size, Get, Put, Batch, Multiple, Summary, Home and Send File;
  - virtual drives (`$C5`) over 512-byte blocks: Read, Read with timestamp, Write;
  - Nibble with ADTPro's abort reply.

  Tests in `UnitTest/CassoLink/CassoLinkHostTests.cpp` replay T187's client side over an `InProcessByteStream` and compare the host's replies with the captures. Per FR-088 and US14/AC7.
- [X] T191 [US14] Set `CassoLinkHost`'s image folder from Casso Explorer (`CassoLinkFolder` in `CassoEmuCore/CassoLink/CassoLinkFolder.h/.cpp`; `CassoLinkSession`, T201, passes it the tab's location):
  - the host folder open in the current tab;
  - for a tab inside a disk image, the folder the image is in;
  - with no host folder open, such as This PC, each request fails with ADTPro's unable status.

  Tests in `UnitTest/CassoLink/CassoLinkHostTests.cpp`. Per FR-088.
- [X] T192 [US14] Add retries and failure handling to `CassoLinkHost`:
  - resend on NAK up to the retry limit, then fail with the reason;
  - close the link when no byte arrives within the timeout;
  - commit an image file only after its last packet, through `IDiskFileIo`'s atomic replace, so no file exists after a failed transfer.

  Tests for each, including a stream closed mid-transfer followed by a second transfer that succeeds with neither end restarted. Per FR-087, SC-030 and the edge cases.
- [X] T193 [US16] Add `ScenarioTests/CassoLinkAdtProTests.cpp`:
  - boot ADTPro 2.1.0's client disk (a GPL fixture under `UnitTest/Fixtures/CassoLink/` with its `LICENSE` note) in a //c `TestMachine`;
  - send a disk to `CassoLinkHost` and receive one from it over T182's bridge;
  - compare each image byte for byte with the one ADTPro's own host made in T187.

  Per SC-032 and US16/AC4.

## Phase 19: The Apple program (US16)

**Purpose**: Casso's own client, a shim and a sector server, built from
source in the core (R16, R17, R22).

- [X] T194 [US16] Write `CassoEmuCore/CassoLink/Apple/Shim.s` in AS65 syntax:
  - resident at `$0300`, under one page;
  - a polled driver for the //c's port 2 6551;
  - Ready (`$D2`) with the program version (2 bytes), the shim version (1) and the drive map (1);
  - Load program (`$CF`): Sector data pages to `$0800`, then a jump to `$0800`.

  Per FR-091 and R17.
- [X] T195 [US16] Write `CassoEmuCore/CassoLink/Apple/Program.s` in AS65 syntax, linked at `$0800`:
  - **Poll loop**: Ready, Idle, then Ready again after 100 ms.
  - **Status screen**: on the text page, with the connection, the drive, the track, the operation and the progress.
  - **Operations**: Read sectors (`$D5`) and Write sectors (`$D6`) with a read-back, Format track (`$C8`), Set speed (`$D7`), Status (`$D4`) and Cancel (`$D1`). A write to a write-protected disk is reported with an error code.
  - **Disk II routines**, Casso's own, written from Apple's published Disk II documentation: seek by the arm's phase sequence, read and write a 6-and-2 sector, format a track with a volume number.

  Per FR-089, FR-090 and R16.
- [X] T196 [US16] Add `LinkProgram` in `CassoEmuCore/CassoLink/LinkProgram.h/.cpp`:
  - `Shim.s` and `Program.s` compiled in as text resources and assembled with CassoCore's assembler in the AS65 dialect;
  - one version constant read by both sides.

  Tests in `UnitTest/CassoLink/LinkProgramTests.cpp`: both assemble with no error on every build, the shim fits under one page, and the assembled version equals the core's. Per FR-096 and R22.
- [X] T197 [US16] Serve Casso's client in `CassoLinkHost`:
  - distinguish it from ADTPro's by the first command (Ready, against ADTPro's set);
  - answer Ready with Idle or the next operation;
  - Load program when the versions differ;
  - Set speed;
  - the sector, track and status operations of the contract.

  Tests over an `InProcessByteStream` with a scripted client. Per FR-089, FR-091 and R16.
- [X] T198 [US16] Add `ScenarioTests/CassoLinkSectorTests.cpp`, running a //c `TestMachine` with the shim and program loaded, against `CassoLinkHost` over the bridge:
  - read and write sectors, and format a track, on a mounted DOS 3.3 image and a ProDOS image, compared byte for byte;
  - a write-protected image reports its error;
  - during a read, the text page contains the connection, the drive, the track, the operation and the progress (FR-090, US14/AC6);
  - with the machine not stepped for ten seconds mid-read, as during a debugger break, the read completes and compares equal.

  Per US16/AC1, US16/AC5 and SC-029.

## Phase 20: Sparse disks and the session (US14)

**Purpose**: catalogs and files over the line without reading whole disks
(R20), one operation at a time (FR-098).

- [X] T199 [US14] Route every sector read and write in `Dos33Volume` and `ProDosVolume` (`CassoEmuCore/Machines/Apple2/Common/`) through one accessor, with no change in behavior. The existing `UnitTest/EmuTests/Dos33VolumeTests.cpp` and `ProDosVolumeTests.cpp` must pass untouched. Per R20.
- [X] T200 [US14] Add `LinkedDisk` in `CassoEmuCore/CassoLink/LinkedDisk.h/.cpp`:
  - a 140 KB sector image with a presence bit per sector, the volume's kind, and the miss list of the last run;
  - a read of an absent sector adds it to the miss list and returns zeros;
  - fault-in: run, fetch the misses in one request, run again, until a run adds nothing;
  - prefetch the VTOC and track 17 for DOS 3.3, and blocks 2 to 5 for ProDOS;
  - cleared when the drive's map or volume changes.

  Tests in `UnitTest/CassoLink/LinkedDiskTests.cpp` over a fake sector source: a catalog and a file read in a fixed number of rounds, and a write that sends only the sectors it changed. Per FR-084, FR-085 and R20.
- [X] T201 [US14] Add `CassoLinkSession` in `CassoEmuCore/CassoLink/CassoLinkSession.h/.cpp`, on its own thread, owning the `IByteStream`, the host engine, a `LinkedDisk` per drive and the operation queue:
  - **States**: "Closed → Opening → Waiting for Ready → Loading program → Ready ⇄ Busy → Closed. Any state goes to Disconnected on a timeout or a closed stream; Disconnected goes to Opening on the next request."
  - **LinkOperation** kinds: enumerate, read file, write file, delete, rename, make directory, read disk, write disk, bootstrap and save link. Each has "progress (0 to 100), result HRESULT, and a message" and moves "Queued → Running → Done | Failed | Canceled"; one runs at a time, and Cancel stops the running one.
  - **Writes** re-read the catalog sectors first and fail if the volume name or catalog differs from the listing they started from.
  - Progress and results are posted, never waited on.

  Tests in `UnitTest/CassoLink/CassoLinkSessionTests.cpp`. Per FR-098, data-model.md and the edge cases.
- [X] T202 [US14] Add Read disk to image and Write image to disk to `CassoLinkSession`:
  - **Read**: every track's standard sectors, written to an image in any format Casso writes (a `.nib` or `.woz` built from the sectors), committed by atomic replace only on success. Unreadable sectors are listed, with an option to write them blank.
  - **Write**: every track formatted, written and read back, after the caller's confirmation. An image whose tracks do not decode to standard sectors fails before any track is written, with those tracks listed.

  Tests on the in-process stream. Per FR-086 and the edge cases.
- [X] T202a [US14] Add `ScenarioTests/CassoLinkSessionTests.cpp`: `CassoLinkSession` against a //c `TestMachine` running the shim and program over the in-process bridge, with a DOS 3.3 image in drive 1 and a ProDOS image in drive 2. List both catalogs, copy a file out of each and one onto each, read each disk to an image and write an image back, comparing every file and image byte for byte with its source. Per SC-029 and US14/AC1 to AC5.

## Phase 21: Casso Explorer (US14)

**Purpose**: the Apple as a root in the tree, browsed and copied as an
image is.

- [X] T203 [US14] Add CassoLink to the tree:
  - `Location::Kind::AppleDrive` in `CassoEmuCore/CassoExplorer/Model/Location.h`;
  - `TreeNode::Kind::CassoLink` and `AppleDrive`, with ids `link:<name>` and `link:<name>|<slot>,<drive>`, in `Model/TreeModel.h/.cpp`;
  - address parsing and formatting in `Model/BrowserModel.cpp`.

  The roots follow This PC. A drive is labeled with its slot, drive and volume; an empty drive is listed as empty; a link with no Apple replying lists its state and the reason in place of drives. Tests in `UnitTest/CassoExplorer/TreeModelTests.cpp`. Per FR-083.
- [X] T204 [US14] Add `links` to `CassoEmuCore/CassoExplorer/Model/CassoExplorerPrefs.h/.cpp`:
  - `name`, unique among links;
  - `transport`: `Serial`, `Tcp`, `WifiModem` or `Emulated`;
  - `port`, `COM3` for serial, or empty to search;
  - `address`, `host:port`;
  - `baud`, default 115200;
  - `bootstrapBaud`, default 9600;
  - `hardwareHandshake`, default false.

  Add a dialog to add and edit a link, in `CassoEmuCore/CassoExplorer/CassoExplorerLinkDialog.h/.cpp`. Tests for load, save and defaults. Per FR-083, FR-094 and data-model.md.
- [X] T205 [US14] Wire an Apple drive's catalog, previews and Properties in `CassoEmuCore/CassoExplorer/CassoExplorerWindow.cpp` and `CassoExplorerBrowser.cpp` through `CassoLinkSession`, with results posted to the window and never waited on. With an ADTPro client connected instead of Casso's, the Apple root lists no drives, lists the reason that browsing and copying require Casso's own Apple program, and offers no copy or disk verbs. Per FR-084, US14/AC1 and AC2, and the edge cases.
- [X] T206 [US14] Make copy, paste, drag out, drag in, delete, rename and new folder (ProDOS) work on an Apple drive, through `CassoExplorerActions.cpp` and `CassoExplorerDragOut.cpp`:
  - `HostFileNaming` styles and the put conversion rules;
  - a write to a write-protected disk fails with the reason.

  Tests through the session's fake stream. Per FR-085 and US14/AC3 and AC4.
- [X] T207 [US14] Add Read disk to image and Write image to disk to an Apple drive's menu and the command bar:
  - the write's confirmation identifies the drive and the volume;
  - progress in the status bar;
  - Cancel;
  - queued operations listed.

  Per FR-086, FR-098 and US14/AC5 and AC6.
- [X] T208 [US14] Add the COM search in `CassoEmuCore/CassoLink/CassoLinkSearch.h/.cpp`, through `IByteStreamFactory`:
  - open each COM port no other program has open, at the default speed;
  - only when an Apple root with no port set is opened, or Search is chosen; never in the background;
  - stop at the first port where a link replies;
  - close each port before the next.

  Tests over a fake factory with a busy port, a silent port and a port with a link. Per FR-083a and the edge cases.
- [X] T209 [US14] Open a COM port or TCP port only while the user has a link started, and release it on stop. A TCP link listens on 127.0.0.1, except with the `WifiModem` transport, which listens on the network. Tests through the fake factory. Per FR-097.

## Phase 21a: The Apple's screen, and finding Casso (US16, US14, US15)

**Purpose**: the Session 2026-10-16 decisions. The Apple displays what the
link is doing from the shim on, a running Casso is a root with no setup, the
link dialog contains only its transport's settings, and the COM search
opens each port at both speeds.

**Independent test**: boot the link disk in an emulated //c and start
Casso Explorer with no link set up: the root for that Casso appears, the
//c's screen displays the shim waiting, then loading, then the program's
status screen with the PC line and each operation's label (US16/AC6, AC7).

- [X] T219 [US16] Move the shim to `$3D00` in `CassoEmuCore/CassoLink/Apple/Shim.s`, with its 6551 control byte for the starting speed at `$3D02`, and its own screen on the text page: a title, `WAITING FOR THE PC ON PORT 2 AT <speed>`, `LOADING PROGRAM <n>%` as each page arrives, and `STARTING VERSION <n>`. Point the program's `SHIM` at `$3D00` in `Program.s`, and fail the build's assembly test in `UnitTest/CassoLink/LinkProgramTests.cpp` when the program ends at or above `$3D00` or the shim ends above `$3FFF`. Per FR-090, FR-094 and R24.
- [X] T220 [US16] Add `LinkProgram::BuildShim (DWORD baud, std::vector<Byte> & outShim)` in `CassoEmuCore/CassoLink/LinkProgram.h/.cpp`, writing the speed byte at offset 2 from `GetSpeedControl`. `CassoLinkSession::Connect` in `CassoEmuCore/CassoLink/CassoLinkSession.cpp` opens Emulated, TCP and Wi-Fi modem links at the link's speed alone; a serial link is opened at the bootstrap speed and then the link's. Tests in `LinkProgramTests.cpp` and `CassoLinkSessionTests.cpp`. Per FR-094 and R24.
- [X] T221 [US16] Add the Label command ($CE) to `CassoEmuCore/CassoLink/CassoLinkCodec.h` and `CassoLinkHost::SendLabel`, sent ahead of each operation the user starts with up to 38 characters, such as `COPYING HELLO TO DRIVE 2`; `CassoLinkSession` builds the text from the `LinkOperation`. Update `contracts/cassolink.md`'s table. Tests in `UnitTest/CassoLink/CassoLinkHostTests.cpp` and `CassoLinkSessionTests.cpp`. Per FR-090 and R24.
- [X] T222 [US16] Add to the program's status screen in `Program.s`: a PC line, `CONNECTED` after any packet and `NO REPLY FOR <n> S` for each unanswered second, sending Ready again each second with no reply; an error line with the last Status error's text until an operation succeeds; and the last Label on the operation's line. A scenario test in `ScenarioTests/CassoLinkSessionTests.cpp` reads the //c's text page: the shim's waiting line before the host connects, then `CONNECTED`, a label, and a write-protected error. Per FR-090, US16/AC7 and R24.
- [X] T223 [P] [US16] Add `RunningCassoRegistry` in `CassoEmuCore/CassoLink/RunningCassoRegistry.h/.cpp` over a file-system seam: `Publish` writes `%LOCALAPPDATA%\Casso\Links\<pid>-<port>.json` with `pid`, `started` (the process's start time as a FILETIME), `machine`, `serialPort` (1 or 2) and `address` (`127.0.0.1:<port>`); `Withdraw` deletes it; `Enumerate` returns the files whose process is alive with that start time, and deletes the rest. Tests in `UnitTest/CassoLink/RunningCassoRegistryTests.cpp` with a fake process probe. Per FR-083b, R25 and data-model.md.
- [X] T224 [US16] Call `Publish` from `MachineHost::ConnectSerialPort` in `CassoEmuCore/Shell/MachineHost.cpp` when a port is connected to TCP, and `Withdraw` when it is connected elsewhere and when the machine is torn down. Tests in `UnitTest/EmuTests/` over the fake factory. Per FR-083b.
- [X] T225 [US16] List the running Cassos as link roots in `CassoEmuCore/CassoExplorer/Model/CassoLinkConnections.cpp` and `TreeModel`, labeled `<machine> (port <n>)`, with an Emulated link's settings never written to `links`; a record for port 1 lists its reason in place of drives. `CassoExplorerWindow` reads the folder at start and on a change notification for it, and removes a root when its Casso exits. Tests in `UnitTest/CassoExplorer/CassoLinkConnectionsTests.cpp` and `TreeModelTests.cpp`. Per FR-083b, US16/AC6 and the edge cases.
- [X] T226 [US14] In `CassoEmuCore/CassoExplorer/CassoExplorerLinkDialog.cpp`, contain only the selected transport's rows: Serial, COM port (empty to search), speed, bootstrap speed and handshaking; TCP, address; Emulated, address defaulting to `127.0.0.1:6502`; Wi-Fi modem, port, speed and handshaking. Changing the transport hides and reveals rows and resizes the dialog. A Wi-Fi modem link's root lists `ATDT <pcname>.local:<port>`. Tests in `UnitTest/CassoExplorer/CassoExplorerLinkDialogTests.cpp`. Per FR-083c, FR-094 and R26.
- [X] T227 [US14] Open each port at the bootstrap speed and then the link's in `CassoLinkSearch::Find` and the session's search, in `CassoEmuCore/CassoLink/CassoLinkSearch.cpp`. Tests in `UnitTest/CassoLink/CassoLinkSearchTests.cpp` with a fake Apple that replies Ready only at 9,600. Per FR-083a and R26.
## Phase 21b: Connections and the ADTPro server (US14, US15, US16)

**Purpose**: the Session 2026-10-22 decisions for section H. A connection
is added with the transport first and only the settings the PC or the Apple
cannot find for itself; ADTPro's client is served by a connection type of
its own, confined to the folder it shares; the boot disk's Uthernet II MAC
address differs between machines.

**Independent test**: quickstart §7b steps 8 and 9: the Add connection
dialog has no name, port-on-the-Apple or slot field and adds no duplicate,
and an ADTPro-compatible server sends and receives a disk to an emulated
//c running ADTPro's client while a CD outside its folder fails.

- [X] T228 [US14] In `CassoEmuCore/CassoLink/CassoLinkSettings.h/.cpp`, add `CassoLinkSettings::MakeName`, returning `Serial (any port)` or the COM port for Serial, `TCP <host>:<port>`, `Wi-Fi modem on port 6400`, `ADTPro server on <COM>` or `ADTPro server at <host>:<port>`; add the `AdtProServer` transport to `LinkTransport` in `CassoEmuCore/Seams/ByteStreamSettings.h` and remove `Emulated` from the transports a saved link can have, keeping it only for the running-Casso roots; add `folder`, and remove the slot field. Reading `links` in `CassoEmuCore/CassoExplorer/Model/CassoExplorerPrefs.cpp` drops an `Emulated` link, makes every other link's name again, and lists only the first of two with the same name. Tests in `UnitTest/CassoExplorer/CassoExplorerPrefsTests.cpp` and `UnitTest/CassoLink/CassoLinkSettingsTests.cpp`. Per FR-083c, R27 and data-model.md.
- [X] T229 [US14] Rebuild `CassoEmuCore/CassoExplorer/CassoExplorerLinkDialog.cpp/.h` and `CassoExplorerLinkChoices`: the transport drop-down first (Serial, TCP, Wi-Fi modem, ADTPro-compatible server); no name field; a COM port drop-down of `Any` and the ports Windows lists; no Apple-side port or slot row; the bootstrap speed above the speed, with an info tip that it is only for the loader typed in through `IN#`; handshake with an info tip recommending a first attempt without it and noting that many null-modem cables do not connect RTS and CTS or loop each end's back, and that a //c's 6551 has CTS tied to ground; a TCP address with no error displayed while it is empty and Add disabled until it parses; a Wi-Fi modem description, working on a //c's port 2, with no port field; for an ADTPro server, a COM port or TCP address and a folder with Browse, Add disabled until a folder is chosen. Add does not add a connection whose `MakeName` an existing link has, and displays a message. Tests in `UnitTest/CassoExplorer/CassoExplorerLinkDialogTests.cpp`. Per FR-083c and R27.
- [X] T230 [US14] A Wi-Fi modem link listens on TCP port 6400 in `CassoEmuCore/CassoExplorer/Model/CassoLinkConnections.cpp`; a port in use is the root's error, and the root lists `<pcname>.local:6400`. The boot disk built for it dials that address. Tests in `UnitTest/CassoExplorer/CassoLinkConnectionsTests.cpp`. Per FR-083c and R27.
- [X] T231 [US15] In `CassoEmuCore/CassoLink/Apple/Shim.s`, take the shim's slot from the header when the boot disk's stage started it, else from `$39` when it is `$C1` to `$C7`, else from the first slot with `$Cn05 = $38`, `$Cn07 = $18`, `$Cn0B = $01` and `$Cn0C = $31`; the 6551 is at `$C088` plus 16 times the slot. `LinkProgram` in `CassoEmuCore/CassoLink/LinkProgram.cpp` stops writing a slot for the typed and cassette bootstraps. Scenario tests in `ScenarioTests/CassoLinkSuperSerialTests.cpp`: a typed bootstrap after `IN#2` on a //c and after `IN#4` with the card in slot 4, and a cassette bootstrap with the card in slot 3. Per FR-092, R29 and the contract's Bootstrap.
- [X] T232 [US14] In `CassoEmuCore/Seams/Win32ComPort.cpp`, with hardware handshake on, check `GetCommModemStatus` while a write is pending, and fail the stream when a write does not finish within its 2-second timeout with CTS low, with `The PC's CTS line is off, so the cable may not connect it. Turn off hardware handshake.`; the session reports it on the root. The decision is a pure function in `CassoEmuCore/Seams/ByteStreamSettings.h/.cpp` tested in `UnitTest/CassoLink/ByteStreamSettingsTests.cpp`. Per FR-083d and R30.
- [X] T233 [US16] Serve ADTPro's requests in `CassoEmuCore/CassoLink/CassoLinkHost.cpp` only on an `AdtProServer` link, and only Casso's own client on every other link. In `CassoEmuCore/CassoLink/CassoLinkFolder.cpp`, resolve the folder with `GetFinalPathNameByHandle` and each requested path with `GetFullPathName`, and fail CD, Directory, Size, Get, Put, Batch and the virtual drives for a result outside the folder's subtree with ADTPro's error reply. Tests in `UnitTest/CassoLink/CassoLinkHostTests.cpp` with `..`, a drive letter, a UNC path and a junction. Per FR-088, R28 and the contract.
- [X] T234 [US16] Remove `CassoExplorerPrefs::emulateAdtPro` (an existing key is ignored), `CassoExplorerCassoLinkPage` from `CassoEmuCore/CassoExplorer/CassoExplorerOptionsDialog.cpp/.h`, and `ApplyAdtProSettings`, `StartLinksForAdtPro` and `GetAdtProFolder` from `CassoEmuCore/CassoExplorer/CassoExplorerWindow.cpp`. An `AdtProServer` link's root in `TreeModel` has a virtual help item, previewed from `CassoExplorerBrowser::GetAdtProHelp`, rewritten for the connection type; opening it or dragging it out writes it to `%TEMP%\Casso\` first. Nothing is written to Documents. Tests in `UnitTest/CassoExplorer/TreeModelTests.cpp` and `CassoLinkConnectionsTests.cpp`. Per FR-088 and R28.
- [X] T235 [US16] In `CassoEmuCore/CassoLink/Apple/Universal.s`, add into the MAC address's last three bytes the sum of RNDL and RNDH (`$4E`, `$4F`) and a 16-bit sum of `$4000-$BFFF` before the W5100 is given it, and detect a slot's W5100 by reading its mode register at `$C084` plus 16 times the slot with bits 6, 5 and 2 clear before writing the address pointer. Tests in `UnitTest/CassoLink/LinkProgramTests.cpp` and `ScenarioTests/CassoLinkUthernetTests.cpp`: two boots with different RAM get different MAC addresses, and a slot whose mode register reads $FF is not written. Per FR-091c, R31 and the contract.

## Phase 21c: Settings (FR-095a, FR-095b)

**Purpose**: the Session 2026-10-22 decisions for the Settings sheet, built
before the tasks were written.

**Independent test**: quickstart §7b step 10.

- [X] T236 [US16] Connect a Super Serial Card's connector to any device while the machine runs through `MachineHost::ConnectSerialPort` with a path in `CassoEmuCore/Shell/MachineHost.cpp`, and the ImageWriter II to a parallel card through `MachineHost::ConnectParallelPrinter` and `IDM_PRINTER_PARALLEL` in `CassoEmuCore/Shell/CpuCommandDispatcher.cpp`; `SettingsPanelState::ApplyConnectors` in `CassoEmuCore/Ui/Settings/SettingsPanelState.cpp` sends the connectors whose new device is not the printer first, and `RequiresReset` is true only for a hardware enable change. Tests in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp`, `CpuCommandDispatcherTests.cpp` and `UnitTest/UiTests/SettingsPanelStateTests.cpp`. Per FR-095a, FR-095b and R32.
- [X] T237 [US16] Set a Super Serial Card to printer mode when the ImageWriter II is chosen on it, saving SW1 as `sw1BeforePrinter`; restore SW1-5 and SW1-6 when the printer moves away if the card is in printer mode; disconnect the printer when the card is switched out of printer mode; report each change with `SettingsPanelState::TakeNotice` and `SettingsSheet::ReportCrossTabChange`; offer the printer only on the Printing tab. Tests in `UnitTest/UiTests/SettingsPanelStateTests.cpp` and `HardwarePageTests.cpp`. Per FR-095a and R32.
- [X] T238 [P] [US16] Scroll a Standard-style `DxuiTabStrip` with arrows when its tabs do not fit, in `Dxui/Widgets/DxuiTabStrip.cpp`; pin a `DxuiInfoTip` until a click elsewhere, a focus change, Escape or a second click, with Escape closing it before a dialog's cancel, in `Dxui/Widgets/DxuiInfoTip.cpp` and `Dxui/Window/DxuiWindow.cpp`. Tests in `UnitTest/Dxui/DxuiTabStripTests.cpp` and `DxuiInfoTipTests.cpp`. Per FR-095b.

## Phase 21d: File operations and the code review (FR-013e, FR-016, FR-018b to FR-018d, FR-088)

**Purpose**: the Session 2026-10-23 decisions, and the defects a review of
the branch found, built before the tasks were written.

**Independent test**: the unit and scenario suites; the live checks are
listed in validation.md.

- [X] T239 [US3] Copy out of a damaged image as a salvage copy: count the sectors lost from each file through an `ISectorReadObserver` in `CassoEmuCore/CassoExplorer/Model/DiskOperations.cpp`, set `CassoExplorerActions::Outcome::damaged` and its warning, and report it in `CassoExplorerWindow::ReportOutcome`. A move from such an image does not delete its originals, and `CassoExplorerDragOut::RenderLog` deletes after a move dragged to File Explorer only when every file was read whole. Tests in `UnitTest/CassoExplorer/CassoExplorerActionsTests.cpp`, `UnitTest/EmuTests/CrossFormatExtractionTests.cpp` and `UnitTest/CassoExplorer/CassoExplorerDragOutTests.cpp`. Per FR-018b and FR-018c.
- [X] T240 [US3] Offer `Replace`, `Skip` and `Keep both` for each name clash of a copy out, through `CassoExplorerActions::CollisionFn` and `ResolveCopyOutPath` in `CassoEmuCore/CassoExplorer/CassoExplorerActions.cpp`. A file skipped there or at the load address prompt sets `Outcome::skipped`, and `Outcome::IsWhole` is then false, so a move does not delete it. Host originals of a move go to the Recycle Bin through `IShellItemVerbs::Recycle`. Tests in `UnitTest/CassoExplorer/CassoExplorerActionsTests.cpp`. Per FR-018c and FR-018d.
- [X] T241 [US3] Add the access byte and dates of a copied entry to `FilePayload` and write them with `ProDosVolume::StampEntry`; count a locked directory in a delete's plan; fail a copy of a folder into itself or a folder inside it; and make a move fail on a locked entry rather than delete it. Tests in `UnitTest/CassoExplorer/CassoExplorerActionsTests.cpp`. Per FR-013e and FR-016.
- [X] T242 [US16] Write a virtual drive's block in place through `IDiskFileIo::WriteAt` and `ICassoLinkFolder::WriteAt`, from `CassoLinkHost::WriteDriveBlock` in `CassoEmuCore/CassoLink/CassoLinkHost.cpp`. Tests in `UnitTest/CassoLink/CassoLinkHostTests.cpp`, and ADTPro's standalone VSDrive on a //c and on a Super Serial Card in `ScenarioTests/CassoLinkAdtProTests.cpp`. Per FR-088.
- [X] T243 [US1] Fix what the review of `CassoEmuCore/CassoExplorer/CassoExplorerWindow.cpp` found:
  - link completions dropped once their window is gone, through `CassoExplorerActions::Guard`;
  - a disk read retried on the drive it started on;
  - a link another window is on not closed, through `CassoLinkConnections::SetViewing` and `IsViewed`;
  - typed paths added to the one list every window shares;
  - Shift+Delete limited to host files;
  - long dropped paths read whole;
  - an edited link found again after its dialog;
  - every window moved off a removed link;
  - undo of an image step only where it was made, and an undone delete's saved copies removed only once they are back.

  Tests in `UnitTest/CassoExplorer/AppleDriveActionsTests.cpp`, `CassoLinkConnectionsTests.cpp` and `CassoExplorerActionsTests.cpp`.
- [X] T244 [P] [US1] Fix what the review of Dxui's list, tree, hex, text and tab strip widgets in `Dxui/Widgets/` found:
  - a click collapsing a multiple selection is reported;
  - header focus steps to the next column;
  - List view columns are measured again for a new row set or DPI;
  - scrolling is clamped after a resize, a regroup or an expand;
  - a tab strip scrolls only by a wheel over it;
  - hex cells are measured again at a new DPI;
  - the hex view copies text when its hex column is hidden;
  - the tree, hex and text views scroll by part notches.

  Tests in `UnitTest/Dxui/DxuiListViewMultiSelectTests.cpp` and `DxuiHexViewTests.cpp`.
- [X] T245 [P] [US1] Fix what the review of `CassoEmuCore/Seams/` found:
  - a menu for items in different folders built from the desktop;
  - a canceled `IFileOperation` returned as `ERROR_CANCELLED`;
  - a stale preview result dropped;
  - a handler the next file does not use unloaded;
  - a dropped folder watch set up again;
  - `Win32FolderWatcher::Unwatch` waits out a running callback.

  Checked live in Casso Explorer: a watched folder's changes are listed, and the window closes at once.
- [X] T246 [P] [US16] Fix what the review of `CassoEmuCore/CassoLink/Apple/` found:
  - the last byte waited out before a speed change at as slow as 1,200 baud;
  - the Uthernet II announcement count started at zero;
  - expansion ROMs deselected after `SlotFinder.s`'s scan.

  Program version 50. A check of the shim's page number was not added: the 6551 shim fills its 768 bytes. CassoLink unit and scenario suites.
- [ ] T247 Close what the review found and this phase did not fix:
  - `Win32ShellPreview` sends Load and Unload to a helper with `SendMessageTimeoutW` on the window's thread, so a helper busy in `DoPreview` stalls each selection change up to 2 s; send from a worker or post the request instead, in `CassoEmuCore/Seams/Win32ShellPreview.cpp` and `Win32PreviewHelper.cpp`;
  - `Win32FolderWatcher::DropUnusable` calls the dropped folders' callbacks without the in-flight mark `Unwatch` waits on, in `CassoEmuCore/Seams/Win32FolderWatcher.cpp`;
  - the 6551 shim's page number is not checked against the page count, since the CRC covers only the data; the check takes room in `CassoEmuCore/CassoLink/Apple/Shim.s`, which is full at 768 bytes;
  - `DxuiTabStrip::SetTabs` does not clamp the selected index, in `Dxui/Widgets/DxuiTabStrip.cpp`;
  - `Win32ShellIcons` caches every icon it renders, a failed load included, and never evicts one, in `CassoEmuCore/Seams/Win32ShellIcons.cpp`.

## Phase 22: Bootstrap, updates and hardware (US15, US14)

**Purpose**: a bare //c brought up from the PC, updated from it, and the
owner's walkthrough.

- [X] T210 [US15] Add `LinkBootstrap` in `CassoEmuCore/CassoLink/LinkBootstrap.h/.cpp`:
  - the monitor text `CALL -151`, the shim as `3D00:` lines of 8 bytes with its speed byte at 9,600, then `3D00G`;
  - sent at the bootstrap speed with 250 ms after each line.

  Confirm the //c serial firmware's Ctrl-A `14B` speed command from the //c Technical Reference transcription. Tests of the generated text in `UnitTest/CassoLink/LinkBootstrapTests.cpp`. Per FR-092 and R23.
- [X] T211 [US15] Add Bootstrap to an Apple root's menu, then the shim's Ready at 9,600 baud, Load program, and Set speed to the link's speed.
  - **Speed change**: both ends switch after the ACK, so speeds that differ are corrected.
  - **Scenario test** in `ScenarioTests/CassoLinkBootstrapTests.cpp`: a //c `TestMachine` at the BASIC prompt with `IN#2` and Ctrl-A `14B` typed, bootstrapped over the bridge to the status screen.

  Per US15/AC1, SC-033 and the edge cases.
- [X] T212 [US15] Add Save link to disk: a `DirectBootBuilder` boot disk whose boot sector loads the shim to `$3D00`, built by `LinkProgram::BuildShim` at the link's speed, which loads the program from the PC. A scenario test boots it in a //c `TestMachine`, and a shim reporting an older program version receives Load program. Per FR-091, US15/AC3 and AC4, and SC-031.
- [X] T213 [US14] Add the `WifiModem` transport:
  - Save link to disk stores the PC's address on the disk;
  - the shim sends the modem's dial command for that address before Ready;
  - Casso Explorer accepts the connection on the network.

  A scenario test drives a scripted modem on the bridge. Per FR-093.
- [ ] T214 [US14] On the owner's //c over a null modem cable, walk quickstart.md §7b steps 5 to 7 and record SC-027, SC-028 and SC-033 in `specs/033-casso-explorer/validation.md`. If 115,200 baud fails the polled receive loop, record the fallback to 57,600. Per SC-027, SC-028, SC-033 and the plan's Risks.

## Phase 23: The cards, after their merges (US16, US15)

**Purpose**: the link over a Super Serial Card and an Uthernet II, and
audio bootstrap, once Casso emulates the cards.

- [X] T215 [US16] Merge the 042 Super Serial Card feature, then:
  - add a Super Serial Card driver to `Shim.s` (the 6551 at `$C0n8` for the card's slot);
  - make bootstrap use `IN#n` and the card's Ctrl-A command;
  - add a scenario test on a //e `TestMachine` with the card in a slot.

  Per FR-093, FR-092 and US16/AC2.
- [X] T216 [US16] Merge the 043 Uthernet II feature, then:
  - add a W5100 transport to `Shim.s` and `Program.s`, a TCP connection to the PC's address;
  - accept it in Casso Explorer;
  - add a scenario test on a //e `TestMachine`.

  Per FR-093 and US16/AC3.
- [X] T217 [US15] Add audio bootstrap for a ][+ or //e: the loader sent as audio into the cassette input, with a scenario test sending it through Casso's cassette emulation (spec 037). Land it with whichever of T215 and T216 is first. Per FR-092 and US15/AC2.

## Phase 24: Section H validation

- [X] T218 Walk quickstart.md §7b steps 1 to 4 on x64 Debug and Release, and record SC-029, SC-030 and SC-032 in `specs/033-casso-explorer/validation.md`. Per SC-029, SC-030 and SC-032.

## Section H dependencies

- **Phase 16 first.** T181 before T182 to T186.
- **Phase 17 after T186.** Phase 18 after T188.
- **Phase 19 after T182 and T189.**
- **Phase 20 after T197.**
- **Phase 21 after T201.**
- **Phase 21a after Phase 21; Phase 22 after Phase 21a;** T214 after everything before it.
- **Phase 23 after the 042 and 043 merges.**
- **Parallel work:**
  - T184 and T185 alongside T182;
  - T189 alongside T194 to T196;
  - T199 and T200 alongside phase 19;
  - T203 and T204 alongside T201.
