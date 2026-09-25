# Research: Sirius Joyport Emulation

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Every decision below was checked against the tree at `d185c107`. The hardware
behavior comes from the Sirius Joyport owner's manual (archive.org
`siriusjoyportmanual`). No GPL emulator source was read; GSSquared's prose docs
disagree with the manual on AN0 and AN1, and the manual wins (spec Assumptions).

## R1. What the manual gives

- **Decision**: AN0 off (`$C058`) selects the left jack, on (`$C059`) the right.
  AN1 off (`$C05A`) selects left/right, on (`$C05B`) up/down. PB0 is fire, PB1
  left or up, PB2 right or down. A switch reads closed with bit 7 clear.
- **Rationale**: the manual's own tables, as quoted in the spec's Context. The
  manual also describes the Controller Select switch: Left and Right hold one
  jack whatever AN0 says, Center lets AN0 choose. Only Center is in scope.
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
  hardware.
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
  is computed from the **shaped** PDL0 and PDL1 deflections (after deadzone and
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
  - 0.5 is half the travel beyond the deadzone, which is well short of full
    deflection (FR-006, spec Assumptions).
- **Alternatives considered**: a threshold on the paddle byte (below 64, above
  191). Correct for Absolute bindings only; rejected because of Rate bindings.

## R6. Which controller drives which jack

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
- **Which source drives**: the existing axis owner decides. Controller owner:
  the Controller source's jacks. ArrowKeys owner: the keyboard-derived jacks.
  MousePaddle or None: all open, except that `FireKeys` fire still reaches both
  jacks under None, as it reaches PB0 today.

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
