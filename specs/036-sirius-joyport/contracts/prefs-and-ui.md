# Contract: The adapter setting, the picker row, the Machine tab and the Controllers page

Covers FR-001, FR-002, FR-012, FR-015 and User Stories 4 and 5.

> **2026-09-27 (GH #156)**: the setting became global, the Machine tab entry
> was removed, and the Controllers page gained the Apple / Atari switch and
> per-mode profile lists. The sections below the rule are the 2026-09-24
> contract as built; where the 2026-09-27 section at the end differs, it wins.
>
> **2026-09-28**: the global setting, the Apple / Atari switch and the
> picker's Joyport row are removed; each player's mode puts it on a jack. The
> 2026-09-28 section at the very end wins over both sections above it.

## Pref

```jsonc
"machines": {
  "Apple2e": {
    "$cassoUiPrefs": {
      "gamePortAdapter": "siriusJoyport"   // omitted when "none"
    }
  }
}
```

- The read and write rules are pure helpers on `MachineInputPrefs`, where the
  other per-machine input keys live, so they are tested in
  `MachineInputPrefsTests.cpp`:

  ```cpp
  static constexpr const char *  kpszGamePortAdapterKey = "gamePortAdapter";

  static GamePortAdapter                    ReadGamePortAdapter       (const JsonValue * uiPrefs,
                                                                       bool              hasAnnunciators);
  static std::pair<std::string, JsonValue>  BuildGamePortAdapterEntry (GamePortAdapter adapter);
  ```

- Read at cold boot (`EmulatorShell::ApplyPersistedChromePrefs`) and on machine
  switch (`MachineManager::SwitchMachine`, beside `mouseConnected`), then
  applied to the new machine's Joyport before its `PowerCycle`. Neither call
  site is reachable from a unit test, so each is a one-line forwarder to
  `ReadGamePortAdapter`.
- Ignored on a machine without `hasAnnunciators`, and never written for one.
- `UserConfigStore::BuildUiPrefsDefaults` gains `"none"`, so `SaveDelta` drops
  the default.

## EmulatorShell

```cpp
void            SetGamePortAdapter       (GamePortAdapter adapter);   // UI thread: live + save
void            ApplyGamePortAdapterLive (GamePortAdapter adapter);   // UI thread: live only
GamePortAdapter GetGamePortAdapter       () const;                    // read from the Joyport itself
void            AdoptGamePortAdapterForMachine (const JsonValue * uiPrefs);
```

`ApplyGamePortAdapterLive` calls `SetAttached` on the live machine's Joyport
under the shared lifetime lock, resubmits the fire keys, and calls
`SyncSelectorState` so the picker redraws. No reset (FR-002).
`SetGamePortAdapter` (the picker) does that and saves through
`DiskSettings::WriteSavedUiPrefs`. The `IDM_GAMEPORT_ADAPTER_*` commands (the
Settings OK) take the live-only path, because the sheet saves the machine's
block itself and a second save could land on another machine when the same OK
switches machines. `GetGamePortAdapter` reads the Joyport's attached state, so
it cannot drift from what the guest reads. `AdoptGamePortAdapterForMachine`
attaches a newly built machine's Joyport from its saved setting: at cold boot
from `ApplyPersistedChromePrefs`, and on a machine switch right after
`BuildMachineDevices`.

## Picker row

- `EmulatorCommands::SetJoyportFns (isOn, isOffered, toggle)`, one checkable
  `DxuiCommand` labeled **Sirius Joyport**.
- `GetPaddlePickerItems` places it in a group of its own, below the players
  and above Profiles and Controller settings, and only when `isOffered()` (the
  machine has annunciators: not the //c). It is a device on the game port, not
  a source, so it is kept out of the source group.
- `isOn` = `GetGamePortAdapter() == SiriusJoyport` (FR-012).
- Tests: `PaddleSourceRowsTests.cpp`: present and checked/unchecked on the
  //e, absent on the //c, the toggle calls back once, position in the list.

## Commands

`IDM_GAMEPORT_ADAPTER_NONE` and `IDM_GAMEPORT_ADAPTER_JOYPORT` in
`resource.h`, routed to the UI thread by `WindowCommandManager::GetCommandRoute`
(like `IDM_MOUSE_CONNECT`), and added to `ChromeCommandRoutingTests.cpp`'s
table.

## Machine tab

- `HardwarePage::BuildNodes` gains `bool supportsGamePortAdapter` and
  `GamePortAdapter adapter`. When supported, it appends a **Game port** group
  with two rows, **None** and **Sirius Joyport**, exactly one checked.
- `SetOnToggle` routes either label through the pure
  `ResolveGamePortToggle` to `SettingsPanelState::SetGamePortAdapter`, and
  re-checks both rows in place with `SetGamePortChecks` (not a `Rebuild`, which
  would replace the running handler). A toggle that would leave neither
  checked is ignored. Whether the group appears comes from
  `SettingsMachineInfo::supportsGamePortAdapter`, set from the machine
  definition's `hasAnnunciators`.
- `SettingsPanelState`: `gamePortAdapter` in `ExtractUiPrefs`, `BuildJson`,
  `ArePrefsEqual`; `Apply` calls `sink.ApplyGamePortAdapter` and never
  `QueueMachineReset` for it.
- `ObserveLiveGamePortAdapter (GamePortAdapter live)`, called each dialog
  tick: if `live` differs from the last observed value, set
  `m_original.prefs.gamePortAdapter = live`, and `m_current`'s too when it
  equaled the old original. Returns whether the tree needs rebuilding.
- Tests: `HardwarePageTests.cpp` (group present on the ][+ and //e, absent on
  the //c, one row checked); `SettingsPanelStateTests.cpp` (round trip, pushes
  live with no reset, `RecordingSink` gains the method, a live change while
  open is kept on OK and not dirty, a pending user edit still wins).

## Controllers page

- `ControllersPage::SetJoyportAttachedFn (std::function<bool()>)`, wired by
  `SettingsSheet` to the shell.
- Attached: the Joystick and Buttons headings, `m_stick` and `m_lights` are
  hidden; a **Joyport** heading, a jack caption and five `ButtonLightView`s
  (Up, Down, Left, Right, Fire) are shown and lit from
  `ComputeLiveReading(sample).switches`. Relayout on any change of the attach
  state.
- `ControllersPageState::GetJoyportJack()` returns `Left`, `Right` or `Both`
  for the controller in Editing: `Both` in single-source mode, otherwise the
  slot it occupies. The caption reads "Left jack", "Right jack" or "Both
  jacks". (Superseded 2026-09-27: the **Joyport** heading and caption became
  one heading, "Atari joystick: left jack", ": right jack" or ": both jacks",
  from `ControllersPageState::GetJoyportHeading`; see the Controllers page
  section below.)
- Detached: the page is unchanged (FR-015).
- Tests: `ControllersPageStateTests.cpp` for `GetJoyportJack` in each mode and
  after Editing moves to player 2.

---

## 2026-09-27: global setting, Apple / Atari switch, per-mode profiles

Research R14-R23.

### Global pref

```jsonc
{
  "gamePortAdapter": "siriusJoyport",   // global section; "none" for Apple mode
  "machines": {
    "Apple2e": {
      "$cassoUiPrefs": {
        "gamePortAdapter": "siriusJoyport"   // legacy: read once for adoption, never written again
      }
    }
  }
}
```

- `GlobalUserPrefs::gamePortAdapter` round-trips through `ToJson` / `FromJson`;
  empty (key absent) means never set. Tested in `GlobalUserPrefsTests.cpp`.
- At cold boot the shell calls `JoyportSetting::ResolveAtLaunch` with the
  global token and the launched machine's `$cassoUiPrefs`. When `isAdopted`,
  it writes the global value at once through `SaveGlobalPrefs`, so later
  launches do not adopt again. A machine switch reads no pref.
- `PersistGamePortAdapterForMachine` is removed; the machine-switch call of
  `AdoptGamePortAdapterForMachine` becomes "apply the global setting to the new
  machine's Joyport through `IsInEffect`".

### EmulatorShell

```cpp
void             SetGamePortAdapter (GamePortAdapter adapter);   // UI thread: live + global save
GamePortAdapter  GetGamePortAdapter () const;                    // the global setting, not the machine's state
bool             IsJoyportInEffect  () const;                    // JoyportSetting::IsInEffect for the running machine
```

- `SetGamePortAdapter` stores the setting, applies `IsInEffect` to the live
  machine's Joyport (under the shared lifetime lock) and to
  `ControllerInputService::SetJoyportAttached`, resubmits the fire keys, saves
  the global prefs, and calls `SyncSelectorState`. It never resets the
  machine.
- `ApplyGamePortAdapterLive` is removed with the Settings apply path; both
  `IDM_GAMEPORT_ADAPTER_*` commands route to `SetGamePortAdapter`.
- `GetGamePortAdapter` now returns the setting, which on the //c differs from
  what the machine reads; callers that want the latter use
  `IsJoyportInEffect`.

### Picker row

- Label **Joyport (Atari mode)**. Placement as before.
- `isOffered` = the running machine has annunciators; `isOn` =
  `IsJoyportInEffect()` (FR-012). On the //c the row is absent, so its check
  state never shows the saved setting.
- Mouse-as-paddle is left out of the picker while `IsJoyportInEffect()`
  (FR-009). The predicate is `JoyportSetting::IsMousePaddleOffered (bool
  isJoyportInEffect)`; spec 034's Player 1 submenu consults it.
- The profile sections list `GetProfileNames (mode)` for the mode in effect
  (FR-020), replacing "Default and Joyport lead every section".
- Player labels, Player 2's Disabled entry and the Automatic row text come from
  `JoyportLabels` (R22) once spec 034's player submenus exist.
- (2026-09-27) With the Joyport on, the rows read "Joyport left: ..." and
  "Joyport right: ..."; Player 2's Disabled entry is "Same as left" in the
  submenu, and the row reads "Joyport right: same as left" in lower case after
  the colon. Each player's Joystick/Paddle pair is disabled and shows Joystick,
  and every profile list holds Joyport-kind profiles. Mouse-as-paddle is
  offered only in Paddle mode, so never with the Joyport; the keys are offered
  with the Joyport on in either saved mode.
- (Superseded 2026-09-27: "the mode in effect" above is now the player's
  effective mode, `PlayerSlotPolicy::GetEffectiveMode`, which gives one of three
  profile kinds, Joystick, Paddle or Joyport.)

### Machine tab

- The Game port group is removed. `HardwarePage::BuildNodes` loses its
  `supportsGamePortAdapter` and `gamePortAdapter` parameters, and
  `HardwarePageTests.cpp` asserts that no machine lists the Joyport.
- `SettingsUiPrefs::gamePortAdapter`, `SetGamePortAdapter`,
  `ObserveLiveGamePortAdapter` and `ISettingsApplySink::ApplyGamePortAdapter`
  are removed, with their tests; `SettingsSheet::OnDialogTick` no longer
  observes the setting.

### Controllers page

- `ControllersPage::SetJoyportFns (std::function<bool()> isOn,
  std::function<bool()> isOffered, std::function<void (bool)> set)` replaces
  `SetJoyportAttachedFn`. `SettingsSheet` wires `isOn` to
  `IsJoyportInEffect`, `isOffered` to `SettingsMachineInfo::supportsGamePortAdapter`,
  and `set` to posting `IDM_GAMEPORT_ADAPTER_JOYPORT` or `_NONE`.
- A **Joyport** section heads the page when offered: a `DxuiToggle` with
  `OnDirection::Down`, checked for Atari mode, labeled "Atari mode" or "Apple
  mode" by its state. Not offered (the //c): no section, profile mode Normal.
  (Superseded 2026-09-27: the state label was replaced by "Apple (rear)" above
  the switch and "Atari (front)" below it, each centered on it side to side
  from the toggle's own geometry, and "Joyport" to its left, centered on it
  top to bottom. On the //c each player plays its own mode's profile kind.)
- The heading above the switch lights reads "Atari joystick", followed by
  ": left jack", ": right jack" or ": both jacks" for the controller in
  Editing, and has no suffix with no controller. (Superseded 2026-09-27: the
  heading "Joyport: left jack".)
- The per-player note reads "left jack", "right jack" or "both jacks" in
  Atari mode. Each player's mode drop-down is disabled and shows Joystick while
  the Joyport is in effect. Player 2's Disabled entry reads "same as left", in
  lower case since it follows "Player 2:".
- The Multiplayer checkbox and its slide are removed from the page (spec 034).
- On a change, the page calls `set`, then
  `ControllersPageState::SetProfileMode (mode)`, which reloads the profile list
  and the edited profile for the new mode in place, the same way
  `SelectProfile` switches profiles. (Superseded 2026-09-27:
  `ControllersPageState::SetJoyportInEffect (bool)` replaced `SetProfileMode`;
  the kind the page edits follows it and the edited player's mode.) The stick art (`JoyportSwitchView`)
  replaces the stick and button lights while the mode is Joyport, as before.
- A change made from the picker while the sheet is open is picked up on the
  page's next poll by comparing `isOn()` with the toggle, and handled like a
  change on the page itself, minus the `set` call.
- Profile creation offers **Default mapping**, **Joyport mapping**,
  **Paddles** and **Copy of** any profile of either mode, in both modes; the
  new profile takes the page's mode. (Superseded 2026-09-27: each kind has
  its own starting points. Joystick: **Default mapping** or a copy of a
  Joystick profile. Paddle: **Paddles mapping** (`ProfileSource::PaddleMapping`)
  or a copy of a Paddle profile. Joyport: **Joyport mapping** or a copy of a
  Joyport profile.)
- Storage (2026-09-27): every profile is saved with `"profileMode"` set to
  `"joystick"`, `"paddle"` or `"joyport"`, and a controller's choices are kept
  per kind in `activeProfiles` (Joystick), `paddleActiveProfiles` and
  `joyportActiveProfiles`.
- Tests: `ControllersPageStateTests.cpp` (the list per mode, built-in first;
  swapping in place; create stamps the mode; reset restores the mode's
  built-in; a copy from the other mode); the page's section visibility through
  `SettingsMachineInfo` in `SettingsPanelStateTests.cpp` or the page-state
  tests, whichever holds the flag.

### Controller Select (FR-016)

Not part of this contract. Deferred pending the owner's decision (R24).

---

## 2026-09-28: per-player Joyport modes

Research R27. Replaces the global pref, the `EmulatorShell` functions, the
picker row, the commands and the Controllers page's Joyport section above.

### Prefs

```jsonc
{
  "controllers": {
    "players": [
      { "entry": "automatic", "mode": "joyportLeft" },
      { "entry": "automatic", "mode": "sameAsPlayer1" }
    ]
  }
}
```

- Mode tokens: `"joystick"`, `"joyportLeft"`, `"joyportRight"`, `"paddle"`,
  `"twoPaddles"` (2026-09-28, later), `"sameAsPlayer1"` (spec 034
  contracts/prefs-schema.md). A Player 2 with no
  saved mode key is Same as Player 1. An unrecognized token reads as
  Joystick, and `"sameAsPlayer1"` on Player 1 reads as Joystick.
- Migration at cold boot, `PlayerModeRules::MigrateAdapter (hasSavedModes,
  globalToken, launchedUiPrefs, launchedHasAnnunciators)`, only while no
  player has a saved mode: a global `"siriusJoyport"`, or
  an empty global token and the launched machine's `"siriusJoyport"`, gives
  `isJoyport`, and the shell applies `ApplyMigration` (Player 1 on
  `joyportLeft`, Player 2 on `sameAsPlayer1`). The shell saves the players'
  modes, which marks the migration done, and whenever `shouldRemoveKey`
  clears the global token, which removes the key on save. The per-machine
  key is never written or removed.

### EmulatorShell

- Removed: `SetGamePortAdapter`, `GetGamePortAdapter`, `IsJoyportInEffect`'s
  use of the setting, and `IDM_GAMEPORT_ADAPTER_JOYPORT` with its route and
  its row in `ChromeCommandRoutingTests.cpp`.
- `EmulatorShell::ApplyJoyportToMachine` (at machine build, and after each
  mode change through `SyncJoyport` on the UI thread) and
  `MigrateJoyportAtLaunch` replace `ApplyGamePortAdapterToMachine` and
  `ResolveGamePortAdapterAtLaunch`.
- After every change to the players' entries (a pick, a mode, a start-over,
  a machine switch), the shell attaches the machine's Joyport when
  `PlayerModeRules::IsJoyportOn`, connects or disconnects its paddle inputs
  from `ArePaddlesConnected` (`SiriusJoyport::SetPaddlesConnected`), and passes `ControllerInputService::SetJoyportAvailable`
  whether the machine has one. None of this resets the machine.

### Picker

- The Joyport row is removed.
- Each player's row reads "Player N: <what plays>", ending in " (paddle)" for a controller or Automatic in Paddle mode and " (two paddles)" in Two paddles mode; no other mode adds a suffix. Its submenu lists the
  modes from `PlayerModeRules::BuildModeChoices`, in the order Joystick,
  Joyport left (Atari), Joyport right (Atari), Paddle, Two paddles, with Same as Player 1
  first for Player 2; the resolved mode's entry is checked and a jack the
  other player holds is disabled. The //c lists no jack.
- Use keys as joystick is listed when `AreKeysOffered` (Player 1 in Joystick
  or a jack) and Use mouse as paddle when `IsMouseOffered` (Player 1 in
  Paddle or Two paddles); a checked one stays listed.
- The notice for a controller Automatic gave a player is
  `PlayerModeRules::DescribeAssignment`.

### Controllers page

- The Joyport section, its toggle and its labels are removed. `DxuiToggle`
  keeps `OnDirection` and its label-visibility API as library code.
- Each player's mode drop-down lists `BuildModeChoices`; a taken jack is a
  disabled item, through `DxuiComboBox`'s new per-item enabled flags
  (disabled items drawn in the disabled text color, skipped by the keyboard,
  not committed by a click).
- The note beside each row gives what the player drives: "left jack", "right
  jack", "both jacks", "joystick 0", "joystick 1", "paddle 0", "paddle 1" and
  so on.
- A player with `AreButtonsCut` has its button binding rows disabled, and
  under its row Dxui's warning badge with "This controller's buttons are
  disabled because Player N is using the Joyport.", N being the other player.
- The profile list follows the kind of the edited player's resolved mode.
  The switch lights show while the controller in Editing plays on a jack; a
  controller whose player is on Joystick or Paddle shows its stick and button
  lights.

### Tests

`PlayerModeRulesTests.cpp` (every function above, including the migration's
three cases), `PaddleSourceRowsTests.cpp` (mode rows, taken jacks, keys and
mouse offers, no Joyport row), `ControllersPageStateTests.cpp` and
`ControllersPageLayoutTests.cpp` (mode list, disabled jack, note, warning
notice, disabled button rows, no switch section), `DxuiComboBoxTests.cpp`
(disabled items), `MachineInputPrefsTests.cpp` or the store tests (mode
tokens round trip).
