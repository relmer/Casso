#include "Pch.h"

#include "Controllers/ControlMapping.h"

#include "Controllers/XInputSampleDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  For
//
//  The primary X and Y axes and the first two buttons, and nothing else.
//
//  IT MUST BE THE PRIMARY AXES, NEVER THE FIRST ONES ENUMERATED. A device can
//  report axes belonging to hardware that is not attached: a flight stick here
//  reported its rudder pedals while they were unplugged, resting pinned at one
//  end of their travel rather than at center. A default mapping that took the
//  first axes it found would land on one of those and hand the guest a control
//  jammed hard over.
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping DefaultMapping::For (const ControllerModelKey & model, const std::vector<ControlId> & controls)
{
    constexpr int   kFirstButton  = 0;
    constexpr int   kSecondButton = 1;
    ControlMapping  mapping;
    ControlId       primaryX      = { ControlKind::Axis, XInputSampleDecoder::kLeftStickX };
    ControlId       primaryY      = { ControlKind::Axis, XInputSampleDecoder::kLeftStickY };
    AxisBinding     xBinding;
    AxisBinding     yBinding;



    UNREFERENCED_PARAMETER (model);

    if (HasControl (controls, primaryX) && HasControl (controls, primaryY))
    {
        xBinding.analog = primaryX;
        yBinding.analog = primaryY;

        mapping.pdl0.push_back (xBinding);
        mapping.pdl1.push_back (yBinding);
    }

    if (HasControl (controls, { ControlKind::Button, kFirstButton }))
    {
        mapping.pb0.push_back ({ { ControlKind::Button, kFirstButton } });
    }

    if (HasControl (controls, { ControlKind::Button, kSecondButton }))
    {
        mapping.pb1.push_back ({ { ControlKind::Button, kSecondButton } });
    }

    return mapping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePaddles
//
//  One player's paddle: axis 0 (an Xbox controller's left stick X) to PDL0
//  with Rate response at the default speed, so the paddle holds where the
//  player leaves it when a self-centering stick is released, and the first
//  button (Xbox: A) to PB0.
//
//  ONE CONTROLLER IS ONE PLAYER. Two people cannot share a controller, so a
//  two-player paddle game takes a controller each, and which paddle each one
//  drives is that controller's assignment, not its profile. PDL1 and PB1 are
//  therefore left unassigned rather than put on a second stick nobody holds.
//
//  The D-pad is left off: a D-pad pair jumps its axis straight to either end,
//  which would throw a paddle to the edge of the screen.
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping DefaultMapping::MakePaddles (const ControllerModelKey & model, const std::vector<ControlId> & controls)
{
    ControlMapping  mapping;
    ControlId       paddle  = { ControlKind::Axis, XInputSampleDecoder::kLeftStickX };
    ControlId       button  = { ControlKind::Button, 0 };
    AxisBinding     binding;



    UNREFERENCED_PARAMETER (model);

    binding.response = AxisResponse::Rate;
    binding.maxSpeed = AxisBinding::kDefaultMaxSpeed;

    if (HasControl (controls, paddle))
    {
        binding.analog = paddle;
        mapping.pdl0.push_back (binding);
    }

    if (HasControl (controls, button))
    {
        mapping.pb0.push_back ({ button });
    }

    return mapping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeJoyport
//
//  Every direction control steers and every fire-like control fires, so an
//  Atari-style game plays however the player holds the controller. The
//  Joyport reads direction switches off PDL0 and PDL1 and fire off PB0; on
//  each axis the control pushed furthest wins, so a stick at rest does not
//  mask the D-pad.
//
//  The primary stick and the first D-pad steer on any controller. A second
//  stick steers too, bound Absolute after the primary one: an Xbox-class
//  controller's right stick, whose layout is fixed, and on a DirectInput
//  gamepad the pair FindSecondStick picks. A DirectInput joystick or wheel
//  gets no second stick, since its other axes can be pedals, a twist or a
//  throttle resting off center, which would hold a direction switch closed.
//  No trigger ever steers: one at rest sits at the end of its travel.
//
//  An Xbox-class controller fires with A, B, X, Y, both bumpers and both
//  triggers, leaving out Back, Start and the stick clicks, which are pressed
//  by accident while steering. Any other controller fires with every button.
//  Nothing is on PB1 or PB2: the Joyport reads neither, so a button left there
//  would do nothing and would show as assigned twice.
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping DefaultMapping::MakeJoyport (
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    constexpr int                                   kDpadHat       = 0;
    constexpr int                                   kXInputBackBtn = 6;
    bool                                            isXInput       = model.kind == ControllerKind::XInput;
    ControlMapping                                  mapping        = For (model, controls);
    ControlId                                       rightX         = { ControlKind::Axis, XInputSampleDecoder::kRightStickX };
    ControlId                                       rightY         = { ControlKind::Axis, XInputSampleDecoder::kRightStickY };
    std::optional<std::pair<ControlId, ControlId>>  secondStick;
    AxisBinding                                     binding;



    if (isXInput && HasControl (controls, rightX) && HasControl (controls, rightY))
    {
        secondStick = std::make_pair (rightX, rightY);
    }
    else if (!isXInput)
    {
        secondStick = FindSecondStick (formFactor, controls);
    }

    if (secondStick.has_value())
    {
        binding.analog = secondStick->first;
        mapping.pdl0.push_back (binding);

        binding.analog = secondStick->second;
        mapping.pdl1.push_back (binding);
    }

    binding      = AxisBinding();
    binding.kind = AxisBindingKind::DigitalPair;

    if (HasControl (controls, { ControlKind::DpadLeft, kDpadHat }) && HasControl (controls, { ControlKind::DpadRight, kDpadHat }))
    {
        binding.negative = { ControlKind::DpadLeft,  kDpadHat };
        binding.positive = { ControlKind::DpadRight, kDpadHat };
        mapping.pdl0.push_back (binding);
    }

    if (HasControl (controls, { ControlKind::DpadUp, kDpadHat }) && HasControl (controls, { ControlKind::DpadDown, kDpadHat }))
    {
        binding.negative = { ControlKind::DpadUp,   kDpadHat };
        binding.positive = { ControlKind::DpadDown, kDpadHat };
        mapping.pdl1.push_back (binding);
    }

    mapping.pb0.clear();
    mapping.pb1.clear();
    mapping.pb2.clear();

    for (const ControlId & control : controls)
    {
        if (control.kind == ControlKind::Button && !(isXInput && control.index >= kXInputBackBtn))
        {
            mapping.pb0.push_back ({ control });
        }
        else if (control.kind == ControlKind::Trigger)
        {
            mapping.pb0.push_back ({ control, ButtonBinding::kTriggerThreshold });
        }
    }

    return mapping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSecondStick
//
//  On a DirectInput gamepad: Z and Rz when it reports both, otherwise Rx and
//  Ry when it reports both, otherwise none. On a joystick or a wheel, none.
//
//  DirectInput has no trigger kind, so a trigger is an axis Casso cannot tell
//  from a stick; the order is what keeps triggers off the switches on the
//  common layouts. A pad that reports Z and Rz uses them for its second stick
//  and Rx and Ry for its triggers, and an Xbox-class pad read through
//  DirectInput reports its triggers as one Z axis with no Rz and its right
//  stick as Rx and Ry.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<ControlId, ControlId>> DefaultMapping::FindSecondStick (
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    ControlId  z  = { ControlKind::Axis, kAxisZ };
    ControlId  rz = { ControlKind::Axis, kAxisRz };
    ControlId  rx = { ControlKind::Axis, kAxisRx };
    ControlId  ry = { ControlKind::Axis, kAxisRy };



    if (formFactor != ControllerFormFactor::Gamepad)
    {
        return std::nullopt;
    }

    if (HasControl (controls, z) && HasControl (controls, rz))
    {
        return std::make_pair (z, rz);
    }

    if (HasControl (controls, rx) && HasControl (controls, ry))
    {
        return std::make_pair (rx, ry);
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasControl
//
////////////////////////////////////////////////////////////////////////////////

bool DefaultMapping::HasControl (const std::vector<ControlId> & controls, const ControlId & control)
{
    return std::find (controls.begin(), controls.end(), control) != controls.end();
}
