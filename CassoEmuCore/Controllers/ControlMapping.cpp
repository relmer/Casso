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
//  The primary stick and the first D-pad steer on any controller. The right
//  stick is added only on an Xbox-class controller, whose layout is fixed:
//  on a DirectInput device the same axis indexes can be pedals or a throttle
//  resting hard over, which would hold a direction switch closed.
//
//  An Xbox-class controller fires with A, B, X, Y, both bumpers and both
//  triggers, leaving out Back, Start and the stick clicks, which are pressed
//  by accident while steering. Any other controller fires with every button.
//  B stays on PB1 as it is in the Default.
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping DefaultMapping::MakeJoyport (const ControllerModelKey & model, const std::vector<ControlId> & controls)
{
    constexpr int   kDpadHat       = 0;
    constexpr int   kXInputBackBtn = 6;
    bool            isXInput       = model.kind == ControllerKind::XInput;
    ControlMapping  mapping        = For (model, controls);
    ControlId       rightX         = { ControlKind::Axis, XInputSampleDecoder::kRightStickX };
    ControlId       rightY         = { ControlKind::Axis, XInputSampleDecoder::kRightStickY };
    AxisBinding     binding;



    if (isXInput && HasControl (controls, rightX) && HasControl (controls, rightY))
    {
        binding.analog = rightX;
        mapping.pdl0.push_back (binding);

        binding.analog = rightY;
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
//  HasControl
//
////////////////////////////////////////////////////////////////////////////////

bool DefaultMapping::HasControl (const std::vector<ControlId> & controls, const ControlId & control)
{
    return std::find (controls.begin(), controls.end(), control) != controls.end();
}
