# Phase 0 Research: Thin Executable, Testable Core

**Feature**: 031-thin-exe-shim | **Date**: 2026-09-09 | **Spec**: [spec.md](spec.md)

Every measurement here was taken on this branch. Nothing in this document is
estimated, and where a figure disagrees with the specification the disagreement
is called out rather than smoothed over.

## R1: The `TCDir` mechanism, verified rather than cited

**Decision**: Reproduce `TCDir`'s arrangement exactly, substituting the windowed
CRT startup symbol.

**Findings**: `C:\Users\relmer\source\repos\relmer\TCDir` holds a working
instance of the target shape.

- `TCDir/Main.cpp` is five lines, all comment, and reads:

  ```
  // This project only produces the .exe from the TCDirCore.lib
  //
  // There is no code here
  // Cheer up, everything is fine
  // Seek TCDirCore
  ```

- The project compiles exactly three items: `Main.cpp`, `resource.h`, `TCDir.rc`.
- Every configuration sets `<EntryPointSymbol>wmainCRTStartup</EntryPointSymbol>`
  alongside `<SubSystem>Console</SubSystem>`.
- `wmain` is defined in `TCDirCore/TCDir.cpp:226`.

**Rationale**: The linker discards a static library member nothing references.
Naming the CRT startup symbol gives it an undefined symbol to resolve; startup
references the entry point; the entry point drags the rest of the program out.
This is FR-003a, confirmed against a program that runs.

**Application to Casso**: `Casso.vcxproj` sets `<SubSystem>Windows</SubSystem>`
in all six configurations and sets **no** `EntryPointSymbol`. `Casso/Main.cpp`
defines `wWinMain` at line 608 of 836. The windowed equivalent of
`wmainCRTStartup` is **`wWinMainCRTStartup`**, which a `Windows` subsystem
project uses by default when it defines `wWinMain`; stating it explicitly is
what forces the pull. `CassoCli.exe` uses narrow `main`, so its symbol is
`mainCRTStartup`.

**Alternatives considered**: `/INCLUDE:` on a symbol in the entry point's
translation unit, and `/WHOLEARCHIVE` on the core library. Both work. Both were
rejected because `TCDir` settles the question, because `/WHOLEARCHIVE` defeats
the dead-code elimination that keeps `CassoCli.exe` from carrying the desk
scene, and because a second mechanism in the same tree is something a reader
has to learn for no benefit.

## R2: The exe is not untested — it is dual-compiled, which is worse

**Finding that contradicts the specification.** The Context table in `spec.md`
records `Casso` (exe) as having **no** unit tests. That is not what the build
does. `UnitTest.vcxproj` carries **38** `<ClCompile Include="..\Casso\...">`
entries, compiling exe sources a second time into `UnitTest.dll`, and
`UnitTest/Casso/` holds seven test files that drive them. Its
`AdditionalIncludeDirectories` lists `..\Casso` and `..\Casso\Ui\Chrome`
explicitly, and `UnitTest.vcxproj` holds a `ProjectReference` to
`Casso.vcxproj` — a reference that yields build ordering but cannot link an
`Application`, which is the mechanical reason the workaround exists at all.

The dual-compiled set spans precisely the areas the specification's slices
target: `Config/` (`UserConfigStore`, `GlobalUserPrefs`, `WindowPlacementProfile`,
`MachineInputPrefs`, `CrtResolver`), `Shell/` (`DiskMru`, `ClipboardManager`,
`CpuManager`), `Ui/Scene/` (`DeskSceneLayout`, `DeskSceneModel`,
`DeskSceneHitTester`, `FullscreenStripState`), `Ui/Settings/` (`DisplayPage`,
`HardwarePage`, `SettingsPanelState`, `PrintingPage`, `ScreenshotsPage`),
`Ui/Chrome/` (`MainMenu`, `DriveWidget`, `LedIndicator`, `CassoBranding`,
`InputDeviceGlyphs`, `Apple2cSwitchBar`, `DriveLabelTruncation`), the Story 2
strays, and `CrtPostProcess.cpp`.

**Why this matters more than a corrected line in a table**: dual compilation is
the workaround that made the missing library boundary survivable, and it has
three costs the extraction removes. The sources compile twice, under two
different precompiled headers. The test binary and the shipping binary contain
separate copies of the same code, so they can diverge under differing
preprocessor state. And the arrangement silently caps coverage at whatever
compiles cleanly in both, which is why the covered set stops where it does.

**Decision**: Every slice that moves a file out of `Casso/` MUST delete that
file's `<ClCompile Include="..\Casso\...">` entry from `UnitTest.vcxproj` in the
same commit, because the file is thereafter reached by linking the library. The
count of such entries is a second progress metric alongside FR-012's line
counts. It MUST reach zero, along with `..\Casso` disappearing from
`AdditionalIncludeDirectories` and the `ProjectReference` to `Casso.vcxproj`
being removed.

**Consequence for sequencing**: those 38 files are already proven UT-reachable.
They are the cheapest and safest moves in the extraction, and they confirm the
specification's slice ordering was chosen correctly — from the opposite
direction to the one the specification argued.

**Watch item**: the dual-compiled sources currently build against
`..\Casso\CassoPch.cpp`. Moving a file changes which precompiled header it sees.
This is the likeliest source of per-slice build breakage and is called out in
`quickstart.md`.

## R3: The library boundary is clean and stays split

**Decision**: Retain `CassoCore` and `CassoEmuCore` as separate libraries
(FR-005b). Extracted code from `Casso/` lands in `CassoEmuCore`.

**Findings**:

- Reference graph: `CassoCore` <- `CassoEmuCore` <- {`Casso.exe`, `CassoCli.exe`};
  `Dxui` is independent; `UnitTest` references all four.
- `CassoCore`'s 78 files contain **zero** includes reaching into `CassoEmuCore`,
  `Dxui` or an executable. The boundary is currently uncrossed.
- `CommandLineOptions.h:332` stores `--machine` as a plain `std::string`, not a
  machine type, which is how one parser serves both executables without
  depending on either.
- `CassoEmuCore/Cli/` (27 files, 5,276 lines) already holds an executable's
  *entire* program: `CliMain`, `CommandLine`, the mode runners, artifact output.
  `CassoCli/CassoCli.cpp` is 23 lines whose `main` calls `CliMain`.
- `Casso.exe` already references `CassoCore` directly, so the assembler is
  linked into the emulator today. A future immediate-mode debugger assembler
  requires no relocation, only a call.

**Rationale**: the GUI's program becomes a sibling of `Cli/` inside
`CassoEmuCore` — a shape the tree already runs, not a new one. Merging the two
libraries would preserve every existing dependency while removing the only
architectural boundary in the tree that the build can enforce.

**Alternatives considered**: a third `CassoShell` library between `CassoEmuCore`
and the exe, rejected because `Cli/` demonstrates a subdirectory suffices; and a
single uber-library, rejected on the grounds above. Neither binary size nor
build parallelism distinguishes the options: static libraries discard
unreferenced members under `/OPT:REF`, and MSBuild compiles translation units in
parallel within a project.

**Misfiled today**: `Cli/Win32DiskFileIo` and `Cli/Win32IntentChannel` (927
lines) are included by `Casso/` as well as the CLI. They are Win32 seam
implementations, not CLI code, and the sweep moves them out of `Cli/`.

## R4: The machine hierarchy — what the sweep actually faces

**Decision**: `CassoEmuCore/Machines/<Family>/<Model>/` with a per-family
`Common/`; chips and machine-neutral devices remain in `CassoEmuCore/Devices/`
(FR-005c).

**Finding that resizes the job**: `Devices/` is 177 files, but only about 28 sit
at its root. The rest are already in subdirectories: `Disk/` (72 files),
`Printer/` (47), `Mockingboard/` (12).

Root-level classification, by inspection:

| Tier | Files |
|---|---|
| Model-specific | `Apple2eMmu`, `Apple2eKeyboard`, `Apple2eSoftSwitchBank`, `Apple2cRomBank` |
| Apple II family | `AppleKeyboard`, `AppleMouse`, `AppleSpeaker`, `AppleGamePort`, `AppleSoftSwitchBank`, `LanguageCard`, `CxxxRomRouter`, the `Disk2*` set |
| Machine-neutral or chip | `Acia6551`, `AciaEndpoints`, `IAciaEndpoint`, `RamDevice`, `RomDevice`, `IMmu`, `IRomBankSwitch`, `ISoftSwitchBank`, `IVideoMode`, `IInputEventSink`, `InputEvent`, `InputEventRing` |

`CassoEmuCore/Video/` is the least machine-neutral area in the tree: 15 of its
20 files carry Apple II assumptions. `AppleTextMode`, `AppleHiResMode`,
`AppleLoResMode`, `AppleDoubleHiResMode`, `Apple80ColTextMode` and
`CharacterRom*` are family code; `VideoTiming`, `IVideoTiming`, `PixelFormat`,
`MonochromeTint` and `NtscColorTable` are neutral.

`CassoEmuCore/Core/` is already the machine-generic framework it needs to be:
`ComponentRegistry`, `CpuFactory`, `MachineConfig`, `MachineConfigUpgrade`,
`MachineScanner`, `MemoryBus`, `MemoryDevice`, `InterruptController`,
`ICycleSink`, `IHostShell`. Ten of its 38 files mention Apple, which is residue
a later specification addresses; FR-005e keeps User Story 0 from touching it.

**Naming**: model directories take the names already used under
`Resources/Machines/`: `Apple2`, `Apple2Plus`, `Apple2e`, `Apple2eEnhanced`,
`Apple2c`. The family is `Apple2` — a machine series, not a vendor, so a Lisa or
a Macintosh would be separate families. `Machines/Apple2/Apple2/` nests a model
inside a family of the same name, which is mildly awkward and is accepted so
that every model directory matches its definition directory.

**Open, queued rather than blocking**: `Disk/` at 72 files needs per-file
classification the plan does not pre-judge — the Disk II controller and mark
decoding are Apple II family, while a WOZ or 2MG container parser arguably is
not. `Printer/` at 47 files is largely emulation of printers that are not Apple
parts. Both are classified during implementation against the rule in FR-005c,
and the classification is recorded in the sweep's commit message.

**Alternatives considered**: vendor as the family level, rejected by the owner
because an Apple II and a Macintosh share nothing; and applying the hierarchy
only to files the extraction touches, rejected because a convention applied to
half a directory teaches a reader no rule (FR-005d).

## R5: Story 7 needs a test harness that does not exist yet

**Decision**: Build a WARP-backed device and a readback path in `UnitTest`
before extracting the render chain; assert against checked-in golden images,
pixel-exact (FR-008a).

**Findings**: `Dxui` creates its device in `Dxui/Window/DxuiHwndSource.cpp:1594`
and `:1611`, both with `D3D_DRIVER_TYPE_HARDWARE`, and both bound to a window
source. There is no software-adapter path and no readback path anywhere in the
tree today. `Casso/CrtPostProcess.cpp` is already dual-compiled into `UnitTest`,
so part of the chain is reachable, but nothing renders in a test.

Shaders build through `Casso/Shaders/Shaders.targets`, imported at
`Casso.vcxproj:473`, with `ShaderResourceIds.h` and HLSL under
`Shaders/{Blit,CRT,Settings}`. Moving the directory moves the import; the
targets file is the item most likely to need adjustment, since its paths are
relative to the importing project.

**Rationale**: `D3D_DRIVER_TYPE_WARP` gives a deterministic rasterizer with no
display and no window, which satisfies Test Isolation. Determinism holds on a
given machine and Windows revision, not across revisions — recorded in the
specification's Assumptions as an accepted cost of the owner's choice of
pixel-exact goldens over tolerances.

**Alternatives considered**: tolerance comparison and computed structural
expectations, both rejected by the owner as too weak an assertion for the code
the slice exists to cover.

## R6: What stays in each executable

**Decision**: after User Story 8, `Casso/` holds `Casso.rc`, `resource.h`, and a
comment-only `Main.cpp`; `CassoCli/` takes the same three shapes.
`MeshCreator` is untouched, being a build-time tool — `Casso.vcxproj:467` builds
it and `:470` runs it to bake each `.mesh`/`.mtl` pair into the `.dmesh` the app
loads.

The `Casso` project currently lists 86 `ClCompile` and 101 `ClInclude` entries
plus `Casso.rc` and the `Shaders.targets` import. All but the resource script
and one comment-only translation unit are removed by the final slice. The
haiku's closing line becomes "Seek CassoEmuCore", which keeps the five
syllables FR-003b requires, since that is where the entry point lands.
