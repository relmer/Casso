# Quickstart: Validating Cassque

**Feature**: 033-cassque | **Date**: 2026-09-10

Contracts: [intent-channel.md](contracts/intent-channel.md),
[host-file-names.md](contracts/host-file-names.md),
[dxui-additions.md](contracts/dxui-additions.md). Model:
[data-model.md](data-model.md).

## Prerequisites

- 032 merged into this branch before the shell phase; the model, codec
  and widget phases need only master.
- x64 Debug and Release green via `scripts/Build.ps1` against `Casso.sln`.
- Test disks: `UnitTest/Fixtures/Disks` holds DOS 3.3 and ProDOS images;
  `Apple2/Demos` holds WOZ images. A scratch copy of each, since
  validation writes to them.

## 1. Byte identity with the command line (SC-001)

For each verb, run the CLI and the same operation from Cassque on a copy
of the same image, then compare:

```powershell
CassoCli disk get scratch.dsk HELLO --basic --out cli-hello.txt
```

Then Get HELLO from Cassque's context menu into a folder, and:

```powershell
fc /b cli-hello.txt "HELLO.Applesoft BASIC.txt"
```

Repeat for get raw, put text, put binary with an address, delete, create
with each format and bootable, init, sector read and write, block read
and write. Expected: every `fc /b` reports no differences on the resulting
image or file.

## 2. Browse and preview (Story 1)

- Expand This PC to a folder of images. Expected: each image is an
  expandable node; a ProDOS image shows its subdirectories; a DOS 3.3
  image shows none.
- Select an image. Expected: the list shows every entry the CLI `list`
  reports, same names, types and sizes; the status bar shows the free
  space the CLI reports.
- Select an Applesoft file, a text file, a hi-res picture at $2000 of
  8192 bytes, a binary of another length, and a disk image in the file
  pane. Expected: listing, text, picture, hex dump with a working
  disassembly toggle, and catalog, in that order.
- Alt+P hides the preview; close and reopen Cassque. Expected: still
  hidden.

## 3. Known folders (Story 1, FR-005)

- Browse under This PC to a folder not in the Casso root, preview a file.
  Expected: the Casso root is unchanged.
- Insert an image from that folder into Casso. Expected: the folder
  appears under the Casso root, and `KnownFolders.json` beside
  `UserPrefs.json` lists it.
- Open Casso's own insert-disk picker. Expected: it offers that folder's
  images.
- Add to Casso on another folder, then Remove from Casso on it.
  Expected: the root and the file follow both.

## 4. Drag and drop (Story 3)

- Drag a file between two images in two tabs. Expected: `fc /b` of the
  two files read back by the CLI reports no differences; type, address
  and locked flag match in both catalogs.
- Drag an Applesoft file to a File Explorer window. Expected:
  `NAME.Applesoft BASIC.txt` appears with the CLI's `--basic` output.
- Drag `NAME.Binary.$2000.bin` back onto an image. Expected: a type B
  entry at $2000 with no dialog.
- Drag a `.txt` with no suffix onto an image. Expected: a text entry, no
  dialog. Drag an 8192-byte file of random bytes. Expected: the put dialog
  opens prefilled at $2000.
- Drag an image onto a running Casso. Expected: Casso inserts it as it
  does from File Explorer.
- Hover a drag over a write-protected image. Expected: no-drop cursor.

## 5. Casso hand-off (Story 4)

- From Casso's menu, open Cassque. Right-click an image, Insert into
  Drive 1. Expected: that Casso's drive 1 widget shows the disk.
- With a single-drive machine as Casso's default and no Casso running,
  right-click an image. Expected: Insert into Drive 2 is disabled.
- Close Casso, Insert into Drive 1. Expected: a Casso launches with the
  disk mounted.
- Open in new Casso while one is running. Expected: a second Casso, the
  first untouched.
- With a disk mounted in Casso, put a file onto it from Cassque. Expected:
  Casso reloads the image, or reports a conflict if the guest had written.
- Click Casso's Cassque menu item twice. Expected: one window, fronted.

## 6. Tabs, keyboard, themes (Story 5)

- Ctrl+T, browse elsewhere, Ctrl+Tab. Expected: each tab keeps its own
  selection and list.
- Complete sections 2 through 5 with the mouse disconnected.
- Set Cassque to Follow system and toggle Windows dark mode. Expected: the
  window changes within a second, no restart.
- Open the theme menu. Expected: Light, Dark, Follow system, Skeuomorphic
  (colors only), DarkModern, RetroTerminal.
- Help, About. Expected: the cassowary picture and the explanation of the
  name.

## 7. Headless tests

```powershell
scripts\RunTests.ps1 -Configuration Debug -Filter Cassque
```

Check `UnitTest.dll` is newer than the build. Required coverage:

- `HostFileNamingTests`: every row of the naming table in both styles,
  round-trip through `Parse`, illegal characters substituted.
- `ContentSnifferTests`: an Applesoft listing, an implicit-LET listing, a
  numbered README that fails on a non-keyword, Integer BASIC falling to
  text, printable text, binary with each suggested address.
- `IntegerBasicDetokenizerTests` and `DisassemblerTests`: known programs
  and every addressing mode.
- `PreviewDecoderTests`: the graphics rule for each address and length,
  hex rows, catalog rows, the error kind on a damaged program.
- `KnownFolderStoreTests`: seed from MRU, append on hand-off, manual add
  and remove, pinned survives, missing folder retained, atomic write
  through the file system mock.
- `CassoTargetingTests`: owner alive, owner dead with a running instance,
  none running, drive count from the default machine.
- `DragPayloadTests`: formats offered per source kind, delayed rendering
  names.
- `BrowserModelTests`: tabs, history, selection, sort, location changes.
- Dxui: `DxuiTreeViewTests` extended for ids, lazy children, checkbox
  hiding and arrow keys; `DxuiListViewTests` for multi-select and sort
  callback; new `DxuiSplitterTests`, `DxuiStatusBarTests`,
  `DxuiFramebufferViewTests`, `DxuiDragDropSourceTests` on a fake data
  object consumer.
- `IntentChannelTests` extended for the new intents and the reply
  encoding.

## 8. Structural checks

```powershell
(Select-String -Path Cassque\Main.cpp -Pattern '^\s*(\w.*\(.*\)\s*\{|return)' ).Count
```

Expected: 0. `Cassque.vcxproj` holds one comment-only source, one `.rc`,
and `EntryPointSymbol=wWinMainCRTStartup` in every configuration. A grep
of `Dxui/` for `Cassque` or `Casso` returns nothing new.

## 9. Gates

```powershell
scripts\RunTests.ps1 -Configuration Debug
```

```powershell
scripts\RunTests.ps1 -Configuration Release
```

```powershell
git add -A; scripts\CheckStyle.ps1 -Mode Tree
```

```powershell
scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis
```

Expected: all green, then the CHANGELOG line and every commit subject
shown for approval before the master merge.
