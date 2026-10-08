# Casso Explorer: owner feedback backlog

Owner feedback from the T077 walkthrough and after, not yet worked. Kept here so
it survives a lost session. Remove an item once it ships and the owner has
passed it. Numbers are the owner's where the owner gave them; FE is File
Explorer, CE is Casso Explorer, LV the list view.

## Built, waiting for the owner's check

- Address bar: the suggestion list drops when editing starts (the merged
  history) and turns into completions as a path is typed; Up and Down walk
  it, Escape closes it; F4 and the chevron open it too. Typed host folders are
  written to FE's TypedPaths; image paths stay in CE's list; the merge puts
  ours after the nearest newer shared entry (an ordering rule, no times).
- Address bar: I-beam pointer while editing; Alt+D no longer beeps; "\"
  paths go on the system drive; history of 25, saved on each typed navigation.
- LV icons looked up by type for ordinary files (cloud placeholders failed);
  rows without an icon keep its space.
- From 2026-09-28: 1 (Casso node tip), 2 (scroll reset), 4 (paths in
  messages), 6 (outline thickness).

- 9 command bar tip stays quiet after a click until the pointer leaves the
  button; 11 menu rows at FE's 46 px at 150%; 12 dots for sort, view and
  theme choices; 17 Copy to folder row removed (Copy as stays); 19 Format dialog fitted
  while hidden; 20 Format starts at the image's file system, ProDOS where
  DOS 3.3 cannot fit.

- Second round, 2026-09-29: saved column widths at the list's own DPI; the
  cut bottom row drawn, the list shifting at its end; dark header dividers
  (#474747, estimated); the address's suggestion popup no longer captures the
  mouse (I-beam); 18 via focus that does not pick a row on a click; paste into
  the same folder makes "name - Copy"; image errors lead with what is wrong
  and put the size on a line after; "Format disk image" with a File row;
  clipboard buttons only when useful, and on image entries; context menus fade
  in; flat command bar buttons (WinUI subtle fills, no border, dimmed pressed
  text, no lit hover while their menu is open, hover and tips tracked while a
  menu is open); the preview's message copies (right-click Copy, Ctrl+C); the
  new-tab + centered on the tabs, thin glyph, rounded hover; undo and redo in
  text boxes (Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z).
- Context menus: the clipboard row at the end nearer the pointer, as the menu
  lands; labels under its icons and dividers between them; icons beside the
  rows (File Explorer's where it has the verb). The preview's text views and
  its message take the I-beam; the message selects and copies.

- 10. Tooltips centered on the pointer, above it or below where there is no
  room on its monitor, with a shadow and no border (all of CE's hover tips).

- Menus against FE, 2026-09-29, measured at 150% by per-row XOR:
  - View drop-down: 3 dip above the first row and below the last, 3 dip
    separators border to border, icons on whole pixels, 8 dip narrower,
    FE's round radio dot, dark border.
  - All menus: 10 dip separators, 2 px thick at 150% in #3C3C3C (dark);
    card #2B2B2B; border 1 px left and 2 px elsewhere at 150%; no strip of
    card outside the border.
  - Icon strip: 64 dip buttons with 2 px rules, 82 px tall at 150%, icons
    and labels placed as FE's.
  - Open row: the icon of the program that opens the item (Casso Explorer's
    for folders and images). Built, not yet seen on screen.
- Passed 2026-09-30: menus, icon-row tips, new-disk extension, empty-image
  right-click, per-folder view mode.
- Message boxes draw Windows 11's status icons; the sector and block write
  confirmation reads "Write sectors" or "Write blocks" with Overwrite and
  Cancel. Not yet seen: the owner hit a plain Win32 box first (see Open).

- Built 2026-09-30, second round: path on the command line (new tab; a
  second launch with the same owner hands it over); folder watch survives a
  recycled folder and a busy folder no longer holds off re-reads (the
  deleted-folder move-up, verified by script); restored or switched-to tabs
  on a missing folder move up; FE hover tab (square bottom, 1 px edge) and
  hover cleared on leaving by any edge; known folders' full paths (the tree
  was cutting them back); address chevron removed (F4 stays); message box
  glyph no longer clipped; "Save as" titles; 4 dip above field errors.

- Passed 2026-09-30: tree overlay for clipped names (DxuiInPlaceTip);
  drops from FE into host folders (shell drop target); past-last-column
  clicks as empty space; Copy address as text (FE puts text only); tab
  hover and square-bottom selected tab; message box margins, Overwrite;
  Refresh removed from menus; address bar menu.

- Built unattended 2026-10-01, not yet seen by the owner:
  - Status bar: one disk image selected in a folder shows its own "X free
    of Y" and volume tip, as when it is open.
  - Every Dxui text box has Windows' edit menu on a right-click (Undo, Cut,
    Copy, Paste, Delete, Select all), shown by the text input itself
    (DxuiHwndSource::FromHwnd finds its popup host).
  - Tiles: a folder shows its name alone (Content keeps its details).
  - Entries inside a disk image get Copy on the menu's clipboard row.
  - Group headers in the icon, Tiles and Content views: a header line per
    group, each group starting a row of its own; collapse, header clicks,
    keyboard stops and Ctrl moves as in Details.
  - Hex view context menu moved into DxuiHexView (Show text only, 1/2/4-
    byte integer, formats, Columns, Copy), with a build hook (CE adds Go
    to) and a settings hook (CE keeps its prefs). Menu unchanged in CE.
  - Per-folder column widths: a resize is kept for that folder; folders
    without their own use the latest widths as the default.
  - Group headers in List: a block of columns per group, label on top,
    items indented and running down then across; blocks side by side.
  - Undo (Ctrl+Z on the list or tree, Edit > Undo, and "Undo delete /
    rename / new folder" at the top of the list's menu): a host delete
    comes back from the Recycle Bin (shell "undelete"), a host rename or
    new folder is reversed; inside an image a rename or new folder is
    reversed and a deletion of files is put back from an AppleSingle copy
    kept in %TEMP%\CassoExplorerUndo (a deletion that takes a folder is not
    kept). Verified by script: host recycle and restore, image new folder,
    image file delete. Not covered: copies and moves the shell performs
    (paste, drops into host folders), puts into images; the restored file
    goes into free sectors, so the image's bytes differ from before.
  - Item captions: Medium and Large icons wrap a name to two centered
    lines, Extra large, Small and List to one, each ending in an ellipsis
    when it runs on (DxuiTextElide::WrapToLines). The focused item draws
    its outline, as the focused row does in Details.
  - Tips: an item view's name cut short with an ellipsis shows whole in a
    hover tip. The rename box sits inside the row in Details (2 dip inset
    top and bottom), as the selection box does.
  - Pre-merge housekeeping done unattended: CheckStyle -Mode Tree clean
    (67 violations fixed, many from earlier rounds), FixDeclAlign applied,
    full suite 6280/6280 in Debug x64; -Target Rebuild -RunCodeAnalysis
    clean after three fixes (a tymed flag test, a null owner window for the
    drag helper, an unchecked GlobalLock in a test). Release and ARM64 not
    yet built.

## Built 2026-10-01, after the owner's review, not yet seen by the owner

- Tiles: name (up to two lines), type, size; block level with the icon;
  clipped, not ellipsized. Content: name over type, then "Date modified:"
  and "Size:" in a second column; a rule between items, none under a
  header; cyan outline for a multiple selection.
- List grouped: headers no longer collapse (no chevron).
- Square focus, selection and hover rects in every item view.
- Dark shell dialogs (uxtheme SetPreferredAppMode at startup).
- Right-click menus open on release; a right-press that moves drags. The
  release was never delivered (DxuiWindow had no OnRButtonUp), so no
  right-click menu opened anywhere until 2026-10-01 late; fixed.
- Undo row uses FE's undo icon; a restore re-reads the folder each second
  for 5 s.
- Paste from Ctrl+V or the command bar goes into the folder shown; a
  folder's own menu still pastes into it.
- Cut items draw dimmed.
- Performance: no constant repaint; grouped views no longer O(n^2); tab
  switching in a few ms (see "Fixed 2026-10-01").
- Thread names in the debugger: Casso UI, Casso Explorer UI, CPU, audio,
  controller input, printer, printer probe, disk image watcher, folder
  watcher, icon loader, info tips.
- Rename acts on the focused item, with others selected too.
- Collisions inside an image: "NAME (2)" (DOS 3.3) or "NAME.2" (ProDOS);
  a copy beside its original "NAME - COPY" / "NAME.COPY"; puts, folder
  puts and copies.
- Check 24: with no Casso running, the drive count comes from the default
  machine's configuration (last selected, else the Apple //e).
- Shell dialogs (delete, rename, paste conflicts, new folder, menu
  commands, Recycle Bin) centered over CE on its monitor.
- Hover tips on items in every view: the shell's own tip for host items
  (read in the background), type, size and date for entries in images,
  the whole name first when it is cut; Details over the row, Content over
  the icon. Not checkable by posted input; the owner to confirm.
- Folder icons show their contents at Medium and larger (shell thumbnail).
- FE's pitches measured at 150%: Extra large 209 x 211, Large 125 x 136,
  Medium 79 x 88 (FE's one-line row plus a second line), Small 34 tall,
  List 33 tall, Tiles 56 tall, Content 53 tall; Content's icon 51 dip in,
  rule 19 dip in, second column 211 dip from the right.

## Built 2026-10-03, not yet seen by the owner

- F2 renames the focused item with several selected (it needed exactly
  one). A right-click on an item already selected moves the focus to it, so
  Rename from its menu renames that item, not the last one selected.
- Recycle Bin: the command bar's Empty Recycle Bin and Restore buttons after
  View, as FE's ("Restore all items", or "Restore the selected items" with
  a selection); the list follows deletions made anywhere (shell change
  notification, verified: 1087 to 1088 items with no input); a drop on the
  Recycle Bin, in the tree or its list, goes to the bin's own shell drop
  target, which recycles it (not checkable by posted input).
- Medium, Large and Extra large wrap a name to four lines, as FE's, and each
  row is as tall as its tallest name (rows scroll as lines now). Names
  break only at spaces; a word with none fills the line, periods included,
  as FE's. Name widths measured from FE: 9 dip in from each side in Medium,
  18 in Large and Extra large. Line breaks match FE for every test name
  with spaces; names without spaces break about one character sooner.
- Extra large icons draw at 256 pixels at most, as FE's (they drew at 256
  dip, 384 pixels at 150%, over the names).
- Small icons: every column as wide as the widest name plus FE's gap, up
  to 309 dip, past which a name is cut short. List: each column as wide as
  its own widest name, uncapped. Both measured from FE.
- Names sort as FE's, numbers by value (f2 before f10), in the list and the
  tree.
- Content: a folder shows its name alone; a file's second line is
  "Type: ..."; the name in FE's 11-point face (9 for the rest).
- Undo (Ctrl+Z and the menu's Undo row) for a paste on the host (a copy
  goes to the Recycle Bin, a move goes back where it came from) and for a
  put or copy into an image (the entries it made are deleted). Paste undo
  checked on screen.
- Several items selected: Open, and Enter, open each (folders and images in
  tabs of their own, files in their programs).
- An image entry's menu: one Copy button, not two, and no Copy row
  repeating it. The list's hover tip no longer shows over an open menu.
- The list's selection no longer drops when the pointer leaves (checked;
  fixed by earlier work).
- A broken image (one that is not a disk at all: wrong size, bad header)
  has a red badge with a cross on its icon, in the list's every view and in
  the tree, and its hover tip says why. A whole disk with no file system on
  it is not broken and has none. Neither has an expand arrow in the tree.
- Column headers: dragging one moves the header itself along the row,
  over a fill, and the others slide aside (150 ms) to open the gap where it
  would land; the accent bar is gone.
- The tree scrolls to the current location when CE opens on one (it
  highlighted the node and left it out of view).
- Each folder keeps its own column order and choice of columns, as it
  already kept widths, sort, grouping and view; a folder with no choice of
  its own follows the latest one made anywhere.
- Properties of an entry inside an image: a window modeled on FE's General
  tab (it was a message box): type with ProDOS's code, location (cut at its
  start, keeping the image's name), size exact to the byte, size on disk
  with its blocks or sectors, load address or aux type, modified (ProDOS
  only; DOS 3.3 records no date), and locked. Read-only, so OK alone; no
  Details tab, since the General tab already holds all the catalog records.
- Navigation pane as File Explorer's (T132-T134): below Casso's root, Home,
  Gallery and the OneDrive root; a line; the folders pinned to Quick access;
  a line; iCloud Drive and Photos, This PC (Casso Explorer's own, with its
  drives), Libraries, Network and Linux, as the shell lists them for the
  pane; the Recycle Bin last. Any of these, and any shell folder by its
  shell: or ::{...} name (address bar or command line), opens in the list
  with the shell's own icons, types and dates; Up goes to the shell
  parent; a tab keeps its name. Pin to Quick access and Unpin from Quick
  access in the list's and the tree's menus for a folder change File
  Explorer's list too (checked both ways on a scratch folder; the owner's
  list was left as it was). Startup stays under a second: Network and the
  shell roots open only when asked.
  Still different: the OneDrive root shows a plain folder icon (FE's is the
  cloud); pins have no pin glyph at their right; expanding Network, in the
  tree or the list, waits on the network on the UI thread (up to 30 s here).
- Sector and block reads and writes: a dialog of their own with a field each
  for track, sector and count (block and count), checked as typed against
  the disk's 35 tracks of 16 sectors or 280 blocks, a count reaching the end
  of the disk at most; Logical (DOS order) or Physical (drive order) for
  sectors; titles without "&" or dots; the picker lists Sector dumps (*.bin)
  and All files, and keeps its own folder and file-name history (its own
  client id). Defaults, said under the fields: track 17 sector 0, where a
  DOS 3.3 catalog is; block 2 for 4, a ProDOS volume directory.
- Copy to folder is gone from the menus (FE has none); recent destinations
  for it are moot.

## Open: from 2026-10-01 (owner's answers and the Recycle Bin review)

- Content, still different: FE leaves the type line off some files (a
  .txt shows none; the shell's own property list for the type decides),
  and FE's second column sits further right on a wide window (about 59% of
  the width, where CE keeps it 211 dip from the right edge).
- Navigation pane as FE (owner's item 4): Home, Gallery, the OneDrive
  root ("Rob - Personal"), divider, Quick access (pinned and frequent),
  iCloud Drive and iCloud Photos (shell namespace extensions pinned to the
  tree), This PC, Libraries, Network, Linux (WSL). Needs CE to browse any
  shell folder through its IShellFolder, not only disk folders. Pin to
  Quick access and Remove from Quick access through the shell's verbs.
- Tree empty-area menu as FE (item 5): Show This PC, Show Network, Show
  libraries, Show all folders, Expand to current folder.
- Address bar leading icon and its drop-down (item 6): the shell's root
  (Desktop) entries, then the folders on the user's Desktop, as FE lists.
- Search box working as FE's (item 7): Windows Search queries (AQS) over
  the index, crawling where unindexed; results in the list; extended with
  matches inside disk images and in Casso's folders.

## Open: from 2026-09-30


- Undo of drops onto host folders: the shell's own drop target carries
  those out and keeps their undo in Explorer's list, not CE's. (Paste, and
  puts and copies into images, built 2026-10-03.)


- Group headers (13), full FE behavior (1-7 built and passed in Details
  view 2026-09-30, with Ctrl+arrows moving focus and Space toggling):
  1. Collapsible; a down chevron at the left when expanded, a right chevron
     when collapsed, as a tree node.
  2. A click on the chevron or a double-click on the header toggles it.
  3. The header is a keyboard stop like a row; Left collapses, Right expands.
  4. Hover and focus rects span the full pane width, not the columns.
  5. Selecting a header selects the group's items; the header itself never
     shows selection. Ctrl and Shift clicks then work as usual.
  6. Focus rect on click or keyboard; hover highlight like a row.
  7. Context menu (FE capture 2026-09-30): the clipboard row, then Collapse
     group (or Expand group, "Left"/"Right"), Expand all groups, Collapse
     all groups, then the ordinary multi-selection menu.
  8. Every view mode. All but List draw the header the same way.
  9. List: each group is a column block under its header label (not a
     Details column header), items indented from the header's left and
     flowing into sub-columns within the block.
- Multi-selection verbs apply to each item (Copy as path: one path per line).


## Open: from 2026-09-29

- Menus, still open: context rows 2 px lower than FE in 33.png (FE itself
  moves 1-2 px with the popup's position; match 30.png); View drop-down
  border (FE may have none, only a shadow; needs a wider FE capture); light
  theme check (needs FE light captures); acrylic backdrop (DWM transient
  backdrop, which likely needs the popup sized to the card with DWM's own
  corners and shadow).




- 21. Tree section for Quick access (pins and frequent folders) matching FE,
  with Pin to Quick access / Unpin in CE's menus, shared with FE's list.

- 9-15 (item views) and the item views' outlines: built 2026-10-01 and
  2026-10-03 (see those sections).


## Open: from the T077 walkthrough

- A mouse release that goes unnoticed leaves a later move starting a stray
  drag or put. Recheck after the drag-start fix.



- Apple file-type icons for entries inside images, by DOS 3.3 and ProDOS
  type; not registered with Windows.
- New spec proposal: disk image icons registered with Windows, Open
  starting a machine, Open with offering CE.


- Optional: rename in the tree keeps its node open by following the rename
  through ReadDirectoryChangesW rather than remove-and-add.

## Fixed 2026-10-01: constant painting

- Root cause 1: Dxui repaints the whole window after any WM_TIMER the client
  reports handled, and CE's 16 ms tooltip tick always did, so CE painted
  back to back forever. The tick now reports not handled and repaints only
  for what moved; the search caret repaints only when it turns on or off.
- Root cause 2: grouped item views rebuilt the whole group layout for each
  item they painted (and for each item a selection band tested), so a frame
  cost the square of the item count: 306 ms in List on system32 (Debug).
  The layout is built once per paint; every view now paints in under 4 ms.
- The tick timer now runs only while something animates; input starts it.
- Group layouts are kept until the grid, the groups or the item count
  change, so hit tests and mouse moves no longer rebuild them.
- Switching to a System32 tab took 760 ms (Debug): 650 ms of it was icons,
  because the cache emptied whenever the icon size changed and every file
  was looked up on disk (a cache-key test that never matched). Each size now
  keeps its cache, per-file icons load on a background thread, and rows'
  cells are built only as the list first shows them. Now 13-75 ms in Debug,
  4-14 ms in Release.

## Built 2026-10-01: Recycle Bin

- A Recycle Bin root in the tree after This PC, as Explorer's pane shows it
  with all folders shown; "Recycle Bin" or shell:RecycleBinFolder in the
  address bar or as CE's argument; tabs keep it across runs.
- Its items with Explorer's columns and order: Name, Original location,
  Date deleted, Size (folders too), Type, Date modified.
- Menu on items: Restore, Delete (for good, after the shell's confirmation),
  Show more options (the bin's own shell menu); on the background, Empty
  Recycle Bin. Enter or double-click shows an item's properties.
- The rest: built 2026-10-03 (above).


## Before merge

- Full suite in all four configurations, -Target Rebuild -RunCodeAnalysis,
  CheckStyle -Mode Tree; mutation checks only where the risk warrants.
- T077 drag checks 17-22, then record every check in validation.md.
- Restore the owner's prefs, UserPrefs (disk1Path), KnownFolders and theme
  from the scratchpad backups; clear d.woz's read-only flag.
- CHANGELOG and README lines, owner-approved; T082 and T131.
