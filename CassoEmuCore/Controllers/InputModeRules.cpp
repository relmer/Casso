#include "Pch.h"

#include "Controllers/InputModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxisOwner
//
//  A chosen controller owns the axes while it is attached. While it is not,
//  the arrow keys stand in if the user also has them on, and otherwise the
//  axes rest at center -- a selection is kept across a disconnect, so the
//  controller takes them back the moment it returns (FR-008a).
//
//  The stand-in by ANOTHER CONTROLLER (FR-008a) is not decided here: it
//  changes which controller is read, not which kind of source owns the axes,
//  so the service picks the stand-in and this still answers Controller.
//
////////////////////////////////////////////////////////////////////////////////

AxisOwner InputModeRules::GetAxisOwner (const State & state)
{
    if (state.hasController && state.isControllerAttached)
    {
        return AxisOwner::Controller;
    }

    if (state.mousePaddle)
    {
        return AxisOwner::MousePaddle;
    }

    if (state.arrowsJoystick)
    {
        return AxisOwner::ArrowKeys;
    }

    return AxisOwner::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSelectingController
//
//  Choosing a controller gives the axes to it, so the arrow keys and the
//  paddle stop driving them (FR-008).
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSelectingController (State state)
{
    state.hasController  = true;
    state.arrowsJoystick = false;
    state.mousePaddle    = false;

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingArrows
//
//  Turning the arrow keys on takes the axes from the paddle and from the
//  controller, which clears the selection: the user asked for the keys.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingArrows (State state, bool on)
{
    state.arrowsJoystick = on;

    if (on)
    {
        state.mousePaddle   = false;
        state.hasController = false;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingMousePaddle
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingMousePaddle (State state, bool on)
{
    state.mousePaddle = on;

    if (on)
    {
        state.arrowsJoystick = false;
        state.hasController  = false;
    }

    return state;
}
