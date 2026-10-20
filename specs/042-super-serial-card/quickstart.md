# Quickstart: validating the Super Serial Card

## Prerequisites

- A Debug or Release x64 build: `scripts\Build.ps1`.
- Fixture ROMs: `scripts\FetchRoms.ps1 -Fixtures` in a fresh worktree.
- For the firmware scenarios only: Apple's SSC firmware at `%LOCALAPPDATA%\Casso\Devices\SuperSerialCard\SSC.rom`, fetched through Casso's Download button. The unit tests never read it; they use a synthetic 2 KB image.

## Automated

```powershell
scripts\RunTests.ps1 -Build -Filter Acia
scripts\RunTests.ps1 -Build -Filter SuperSerial
scripts\RunTests.ps1 -Build -Filter ExpansionRom
scripts\RunTests.ps1 -Build
```

The filtered runs are the edit loop; the last is the suite.

What they cover:

| Area | Checks |
|---|---|
| 6551 timing | transmit of 1,000 bytes at every baud code within 1% of the computed time (SC-004); TDRE double buffering; CTS false stopping the transmitter |
| 6551 receive | queue delivery one character time apart; overrun on the first byte not read in time (SC-003); DTR off pausing the queue; IRQ and its clear |
| Switch registers | each switch at its R-001 bit, OFF reading 1; undriven bits floating; SW2-6 gating the IRQ |
| Firmware mapping | `$Cs00` from ROM `$700`; `$C800`-`$CEFF` from ROM `$000` while selected; `$CFxx` deselecting; RESET deselecting; no firmware reading as the floating bus |
| //e rules | INTCXROM hiding the page and expansion; INTC8ROM hiding the expansion and a `$CFFF` access under it not deselecting the card; slot 3 and SLOTC3ROM |
| ][+ | the page and expansion as bus devices beside a Disk II slot ROM |
| Config | the `firmware`, `switches` and `ports.path` keys; missing firmware not failing the load; the slot-2 migration |
| //c | its ports' existing tests, updated for paced transmit (SC-006) |

## Manual checks

Launch with `--title` per the workspace rules.

1. Settings > Hardware on a //e: turn on "Super Serial Card" in slot 2. With no firmware, the download offer appears; accept it. Confirm the reset.
2. Settings > Serial card: set printer mode, 9,600 baud, attachment "File (receive what the card sends)" with a path in your temp folder.
3. In Applesoft, type a short program, then `PR#2`, `LIST`, `PR#0`. The file contains the listing (SC-001).
4. Switch the attachment to the loopback plug and the mode to communications. `IN#2`, then type `PRINT 6*7`: the line comes back through the plug and prints 42. Not `PR#2` as well: with both, everything printed comes back in as input.
5. Delete or rename `SSC.rom`, restart: the infobar reports the missing firmware with a Download button, and the machine runs. `PR#2` now hangs, as it would with an empty ROM socket; Ctrl-Reset recovers.
6. A //c: Settings offers no Super Serial Card. `PR#1` sends to the printer port one character time apart at the baud rate the //c firmware sets. Both //c ports have a loopback plug attached and no printer, so nothing prints.
