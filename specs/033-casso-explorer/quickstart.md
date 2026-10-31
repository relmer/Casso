# Quickstart: Validating Casso Explorer

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

For each verb, run the CLI and the same operation from Casso Explorer on a copy
of the same image, then compare:

```powershell
CassoCli disk get scratch.dsk HELLO --basic --out cli-hello.txt
```

Then Get HELLO from Casso Explorer's context menu into a folder, and:

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
- Alt+P hides the preview; close and reopen Casso Explorer. Expected: still
  hidden.
- Build a folder of 200 copies of a fixture image and select it under
  This PC with a stopwatch. Expected: the list is full within one
  second. Select one image. Expected: its catalog appears within 200 ms.
- Rename a file on a DOS 3.3 image and on a ProDOS image with F2.
  Expected: the CLI `list` shows the new name; a name that collides is
  refused with a message.
- Point the tree at a file with a `.dsk` extension that is not a disk
  image. Expected: no expand affordance, the parse error as its tooltip,
  and the same error in the list area when selected.

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

- From Casso's menu, open Casso Explorer. Right-click an image, Insert into
  Drive 1. Expected: that Casso's drive 1 widget shows the disk.
- With a single-drive machine as Casso's default and no Casso running,
  right-click an image. Expected: Insert into Casso drive 2 is disabled.
- Close Casso, Insert into Casso drive 1. Expected: a Casso launches with the
  disk mounted.
- Open in new Casso while one is running. Expected: a second Casso, the
  first untouched.
- With a disk mounted in Casso, put a file onto it from Casso Explorer. Expected:
  Casso reloads the image, or reports a conflict if the guest had written.
- Click Casso's Casso Explorer menu item twice. Expected: one window, fronted.

## 6. Tabs, keyboard, themes (Story 5)

- Ctrl+T, browse elsewhere, Ctrl+Tab. Expected: each tab keeps its own
  selection and list.
- Complete sections 2 through 5 with the mouse disconnected.
- Set Casso Explorer to Follow system and toggle Windows dark mode. Expected: the
  window changes within a second, no restart.
- Open the Theme dropdown. Expected: Skeuomorphic (colors only), Retro Terminal,
  Dark Modern, Dark, Light, Follow system.
- Help, About. Expected: the cassowary picture and the explanation of the
  name.

## 7. Read a file's bytes (Story 6)

Select a binary file on a disk image -- one the graphics rule does not
claim, so the preview is its bytes.

- Expected: offsets down the left from the file's load address, the hex
  digits, then the characters, all in a fixed-width face with the columns
  lined up down the pane.
- Drag across the digits. Expected: the same run lights in the characters
  at the same time, and keeps going across the row boundary.
- Shift+click further down, then Shift+Down. Expected: the run extends;
  the anchor stays where the drag began.
- Ctrl+C, then paste into Notepad. Expected: hex digits, spaced by the
  grouping in force.
- Click in the characters and Ctrl+C again. Expected: the characters this
  time, for the same bytes.
- Tab from the file list. Expected: the digits, then the characters, then
  out of the pane. Shift+Tab runs the three backwards.
- Ctrl+A in each column, then Copy. Expected: the whole file, as digits or
  as characters by the column.
- View, bytes grouped by four. Expected: the digits regroup and the
  selection is unchanged. Close and reopen Casso Explorer. Expected: still four.
- Ctrl+G, type an address in the file. Expected: the caret lands there and
  the view scrolls to it. Type one outside the file. Expected: a message
  naming the range, and the view does not move.
- Right-click inside the selection. Expected: Copy, Select all, Go to
  offset, and the selection is left alone. Right-click outside it.
  Expected: the byte under the pointer is selected instead.
- Select a text file, then a BASIC program. Expected: the preview is lines
  of text, not bytes; the bytes come back with the next binary.
- Toggle View, Disassemble binary. Expected: mnemonics from the same load
  address, as lines rather than a hex view.

## 7a. Apple text previews (FR-082, FR-068)

- Select a text file on a disk image. Result: lines of text in the
  theme's monospaced face, not the hex view.
- Export an Applesoft program to a host folder and select the
  `.Applesoft BASIC.txt` file. Result: the same lines as the program
  inside the image, as text. Select a plain ASCII `.txt` file without an
  export suffix. Result: the preview File Explorer draws for a `.txt`
  file (FR-073), or the hex view where Windows has no preview for it.
- Look at the command bar. Result: Theme and the Preview toggle at the
  far end, and no Monitor drop-down. Open the options dialog's Display
  page and pick amber. Result: a picture preview redrawn on amber.

## 7b. CassoLink (stories 14 to 16, FR-083 to FR-098)

Contract: [contracts/cassolink.md](contracts/cassolink.md). Steps 1 to 4
run with no hardware; steps 5 to 7 use the owner's //c on a null modem cable.

1. **Emulated //c, over TCP.** Start Casso on a //c with the link disk
   from Save link to disk, set its port 2 to a TCP port on the Hardware
   settings page, and start Casso Explorer.
   Result: a root for that Casso with no setup (FR-083b). Before it
   connects, the shim is waiting on port 2 at 115,200 on the //c's screen;
   once it connects, the program's status screen with the PC line
   `CONNECTED` and each operation's label (FR-090). Opening the root lists
   two drives, each labeled with its volume. Close that Casso. Result: its
   root is removed.
   Copy a 10 KB file out and back, and read a disk to `.dsk`. Result: the
   file and the image compare equal to their sources with `fc /b`.
2. **The debugger.** During a disk read, break into Casso's debugger for 10
   seconds and resume. Result: the read completes and its image compares
   equal.
3. **ADTPro's client.** Boot ADTPro 2.1.0's client disk in an emulated //c,
   join it to Casso over the TCP bridge, and send and receive a disk.
   Result: images equal to those ADTPro's own host makes (SC-032).
4. **Failures.** Close the TCP connection mid-read. Result: a message with
   the reason, no image file left behind, and the next read succeeds with
   neither end restarted (SC-030).
5. **Bootstrap.** On the //c at the BASIC prompt, type `IN#2` and Ctrl-A
   `14B`, and choose Bootstrap. Result: the status screen within 60
   seconds (SC-033). Save link to disk, boot it, and confirm it loads the
   current program.
6. **Speed.** Read a 140 KB disk at 115,200 baud. Result: under 30 seconds,
   and equal to ADTPro's image of the same disk (SC-027). Copy a 10 KB file
   each way. Result: under 5 seconds each (SC-028).
7. **Write image to disk.** Write a `.dsk` to a blank disk in drive 2 and
   boot it. Result: it boots; a `.woz` with a non-standard track fails with
   the track listed.
8. **Add connection** (FR-083c). Open Add connection. Result: the
   transport comes first; there is no name field and no Apple-side port or
   slot; the COM port is a drop-down of Any and the ports found; the
   bootstrap speed is above the speed, and it and handshake each have an
   info tip; no error is displayed for an empty TCP address. Add a Serial connection
   on Any, then the same again. Result: the root `Serial (any port)`, and
   no second one added. A Wi-Fi modem connection's root lists
   `<pcname>.local:6400`.
9. **ADTPro-compatible server** (FR-088). Add one on the TCP address of an
   emulated //c running ADTPro's client, sharing a test folder. Result:
   its root has the help item, and no file is written to Documents. From
   ADTPro's client, send and receive a disk, then CD to `..` and to
   `C:\`. Result: the transfers work and both CDs fail. An ordinary
   CassoLink connection to the same machine does not answer ADTPro.
10. **Settings, live** (FR-095a, FR-095b). In a running //e with a parallel
    card and a Super Serial Card, move the ImageWriter II to the Serial card
    on the Printing tab. Result: no reboot, a message that the card is in
    printer mode, and `PR#2` prints. Set the card to communications mode.
    Result: a message that the printer is not connected. Change the card's
    connector to a loopback plug. Result: no reboot.

**Whole disks in every format, on a real //c over serial:** for each of .dsk, .do, .po, .woz and .nib, read a whole disk into an image, write that image to a scratch floppy, read the floppy back, and compare its sectors with the source disk.

## 7c. Full send (Story 17, FR-107 to FR-116)

Prerequisites: the link running on an emulated //c with two drives, then on a real //c, and a set of disks that includes a duplicate and a disk with an unreadable sector.

1. Set an empty folder on the Full send page of Options, and press F on the Apple's CassoLink screen. Each image appears as `NNNN NAME.ext`, and `full-send.csv` has a row for it.
2. Swap the idle drive during each transfer. The session continues with no key press, at under 30 seconds a disk at 115,200 baud (SC-038).
3. Make no swap. A prompt appears on the Apple, and neither drive spins until Space.
4. Press Space with a repeated disk present. It is sent, and its hash marks it as a duplicate, and no second image is on the PC (SC-039).
5. Send the disk with an unreadable sector. Its unreadable list is beside its image, the row has `possibly-protected`, and the session continues.
6. Press Esc during a transfer. Nothing happens. Press Q. No partial image remains, the CassoLink screen returns, and a catalog read works.
7. Press F again with the same folder. Sequence numbers continue, and a disk from the first run is not sent without Space.
8. Compare each image with one from Read disk to image on the same disk.

## 8. Headless tests

```powershell
scripts\RunTests.ps1 -Configuration Debug -Filter Casso Explorer
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
  and remove, missing folder retained, atomic write
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

## 9. Structural checks

```powershell
(Select-String -Path CassoExplorer\Main.cpp -Pattern '^\s*(\w.*\(.*\)\s*\{|return)' ).Count
```

Expected: 0. `CassoExplorer.vcxproj` holds one comment-only source, one `.rc`,
and `EntryPointSymbol=wWinMainCRTStartup` in every configuration. A grep
of `Dxui/` for `CassoExplorer` or `Casso` returns nothing new.

## 10. Gates

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
