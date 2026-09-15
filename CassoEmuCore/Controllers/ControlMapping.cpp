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
//  The starting point for two-player paddle games: axis 0 to PDL0 and axis 3
//  to PDL1, both with Rate response at the default speed, so each paddle holds
//  where its player leaves it when a self-centering stick is released. On an
//  Xbox controller that is left stick X and right stick X, one stick per
//  player. On a DirectInput device axis 3 is Rx; a device that does not report
//  it leaves PDL1 unassigned rather than borrowing an axis that may have no
//  hardware behind it.
//
//  The buttons are the default mapping's, the first two buttons (Xbox: A and
//  B), so a profile made from this template differs from the default only in
//  its paddles.
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping DefaultMapping::MakePaddles (const ControllerModelKey & model, const std::vector<ControlId> & controls)
{
    ControlMapping  mapping = For (model, controls);
    ControlId       player1 = { ControlKind::Axis, XInputSampleDecoder::kLeftStickX };
    ControlId       player2 = { ControlKind::Axis, XInputSampleDecoder::kRightStickX };
    AxisBinding     binding;



    binding.response = AxisResponse::Rate;
    binding.maxSpeed = AxisBinding::kDefaultMaxSpeed;

    mapping.pdl0.clear();
    mapping.pdl1.clear();

    if (HasControl (controls, player1))
    {
        binding.analog = player1;
        mapping.pdl0.push_back (binding);
    }

    if (HasControl (controls, player2))
    {
        binding.analog = player2;
        mapping.pdl1.push_back (binding);
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
