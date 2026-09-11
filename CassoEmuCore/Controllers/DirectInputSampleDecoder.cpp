#include "Pch.h"

#include "Controllers/DirectInputSampleDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
//  Only the slots the device reports are read; the rest stay at rest, so a
//  stick with no Z axis never reads DIJOYSTATE2's zero there as a control
//  being held at center.
//
////////////////////////////////////////////////////////////////////////////////

ControllerSample DirectInputSampleDecoder::Decode (
    const DirectInputJoystickState  & state,
    const DirectInputObjectLayout   & layout)
{
    constexpr BYTE     kButtonDownBit = 0x80;
    ControllerSample   sample;
    const LONG         axisValues[ControllerSample::kAxisCount] =
    {
        state.x, state.y, state.z, state.rx, state.ry, state.rz, state.slider[0], state.slider[1],
    };
    int                hatCount    = std::clamp (layout.hatCount,    0, DirectInputJoystickState::kPovCount);
    int                buttonCount = std::clamp (layout.buttonCount, 0, DirectInputJoystickState::kButtonCount);



    sample.connected = true;

    for (int axis = 0; axis < ControllerSample::kAxisCount; axis++)
    {
        if (layout.presentAxes.test (static_cast<size_t> (axis)))
        {
            sample.axes[axis] = NormalizeAxis (axisValues[axis]);
        }
    }

    for (int hat = 0; hat < hatCount; hat++)
    {
        sample.hats[hat] = DecodePov (state.pov[hat]);
    }

    for (int button = 0; button < buttonCount; button++)
    {
        sample.buttons.set (static_cast<size_t> (button), (state.buttons[button] & kButtonDownBit) != 0);
    }

    return sample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListControls
//
//  Every control the device reports, in sample order: axes, then buttons,
//  then the four directions of each hat. DirectInput has no triggers of its
//  own; a trigger on such a device is one of its axes.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ControlId> DirectInputSampleDecoder::ListControls (const DirectInputObjectLayout & layout)
{
    constexpr ControlKind   kDpadKinds[] = { ControlKind::DpadUp, ControlKind::DpadDown, ControlKind::DpadLeft, ControlKind::DpadRight };
    std::vector<ControlId>  controls;
    int                     hatCount     = std::clamp (layout.hatCount,    0, ControllerSample::kHatCount);
    int                     buttonCount  = std::clamp (layout.buttonCount, 0, ControllerSample::kButtonCount);



    for (int axis = 0; axis < ControllerSample::kAxisCount; axis++)
    {
        if (layout.presentAxes.test (static_cast<size_t> (axis)))
        {
            controls.push_back ({ ControlKind::Axis, axis });
        }
    }

    for (int button = 0; button < buttonCount; button++)
    {
        controls.push_back ({ ControlKind::Button, button });
    }

    for (int hat = 0; hat < hatCount; hat++)
    {
        for (ControlKind kind : kDpadKinds)
        {
            controls.push_back ({ kind, hat });
        }
    }

    return controls;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeAxis
//
//  [-32768, 32767] to [-1, 1], scaling each side by its own magnitude so 0
//  maps to exactly 0 and both extremes reach exactly -1 and 1.
//
////////////////////////////////////////////////////////////////////////////////

float DirectInputSampleDecoder::NormalizeAxis (LONG value)
{
    constexpr float  kNegativeRange = 32768.0f;
    constexpr float  kPositiveRange = 32767.0f;
    float            normalized     = 0.0f;



    if (value < 0)
    {
        normalized = std::max (static_cast<float> (value) / kNegativeRange, -1.0f);
    }
    else
    {
        normalized = std::min (static_cast<float> (value) / kPositiveRange, 1.0f);
    }

    return normalized;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodePov
//
//  A POV hat reports hundredths of a degree clockwise from north, or a low
//  word of 0xFFFF when centered. Each of the eight 45-degree sectors, centered
//  on a compass point, sets one direction bit for a cardinal and two for a
//  diagonal.
//
////////////////////////////////////////////////////////////////////////////////

Byte DirectInputSampleDecoder::DecodePov (DWORD pov)
{
    constexpr DWORD  kCenteredLowWord = 0xFFFF;
    constexpr DWORD  kFullCircle      = 36000;
    constexpr DWORD  kSectorWidth     = 4500;
    constexpr DWORD  kHalfSector      = kSectorWidth / 2;
    constexpr Byte   kSectorBits[]    =
    {
        ControllerSample::kHatUp,
        ControllerSample::kHatUp   | ControllerSample::kHatRight,
        ControllerSample::kHatRight,
        ControllerSample::kHatDown | ControllerSample::kHatRight,
        ControllerSample::kHatDown,
        ControllerSample::kHatDown | ControllerSample::kHatLeft,
        ControllerSample::kHatLeft,
        ControllerSample::kHatUp   | ControllerSample::kHatLeft,
    };
    constexpr DWORD  kSectorCount     = static_cast<DWORD> (std::size (kSectorBits));
    Byte             bits             = 0;
    DWORD            sector           = 0;



    if (LOWORD (pov) != kCenteredLowWord && pov < kFullCircle)
    {
        sector = ((pov + kHalfSector) / kSectorWidth) % kSectorCount;
        bits   = kSectorBits[sector];
    }

    return bits;
}
