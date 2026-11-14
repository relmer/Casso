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
