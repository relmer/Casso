#pragma once

#include "Pch.h"

#include "Controllers/GamePortInputMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceVerdict
//
//  What becomes of an action from the user while the machine may be behind
//  live: it goes ahead, the user is asked first, it waits behind a question
//  or an answer already given, or it is dropped.
//
////////////////////////////////////////////////////////////////////////////////

enum class DivergenceVerdict
{
    Proceed,
    Ask,
    Hold,
    Drop,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInput
//
//  A host input other than a key or the game port that asked to discard
//  history: the guest mouse's button pressed over the display, or the //c's
//  80/40 switch flipped. Each is a single change, held while the question
//  is open and after a yes until the machine is live.
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
//  would make that history false from there on. Such an action is held back
//  until the user agrees to discard the recorded history after the current
//  point; on a yes the history is cut there and the action runs live, on a
//  no it is dropped and the replay goes on.
//
//  Keys need more than a yes or no: a key press arrives as a key down and
//  then a character, and the character can arrive while the question is
//  still open or after the yes but before the machine is live. Those wait
//  in order and are handed back once the machine is live; after a no, they
//  are dropped with the press that asked.
//
//  The game port is a state, not a stream of presses: the paddles, the
//  buttons and the Joyport's switches as the host wants them. Behind live
//  that state is compared with the one the recorded history has the machine
//  reading, and with the one the host held before: only a real press or
//  deflection that the recording does not already have asks. A stick left
//  where it was, one wandering within kAxisTolerance, or one let go of asks
//  nothing. After a no the state the host then wanted becomes the one it
//  held before, so a stick kept where it was refused asks no more.
//
////////////////////////////////////////////////////////////////////////////////

class DivergenceGate
{
public:
    static constexpr const wchar_t  * kpszTitle    = L"Replaying history";
    static constexpr const wchar_t  * kpszQuestion = L"The machine is replaying recorded history. Making this change discards the history recorded after this point.\n\nDiscard it and make the change?";

    //  How far, in paddle units, an axis may move before it counts as moved.
    static constexpr int              kAxisTolerance = 16;

    //  A command posted to the machine's thread that changes its state.
    static bool               IsStateChangingCommand (WORD id);

    //  A command, or a debugger edit: asked about behind live only.
    static DivergenceVerdict  Judge                  (bool isBehindLive, bool isStateChange);

    //  Whether the game-port state the host wants changes what the machine
    //  reads: it moved from what the host held before, the recording does not
    //  already have it, and it is not everything let go.
    static bool               IsGamePortDivergence   (const GamePortState & before, const GamePortState & wanted, const GamePortState & recorded);

    //  A key event bound for the emulated machine.
    DivergenceVerdict         JudgeKey               (bool isBehindLive, const DxuiKeyEvent & ev);
    void                      Hold                   (const DxuiKeyEvent & ev) { m_held.push_back (ev); }

    //  The game-port state the host wants, against the state last written
    //  to the machine and the state the recording has it reading.
    DivergenceVerdict         JudgeGamePort          (bool isBehindLive, const GamePortState & lastWritten, const GamePortState & wanted, const GamePortState & recorded);

    //  A host input other than a key or the game port.
    DivergenceVerdict         JudgeInput             (bool isBehindLive);
    void                      HoldInput              (HeldInput input) { m_heldInputs.push_back (input); }

    //  The user's answer to the question an input asked.
    void                      Answer                 (bool isConfirmed);

    //  The machine is live again: the keys and other inputs held after a
    //  yes, each in order.
    void                      TakeHeld               (std::vector<DxuiKeyEvent> & outEvents, std::vector<HeldInput> & outInputs);

    //  The machine is live: the game-port state held at a no is forgotten.
    void                      OnLive                 ();

    bool                      IsAsking               () const { return m_state == State::Asking; }
    bool                      IsAwaitingLive         () const { return m_state == State::AwaitingLive; }

private:
    enum class State
    {
        Idle,
        Asking,
        AwaitingLive,
    };

    static bool  IsAxisApart (Byte a, Byte b);
    static bool  IsApart     (const GamePortState & a, const GamePortState & b);
    static bool  IsAtRest    (const GamePortState & state);

    void         StartAsking ();

    State                      m_state             = State::Idle;
    std::vector<DxuiKeyEvent>  m_held;
    std::vector<HeldInput>     m_heldInputs;
    GamePortState              m_gamePortWanted;
    GamePortState              m_gamePortBefore;
    bool                       m_hasGamePortWanted = false;
    bool                       m_hasGamePortBefore = false;
};
