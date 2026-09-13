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
    // Ownership is per axis: two controllers can hold PDL0 and PDL1
    // independently, and each axis has at most one owner (FR-036).
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
| `Controller` | Axes, PB0-PB2 | The controller service |

Arrow axes and fire buttons are separate sources so focus loss can release the buttons without moving the axes, which is how the machine behaved before the mixer.

## Behavior

| Rule | Detail |
|---|---|
| Buttons | Final PBn = OR of every source's PBn (FR-014) |
| Axes | Final PDLn = the owner's contribution, or center (127) when the owner has none or is `None` |
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

## Unit-test obligations

- A button held by one source stays pressed when another source releases it.
- Owner switch uses the new owner's last contribution immediately; no owner rests at center.
- No sink call when a submission changes nothing.
- A refused release is delivered by `FlushPending`, by the next submission, and by `NotifyMachineRebuilt`.
- A submission from another thread requests one flush and does not write.
- `MachineGamePortSink` against real `AppleGamePort`, `Apple2eSoftSwitchBank` and `Apple2eKeyboard` instances: ][+ routing including PB2, //e routing with PB2 as Shift, //c leaving `$C063` alone, no game port returning true and writing nothing, a held lifetime lock returning false.
- Existing input tests (`UnitTest/EmuTests/GamePortTests.cpp`, `InputEventCoalescingTests.cpp`) continue to pass unchanged.
