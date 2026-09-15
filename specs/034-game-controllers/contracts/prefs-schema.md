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
| `controllerAxes` | Array of `{ "controller": <unit token>, "axes": [<axis index 0-3>, ...] }` | No assignment: the selected controller holds PDL0 and PDL1, exactly as before this key existed |

```json
"controllerAxes": [
  { "controller": "xinput",                          "axes": [0] },
  { "controller": "dinput:231d:0121/guid:{01661270}", "axes": [1] }
]
```

- A selection is written as the unit token for DirectInput and the model token for XInput (FR-018a).
- `controllerAxes` is always written, as `[]` when there is no assignment, because the block is spliced key by key and an omitted key would leave a cleared assignment in the file. Absent and `[]` read the same.
- An empty array is the default: a machine with only `controller` saved behaves exactly as before (FR-038). The selected controller needs no entry; an entry for it replaces its PDL0/PDL1 default.
- Entries are applied in order with displacement (FR-036): an axis listed for two controllers goes to the later one. An entry with an unreadable token, no `axes` array, or a non-object is skipped; an axis index that is not a number in 0-3 is ignored.
- Axis indexes past the machine's count are kept on load and save and ignored when played, so a //c keeps a //e's four-axis assignment (FR-035).
- A pick from the command-bar paddle picker clears the assignment.
- Selecting arrows-to-joystick or mouse-to-paddle removes `controller` (FR-008), which lets automatic selection apply at the next connect (FR-032).

## Unit-test obligations

- Round-trip of every field through `InMemoryFileSystem`.
- Each rule in the table above, including that a rejected entry is reported rather than silently replaced.
- Profile lookup by model token and name, the interface GH #78 will use.
