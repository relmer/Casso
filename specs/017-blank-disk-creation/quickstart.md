# Quickstart: validating Blank Disk Creation & Mounting (017)

Prerequisites: x64 Debug build of `Casso.sln`; full `UnitTest.dll` suite green.
Automated gates live in `UnitTest/EmuTests/` (see plan.md); the scenarios below
are the user-level proof.

## 1. Default create-and-save (US1, SC-001/SC-002)

1. Launch Casso (//e), click an empty drive → insert picker opens.
2. `<Create new disk...>` is the FIRST row; sort by any column, type in the
   search box; it stays first and never filters out.
3. Select it; the create dialog opens in `Documents\Casso Disks` (created on
   demand) with a unique default name like `Blank Disk.woz`, format WOZ,
   contents DOS 3.3, bootable off.
4. Click Create. The drive mounts the new disk (door closes, widget shows it,
   MRU gains it).
5. In the guest (with DOS booted from a master in drive 1):
   `SAVE TEST` → `LOAD TEST` → `LIST`, no I/O error.
6. Quit, relaunch, remount the image: `LOAD TEST` still works (SC-002).

## 2. Format × contents matrix (US2, SC-003)

For each offered combination, WOZ×{DOS 3.3, ProDOS, unformatted},
DSK×{DOS 3.3, unformatted}, PO×{ProDOS, unformatted}:

- DOS 3.3 disks: guest `CATALOG` lists clean and empty.
- ProDOS disks: guest `CAT` (from a booted ProDOS) shows an empty `NEWDISK`
  volume, 273 blocks free.
- Unformatted: DOS `CATALOG` fails until the guest `INIT`s it; after `INIT
  HELLO` the disk boots.
- Invalid pairings (DSK+ProDOS, PO+DOS 3.3) are not selectable (FR-010).

## 3. Bootable (US2 scenario 4/5, SC-006)

1. With the DOS 3.3 System Master absent from the cache: bootable toggle is
   disabled with an explanation + Download affordance.
2. Download; toggle enables. Create bootable DOS 3.3 WOZ; boot it (PR#6 /
   power-cycle with only it mounted) → clean Applesoft prompt (HELLO runs,
   no `FILE NOT FOUND`).
3. Create bootable ProDOS 1.1.1 disk → boots to ProDOS/BASIC.SYSTEM prompt.

## 4. Save-dialog navigation (US3)

1. In the create dialog, double-click into a subfolder, `..` back up, watch
   the list show that folder's existing images (folders first, name-sorted).
2. Type an existing image's name → Create asks for explicit overwrite confirm;
   declining changes nothing (FR-007).
3. Create in a custom folder; reopen the dialog later, it opens in that
   folder (lastDiskCreateFolder persisted; survives relaunch).
4. Keyboard-only pass: Tab through list → name → format → contents → bootable
   → buttons; Enter in the name field creates; Escape cancels.

## 5. Refusals & failures (FR-009/011/018, SC-004)

1. Target an image currently mounted in a drive → refused with "mounted in
   Drive N"; no overwrite-confirm offered; file untouched.
2. Create into the drive that already holds a disk → replace-confirm appears
   first (FR-009).
3. Make the target folder read-only → clear error, no partial file, prior
   mount untouched (FR-011).

## 6. Write-protect toggle (US4, SC-005)

1. Mount a created WOZ; Disk menu → "Write-Protect Disk 1" → padlock appears
   on the widget (tooltip names the image flag); guest `SAVE` fails with
   `WRITE PROTECTED`. Untoggle → `SAVE` succeeds.
2. Remount the WOZ after toggling ON in a previous session → still protected
   (flag travels in the file's INFO chunk).
3. Repeat with a `.dsk`: toggle sets the host file's read-only attribute
   (verify in Explorer); guest behavior identical; toggle OFF clears it.
4. Deny the attribute change (e.g. ACL) → error dialog names the cause; menu
   check state still matches reality (FR-016).
5. Settings > Disk's per-drive user write-protect still works independently
   and both are reflected in the tooltip's cause list.
