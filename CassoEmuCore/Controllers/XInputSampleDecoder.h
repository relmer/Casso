#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  XInputGamepadState
//
//  The fields of XInput's XINPUT_GAMEPAD, in the same order, so tests can
//  build one without XInput headers.
//
////////////////////////////////////////////////////////////////////////////////

struct XInputGamepadState
{
    WORD   buttons      = 0;
    BYTE   leftTrigger  = 0;
    BYTE   rightTrigger = 0;
    SHORT  thumbLX      = 0;
    SHORT  thumbLY      = 0;
    SHORT  thumbRX      = 0;
    SHORT  thumbRY      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  XInputSampleDecoder
//
//  Turns an Xbox controller state into a normalized controller sample, using
//  one fixed control layout for every Xbox-class controller.
//
////////////////////////////////////////////////////////////////////////////////

class XInputSampleDecoder
{
public:

    static constexpr int  kLeftStickX   = 0;
    static constexpr int  kLeftStickY   = 1;
    static constexpr int  kRightStickX  = 3;
    static constexpr int  kRightStickY  = 4;
    static constexpr int  kButtonCount  = 10;

    static ControllerSample        Decode       (const XInputGamepadState & state);
    static std::vector<ControlId>  ListControls ();
};
