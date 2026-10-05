#pragma once

#include "Pch.h"





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
////////////////////////////////////////////////////////////////////////////////

class DivergenceGate
{
public:
    static constexpr const wchar_t  * kpszTitle    = L"Replaying history";
    static constexpr const wchar_t  * kpszQuestion = L"The machine is replaying recorded history. Making this change discards the history recorded after this point.\n\nDiscard it and make the change?";

    //  A command posted to the machine's thread that changes its state.
    static bool               IsStateChangingCommand (WORD id);

    //  A command, or a debugger edit: asked about behind live only.
    static DivergenceVerdict  Judge                  (bool isBehindLive, bool isStateChange);

    //  A key event bound for the emulated machine.
    DivergenceVerdict         JudgeKey               (bool isBehindLive, const DxuiKeyEvent & ev);
    void                      Hold                   (const DxuiKeyEvent & ev) { m_held.push_back (ev); }

    //  The user's answer to the question a key asked.
    void                      Answer                 (bool isConfirmed);

    //  The machine is live again: the keys held after a yes, in order.
    void                      TakeHeld               (std::vector<DxuiKeyEvent> & outEvents);

    bool                      IsAsking               () const { return m_state == State::Asking; }
    bool                      IsAwaitingLive         () const { return m_state == State::AwaitingLive; }

private:
    enum class State
    {
        Idle,
        Asking,
        AwaitingLive,
    };

    State                      m_state = State::Idle;
    std::vector<DxuiKeyEvent>  m_held;
};
