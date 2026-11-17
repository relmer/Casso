# Implementation Plan: Apple /// Machine

**Branch**: `050-Apple-III` | **Date**: 2026-11-17 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/050-apple3/spec.md`

## Summary

Add the Apple /// and the Apple /// Plus as machines. The memory mapping,
the three registers and extended addressing go in a new `Apple3Mmu`; the
existing 6522, 6551 and Disk II models are reused behind Apple /// wiring;
video, keyboard, sound, the clock chip and the analog inputs are new devices.
The debugger gets a bank-qualified address type that replaces the 16-bit
address in its tables, commands and panes, panels for each new device, and
Business BASIC source-level debugging. The skeuomorphic theme gets measured
models of the body, the Monitor /// and the Disk ///, and a stacked desk
arrangement. Decisions and their reasons are in [research.md](research.md).

## Technical Context

**Language/Version**: C++ (MSVC v145, Visual Studio 2026); Python with
CadQuery for the model generators; PowerShell for scripts

**Primary Dependencies**: none new. Direct3D 11 and Dxui (existing);
`Via6522`, `Acia6551`, `Disk2NibbleEngine`, `MemoryBus`, `InputJournal` and
035's debugger core (existing)

**Storage**: machine definition JSON, user prefs JSON, debugger project
files, saved machine state for history; each gets new fields and a version
step

**Testing**: Microsoft Native CppUnitTest (`UnitTest`), the scenario suite
for SOS boots, Dormann and full-depth Harte for the CPU change,
`scripts/CompareMameFrame.ps1` for video

**Target Platform**: Windows x64 and ARM64

**Project Type**: desktop application with a testable core
(`CassoCore`, `CassoEmuCore`) and thin executables

**Performance Goals**: the Apple /// runs at real speed at 2 MHz with the
debugger's history on; Apple II emulation speed does not change (SC-011)

**Constraints**: written from Apple's documents, with existing emulators consulted only for
facts the documents lack and no code copied (research R1); no
Apple software committed; every new UI string approved by the owner; merges
from `035-debugger` only

**Scale/Scope**: two machines, about a dozen new devices and video modes, a
debugger-wide address migration, twelve device panels, a second BASIC
dialect in the debugger, three new 3D models and a new desk arrangement

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Code Quality | Pass | New code follows `.github/copilot-instructions.md`: EHM, formatting, verb-first function names, no anonymous namespaces |
| II. Testing Discipline | Pass | Each device is unit-tested with synthetic inputs; SOS boots are scenario tests; a test that cannot find its disk or ROM fails |
| III. User Experience Consistency | Pass | The Apple /// uses the existing machine list, settings pages, drive widgets and debugger panes; new strings are in `contracts/ui-strings.md` for approval |
| IV. Performance Requirements | Pass | The Apple II CPU cores get no new member and no new test; the new addressing mode is a case only `Cpu6502Apple3` executes (research R4) |
| V. Simplicity & Maintainability | Pass, see Complexity Tracking | The debugger address migration is large and is justified below |
| VI. Thin Executable, Testable Core | Pass | Everything is in `CassoEmuCore` and `CassoCore`; `Casso` gets resource lines for the new models and machine files only |

**Post-design re-check**: Pass. The design does not add a project, a dependency
or logic to an executable.

## Project Structure

### Documentation (this feature)

```text
specs/050-apple3/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── hardware-map.md
│   ├── machine-config.md
│   ├── debugger-addresses.md
│   ├── device-panels.md
│   ├── desk-models.md
│   └── ui-strings.md
└── tasks.md                 (from /speckit-tasks)
```

### Source Code (repository root)

```text
CassoCore/
├── GlobalAddressingModes.h, Cpu.cpp    one new addressing mode and its fetch case
└── Disassembler.h, .cpp                machine helper: resolved access, inline calls

CassoEmuCore/
├── Machines/Apple3/                    new
│   ├── Apple3.h, Apple3Plus.h          IMachine
│   ├── Apple3Mmu                       banks, registers, extended addressing, lockout
│   ├── Cpu6502Apple3                   the NMOS 6502 with the extended indirect mode
│   ├── Apple3SystemVias                two Via6522 and their wiring
│   ├── Apple3Keyboard, Apple3Sound, Apple3AnalogInputs, Apple3Slots
│   ├── Apple3DiskController            four drives over the Disk II engine
│   ├── Apple3CharacterSet, Apple3Video
│   ├── Apple3TextMode, Apple3ColorTextMode, Apple380ColTextMode
│   ├── Apple3HiResMode, Apple3ColorHiResMode, Apple3Color140Mode, Apple3HiRes560Mode
│   └── SosCallNames, Apple3Symbols
├── Devices/Mm58167                     new clock chip model
├── Machines/MachineDefinitions.cpp     the two machines
├── Machines/IMachine.h                 new questions with Apple ][ defaults
├── Shell/MachineBuilder.cpp            Apple /// build path (factored into a
│                                       separate builder class; the file is over 2k lines)
├── Config/MonitorCatalog.h             Apple Monitor ///
├── Debugger/
│   ├── DebugAddress                    new value type
│   ├── IDebugTarget.h, MachineDebugTarget, DebugMemoryView    bank forms
│   ├── BreakpointTable, WatchTable, SymbolTable, DataBlockTable, HeatBankMap,
│   │   CallStack, Reply.h (TraceRecord), MemoryMap             DebugAddress
│   ├── formatters of the four command modes                    parse and print
│   └── Basic/BusinessBasicProgram      new, beside ApplesoftProgram
├── Ui/Debugger/Panes/                  bank column, registers, new visuals
│   ├── CharacterSetView, DacMeterView  new
│   └── MemoryMapBar, DiagnosticsPane, MemoryPane, DisassemblyViews, ...
├── Ui/Scene/                           DeskDeviceKind, stacked layout
└── Ui/Settings/                        Apple /// machine page options

Resources/
├── Machines/Apple3/, Machines/Apple3Plus/
└── Models/Apple3/, Models/Monitor3/, Models/Disk3/

scripts/
├── modelgen/cad_apple3.py, cad_monitor3.py, cad_disk3.py
├── CompareMameFrame.ps1
├── FetchStockDisks.ps1                 SOS disks
└── FetchRoms.ps1                       Apple3.rom

UnitTest/Apple3Tests/                   new
ScenarioTests/Apple3SosTests.cpp        new
```

**Structure Decision**: the existing projects, with one new directory per
concern. Nothing is added to `Casso/` beyond the `BakedMesh` entries and
`RCDATA` lines the model README requires.

## Implementation order

The tasks follow research R24. Each stage ends in something that can be
checked.

1. **Documents**: collect them; fill the **Checked** column of
   `contracts/hardware-map.md` and the dimension tables of
   `contracts/desk-models.md`. Rows that the documents contradict are
   corrected, and the spec is updated where a requirement depended on them.
2. **Memory and CPU**: `Apple3Mmu`, `Cpu6502Apple3`, Dormann,
   Harte at full depth and the per-instruction timing comparison.
3. **To the diagnostics screen**: the VIAs, the character set, the text
   modes, the keyboard, the machine definition and the builder. Check: the
   ROM's no-disk screen.
4. **To a SOS boot**: the disk controller and the time base. Check: Business
   BASIC prompt, format and copy in the scenario suite.
5. **Debugger addresses** (beside stages 2 to 4): `DebugAddress`, the
   target, the tables, the four modes' parsers and printers, the panes.
   Check after each step: the Apple II debugger tests pass with no edits to
   their expectations.
6. **The rest of the machine**: graphics modes, sound, the clock chip, the
   serial port, joysticks, settings pages, the command line and the feature
   audit of FR-035. Check: Confidence Test disk; MAME frame comparison.
7. **Debugger for the Apple ///**: register display and breakpoints,
   extension byte annotation, SOS calls, machine symbols, the panels and
   visuals, the logs. History for every new device.
8. **Apple II emulation mode**, then the **Apple /// Plus**.
9. **Business BASIC debugging**.
10. **Models and the desk arrangement** (independent; can start after
    stage 1).
11. **ProFile**, after a merge from `035-debugger` that includes 048.
12. **Merge gate**, `CHANGELOG.md` and `README.md`, with the owner's
    approval of the user-facing text.

## Risks

| Risk | Handling |
|---|---|
| The hardware map is from secondary sources and memory | Stage 1 gates every device on a checked row |
| Model dimensions are missing from Apple's documents | Measured from photographs and listed as measured in each README; SC-010 then applies to the documented dimensions only, and the spec is updated to match if it happens |
| The debugger address migration touches several hundred sites | Implicit conversion from `Word`, staged, with the Apple II tests as the check at each step |
| Renames on master arrive in this branch through 035 | Merge from `035-debugger` early and often, per `CLAUDE.md` |
| Business BASIC internals are undocumented | Worked out by observing the interpreter in Casso; each release identified by signature; unknown releases reported |
| MAME's Apple /// may itself be wrong for a mode | A difference is checked against the documents before either side is changed |
| 048 is merged into the branch late | The ProFile is the last stage, and the default slot is empty until then |

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| A new address type across the debugger | A 16-bit address cannot identify a location on a banked machine (FR-036 to FR-047) | With a bank field added only to breakpoints, the trace, heat map, symbols and panes report the wrong bank's bytes |
| A CPU variant class, `Cpu6502Apple3` | Extended addressing changes where an indirect access goes, and with an instruction-level core the bus cannot do that alone | A wholly separate CPU would duplicate the 6502 and its validation; a hook in the shared core would add a member and a test to every machine |
| A tick-based time base for one machine | The CPU changes speed inside a frame and the devices do not | With the clock speed setting changed on a switch, devices count the wrong elapsed time |
