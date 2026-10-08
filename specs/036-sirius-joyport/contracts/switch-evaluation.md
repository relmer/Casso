# Contract: From controllers and keys to the Joyport's switches

Covers FR-005 to FR-009 and FR-014. Extends spec 034's
[game-port-mixer.md](../../034-game-controllers/contracts/game-port-mixer.md).

## MappingEvaluator

```cpp
static constexpr float kSwitchThreshold = 0.5f;   // of the shaped deflection
```

`Evaluate` fills `GamePortContribution::switches` on every call, whatever the
machine or attach state:

- PDL0's shaped value (after dead zone, calibration and inversion, before
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

> **Superseded (2026-09-27) by "Jack assignment" below**, which derives the
> jacks from spec 034's players (research R21). The table is kept as built.

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

---

## 2026-09-27 additions (GH #156)

### Jack assignment (FR-008, research R21)

```cpp
enum class JoyportPlayerState { Idle, Driving, Held };   // spec 034's PlayerSlotState, reduced (R21)

struct JoyportPlayers
{
    std::array<JoyportPlayerState, 2>  players           = {};
    bool                               isPlayer2Disabled = false;   // Player 2 Disabled: "Same as left" in the submenu, "same as left" after a colon
};

enum class JoyportJackSource { None, Player1, Player2 };

class JoyportJackRules
{
public:
    static std::array<JoyportJackSource, 2>  AssignJacks (const JoyportPlayers & players);
};
```

| Player 1 | Player 2 | Left | Right |
|---|---|---|---|
| Driving | Idle | Player1 | Player1 |
| Idle | Driving | Player2 | Player2 |
| Driving | Disabled | Player1 | Player1 |
| Driving | Driving | Player1 | Player2 |
| Held | Driving | None | Player2 |
| Driving | Held | Player1 | None |
| anything else | | None | None |

The reduction from 034's `PlayerSlotState` (`Playing`, `Provisional` or the
arrow keys are Driving; `Held` is Held; the rest Idle) is a second pure
function, `JoyportJackRules::ReducePlayerState`, added when 034's type
exists.

`None` reads every switch open. `ControllerInputService::AddJoyportSwitches`
places each player's pre-merge switches on the jacks this returns. Slot targets
play no part. When Player 1 is the arrow keys, the mixer takes that player's
switches from the keyboard sources; a jack assigned to Player 2 still takes the
Controller source's switches for Player 2.

Tests: `UnitTest/ControllerTests/JoyportJackRulesTests.cpp` (every row, plus
"a held slot never passes to the other player"); `ControllerInputServiceTests.cpp`
(US3 scenarios 1, 2, 4, 5 and 6 through `FakeControllerBackend`, once spec
034's players exist).

### The Joyport profile's second stick (FR-017, research R18)

`DefaultMapping::MakeJoyport (model, formFactor, controls)`:

| Device | Steers | Fires |
|---|---|---|
| XInput | left stick, right stick, D-pad | A, B, X, Y, both bumpers, both triggers |
| DirectInput gamepad | primary stick (X/Y), D-pad, and Z/Rz if both present, else Rx/Ry if both present | every button |
| DirectInput joystick or wheel | primary stick, D-pad | every button |

No trigger (`ControlKind::Trigger`) is ever an axis binding. Tests in
`ControllerProfileStoreTests.cpp` beside the existing built-in profile tests:
each row; a gamepad with Z only and Rx/Ry gets Rx/Ry; one with neither pair
gets no second stick; a joystick with Z/Rz gets none.

### Mode of the profile played (FR-018, FR-020, research R17)

The service resolves a controller's profile for the mode in effect: the
mode's active choice when it points at a profile of that mode, otherwise the
mode's built-in profile. `SetActiveProfile` with a profile of the other mode
leaves the choice unchanged. Evaluation of the chosen mapping is unchanged.

(Superseded 2026-09-27: the two modes became three profile kinds, Joystick,
Paddle and Joyport, research R25.) The service resolves each player's profile
for that player's effective mode, `PlayerSlotPolicy::GetEffectiveMode`: while
the Joyport is in effect both players are Joystick for routing and every
controller plays its Joyport-kind choice (`joyportActiveProfiles`); otherwise
a Joystick player plays its `activeProfiles` choice and a Paddle player its
`paddleActiveProfiles` choice, each falling back to the kind's built-in
(Default, Paddles or Joyport). Mouse-as-paddle is a Paddle-mode source only, so
it never feeds the jacks (R26).

---

## 2026-09-28 changes (GH #156, research R27)

### Jack assignment from resolved modes

```cpp
struct JoyportPlayers
{
    std::array<JoyportPlayerState, 2>     players = {};
    std::array<std::optional<size_t>, 2>  jacks;     // the jack each player's resolved mode holds, if any
};

class JoyportJackRules
{
public:
    static JackSources         AssignJacks       (const JoyportPlayers & players);
    static JoyportPlayerState  ReducePlayerState (size_t player, const PlayerSlot & slot, const PlayerEntry & entry);
    static JoyportPlayers      ReducePlayers     (const PlayerSlots & slots, const PlayerEntries & entries, bool hasJoyport);
    static std::bitset<2>      GetPlayerJacks    (const JackSources & sources, size_t player);
};
```

`isPlayer2Disabled` is removed, and the table above no longer applies. A
Driving player drives the jack its resolved mode holds
(`PlayerModeRules::ResolveMode`), and also the other jack while that jack is
free: no player's mode holds it, or its player is Idle. A Held player's jack
is `None`, and the other player does not take it. A player on Joystick,
Paddle or Disabled holds no jack.

| Player 1 | Player 2 | Left | Right |
|---|---|---|---|
| left, Driving | Joystick or Paddle | Player1 | Player1 |
| left, Driving | right (Same as Player 1), Idle | Player1 | Player1 |
| left, Driving | right, Driving | Player1 | Player2 |
| right, Driving | left, Driving | Player2 | Player1 |
| left, Held | right, Driving | None | Player2 |
| Joystick | left, Driving | Player2 | Player2 |
| off the Joyport | off the Joyport | None | None |

`GetPlayerJacks` gives the notice's jack suffix (`PlayerModeRules::DescribeAssignment`)
and the Controllers page's note.

### Mixer

`ComputeTargetLocked` takes the keyboard's part of the jacks from the jacks
the ArrowKeys contribution marks, `GamePortContribution::keyJacks`, rather
than from who owns the axes. The arrow keys own PDL0 and PDL1 only while
Player 1 is not on a Joyport mode; on a Joyport mode they drive Player 1's
jack or jacks and no paddle input. The mouse owns PDL0 only while Player 2
plays.

### Buttons cut and paddle inputs

While `PlayerModeRules::IsJoyportOn`, the Joyport drives PB0-PB2, so a player
on Joystick or Paddle (`AreButtonsCut`) contributes no button lines. It still
drives its paddle inputs as a lone player would: Joystick PDL0 and PDL1,
Paddle PDL0. The shell connects the Joyport's paddle inputs from
`ArePaddlesConnected`, so they read as no paddle connected only while every
playing player is on a jack.

### Profile kind

Each controller plays the profile kind of its own player's resolved mode:
Joyport for either jack, else Joystick or Paddle. The other player's kind is
unaffected. `PlayerSlotPolicy::GetEffectiveMode`, which made both players
Joystick while the Joyport was on, is removed; `PlayerModeRules::ResolveMode`
takes its place.

### Tests

`JoyportJackRulesTests.cpp`: every row above, a lone Joyport player on both
jacks with the other jack free, a held jack never passed to the other player,
Same as Player 1 on the other jack. `ControllerInputServiceTests.cpp`: a
player on Joystick beside a Joyport player drives PDL0 and PDL1 and no button;
the Joyport player's controller plays its Joyport choice and the other its own
kind. `GamePortInputMixerTests.cpp`: the keys drive the marked jacks and no
paddle on a Joyport mode; the mouse drives PDL0 only while Player 2 plays.
