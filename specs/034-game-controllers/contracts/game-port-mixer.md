# Contract: Game Port Input Mixer

**Feature**: `034-game-controllers` | **Research**: [../research.md](../research.md) R9

Replaces last-writer-wins on PDL0/PDL1/PB0/PB1 with one owner of the final values.

## Interface

`CassoEmuCore/Controllers/GamePortInputMixer.h`

```cpp
enum class GamePortSource { FireKeys, AppleModifierKeys, MousePaddle, Controller };
enum class AxisOwner      { None, ArrowKeys, MousePaddle, Controller };

struct GamePortState
{
    std::array<Byte, 2>  paddle  = { 127, 127 };
    std::bitset<3>       buttons;
};

class IGamePortSink
{
public:

    virtual ~IGamePortSink () = default;

    // Applies every field that differs from lastApplied; false if the machine
    // could not be written right now (lifetime lock busy).
    virtual bool TryApply (const GamePortState & state, const GamePortState & lastApplied) = 0;
};

class GamePortInputMixer
{
public:

    void Attach         (IGamePortSink * pSink);   // nullptr on machine teardown
    void SetAxisOwner   (AxisOwner owner);
    void Submit         (GamePortSource source, const GamePortContribution & contribution);
    void ReleaseSource  (GamePortSource source);   // contribution := rest
    void ReleaseAll     ();
    bool FlushPending   ();                        // retry a refused write; true when nothing is pending
    bool HasPendingWrite () const;
};
```

## Behavior

| Rule | Detail |
|---|---|
| Buttons | Final PBn = OR of every source's PBn for PB0-PB2 (FR-014) |
| PB2 per machine | ][/][+: `AppleGamePort` button 2. //e: ORs with the Shift key, which is the same `$C063` line. //c: dropped, since `$C063` is the mouse button (R15) |
| Axes | Final PDLn = the owner's contribution, or center (127) when the owner has none or is `None` |
| Owner switch | Takes effect immediately; the new owner's last contribution is used, so arrows held when the controller reconnects stop driving the axes at once (spec edge case) |
| Writes | The sink is called only when a final value changes, so a controller at rest writes nothing however often it is sampled |
| Refused write | If `TryApply` returns false, the mixer keeps the target state and `lastApplied` unchanged and reports `HasPendingWrite`. Every later `Submit`, `ReleaseSource` or `FlushPending` retries the whole target state, so a release is never lost. The controller thread calls `FlushPending` on each wake and, while a write is pending, waits with a 5 ms timeout even if no controller needs polling; the UI thread's input handlers call it too. A disconnect release therefore reaches the machine within one machine-switch lock hold plus 5 ms (FR-010, SC-005). |
| Teardown | `Attach (nullptr)` drops writes; contributions are kept and replayed when a new sink attaches, after `ReleaseAll` if the machine changed |
| Threading | All members lock one internal mutex; callers are the UI thread (keyboard and mouse sources) and the controller thread. The sink is invoked outside the lock after computing the change set |

## Sink implementations

| Class | Writes |
|---|---|
| `MachineGamePortSink` (`CassoEmuCore/Shell/`) | ][+: `AppleGamePort::SetPaddle`/`SetButton` (indexes 0-2). //e and //c: `Apple2eSoftSwitchBank::SetPaddle`, `Apple2eKeyboard::SetOpenApple`/`SetClosedApple`; PB2 on the //e through the keyboard's Shift state, and not at all on the //c. Takes the machine lifetime lock with `try_to_lock`; returns false if it is not available. Writes nothing and returns true when the machine has no game port (FR-017). |
| `RecordingGamePortSink` (`UnitTest/ControllerTests/`) | Records every applied state; can be told to refuse the next N writes |

## Migration of existing writers

Every direct write listed in research R9 becomes a `Submit` or `ReleaseSource` on the mixer. After migration, no code outside `MachineGamePortSink` calls the four device setters; a unit test sweeps for this by construction (the sink is the only type given `MachineRefs` for game-port writes).

## Unit-test obligations

- Keyboard PB0 held, controller PB0 pressed then released: PB0 stays pressed.
- Owner switch from `Controller` to `ArrowKeys` while arrows are held and the stick is deflected: axes follow arrows on the next evaluation.
- No sink call when a submission changes nothing.
- A refused release is retried by `FlushPending` and by the next submission, and reaches the sink; `HasPendingWrite` is true only in between.
- `MachineGamePortSink` against real `AppleGamePort`, `Apple2eSoftSwitchBank` and `Apple2eKeyboard` instances: ][+ routing including PB2, //e routing with PB2 as Shift, //c dropping PB2, a machine with no game port returning true and writing nothing, and a held lifetime lock returning false.
- Existing input tests (`UnitTest/EmuTests/GamePortTests.cpp`, `InputEventCoalescingTests.cpp`) continue to pass unchanged.
