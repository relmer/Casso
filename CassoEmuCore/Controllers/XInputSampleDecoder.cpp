#include "Pch.h"

#include "Controllers/XInputSampleDecoder.h"

#include "Controllers/DirectInputSampleDecoder.h"





struct XInputButtonBit
{
    WORD  mask;
    int   buttonIndex;
};

// XINPUT_GAMEPAD_* bits (Xinput.h) to sample button indexes: A, B, X, Y,
// LB, RB, Back, Start, left stick click, right stick click.
static constexpr XInputButtonBit  s_kXInputButtons[] =
{
    { 0x1000, 0 },
    { 0x2000, 1 },
    { 0x4000, 2 },
    { 0x8000, 3 },
    { 0x0100, 4 },
    { 0x0200, 5 },
    { 0x0020, 6 },
    { 0x0010, 7 },
    { 0x0040, 8 },
    { 0x0080, 9 },
};

struct XInputDpadBit
{
    WORD  mask;
    Byte  hatBit;
};

// XINPUT_GAMEPAD_DPAD_* bits to hat 0 direction bits.
static constexpr XInputDpadBit  s_kXInputDpad[] =
{
    { 0x0001, ControllerSample::kHatUp    },
    { 0x0002, ControllerSample::kHatDown  },
    { 0x0004, ControllerSample::kHatLeft  },
    { 0x0008, ControllerSample::kHatRight },
};





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
//  Thumb Y is flipped so up reads negative, the same orientation DirectInput
//  sticks report and the Apple II paddle uses (0 is up). Triggers scale from
//  0-255 to [0, 1]. The Guide button is not part of XINPUT_GAMEPAD and is not
//  read.
//
////////////////////////////////////////////////////////////////////////////////

ControllerSample XInputSampleDecoder::Decode (const XInputGamepadState & state)
{
    constexpr float    kTriggerRange = 255.0f;
    constexpr int      kLeftTrigger  = 0;
    constexpr int      kRightTrigger = 1;
    constexpr int      kDpadHat      = 0;
    ControllerSample   sample;



    sample.connected = true;

    sample.axes[kLeftStickX]  =  DirectInputSampleDecoder::NormalizeAxis (state.thumbLX);
    sample.axes[kLeftStickY]  = -DirectInputSampleDecoder::NormalizeAxis (state.thumbLY);
    sample.axes[kRightStickX] =  DirectInputSampleDecoder::NormalizeAxis (state.thumbRX);
    sample.axes[kRightStickY] = -DirectInputSampleDecoder::NormalizeAxis (state.thumbRY);

    sample.triggers[kLeftTrigger]  = static_cast<float> (state.leftTrigger)  / kTriggerRange;
    sample.triggers[kRightTrigger] = static_cast<float> (state.rightTrigger) / kTriggerRange;

    for (const XInputButtonBit & entry : s_kXInputButtons)
    {
        sample.buttons.set (static_cast<size_t> (entry.buttonIndex), (state.buttons & entry.mask) != 0);
    }

    for (const XInputDpadBit & entry : s_kXInputDpad)
    {
        if ((state.buttons & entry.mask) != 0)
        {
            sample.hats[kDpadHat] |= entry.hatBit;
        }
    }

    return sample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListControls
//
//  The same list for every Xbox-class controller: both sticks, both
//  triggers, ten buttons and the D-pad.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ControlId> XInputSampleDecoder::ListControls()
{
    constexpr int           kDpadHat = 0;
    std::vector<ControlId>  controls =
    {
        { ControlKind::Axis,      kLeftStickX  },
        { ControlKind::Axis,      kLeftStickY  },
        { ControlKind::Axis,      kRightStickX },
        { ControlKind::Axis,      kRightStickY },
        { ControlKind::Trigger,   0            },
        { ControlKind::Trigger,   1            },
    };



    for (int button = 0; button < kButtonCount; button++)
    {
        controls.push_back ({ ControlKind::Button, button });
    }

    controls.push_back ({ ControlKind::DpadUp,    kDpadHat });
    controls.push_back ({ ControlKind::DpadDown,  kDpadHat });
    controls.push_back ({ ControlKind::DpadLeft,  kDpadHat });
    controls.push_back ({ ControlKind::DpadRight, kDpadHat });

    return controls;
}
