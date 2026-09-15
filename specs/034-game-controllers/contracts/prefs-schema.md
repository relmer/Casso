# Contract: Controller Preferences Schema

**Feature**: `034-game-controllers` | **Data model**: [../data-model.md](../data-model.md)

## Global: `controllers` section of the global user prefs

Added as a known top-level key in `GlobalUserPrefs` (`CassoEmuCore/Config/GlobalUserPrefs.cpp`, `s_kKnownTopLevel`).

```json
{
  "controllers": {
    "models": {
      "xinput:045e:0b13": {
        "deadzone": 0.24,
        "profiles": [
          { "name": "Default", "default": true, "mapping": { "...": "..." } },
          { "name": "Lode Runner (D-pad)", "mapping": { "...": "..." } }
        ]
      }
    },
    "calibration": {
      "dinput:044f:b10a/{8E8A...}": {
        "mode": "user",
        "axes": [ { "index": 0, "center": 0.02, "min": -0.93, "max": 0.95 } ]
      }
    }
  }
}
```

### Mapping object

```json
{
  "pdl0": [ { "analog": "axis:0", "inverted": false, "response": "rate", "maxSpeed": 256 } ],
  "pdl1": [ { "negative": "dpad-up:0", "positive": "dpad-down:0" } ],
  "pb0":  [ { "control": "button:0" }, { "control": "trigger:1", "threshold": 0.12 } ],
  "pb1":  [ { "control": "axis:2", "threshold": 0.5, "negative": true } ],
  "pb2":  [ { "control": "button:4" } ]
}
```

- An axis binding has either `analog` or both `negative` and `positive`.
- `response` is `absolute` (default when absent) or `rate`; `maxSpeed` is read only for `rate` and clamped to [16, 1024].
- `pb2` may be absent; it is kept on load and save for every machine, and ignored at evaluation on the //c.
- `pdl2` and `pdl3` follow the same binding rules as `pdl0`. Absent means empty, and each is written only when it has a binding, so a two-axis mapping is written exactly as before. A profile with an unreadable `pdl2`/`pdl3` binding is rejected like any other.
- `ControlId` tokens are defined in the data model.

### Rules

| Rule | Behavior |
|---|---|
| Unknown keys inside `controllers` | Preserved on save (same passthrough guarantee as unknown top-level keys) |
| A model entry that fails validation | That model is rebuilt with only its Default profile; other models load; the failure is reported once through the shell notice |
| A profile that fails validation | That profile is dropped and reported; the rest of the model loads |
| Missing Default profile | Recreated from the default mapping |
| Duplicate profile names (case-insensitive) | Later duplicates dropped and reported |
| A calibration entry that fails the invariant | Entry dropped; the unit uses automatic calibration; reported |
| `deadzone` out of [0, 0.9] | Clamped |

## Per machine: `$cassoUiPrefs` block

Added to `MachineInputPrefs` (`CassoEmuCore/Config/MachineInputPrefs.h`) beside `arrowsToJoystick` and `pointerMapping`.

| Key | Value | Absent means |
|---|---|---|
| `controller` | Unit token, e.g. `xinput:045e:0b13` or `dinput:044f:b10a/{8E8A...}` | No controller selected |
| `controllerProfile` | Profile name | Default |
| `multiplayer` | `{ "enabled": <bool>, "players": [ <slot>, <slot> ] }` | Single-source mode: the selected controller drives PDL0/PDL1 and PB0-PB2, exactly as before this key existed |

```json
"multiplayer": {
  "enabled": true,
  "players": [
    { "controller": "xinput",                           "maps": "joystick0" },
    { "controller": "dinput:231d:0121/guid:{01661270}", "maps": "joystick1" }
  ]
}
```

- A selection is written as the unit token for DirectInput and the model token for XInput (FR-018a).
- `maps` is one of `joystick0` (PDL0/PDL1), `joystick1` (PDL2/PDL3), `paddle0`, `paddle1`, `paddle2`, `paddle3`. An unknown token reads as `joystick0`, which every machine with a game port can play.
- The block is always written, with both slots and with `enabled` false when the mode is off, because the block is spliced key by key and an omitted key would leave a setup the user turned off in the file. Absent and `"enabled": false` read the same.
- An absent block is the default: a machine with only `controller` saved behaves exactly as before (FR-037, FR-038).
- A slot with an unreadable or empty controller token, or a non-object slot, is left EMPTY rather than dropping the block: the other player keeps playing, and an empty slot is what the settings page shows for the one that could not be restored.
- The setup is normalized on load (FR-036): a second slot repeating the first slot's controller, or claiming a paddle it already holds, is emptied rather than played. A hand-edited file cannot put two players on one paddle.
- Targets naming paddles past the machine's count are kept on load and save and play nothing, so a //c keeps a //e's four-axis setup (FR-035).
- A pick from the command-bar paddle picker turns the mode off, keeping both slots.
- **`controllerAxes`, the short-lived per-axis assignment key, is gone.** It never shipped in a release, so it is neither read nor migrated; a file still carrying it keeps it as an unknown key and it has no effect.
- Selecting arrows-to-joystick or mouse-to-paddle removes `controller` (FR-008), which lets automatic selection apply at the next connect (FR-032).

## Unit-test obligations

- Round-trip of every field through `InMemoryFileSystem`.
- Each rule in the table above, including that a rejected entry is reported rather than silently replaced.
- Profile lookup by model token and name, the interface GH #78 will use.
