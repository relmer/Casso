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
//  HasControl
//
////////////////////////////////////////////////////////////////////////////////

bool DefaultMapping::HasControl (const std::vector<ControlId> & controls, const ControlId & control)
{
    return std::find (controls.begin(), controls.end(), control) != controls.end();
}
