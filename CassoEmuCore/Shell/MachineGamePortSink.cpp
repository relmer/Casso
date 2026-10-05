#include "Pch.h"

#include "Shell/MachineGamePortSink.h"

#include "Debugger/Reverse/DivergenceGate.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineGamePortSink
//
//  getTargets is asked for the devices on every write, under the lifetime
//  lock, so the sink never holds a pointer across a machine rebuild. Writes
//  pass through the input gate when one is set, which reverse execution holds
//  while the machine is behind live.
//
////////////////////////////////////////////////////////////////////////////////

MachineGamePortSink::MachineGamePortSink (
    std::shared_mutex  & lifetimeLock,
    TargetsFn            getTargets) :
    m_lifetimeLock (lifetimeLock),
    m_getTargets   (std::move (getTargets))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDivergenceGate
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::SetDivergenceGate (
    DivergenceGate         * divergenceGate,
    std::function<void()>    requestAsk)
{
    m_divergenceGate = divergenceGate;
    m_requestAsk     = std::move (requestAsk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryApply
//
//  Returns false when a machine rebuild holds the devices, or while reverse
//  execution has the machine behind live, in which case nothing is written
//  and the mixer keeps the state pending until the machine is live again. A
//  machine with no game port accepts the state and writes nothing (FR-017).
//
//  Behind live the refused state is judged against the recording, so a real
//  press or deflection asks whether to discard the history ahead.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineGamePortSink::TryApply (const GamePortState & target, const GamePortState * lastApplied)
{
    std::shared_lock<std::shared_mutex>  lifetime (m_lifetimeLock, std::try_to_lock);
    std::shared_lock<std::shared_mutex>  gate;
    GamePortTargets                      targets;
    bool                                 isAvailable = lifetime.owns_lock();
    bool                                 isLive      = true;



    if (isAvailable)
    {
        targets = m_getTargets();
        isLive  = m_inputGate == nullptr || m_inputGate->TryEnter (gate);
    }

    if (isAvailable && isLive)
    {
        WritePaddles (targets, target, lastApplied);
        WriteButtons (targets, target, lastApplied);
        WriteJacks   (targets, target, lastApplied);
    }
    else if (isAvailable)
    {
        JudgeBehindLive (targets, target, lastApplied);
    }

    return isAvailable && isLive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JudgeBehindLive
//
//  The three states compared see only the lines the machine reads, so a
//  line it has no wire for, or one the Joyport stands in front of, never
//  asks.
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::JudgeBehindLive (
    const GamePortTargets  & targets,
    const GamePortState    & target,
    const GamePortState    * lastApplied)
{
    GamePortState      lastWritten;
    GamePortState      wanted   = target;
    GamePortState      recorded = ReadRecorded (targets);
    DivergenceVerdict  verdict  = DivergenceVerdict::Proceed;



    if (m_divergenceGate == nullptr)
    {
        return;
    }

    if (lastApplied != nullptr)
    {
        lastWritten = *lastApplied;
    }

    MaskUnread (targets, lastWritten);
    MaskUnread (targets, wanted);
    MaskUnread (targets, recorded);

    verdict = m_divergenceGate->JudgeGamePort (true, lastWritten, wanted, recorded);

    if (verdict == DivergenceVerdict::Ask && m_requestAsk)
    {
        m_requestAsk();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRecorded
//
//  Behind live the devices hold what the recording has the machine reading.
//  On the //e the buttons are the keyboard's Apple keys and Shift line.
//
////////////////////////////////////////////////////////////////////////////////

GamePortState MachineGamePortSink::ReadRecorded (const GamePortTargets & targets)
{
    constexpr int   kOpenAppleButton  = 0;
    constexpr int   kSolidAppleButton = 1;
    constexpr int   kShiftButton      = 2;
    GamePortState   recorded;
    int             axisCount         = static_cast<int> (std::min (targets.axisCount, recorded.paddle.size()));



    for (int axis = 0; axis < axisCount; axis++)
    {
        if (targets.gamePort != nullptr)
        {
            recorded.paddle[axis] = targets.gamePort->GetPaddle (axis);
        }
        else if (targets.iieSwitches != nullptr)
        {
            recorded.paddle[axis] = targets.iieSwitches->GetPaddle (axis);
        }
    }

    if (targets.iieKeyboard != nullptr)
    {
        recorded.buttons.set (kOpenAppleButton,  targets.iieKeyboard->IsOpenApplePressed());
        recorded.buttons.set (kSolidAppleButton, targets.iieKeyboard->IsClosedApplePressed());
        recorded.buttons.set (kShiftButton,      targets.iieKeyboard->IsShiftPressed());
    }
    else if (targets.gamePort != nullptr)
    {
        for (int index = 0; index < static_cast<int> (recorded.buttons.size()); index++)
        {
            recorded.buttons.set (index, targets.gamePort->IsButtonPressed (index));
        }
    }

    for (size_t jack = 0; targets.joyport != nullptr && jack < JoyportJacks::kJackCount; jack++)
    {
        recorded.jacks.jack[jack] = targets.joyport->GetJackSwitches (jack);
    }

    return recorded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MaskUnread
//
//  Puts every line the machine does not read at rest: the axes past the
//  ones it has, PB2 on the //c (its mouse button's line), and the Joyport's
//  switches while it is detached. While it is attached it answers the
//  buttons in their place, and with nothing in its rear sockets the paddles
//  too.
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::MaskUnread (
    const GamePortTargets  & targets,
    GamePortState          & state)
{
    constexpr size_t  kShiftButton  = 2;
    GamePortState     rest;
    bool              hasPaddles    = targets.gamePort != nullptr || targets.iieSwitches != nullptr;
    bool              hasButtons    = targets.gamePort != nullptr || targets.iieKeyboard != nullptr;
    bool              isJoyportOn   = targets.joyport  != nullptr && targets.joyport->IsAttached();
    bool              isMouseOnPb2  = targets.gamePort == nullptr && targets.iieKeyboard != nullptr && targets.iieKeyboard->HasMouseOnShiftLine();
    bool              arePaddlesOut = isJoyportOn && !targets.joyport->ArePaddlesConnected();



    for (size_t axis = 0; axis < state.paddle.size(); axis++)
    {
        if (!hasPaddles || arePaddlesOut || axis >= targets.axisCount)
        {
            state.paddle[axis] = rest.paddle[axis];
        }
    }

    if (!hasButtons || isJoyportOn)
    {
        state.buttons.reset();
    }
    else if (isMouseOnPb2)
    {
        state.buttons.reset (kShiftButton);
    }

    if (!isJoyportOn)
    {
        state.jacks = rest.jacks;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WritePaddles
//
//  Only the axes the machine has. The //c's soft-switch bank models all four
//  one-shots, but its PDL2 and PDL3 lines are the mouse, so a value held for
//  them is never written there.
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::WritePaddles (
    const GamePortTargets  & targets,
    const GamePortState    & target,
    const GamePortState    * lastApplied)
{
    int  axisCount = static_cast<int> (std::min (targets.axisCount, target.paddle.size()));



    for (int axis = 0; axis < axisCount; axis++)
    {
        bool  isChanged = lastApplied == nullptr || lastApplied->paddle[axis] != target.paddle[axis];

        if (!isChanged)
        {
            continue;
        }

        if (targets.gamePort != nullptr)
        {
            targets.gamePort->SetPaddle (axis, target.paddle[axis]);
        }

        if (targets.iieSwitches != nullptr)
        {
            targets.iieSwitches->SetPaddle (axis, target.paddle[axis]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteButtons
//
//  On the //e the three buttons are the keyboard's Open-Apple, Solid-Apple
//  and Shift lines, the same wires the game port's PB0-PB2 share. The //c
//  wires its mouse button to the Shift line, so PB2 has nothing to drive
//  there and is not written.
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::WriteButtons (
    const GamePortTargets  & targets,
    const GamePortState    & target,
    const GamePortState    * lastApplied)
{
    constexpr int  kOpenAppleButton  = 0;
    constexpr int  kSolidAppleButton = 1;
    constexpr int  kShiftButton      = 2;



    for (int index = 0; index < static_cast<int> (target.buttons.size()); index++)
    {
        bool  isPressed = target.buttons.test (index);
        bool  isChanged = lastApplied == nullptr || lastApplied->buttons.test (index) != isPressed;

        if (!isChanged)
        {
            continue;
        }

        if (targets.gamePort != nullptr)
        {
            targets.gamePort->SetButton (index, isPressed);
        }

        if (targets.iieKeyboard == nullptr)
        {
            continue;
        }

        if (index == kOpenAppleButton)
        {
            targets.iieKeyboard->SetOpenApple (isPressed);
        }
        else if (index == kSolidAppleButton)
        {
            targets.iieKeyboard->SetClosedApple (isPressed);
        }
        else if (index == kShiftButton && !targets.iieKeyboard->HasMouseOnShiftLine())
        {
            targets.iieKeyboard->SetShift (isPressed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteJacks
//
//  Written whether or not the Joyport is attached, so attaching it mid-game
//  reads the switches as they are now rather than as they were when it was
//  last attached.
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::WriteJacks (
    const GamePortTargets  & targets,
    const GamePortState    & target,
    const GamePortState    * lastApplied)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (targets.joyport == nullptr, S_OK);

    for (size_t jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        bool  isChanged = lastApplied == nullptr || lastApplied->jacks.jack[jack] != target.jacks.jack[jack];

        if (isChanged)
        {
            targets.joyport->SetJackSwitches (jack, target.jacks.jack[jack]);
        }
    }

Error:
    return;
}
