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

- Merged `origin/048-smartport-drives` at `929c28d32`. `scripts\CheckStyle.ps1 -Mode Tree`: OK.
- `scripts\RunMergeGate.ps1`: passed. Unit suite: 9,915 of 9,915 in Debug and 9,914 of 9,914 in Release. Scenario suite: 245 of 245 in Debug and under ASan. x64 and ARM64 build in Debug and Release.
- Code analysis failed the first runs on code from 048: C6011 in `CommandLineParserBasic.cpp`, C6387 in `Win32VolumeFormat.cpp` and C28020 in `NuLzw.cpp`, each fixed with a check, and C6262 in five test files, fixed by moving each test's rigs, rings and desk scene to the heap.
