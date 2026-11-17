# Quickstart: validating the Apple /// machine

How to check that the feature works end to end. Details of addresses,
panels and file formats are in `contracts/`.

## Prerequisites

- `Apple3.rom` installed where Casso looks for ROMs. For the test fixtures:

  ```powershell
  scripts\FetchRoms.ps1 -Fixtures
  ```

- The SOS disks, fetched by the scenario run or once manually:

  ```powershell
  scripts\FetchStockDisks.ps1
  ```

- For frame comparison only: MAME with its `apple3` ROM set, per
  the memory note "MAME Apple /// and SOS boot proofs".

## Unit tests

```powershell
scripts\RunTests.ps1 -Build -Filter Apple3
```

Covers the memory mapping for every environment register value, extended
addressing, zero page and stack relocation, the VIA wiring, drive selection,
each video mode's decoding, character set loading, keyboard codes, the clock
chip, `DebugAddress` parsing and printing, bank-qualified breakpoints, and
the desk fit rules.

## CPU validation

```powershell
scripts\RunDormannTest.ps1
```

```powershell
scripts\RunHarteTests.ps1 -SkipGenerate
```

Harte runs at full depth (rename `HarteTests.off` back first). Both pass,
and the runner prints the depth it ran.

## Scenario suite

```powershell
scripts\RunTests.ps1 -Build -Scenario
```

Results for the Apple /// cases:

1. Business BASIC boots to its prompt on 128K and on 256K; a typed program
   prints `42`.
2. System Utilities formats a blank image in Drive 2 and copies a file to
   it; the file reads back from the image byte for byte.
3. The Confidence Test disk passes every test.
4. The Apple II Emulation disk starts emulation, and the DOS 3.3 System
   Master then boots.
5. The Apple /// Plus boots Business BASIC.

A missing disk or ROM stops the run with the file it could not find.

## Frame comparison with MAME

Ask the owner before anything is launched on screen; this script runs both
programs headless.

```powershell
scripts\CompareMameFrame.ps1 -Disk <image> -Seconds 20
```

Run once per video mode with a disk that draws in that mode. Result: zero
differing pixels by color index, or a list of the differing pixels.

## In the app

Launch with a title, in the background unless the owner asked for the run:

```powershell
$label = Split-Path -Leaf (git rev-parse --show-toplevel)
Start-Process .\x64\Debug\Casso.exe -WindowStyle Minimized -ArgumentList '--title', $label, '--machine', 'Apple3'
```

Check:

1. The machine list has Apple /// and Apple /// Plus; settings have the
   memory, external drives, slots, serial and joystick options and no Apple
   II-only options.
2. Skeuomorphic theme: the body, the ProFile and the Monitor /// are
   stacked; moving the ProFile to the side puts the monitor on the body;
   external drives are beside the stack; the built-in drive's door, lamp,
   drop and eject work.
3. The View menu's color, green, amber and white styles apply to the
   Monitor ///.
4. Debugger, stopped in Business BASIC:
   - the registers pane lists E, B and Z with bit tips;
   - a memory window on `2:3000` lists bank 2 whatever bank is switched in;
   - a breakpoint on a bank-qualified address stops only in that bank;
   - an indirect instruction through an enabled extension byte has its
     resolved address on the line;
   - a SOS call is one line with its SOS name, and step over runs it;
   - each panel in `contracts/device-panels.md` is listed and updates;
   - the BASIC view lists a typed program, and a line breakpoint stops.
5. History: seek back through the boot; the screen, the registers and the
   panels match the live run at that point.

## Merge gate

```powershell
scripts\RunMergeGate.ps1
```

Run once, right before the merge to master, after the last merge from
`035-debugger`.
