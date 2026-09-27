# Contract: Game Port Input Mixer

**Feature**: `034-game-controllers` | **Research**: [../research.md](../research.md) R9

Replaces last-writer-wins on PDL0/PDL1/PB0-PB2 with one owner of the final values.

## Interface

`CassoEmuCore/Controllers/GamePortInputMixer.h`

```cpp
enum class GamePortSource { ArrowKeys, FireKeys, AppleModifierKeys, MousePaddle, Controller };
enum class AxisOwner      { None, ArrowKeys, MousePaddle, Controller };

// PDL0-PDL3. A machine with fewer axes (the //c has two) leaves the rest at
// center and never offers them as targets (FR-034, FR-035).
constexpr size_t  kGamePortAxisCount = 4;

struct GamePortState
{
    static constexpr Byte  kPaddleCenter = 127;

    std::array<Byte, kGamePortAxisCount>  paddle = { kPaddleCenter, kPaddleCenter,
                                                     kPaddleCenter, kPaddleCenter };
    std::bitset<3>                        buttons;
};

class IGamePortSink
{
public:
    virtual ~IGamePortSink () = default;

    // lastApplied is null when every field must be written.
    virtual bool TryApply (const GamePortState & target, const GamePortState * lastApplied) = 0;
};

class GamePortInputMixer
{
public:
    void           SetSink              (IGamePortSink * sink);
    void           SetApplyThread       (std::thread::id applyThread, std::function<void()> requestFlush);
    // Ownership is per axis: each axis has at most one owner, and handing an
    // axis to a source displaces its previous owner for that axis only
    // (FR-036). The one-argument form sets all four.
    void           SetAxisOwner         (AxisOwner owner);
    void           SetAxisOwner         (size_t axis, AxisOwner owner);
    void           Submit               (GamePortSource source, const GamePortContribution & contribution);
    void           ReleaseSource        (GamePortSource source);
    void           NotifyMachineRebuilt ();
    bool           FlushPending         ();
    bool           HasPendingWrite      () const;
    GamePortState  GetTargetState       () const;
};
```

## Sources

| Source | Carries | Submitted by |
|---|---|---|
| `ArrowKeys` | Axes | Arrow keys in arrows-to-joystick mode |
| `FireKeys` | PB0, PB1 | X/Z plus left/right Alt in arrows-to-joystick mode, foreground only |
| `AppleModifierKeys` | PB0, PB1, PB2 | Left Alt, right Alt and Shift on the //e and //c (PB2 is Shift) |
| `MousePaddle` | Axes, PB0, PB1 | Captured mouse in paddle mode |
| `Controller` | Axes, PB0-PB2 | The controller service: every driving controller merged, each axis present only where a controller holds it (see data-model `ControllerAxisAssignment`) |

Arrow axes and fire buttons are separate sources so focus loss can release the buttons without moving the axes, which is how the machine behaved before the mixer.

## Behavior

| Rule | Detail |
|---|---|
| Buttons | Final PBn = OR of every source's PBn (FR-014) |
| Axes | Final PDLn = PDLn's owner's contribution for that axis, or center (127) when that owner does not drive PDLn or is `None`. A contribution holds each axis as its own optional |
| Axis count | The sink writes only the machine's axes (`GamePortTargets::axisCount`, from `MachineDefinition::gamePortAxisCount`): four on the ][, ][+ and //e, two on the //c, whose PDL2/PDL3 lines are the mouse (FR-034) |
| Owner switch | Takes effect immediately with the new owner's last contribution, so arrows held when the controller reconnects stop driving the axes at once (spec edge case) |
| Writes | The sink is called only when the final state differs from the last applied state, so a controller at rest writes nothing however often it is sampled |
| Apply thread | The sink is called only on the apply thread (the UI thread in the shell). The device setters behind it notify the input debug panel, whose host-input callbacks are UI-thread only. A `Submit` from another thread records its values and calls `requestFlush` once until the next `FlushPending`; the shell posts a window message that calls `FlushPending`. With no apply thread set, every caller writes inline |
| Refused write | `TryApply` returns false when the machine sink cannot take the lifetime lock, which only a machine rebuild holds exclusively. The target state stays pending and `HasPendingWrite` is true. It is delivered by the next `Submit`, `SetAxisOwner` or `FlushPending` on the apply thread, and always by `NotifyMachineRebuilt`, which the rebuild calls when it releases the lock. So no release is lost across a machine switch (FR-010) |
| Rebuild | `NotifyMachineRebuilt` marks the machine as holding nothing the mixer wrote; the next write covers every field and is scheduled immediately |
| Threading | All members lock one internal mutex; the sink runs outside it |

## Sink implementations

| Class | Writes |
|---|---|
| `MachineGamePortSink` (`CassoEmuCore/Shell/`) | ][/][+: `AppleGamePort::SetPaddle`/`SetButton` (indexes 0-2). //e and //c: `Apple2eSoftSwitchBank::SetPaddle`, `Apple2eKeyboard::SetOpenApple`/`SetClosedApple`; PB2 through `Apple2eKeyboard::SetShift` on the //e and not at all on the //c, whose `$C063` is the mouse button. Takes the machine lifetime lock with `try_to_lock` and returns false if unavailable; returns true without writing when the machine has no game port (FR-017). It receives its device pointers through a small targets structure rather than `MachineHost`, so tests construct real devices directly |
| `RecordingGamePortSink` (`UnitTest/ControllerTests/`) | Records every applied state; can be told to refuse the next N writes |

## Migration of existing writers

Every direct write listed in research R9 becomes a `Submit` or `ReleaseSource` on the mixer. After migration, no code outside `MachineGamePortSink` calls the device setters for the game port.

Deliberate behavior changes, both toward the rule that the axis owner decides:

- Leaving paddle mode for Off recenters the paddles; before, they kept the last mouse position.
- Entering paddle mode centers the paddles on the ][ and ][+ as well as the //e; before, only the //e was centered.

## The `Controller` contribution with two player slots (2026-09-27)

The mixer's interface and rules above are unchanged. What changes is how `ControllerInputService` composes the one `Controller` contribution it submits (research R17, R18; data model [Players](../data-model.md#players-2026-09-27)):

| Rule | Detail |
|---|---|
| One playing | When `PlayerSlotPolicy::IsOnePlaying` holds, the playing controller (or Automatic's provisional Player 1) drives PDL0, PDL1 and PB0-PB2 from its `pdl0`, `pdl1` and `pb0`-`pb2` bindings, whichever slot it holds and whatever that slot's target (FR-042, FR-039) |
| Both playing | Each slot drives its target's paddles, its `pdl0`.. bindings landing on them in ascending order (FR-038), and the button lines of its target (below) |
| Button lines by target | Joystick 0: `pb0` to PB0 and `pb1` to PB1. Joystick 1: `pb0` to PB2. Paddle 0/1/2: `pb0` to PB0/PB1/PB2. Paddle 3: none. Bindings with no line are ignored and kept in the profile (FR-039) |
| Held slot on disconnect | The leaver's paddles are left absent (center) and its lines released within one tick (FR-010). The slot is held for it while the other slot plays, and a held slot blocks the one-playing rule, so the remaining player keeps only its own target's paddles and lines and is not widened onto the leaver's (FR-040, SC-012) |
| Return | The held slot's controller takes the slot back and drives its target again |
| Start over | With no slot playing and none held, the Automatic slots empty and nothing drives the port until R16 fills one (FR-040) |
| Waiting | A slot filled by Automatic whose holder has given no input contributes nothing, and the controller holding it is watched only for its first input (R23) |
| Keys and mouse | Player 1 on the keys or the mouse makes the `ArrowKeys` or `MousePaddle` source the axis owner for PDL0/PDL1 as before; Player 2's controller, when playing, is the owner of its target's axes |
| No second joining changes the first | A second controller that starts playing while the first plays on Joystick 0 takes Joystick 1 or a free paddle and never PDL0, PDL1, PB0 or PB1 (SC-014, edge case "A second controller bumped") |

Supersedes, for multiplayer, the Phase 9 rule "Player one's `pb0` drives PB0 and player two's drives PB1".

## Unit-test obligations

- A button held by one source stays pressed when another source releases it.
- Owner switch uses the new owner's last contribution immediately; no owner rests at center.
- No sink call when a submission changes nothing.
- A refused release is delivered by `FlushPending`, by the next submission, and by `NotifyMachineRebuilt`.
- A submission from another thread requests one flush and does not write.
- `MachineGamePortSink` against real `AppleGamePort`, `Apple2eSoftSwitchBank` and `Apple2eKeyboard` instances: ][+ routing including PB2, //e routing with PB2 as Shift, //c leaving `$C063` alone, no game port returning true and writing nothing, a held lifetime lock returning false.
- Existing input tests (`UnitTest/EmuTests/GamePortTests.cpp`, `InputEventCoalescingTests.cpp`) continue to pass unchanged.
- (2026-09-27) Through `ControllerInputServiceTests`: each row of the button-lines table with both players playing; one playing drives PB0-PB2 from either slot; a held slot keeps the remaining player on its own lines; a return restores the leaver; start over leaves nothing driving until input; a second controller joining never changes PDL0, PDL1, PB0 or PB1 while the first plays on Joystick 0.
