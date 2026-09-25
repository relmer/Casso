# Contract: The adapter setting, the picker row, the Machine tab and the Controllers page

Covers FR-001, FR-002, FR-012, FR-015 and User Stories 4 and 5.

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
void            SetGamePortAdapter (GamePortAdapter adapter);   // UI thread
GamePortAdapter GetGamePortAdapter () const;
```

`SetGamePortAdapter` stores the value, calls `SetAttached` on the live
machine's Joyport under the shared lifetime lock, persists it through
`DiskSettings::WriteSavedUiPrefs`, and calls `SyncSelectorState` so the picker
redraws. No reset (FR-002). It is the one path both the picker and the
`IDM_GAMEPORT_ADAPTER_*` commands take.

## Picker row

- `EmulatorCommands::SetJoyportFns (isOn, isOffered, toggle)`, one checkable
  `DxuiCommand` labeled **Sirius Joyport**.
- `GetPaddlePickerItems` places it after the source rows and before the
  separator above Multiplayer, and only when `isOffered()` (the machine has
  annunciators: not the //c).
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
- `SetOnToggle` routes either label to
  `SettingsPanelState::SetGamePortAdapter`; a toggle that would leave neither
  checked is ignored.
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
  jacks".
- Detached: the page is unchanged (FR-015).
- Tests: `ControllersPageStateTests.cpp` for `GetJoyportJack` in each mode and
  after Editing moves to player 2.
