# Contract: firmware fetch

New rows in `scripts/FetchRoms.ps1`'s catalog, in the form of the existing
rows (`Url`, `Dest`, `Subdir`, `Size`, `Desc`) plus a `Sha1` column checked
after download. A file that fails its checksum is deleted and the fetch fails.

| Dest | Subdir | Size | Source folder on the mirror |
|---|---|---|---|
| `341-0437-A.rom` | `Devices/AppleScsiCard` | 16384 | Interface Cards/SCSI Controllers |
| `341-0438-A.rom` | `Devices/Apple35Card` | 32768 | Interface Cards/Disk Drive Controllers/Apple II 3.5 Disk Controller Card/ROM Images |
| `Liron.rom` | `Devices/LironCard` | 4096 | Interface Cards/Disk Drive Controllers/Apple 3.5 Floppy Disk Drive Interface Card/ROM Images |
| `UniDisk35.rom` | `Devices/UniDisk35` | 8192 | Peripherals/Disk Drives/Apple UniDisk 3.5/ROM Images |
| `341-0271-A.rom` | `Devices/ProFileCard` | 2048 | Interface Cards/Hard Disk Drive Controllers/Apple II ProFile Interface Card/ROM Images |
| `341-0299-B.rom` | `Devices/ProFileCard` | 2048 | same |
| `Apple2cPlus.rom` | `Machines/Apple2cPlus` | 32768 | Computers/Apple II/Apple IIc plus/ROM Images |
| `Apple2cPlus_Video.rom` | `Machines/Apple2cPlus` | 4096 | same |

`-Fixtures` gains the subset the unit and scenario tests use. A2retroNET is
built from source, so it is not fetched.
