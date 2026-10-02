# Research: Sirius Joyport Emulation

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Every decision below was checked against the tree at `d185c107`. The hardware
behavior comes from the [Sirius Joyport owner's manual](https://mirrors.apple2.org.za/ftp.apple.asimov.net/unsorted/Sirius%20Joyport%20Manual.pdf),
with a searchable [OCR copy](Sirius%20Joyport%20Manual%20%28OCR%29.pdf) kept in this
directory (also archive.org `siriusjoyportmanual`). No GPL emulator source was read; GSSquared's prose docs
disagree with the manual on AN0 and AN1, and the manual wins (spec Assumptions).

R14-R24 record the 2026-09-27 decisions (spec Clarifications, GH #156) and were
checked against the tree at `b3feaa4a`, which includes the built-in Joyport
profile (`3f0c4620`) and the per-mode profile choices (`c00c2c7a`). Where one
of them replaces an earlier decision, the earlier one is marked superseded and
kept as written.

## R1. What the manual gives

- **Decision**: AN0 off (`$C058`) selects the left jack, on (`$C059`) the right.
  AN1 off (`$C05A`) selects left/right, on (`$C05B`) up/down. PB0 is fire, PB1
  left or up, PB2 right or down. A switch reads closed with bit 7 clear.
- **Rationale**: the manual's own tables, as quoted in the spec's Context. The
  manual also describes the Controller Select switch: Left and Right hold one
  jack whatever AN0 says, Center lets AN0 choose. Only Center is in scope.
  (2026-09-27: whether Left and Right are emulated is now an open decision,
  FR-016; see R24.)
- **Alternatives considered**: GSSquared's documented sense (both lines
  reversed). Rejected; Wavy Navy on the ][+ is the check (quickstart V6).

## R2. Where the annunciator state lives

- **Decision**: `AppleSoftSwitchBank` (the ][/][+ bank, and the base of
  `Apple2eSoftSwitchBank`) gains an `m_annunciators` bitfield for AN0-AN2, set
  by any access to `$C058`-`$C05D`, with `IsAnnunciatorOn (int index)`. The //e
  bank delegates `$C058`-`$C05D` to the base after its existing //c IOU branch,
  so while IOU access is on, the //c mouse still receives them and the
  annunciators do not change. `$C05E`/`$C05F` stay the //e's DHIRES arms.
- **Rationale**: today both banks drop `$C058`-`$C05D` in a `default:` arm; no
  machine stores AN0-AN2. On the //e and //c these addresses reach the bank
  only through `Apple2eKeyboard`'s sibling forwarding (the keyboard's range,
  `$C000`-`$C063`, starts lower and wins the overlap), which is unchanged.
  Writes already route to `Read`, so reads and writes both toggle, as on the
  hardware. On the //c with IOU access off, `$C058`-`$C05D` also record AN0-AN2,
  although the //c has no annunciator lines. That is deliberate and harmless:
  the //c never has a Joyport to read them, and excluding it would add a
  machine check to the bank for no observable effect.
- **Power-on state**: all off (spec Assumptions): the member starts at zero,
  and a new `AppleSoftSwitchBank::PowerCycle` override clears it and then calls
  the virtual `SoftReset()`, so the //e bank inherits it. `Reset()` and
  `SoftReset()` do not touch it, so a Ctrl-Reset changes the annunciators only
  if the reset code writes them. This matters on the //e, whose bank
  `SoftReset` runs its whole `Reset()` today.
- **Alternatives considered**: a separate annunciator device on the bus.
  Rejected; it would fight the keyboard/bank overlap for the same addresses.

## R3. Which device answers a Joyport button read

- **Decision**: a new `SiriusJoyport` class in
  `CassoEmuCore/Machines/Apple2/Common/`, not a bus device. The two devices that
  already answer PB0-PB2 (`AppleGamePort` on the ][/][+, `Apple2eKeyboard` on the
  //e) each get a `SiriusJoyport *` and ask it first:
  `bool TryReadButton (int index, Byte & value)`. When it returns true the
  device uses that value (and still emits its button-read event); when false it
  answers as it does today. The paddle reads ask `IsDrivingPaddles()` and read
  bit 7 set (no pot) while it returns true.
- **Rationale**: the answer has to be computed at read time from the current
  annunciators (FR-004), which rules out staging values with `SetButton`. The
  sibling-pointer precedent exists: the //e bank's status register already reads
  bits owned by the MMU and the language card, and the keyboard's `$C063`
  depends on the //c mouse. The Joyport holds a pointer to the machine's
  `AppleSoftSwitchBank` for the annunciators.
- **Built for**: every machine whose `MachineDefinition` has the new
  `hasAnnunciators` flag: the ][, ][+, //e and enhanced //e. Not the //c
  (FR-001). `MachineHost` owns it (`SetJoyport` / `GetJoyport`, beside the
  mouse); `MachineRefs` gains a `joyport` pointer for the sink.
- **Alternatives considered**: putting the switch logic inside each of the two
  reading devices. Rejected; the rules would exist twice and the ][+ and //e
  would drift.

## R4. The reset window

- **Decision**: `SiriusJoyport::OnMachineReset()` stamps the CPU's total-cycle
  counter. While fewer than `kReleaseCycles = 500'000` cycles (about 0.49 s at
  1.023 MHz) have run since the stamp, `TryReadButton` returns false and the
  reading device answers exactly as with no Joyport, including the //e's Open
  Apple and Closed Apple keys and their reset holds (FR-011).
  `MachineHost::SoftReset` and `MachineHost::PowerCycle` call it **after** the
  CPU has reset. A machine switch ends in `PowerCycle`, so it is covered.
- **Rationale**:
  - The //e firmware reads `$C061` 577 cycles after /RESET
    (`Apple2eKeyboard.h`, the comment on `kResetHoldCycles`). 500,000 cycles is
    several hundred times that and still short enough that a game never
    notices.
  - `PowerCycle` zeroes the cycle counter *after* the bus devices' own
    `PowerCycle` has run, so a stamp taken inside a bus device's reset would be
    stale by millions of cycles. Calling the Joyport from `MachineHost` after
    the CPU avoids that, and needs no per-slice tick. The Joyport is not a bus
    device, so it does not receive `SoftResetAll` / `PowerCycleAll`.
  - Counting emulated cycles, not host time, keeps the window the firmware's at
    any speed setting, the same reasoning as `kResetHoldCycles`.
- **Attach while running**: `SetAttached (true)` does not open a window. The
  Joyport answers the next read (spec Clarifications: no reset needed).
- **Alternatives considered**:
  - A tick-driven countdown like `TickResetHold`. Rejected; headless and test
    runs do not tick, so tests would need to drive it by hand.
  - Suppressing only the //e ROM's self-test check. Rejected; it would patch
    firmware behavior instead of the lines, and would not cover a held fire
    button on reboot.

## R5. Where the switches come from

- **Decision**: `MappingEvaluator::Evaluate` also fills a new
  `GamePortContribution::switches` field (`JoystickSwitches`, five bits). It
  is computed from the **shaped** PDL0 and PDL1 deflections (after dead zone and
  calibration, before `ToAxisPaddle`) against `kSwitchThreshold = 0.5f`, and
  from the PB0 bindings for fire.
- **Rationale**:
  - The evaluator's byte output cannot tell a full-deflection stick from a
    D-pad, and a **Rate** binding (the Paddles template) outputs a held
    position rather than a deflection. A threshold on the byte would leave a
    rate-bound direction closed after the stick is released. The shaped float
    exists only inside `EvaluatePair`, so the switches have to be produced there.
  - A digital pair shapes to exactly -1, 0 or +1, so it crosses any threshold
    below 1 the moment it is pressed (FR-005: "no threshold involved"), and both
    directions held already shape to 0, which closes neither (FR-007).
  - Each axis is judged on its own, so a diagonal closes two switches (FR-005).
    The threshold is on each axis's shaped value, not on the radius.
  - 0.5 is half the travel beyond the dead zone, which is well short of full
    deflection (FR-006, spec Assumptions).
- **Alternatives considered**: a threshold on the paddle byte (below 64, above
  191). Correct for Absolute bindings only; rejected because of Rate bindings.

## R6. Which controller drives which jack

> **Partly superseded by R21 (2026-09-27).** The keyboard, owner and Alt-key
> rules below stand. The controller rows (single source on both jacks,
> multiplayer slot 1 left and slot 2 right) are replaced by R21, which derives
> the jacks from spec 034's players and adds the held jack and Same as left.

- **Decision**: `ControllerInputService` builds a `JoyportJacks` value (two
  `JoystickSwitches`) from each driver's own pre-merge `logical->switches`,
  beside the existing `BuildMergedLocked`, and hands it to the mixer on the
  same `Submit` as the merged contribution:
  - **Single-source**: the selection's switches on both jacks (FR-008).
  - **Multiplayer live**: slot 1's controller on the left jack, slot 2's on the
    right, whatever each slot's paddle target is (User Story 3, scenario 3). A
    slot whose controller is absent or disconnected reads all open.
  - **Multiplayer configured but no slot connected** (not live): the service
    already falls back to single-source for the paddles, and the jacks follow
    the same fallback.
- **Rationale**: the merge collapses players (player 2's fire lands on PB1), so
  the jacks must be taken before it. Keeping them on the Controller source's
  contribution keeps the mixer the single writer (spec 034's FR-014).
- **Keyboard sources**: the mixer derives left-jack switches for the
  `ArrowKeys` source from its PDL0/PDL1 bytes (0, 127 or 255, so no threshold
  question arises) and fire from the `FireKeys` source's PB0, and copies them
  to the right jack (single-source rule). `MousePaddle` and
  `AppleModifierKeys` never drive switches (FR-009, FR-010).
- **The Alt keys**: the `FireKeys` source is submitted only in
  arrows-to-joystick mode, and today it takes PB0 from X **or left Alt** and
  PB1 from Z or right Alt. Left and right Alt are also the //e's Open Apple and
  Closed Apple, so left Alt would close Fire, which FR-010 rules out. While the
  Joyport is attached the source takes X and Z only; the rule is a pure
  `InputModeRules::GetFireKeyButtons` so it is testable, and the shell only
  reads the keys. Detached, nothing changes (FR-013).
- **Which source drives**: the existing axis owner decides. Controller owner:
  the Controller source's jacks. ArrowKeys owner: the keyboard-derived jacks.
  MousePaddle or None: all open. Under None the `FireKeys` source is either
  absent or left over from arrows mode, and the spec's "no controller at all"
  edge case requires every switch open.

## R7. How the switches reach the Joyport

- **Decision**: `GamePortState` gains `JoyportJacks jacks`. The mixer computes
  it in `ComputeTargetLocked` and `MachineGamePortSink::WriteJacks` calls
  `joyport->SetJackSwitches (jack, switches)` when they changed, beside the
  existing paddle and button writes. The Joyport stores each jack in an
  `std::atomic<Byte>`.
- **Rationale**: the existing flush path (controller thread `Submit`, one posted
  `WM_APP_GAMEPORT_FLUSH`, UI-thread `TryApply`) already meets SC-002 for
  buttons; the jacks ride the same path. The CPU thread only loads an atomic
  per read.
- **Detach and machine switch**: a detached Joyport returns false from
  `TryReadButton` and `IsDrivingPaddles`, so every read falls straight back to
  the staged buttons and paddles that the sink has kept writing all along. There
  is nothing to release (spec edge case: machine switch with a controller held).

## R8. The adapter setting and its persistence

> **Superseded by R14 (2026-09-27)** for where the setting is kept: it is now
> global. The enum and its tokens stand.

- **Decision**: a `GamePortAdapter` enum (`None`, `SiriusJoyport`) with tokens
  `"none"` and `"siriusJoyport"`, stored per machine as
  `$cassoUiPrefs.gamePortAdapter`. Default `None`, so `SaveDelta` omits it. It
  is read in `EmulatorShell::ApplyPersistedChromePrefs` (cold boot) and in
  `MachineManager::SwitchMachine` beside `mouseConnected`, and written from the
  picker through `DiskSettings::WriteSavedUiPrefs`. A machine without
  `hasAnnunciators` ignores the key.
- **Rationale**: this is how `mouseConnected` and the input mode already
  persist per machine. An enum rather than a bool matches the spec's entity
  ("None, or the Sirius Joyport") without adding choices.
- **Alternatives considered**: a `ports[]` entry like the //c's `joystick`
  port. Rejected; the ][+ and //e have no `ports` array and the adapter is a UI
  pref, not machine hardware that `MachineConfigLoader` must build.

## R9. The command-bar picker row

> **Amended by R15 (2026-09-27).** The row is now labeled "Joyport (Atari
> mode)", and `isOffered` comes from R16's rule. Placement, check state and
> thread handling stand.

- **Decision**: a checkable **Sirius Joyport** command in the paddle-source
  picker, placed after the source rows and before the Multiplayer separator's
  group, wired like the mouse toggle: `EmulatorCommands::SetJoyportFns (isOn,
  isOffered, toggle)`. `isOffered` is the machine's `hasAnnunciators`. The
  toggle calls `EmulatorShell::SetGamePortAdapter`, which sets the Joyport on
  the live machine, persists the pref and resyncs the picker (FR-012).
- **Rationale**: the Joyport is not a paddle source. The source rows are
  mutually exclusive, and the Joyport is attached on top of whichever source is
  chosen, so it cannot be a `PaddleSource` row. The mouse toggle is the
  existing precedent for a checkable, machine-dependent toolbar command.
- **Thread**: the dispatch runs on the UI thread, which is also where the sink
  writes. `SiriusJoyport::SetAttached` stores an atomic, and the machine
  lifetime lock is held shared around the pointer, as in the sink.

## R10. The Machine tab entry

> **Superseded by R15 (2026-09-27).** The Machine tab no longer lists the
> Joyport; the Game port group and its OK-applied path are removed.

- **Decision**: `HardwarePage::BuildNodes` gains a **Game port** group, present
  only when the machine has annunciators, holding two checkbox rows, **None**
  and **Sirius Joyport**, that behave as a radio pair: `SetOnToggle` routes
  either label to `SettingsPanelState::SetGamePortAdapter`, and the rebuilt
  nodes show exactly one checked. Unchecking the checked row is ignored.
- **Rationale**: `DxuiTreeView` offers only checkboxes, and the spec asks for
  the choices None and Sirius Joyport in the device tree. A radio pair of
  checkbox rows gives both choices without a new widget. Synthetic leaf nodes
  not backed by a `HardwareEntry` already exist (External drive, Mouse,
  Drive 2).
- **Apply**: `SettingsUiPrefs` gains `gamePortAdapter`, read in
  `ExtractUiPrefs`, written in `BuildJson`, compared in `ArePrefsEqual`, and
  pushed live through a new `ISettingsApplySink::ApplyGamePortAdapter` that
  posts `IDM_GAMEPORT_ADAPTER_NONE` / `IDM_GAMEPORT_ADAPTER_JOYPORT` to the UI
  thread. It never queues a reset (FR-002).
- **Alternatives considered**: a combo box beside the tree. Rejected as a new
  layout for a two-choice setting.

## R11. The picker used while Settings is open

> **Superseded by R15 (2026-09-27).** The setting leaves the sheet's snapshot
> and applies at once from the Controllers page, so OK has nothing to revert
> and the live observation is removed.

- **Decision**: `SettingsPanelState::ObserveLiveGamePortAdapter (adapter)`,
  called from `SettingsSheet::OnDialogTick` with the shell's live value. When
  the live value differs from the last one observed, the state re-seeds
  `m_original.prefs.gamePortAdapter`, and also `m_current`'s if the user had not
  changed the entry (current equal to the old original). The tree is rebuilt.
- **Rationale**: every key the sheet owns is otherwise written back from the
  snapshot taken when the sheet opened, so a picker change made while the sheet
  is open would be reverted on OK (spec edge case). Re-seeding the baseline
  keeps the entry not dirty and makes OK write the live value. A pending edit
  the user made on the Machine tab still wins on OK, as the last explicit act.
- **Alternatives considered**: keeping the setting out of the snapshot and
  applying it at once like multiplayer. Rejected; FR-002 says the Settings path
  applies on OK like the sheet's other settings.

## R12. The Controllers page lights

> **As built, and amended by R23 (2026-09-27).** The five lights were built as
> `JoyportSwitchView`, an Atari stick drawn from above whose markers and fire
> button light, in place of five `ButtonLightView`s; the jack heading stands.
> R23 adds the Joyport section and the per-mode profile list above it.

- **Decision**: while the Joyport is attached, `ControllersPage` hides the
  stick widget, the button lights and their headings, and shows a **Joyport**
  heading with five `ButtonLightView`s (Up, Down, Left, Right, Fire) lit from
  `ComputeLiveReading(sample).switches`, which the page's own evaluator already
  produces once R5 lands. A caption gives the jack:
  `ControllersPageState::GetJoyportJack()` returns left, right or both (single
  source) for the controller in Editing, and the page shows "Left jack",
  "Right jack" or "Both jacks". The page relayouts when the attach state
  changes, as it does for multiplayer.
- **Rationale**: reuses the page's live reading, which already applies the
  pending mapping, so the lights show what the edited profile would close
  (User Story 5's purpose). `ButtonLightView` needs no change.

## R13. The readout disk

- **Decision**: `Disks/Casso/JoyportTest.bas`, built into
  `Disks/Casso/JoyportTest.dsk` with the same three `CassoCli disk` commands as
  `JoystickTest.dsk` (spec 034 quickstart). It writes each of the four
  annunciator combinations with `POKE`, reads `PEEK(49249)`-`PEEK(49251)`,
  and prints a two-column table (left and right jack) of UP, DOWN, LEFT, RIGHT
  and FIRE as CLOSED or OPEN, in uppercase for the ][+.
- **Rationale**: SC-001 is stated in terms of a readout disk; one screen shows
  all 20 combinations. AppleWin issue #1517's `JOYPORT.DSK` is a useful second
  opinion, but it is a third-party image, so it is used only if the user
  supplies it and is never committed.

## R14. The Joyport setting is global, and adopts the old per-machine value once

> **Superseded by R27 (2026-09-28).** There is no Joyport setting; the
> global and per-machine values are read once by R27's migration.

- **Decision**: `GlobalUserPrefs` gains `std::string gamePortAdapter`, holding
  the existing tokens `"none"` or `"siriusJoyport"` (R8), empty meaning never
  set. It is read at launch and written by every change from the picker or the
  Controllers page, through `SaveGlobalPrefs`. When it is empty at launch, the
  shell adopts the launched machine's `$cassoUiPrefs.gamePortAdapter` through
  the existing `MachineInputPrefs::ReadGamePortAdapter (uiPrefs,
  hasAnnunciators)`, stores the result globally at once, and never reads the
  per-machine key again. A machine switch no longer reads anything; it applies
  the global value through R16's rule.
- **The decision is a pure function**:
  `JoyportSetting::ResolveAtLaunch (const std::string & globalToken, const
  JsonValue * launchedUiPrefs, bool launchedHasAnnunciators)` returns the
  setting and whether it was adopted (so the caller saves). Both call sites,
  cold boot and the adoption save, stay one-line forwarders.
- **The per-machine key**: no longer written. Values already in a user's file
  are left in place, like the 1.22 input keys in `GlobalUserPrefs`, so an older
  build still finds what it wrote. `PersistGamePortAdapterForMachine` and the
  machine-switch half of `AdoptGamePortAdapterForMachine` are removed;
  `BuildUiPrefsDefaults` keeps the `"none"` default so `SaveDelta` still drops
  a default a user's file never had.
- **Rationale**: the spec makes the setting follow the game, not the machine
  (Clarifications 2026-09-27). One adoption from the machine being launched
  keeps an upgrading user's Joyport on where they used it; empty-means-unset
  makes it one-time without a separate flag, and an unknown token still reads
  as `None` (R8).
- **Alternatives considered**: adopting from any machine that had it on.
  Rejected; FR-002 specifies the launched machine, and scanning every machine
  block would let a machine the user has not run in months decide it. A
  migration flag in the file. Rejected; the empty token already carries it.

## R15. Apple / Atari is the control that turns the Joyport on; the Machine tab drops it

> **Superseded by R27 (2026-09-28).** The switch section, the picker row and
> the commands are removed; a player's mode turns the Joyport on. The Machine
> tab removal stands.

- **Decision**: the Joyport is turned on and off in two places, both of which
  call one shell function, `EmulatorShell::SetGamePortAdapter`, which sets the
  live machine's Joyport, the controller service's mode and the global pref,
  then resyncs the picker:
  - the picker row, relabeled **Joyport (Atari mode)**, placement unchanged
    (R9);
  - a **Joyport** section at the top of the Controllers page, drawn as the
    unit's Apple / Atari switch seen from above (R19), with "Atari mode" or
    "Apple mode" beside it. It applies at once, like the page's multiplayer
    controls, through a `SetJoyportFns (isOn, isOffered, set)` on
    `ControllersPage` that `SettingsSheet` wires to the shell.
    (Superseded 2026-09-27: the switch carries "Apple (rear)" above it and
    "Atari (front)" below it, with "Joyport" to its left, and the page has no
    Multiplayer checkbox any more; see R23.)
- **Removed**: the Machine tab's Game port group (`HardwarePage::BuildGamePortGroup`,
  `SetGamePortChecks`, `ResolveGamePortToggle`, the `BuildNodes` parameters),
  `SettingsUiPrefs::gamePortAdapter`, `SettingsPanelState::SetGamePortAdapter`
  and `ObserveLiveGamePortAdapter`, `ISettingsApplySink::ApplyGamePortAdapter`,
  and the `OnDialogTick` observation. `SettingsMachineInfo::supportsGamePortAdapter`
  stays and now gates the Controllers page section. The two
  `IDM_GAMEPORT_ADAPTER_*` commands stay, now posted by the Controllers page,
  and route to `SetGamePortAdapter` (live and saved) instead of the live-only
  path, since nothing else saves the setting any more.
- **Rationale**: the spec moves the setting to the Controllers page (FR-001)
  and makes every change immediate (FR-002). Out of the sheet's snapshot, the
  edge case "pressing OK does not undo it" holds by construction, which is why
  R11's re-seeding goes.
- **Removal is not deletion of library code**: every item removed above is
  Casso's own Settings plumbing for this one setting, not Dxui library code.

## R16. The //c reads the Joyport as off and keeps the setting

> **Superseded by R27 (2026-09-28).** `JoyportSetting::IsInEffect` is
> replaced by `PlayerModeRules::IsJoyportOn`; on the //c a Joyport mode
> plays as Joystick and is kept.

- **Decision**: one pure rule,
  `JoyportSetting::IsInEffect (GamePortAdapter setting, bool hasAnnunciators)`,
  true only for the Joyport on a machine with annunciators. Every consumer calls
  it: `SiriusJoyport::SetAttached` on a machine switch and at launch,
  `ControllerInputService::SetJoyportAttached` (so the //c plays normal-mode
  profiles), the picker row's `isOffered` and `isOn`, the Controllers page
  section and its profile mode, and the player labels (R22). The global value
  itself is never rewritten by a machine switch.
- **Rationale**: the //c builds no Joyport (R3), so the device half already
  holds; what changes is that the service, the picker and the page must stop
  reading the saved value directly. One predicate keeps the five consumers from
  disagreeing, and switching back to the //e finds the setting as it was left
  (US4 scenario 4).
- **Machine switch with a controller held** (spec edge case): unchanged from
  R7; the //c has no Joyport, so its reads fall straight through.

## R17. Each profile belongs to one mode, fixed at creation

> **Superseded 2026-09-27**: the two modes Normal and Joyport below were
> replaced by three kinds, described in R25. The rules for fixing the kind at
> creation, resetting to the kind's built-in and listing by kind carry over.

- **Decision**: `ControllerProfile` gains `ProfileMode mode = ProfileMode::Normal`.
  - Built-in profiles take their mode from their kind: Default is Normal and
    Joyport is Joyport, set by `EnsureBuiltInProfiles` and on load whatever the
    file holds.
  - A user profile's mode is saved as `"profileMode": "joyport"` in its profile
    object, omitted for Normal. A profile with no such member, which is every
    profile saved before this change, is Normal (FR-020). An unknown value
    reads as Normal rather than dropping the profile, since the mapping itself
    is sound.
  - `ControllerProfileStore::CreateProfile` gains the mode in effect and stamps
    it on the new profile. `ProfileSource` gains `JoyportMapping`, so both
    modes offer the Default mapping, the Joyport mapping, Paddles, and a copy of
    any profile of either mode.
  - `ResetProfile` restores the built-in mapping of the profile's own mode: a
    user profile in Joyport mode resets to the Joyport mapping, not the
    Default's. `MakeBuiltInMapping`'s "a user profile resets to the Default's"
    becomes "to its mode's built-in".
  - Lists: `ControllerModelSettings::GetProfileNames (ProfileMode)` returns the
    mode's built-in profile first, then that mode's user profiles in stored
    order. The picker's `ProfileSection` and the Controllers page both build
    from it, which replaces `SetProfileSections`' rule of listing Default and
    Joyport first in every section.
  - Names: `CheckProfileName` already compares against every profile of the
    model, so names stay unique across both modes with no change (FR-020, spec
    034 FR-027). A test pins it.
  - Choices: an active-profile entry that points at a profile of the other mode is
    treated as unset, so the controller plays its mode's built-in profile, and
    the entry is dropped the next time the choices are saved. This is a
    silent normalization rather than a rejected entry: it was reachable only
    in development builds of this branch, and nothing the user made is lost.
- **Reverses part of `3f0c4620` and `c00c2c7a`**: those let any profile be
  picked in either mode, and `ChosenProfile_IsRememberedForEachMode` picks the
  Joyport profile in normal mode and the Default in Joyport mode. The test is
  rewritten to pick a normal-mode user profile and a Joyport-mode user profile;
  a new test pins that picking a profile of the other mode is rejected and
  leaves the choice unchanged.
- **Rationale**: FR-020. A field on the profile, rather than two profile lists
  per model, keeps one namespace for names, one store for mappings, and lets
  "copy of any profile of either mode" stay a lookup by name.
- **Alternatives considered**: deriving the mode from the active-profile map a
  profile was last picked in. Rejected; a profile never picked would have no
  mode, and FR-020 fixes the mode at creation.

## R18. The second stick on a DirectInput gamepad

- **Decision**: `DefaultMapping::MakeJoyport` gains the device's
  `ControllerFormFactor` (from `ControllerDeviceInfo::formFactor`,
  `CassoEmuCore/Controllers/ControllerTypes.h`), and so do
  `MakeBuiltInMapping`, `EnsureBuiltInProfiles`, `GetBuiltInSettings`,
  `GetOrCreateModel`, `CreateProfile` and `ResetProfile`, as an explicit
  parameter beside the model with no default, so the compiler finds every
  caller. For `ControllerKind::DirectInput` with form factor `Gamepad`, a
  pure `DefaultMapping::FindSecondStick (formFactor, controls)` picks:
  - Z (axis 2) and Rz (axis 5) when the device reports both;
  - otherwise Rx (axis 3) and Ry (axis 4) when it reports both;
  - otherwise none.
  The pair is added as Absolute bindings on PDL0 and PDL1 after the primary
  stick, as the XInput right stick is. Joysticks and wheels are unchanged:
  primary stick, D-pad, every button (FR-017).
- **Axis order**: `DirectInputSampleDecoder::Decode` fills `sample.axes` as
  x, y, z, rx, ry, rz, slider 0, slider 1, and `ControlId` indexes axes the
  same way (0-7), so the indexes above are the decoder's own.
- **Triggers**: XInput triggers are `ControlKind::Trigger` and are only ever
  bound to PB0. DirectInput has no trigger kind (the decoder's `ListControls`
  comment), so a DirectInput trigger is an axis Casso cannot tell apart from a
  stick. The Z/Rz-before-Rx/Ry order is what keeps them off the switches on
  the common layouts: a pad reporting Z and Rz uses them for its right stick
  and Rx/Ry for its triggers, and an Xbox-class pad read through DirectInput
  reports its triggers as one Z axis with no Rz, so the rule falls through to
  Rx/Ry for its right stick. A pad that breaks both conventions gets a wrong
  second stick, which the user can edit out of the profile.
- **Model versus unit**: profiles are per model and the form factor is per
  unit, but every unit of a DirectInput model has the same vendor and product
  and so the same `dwDevType`; the first unit seen decides the built-in
  mapping, as its controls list already does.
- **Existing profiles**: a Joyport profile already saved keeps its mapping
  (FR-017's "keeping its mapping" rule applies to user data); a user gets the
  second stick by resetting it.

## R19. The vertical switch is an orientation option on DxuiToggle

- **Decision**: `DxuiToggle` gains `enum class OnDirection { Right, Up, Down }`
  and `SetOnDirection`, default `Right` (today's horizontal pill). `Up` and
  `Down` lay the pill out vertically, with the thumb traveling toward that end
  when checked. The pill and thumb rectangles come from a pure static
  `ComputeTrackAndThumb (const RECT & pill, OnDirection, bool checked)`, so the
  geometry is tested without a painter; hit testing and keyboard handling are
  unchanged. The Joyport section uses `Down`, since the unit's front is toward
  the bottom and front is Atari mode (FR-001), and sets the label to "Atari
  mode" or "Apple mode" on each change. (Superseded 2026-09-27: the label
  beside the knob was replaced by fixed labels, "Apple (rear)" above the switch
  and "Atari (front)" below it, each centered on it side to side from the
  toggle's own geometry, and "Joyport" to its left, centered top to bottom.)
- **Rationale**: the spec's planning note puts the orientation on the existing
  toggle; a direction rather than a bool records which end is on, which a
  vertical switch needs and a horizontal one never did.
- **Alternatives considered**: a Joyport-specific view drawing the unit's
  switch. Rejected; it would duplicate focus, hover and accessibility handling
  the toggle already has.

## R20. One controller is never an analog joystick and an Atari stick at once

> **Amended by R27 (2026-09-28).** A player is one or the other. A player on
> Joystick or Paddle beside a Joyport player drives its paddle inputs with no
> buttons, so the paddle inputs read as no paddle connected only while every
> playing player is on a jack.

- **Decision**: unchanged mechanism, recorded because the spec now states it as
  a requirement (FR-009): while the Joyport is in effect, `IsDrivingPaddles`
  makes `$C064`-`$C067` read as no paddle connected (R3), controllers drive the
  switches, and the picker leaves mouse-as-paddle out. The picker rule is a
  pure predicate the picker consults, so spec 034's Player 1 submenu does not
  have to know about the Joyport.
- **Rationale**: the paddle inputs and the Joyport's switches are at different
  addresses, but both kinds of fire button are read at `$C061`-`$C063` with
  opposite polarity: an Apple button reads bit 7 set when pressed, an Atari
  switch bit 7 clear when closed. With both offered, a joystick game would
  steer from the paddles and read fire inverted, and a game that probes the
  paddle inputs to detect a joystick could pick the wrong controller. The
  hardware's Apple / Atari switch never connects both either.

## R21. Which controller drives which jack, from spec 034's players

> **Amended by R27 (2026-09-28).** The jacks come from the players' resolved
> modes, not from Player 1 left and Player 2 right; `isPlayer2Disabled` and
> the Same as left row are gone.

- **Decision**: a pure `JoyportJackRules::AssignJacks (const JoyportPlayers &)`
  returns, for each jack, which player's switches it carries. Its input is a
  reduction of spec 034's `PlayerSlotState` (034 data-model, "Players
  (2026-09-27)") to three states per player, plus whether Player 2 is
  Disabled:

  | 034 slot state | Joyport state |
  |---|---|
  | `Playing`, `Provisional`, or Player 1 on the arrow keys | Driving |
  | `Held` | Held |
  | `Empty`, `Waiting`, Player 1 on mouse-as-paddle | Idle |

  | Player 1 | Player 2 | Left jack | Right jack |
  |---|---|---|---|
  | Driving | Idle | Player 1 | Player 1 |
  | Idle | Driving | Player 2 | Player 2 |
  | Driving | Disabled (Same as left) | Player 1 | Player 1 |
  | Driving | Driving | Player 1 | Player 2 |
  | Held | Driving | open | Player 2 |
  | Driving | Held | Player 1 | open |
  | anything else | | open | open |

  The first two rows are FR-008's "one controller playing alone", which
  matches 034's `IsOnePlaying`. A Held slot is how 034 keeps a leaver's place
  (its FR-040), so the split survives their absence and the remaining player
  does not regain the other jack. When 034's Automatic starts over, both slots
  go Idle and the first player again drives both. Slot targets play no part
  (US3 scenario 3). `ControllerInputService::AddJoyportSwitches` is rewritten to
  place each driver by this assignment instead of by `std::optional<size_t>
  player`.
- **Keyboard**: the arrow keys are one of Player 1's entries in spec 034's
  picker. When Player 1 is the arrow keys and Player 2 holds a controller, the
  left jack carries the keys and the right jack the controller; the mixer
  composes the jacks from the assignment instead of from the axis owner alone
  (R6's owner table stands for the non-Joyport paddles and buttons).
- **Rationale**: FR-008 as clarified. Deriving from the slot holders rather
  than from recent input makes the result deterministic and testable with
  `FakeControllerBackend`.
- **Dependency**: spec 034's player state and Automatic (its FR-040 to
  FR-042). The rule's tests use a plain `JoyportPlayers` value, so they do not
  wait on 034; wiring it into the service does.

## R22. Joyport labels in the picker and the notices

> **Superseded by R27 (2026-09-28).** `JoyportLabels` is removed; the players
> keep Player 1 and Player 2, and `PlayerModeRules` gives the mode labels and
> the notice text.

- **Decision**: a pure `JoyportLabels` helper (in `CassoEmuCore/Controllers/`)
  returns the player row label, Player 2's Disabled entry label, the Automatic
  row text and the notice text for a given player and `IsInEffect` state:
  - off: whatever spec 034 shows (Player 1, Player 2, Disabled);
  - on: "Joyport left", "Joyport right", "Same as left"; Joyport right on
    Automatic with no holder reads "Joyport right: same as left"; the notice
    for a controller playing alone reads "Joyport left and right:
    description", and otherwise "Joyport left: description" or "Joyport
    right: description" (FR-019).
  - (2026-09-27) Player 2's Disabled entry reads "Same as left" as the
    submenu entry, and "same as left" in lower case wherever it follows a
    colon: the row "Joyport right: same as left" and the Controllers page's
    Player 2 drop-down, which follows "Player 2:". The page's per-player note
    reads "left jack", "right jack" or "both jacks" in Atari mode.
  Spec 034's picker and notice code calls it where it now builds those
  strings.
- **Rationale**: one place for the strings keeps the picker and the notices
  from drifting, and the rule is testable as plain data.
- **Dependency**: spec 034's player submenus and notice stack (its FR-008 and
  FR-044). The helper and its tests do not wait; the hookup does.

## R23. The Controllers page's Joyport section and per-mode lists

> **Amended by R27 (2026-09-28).** The Joyport section and its switch are
> removed; the per-kind lists follow each player's own mode.

- **Decision**: the page gains a **Joyport** section, above the controller
  list, shown only when `SettingsMachineInfo::supportsGamePortAdapter` is true
  (not the //c). It holds the vertical toggle (R19). Moving it calls the shell
  at once (R15), then `ControllersPageState::SetProfileMode` with the new
  mode, which now reloads the profile list and the edited profile in place:
  the edited controller switches to its choice for the new mode, as
  `SelectProfile` does today, so a pending edit is handled exactly as when the
  user picks another profile. The stick art (R12) replaces the stick and
  button lights while the mode is Joyport.
- **Rationale**: FR-001 and FR-020 ("moving the Joyport switch on the
  Controllers page MUST swap the list in place"). `SetProfileMode` today only
  takes effect before `Load`, which is why it needs the reload.
- **Mouse-as-paddle** is not on this page, so nothing here hides it (R20).
- **Layout (2026-09-27)**: "Apple (rear)" above the switch and "Atari (front)"
  below it, each centered on it side to side, and "Joyport" to its left,
  centered on it top to bottom (R19). The heading above the switch lights reads
  "Atari joystick", followed by ": left jack", ": right jack" or ": both jacks"
  for the controller in Editing, with no suffix when there is no controller.
  (Superseded 2026-09-27: the heading "Joyport: left jack" was replaced by
  "Atari joystick: left jack".) The Multiplayer checkbox and its slide are gone
  from the page (spec 034). While the Joyport is in effect each player's mode
  drop-down is disabled and shows Joystick (R26).
- (Superseded 2026-09-27: `SetProfileMode` was replaced by
  `ControllersPageState::SetJoyportInEffect (bool)`; the profile kind the page
  edits follows it and the edited player's mode, R25.)

## R25. Three profile kinds: Joystick, Paddle, Joyport (2026-09-27)

- **Decision**: `ProfileMode { Joystick, Paddle, Joyport }` replaces Normal
  and Joyport. Built-ins: Default (Joystick), Paddles (Paddle, the new
  `ControllerProfileKind::Paddles` from `DefaultMapping::MakePaddles`) and
  Joyport (Joyport).
  - `ProfileSource::Paddles` became `ProfileSource::PaddleMapping`, the Paddle
    kind's built-in mapping, shown as "Paddles mapping" in the New profile
    dialog. Starting points per kind: Joystick, the Default mapping or a copy of
    a Joystick profile; Paddle, the Paddles mapping or a copy of a Paddle
    profile; Joyport, the Joyport mapping or a copy of a Joyport profile.
    (Superseded: Paddles as a starting point for a normal-mode profile, and
    copies across modes.)
  - Every profile is saved with `"profileMode"` set to `"joystick"`,
    `"paddle"` or `"joyport"`. A profile saved without one is classified: PDL0
    bound and PDL1 not is Paddle, anything else Joystick; `"joyport"` stays
    Joyport. Built-ins take their kind; the built-in Paddles is saved with
    `"paddles": true`.
  - A controller keeps a chosen profile per kind: `activeProfiles` (Joystick,
    the old key kept), `paddleActiveProfiles` and `joyportActiveProfiles`. A
    legacy Joystick choice of a profile that now classifies as Paddle moves to
    the Paddle map on read.
- **Rationale**: spec 034 gives each player a Joystick or Paddle mode, and a
  profile list per player that follows that mode; spec 036 FR-020.

## R26. Player modes while the Joyport is in effect (2026-09-27)

> **Superseded by R27 (2026-09-28).** The Joyport jacks are modes; the mode
> list is never disabled for the Joyport, and only a Joyport player plays its
> Joyport-kind choice.

- **Decision**: while the Joyport is in effect both players are Atari sticks.
  `PlayerSlotPolicy::GetEffectiveMode` reports Joystick for both, every
  controller plays its Joyport-kind choice, and the mode cannot be chosen: the
  picker's Joystick/Paddle pair and the Controllers page's mode drop-down are
  disabled and show Joystick. The saved mode is kept for when the Joyport is
  turned off.
- **Mouse and keys**: mouse-as-paddle is offered only in Paddle mode and never
  with the Joyport, so the mouse and the Joyport are never on together (R20).
  The keys are offered with the Joyport on in either saved mode.

## R24. The Controller Select switch: deferred

> **Closed 2026-09-28 (R27).** FR-016 is closed as not needed; the tasks of
> the optional phase are marked dropped.

- **Decision**: not planned. FR-016 carries an open clarification marker that
  the owner has not resolved, so the emulator keeps the switch at Center, as
  the `SiriusJoyport` contract and its tests already do. Nothing in R14-R23
  depends on it.
- **If adopted**, the shape it would take, recorded so the decision can be
  made against it and not as a commitment: a global `joyportControllerSelect`
  token (`left`, `center`, `right`) beside `gamePortAdapter` (R14); a
  `SiriusJoyport::SetControllerSelect` read on each `TryReadButton`, where Left
  or Right fixes the jack whatever AN0 selects; a three-position Dxui control in
  the toggle's style, disabled in Apple mode (spec planning notes); and the
  manual's test program as the check for SC-008. The earlier pass through that
  program closed switches on both jacks for its one-stick sections, which is
  why it passed without this switch (validation.md).

## R27. A Joyport mode for each player (2026-09-28)

- **Decision**: the global Apple / Atari setting goes. `PlayerMode` gains
  `JoyportLeft`, `JoyportRight` and `SameAsPlayer1`, in the order `Joystick`,
  `JoyportLeft`, `JoyportRight`, `Paddle`, `SameAsPlayer1`, saved as
  `"joystick"`, `"joyportLeft"`, `"joyportRight"`, `"paddle"` and
  `"sameAsPlayer1"`; a Player 2 with no saved mode is Same as Player 1. A new
  pure `PlayerModeRules` (`CassoEmuCore/Controllers/`) replaces
  `JoyportSetting` and `JoyportLabels`:
  - `ResolveMode` gives Player 1's mode for Same as Player 1, or the other
    jack when Player 1 is on one; on a machine with no Joyport a jack plays as
    Joystick.
  - `IsJoyportOn` is true while any player's resolved mode is a jack on a
    machine that has a Joyport; `GetJack` gives a mode's jack;
    `IsModeTaken` reports a jack the other player's resolved mode holds (a
    Disabled Player 2 holds none).
  - `AreButtonsCut` is true for a player off the Joyport while the other is on
    it; `ArePaddlesConnected` is false only while every playing player is on a
    jack.
  - `AreKeysOffered` (Player 1 in Joystick or a Joyport mode) and
    `IsMouseOffered` (Player 1 in Paddle mode).
  - `BuildModeChoices` lists the modes with checked and enabled flags;
    `GetModeLabel` and `GetPlayerLabel` give the labels; `DescribeAssignment`
    gives "Player N: description", or with the jacks it drives "Player N
    (Joyport left): description", "(Joyport right)" or "(Joyport left and
    right)".
  - `MigrateAdapter` reads the old global `gamePortAdapter`, or the launched
    machine's value where the global one was never set, and `ApplyMigration`
    puts Player 1 on Joyport left and Player 2 on Same as Player 1 when it was
    the Joyport. The caller then writes the global key as `"none"`, which only
    marks the migration as done, so no machine's old value is read again.
- **Jacks**: `JoyportJackRules::ReducePlayers` gives each player its state
  (`Driving`, `Held`, `Idle`) and the jack its resolved mode holds, if any;
  `AssignJacks` puts each Driving player on its jack and also on the other jack
  while that jack is free (no player's mode on it, or its player Idle). A Held
  player's jack reads open and is not handed to the other player.
  `isPlayer2Disabled` is gone.
- **Service and shell**: `ControllerInputService::SetJoyportAvailable` takes
  whether the machine has a Joyport (it replaces `SetJoyportAttached`), and
  the service derives whether it is on from the entries. After every entry
  change the shell attaches the machine's Joyport when `IsJoyportOn` and
  connects or disconnects its paddle inputs from `ArePaddlesConnected`. Each
  controller plays the profile kind of its own player's resolved mode.
- **Mixer**: the keys-to-jacks fallback follows the jacks the arrow-keys
  contribution marks (`keyJacks`), not the axis owner. The arrow keys own
  PDL0 and PDL1 only while Player 1 is not on a Joyport mode, and the mouse
  owns PDL0 only while Player 2 plays (spec 034 research R27).
- **UI**: the picker's Joyport row, its menu command
  (`IDM_GAMEPORT_ADAPTER_JOYPORT`), `EmulatorShell::SetGamePortAdapter` /
  `GetGamePortAdapter` and the Controllers page's switch section are removed.
  Each player's submenu and row list the modes; a taken jack is disabled,
  which on the page needs per-item enabled flags on `DxuiComboBox`. A player
  whose buttons are cut has its button binding rows disabled and, under its
  row, Dxui's warning badge with "This controller's buttons are disabled
  because Player N is using the Joyport." `DxuiToggle` keeps its orientation
  option and label-visibility API as library code.
- **FR-016**: closed as not needed. The manual's test program picks a jack for
  its one-stick sections with the Controller Select switch; putting a player
  on that jack does the same.
- **Rationale**: the switch made both players Atari sticks at once, which ruled
  out a player on a rear-socket joystick or paddle beside an Atari stick, and
  took one control on the page and one row in the picker to express what a
  player's mode already carries.
- **Alternatives considered**: keeping the switch and adding a jack choice per
  player. Rejected; two controls would then set whether the Joyport is on.
