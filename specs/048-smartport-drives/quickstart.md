# Quickstart: validating SmartPort and hard drive devices

## Prerequisites

```powershell
scripts/FetchRoms.ps1 -Fixtures
scripts/Build.ps1
```

Test images come from `casso disk create`, as in spec 033's MAME table:

```powershell
x64\Debug\CassoCli.exe disk create --size 800K --bootable b800.po
x64\Debug\CassoCli.exe disk create --size 32M  --bootable b32.hdv
```

## Unit and scenario tests

```powershell
scripts/RunTests.ps1 -Build -Filter BlockDevice
scripts/RunTests.ps1 -Build -Filter Floppy35
scripts/RunTests.ps1 -Build -Scenario
```

Result: every boot test ends at the ProDOS BASIC prompt or the Users Disk
menu, matching the MAME screenshots in spec 033's `validation.md`.

## Manual checks (enhanced //e, desk scene theme)

| Story | Setup | Result |
|---|---|---|
| US1 | SCSI card in slot 7, `b32.hdv` at ID 6 | ProDOS boots; HD20SC on the desk, lamp on access |
| US2 | A2Pico in slot 4 over a Casso Explorer card folder | First drive boots, all drives online |
| US3 | //c with a UniDisk 3.5 on the disk port, `b800.po` | ProDOS boots; eject sound and model empty after eject |
| US4 | 3.5 controller card in slot 5, Apple 3.5 Drive, a WOZ 3.5 image | Boots, protection check passes |
| US5 | //c Plus, `b800.po` in the internal drive | Boots at 4 MHz |
| US6 | ProFile card, 5 MB image | Boots; the ProFile spins up on the desk |

Launch per the workspace rules:

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label, '--hd', '7.6=b32.hdv'
```

## MAME comparison

```powershell
scripts/MameSnapshot.ps1 -Machine apple2ee -Card scsi -HardDisk b32.hdv -Out b32.scsi.png
```

Record each comparison in `specs/048-smartport-drives/validation.md`.
