# Quickstart: validating the picker

How to prove the feature works. Contracts are in [contracts/](contracts/); types are in [data-model.md](data-model.md).

## Prerequisites

Run once in a new worktree, or the suite stops before any test and some Casso Explorer tests fail with `E_FAIL`:

```powershell
scripts\FetchRoms.ps1 -Fixtures
scripts\FetchCiderPressTestFiles.ps1 -Extract
```

## Unit tests

```powershell
scripts\RunTests.ps1 -Build -Filter ImageClassifier
scripts\RunTests.ps1 -Filter DriveImageRule
scripts\RunTests.ps1 -Filter DriveRuleMountAgreement
scripts\RunTests.ps1 -Filter Picker
scripts\RunTests.ps1 -Filter DropRule
scripts\RunTests.ps1 -Filter CassoImagesListing
scripts\RunTests.ps1 -Filter CassoHostDialogs
scripts\RunTests.ps1 -Filter DiskMru
```

A filtered run is not the suite. Before the branch is reported done:

```powershell
scripts\RunTests.ps1 -Build
scripts\RunTests.ps1 -Build -Scenario
```

Every run is green, and `ImageClassifierScenarioTests` reports a non-zero count of fixtures classified and compared with the mount.

## Window checks

**Hard rules for these runs.** Never start the owner's Casso Explorer. Never open COM3 or any serial port. Use a test copy of the emulator with a profile folder set, so its preferences are separate, and restore only what the run changed. Launch minimized with `--title <worktree name>`. Send posted input only; never foreground input, and ask before moving the real pointer.

The picker is in the emulator's process, so these checks start only `Casso.exe`. A check that finds a `CassoExplorer.exe` process started by the run, or a write to `CassoExplorerPrefs.json` or the session file, is a failure of FR-031.

Prepare a folder with one image per row of the table and insert one of them once, so the Casso node has the folder.

| Image | Disk II | Apple 3.5 Drive | UniDisk 3.5 | HD20SC / ProFile | Recorder |
|---|---|---|---|---|---|
| 140K `.dsk`, `.do`, `.po`, `.nib`, 5.25-inch WOZ | listed | no | no | no | no |
| 400K and 800K `.po`, `.2mg`, DiskCopy 4.2 | no | listed | listed | no | no |
| 3.5-inch WOZ | no | listed | as the device mounts | no | no |
| 32 MB `.hdv` | no | no | no | listed when it fits | no |
| tape recording | no | no | no | no | listed |
| truncated `.dsk`, a `.txt` | no | no | no | no | no |

For each drive kind on a machine that has it:

1. Click the drive. The picker opens on the Casso node, the header has the drive and what is listed, and the rows match the drive's column above.
2. Click the same drive again. No second picker opens.
3. Click a second drive. A second picker opens, and the first has the same drive and listing.
4. Double-click an image. It is inserted in that picker's drive, the door animation runs, and the picker closes.
5. Browse to another folder, close the picker, and open it again for the same drive. It is on the Casso node's list with the same columns, sort and size. Select "Stock disks" under the Casso node and confirm only stock disks the drive accepts are listed.
6. Open and cancel. The drive's contents do not change and an empty drive's door rests open.
7. Switch the machine with a picker open. The picker closes and nothing is inserted.
8. Try Delete, F2, Ctrl+C, Ctrl+V, Ctrl+N and a right-click on a row and on the background. No file-changing command runs or is offered.

Drops: drag each image over each drive from File Explorer and from a picker. The cursor is the copy cursor only where the table lists the image, and releasing elsewhere changes nothing.

Settings: on the Hardware page, select a port for each storage card and browse. The picker is modal over Settings, lists what that port's drive accepts, and writes the path into the field.

Casso Explorer as a file manager (a test copy, with no saved serial session): the Form factor and Formatted size columns fill in an ordinary folder, and the Casso node lists images by most recent use.

Captures of the picker for each drive kind, in a light and a dark theme, go to the owner for the gestalt check before the branch is reported done.

## Performance

In a folder of 500 images on a local drive, measured in a Release build: the list appears in under 1 s, both columns are filled in under 2 s on the first visit, and in under 0.5 s on a return visit.

## Before merging

Merge `048-smartport-drives` again, run `scripts\CheckStyle.ps1 -Mode Tree`, then `scripts\RunMergeGate.ps1`. Merge only on the owner's instruction.
