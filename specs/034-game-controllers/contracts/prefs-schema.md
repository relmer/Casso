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
    },
    "activeProfiles": {
      "xinput/product:045e:02e0":   "Lode Runner (D-pad)",
      "xinput/product:045e:02e0:2": ""
    }
  }
}
```

`activeProfiles` maps a unit token to the name of that controller's active profile, one of its model's profiles. An empty name is the Default, the same as no entry. An entry is kept even when it names the Default, since its presence is what stops a legacy `controllerProfile` from being moved onto that controller again (below).

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
| An `activeProfiles` entry whose key is not a unit token, or whose value is not a string | Entry dropped and reported; that controller uses Default |
| An `activeProfiles` name its model has no profile of | Kept; the controller uses Default until a profile of that name exists |

### Players and last holders (2026-09-27)

Two keys join the `controllers` section (research R19, R21):

```json
"controllers": {
  "players": [
    { "entry": "controller", "controller": "xinput/product:045e:0b13", "maps": "joystick0" },
    { "entry": "automatic" }
  ],
  "lastHolders": [ "xinput/product:045e:0b13", "dinput:231d:0121/guid:{01661270}" ]
}
```

| Key | Value | Rules |
|---|---|---|
| `players` | Array of exactly two objects, Player 1 then Player 2 | Absent = the one-time adoption has not run (below). Written in full whenever an entry or a user-set target changes |
| `players[n].entry` | `automatic`, `controller`, `keys`, `mouse` or `disabled` | `keys` and `mouse` valid for Player 1 only, `disabled` for Player 2 only; an invalid or unknown value reads as `automatic` and is reported once |
| `players[n].controller` | Unit token | Required for `controller`; an unreadable token reads as `automatic` and is reported |
| `players[n].maps` | Target token as in the per-machine block below | Absent = follow the active profile (FR-043). Kept for paddles the machine lacks (FR-035) |
| `lastHolders` | Array of two unit tokens or `null` | Only for the notice rule (FR-044); an unreadable entry reads as `null`, which means the next Automatic assignment to that slot shows a notice |

- Both entries picking one controller, or two user-set targets that overlap, are normalized on load: Player 2's entry becomes `automatic`, or its `maps` is dropped (FR-036).
- `players` never holds what Automatic chose; that lives only in `lastHolders` and never assigns a controller (FR-011).

### One-time adoption from the launched machine

Runs at launch when `controllers.players` is absent, reading the launched machine's `$cassoUiPrefs` block, then writes `players` and `lastHolders` so it never runs again:

| Machine key | Becomes |
|---|---|
| `arrowsToJoystick: true` | Player 1 `keys` |
| `pointerMapping: "paddle"` | Player 1 `mouse`, unless `arrowsToJoystick` already gave Player 1 the keys |
| `controller` | Player 1 `controller` with that token, unless Player 1 already has the keys or the mouse |
| `multiplayer` with `enabled: true` | Each filled slot becomes that player's `controller` entry and `maps`; Player 1's slot outranks the three rows above |
| anything else, or nothing | `automatic`; Player 2 is `automatic` when the block is absent or not enabled |

`lastHolders` starts as two `null`s and fills as slots are held, by a pick as much as by Automatic (FR-044). Other machines' `controller`, `multiplayer`, `arrowsToJoystick` and paddle `pointerMapping` values are ignored from then on and left in their files, so an older build keeps reading its own keys.

## Per machine: `$cassoUiPrefs` block

**Superseded for selection (2026-09-27)**: this build writes neither `controller` nor `multiplayer`, and reads them only for the one-time adoption above and for the `controllerProfile` move (FR-029), which still keys on the machine's `controller`. A `pointerMapping` of `mouse`, the //c IOU mouse, stays per machine. The rest of this section is the schema older builds write.

Added to `MachineInputPrefs` (`CassoEmuCore/Config/MachineInputPrefs.h`) beside `arrowsToJoystick` and `pointerMapping`.

| Key | Value | Absent means |
|---|---|---|
| `controller` | Unit token, e.g. `xinput:045e:0b13` or `dinput:044f:b10a/{8E8A...}` | No controller selected |
| `controllerProfile` | Profile name. Read only: written by builds before the active profile moved to `activeProfiles` | Nothing to move |
| `multiplayer` | `{ "enabled": <bool>, "players": [ <slot>, <slot> ] }` | Single-source mode: the selected controller drives PDL0/PDL1 and PB0-PB2, exactly as before this key existed |

```json
"multiplayer": {
  "enabled": true,
  "players": [
    { "controller": "xinput/product:045e:0b13",         "maps": "joystick0" },
    { "controller": "dinput:231d:0121/guid:{01661270}", "maps": "joystick1" }
  ]
}
```

- A selection is written as the unit token: `dinput:044f:b10a/guid:{8E8A...}` for DirectInput and `xinput/product:<vvvv>:<pppp>` for XInput, with `:<n>` from 2 up for a second controller of one product. A file written before product keys holds `xinput/slot:<n>`, where `<n>` is 0-3; it still loads, and a player slot holding one is moved onto the product key of the controller in that slot and saved. The MODEL token stays `xinput` (FR-018a), so profiles and deadzone are unaffected. A file holding a bare `xinput`, which is what every file written before this shipped holds, still loads: it reads as an Xbox-class controller with no slot and matches whichever one is connected.
- `maps` is one of `joystick0` (PDL0/PDL1), `joystick1` (PDL2/PDL3), `paddle0`, `paddle1`, `paddle2`, `paddle3`. An unknown token reads as `joystick0`, which every machine with a game port can play.
- The block is always written, with both slots and with `enabled` false when the mode is off, because the block is spliced key by key and an omitted key would leave a setup the user turned off in the file. Absent and `"enabled": false` read the same.
- An absent block is the default: a machine with only `controller` saved behaves exactly as before (FR-037, FR-038).
- A slot with an unreadable or empty controller token, or a non-object slot, is left EMPTY rather than dropping the block: the other player keeps playing, and an empty slot is what the settings page shows for the one that could not be restored.
- The setup is normalized on load (FR-036): a second slot repeating the first slot's controller, or claiming a paddle it already holds, is emptied rather than played. A hand-edited file cannot put two players on one paddle.
- Targets naming paddles past the machine's count are kept on load and save and play nothing, so a //c keeps a //e's four-axis setup (FR-035).
- A pick from the command-bar paddle picker turns the mode off, keeping both slots.
- **`controllerAxes`, the short-lived per-axis assignment key, is gone.** It never shipped in a release, so it is neither read nor migrated; a file still carrying it keeps it as an unknown key and it has no effect.
- Selecting arrows-to-joystick or mouse-to-paddle removes `controller` (FR-008), which lets automatic selection apply at the next connect (FR-032).
- A `controllerProfile` is moved to `activeProfiles` under the machine's saved `controller` when the machine is loaded, if that controller has no `activeProfiles` entry yet (FR-029). It is never written again, so once any machine has moved its name onto a controller, another machine's older name cannot overwrite it.

## Unit-test obligations

- Round-trip of every field through `InMemoryFileSystem`.
- Each rule in the table above, including that a rejected entry is reported rather than silently replaced.
- Profile lookup by model token and name, the interface GH #78 will use.
- (2026-09-27) `players` and `lastHolders` round trip; each invalid value in the players table reads as documented and is reported; a repeated controller or an overlapping `maps` is normalized; every row of the adoption table, including that adoption runs once and that other machines' keys are left in their files.
