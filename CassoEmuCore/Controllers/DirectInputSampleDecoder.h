#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DirectInputJoystickState
//
//  The leading fields of DirectInput's DIJOYSTATE2, in the same order, so the
//  backend can copy a device state into it and tests can build one without
//  DirectInput headers. Axes arrive already ranged to [-32768, 32767].
//
////////////////////////////////////////////////////////////////////////////////

struct DirectInputJoystickState
{
    static constexpr int  kPovCount    = 4;
    static constexpr int  kButtonCount = 128;
    static constexpr int  kSliderCount = 2;

    LONG   x                     = 0;
    LONG   y                     = 0;
    LONG   z                     = 0;
    LONG   rx                    = 0;
    LONG   ry                    = 0;
    LONG   rz                    = 0;
    LONG   slider[kSliderCount]  = {};
    DWORD  pov[kPovCount]        = {};
    BYTE   buttons[kButtonCount] = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectInputObjectLayout
//
//  Which of DIJOYSTATE2's slots a device actually reports, as its object
//  enumeration found them.
//
////////////////////////////////////////////////////////////////////////////////

struct DirectInputObjectLayout
{
    std::bitset<ControllerSample::kAxisCount>  presentAxes;
    int                                        hatCount    = 0;
    int                                        buttonCount = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectInputSampleDecoder
//
//  Turns a DirectInput device state into a normalized controller sample.
//
////////////////////////////////////////////////////////////////////////////////

class DirectInputSampleDecoder
{
public:

    static ControllerSample        Decode        (const DirectInputJoystickState & state, const DirectInputObjectLayout & layout);
    static std::vector<ControlId>  ListControls  (const DirectInputObjectLayout & layout);
    static float                   NormalizeAxis (LONG value);
    static Byte                    DecodePov     (DWORD pov);
};
