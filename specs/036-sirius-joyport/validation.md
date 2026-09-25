# Validation: Sirius Joyport Emulation

**Spec**: [spec.md](spec.md) | **Quickstart**: [quickstart.md](quickstart.md)

Results per phase. "Automated" rows are tests that run in the suites; "manual"
rows need a person, a real controller, or a commercial disk.

## Baseline

| Check | Build | Result |
|---|---|---|
| Full unit suite before any change | x64 Debug, `d185c107` | 5,586 of 5,586 passed |

## Phase 2: annunciators and the device

| Check | Kind | Result |
|---|---|---|
| AN0-AN2 by read and write on the ][+ and //e banks; DHIRES on AN3; //c IOU; power cycle clears, Ctrl-Reset keeps (`AnnunciatorTests`) | automated | pass |
| 12 (button, AN0, AN1) combinations; reset window edges; attach mid-run; reset while detached (`SiriusJoyportTests`) | automated | pass |
| Both jacks through the bus on the ][+, //e and enhanced //e; paddles read 255; detached reads unchanged (`JoyportMachineTests`) | automated | pass |
| Mutation checks: AN0/AN1 swapped, sink jack write stubbed, token parse stubbed, //e keyboard and ][+ game port skipping the Joyport, annunciator handling removed | automated | every targeted test went red |
| Full unit suite | x64 Debug, `7a5a6d08` | 5,612 of 5,612 passed |

## Phase 3: one controller (US1)

| Check | Kind | Result |
|---|---|---|
| Threshold on each direction, after the deadzone; diagonal; digital pair; rate binding opens on release; inverted; PB0 fire, PB1/PB2 nothing (`MappingEvaluatorTests`) | automated | pass |
| One controller on both jacks; none closes nothing (`ControllerInputServiceTests`) | automated | pass |
| Jacks by axis owner; Apple keys never close a switch (`GamePortInputMixerTests`) | automated | pass |
| Picker row position, check state, toggle, absent on the //c (`PaddleSourceRowsTests`) | automated | pass |
| Alt keys left out of the fire keys while attached (`InputModeRulesTests`) | automated | pass |
| SC-001: `JoyportTest.dsk` booted on a //e, Applesoft reads all ten switches in both AN1 states on both jacks, two complementary patterns: 20 of 20 (`GuestVisibleJoyportTests`) | automated, scenario | pass |
| Mutation checks: switches judged on the paddle byte (only the rate-binding test catches it, as designed), Apple keys into fire, picker row always offered, Alt kept while attached, every driver on the left jack | automated | every targeted test went red |
| Full unit suite; scenario suite | x64 Debug | 5,637 of 5,637; 23 of 23 |
| V1, V2: a real controller on the readout disk, //e and ][+ | manual | not yet run: needs a person holding a controller |
| The Joyport manual's own Applesoft test program (`Joyport.do`, from the Google Drive link in web-a2e #19 and apple2ts #213; not committed), booted on a //e and driven headlessly by a throwaway harness that read each prompt and closed the switch it asked for: both non-centered sections on both jacks (Casso emulates only the Center position of the rear switch), then the centered section with each switch closed on ONLY the jack the program named | automated, one-off | passed every Atari-stick step through to the Apple-mode paddle section, which is not emulated. The listing itself confirms the mapping: PB2 right/down, PB1 left/up, PB0 fire, and in the centered section the left stick on AN0 off and the right on AN0 on |

## Phase 4: resets (US2)

| Check | Kind | Result |
|---|---|---|
| SC-003: 20 Ctrl-Resets on a //e with the Joyport attached and every line open, each a plain warm reset; 20 power-ons, each reaching the BASIC prompt with Applesoft initialized, on the real //e ROM (`JoyportMachineTests`) | automated | 20 of 20 and 20 of 20 |
| Open Apple held through Ctrl-Reset still reboots; a held fire does not turn a reset into a reboot; after the window Open Apple, Closed Apple and Shift change nothing; a power cycle opens the window again; a machine switch to the ][+ or //c leaves nothing held | automated | pass |
| Mutation check: the reset window removed from `MachineHost` | automated | five tests went red, including the Open Apple one, which ran self-test instead of rebooting |
| Every reset entry point (Ctrl-Reset, power cycle, machine switch) reaches `MachineHost::SoftReset` or `PowerCycle` | code read | yes |
| Full unit suite | x64 Debug | 5,644 of 5,644 |
| V3 in the running app | manual | not yet run; the automated cases boot the same ROM through the same reset paths |

## Phase 5: the setting (US4)

| Check | Kind | Result |
|---|---|---|
| Game port group with exactly one of None / Sirius Joyport checked; absent on the //c; the rows act as a radio pair and re-check in place (`HardwarePageTests`) | automated | pass |
| Round trip, dirty, pushed live with no reset; a picker change while the sheet is open is kept on OK and is not dirty; a Machine-tab edit survives the picker; offered on the machines with annunciators only (`SettingsPanelStateTests`) | automated | pass |
| Commands unique and routed to the UI thread (`ChromeCommandRoutingTests`) | automated | pass |
| `"none"` not stored; the Joyport kept for its own machine only (`UserConfigStoreTests`) | automated | pass |
| SC-007: token reading, the builder's round trip, and each machine adopting only its own saved value, the //c none even when its file claims one (`MachineInputPrefsTests`) | automated | pass |
| Mutation checks: the dirty check ignoring the field; the reader ignoring annunciators; the group always offered; the live observation not re-seeding the baseline | automated | every targeted test went red |
| Full unit suite | x64 Debug | 5,658 of 5,658 |
| V5, V9 in the running app (picker, Machine tab, relaunch, machine switch, picker used while Settings is open) | manual | not yet run: opening the Settings sheet or the picker comes up over the user's work |

## Phase 6: two players (US3)

The multiplayer rule landed with Phase 3, since it is the same few lines that
place a player on a jack.

| Check | Kind | Result |
|---|---|---|
| Each player on their own jack; player 2's fire only on the right jack; slot is the jack whatever its paddles; player 2 leaving opens only the right jack; multiplayer with nobody connected falls back to one controller (`ControllerInputServiceTests`) | automated | pass |
| V4: two real controllers | manual | not yet run |

## Phase 7: the Controllers page (US5)

| Check | Kind | Result |
|---|---|---|
| Jack for the controller in Editing: both alone; left and right by slot in multiplayer, whatever the slots' paddles, following Editing; none with nothing to edit; heading text; the live reading carries the pending mapping's switches (`ControllersPageStateTests`) | automated | pass |
| Mutation check: the jack always Both | automated | two tests went red |
| Full unit suite | x64 Debug | 5,662 of 5,662 |
| V8: the five lights and the heading on the running page, a diagonal, short of and past halfway, detaching | manual | not yet run: opening Settings comes up over the user's work |

## Merge gate (`0fc1a7ca`)

| Check | Result |
|---|---|
| x64 Debug `-Target Rebuild -RunCodeAnalysis` | 0 warnings, 0 errors |
| x64 Release `-Target Rebuild -RunCodeAnalysis` | 0 warnings, 0 errors |
| ARM64 Debug build (build only; no ARM64 device) | 0 warnings, 0 errors |
| Full unit suite, x64 Debug | 5,662 of 5,662 |
| Full unit suite, x64 Release | 5,660 of 5,660 (Release skips the assertion-behavior tests) |
| Scenario suite, x64 Debug | 23 of 23 |
| `scripts/CheckStyle.ps1 -Mode Tree` | 1,562 files, OK |
| SC-002 (one frame) | not measured separately: the switches ride the same `Submit` and UI-thread flush as the controller buttons whose latency spec 034 measured |
| V6 Wavy Navy on the ][+, V7 Boulder Dash on the //e, V10 detached with `JoystickTest.dsk` | manual, not yet run: need the game disks and a person at a controller |
