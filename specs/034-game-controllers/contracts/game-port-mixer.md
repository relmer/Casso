# Contract: Game Port Input Mixer

**Feature**: `034-game-controllers` | **Research**: [../research.md](../research.md) R9

Replaces last-writer-wins on PDL0/PDL1/PB0/PB1 with one owner of the final values.

## Interface

`CassoEmuCore/Controllers/GamePortInputMixer.h`

```cpp
enum class GamePortSource { FireKeys, AppleModifierKeys, MousePaddle, Controller };
enum class AxisOwner      { None, ArrowKeys, MousePaddle, Controller };

class IGamePortSink
{
public:

    virtual ~IGamePortSink () = default;

    virtual void SetPaddle (int axis, Byte position) = 0;
    virtual void SetButton (int index, bool pressed) = 0;
};

class GamePortInputMixer
{
public:

    void Attach         (IGamePortSink * pSink);   // nullptr on machine teardown
    void SetAxisOwner   (AxisOwner owner);
    void Submit         (GamePortSource source, const GamePortContribution & contribution);
    void ReleaseSource  (GamePortSource source);   // contribution := rest
    void ReleaseAll     ();
};
```

## Behavior

| Rule | Detail |
|---|---|
| Buttons | Final PBn = OR of every source's PBn (FR-014) |
| Axes | Final PDLn = the owner's contribution, or center (127) when the owner has none or is `None` |
| Owner switch | Takes effect immediately; the new owner's last contribution is used, so arrows held when the controller reconnects stop driving the axes at once (spec edge case) |
| Writes | The sink is called only when a final value changes, so a 250 Hz controller at rest writes nothing |
| Teardown | `Attach (nullptr)` drops writes; contributions are kept and replayed when a new sink attaches, after `ReleaseAll` if the machine changed |
| Threading | All members lock one internal mutex; callers are the UI thread (keyboard and mouse sources) and the controller thread. The sink is invoked outside the lock after computing the change set |

## Sink implementations

| Class | Writes |
|---|---|
| `MachineGamePortSink` (`CassoEmuCore/Shell/`) | ][+: `AppleGamePort::SetPaddle`/`SetButton`. //e and //c: `Apple2eSoftSwitchBank::SetPaddle`, `Apple2eKeyboard::SetOpenApple`/`SetClosedApple`. Takes the machine lifetime lock with `try_to_lock`; if it is not available the write is dropped and the change set stays pending for the next submit. |
| `RecordingGamePortSink` (`UnitTest/ControllerTests/`) | Records every call for assertions |

## Migration of existing writers

Every direct write listed in research R9 becomes a `Submit` or `ReleaseSource` on the mixer. After migration, no code outside `MachineGamePortSink` calls the four device setters; a unit test sweeps for this by construction (the sink is the only type given `MachineRefs` for game-port writes).

## Unit-test obligations

- Keyboard PB0 held, controller PB0 pressed then released: PB0 stays pressed.
- Owner switch from `Controller` to `ArrowKeys` while arrows are held and the stick is deflected: axes follow arrows on the next evaluation.
- No sink call when a submission changes nothing.
- Existing input tests (`UnitTest/EmuTests/GamePortTests.cpp`, `InputEventCoalescingTests.cpp`) continue to pass unchanged.
