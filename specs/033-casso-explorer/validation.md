# 033 Casso Explorer validation

Results of walking `quickstart.md`. Each section records the build it ran
on, what was checked, and what came out. Sections that need a person at the
keyboard are marked as such until someone walks them.

## 1-7. Interactive walkthrough

Not yet walked by hand on a Release build (T077). Parts checked by driving
the Debug build with posted input on 2026-09-17:

| Check | Result |
|---|---|
| Right-click a host file: icon row (Cut, Copy, Rename, Share, Delete), Open, Open with, Refresh, Copy as path, Properties, Show more options | Shown as listed |
| F2 on a host file: an edit box over the name with the stem selected; Escape restores the row | As expected |
| The address bar's `>` menu on a folder of ~60 subfolders, window mid-screen: hangs under the bar at the room below, scrolls by wheel, thumb moves | As expected |
| The same, window near the bottom of the screen: flips above the bar and fits the room above | As expected |

Still to check by hand: the Open with submenu's programs, Show more options
opening the shell menu and its items, Share opening the share sheet, paste
of files cut or copied in Explorer, a drag from Explorer and from another
image onto the list and the tree, and a read-only image refusing the drop.

### Release x64, 2026-09-18, commit `c8ccbca9`

Driven with posted clicks and keys, and with the posted command message
(`WM_APP + 0x33`) where a key needs a modifier, on copies of
`UnitTest/Fixtures/CassoExplorer/dos33.dsk` and `prodos.po`. The CLI side ran
`CassoCli disk delete` on a second copy of each.

| Check | Result |
|---|---|
| §1 Delete ODD from the DOS 3.3 image, Delete key then Yes; `fc /b` against the CLI's delete | No differences |
| §1 The same on the ProDOS image | No differences |
| §2 The DOS 3.3 image lists every entry `disk list` reports, with the same types | As expected |
| §2 HELLO previews as its listing; PICTURE ($2000, 8192 bytes) as a hi-res picture | As expected |
| §6 F1 opens About with the cassowary picture and the explanation of the name | As expected |
| Posted command 502 (Large icons) switches the list's view | As expected |
| The command bar's See more, reached by Tab and Left, opens with Enter | Shows About Casso Explorer, F1 |
| Context menu Open with, reached by the Apps key and arrows (Debug) | Lists the handlers; Enter on Notepad opened the file |

Not yet checked on Release: the rest of §1 (get, put, create, init, sector
and block reads and writes need the folder picker or a drag), §3 and §5
(they need Casso running with a disk and its menus), §4 (drags), the
Ctrl shortcuts of §6 other than through the command message, and §7.

### Release x64, 2026-09-25, commit `1cbee0e2`

Driven with posted clicks and the posted command message on copies of
`UnitTest/Fixtures/CassoExplorer/dos33.dsk` and `prodos.po`, a folder of 200
copies of `prodos.po`, and a 26-byte text file named `broken.dsk`. Timings are
the round trip of a sent command message, which returns once the list is
refilled.

| Check | Result |
|---|---|
| §2 Refresh on the folder of 200 images, five times | 29 to 67 ms; the list is full each time, well inside one second |
| §2 Refresh on each image, which reads its catalog again, three times each | ProDOS 51 to 66 ms, DOS 3.3 57 to 67 ms, inside 200 ms |
| §2 The DOS 3.3 image against `disk list` | The same seven entries and types; 379 free sectors shown as 94.7 KB free |
| §2 The ProDOS image against `disk list` | The same entries; 214 free blocks shown as 107 KB free |
| §2 A tab at `broken.dsk` | The size error in the list area, 0 items |
| §2 The preview toggle, then close | `previewVisible` saved as changed |
| §6 Two tabs, a different row selected in each, switched away and back | Each keeps its own selection and status |
| §6 The Theme drop-down | Light theme, Dark theme, Follow system, Casso Skeuomorphic (colors only), Casso Dark Modern, Casso Retro Terminal |
| §2 Rename by the command | The edit box opens over the name with it selected |

Found on this pass:

- Keys posted to a window that is not in the foreground do not reach it, so
  F2 and typing into the rename box did nothing; rename needs the keyboard.
- With the location deep in the folder tree, every label lay past the pane's
  right edge. The tree has no horizontal scroll, as Explorer's has none; the
  cause was an 18 dip indent per level against Explorer's 8, now matched.

Still to check by a person: §1 beyond delete (get, put, create, init,
sector and block reads and writes need the folder picker or a drag); §2
rename on each image with a colliding name, and the broken image's tooltip
in the tree; §3 and §5, which need Casso running with its menus; §4, the
drags; §6 with the mouse disconnected and with Windows dark mode toggled.

### Shipping, 2026-09-22

Run on the renamed tree with a Release x64 build.

| Check | Result |
|---|---|
| `scripts/StageRelease.ps1 -Platforms x64` into a scratch folder | `CassoExplorer.exe` staged beside `Casso.exe` and `CassoCli.exe`, with `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll` and `vcruntime140_1.dll` |
| The staged `CassoExplorer.exe`, started from that folder | Opens, titled "Casso Explorer"; no build tree needed |
| `scripts/GenerateMsixAssets.ps1 -Source Resources/Icons/CassoExplorer.png -Prefix Explorer` | 40 pictures written; Casso's 40 left alone |
| The same script with no prefix | 40 written; Casso Explorer's 40 left alone |
| `scripts/BuildMsix.ps1` over the staged payload | Bundle built; the package holds `Casso.exe`, `CassoCli.exe` and `CassoExplorer.exe`, and 35 Explorer tile pictures |
| `Installer/Package.appxmanifest` | Parses; three applications: Casso, CassoExplorer, CassoCli |

Not yet checked: installing the package and starting Casso Explorer from the
Start menu, from its executable's name at a prompt, and from Casso's Browse
disks command (T131, SC-014), and unpacking the zip on a machine with no
build tools (SC-015). Both need a signed package or a development
certificate.

### Navigation pane and search beside File Explorer, 2026-10-11

Debug x64, the same machine and user as File Explorer, both windows captured
with the same folders open.

| Check | Result |
|---|---|
| SC-016: section order | Matches: Home, Gallery, the OneDrive root; a line; Quick access; a line; iCloud Drive, iCloud Photos, This PC with its drives, Libraries, Network, Linux. Casso's own root comes first, as specified |
| SC-016: Quick access content | Matches after fixes: the pinned folders with a pin each, then the frequent folders (Screenshots, Assets, Demos) with none, and no expanders on either |
| SC-016: folder content | Matches after a fix: OneDrive's `SkyDrive camera roll` is listed as Camera Roll, from its desktop.ini |
| SC-016: a pin made in File Explorer | A scratch folder pinned with the shell's Pin to Quick access verb appears in Casso Explorer's Quick access, with its pin |
| SC-016: a pin removed in Casso Explorer | Unpin from Quick access in Casso Explorer's pane; the folder left File Explorer's Quick access |
| SC-016: a pin changed while Casso Explorer is open | A folder pinned and then unpinned through the shell appears with its pin, then leaves Casso Explorer's Quick access, within seconds and without a refresh |
| SC-016: OneDrive's zip files | Not matched: File Explorer's tree lists the .zip files under the OneDrive root as folders, and Casso Explorer's lists none |
| SC-017: first results | 1,000 text files in an indexed folder under the user profile, 100 of them containing the word "zephyrine". With a restored search tab for the word, the first results were on screen 0.86 s after the process started, startup included |
| SC-017: same files as File Explorer | 100 results, the same 100 the Windows Search index returns for the word over that folder |

### Apple file-type icons, 2026-10-13

Debug x64 at 150%. A ProDOS image built for the check with CassoCli: 12 text
(TXT), 12 Applesoft (BAS), 12 binary (BIN) and 12 system (SYS) files, and 2
directories, 50 entries in all.

| Check | Result |
|---|---|
| SC-019: Details, List, Small icons | Each kind has its own icon at 16-24 px: the orange B page, the slate T page, the BASIC monitor, the red S page and the folder |
| SC-019: Medium, Large, Extra large icons | The same icons, drawn at each size. Directories drew as a small folder in a dark box at these sizes until a fix in this pass, and the pages drew blurred at Extra large |
| SC-019: Tiles, Content | The same icons, beside the name, the type and the size |

### File associations, 2026-10-13

Debug x64, the owner's own account, registered and unregistered from Casso
Explorer's Options. `HKCU\Software\Classes` and File Explorer's `FileExts`
record were exported before the first registration and after the last.

| Check | Result |
|---|---|
| SC-018: the program and icon Windows resolves | All six formats open with `Casso.exe --disk1 "%1"` and take the icon `Casso.exe,-108`, by AssocQueryString in a new process. Before a fix in this pass, .do, .po, .nib and .nb2 resolved to Windows' Open with prompt and a blank page: Casso Explorer was a second type under each, and for two types with no choice made, Windows displays that prompt |
| SC-018: Open with | Casso and Casso Explorer are both recommended for all six formats |
| SC-018: the icon on screen | Casso's floppy icon on all six in a new process, by SHGetFileInfo and in Casso Explorer's own listing. A File Explorer window open from before the registration drew blank pages after it, after a refresh, and after the icon was moved to another file, so that process does not reread the types |
| SC-018: Open | The resolved command, run on a copy of the Casso Rocks disk, started Casso with the image in drive 1 and booted it to its first prompt |
| SC-018: after unregistering | No association differs. `Classes` differs only in Windows' own caches (MuiCache and timestamps). `FileExts` has three keys more, for .do, .nib and .nb2, each with an empty OpenWithList and no values; Windows adds these for an extension it handles, and .po had one before Casso was registered. Before a fix in this pass, Casso's type stayed in the .do record |

### Picture previews on Casso's monitors, 2026-10-13

Debug x64 at 150%, a scratch DOS 3.3 image with a hi-res and a double
hi-res picture.

| Check | Result |
|---|---|
| SC-021: redraw on a monitor switch | 36 to 41 ms from the command to the redrawn picture, over six switches through all four monitors; the picture is decoded again, drawn through the CRT pass and read back in that time |
| SC-021: the choice after a restart | Amber chosen from the Monitor drop-down, and a brightness of 198% set on the Display page, were both in effect after Casso Explorer was closed and started again |
| SC-021: Cancel | A brightness change on the Display page brightened the preview at once (mean level 68 to 146), and Cancel put it back to 68 |
| FR-067: the custom monitor | Custom monochrome from the Display page drew the picture in the default light blue; the color picker opened from the swatch, a hue of 32 degrees redrew the preview in orange while the picker was open, and after OK and a restart the preview was orange and the prefs file had `custom` and `FFB866` |
| SC-020: setup | The Casso Rocks color hi-res cassowary, booted straight to hi-res page 1 in Casso and stored as a binary at $2000 for Casso Explorer; both on the color monitor in Dark Modern with no effect changed; Casso's client 1120 wide and Casso Explorer's preview pane widened so both drew it at 1120 by 768 |
| SC-020: before the fixes | 38% of pixels identical, every one within 2 codes per channel inside the picture, and up to 25 apart in its first and last rows. The final pass's dither was keyed on the pixel's place in the render target, and Casso draws the picture lower in its back buffer than the preview drew it in its own; and the preview's target ended at the picture's edges, where Casso's bloom spills into its letterbox |
| SC-020: after | 99.72% of pixels identical over the picture and its top and bottom margin, and the rest one code apart in one channel, scattered through the bright areas. The dither is now counted from the picture's corner, and the preview draws the picture in a black margin as wide as its bloom and bleed sample, the glow showing in it. The one-code differences are float rounding: Casso's target is its whole client area and the preview's is the picture and its margin |

### Apple text previews, 2026-10-13

Debug x64 at 125%, a host folder with a 300-line `LONG.Applesoft BASIC.txt`.

| Check | Result |
|---|---|
| SC-026, FR-082 | The plain ASCII export previewed as lines of text in the theme's monospaced face; a plain `.txt` without an export suffix opens in the hex view, and a disk text file splits into lines (unit tests) |
| FR-068 | The command bar ends with Theme and the Preview toggle; no Monitor drop-down |
| Dropped | A CRT pass over the text and a font made from the video ROM were each built and run, then removed at the owner's request |

### Shell previews, File Explorer's menus and archives, 2026-10-14

Debug x64, commit `45787513`, driven with posted clicks. The files are a PDF,
a Markdown file, a PNG and a two-paragraph Word document made for the test,
and a zip of two of them.

| Check | Result |
|---|---|
| SC-022: .pdf | The PDF handler's window appeared 111 to 120 ms after the click, and the page was drawn by 172 to 175 ms, over three rounds |
| SC-022: .docx | Word's handler window appeared at 113 to 136 ms, and both paragraphs were drawn in the theme's colors by 171 to 183 ms |
| SC-022: .png | The thumbnail settled in the preview pane within 264 to 311 ms. Each sample is a full-window capture, so these are upper bounds |
| SC-022: same preview | Each preview comes from the handler File Explorer loads for the extension; a Markdown file on the disk uses the PowerToys handler |
| SC-023: a .png file | Icon row Cut, Copy, Rename, Share, Delete; Open; Open with; New; Copy as path; Properties; then the installed apps' commands with their icons: Edit with Clipchamp, Edit with Paint, Unlock with File Locksmith, Edit with Photos, Edit in Notepad, Beyond Compare 5, a WinRAR submenu; `Show more options` |
| SC-023: a tree node | Icon row Cut, Copy, Delete; Collapse; Open in new tab; Pin to Quick access; Add to Casso; Copy as path; Properties; then Move to OneDrive, Unlock with File Locksmith, Rename with PowerRename, Beyond Compare 5, a WinRAR submenu; `Show more options` |
| SC-023: against File Explorer | Not compared yet; the comparison takes File Explorer's menus for the same items on this machine |
| Open with Code | Absent from both programs on this machine. VS Code's `GetState` returns `ECS_HIDDEN` unless `Software\Classes\VSCodeContextMenu` exists under `HKCU` or `HKLM`, and neither key exists here. Its manifest is inside the `.appx` in its package folder, which is now read |
| SC-024 | Double-click the zip, click a file, Extract all, Extract: four actions. The list and the preview each appeared within the 2 s allowed for them; the wizard opened 0.4 s after the click with the zip's own folder as the destination; both files were extracted 0.2 s after Extract |
| SC-025 | Not measured: a drag from File Explorer moves the real pointer, so a person at the keyboard walks it (T077) |

### Quickstart 7a, 2026-10-14

x64 Debug and Release, commit `f4537304`, driven with posted clicks on a copy
of `UnitTest/Fixtures/CassoExplorer/dos33.dsk`. The export was written by
`CassoCli disk get HELLO --basic` under Casso Explorer's descriptive name;
Casso Explorer's own export reads the file through the same
`DiskOperations::Get` call with the same encoding. The results were identical
on both builds at every step.

| Check | Result |
|---|---|
| SC-026, FR-082: the disk text file | NOTES previewed as its lines in the monospaced face, the tab expanded |
| SC-026: the export against the image | `HELLO.Applesoft BASIC.txt` and HELLO inside the image previewed as the same six lines |
| FR-073: a plain `.txt` | Previewed by the `.txt` preview handler installed here (PowerToys), as File Explorer previews it. The quickstart listed the hex view here, written before the shell previews; that line is corrected |
| FR-068 | The command bar ends with Theme and the Preview toggle, with no Monitor drop-down |
| FR-067, Release | With PICTURE selected, Amber monochrome picked on the options dialog's Display page turned the preview amber before OK: the lit pixels' mean went from R 118 G 121 B 121 to R 151 G 124 B 33. Cancel was then pressed |

### Quickstart 1, 2026-10-18

x64 Debug at commit `9fd4f138`, driven with posted clicks, against
`CassoCli` from the same build. Each operation ran on its own copy of
`UnitTest/Fixtures/CassoExplorer/dos33.dsk` or `prodos.po`, once in Casso
Explorer and once in the CLI, and the resulting files or images were compared
with `fc /b`.

| Operation | Casso Explorer | CLI | `fc /b` |
|---|---|---|---|
| Get as a listing | HELLO, Copy as Descriptive, pasted in a folder | `get HELLO --basic` | no differences |
| Get raw | PICTURE, Copy as CiderPress: `PICTURE#062000` | `get PICTURE` | no differences |
| Put text | `MEMO.txt` with Put file, stored as a T file with no dialog | `put MEMO.txt --text --as MEMO` | no differences |
| Put binary at an address | `BLOB.Binary.$6000.bin` with Put file, stored as B at $6000 with no dialog | `put --type B --load $6000 --as BLOB` | no differences |
| Delete | BLOB, Delete, Yes | `delete BLOB` | no differences |
| Create, defaults | New, Disk image: WOZ, DOS 3.3, volume 254 | `create D1.woz` | no differences |
| Create, bootable | The same with Bootable | `create D2.woz --bootable` | no differences |
| Create, ProDOS | ProDOS order, ProDOS, volume NEWDISK | `create D3.po --format prodos` | no differences |
| Init | Format disk image, DOS 3.3, Yes | `init` | no differences |
| Sector read | Track 17, sector 0, 3 logical sectors | `sectorread --logical --track 17 --sector 0 --count 3` | no differences |
| Sector write | A 512-byte file at track 20, sector 5, Overwrite | `sectorwrite --logical --track 20 --sector 5` | no differences |
| Block read | Blocks 2 to 5 of the ProDOS image | `blockread --block 2 --count 4` | no differences |
| Block write | A 512-byte file at block 100, Overwrite | `blockwrite --block 100` | no differences |

Notes from the walk:

- A Put of `MEMO.txt` stores the entry as `MEMO`, as `.txt` is the text
  form's suffix; the CLI's `put` with no `--as` stores it as `MEMO.TXT`. The
  row above passes `--as` so that both entries are `MEMO`.
- Copy as CiderPress on an Applesoft program copies its listing, as FR-017a
  applies the host file name setting only to raw copies.
- New, Disk image with an image selected in the list took the selection as
  the files to copy onto the new disk, and reported that it did not fit;
  File Explorer's New takes no account of the selection.

### Quickstart 7b steps 1 to 4, 2026-10-17 and 18

x64 Release, then x64 Debug at commit `64741e1d`, driven with posted clicks and
keys. An emulated //c in Casso had `link.dsk` in drive 1 and a copy of
`UnitTest/Fixtures/CassoExplorer/dos33.dsk` with a 10 KB binary, TENK at
$4000, in drive 2; its port 2 was the TCP bridge on port 6502. For step 3,
drive 1 had ADTPro 2.1.0's client disk. Eight defects were found in the walk,
each fixed before the Debug pass and listed below the table.

| Check | Result |
|---|---|
| FR-083b: no setup | With Casso running and Casso Explorer started, CassoLink listed `Casso on port 6502`, and the connection opened with the status bar at `Connected to Apple //c (ROM 4)` |
| FR-090: the screens | Before Casso Explorer started, the shim's `WAITING FOR THE PC ON PORT 2 AT 115200`; connected, the program's status screen with `PC: CONNECTED`; during the catalog read, `READING THE CATALOG OF D2` |
| Drives and volumes | Two drives, `Slot 6, Drive 2 (DOS 3.3 volume 77)` once its catalog was read, eight files with TENK at 11 KB |
| Closing Casso | Its connection was removed from CassoLink, and the list on it went to This PC |
| SC-029: copy out | TENK copied and pasted into a folder as `TENK.Binary.$4000.bin`: `fc /b` against the original, no differences, on both builds |
| SC-029: copy in | A copy pasted onto drive 2 as TENK2, read back with `CassoCli disk get`: loads at $4000, `fc /b` no differences, on both builds |
| SC-029: disk to `.dsk` | Create image, 30 s: `fc /b` against the source image, no differences, on both builds |
| Step 2: a pause | The //c paused 10.0 s (Release 11.1 s) in the middle of a read; the read completed in 38 s (Debug 40 s), equal to the disk |
| SC-032: ADTPro send | The client's Send of drive 1, 29 s on both builds: `STEP3.dsk` in the ADTPro folder, byte for byte the client disk |
| SC-032: ADTPro receive | The client's Receive into drive 2, picked from its file chooser: drive 2's image byte for byte the client disk, 43 s (Debug 45 s), `COMPLETE!` |
| SC-030: the line closed | Port 2 set to Not connected partway through a read: `Drive 2: the connection to the Apple closed.`, no image file. Port 2 back on TCP: the connection was made again with no action, and the next read, 28 s (Debug 29 s), equal, with neither program restarted |

Defects found and fixed in the walk:

- A file copied out of a disk image or a CassoLink drive could not be pasted
  in Casso Explorer itself; Paste now goes through the folder's drop target.
- Paste was offered only in host folders; it now puts files onto disk images
  and CassoLink drives by the rules a drop follows.
- The status bar drew its activity text over the item count, and a link
  operation's progress flickered on and off as other updates replaced it.
- With ADTPro's client served, a Casso found on its own was not opened until
  its connection was, so the client's first request timed out.
- A receive into a //c running at its own speed stopped after 40 blocks: the
  host's timeout for the client's next request, 3 s, was shorter than the
  client's write of the blocks to its disk. The timeout is now 30 s.
- A line that closed was reported as `the Apple did not complete it`.
- The message box's error icon was a pink circle with a black cross.
- The tree did not open to a CassoLink connection or drive open in the list, and
  highlighted `Documents\ADTPro` for a tab in the ADTPro folder.

### Quickstart 5 and 6, the steps left from 2026-10-18, on 2026-10-20

x64 Release after commit `dbdbdd85`, driven with posted clicks. No Casso was
running.

| Check | Result |
|---|---|
| §5: a single-drive default machine | With drive 2 of the Apple //e Enhanced detached in the user's prefs, `dos33.dsk`'s menu has Insert into drive 2 disabled. Before the fix in `dbdbdd85` it was enabled: the drive count was read from the shipped machine file without the user's changes |
| §5: no Casso running | Insert into drive 1 started a Casso, the Apple //e Enhanced with its one drive, with `dos33.dsk` in drive 1 |
| §6: Follow system | Windows' app mode set to light and back: the window was light within a second and dark again within a second, with no restart |

### Quickstart 7b steps 5 to 7 on the owner's //c, 2026-10-20

The owner's //c, ROM 4, on COM3 through a null modem cable, with a Disk II on
its external port as drive 2; program version 43, shim version 4. The
timings come from a session on the same code as Casso Explorer's, driven by
a probe outside the suite. The hardware findings are in
`real-2c-findings.md`.

| Check | Result |
|---|---|
| SC-033: bootstrap | Bootstrapped from Casso Explorer several times with `IN#2` and Ctrl+A `14B` typed on the //c; the status screen each time, after the loader's typing of about 40 s |
| Leaving CassoLink | Escape on the //c: the BASIC prompt, `LIST` empty, `CATALOG` of drive 1 worked, no stray characters |
| ADTPro's own bootstrap | Speediboot from ADTPro 2.1.0's server, after `IN#2` and Ctrl+A `14B`: the monitor beeped at the loader's lines, at its 250 ms pacing and at 1000 ms, and the client did not start; its client disk, written to drive 2 by CassoLink, booted |
| Drive speeds | Drive 1 at 293.2 rpm (2.3% slow), drive 2 at 298.4 rpm; each format's gap fits its drive |
| Write image to disk | The DOS 3.3 System Master to drive 1, 65.9 s including the drive's timing; every track verified |
| SC-027: disk to image | Drive 1 into an image at 115,200 baud in 27.6 s, byte for byte the System Master image written to it. After two files were written to it, read again in 28.1 s: byte for byte the image ADTPro 2.1.0's client sent of the same disk, from drive 2, in 26.4 s |
| SC-028: file from the //c | A 10 KB binary, T10K, in 3.2 s from a new session after its catalog (7.1 s, the first listing of the drive), equal to the original |
| SC-028: file onto the //c | **Not met:** T10K written in 31.0 s with program version 43. With version 47, a new 10 KB file in 5.7 s and 7.6 s on an empty disk, and 9.2 s and 6.7 s on the nearly full System Master, equal when read back. The rest is about 2.2 revolutions a track and the tracks a file is spread over; the owner stopped here. Finding 34 |
| Step 5: Save link to disk, boot it | The link disk, program version 45, written to drive 2 over the link in 59.8 s, moved to drive 1 and booted: the shim waiting on port 2 at 115,200; the PC connected with no bootstrap in about 10 s, and both drives were listed |
| Step 7: booting a written image | ADTPro 2.1.0's client disk, written to drive 2 over the link in 60.4 s, moved to drive 1 and booted to ADTPro's menu. Writing a `.woz` with a non-standard track fails with the track listed, in the scenario tests |

### File Explorer's menus beside Casso Explorer's, 2026-10-18

x64 Debug, the working tree after commit `0ff8f5ff`. Each menu was opened with a
real right click, File Explorer's in `C:\LinkTest\menus` and Casso Explorer's on
the same items, and both were captured from the screen.

| Check | Result |
|---|---|
| SC-023: a .png file | The same icon row and items in the same order, with two exceptions: AI actions, and Share with. The apps' commands follow File Explorer's order, which is by each app's own display name, `Photos` rather than the package's `Microsoft Photos` |
| SC-023: a folder | The same items in the same order, with Paste in the icon row while the clipboard has files |
| SC-023: two files | The same items in the same order, except Share with |
| SC-023: the background | Paste; View, Sort by, Group by; Undo with the step it reverses, New, Properties; then the apps' commands: the same items in the same order |
| SC-023: a tree node | Icon row Cut, Copy, Paste, Rename, Delete; Collapse, with no Open; then the same items in the same order. Rename edits the node's label in place and renamed the folder; Paste copied a file into it |
| Not offered | Share with: ntshrui.dll, the shell's handler for it, faults inside Casso Explorer, so it is left out. AI actions is not among the commands Casso Explorer reads: packaged apps' manifests, the shell's command store and the file type's static verbs |
| Casso's own | Add to Casso, on a folder and the background, which File Explorer does not have |
| SC-025: the drag | `src.txt` dragged with a real mouse from a File Explorer window, the pointer resting 1.5 s on each target in Casso Explorer, then over the same kinds of item in File Explorer; Escape ended each drag, and nothing was dropped |
| SC-025: a folder | Both: the row lit, `Move to Folder` |
| SC-025: an archive | Both: the zip's row lit, `Copy` |
| SC-025: a program | Both: the row lit, `Open with tool.exe` |
| SC-025: a drive | Both: the tree node lit, `Copy to Micron 2TB SSD (F:)`. File Explorer opened the collapsed node after the drag rested on it 970 to 1,086 ms; Casso Explorer did not, and now opens one after a second, measured at 900 to 1,017 ms |
| SC-025: a disk image | Casso Explorer: the tree node lit, `Copy to dos33.dsk`. Not compared: the File Explorer pass had no disk image among its targets |
| Tabs | A drag over a tab opens it in both programs |
### Quickstart 2 to 6 on x64 Release, 2026-10-18

x64 Release after commit `9a37b6ee`, driven with real mouse and key input. The
images are copies of `UnitTest/Fixtures/CassoExplorer/dos33.dsk` (`a.dsk`,
`b.dsk`, and `ro.dsk` marked read-only) and `prodos.po` (`p.po`), with a 24-byte
text file called `broken.dsk`, a text file `plain.txt`, and 8192 random bytes in
`rand.bin`. Casso was a Release //c of the walk's own, and Casso Explorer was
opened from its Storage menu. Five defects were found, each fixed and checked
again on the rebuilt Release, and are listed below the table.

| Check | Result |
|---|---|
| §2 The tree | `a.dsk` and `b.dsk` have no expand affordance, `p.po` has one with SUBDIR under it, `broken.dsk` has the broken badge and none |
| §2 `broken.dsk` | Its tree node's tooltip and the list area both contain `broken.dsk is not a valid .dsk image. File size is 24 bytes but should be 143,360 bytes.` |
| §2 Lists against `disk list` | `a.dsk`: the same seven entries and types, 94.7 KB free. `p.po`: the same seven entries, SUBDIR among them, 107 KB free |
| §2 Previews | NOTES as text; ODD, 777 bytes at $0803, as a hex dump, and with Disassemble as 6502 instructions from `ORA $FB66,Y` on, then as the hex dump again; `a.dsk` selected in its folder as its catalog, matching `disk list` |
| §2 Rename, DOS 3.3 | F2 on HELLO, NOTES typed: `a.dsk: NOTES: is already the name of another file on this volume`. HELLO2 typed: HELLO2 is in `disk list` |
| §2 Rename, ProDOS | The same on `p.po`: the same refusal, then HELLO2 in `disk list` |
| §3 Browse, then insert | Browsing `C:\LinkTest\walk` and previewing in it left it out of `KnownFolders.json`. Insert into drive 1 on `a.dsk` put the disk in Casso's drive 1, added the folder to `KnownFolders.json` and to the Casso root |
| §3 Casso's picker | Insert drive 2 lists `a.dsk`, `p.po` and `broken.dsk` from that folder |
| §3 Add and Remove | Add to Casso on `out` added it to the file, and Remove from Casso took it out |
| §4 Between two tabs | ODD dragged from `a.dsk` onto the tab showing `b.dsk`, which opened, and into its list: `ODD (2)`, B at $0803, unlocked, as ODD is; the two read back by `disk get`, `fc /b` no differences |
| §4 To File Explorer | HELLO2 dropped into a File Explorer window as `HELLO2.Applesoft BASIC.txt`, `fc /b` against `disk get --basic` no differences; PICTURE as `PICTURE.Binary.$2000.bin` |
| §4 Back onto an image | `PICTURE.Binary.$2000.bin` dropped on `b.dsk`, with `Copy to b.dsk` under the pointer: `PICTURE (2)`, loads at $2000, 8192 bytes, no dialog; read back, `fc /b` no differences |
| §4 A text file, then random bytes | `plain.txt` on `a.dsk`: PLAIN, type T, no dialog. `rand.bin`: the load address prompt with `$2000` filled in, then RAND, type B |
| §4 Onto Casso | `b.dsk` dropped on Casso's drive 2 replaced `x.dsk` there |
| §4 Read-only | Over `ro.dsk`, the no-drop pointer |
| §5 From Casso's menu | Browse disks in Casso Explorer opened Casso Explorer; chosen again, no second process, and the window came to the front |
| §5 Insert, Open in new Casso | Insert into drive 1 as in §3. Open in new Casso on `b.dsk`: a second Casso with `b.dsk` in drive 1, the first one's title and disks as they were |
| §5 A put onto a mounted disk | Paste onto `a.dsk` in drive 1: Casso reloaded it, with `Casso Explorer modified a.dsk and inserted it into Drive 1.` |
| §6 Keyboard | Ctrl+T, Ctrl+L and a typed path, Down to select, Ctrl+Tab to a tab whose list and selection were as left, Shift+F10 for the menu, Delete moving a file to the Recycle Bin, Ctrl+W closing the tab, F2 and Enter for the renames above |

Not checked on this pass:

- §5 with no Casso running: the only other Casso on the machine belonged to
  another session, and Insert into Drive 1 would have gone to it.
- §5 Insert into Drive 2 with a single-drive machine as Casso's default,
  which means changing the default machine.
- §6 Windows dark mode under Follow system, a system setting.

Defects found and fixed:

- A broken image in the tree had no tooltip; the node's location is an image,
  and the lookup used accepted only folders.
- Disassemble binary was in no menu or toolbar. It is now a toggle on the hex
  preview's toolbar, which remains visible over the disassembly.
- A file dropped on a disk image's row from the folder with that image had
  no effect: the row resolved to the folder, and a drop into the folder its
  files came from is no drop.
- A disk changed by Casso Explorer was reported as `CassoCli modified`.
- The load address prompt's title was in title case.
### Follow-up checks on x64 Release, 2026-10-19

The build of commit `ea371969`, driven with real input.

| Check | Result |
|---|---|
| New, Disk image with `plain.txt` selected | `NEWD.woz` created empty: 496 sectors free of 560 |
| The delete prompt | `Delete PLAIN from a.dsk?`, and with two entries selected, `Delete these 2 items from a.dsk?` |
| The load address prompt | Titled `Load address`, with `$2000` filled in |
| A folder renamed in the tree, then Ctrl+Z | `out` became `out2` and then `out` again |
| `CassoCli disk put` with no `--as` | `NEWPIC.Binary.$2000.bin` stored as NEWPIC, B at $2000, and `NEWHI.Applesoft BASIC.txt` as NEWHI, an Applesoft program, on a DOS 3.3 and a ProDOS image; both read back with `fc /b` no differences. `MEMO.txt` stored as `MEMO.TXT` |
| A soft reset during SAVE | On a //e with a copy of the DOS 3.3 System Master: SAVE T, SAVE U interrupted by Reset, SAVE V, then Casso closed. No prompt and no spare copy; T and V in the catalog. The file was not written at the reset, so this pass did not catch a write in flight; the unit test `ASoftResetFlushesADirtyDiskThroughTheStore` covers that case |

Not checked: the two §5 steps for when no Casso is running, as two Cassos
of other sessions were running, and §6 Windows dark mode.
## 8. Headless tests

Run 2026-09-17 on commit `fc7d628e` plus the working tree, after a build of
each configuration.

| Configuration | `-Filter Casso Explorer` | Full suite |
|---|---|---|
| Debug x64 | 95 of 95 passed | 6054 of 6054 passed |
| Release x64 | 95 of 95 passed | 6052 of 6052 passed |

Release runs two fewer tests than Debug: two tests exist only in Debug
builds.

Required test classes, all present: `HostFileNamingTests`,
`ContentSnifferTests`, `IntegerBasicDetokenizerTests`, `DisassemblerTests`,
`PreviewDecoderTests`, `KnownFolderStoreTests`, `CassoTargetingTests`,
`DragPayloadTests`, `BrowserModelTests`, `TreeViewTests` and
`TreeViewNavigationTests` (the tree view's), `DxuiListViewMultiSelectTests`
and the other `DxuiListView*Tests` (the list view's), `DxuiSplitterTests`,
`DxuiStatusBarTests`, `DxuiFramebufferViewTests`,
`DxuiDragDropSourceTests`, `IntentChannelTests`.

## 9. Structural checks

| Check | Result |
|---|---|
| Statements in `CassoExplorer\Main.cpp` | 0 |
| `CassoExplorer.vcxproj` sources | `Main.cpp` and `CassoExplorer.rc` only |
| `EntryPointSymbol=wWinMainCRTStartup` | In all four configurations |
| `Dxui/` naming Casso Explorer or Casso, against master | One comment added on this branch, in `DxuiIconImage.cpp`; reworded to name no application |

## 10. Gates

| Gate | Result |
|---|---|
| `scripts/CheckStyle.ps1 -Mode Tree` | OK, 1708 files |
| `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, all four configurations | Zero warnings, 2026-09-16 (T081); to be rerun before the master merge |
