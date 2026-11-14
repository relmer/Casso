# Validation: SmartPort and hard drive devices

Results of the boots and measurements the tasks call for. Each entry gives
what was run, on what, and what happened.

## Apple II SCSI Card

Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`, Apple's ROM
341-0437-A, enhanced //e unless stated:

| Case | Result |
|---|---|
| ProDOS 1.0.1 (the ProDOS Users Disk in block order) at ID 6, no floppy controller | Boots to the ProDOS banner |
| ProDOS 2 (the Merlin Pro fixture in block order) at ID 6, a blank ProDOS volume at ID 5 | Boots to BASIC.SYSTEM; `CAT /SECOND` lists the volume at ID 5; a guest `CREATE` is in the image afterward and the volume's integrity report is clean |
| No image on the card, floppy in slot 6 | The floppy boots |
| Blank 1 MB image at ID 6, floppy in slot 6 | The floppy boots |
| ][+ and unenhanced //e, ProDOS 1.0.1 at ID 6 | Boots with `PR#7`; neither ROM starts the card at power-on |

### Boot delay (FR-014c)

Emulated time from power-on to the first instruction of the Disk II boot ROM,
real timing off, enhanced //e:

| Setup | Casso | MAME 0.289 |
|---|---|---|
| No card | 0.13 s | 0.13 s |
| Card, no hard disk | 4.8 s | 1.5 s (a CD drive at ID 1) |
| Card, blank 32 MB hard disk at ID 6 | 1.2 s | 2.8 s |

The default configuration has the stock image at ID 6, the last row.

## Compared with MAME 0.289 (T026, T054, T072, T073)

The same image files in both, made by `CassoCli disk create --bootable` in
`%USERPROFILE%\Downloads\Casso\mame048`: an 800K ProDOS volume as `.po`,
as DiskCopy 4.2 and, for the SCSI card, as `.hdv`; 5 MB and 32 MB `.hdv`;
32 MB `.2mg`; and a 140K `.po` for the //c. MAME ran headless, and a Lua
script printed the emulated time at which "PRODOS BASIC" first appeared
on the text page. Casso ran `ScenarioTests/MameComparisonTests.cpp` with
`CASSO_MAME_COMPARE` set to the folder, sampling the screen every 0.1 s.
MAME's ROMs: `a2scsi` 341-0437-A, `a2superdrive` 341-0438-A (copied in from
the fetch on 2026-11-14) and `apple2cp` ROM 05; all verify.

| Case | Casso | MAME |
|---|---|---|
| SCSI card in slot 7, ID 6: 800K `.hdv`, 5 MB `.hdv`, 32 MB `.hdv`, 32 MB `.2mg` | ProDOS BASIC prompt at 2.84 s | ProDOS BASIC prompt at 2.97 s |
| 3.5 controller card in slot 5, Apple 3.5 Drive: 800K `.po`, DiskCopy 4.2 | 3.33 s | 3.97 s |
| //c Plus internal drive, 800K `.po` | 11.76 s | 12.10 s |
| //c, 140K `.po` | 9.11 s | 9.08 s |

Every image boots to the same screen on both. Each case's four or two images
take the same time on each emulator.

The first run had the 3.5 controller card at 14.80 s. Casso's Apple 3.5
Drive read every register it didn't model as 1, so the card's firmware took
it for a SuperDrive in MFM mode: it read the SuperDrive, MFM mode, index and
1.44 MB lines, failed to read the GCR disk as MFM, and recalibrated with 16
steps out after each seek, 606 steps in all. MAME's Sony drive reads those
lines as 0 on an 800K drive. With them at 0, and the disk switched line set
by an insert and cleared by its command, the boot makes 6 steps.
`Sony35DriveTests::SuperDriveLines_ReadLowOnAnEightHundredKDrive` tests it.

### //c Plus speed (SC-005a)

`POKE 768,2:FOR I=1 TO 5000:NEXT:POKE 768,1` typed at the ProDOS BASIC
prompt, timed from the first POKE to the last:

| | Casso | MAME |
|---|---|---|
| //c | 4.834 s | 4.833 s |
| //c Plus, accelerator at its power-on setting | 1.233 s | 1.233 s |
| Ratio | 3.92 | 3.92 |

3.92 is 4 MHz over the //c's 1.0205 MHz. MAME's times are to the frame, 1/60
s. The first Casso run measured 0.771 s and 4.453 s, because
`KeystrokeInjector::InjectLine` runs half a second after the Return before
returning; the test types the line and the Return separately.

### Not compared

MAME doesn't emulate the ProFile, the UniDisk 3.5 or its card, so those have
no MAME result. A WOZ 3.5-inch image and an external drive on the //c Plus
port are open with T054 and T073.
