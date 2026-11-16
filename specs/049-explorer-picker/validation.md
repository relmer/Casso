# Validation: Casso Explorer as the insert-disk picker

Window checks run on a test copy of the emulator: `CASSO_PROFILE_DIR` set to a scratch folder, so the owner's `UserPrefs.json`, `CassoExplorerPrefs.json` and `CassoExplorerSession.json` were not read or written, launched minimized without activation, placed on the portrait monitor, driven by posted messages and captured with `PrintWindow`. No Casso Explorer process was started and no serial port was opened: the picker runs inside `Casso.exe` with no CassoLink connections.

## Phase 7 (2026-11-14), Apple //e with a Disk II, an HD20SC and the recorder

| Check | Result |
|---|---|
| Storage > Insert disk for drive 1 opens "Insert disk - Drive 1 - Disk ][" on the Casso node's list | Pass. Five 140K images listed; the status bar has "5 items, 4 more not for this drive" (the text at the time; since reworded) |
| First-use columns: Name, Form factor, Formatted size, File system, Volume name, Last used, Folder; last used first | Pass |
| The tree has no CassoLink node; the header has the drive and "5.25-inch disk images" | Pass |
| Selecting a row enables Insert; the Preview button lists the selected image's catalog | Pass |
| Insert mounts the image in drive 1 and closes that picker | Pass. The drive bar had the new image's name |
| Insert disk for drive 2 while drive 1's picker is open opens a second picker; a second request for drive 1 does not open a third | Pass |
| Esc closes the picker and nothing is inserted | Pass |
| Two clicks on a row insert it | Pass |
| A click on the recorder opens "Insert tape - Cassette recorder", which does not list a disk image | Pass. "0 items, 9 more not for this drive" |
| A click on the HD20SC opens "Insert disk - HD20SC - ID 6" with "Hard disk images up to 32 MB" | Pass. The 1.6 MB `.hdv` was the one image listed; the 800K images were not |
| Insert for the hard disk stages the image, and the infobar opens | Pass. The machine's saved port had the image's path; the infobar had the drive and Restart now |
| Restart now rebuilds the machine with the image attached and closes the infobar | Pass |
| Closing the emulator with a picker open closes the picker | Pass |
| The profile folder after the runs | `CassoPickerPrefs-Floppy525.json`, `CassoPickerPrefs-HardDisk.json`, `KnownFolders.json`, `UserPrefs.json`. No `CassoExplorerPrefs.json`, no session file |

## Phase 8 (2026-11-14)

| Check | Result |
|---|---|
| A 5.25-inch drive's picker has a "Stock disks" node first under the Casso node, and a "Create new disk..." button | Pass (capture) |

## Not checked on screen

- The New disk image dialog from "Create new disk...", "New blank tape", a stock disk's download, the modal open and save forms (Settings' Hardware page, the recorder's new tape, Casso Explorer's "Write image to..." and its other image requests), and a 3.5-inch drive's picker. Each opens a modal window. In three of the runs above a picker took the foreground from the owner's work (twice when the test copy's launch had activated it, once after posted clicks), so no modal window was opened from a posted message. These paths are covered by unit tests over the window's parts (`ExplorerPickerWindowTests`, `DrivePickersTests`, `CassoHostDialogsTests`, `PickerSaveFormTests`) and are not yet checked in the running app.
- The no-drop cursor during a real drag. An OLE drag cannot be posted. `DropRuleTests` covers which tags accept which images.
- Casso Explorer's image requests. A test copy of Casso Explorer was not launched.
- Listing time for a Casso node with 500 images.


## Phase 10 (T091)

- References to the removed code (`PromptInsertDiskMru`, `PromptInsertTapeMru`, `PromptForDiskImage`, `BrowseFloppy35`, `MakeDiskPickerKind`, `PromptBootDiskMru`, `DiskMruPickerSession`, `MediaPickerKind`) in the C++ sources: 0.
- `PickFileToOpen` and `PickFileToSave` calls in `CassoEmuCore/Shell` and `CassoEmuCore/Ui`: 11. Image requests with `content = Images`: the lost-disk save (while the machine runs), the tape open and the new tape, the inspector's compare, its image save and its WOZ export, and the Hardware page's image. Not image requests: the printout's PNG, the block inspector's text and binary export, and the serial card's two file requests.
- `WindowCommandManager::CreateBlankDiskForDrive` and its dialog are no longer called: a drive's picker creates through Casso Explorer's New disk image dialog.

## Phase 11 (2026-11-15)

- Unit suite: 9,913 of 9,913 passed. Scenario suite: 245 of 245 passed, `ImageClassifierScenarioTests` among them (18 fixture images, each mounted on a Disk II, a 3.5-inch drive, an HD20SC and a 5 MB ProFile).
- `scripts\CheckStyle.ps1 -Mode Tree`: OK after three blank-line fixes.
- The startup form, on a test copy with no saved boot disk: it opened minimized without activation, had the Stock disks node and "Start without a disk", and closing it quit Casso. Start and Start without a disk were not clicked on screen; unit tests cover both outcomes.
- T097: the docs that mention the insert disk dialog are release notes in `docs/WhatsNew.md`, which describe past releases and are left as written.

## Merge gate (2026-11-15)

- Merged `origin/048-smartport-drives` at `929c28d32`, then at `cf67245e0`. `scripts\CheckStyle.ps1 -Mode Tree`: OK.
- `scripts\RunMergeGate.ps1`: passed. At `a0cbe9109`: unit suite 9,917 of 9,917 in Debug and 9,916 of 9,916 in Release, scenario suite 250 of 250 in Debug and under ASan. One earlier run failed `AShimAtTheBootstrapSpeed_IsReadWithinASlice`, a serial timing test from master, which passed alone three times and in the next full run. x64 and ARM64 build in Debug and Release.
- Code analysis failed the first runs on code from 048: C6011 in `CommandLineParserBasic.cpp`, C6387 in `Win32VolumeFormat.cpp` and C28020 in `NuLzw.cpp`, each fixed with a check, and C6262 in five test files, fixed by moving each test's rigs, rings and desk scene to the heap.

## On screen (2026-11-15)

Test copies of Casso at `3a8b17f67`, each with `CASSO_PROFILE_DIR` set to a scratch profile and the title "Explorer Picker Test":

- Startup form with an empty Casso list: it opened on Stock disks. Picking the ProDOS Users Disk and Start downloaded it into the profile's Disks folder and booted it.
- Startup form with images in the Casso list: it opened on the Casso list. "Start without a disk" started Casso with drive 1 empty.
- //c Plus: no startup form, since the machine has no Disk II controller. The built-in 3.5-inch drive's picker listed the two 800K images and hid seven, and Insert closed it.
- Insert tape: the picker was empty, with nine images hidden. New blank tape opened the modal Save as form in Documents\Casso Disks, and Cancel returned to the tape picker.
- Not run: Casso Explorer's image requests. A test copy from this build uses the window class `CassoExplorerTestCopyWindow`, and another session's test copy with that class was running, so a launch would go to that copy.
- Found: on a //c Plus the 3.5-inch drive's motor sound runs from launch with no disk inserted (code from 048, reported to that branch). The Size column of a stock disk that isn't downloaded yet read "0 bytes", and is now blank.
- Startup form for the boot drive, at `7be4a7872`: a //c opened it for its built-in drive, and a //c Plus for its internal 3.5-inch drive with only the 800K images listed. A pick on the //c Plus booted from the image and was saved on the internal port, and the next launch skipped the form. Not on screen: a machine with Disk II turned off and a 3.5-inch or UniDisk 3.5 card, and a hard disk in slot 7, which `BootDriveRuleTests` covers.

## T032, T095 and the 800K stock disk (2026-11-15)

- T032, measured by `PickerTimingTests` in a Release build: 500 images on the local drive (200 DOS 3.3 `.dsk`, 200 ProDOS `.po`, 100 `.woz`), listed through the browser the picker uses. First visit: the list in 36 ms, both columns filled in 709 ms. Return visit: the list and both columns in 2 ms. The images were written by the test just before, so Windows had them in its file cache.
- T095, a test copy of Casso Explorer from this build with a scratch profile folder, two image folders in its `KnownFolders.json` and no CassoLink connections. Launched minimized without activation, placed on the portrait monitor, captured with `PrintWindow`, closed with its Exit command. No input was posted. The Casso node listed the six images of both folders as one list, with the Folder, File system and Volume name columns filled. A plain folder had the File system and Volume name columns filled.
- The test copy's window class was `CassoExplorerTestCopyWindow.B8EEC345`: a hash of the profile folder is now part of the class, so a test copy with a different profile folder opens in a separate app.
- The 800K stock disk built from the downloaded ProDOS 2.4.3 image (SHA-256 matched) booted to Bitsy Bye on an enhanced //e with the Apple 3.5 controller card, listing every file of the 140K disk, and to the ProDOS 2.4.3 banner on a //c Plus. `StockDisk800K_Boots` boots one built from the ProDOS User's Disk on both.

## T073, T080, T089, T023 and the tips (2026-11-16)

Test copies of Casso and Casso Explorer at `33b1fc9a6`, each with a scratch profile and the title "Explorer Picker Test", on the portrait monitor. The owner allowed focus changes and the real pointer for this run.

| Check | Result |
|---|---|
| T073: Create new disk from a Disk II picker | Pass. `New Disk.woz` (DOS 3.3, 140K) was made in the folder listed in the picker and inserted in drive 1 |
| T073: Create new disk from the //c Plus's 3.5-inch picker | Pass. The dialog started at ProDOS, .po, 800K. `New Disk.po` was made and inserted: a power cycle reported "UNABLE TO FIND A BOOTABLE DISK ONLINE." |
| T073: New blank tape from the recorder's picker | Pass. The Save as form saved `New tape.wav`, and its name was on the recorder |
| T073: a stock disk | Pass. Insert put the DOS 3.3 System Master in drive 2. On the //c Plus's startup form, "ProDOS 2.4.3 (800K)" was built from the downloaded ProDOS 2.4.3 and booted to Bitsy Bye |
| T080: Choose image for the HD20SC on the Hardware page | Pass. The picker was modal over Settings, listed hard disk images up to 32 MB, and the pick was written into the drive's row |
| T080: Choose image for the UniDisk 3.5 | Pass. The picker listed only 400K and 800K images, and the pick was written into the row. Settings was canceled |
| T089 | The save form ran from the emulator's New blank tape. "Save disk as" for a lost disk and Casso Explorer's "Write image to..." on a real connection were not run; `PickerSaveFormTests` and `CassoHostDialogsTests` cover them |
| T023: a real OLE drag of a file onto the //c Plus's 3.5-inch drive | A 140K image: effect None, the no-drop cursor, nothing inserted. An 800K image: effect Copy, inserted, and it booted |
| T023: the same onto the //e's Disk II | An 800K image: effect None. A 140K image: effect Copy, and its name was on the drive |
| The stock disk tips and the Casso node's tip in a disk drive's picker | Pass, with the window active: the approved texts, and "Disk images in the folders Casso has used, most recently used first" |
| Tips hidden when a picker opens | Not checked: neither the toolbar's labeled buttons nor the drive bar raised a tip to hide |

Found:
- Two clicks on a stock disk's row select it and open an empty preview, but do not insert it. Insert does.
- With a stock disk selected, the status bar reads ", 140 KB", with a stray comma.
- The //c Plus's 3.5-inch picker has no "In drive" mark for the inserted image.
- The 3.5-inch picker draws a tree row's arrow below the bottom of the tree.
- On the Hardware page, a click on a drive's name does not select the drive, so Choose image stays unavailable. Only a click on the empty part of the row selects it.
- On a //c Plus, Insert disk for drive 1 opens the external 5.25-inch drive's picker.

## The fixes for those findings (2026-11-16)

| Finding | Result |
|---|---|
| Two clicks on a stock disk | Not a defect. The test posted WM_LBUTTONDBLCLK, which a Dxui window never receives without CS_DBLCLKS. A real double-click inserted ProDOS 1.0.1 and closed the picker |
| The stray comma | Fixed in `346c9273d`. The status bar reads "140 KB", and nothing for a stock disk not yet downloaded |
| The //c Plus's "In drive" mark | Fixed in `fafe6b9f6`. 3.5-inch drives and hard disks are marked too: the built-in drive's image reads "In slot 5 drive 1" |
| The arrow below the tree | Fixed in `5fe94bfa3`. The tree clips its arrows and check boxes to the visible rows |
| A click on a drive's name | Fixed in `1579abe33`. It selects the drive, and Choose image becomes available |
| Insert disk for drive 1 on a //c Plus | Fixed. On a //c Plus, the drive 1 commands act on the built-in 3.5-inch drive, and the drive 2 commands on the first 5.25-inch drive on the disk port. A test copy opened "Built-in 3.5 - S5,D1" for Insert drive 1. With no 5.25-inch drive attached, Insert drive 2 opened nothing. With one attached, it opened "Drive 1 - Disk IIc" |
