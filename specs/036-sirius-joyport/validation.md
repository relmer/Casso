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
| The [Joyport manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf)'s own Applesoft test program (`Joyport.do`, from the Google Drive link in web-a2e #19 and apple2ts #213; not committed), booted on a //e and driven headlessly by a throwaway harness that read each prompt and closed the switch it asked for: both non-centered sections on both jacks (Casso emulates only the Center position of the rear switch), then the centered section with each switch closed on ONLY the jack the program named | automated, one-off | passed every Atari-stick step through to the Apple-mode paddle section, which is not emulated. The listing itself confirms the mapping: PB2 right/down, PB1 left/up, PB0 fire, and in the centered section the left stick on AN0 off and the right on AN0 on |

Note (2026-09-27): the pass above ran both one-stick sections with the
switches closed on both jacks, because only the Center position of the
Controller Select switch is emulated and the harness drove one stick. It
therefore did not show that those sections depend on the switch at Left or
Right, which GH #156 found with two controllers. Whether to emulate that
switch is open (spec FR-016).

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

## Phase 9: profile modes (US7)

| Check | Kind | Result |
|---|---|---|
| `profileMode` written for a Joyport-mode profile only and read back; absent or unknown reads normal and keeps the profile; the built-in profiles take their mode from their kind; a user profile called Joyport becomes a Joyport-mode built-in keeping its mapping (`ControllerProfileStoreTests`) | automated | pass |
| Lists per mode with the built-in first; every `ProfileSource` in each mode stamps the mode in effect and starts from the right mapping, a copy of either mode's profile included; a name used in the other mode is a duplicate; reset by the profile's mode (`ControllerProfileStoreTests`) | automated | pass |
| A normal-mode and a Joyport-mode user profile each remembered for its mode; a profile of the other mode picked, or loaded from the prefs, is ignored, plays the mode's built-in profile and is not saved back (`ControllerInputServiceTests`) | automated | pass |
| The page's list follows its mode; `SetProfileMode` after Load swaps the list and the edited profile, keeping a pending edit; each mode's choices kept for OK and reverted by Cancel; a created profile belongs to the page's mode and resets to its built-in; copy sources list both modes (`ControllersPageStateTests`) | automated | pass |
| Picker sections: normal mode lists the Default first and no Joyport profile, Joyport mode the Joyport profile first and no Default; the mode's built-in is checked with nothing chosen (`PaddleSourceRowsTests`) | automated | pass |
| Mutation: `GetProfileNames` ignoring the mode | automated | 4 tests went red: `GetProfileNames_ListsTheModesBuiltInFirstThenItsOwnProfiles`, `ProfileNames_FollowThePagesMode`, both `ProfileRows_In*Mode_*` |
| Mutation: `ReadProfile` ignoring `profileMode` | automated | 2 tests went red: `ProfileMode_IsSavedForJoyportModeOnlyAndReadBack`, `ProfileMode_AbsentOrUnknownReadsNormalAndKeepsTheProfile` |
| Mutation: `ResetProfile` always the Default mapping | automated | 2 tests went red: `ResetProfile_RestoresTheBuiltInMappingOfTheProfilesMode`, `ResetProfile_RestoresTheJoyportProfileToItsOwnMapping` |
| Mutation: `SetActiveProfile` accepting the other mode's profile | automated | 1 test went red: `ChosenProfile_OfTheOtherMode_IsIgnored` |
| Mutation: `SetProfileMode` only storing the value | automated | 1 test went red: `SetProfileMode_AfterLoad_SwapsTheListAndTheEditedProfile` |
| Mutation: `SetProfileSections` adding both built-in profiles again | automated | 6 of 7 `ProfileRows_*` tests went red, including both mode tests |
| Full unit suite | x64 Release | 5,709 of 5,709 |
| V17 in the running app | manual | not yet run |

## Phase 10: the Joyport profile's second stick (US7)

| Check | Kind | Result |
|---|---|---|
| `FindSecondStick`: a DirectInput gamepad with Z and Rz gets Z/Rz; with Z, Rx and Ry but no Rz, or Rx and Ry alone, gets Rx/Ry; neither pair whole gets none; a joystick and a wheel get none (`ControllerProfileStoreTests`) | automated | pass |
| `MakeJoyport`: the second stick bound Absolute on PDL0 and PDL1 after the primary stick; an Xbox controller keeps its right stick; a joystick and a wheel steer with the primary stick and D-pad only; for every form factor on both an Xbox and a DirectInput model, no trigger on any axis (`ControllerProfileStoreTests`) | automated | pass |
| The form factor reaches the built-in mapping: the service plays a gamepad's Z with the Joyport on and not a joystick's (`ControllerInputServiceTests`); the page's entry keeps the device's form factor and shows its Joyport mapping (`ControllersPageStateTests`) | automated | pass |
| Mutation: `FindSecondStick` ignoring the form factor | automated | 4 tests went red: `FindSecondStick_OnAJoystickOrWheel_IsNone`, `JoyportMapping_OnADirectInputJoystickLeavesTheOtherAxesAlone`, both `JoyportProfile_*` |
| Mutation: `FindSecondStick` preferring Rx/Ry | automated | 1 test went red: `FindSecondStick_OnAGamepad_PrefersZAndRz` |
| Mutation: `MakeJoyport` binding a trigger axis | automated | 2 tests went red: `JoyportMapping_NoTriggerEverSteers`, `JoyportMapping_SteersWithEverythingAndFiresWithEverything` |
| Full unit suite | x64 Release | 5,717 of 5,717 |
| V16: a DirectInput gamepad and a flight stick | manual | not yet run |

## Phase 11: a global Joyport setting (US4)

| Check | Kind | Result |
|---|---|---|
| `JoyportSetting::IsInEffect` for all four combinations of setting and annunciators; `IsMousePaddleOffered` only while the Joyport is not in effect (`JoyportSettingTests`) | automated | pass |
| `JoyportSetting::ResolveAtLaunch`: a set global token wins and is not adopted, whatever the machine block holds, the //c included; an empty token adopts the launched //e's saved Joyport, and None with no key, no block, or on the //c; an unknown token is None and not adopted (`JoyportSettingTests`) | automated | pass |
| `GlobalUserPrefs::gamePortAdapter` round-trips both tokens and is written once; an absent key loads empty and a save leaves it unwritten (`GlobalUserPrefsTests`) | automated | pass |
| `ReadGamePortAdapter` stays the adoption reader; a machine's input entries never write the key; a legacy key in a machine block survives a later `SaveDelta` untouched (`MachineInputPrefsTests`, `UserConfigStoreTests`) | automated | pass |
| Mutation: `IsInEffect` ignoring `hasAnnunciators` | automated | 1 test went red: `IsInEffect_OnlyForTheJoyportOnAMachineWithAnnunciators` (`the //c reads it as off`) |
| Mutation: `ResolveAtLaunch` adopting when the global token is set | automated | 2 tests went red: `ResolveAtLaunch_ASetGlobalTokenWinsAndIsNotAdopted` and `ResolveAtLaunch_AnUnknownTokenIsNoneAndNotAdopted` |
| Mutation: `GlobalUserPrefs` not writing the key | automated | 1 test went red: `GamePortAdapter_ASetValueRoundTripsAndIsWrittenOnce` (`Expected:<none> Actual:<>`) |
| Full unit suite | x64 Release | 5,796 of 5,796 |
| V12-V14 in the running app | manual | not run in this phase; the setting's on-screen check is made with Phase 12's switch |

## Phase 16: new profile starting points (US7)

| Check | Kind | Result |
|---|---|---|
| `ControllerProfileStore::CreateProfile` refuses a copy of the other mode's profile, the other mode's built-in included, with `NotFound`, and adds nothing; every other source still stamps the mode in effect (`ControllerProfileStoreTests`) | automated | pass |
| `GetCopySourceNames` lists only the page mode's profiles, less its built-in profile while that is still the built-in mapping, and lists it once edited, a pending edit included; empty with no controller or nothing saved; the page's `CreateProfile` refuses a copy of the other mode's profile (`ControllersPageStateTests`) | automated | pass |
| `GetStartingPoints`: Default mapping, Paddles and Copy of in normal mode, Joyport mapping and Copy of in Joyport mode, Copy of left out with nothing to copy; `GetStartingPointLabel` for every source (`ControllersPageStateTests`) | automated | pass |
| Mutation: the store's `CreateProfile` without the mode check | automated | 2 tests went red: `CreateProfile_ACopyOfTheOtherModesProfile_IsRefused` (`a Joyport-mode profile in normal mode`) and `CreateProfile_FromEverySourceInEachMode_StampsTheModeInEffect` |
| Mutation: the built-in profile always in the copy list | automated | 2 tests went red: `CopySources_ListThePagesModeLessAnUneditedBuiltIn` (`normal mode: its own profiles, less the unedited Default`) and `CopySources_AModelWithNothingSavedHasNone` |
| Mutation: the Joyport mapping offered in normal mode | automated | 1 test went red: `StartingPoints_AreTheModesOwn` (`normal mode: the Default mapping, Paddles, or a copy`) |
| Mutation: the page's `CreateProfile` without the mode check | automated | 1 test went red: `CreateProfile_ACopyOfTheOtherModesProfile_IsRefused` (`a normal-mode profile in Joyport mode`) |
| Full unit suite | x64 Release | 5,790 of 5,790 |
| The New profile dialog on screen in both modes | manual | not run: opening it needs an attached controller and a walk through the sheet by posted input; left for the owner |