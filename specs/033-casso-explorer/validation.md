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

### Apple text on the monitor, 2026-10-13

Debug x64 at 125%, the green monitor, the fixture DOS 3.3 image and a host
folder with a 300-line `LONG.Applesoft BASIC.txt`.

| Check | Result |
|---|---|
| SC-026: glyphs | The long export in the enhanced //e ROM's glyphs, sharp green on black at 16 pixels, two to a dot; lowercase from the ROM's lowercase set, its descenders below the baseline |
| SC-026: wrapping | Lines wrapped at word breaks at the pane's width |
| Selection | A drag across the first lines drew them black on green |
| The font | DirectWrite reads the generated file as one family, `Casso Apple II`, with 96 glyphs; every printable character's outline starts at its first lit dot and ends at its last (unit test) |
| Host text | A `.txt` file without an export suffix drew in the theme's font, and the export drew in the Apple face (unit test) |
| No ROM, a ][+ ROM | The built-in table with no error, and lowercase folded to uppercase (unit tests) |
| Earlier pass | The same text through the CRT pass, a band at a time, drew and scrolled correctly; the effects were dropped at the owner's request, and the text with them became selectable |
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
