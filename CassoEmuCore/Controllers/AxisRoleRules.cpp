#include "Pch.h"

#include "Controllers/AxisRoleRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Classify
//
//  A usage that says what the axis is wins over its slot: a Rudder usage in
//  a slider slot is still pedals. Only with no usage that decides does the
//  slot count, and then only a joystick's Z, which is its throttle by
//  convention; a gamepad's Z is usually a second stick or its triggers, and
//  a wheel's is a pedal.
//
////////////////////////////////////////////////////////////////////////////////

AxisRole AxisRoleRules::Classify (
    Word                  usagePage,
    Word                  usage,
    bool                  isSliderType,
    int                   axisIndex,
    ControllerFormFactor  formFactor)
{
    constexpr int  kAxisZ     = 2;
    bool           isGeneric  = usagePage == kUsagePageGeneric;
    bool           isNoUsage  = usagePage == 0 && usage == 0;
    bool           isZ        = (isGeneric && usage == kUsageZ) || (isNoUsage && axisIndex == kAxisZ);



    if (usagePage == kUsagePageSimulation && usage == kUsageThrottle)
    {
        return AxisRole::Throttle;
    }

    if (IsSpringUsage (usagePage, usage))
    {
        return AxisRole::Centering;
    }

    if (isGeneric && usage == kUsageDial)
    {
        return AxisRole::Dial;
    }

    if ((isGeneric && usage == kUsageSlider) || isSliderType)
    {
        return AxisRole::Slider;
    }

    if (isZ && formFactor == ControllerFormFactor::Joystick)
    {
        return AxisRole::JoystickZ;
    }

    return AxisRole::Centering;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSpringUsage
//
//  A wheel, rudder pedals, and accelerator and brake pedals, which often
//  spring back to center or to one end.
//
////////////////////////////////////////////////////////////////////////////////

bool AxisRoleRules::IsSpringUsage (Word usagePage, Word usage)
{
    if (usagePage == kUsagePageGeneric)
    {
        return usage == kUsageWheel;
    }

    if (usagePage == kUsagePageSimulation)
    {
        return usage == kUsageRudder || usage == kUsageAccelerator || usage == kUsageBrake || usage == kUsageSteering;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsNonCentering
//
////////////////////////////////////////////////////////////////////////////////

bool AxisRoleRules::IsNonCentering (AxisRole role)
{
    return role != AxisRole::Centering;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRole
//
////////////////////////////////////////////////////////////////////////////////

AxisRole AxisRoleRules::GetRole (const std::vector<ControlId> & controls, const ControlId & control)
{
    auto  found = std::find (controls.begin(), controls.end(), control);



    return (found != controls.end()) ? found->role : AxisRole::Centering;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNonCenteringAxes
//
////////////////////////////////////////////////////////////////////////////////

std::bitset<ControllerSample::kAxisCount> AxisRoleRules::GetNonCenteringAxes (const std::vector<ControlId> & controls)
{
    std::bitset<ControllerSample::kAxisCount>  axes;



    for (const ControlId & control : controls)
    {
        if (control.kind == ControlKind::Axis && control.index >= 0 && control.index < ControllerSample::kAxisCount && IsNonCentering (control.role))
        {
            axes.set ((size_t) control.index);
        }
    }

    return axes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindNonCenteringAxis
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ControlId> AxisRoleRules::FindNonCenteringAxis (const std::vector<ControlId> & controls)
{
    for (const ControlId & control : controls)
    {
        if (control.kind == ControlKind::Axis && IsNonCentering (control.role))
        {
            return control;
        }
    }

    return std::nullopt;
}
