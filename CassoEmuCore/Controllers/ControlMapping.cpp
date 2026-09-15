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
//  HasControl
//
////////////////////////////////////////////////////////////////////////////////

bool DefaultMapping::HasControl (const std::vector<ControlId> & controls, const ControlId & control)
{
    return std::find (controls.begin(), controls.end(), control) != controls.end();
}
