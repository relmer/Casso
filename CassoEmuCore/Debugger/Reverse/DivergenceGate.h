#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"
#include "Debugger/Reverse/HeldInputWatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceVerdict
//
//  What becomes of an action from the user while the machine may be behind
//  live: it goes ahead, history forks first, it is deferred -- until the
//  guest reads it, or until the fork makes the machine live -- or it is
//  dropped.
//
////////////////////////////////////////////////////////////////////////////////

enum class DivergenceVerdict
{
    Proceed,
    Fork,
    Hold,
    Drop,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInput
//
//  A host input other than a key or the game port held behind live: the
//  guest mouse's button pressed over the display, or the //c's 80/40 switch
//  flipped. Each is a single change, handed back in order once the machine
//  is live.
//
////////////////////////////////////////////////////////////////////////////////

enum class HeldInput
{
    MousePress,
    EightyColumnToggle,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate
//
//  Behind live the machine replays recorded history, and anything that would
//  change its state -- a key into the emulated machine, a disk mounted,
//  ejected or write-protected, a reset or power cycle, a debugger edit --
//  would make that history false from there on. History forks at once, with
//  no question: the recorded future becomes a branch left behind, and the
//  command runs live.
//
//  Input the guest reads -- keys, a paste, the game port, the //c mouse --
//  is deferred instead, and the replay goes on. When the replayed guest
//  reads a line the deferred input would change (see HeldInputWatch), the
//  shell puts the machine back before that read, history forks there, and
//  the deferred input goes in live. A replay that arrives at live with no
//  fork due posts the deferred input back to go in there.
//
//  A key press leaves its key waiting in the latch until the guest reads it,
//  so a press stays deferred after its release. A button, a stick or the
//  mouse button let go of before any read is no longer held: what the guest
//  would read of it is what it reads anyway.
//
//  The game port is a state, not a stream of presses. Each of its lines is
//  held when it moved from what the host held before -- the state last
//  written, or the one refused at the last no -- and is not at rest: a stick
//  left where it was, one wandering within kAxisTolerance, or one let go of
//  holds nothing.
//
//  The //c's 80/40 switch flips whatever the machine has when it goes in,
//  so it has no value to compare a read against; it forks at once, as a
//  command does.
//
////////////////////////////////////////////////////////////////////////////////

class DivergenceGate
{
public:
    //  How far, in paddle units, an axis may move before it counts as moved.
    static constexpr int              kAxisTolerance = 16;

    //  A command posted to the machine's thread that changes its state.
    static bool               IsStateChangingCommand (WORD id);

    //  A command, or a debugger edit: a fork first, behind live only.
    static DivergenceVerdict  Judge                  (bool isBehindLive, bool isStateChange);

    //  The game-port lines the host holds: each that moved from what the
    //  host held before and is not at rest.
    static void               GetHeldGamePortLines   (const GamePortState & before, const GamePortState & wanted, HeldInputLines & outLines);

    //  A key event bound for the emulated machine, then held with the key
    //  it leaves in the latch, if any, and whether it is a key this
    //  machine's keyboard has.
    DivergenceVerdict         JudgeKey               (bool isBehindLive, const DxuiKeyEvent & ev);
    void                      HoldKey                (const DxuiKeyEvent & ev, std::optional<Byte> latch, bool isMachineKey);

    //  The game-port state the host wants, against the state last written
    //  to the machine.
    DivergenceVerdict         JudgeGamePort          (bool isBehindLive, const GamePortState & lastWritten, const GamePortState & wanted);

    //  A host input other than a key or the game port. One the guest reads
    //  is deferred; one it does not forks at once.
    DivergenceVerdict         JudgeInput             (bool isBehindLive, bool isReadGated);
    void                      HoldInput              (HeldInput input);

    //  The guest mouse's button let go of, and the host pointer over the
    //  picture in Mouse mode, behind live.
    void                      ReleaseMousePress      ();
    void                      SetMouseTarget         (std::optional<uint32_t> target);
    std::optional<uint32_t>   GetMouseTarget         () const { return m_mouseTarget; }

    //  A paste behind live, leaving its first character in the latch.
    void                      HoldPaste              (Byte latch);
    bool                      HasHeldPaste           () const { return m_isPasteHeld; }

    //  The replayed guest read a deferred line: true when history forks there
    //  now. False when nothing is held any more.
    bool                      OnHeldInputRead        ();

    //  The fork made for the deferred input, which goes in once the machine
    //  is live; false drops what was deferred.
    void                      Answer                 (bool isConfirmed);

    //  The machine is live again: the keys and other inputs held after a
    //  yes, or held unread, each in order.
    void                      TakeHeld               (std::vector<DxuiKeyEvent> & outEvents, std::vector<HeldInput> & outInputs);

    //  The machine is live: the game-port state held at a no is forgotten.
    void                      OnLive                 ();

    //  What the held input puts on the lines the guest reads; empty unless
    //  input is deferred with no fork made for it.
    HeldInputLines            GetHeldLines           () const;

    bool                      IsHolding              () const { return m_state == State::Holding; }
    bool                      IsForking               () const { return m_state == State::Forking; }
    bool                      IsAwaitingLive         () const { return m_state == State::AwaitingLive; }

private:
    enum class State
    {
        Idle,
        Holding,
        Forking,
        AwaitingLive,
    };

    static bool  IsAxisApart   (Byte a, Byte b);
    static bool  IsAxisAtRest  (Byte value);

    void         Hold          ();
    void         StartForking   ();
    void         DropHeld      ();
    bool         HasAnyHeld    () const;
    void         SettleHolding ();

    State                      m_state                = State::Idle;
    std::vector<DxuiKeyEvent>  m_held;
    std::vector<HeldInput>     m_heldInputs;
    std::set<WPARAM>           m_keysDown;
    std::optional<Byte>        m_keyLatch;
    std::optional<uint32_t>    m_mouseTarget;
    HeldInputLines             m_gamePortLines;
    GamePortState              m_gamePortWanted;
    GamePortState              m_gamePortBefore;
    bool                       m_hasGamePortWanted    = false;
    bool                       m_hasGamePortBefore    = false;
    bool                       m_isPasteHeld          = false;
    bool                       m_isMouseTargetRefused = false;
};
