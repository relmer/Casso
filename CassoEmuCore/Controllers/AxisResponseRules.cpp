#include "Pch.h"

#include "Controllers/AxisResponseRules.h"

#include "Controllers/AxisRoleRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IsPositionOffered
//
////////////////////////////////////////////////////////////////////////////////

bool AxisResponseRules::IsPositionOffered (
    ProfileMode              mode,
    const AxisBinding      & binding,
    ControllerKind           kind,
    ControllerFormFactor     formFactor)
{
    return mode                == ProfileMode::Paddle         &&
           binding.kind        == AxisBindingKind::Analog     &&
           binding.analog.kind == ControlKind::Axis           &&
           kind                == ControllerKind::DirectInput &&
           formFactor          != ControllerFormFactor::Gamepad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAllowedResponse
//
////////////////////////////////////////////////////////////////////////////////

AxisResponse AxisResponseRules::GetAllowedResponse (
    ProfileMode              mode,
    const AxisBinding      & binding,
    ControllerKind           kind,
    ControllerFormFactor     formFactor)
{
    AxisResponse  response = binding.response;



    if (mode == ProfileMode::Joystick)
    {
        response = AxisResponse::Absolute;
    }
    else if (mode == ProfileMode::Paddle && !IsPositionOffered (mode, binding, kind, formFactor))
    {
        response = AxisResponse::Rate;
    }

    return response;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNewResponse
//
////////////////////////////////////////////////////////////////////////////////

AxisResponse AxisResponseRules::GetNewResponse (
    ProfileMode              mode,
    const AxisBinding      & binding,
    ControllerKind           kind,
    ControllerFormFactor     formFactor,
    AxisRole                 role)
{
    AxisResponse  response = binding.response;
    bool          isKnob   = IsPositionOffered (mode, binding, kind, formFactor) && AxisRoleRules::IsNonCentering (role);



    if (mode == ProfileMode::Joystick)
    {
        response = AxisResponse::Absolute;
    }
    else if (mode == ProfileMode::Paddle)
    {
        response = isKnob ? AxisResponse::Absolute : AxisResponse::Rate;
    }

    return response;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Normalize
//
////////////////////////////////////////////////////////////////////////////////

void AxisResponseRules::Normalize (
    ControlMapping         & mapping,
    ProfileMode              mode,
    ControllerKind           kind,
    ControllerFormFactor     formFactor)
{
    for (std::vector<AxisBinding> * list : { &mapping.pdl0, &mapping.pdl1, &mapping.pdl2, &mapping.pdl3 })
    {
        for (AxisBinding & binding : *list)
        {
            if (binding.kind == AxisBindingKind::Analog)
            {
                binding.response = GetAllowedResponse (mode, binding, kind, formFactor);
            }
        }
    }
}
