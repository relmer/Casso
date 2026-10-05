#include "Pch.h"

#include "Debugger/Reverse/DivergenceGate.h"

#include "Shell/CpuCommandDispatcher.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::IsStateChangingCommand
//
//  The commands the input journal records, which are the ones that change
//  what the machine computes; the answer to a changed disk file, which can
//  put a different disk in the drive; and a machine switch, a state file
//  loaded or a debugger restart, each of which replaces the machine. A
//  change to the machine's configuration in Settings arrives as a switch.
//  A paste types into the machine as keys do.
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsStateChangingCommand (WORD id)
{
    EmulatorCommand  command;
    InputRecord      input;



    command.id = id;

    switch (id)
    {
        case IDM_EDIT_PASTE:
        case IDM_DISK_RESOLVE_CHANGE:
        case IDM_FILE_OPEN:
        case IDM_FILE_LOAD_STATE:
        case IDM_DEBUG_RESTART:
            return true;

        default:
            return CpuCommandDispatcher::TryGetJournalInput (command, input);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::Judge
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::Judge (
    bool  isBehindLive,
    bool  isStateChange)
{
    return (isBehindLive && isStateChange) ? DivergenceVerdict::Ask : DivergenceVerdict::Proceed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeKey
//
//  While a question is open, or after a yes until the machine is live, every
//  key event waits its turn. Otherwise, live, the key goes ahead; behind
//  live, a fresh press asks, and anything else -- a release, a repeat, the
//  character of a press that was refused -- is dropped.
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::JudgeKey (
    bool                  isBehindLive,
    const DxuiKeyEvent  & ev)
{
    bool  isPress = ev.kind == DxuiKeyEventKind::Down && !ev.repeat;



    if (m_state != State::Idle)
    {
        return DivergenceVerdict::Hold;
    }

    if (!isBehindLive)
    {
        return DivergenceVerdict::Proceed;
    }

    if (!isPress)
    {
        return DivergenceVerdict::Drop;
    }

    StartAsking();

    return DivergenceVerdict::Ask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::Answer
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::Answer (bool isConfirmed)
{
    if (m_state != State::Asking)
    {
        return;
    }

    if (isConfirmed)
    {
        m_state = State::AwaitingLive;
        return;
    }

    m_state = State::Idle;
    m_held.clear();
    m_heldInputs.clear();

    if (m_hasGamePortWanted)
    {
        m_gamePortBefore    = m_gamePortWanted;
        m_hasGamePortBefore = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::TakeHeld
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::TakeHeld (
    std::vector<DxuiKeyEvent>  & outEvents,
    std::vector<HeldInput>     & outInputs)
{
    outEvents.clear();
    outInputs.clear();

    if (m_state != State::AwaitingLive)
    {
        return;
    }

    outEvents.swap (m_held);
    outInputs.swap (m_heldInputs);
    m_state = State::Idle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::IsGamePortDivergence
//
//  Three tests, all needed. Moved from before: a stick held where it was, or
//  wandering within the tolerance, is not the user doing anything. Apart
//  from the recording: what the machine already reads there changes nothing.
//  Not at rest: letting go is never a question, as a key's release is not;
//  the machine takes the released state once it is live.
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsGamePortDivergence (
    const GamePortState  & before,
    const GamePortState  & wanted,
    const GamePortState  & recorded)
{
    return IsApart (before, wanted) && IsApart (recorded, wanted) && !IsAtRest (wanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeGamePort
//
//  Live, the state is written. Behind live, while a question is open or a
//  yes waits for the machine to be live, it waits too; otherwise it asks
//  when it changes what the machine reads, and is dropped when it does not.
//  What the host held before is the state last written to the machine, or
//  the state it wanted when it was last told no.
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::JudgeGamePort (
    bool                   isBehindLive,
    const GamePortState  & lastWritten,
    const GamePortState  & wanted,
    const GamePortState  & recorded)
{
    const GamePortState  & before = m_hasGamePortBefore ? m_gamePortBefore : lastWritten;



    if (!isBehindLive)
    {
        return DivergenceVerdict::Proceed;
    }

    m_gamePortWanted    = wanted;
    m_hasGamePortWanted = true;

    if (m_state != State::Idle)
    {
        return DivergenceVerdict::Hold;
    }

    if (!IsGamePortDivergence (before, wanted, recorded))
    {
        return DivergenceVerdict::Drop;
    }

    StartAsking();

    return DivergenceVerdict::Ask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeInput
//
//  The guest mouse's button or the //c's 80/40 switch: each is one change,
//  so behind live every one asks, or waits on a question already open.
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::JudgeInput (bool isBehindLive)
{
    if (m_state != State::Idle)
    {
        return DivergenceVerdict::Hold;
    }

    if (!isBehindLive)
    {
        return DivergenceVerdict::Proceed;
    }

    StartAsking();

    return DivergenceVerdict::Ask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::OnLive
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::OnLive()
{
    m_hasGamePortWanted = false;
    m_hasGamePortBefore = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::IsAxisApart
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsAxisApart (
    Byte  a,
    Byte  b)
{
    return std::abs (static_cast<int> (a) - static_cast<int> (b)) > kAxisTolerance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::IsApart
//
//  Any button or Joyport switch different, or any axis further apart than
//  the tolerance.
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsApart (
    const GamePortState  & a,
    const GamePortState  & b)
{
    bool  isApart = a.buttons != b.buttons || !(a.jacks == b.jacks);



    for (size_t axis = 0; axis < a.paddle.size() && !isApart; axis++)
    {
        isApart = IsAxisApart (a.paddle[axis], b.paddle[axis]);
    }

    return isApart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::IsAtRest
//
//  Everything let go: no button down, no switch closed, every axis within
//  the tolerance of center.
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsAtRest (const GamePortState & state)
{
    GamePortState  rest;



    return !IsApart (rest, state);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::StartAsking
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::StartAsking()
{
    m_state = State::Asking;
    m_held.clear();
    m_heldInputs.clear();
}





