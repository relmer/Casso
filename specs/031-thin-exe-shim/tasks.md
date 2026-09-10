# Tasks: Thin Executable, Testable Core — `Casso.exe`

**Feature**: 031-thin-exe-shim | **Plan**: [plan.md](plan.md) | **Spec**: [spec.md](spec.md)

**Tests are required.** FR-009 makes a move without tests incomplete, so every
story phase carries test tasks and they are not optional.

**Delivery**: one long-lived branch. Each phase is committed and pushed on
completion, and more often where the work divides. No phase merges to master
alone; the branch merges once, on the owner's approval (FR-006).

**Per-phase gate** (from [quickstart.md](quickstart.md)), run at every phase
boundary: Debug and Release build, suite green, `CheckStyle.ps1 -Mode Tree`
clean, measurement recorded in the commit message, master merged in. Where a
phase exposed a defect, it also ships a test that fails against the pre-move
behavior and a `CHANGELOG.md` entry for the fix (FR-011, FR-015).

---

## Phase 1: Setup

- [x] T001 Amend Principle VI in `.specify/memory/constitution.md` so "What Actually Stays" grants an executable no code at all, citing `TCDir`; in the same amendment correct the `crt-pi`, libretro `bloom` and `ntsc-adaptive` allowlist rows, whose Used By and Location still read `Casso` and `Casso/Shaders/`; bump the version and Last Amended date per the amendment process
- [x] T002 Record the branch-point measurement in `specs/031-thin-exe-shim/measurements.md`: per-project line counts, `Casso` `ClCompile`/`ClInclude` counts, and the 38 dual-compile entries
- [x] T003 Merge `origin/master` into the branch and confirm Debug and Release build before any file moves

---

## Phase 2: Foundational

**Blocking**: every story phase writes into the structure these tasks create.

- [x] T004 Create the receiving directories in `CassoEmuCore/`: `Machines/Apple2/{Common,Apple2,Apple2Plus,Apple2e,Apple2eEnhanced,Apple2c}/`, `Config/`, `Shell/`, `Ui/{Chrome,Dialogs,Scene,Settings,Debug}/`, `Print/`, `Seams/`, `Gui/`
- [x] T005 Add a helper to `scripts/` that counts functions defined in a project's translation units, so SC-002 is a check that can fail rather than an inspection
- [x] T006 Add a helper to `scripts/` that reports remaining `..\Casso` `ClCompile` entries in `UnitTest/UnitTest.vcxproj`, for the per-phase measurement

---

## Phase 3: User Story 0 — Machine hierarchy (P0)

**Goal**: machine-specific code sits under its machine before any extraction begins.

**Independent test**: the suite passes unchanged; the diff shows no content change beyond include paths and header guards.

- [x] T007 [US0] Classify every file in `CassoEmuCore/Devices/Disk/` (72 files) against the FR-005c rule; record the classification and its reasoning for the commit message
- [x] T008 [P] [US0] Classify every file in `CassoEmuCore/Devices/Printer/` (47 files) against the FR-005c rule
- [x] T009 [P] [US0] Classify every file in `CassoEmuCore/Devices/Mockingboard/` (12 files) against the FR-005c rule
- [x] T010 [US0] Move model-specific devices to `CassoEmuCore/Machines/Apple2/Apple2e/` and `.../Apple2c/`: `Apple2eMmu`, `Apple2eKeyboard`, `Apple2eSoftSwitchBank`, `Apple2cRomBank`
- [x] T011 [US0] Move Apple II family devices to `CassoEmuCore/Machines/Apple2/Common/`: `AppleKeyboard`, `AppleMouse`, `AppleSpeaker`, `AppleGamePort`, `AppleSoftSwitchBank`, `LanguageCard`, `CxxxRomRouter`
- [x] T012 [US0] Move the `Disk2*` set and the `Devices/Disk/` files classified as family code to `CassoEmuCore/Machines/Apple2/Common/`; leave container and file-format parsers in `CassoEmuCore/Devices/Disk/`
- [x] T013 [US0] Move the Apple video modes and character ROM data to `CassoEmuCore/Machines/Apple2/Common/`: `AppleTextMode`, `Apple80ColTextMode`, `AppleLoResMode`, `AppleHiResMode`, `AppleDoubleHiResMode`, `CharacterRom*`; leave `VideoTiming`, `IVideoTiming`, `PixelFormat`, `MonochromeTint`, `NtscColorTable` in `CassoEmuCore/Video/`
- [x] T014 [US0] Move the `Devices/Printer/` and `Devices/Mockingboard/` files classified as family code into `CassoEmuCore/Machines/Apple2/Common/`, leaving chip emulation in `CassoEmuCore/Devices/`
- [x] T015 [US0] Move `Cli/Win32DiskFileIo` and `Cli/Win32IntentChannel` to `CassoEmuCore/Seams/`; they are seam implementations included by `Casso/` as well as the CLI and are misfiled under `Cli/`
- [x] T016 [US0] Update `CassoEmuCore/CassoEmuCore.vcxproj` file entries and filters for every move in T010-T015
- [x] T017 [US0] Update include paths and header guards across the tree for the moved files; change nothing else (FR-005e)
- [x] T018 [US0] Verify no file remaining in a machine-neutral directory (`Devices/`, `Video/`, `Audio/`, `Core/`) assumes one particular machine, and that every model directory matches one under `Resources/Machines/`
- [x] T019 [US0] Run the per-phase gate and commit with the classification reasoning and the measurement

---

## Phase 4: User Story 1 — Persistence and preferences (P1)

**Goal**: preference merge, recovery and placement restore are testable.

**Independent test**: synthetic in-memory file system; no disk, no registry, no window.

- [x] T020 [US1] Move `Casso/Config/` to `CassoEmuCore/Config/`, taking the `IFileSystem` seam and `Win32FileSystem` together
- [x] T021 [US1] Update `CassoEmuCore.vcxproj`, `Casso.vcxproj` and include paths for the moved files
- [x] T022 [US1] Delete the `Config/` `ClCompile` entries from `UnitTest/UnitTest.vcxproj` (`UserConfigStore`, `GlobalUserPrefs`, `WindowPlacementProfile`, `MachineInputPrefs`, `CrtResolver`) — linking replaces dual compilation
- [x] T023 [P] [US1] Test in `UnitTest/Config/`: a preferences document with a corrupt or absent block falls back to documented defaults, preserves the rest, and does not abort
- [x] T024 [P] [US1] Test in `UnitTest/Config/`: preference merge order across global, per-machine and override layers
- [x] T025 [P] [US1] Test in `UnitTest/Config/`: a saved window placement referring to a monitor arrangement that no longer exists restores onto a currently attached work area
- [x] T026 [P] [US1] Test in `UnitTest/Config/`: monitor catalog lookup and CRT override resolution against synthetic catalogs
- [x] T027 [US1] Launch the emulator and confirm every Settings page reads and writes as before
- [x] T028 [US1] Run the per-phase gate and commit with the measurement

---

## Phase 5: User Story 2 — The already-pure strays (P1)

**Goal**: the self-contained modules stranded in the exe become linkable.

**Independent test**: each moved module is constructed from synthetic data and asserted with no exe involvement.

- [x] T029 [P] [US2] Move `TrackSectorPredicate` to `CassoEmuCore/Devices/Disk/` and update its project entries
- [x] T030 [P] [US2] Move `DebugDialogProjection`, `Disk2DebugDialogState`, `InputDebugDialogState`, `InputEventDisplay`, `Disk2EventDisplay` to `CassoEmuCore/Ui/Debug/`
- [x] T031 [P] [US2] Move `DiskSettings` to `CassoEmuCore/Config/` and `PerfStats` to `CassoEmuCore/Core/`
- [x] T032 [US2] Split `AssetBootstrap`: the resolution and catalog half moves to `CassoEmuCore/Config/`, the remainder stays until Story 8
- [x] T033 [US2] Delete the corresponding `..\Casso` `ClCompile` entries from `UnitTest/UnitTest.vcxproj`
- [x] T034 [P] [US2] Test: `TrackSectorPredicate` evaluated at its geometry boundaries matches the documented rule at each boundary
- [x] T035 [P] [US2] Test: the debug projections format a synthetic event stream into exactly the expected rows, including the empty and overflow cases
- [x] T036 [P] [US2] Tests for `DiskSettings`, `PerfStats` and the `AssetBootstrap` catalog half against synthetic inputs
- [x] T037 [US2] Run the per-phase gate and commit with the measurement

---

## Phase 6: User Story 3 — Layout, pacing and input mapping (P2)

**Goal**: the pure functions inside `EmulatorShell` become assertable.

**Independent test**: synthetic client sizes, DPI, band thicknesses, video state and input events in; rectangles, publish decisions and staged guest state out.

- [ ] T038 [US3] Extract viewport and chrome-band rectangle math from `Casso/EmulatorShell.cpp` into `CassoEmuCore/Shell/Layout/`
- [ ] T039 [US3] Extract client-size-for-content inversion, drive-widget row placement and work-area centering into `CassoEmuCore/Shell/Layout/`
- [ ] T040 [US3] Extract the publish-rate throttle and the dirty-render signature gate into `CassoEmuCore/Shell/Pacing/`
- [ ] T041 [US3] Extract the VK classifiers, Apple modifier mirroring, joystick axis and button staging, paddle recenter math and the absolute guest-mouse clamp-window mapping into `CassoEmuCore/Shell/Input/`
- [ ] T042 [US3] Update `EmulatorShell` to call the extracted functions, leaving its own behavior unchanged
- [ ] T043 [P] [US3] Test: a viewport rectangle computed from a client size and inverted back returns the original size at every supported DPI
- [ ] T044 [P] [US3] Test: the render gate declines an unchanged screen, and re-rasterizes when video mode, flash phase, color state or video RAM changes
- [ ] T045 [P] [US3] Test: a machine with one connected drive lays the drive row out centered, not offset
- [ ] T046 [P] [US3] Test: input mapping — VK classification, modifier mirroring, joystick staging, paddle recenter, guest-mouse clamp mapping
- [ ] T047 [US3] Launch the emulator and confirm layout, pacing and input feel unchanged
- [ ] T048 [US3] Run the per-phase gate and commit with the measurement

---

## Phase 7: User Story 4 — Shell managers behind seams (P2)

**Goal**: machine switch, disk mount, MRU, clipboard and screenshot capture are testable.

**Independent test**: mock sinks and a synthetic file system; assert dispatch, mount/eject, MRU ordering and encoded bytes.

- [x] T049 [US4] Move `Casso/Shell/` to `CassoEmuCore/Shell/`: `WindowCommandManager`, `MachineManager`, `DiskManager`, `CpuManager`, `ClipboardManager`, `ScreenshotCapture`, `DiskMru`, `WindowManager`, `ModernPrintDialog`
- [x] T050 [US4] Move `Casso/Print/` to `CassoEmuCore/Print/`
- [ ] T051 [US4] Introduce seams for the clipboard round-trip and the image encode, both sides in core, and place the OS-owned print and file dialogs behind seams as well
- [x] T052 [US4] Delete the `Shell/` `ClCompile` entries from `UnitTest/UnitTest.vcxproj`
- [ ] T053 [P] [US4] Test: switching machines re-attaches an open debug panel to the new controller and audio source — the shipped fix that has no test guarding it
- [ ] T054 [P] [US4] Test: an MRU list at capacity moves an already-present entry to the front without duplicating and without evicting an unrelated entry
- [ ] T055 [P] [US4] Test: a screenshot captured from a synthetic framebuffer decodes back to the same pixels at the expected dimensions
- [ ] T056 [P] [US4] Test: command dispatch and mount/eject outcomes against mock sinks
- [ ] T057 [US4] Launch the emulator and confirm machine switch, mount, clipboard and capture behave as before
- [x] T058 [US4] Run the per-phase gate and commit with the measurement

---

## Phase 8: User Story 5 — UI state separates from painting (P3)

**Goal**: what a page, band or scene decides is testable; only painting stays.

**Independent test**: construct each state object directly, apply changes, assert state and emitted apply commands, with no painter and no window.

- [x] T059 [US5] Move `Casso/Ui/Settings/` to `CassoEmuCore/Ui/Settings/`
- [x] T060 [US5] Move `Casso/Ui/Chrome/` to `CassoEmuCore/Ui/Chrome/`
- [x] T061 [US5] Move `Casso/Ui/Scene/` to `CassoEmuCore/Ui/Scene/`
- [x] T062 [US5] Move `Casso/Ui/Dialogs/` to `CassoEmuCore/Ui/Dialogs/`
- [x] T063 [US5] Move the `Casso/Ui/` root files to `CassoEmuCore/Ui/`
- [x] T064 [US5] Delete the remaining `Ui/` `ClCompile` entries from `UnitTest/UnitTest.vcxproj` and drop `..\Casso\Ui\Chrome` from its `AdditionalIncludeDirectories`
- [ ] T065 [P] [US5] Test: a control disabled by another control's value changes enablement when the governing value changes, asserted without painting
- [ ] T066 [P] [US5] Test: desk-scene layout places every element rectangle inside the scene bounds at every supported DPI
- [ ] T067 [P] [US5] Test: settings page validation and the apply commands each page emits
- [ ] T068 [P] [US5] Test: chrome state synchronization against synthetic machine state
- [ ] T069 [US5] Launch the emulator and walk every Settings page, the chrome bands and the desk scene, confirming no visible change
- [x] T070 [US5] Run the per-phase gate and commit with the measurement

---

## Phase 9: User Story 6 — `EmulatorShell` moves entirely (P3)

**Goal**: build, run, step, reset, power-cycle and observe a machine from a test.

**Independent test**: nothing faked but the passage of time.

- [ ] T071 [US6] Move the remainder of `EmulatorShell` to `CassoEmuCore/Shell/`, splitting it by concern rather than relocating a 15,995-line file whole
- [ ] T072 [US6] Separate device construction and the machine lifecycle façade into `CassoEmuCore/Shell/MachineHost`
- [ ] T073 [US6] Move the window, its creation and its message pump into `CassoEmuCore/Shell/Window`; being in an executable is not what makes them work
- [ ] T074 [US6] Move CPU-thread orchestration and soft-switch state ownership into core, behind a seam for the passage of time
- [ ] T075 [P] [US6] Test: a machine built headlessly runs a fixed number of cycles and its memory and soft-switch state assert as expected
- [ ] T076 [P] [US6] Test: a soft reset preserves user RAM and takes the reset vector
- [ ] T077 [P] [US6] Test: a power cycle re-seeds every DRAM-owning device before the reset sequence runs, and the result differs from a soft reset
- [ ] T078 [P] [US6] Test: stepping a paused machine retires exactly one instruction and advances the program counter accordingly
- [ ] T079 [US6] Launch the emulator and confirm boot, reset, power cycle, pause and step behave as before
- [ ] T080 [US6] Run the per-phase gate and commit with the measurement

---

## Phase 10: User Story 7 — Rendering and mixing (P3)

**Goal**: pixels and samples become assertable.

**Independent test**: render a synthetic framebuffer on a software adapter, read back, assert against a golden; mix synthetic sources and assert samples.

- [x] T081 [US7] Add a WARP-backed device and a readback path to `UnitTest`, with no window and no display; `Dxui` creates only `D3D_DRIVER_TYPE_HARDWARE` devices today
- [ ] T082 [US7] Add golden-image storage and a pixel-exact comparison helper with no tolerance parameter (FR-008a)
- [x] T083 [US7] Move `Casso/Shaders/` to `CassoEmuCore/Shaders/` and relocate the `Shaders.targets` import, whose paths are relative to the importing project
- [x] T084 [US7] Move `CrtPostProcess` and the pass structure, parameter resolution and compositing arithmetic to `CassoEmuCore/Render/`
- [x] T085 [US7] Move the audio mixing into `CassoEmuCore/Audio/`, leaving only handing finished samples to the endpoint
- [x] T086 [US7] Delete the `CrtPostProcess` `ClCompile` entry from `UnitTest/UnitTest.vcxproj`
- [ ] T087 [P] [US7] Test: a synthetic framebuffer through the full pass chain matches its checked-in golden pixel-exact
- [ ] T088 [P] [US7] Test: changing one CRT parameter changes the image in the documented direction
- [ ] T089 [P] [US7] Test: a mixed span of sources at known gains and pans matches expected samples, including at the clipping boundary
- [x] T090 [P] [US7] Test: the same inputs run twice produce identical outputs
- [x] T091 [US7] Launch the emulator and compare the rendered picture against a capture taken before the phase
- [x] T092 [US7] Run the per-phase gate and commit with the measurement

---

## Phase 11: User Story 8 — The executables become linker targets (P3)

**Goal**: neither executable project defines any function.

**Independent test**: count functions in each project; the expected answer is zero. Both executables build and start.

- [x] T093 [US8] Move `wWinMain` from `Casso/Main.cpp` to `CassoEmuCore/Gui/`, along with everything else remaining in `Casso/`
- [x] T094 [US8] Set `<EntryPointSymbol>wWinMainCRTStartup</EntryPointSymbol>` in every configuration of `Casso.vcxproj`
- [x] T095 [US8] Reduce `Casso.vcxproj` to `Casso.rc`, `resource.h` and one comment-only `Main.cpp`, removing all other `ClCompile` and `ClInclude` entries
- [x] T096 [US8] Write `Casso/Main.cpp` as the comment-only translation unit, whose haiku closes "CassoEmuCore" — five syllables (FR-003b)
- [x] T097 [US8] Move `main` from `CassoCli/CassoCli.cpp` into `CassoEmuCore/Cli/`, set `<EntryPointSymbol>mainCRTStartup</EntryPointSymbol>`, and reduce the project the same way
- [x] T098 [US8] Remove the `ProjectReference` to `Casso.vcxproj` from `UnitTest/UnitTest.vcxproj`, drop `..\Casso` from its `AdditionalIncludeDirectories`, and confirm zero `..\Casso` `ClCompile` entries remain
- [x] T099 [P] [US8] Test: the function count in each executable project is zero
- [x] T100 [US8] Build Debug and Release and confirm both executables link and start
- [x] T101 [US8] Run the per-phase gate and commit with the measurement

---

## Phase 12: User Story 9 — Invariant hardware moves into code (P2)

**Goal**: no user-editable file can compose a machine that never shipped.

**Independent test**: construct each model's definition directly and assert its devices, layout, video modes and CPU; assert a delta naming `internalDevices` changes nothing.

- [x] T109 [US9] Add `CassoEmuCore/Machines/IMachineDefinition.h` declaring the invariant surface a model supplies: internal devices, keyboard layout, video modes, CPU, RAM layout
- [x] T110 [P] [US9] Define `Apple2`, `Apple2Plus` in `CassoEmuCore/Machines/Apple2/<Model>/`, each returning its own device list
- [x] T111 [P] [US9] Define `Apple2e`, `Apple2eEnhanced` — identical device lists, differing only in CPU and ROM
- [x] T112 [P] [US9] Define `Apple2c`, including the ROM bank that is currently wired from the executable
- [x] T113 [US9] Add a definition lookup by model id and have machine construction take the invariant fields from it rather than from `MachineConfig`
- [x] T114 [US9] Remove `internalDevices`, keyboard layout, video modes, CPU and RAM from the JSON schema and from the embedded defaults; leave slots, ports and ROM overrides. Needs a `$cassoMachineVersion` bump and an upgrade path in `MachineConfigUpgrade`
- [x] T115 [US9] Remove `internalDevices` from the delta-merge in `CassoEmuCore/Config/UserConfigStore.cpp` (:2159, :2316) and bump the machine-definition version with an upgrade path
- [x] T116 [US9] Rename device type strings to the `-family-` form and update the registry, the definitions and every test
- [x] T117 [P] [US9] Test: a delta naming a different keyboard for the //c leaves the //c's own keyboard in place
- [x] T118 [P] [US9] Test: each model's definition reports the expected devices, layout, video modes and CPU
- [x] T119 [P] [US9] Test: slot contents and attached peripherals set in JSON still take effect
- [ ] T120 [US9] Launch the emulator, switch between all five machines, and confirm each still boots and behaves as before
- [ ] T121 [US9] Run the per-phase gate and commit with the measurement

### Machine classes (executed inside slices 4 and 6, not before)

These absorb most of `MachineManager`, which User Story 4 extracts and User
Story 6 finishes. Building them ahead of those slices would mean building the
hierarchy, moving it, then rewiring the shell around it.

- [x] T128 [US9] Add `Apple2` in `CassoEmuCore/Machines/Apple2/Apple2/`: speaker, game port, 40-column text, lo-res and hi-res, a virtual slot count, and a virtual game-port factory the //e can decline. It is the base as well as a machine — there is no abstract root above the ][
- [x] T129 [US9] Add `Apple2Plus` deriving from `Apple2`, then `Apple2e` deriving from `Apple2Plus`: aux bank, MMU, extended soft switches, full keyboard, 80-column text and double hi-res in its own initialization. `Apple2e` overrides the game-port factory to create nothing, because `Apple2eSoftSwitchBank` owns the paddle timer and `PREAD`
- [x] T130 [P] [US9] Confirm `Apple2Plus` stays nearly empty: its only differences from the ][ are a ROM file and a default slot card, both of which are configuration
- [x] T131 [P] [US9] Add `Apple2eEnhanced` deriving from `Apple2e`: the 65C02 and nothing else
- [x] T132 [US9] Add `Apple2c` deriving from `Apple2e` as a SIBLING of `Apple2eEnhanced`: 65C02, slot count zero, back-panel ports, ROM banking. Do not derive it from `Apple2eEnhanced`, which shipped a year later
- [ ] T133 [US9] Move the `apple2e-family-mmu` special case (`MachineManager.cpp:266`) and `WireApple2cRomBank` (`:961`) into the machines that own them; the //e's MMU and the //c's ROM bank are wired from the executable today
- [ ] T134 [US9] Replace the 14 `IsApple2c()` call sites with capability queries on the machine — which drive mesh, whether a switch band exists, whether a switch bar is shown — since every one of them is a presentation question, not an emulation one
- [ ] T135 [US9] Delete `videoConfig.modes`: nothing in production reads it, `MachineManager` builds all five renderers unconditionally, and each machine's initialization now constructs its own
- [x] T136 [P] [US9] Tests: each machine reports its own slot count, CPU and renderers; an `Apple2` has no 80-column renderer; a //c has zero slots and a //e seven

---

## Phase 13: Disk-layer factoring (P2)

**Goal**: the reusable core of the disk layer stops assuming an Apple II.

- [ ] T122 [P] Split `DiskImage`: the bit-stream track buffer stays generic; the 40/35-track, 6400-byte, 143,360-byte and quarter-track constants and the `WozMetadata` member move to an Apple II geometry
- [ ] T123 [P] Split `DiskImageStore`: the mount/eject/flush lifecycle and salvage assessment stay generic; `kSlotCount = 8` and `(slot, drive)` addressing become a machine-supplied addressing scheme
- [ ] T124 [P] Split `MountDiagnosis`: seven generic failure modes stay; `NotAWozFile`, `MalformedWoz`, `WrongSizeForNibble` and `NotANibbleStream` move to the Apple II format layer
- [ ] T125 Split `DiskCommandRunner`: the command grammar and dispatch stay generic; the `ApplesoftTokenizer` and `AppleTextCodec` calls and the DOS 3.3 / ProDOS entry formatting move behind a filesystem-formatter seam
- [ ] T126 [P] Tests for each split half, generic and Apple II
- [ ] T127 Run the per-phase gate and commit with the measurement

---

## Phase 14: Polish and cross-cutting

- [x] T102a Audit the branch for FR-005 compliance: no commit message, code comment or Constitution Check names a platform API as a reason for placement
- [x] T102b Audit the branch for FR-014 compliance: every commit that adds new logic added it to a core library, so the exe did not regrow behind the extraction
- [x] T102c Confirm FR-008 held: no slice was folded into an unrelated feature branch, and no unrelated feature work landed on this one
- [x] T102 Confirm `CassoCore` still has no include reaching into `CassoEmuCore`, `Dxui` or an executable (FR-005b)
- [x] T103 Confirm ARM64 compiles in Debug and Release
- [x] T104 Record the final measurements in `specs/031-thin-exe-shim/measurements.md` so issue #85 can be closed against them (FR-016)
- [ ] T105 Add `CHANGELOG.md` entries for defects the extraction exposed and any user-visible change, and nothing for the extraction itself (FR-015)
- [x] T106 Update `docs/` and `ARCHITECTURE.md` where they describe the old placement
- [ ] T107 Run `CheckStyle.ps1 -Mode Tree`, the full suite in Debug and Release, and the final gate from `quickstart.md`
- [ ] T108 Present the changelog, README changes and every commit message to the owner, and wait for explicit approval before proposing the merge to master

---

## Dependencies

```
Setup (T001-T003)
   -> Foundational (T004-T006)
      -> US0 (T007-T019)          fixed first; every later phase writes into it
         -> US1 (T020-T028)  ─┐
         -> US2 (T029-T037)  ─┤
                              ├-> US3 (T038-T048)
                              └-> US4 (T049-T058)
                                     -> US5 (T059-T070)
                                           ├-> US6 (T071-T080)
                                           └-> US7 (T081-T092)
                                                  -> US8 (T093-T101)
                                                        -> Polish (T102-T108)
```

US1 and US2 are independent of each other and can interleave. US3 and US4 both
depend on the seam pattern US1 establishes. US7 blocks nothing but US8.

## Parallel opportunities

- **T008, T009** — device classification in separate directories.
- **T023-T026** — Config tests, separate files.
- **T029-T031, T034-T036** — stray moves and their tests, all separate files.
- **T043-T046** — layout, pacing and input tests, separate files.
- **T053-T056** — manager tests, separate files.
- **T065-T068** — UI state tests, separate files.
- **T075-T078** — machine lifecycle tests, separate files.
- **T087-T090** — render and audio tests, separate files.

Move tasks within one phase are generally *not* parallel: they touch the same
`.vcxproj` and the same include graph.

## Implementation strategy

**The first shippable increment is Phase 3 (US0)** — not a user story in the
usual sense, but the phase that makes every later one cheap, and the one whose
cost rises the longest it is deferred.

**The first extraction increment is Phase 4 (US1)**: `Config/` already has its
seam, five of its files are already dual-compiled and therefore proven
UT-reachable, and it establishes the pattern — move the interface and its
platform implementation together, delete the dual-compile entries, backfill
tests against a mock — that Phases 5 through 11 repeat.

**Retire dual compilation as you go.** Thirty-eight entries at branch point,
zero at T098. It is the clearest per-phase signal that a move actually removed
the workaround rather than adding a second one.

**Merge master in at every phase boundary.** Four sweeping renames have landed
since August 2026. They merge cleanly and then fail to compile, so the conflict
count never shows what the merge will cost.
