# 033 Cassque validation

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

## 8. Headless tests

Run 2026-09-17 on commit `fc7d628e` plus the working tree, after a build of
each configuration.

| Configuration | `-Filter Cassque` | Full suite |
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
| Statements in `Cassque\Main.cpp` | 0 |
| `Cassque.vcxproj` sources | `Main.cpp` and `Cassque.rc` only |
| `EntryPointSymbol=wWinMainCRTStartup` | In all four configurations |
| `Dxui/` naming Cassque or Casso, against master | One comment added on this branch, in `DxuiIconImage.cpp`; reworded to name no application |

## 10. Gates

| Gate | Result |
|---|---|
| `scripts/CheckStyle.ps1 -Mode Tree` | OK, 1708 files |
| `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, all four configurations | Zero warnings, 2026-09-16 (T081); to be rerun before the master merge |
