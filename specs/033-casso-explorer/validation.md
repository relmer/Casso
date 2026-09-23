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
