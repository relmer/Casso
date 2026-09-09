# Contract: Machine Directory Layout

**Feature**: 031-thin-exe-shim | **Requirements**: FR-005c, FR-005d, FR-005e, SC-002b

This contract governs where machine code lives in `CassoEmuCore`. It is applied
to the whole tree in User Story 0, before any code leaves `Casso/`, so that
later slices land files in their final home rather than moving them twice.

## The rule

Every file in `CassoEmuCore` answers one question: **which machines does this
code assume?**

| Answer | Home |
|---|---|
| One model | `Machines/<Family>/<Model>/` |
| Several models of one family | `Machines/<Family>/Common/` |
| No machine at all | `Devices/`, `Video/`, `Audio/`, `Core/` as today |

A **family** is a machine series whose models share code. It is not a vendor.
An Apple II and a Macintosh come from one maker and share nothing, so they are
separate families.

A **chip** belongs to no machine. A 6551 appears in the //c but a 6551 is not an
Apple part, and a 6522 or a SID would sit in the same position. Chips live in
`Devices/` beside `RamDevice` and `RomDevice`. This is what keeps the first
cross-vendor chip from forcing the hierarchy open again.

## Names

Model directories use the names the machine definitions already use under
`Resources/Machines/`, so a reader moving between the JSON and the code sees one
set of names.

```
CassoEmuCore/Machines/Apple2/
├── Common/
├── Apple2/
├── Apple2Plus/
├── Apple2e/
├── Apple2eEnhanced/
└── Apple2c/
```

`Machines/Apple2/Apple2/` nests a model inside a family of the same name. It is
mildly awkward and it is accepted, because the alternative renames a model
directory away from its definition directory and breaks the single set of names
this rule exists to produce.

## Placement of the current tree

| File or group | Home |
|---|---|
| `Apple2eMmu`, `Apple2eKeyboard`, `Apple2eSoftSwitchBank` | `Machines/Apple2/Apple2e/` |
| `Apple2cRomBank` | `Machines/Apple2/Apple2c/` |
| `AppleKeyboard`, `AppleMouse`, `AppleSpeaker`, `AppleGamePort`, `AppleSoftSwitchBank` | `Machines/Apple2/Common/` |
| `LanguageCard`, `CxxxRomRouter` | `Machines/Apple2/Common/` |
| `Disk2Controller`, `Disk2AddressMarkWatcher`, `Disk2Event`, `Disk2EventRing`, `IDisk2EventSink` | `Machines/Apple2/Common/` |
| `AppleTextMode`, `Apple80ColTextMode`, `AppleLoResMode`, `AppleHiResMode`, `AppleDoubleHiResMode`, `CharacterRom*` | `Machines/Apple2/Common/` |
| `RamDevice`, `RomDevice`, `Acia6551`, `AciaEndpoints`, `IAciaEndpoint`, `IMmu`, `IRomBankSwitch`, `ISoftSwitchBank`, `IVideoMode`, `IInputEventSink`, `InputEvent`, `InputEventRing` | `Devices/` |
| `VideoTiming`, `IVideoTiming`, `PixelFormat`, `MonochromeTint`, `NtscColorTable` | `Video/` |
| `Core/` | unchanged |

`Devices/Disk/` (72 files), `Devices/Printer/` (47) and
`Devices/Mockingboard/` (12) are classified per file during implementation
against the rule above. The Disk II controller and its mark decoding are Apple
II family code; a WOZ or 2MG container parser is a file format and belongs to no
machine. The resulting classification is recorded in the sweep's commit message
so a later reader can see the reasoning rather than infer it.

## What the sweep may change

Nothing but placement, include paths and header guards (FR-005e). It adds no
behavior, so it asserts none: its acceptance is a green build and a green suite
in Debug and Release.

Where a file mixes machine-generic and machine-specific concerns, splitting it
would change behavior and belongs to a later specification. The sweep places it
with the machine it currently assumes and leaves the mixture intact.

## Verification

| Check | Passes when |
|---|---|
| Placement | No file under `Devices/`, `Video/` or `Audio/` assumes a particular machine |
| Family level | `Common/` holds only code shared by two or more models of that family |
| Chips | No chip sits under `Machines/` |
| Names | Every model directory matches a directory under `Resources/Machines/`. The reverse does not hold and is not required: a model whose behavior is entirely its definition plus family code has no directory, and three of the five Apple II models are in that position |
| Mechanical | Diff shows no content change beyond include paths and header guards |
| Green | Debug and Release build, full suite passes |
| Future cost | Adding a machine means adding a directory and a definition, not editing shared code |
