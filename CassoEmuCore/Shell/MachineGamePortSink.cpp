#include "Pch.h"

#include "Shell/MachineGamePortSink.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineGamePortSink
//
//  getTargets is asked for the devices on every write, under the lifetime
//  lock, so the sink never holds a pointer across a machine rebuild.
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
//  TryApply
//
//  Returns false only when a machine rebuild holds the devices, in which case
//  nothing is written and the mixer keeps the state pending. A machine with no
//  game port accepts the state and writes nothing (FR-017).
//
////////////////////////////////////////////////////////////////////////////////

bool MachineGamePortSink::TryApply (const GamePortState & target, const GamePortState * lastApplied)
{
    std::shared_lock<std::shared_mutex>  lifetime (m_lifetimeLock, std::try_to_lock);
    GamePortTargets                      targets;
    bool                                 isAvailable = lifetime.owns_lock();



    if (isAvailable)
    {
        targets = m_getTargets();

        WritePaddles (targets, target, lastApplied);
        WriteButtons (targets, target, lastApplied);
    }

    return isAvailable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WritePaddles
//
////////////////////////////////////////////////////////////////////////////////

void MachineGamePortSink::WritePaddles (
    const GamePortTargets  & targets,
    const GamePortState    & target,
    const GamePortState    * lastApplied)
{
    for (int axis = 0; axis < static_cast<int> (target.paddle.size()); axis++)
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
