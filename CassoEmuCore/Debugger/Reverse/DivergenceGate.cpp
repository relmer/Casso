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

    m_state = State::Asking;
    m_held.clear();

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::TakeHeld
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::TakeHeld (std::vector<DxuiKeyEvent> & outEvents)
{
    outEvents.clear();

    if (m_state != State::AwaitingLive)
    {
        return;
    }

    outEvents.swap (m_held);
    m_state = State::Idle;
}





