#include "Pch.h"

#include "Debugger/Reverse/DivergenceGate.h"

#include "Shell/CpuCommandDispatcher.h"
#include "resource.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Shell/CpuManager.h"





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
//  DivergenceGate::GetHeldGamePortLines
//
//  Line by line, two tests, both needed. Moved from before: a stick held
//  where it was, or wandering within the tolerance, is not the user doing
//  anything. Not at rest: letting go is never in question, as a key's
//  release is not; the machine takes the released state once it is live.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::GetHeldGamePortLines (
    const GamePortState  & before,
    const GamePortState  & wanted,
    HeldInputLines       & outLines)
{
    size_t  index = 0;



    for (index = 0; index < outLines.buttons.size(); index++)
    {
        bool  isPressed = wanted.buttons.test (index);

        outLines.buttons[index].reset();

        if (isPressed && !before.buttons.test (index))
        {
            outLines.buttons[index] = true;
        }
    }

    for (index = 0; index < outLines.paddles.size(); index++)
    {
        Byte  value = wanted.paddle[index];

        outLines.paddles[index].reset();

        if (IsAxisApart (before.paddle[index], value) && !IsAxisAtRest (value))
        {
            outLines.paddles[index] = value;
        }
    }

    for (index = 0; index < outLines.jacks.size(); index++)
    {
        const JoystickSwitches  & switches = wanted.jacks.jack[index];

        outLines.jacks[index].reset();

        if (switches != before.jacks.jack[index] && switches.any())
        {
            outLines.jacks[index] = switches;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeKey
//
//  With anything held -- unread, behind a question, or after a yes until the
//  machine is live -- every key event waits its turn, so the keys go in in
//  the order they were pressed. Otherwise, live, the key goes ahead; behind
//  live a fresh press starts holding, and a release, a repeat or a character
//  is dropped: its press was never seen, or was told no.
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

    Hold();

    return DivergenceVerdict::Hold;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::HoldKey
//
//  The latest key latched is the one waiting: the latch holds one key.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::HoldKey (
    const DxuiKeyEvent   & ev,
    std::optional<Byte>    latch,
    bool                   isMachineKey)
{
    m_held.push_back (ev);

    if (latch.has_value())
    {
        m_keyLatch = latch;
    }

    if (isMachineKey && ev.kind == DxuiKeyEventKind::Down && !ev.repeat)
    {
        m_keysDown.insert (ev.vk);
    }
    else if (ev.kind == DxuiKeyEventKind::Up)
    {
        m_keysDown.erase (ev.vk);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeGamePort
//
//  Live, the state is written. Behind live, while a question is open or a
//  yes waits for the machine to be live, it waits too; otherwise the lines
//  it holds are held, and a state holding none is dropped. What the host
//  held before is the state last written to the machine, or the state it
//  wanted when it was last told no.
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::JudgeGamePort (
    bool                   isBehindLive,
    const GamePortState  & lastWritten,
    const GamePortState  & wanted)
{
    const GamePortState  & before  = m_hasGamePortBefore ? m_gamePortBefore : lastWritten;
    DivergenceVerdict      verdict = DivergenceVerdict::Hold;



    if (!isBehindLive)
    {
        return DivergenceVerdict::Proceed;
    }

    m_gamePortWanted    = wanted;
    m_hasGamePortWanted = true;

    GetHeldGamePortLines (before, wanted, m_gamePortLines);

    if (m_state == State::Asking || m_state == State::AwaitingLive)
    {
        return DivergenceVerdict::Hold;
    }

    if (HeldInputLines() == m_gamePortLines)
    {
        verdict = DivergenceVerdict::Drop;
    }
    else
    {
        Hold();
    }

    SettleHolding();

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::JudgeInput
//
//  The guest mouse's button is held until the guest reads it. The //c's
//  80/40 switch is asked about at once, joined by anything held already.
//
////////////////////////////////////////////////////////////////////////////////

DivergenceVerdict DivergenceGate::JudgeInput (
    bool  isBehindLive,
    bool  isReadGated)
{
    if (m_state == State::Asking || m_state == State::AwaitingLive)
    {
        return DivergenceVerdict::Hold;
    }

    if (!isBehindLive)
    {
        return DivergenceVerdict::Proceed;
    }

    if (isReadGated)
    {
        Hold();
        return DivergenceVerdict::Hold;
    }

    StartAsking();

    return DivergenceVerdict::Ask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::HoldInput
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::HoldInput (HeldInput input)
{
    m_heldInputs.push_back (input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::ReleaseMousePress
//
//  A press let go of before the guest read it is dropped, unless a question
//  already took it in: then the click goes in as a click once live.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::ReleaseMousePress()
{
    if (m_state != State::Holding)
    {
        return;
    }

    std::erase (m_heldInputs, HeldInput::MousePress);

    SettleHolding();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::SetMouseTarget
//
//  The pointer's place over the picture is the guest mouse's target; it
//  starts holding, and leaving the picture gives the target up. After a no
//  to a held target the pointer is left alone until it leaves the picture,
//  so moving on across it does not ask again at every step.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::SetMouseTarget (std::optional<uint32_t> target)
{
    if (m_state == State::Asking || m_state == State::AwaitingLive)
    {
        return;
    }

    if (!target.has_value())
    {
        m_isMouseTargetRefused = false;
    }

    if (m_isMouseTargetRefused)
    {
        return;
    }

    m_mouseTarget = target;

    if (target.has_value())
    {
        Hold();
    }

    SettleHolding();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::HoldPaste
//
//  A paste waits in its buffer until the machine is live, typing its first
//  character first.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::HoldPaste (Byte latch)
{
    if (m_state == State::Idle)
    {
        Hold();
    }

    m_isPasteHeld = true;

    if (!m_keyLatch.has_value())
    {
        m_keyLatch = latch;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::OnHeldInputRead
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::OnHeldInputRead()
{
    bool  isAsking = m_state == State::Holding && HasAnyHeld();



    if (isAsking)
    {
        StartAsking();
    }

    return isAsking;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::Answer
//
//  A no drops what was held, and the game-port state the host then wanted
//  becomes the one it held before, so a stick kept where it was refused
//  holds nothing more; a pointer refused over the picture likewise holds
//  nothing until it leaves it.
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

    m_isMouseTargetRefused = m_mouseTarget.has_value();

    DropHeld();

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

    if (m_state != State::AwaitingLive && m_state != State::Holding)
    {
        return;
    }

    outEvents.swap (m_held);
    outInputs.swap (m_heldInputs);

    DropHeld();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::OnLive
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::OnLive()
{
    m_hasGamePortWanted    = false;
    m_hasGamePortBefore    = false;
    m_isMouseTargetRefused = false;
    m_gamePortLines        = HeldInputLines();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::GetHeldLines
//
//  A held press of a key this machine has keeps any-key-down up until its
//  release; the mouse's button is down while its press is held.
//
////////////////////////////////////////////////////////////////////////////////

HeldInputLines DivergenceGate::GetHeldLines() const
{
    HeldInputLines  lines;



    if (m_state != State::Holding)
    {
        return lines;
    }

    lines             = m_gamePortLines;
    lines.keyLatch    = m_keyLatch;
    lines.isKeyDown   = !m_keysDown.empty();
    lines.mouseTarget = m_mouseTarget;

    if (std::ranges::find (m_heldInputs, HeldInput::MousePress) != m_heldInputs.end())
    {
        lines.mouseButton = true;
    }

    return lines;
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
//  DivergenceGate::IsAxisAtRest
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::IsAxisAtRest (Byte value)
{
    return !IsAxisApart (GamePortState::kPaddleCenter, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::Hold
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::Hold()
{
    if (m_state == State::Idle)
    {
        m_state = State::Holding;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::StartAsking
//
//  Whatever is held already is part of the same question.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::StartAsking()
{
    m_state = State::Asking;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::DropHeld
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::DropHeld()
{
    m_state       = State::Idle;
    m_isPasteHeld = false;

    m_held.clear();
    m_heldInputs.clear();
    m_keysDown.clear();
    m_keyLatch.reset();
    m_mouseTarget.reset();

    m_gamePortLines = HeldInputLines();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::HasAnyHeld
//
////////////////////////////////////////////////////////////////////////////////

bool DivergenceGate::HasAnyHeld() const
{
    HeldInputLines  none;
    bool            hasEvents = !m_held.empty() || !m_heldInputs.empty() || m_isPasteHeld;



    return hasEvents || m_mouseTarget.has_value() || !(m_gamePortLines == none);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGate::SettleHolding
//
//  Holding with nothing held any more is not holding.
//
////////////////////////////////////////////////////////////////////////////////

void DivergenceGate::SettleHolding()
{
    if (m_state == State::Holding && !HasAnyHeld())
    {
        m_state = State::Idle;
    }
}




