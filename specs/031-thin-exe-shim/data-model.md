# Phase 1 Data Model: Thin Executable, Testable Core

**Feature**: 031-thin-exe-shim | **Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

This feature moves code rather than adding a domain, so its entities are
structural: the units the work is organized into, and the destination each file
resolves to. The destination mapping is the load-bearing part — it is what makes
a slice a mechanical operation rather than a judgment call repeated 189 times.

## Entities

### Slice

One completable extraction, and the unit of commit and review. Not a release:
FR-006 puts all slices on one branch and merges once.

| Field | Description |
|---|---|
| Story | The user story it implements (0 through 8) |
| Modules | The files it relocates |
| Destination | The library and directory they land in |
| Tests | Test files added in the same slice (FR-009) |
| Measurement | Before/after line counts for `Casso` and the receiver (FR-012) |
| Dual-compile delta | `<ClCompile Include="..\Casso\...">` entries removed from `UnitTest.vcxproj` |
| Evidence | What Principle VI verification recorded (FR-013) |

**Invariant**: at the end of a slice the tree builds in Debug and Release and
the full suite is green. A slice that cannot reach that state splits.

### Seam

An interface that lets a test substitute for an OS service. **Both sides live in
core** — the interface and its platform implementation — because only the test
needs the substitution and a test links core.

Existing instance: `IFileSystem` / `Win32FileSystem` in `Casso/Config/`, which
moves whole in Slice 1. `Cli/Win32DiskFileIo` and `Cli/Win32IntentChannel` are
seam implementations misfiled under `Cli/`; they move to a seam directory.

### Linker target

What an executable project becomes under FR-003.

| Field | Value |
|---|---|
| Contents | Resource script, generated resource header, one comment-only translation unit |
| Function count | Zero, entry point included (SC-002) |
| Link setting | `<EntryPointSymbol>` naming the CRT startup symbol (FR-003a) |
| Symbol, `Casso.exe` | `wWinMainCRTStartup` (`Windows` subsystem, `wWinMain`) |
| Symbol, `CassoCli.exe` | `mainCRTStartup` (`Console` subsystem, narrow `main`) |

### Family and Model

A **Family** is a machine series whose models share code. Not a vendor: an
Apple II and a Macintosh share nothing, so they would be separate families.

A **Model** is one emulated machine. Model directory names match the machine's
definition directory under `Resources/Machines/`, so one set of names spans the
JSON and the code.

| Family | Models |
|---|---|
| `Apple2` | `Apple2`, `Apple2Plus`, `Apple2e`, `Apple2eEnhanced`, `Apple2c` |

Future families (`Vic20`, `C64`, NES) add directories; they do not edit shared
code to admit themselves (SC-002b).

### Golden image

A checked-in expected render, compared pixel-exact with no tolerance (FR-008a).
A pass change that legitimately alters output regenerates the affected goldens
in the same slice, and the regenerated image must appear in that slice's diff so
a reviewer sees the picture change and not only the shader change.

### Dual-compile entry

A `<ClCompile Include="..\Casso\...">` line in `UnitTest.vcxproj`, compiling an
exe source a second time into the test DLL. There are 38 at branch point. Each
is deleted by the slice that relocates its file, because linking replaces it.
The count reaching zero is a progress metric alongside FR-012's line counts.

## Destination mapping

The receiving library is `CassoEmuCore` for everything leaving `Casso/`
(FR-005b). `CassoCore` receives nothing and keeps its one-way boundary.

### Source areas out of `Casso/`

| Source | Lines | Destination | Slice |
|---|---:|---|---|
| `Casso/Config/` | 6,431 | `CassoEmuCore/Config/` | 1 |
| Strays at `Casso/` root | — | see below | 2 |
| `EmulatorShell` pure math | — | `CassoEmuCore/Shell/Layout/`, `.../Pacing/`, `.../Input/` | 3 |
| `Casso/Shell/` | 9,125 | `CassoEmuCore/Shell/` | 4 |
| `Casso/Ui/Settings/` | 12,666 | `CassoEmuCore/Ui/Settings/` | 5 |
| `Casso/Ui/Chrome/` | 7,221 | `CassoEmuCore/Ui/Chrome/` | 5 |
| `Casso/Ui/Scene/` | 9,119 | `CassoEmuCore/Ui/Scene/` | 5 |
| `Casso/Ui/Dialogs/` | 3,443 | `CassoEmuCore/Ui/Dialogs/` | 5 |
| `Casso/Ui/` root | 14,857 | `CassoEmuCore/Ui/` | 5 |
| `EmulatorShell` remainder | measured at slice 3's close | `CassoEmuCore/Shell/` | 6 |
| `Casso/Shaders/` | — | `CassoEmuCore/Shaders/` + `Shaders.targets` import | 7 |
| `CrtPostProcess`, render, mixing | — | `CassoEmuCore/Render/` | 7 |
| `Casso/Print/` | 482 | `CassoEmuCore/Print/` | 4 |
| `Casso/Main.cpp` (`wWinMain`) | 836 | `CassoEmuCore/Gui/` | 8 |

Story 2 strays, all at `Casso/` root unless noted: `TrackSectorPredicate`,
`DebugDialogProjection`, `Disk2DebugDialogState`, `InputDebugDialogState`,
`DiskSettings`, `InputEventDisplay`, `Disk2EventDisplay`, `PerfStats`, and the
resolution/catalog half of `AssetBootstrap`. Destinations follow the concern:
disk projections to `CassoEmuCore/Ui/Debug/`, `PerfStats` to
`CassoEmuCore/Core/`, `AssetBootstrap`'s catalog half to `CassoEmuCore/Config/`.

### Machine hierarchy, User Story 0

```
CassoEmuCore/
├── Machines/
│   └── Apple2/
│       ├── Common/          AppleKeyboard, AppleMouse, AppleSpeaker,
│       │                    AppleGamePort, AppleSoftSwitchBank,
│       │                    LanguageCard, CxxxRomRouter, Disk2*,
│       │                    Apple video modes, CharacterRom*
│       ├── Apple2/
│       ├── Apple2Plus/
│       ├── Apple2e/         Apple2eMmu, Apple2eKeyboard,
│       │                    Apple2eSoftSwitchBank
│       ├── Apple2eEnhanced/
│       └── Apple2c/         Apple2cRomBank
├── Devices/                 RamDevice, RomDevice, Acia6551, AciaEndpoints,
│                            IMmu, IRomBankSwitch, ISoftSwitchBank,
│                            IVideoMode, IInputEventSink, InputEvent,
│                            InputEventRing
├── Video/                   VideoTiming, IVideoTiming, PixelFormat,
│                            MonochromeTint, NtscColorTable
└── Core/                    unchanged by this slice
```

`Devices/Disk/` (72 files), `Devices/Printer/` (47) and
`Devices/Mockingboard/` (12) are classified per file during implementation
against the FR-005c rule, and the classification is recorded in the sweep's
commit message.

## State transitions

The `Casso` project moves through exactly one sequence, and each step is
checkable:

```
93,927 lines, 86 ClCompile, 38 dual-compiled, no EntryPointSymbol
   -> [Slice 0]  same line count, machine hierarchy in place
   -> [Slices 1-7]  line count falls, dual-compile count falls toward zero
   -> [Slice 8]  0 lines, 1 ClCompile (comment-only), 0 dual-compiled,
                 EntryPointSymbol set, ProjectReference from UnitTest removed
```

## Validation rules

| Rule | Source | How it is checked |
|---|---|---|
| Exe defines zero functions | FR-003, SC-002 | Count functions in the project's translation units |
| Placement never argued from a platform API | FR-002, FR-005 | Review of commit messages, comments and the Constitution Check |
| Moved module has tests in the same slice | FR-009, SC-003 | Test files added in the same commit range |
| Tests touch no real system state | FR-010 | Review against Test Isolation |
| Slice preserves behavior | FR-007, SC-005 | Suite green, plus manual confirmation for UI slices |
| Sweep changes only include paths and guards | FR-005e | Diff inspection: no content change beyond those |
| Machine code sits under its machine | FR-005c, SC-002b | Directory inspection |
| `CassoCore` has no upward include | FR-005b | Grep for includes reaching into `CassoEmuCore`, `Dxui`, an exe |
| Goldens compared pixel-exact | FR-008a | Test assertion, no tolerance parameter |
| Measurement recorded per slice | FR-012, SC-007 | Commit message carries before/after counts |
