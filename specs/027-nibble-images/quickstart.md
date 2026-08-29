# Quickstart: validating nibble disk image support

How to prove the feature works end to end, and how to get a test image without
downloading one.

## Prerequisites

- The solution builds. Build through `Casso.sln`, not the `.vcxproj`, or the
  executable lands in the wrong directory and a stale binary gets tested.
- No third-party binaries are downloaded for any step here. The test image is
  generated from a disk already in the repository.

```powershell
scripts\Build.ps1 -Configuration Debug
```

## Making a test image

There is no `.nib` in the tree and none should be downloaded. Two different images
are wanted, and they are not made the same way.

**A converted copy of a real disk**, for the boot and catalog comparisons. This is
`Apple2/Demos/casso-rocks.dsk` GCR-encoded into a nibble image -- the same work the
mount path already does, serialized out. Until the codec exists there is no command
for it, so produce it from the codec directly (the fixture builder in T003 is the
same conversion, and the round-trip assertions live beside it).

**A blank formatted disk**, for the `create` and `init` scenarios:

```powershell
CassoCli disk create work.nib --type nib --bootable
```

Do not confuse the two. `disk create` writes an image with a fresh catalog and
nothing in it; it will not boot into `casso-rocks`, because that program was never
put on it.

Expect 232,960 bytes. Verify before trusting it:

```powershell
(Get-Item casso-rocks.nib).Length
```

A `.nb2`-sized companion (223,440 bytes) is worth generating too, and a copy of the
232,960-byte file renamed to `.nb2` -- the mismatched-name case is the one the
length-decides rule exists for.

## Scenario 1 -- it mounts and boots

```powershell
x64\Debug\Casso.exe --machine Apple2e --disk1 casso-rocks.nib
```

Expected: the converted image boots and behaves as `casso-rocks.dsk` does. Use the
converted copy here, not the blank one from `disk create`. Capture the framebuffer to compare
against the same disk in another format rather than judging by eye.

Repeat with the file renamed `.nb2`, and with the 6,384-per-track image named
`.nib`. Both must open.

## Scenario 2 -- an untouched image is not modified

```powershell
$before = (Get-FileHash casso-rocks.nib).Hash
```

Mount, let it boot, eject, quit. Then:

```powershell
(Get-FileHash casso-rocks.nib).Hash -eq $before
```

Expected: `True`. A mount that writes anything at all has failed FR-010.

## Scenario 3 -- guest writes survive

Boot a DOS 3.3 nibble image, `SAVE` a short program, eject, remount, `CATALOG`.

Expected: the program is there and loads. Then confirm the blast radius was one
track: compare the file against its pre-write copy and check that only the blocks
for tracks the write touched differ.

## Scenario 4 -- it does not degrade

Repeat scenario 3 several times against the same file, saving and deleting.

Expected: the volume stays readable every cycle. This is the test that catches a
padding rule that eats a little more of the track each pass.

## Scenario 5 -- refusals name the problem

```powershell
CassoCli disk list truncated.nib      # a file cut to 100,000 bytes
CassoCli disk list renamed.nib        # 232,960 bytes of zeros, or a renamed archive
```

Expected: two different messages. One names the length found and the lengths
accepted; the other says the size is right and the contents are not nibbles.
Neither may raise an assertion dialog in a Debug build.

## Scenario 6 -- the console commands agree with the sector path

Run each command against the nibble image and against the same disk as `.dsk`,
and compare:

```powershell
CassoCli disk list casso-rocks.nib
CassoCli disk create work.nib --type nib --bootable
CassoCli disk put work.nib prog.bin --as PROG --type B --load $6000
CassoCli disk get work.nib PROG --out roundtrip.bin
CassoCli disk init work.nib --format prodos --volume WORK
CassoCli disk sectorread work.nib --logical --track 0 --sector 0 --out boot.bin
```

Expected: identical results to the `.dsk` equivalents, and `work.nib` mounts and
boots after `create --bootable`.

## Scenario 7 -- the file filter followed along

No command for this one: confirm by inspection that
`Casso/Ui/DriveWidgetState.h` was not changed, and that the picker and
drag-and-drop now offer `.nib` and `.nb2`. If the filter needed editing, the second
extension list has come back.

## Gates before this is done

```powershell
scripts\Build.ps1 -Configuration Debug
scripts\Build.ps1 -Configuration Release
scripts\RunTests.ps1 -Configuration Debug -Build
scripts\RunTests.ps1 -Configuration Release -Build
scripts\CheckStyle.ps1
scripts\Build.ps1 -RunCodeAnalysis
```

Zero warnings in both configurations, both suites green with counts reported, style
clean. Debug and Release run different test sets, so neither substitutes for the
other. A Debug suite run on a fresh worktree has taken up to 14 minutes; run it in
the background and wait for it.

Confirm the test assembly is newer than the sources before believing any result --
`RunTests.ps1` guards this, but the guard only detects staleness in one direction.
