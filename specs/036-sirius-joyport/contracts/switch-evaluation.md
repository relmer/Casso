# Contract: From controllers and keys to the Joyport's switches

Covers FR-005 to FR-009 and FR-014. Extends spec 034's
[game-port-mixer.md](../../034-game-controllers/contracts/game-port-mixer.md).

## MappingEvaluator

```cpp
static constexpr float kSwitchThreshold = 0.5f;   // of the shaped deflection
```

`Evaluate` fills `GamePortContribution::switches` on every call, whatever the
machine or attach state:

- PDL0's shaped value (after deadzone, calibration and inversion, before
  `ToAxisPaddle`) `<= -kSwitchThreshold` closes Left; `>= +kSwitchThreshold`
  closes Right. PDL1 does the same for Up and Down.
- The shaped value is the deflection, for Rate bindings too, so a rate-bound
  axis opens its switch when the stick returns.
- A digital pair shapes to -1, 0 or +1: pressed closes, both held closes
  neither (FR-007).
- Several bindings on one axis: the one furthest from center wins, as for the
  paddle byte.
- Fire = the PB0 bindings, by the existing `IsButtonListHeld` rule.
- PB1, PB2, PDL2 and PDL3 bindings never touch the switches (spec edge case).

## ControllerInputService

After `BuildMergedLocked`, the service sets `merged.jacks`:

| Mode | Left jack | Right jack |
|---|---|---|
| Single-source (a selection) | selection's `switches` | same as left |
| Multiplayer live | slot 1's `switches`, or all open if absent or disconnected | slot 2's, likewise |
| No driver | all open | all open |

Slot paddle targets (Joystick0, Joystick1, Paddle0-3) play no part. The
existing paddle and PB placement is unchanged.

## GamePortInputMixer

`ComputeTargetLocked` fills `GamePortState::jacks` from the axis owner:

| Axis owner | Jacks |
|---|---|
| `Controller` | the Controller source's `jacks`, or all open if it has none |
| `ArrowKeys` | left = directions from the ArrowKeys source's PDL0/PDL1 (0 closes Left/Up, 255 closes Right/Down), fire = FireKeys PB0; right = left |
| `MousePaddle` | all open |
| `None` | all open |

`AppleModifierKeys` never reaches the jacks (FR-010). Buttons and paddles are
computed exactly as before (FR-013).

## Fire keys

```cpp
// InputModeRules
static std::bitset<2> GetFireKeyButtons (bool xDown,
                                         bool zDown,
                                         bool leftAltDown,
                                         bool rightAltDown,
                                         bool isJoyportAttached);
```

Detached: PB0 = X or left Alt, PB1 = Z or right Alt (today's rule). Attached:
PB0 = X, PB1 = Z. `EmulatorShell::UpdateJoystickButtonsFromKeys` reads the four
keys and submits the result as the `FireKeys` source, and `SetGamePortAdapter`
resubmits it while arrows-to-joystick is on. Tested in
`InputModeRulesTests.cpp`.

## MachineGamePortSink

`TryApply` gains `WriteJacks`: for each jack whose switches differ from
`lastApplied`, `targets.joyport->SetJackSwitches (jack, switches)`. Skipped when
`joyport` is null. The attach state is not the sink's business: the jacks are
written whether or not the Joyport is attached, so attaching mid-game reads the
current switches at once.

## Tests (UnitTest/ControllerTests/)

- `MappingEvaluatorTests.cpp`: below and above the threshold on each of the
  four directions; a diagonal closes two; a digital pair closes with no
  threshold and both held closes neither; a Rate binding opens on release; PB0
  drives fire; PB1/PB2 do not.
- `ControllerInputServiceTests.cpp`: single-source on both jacks; two players
  each on their own jack with slot targets swapped; player 2 disconnected opens
  the right jack only.
- `GamePortInputMixerTests.cpp`: each owner row of the table above, with a
  FireKeys contribution present under None; Apple modifier keys leave the jacks
  open.
- `InputModeRulesTests.cpp`: `GetFireKeyButtons` attached and detached.
- `MachineGamePortSinkTests.cpp`: jacks reach the Joyport, only on change; no
  write on the //c.
