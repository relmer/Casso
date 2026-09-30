#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AxisRoleRules
//
//  Whether an axis springs back when let go, decided once, at enumeration,
//  from what the device reports for it. An axis that stays where it is left
//  plays a paddle the way a real paddle's knob does: at Position, with no
//  center dead zone, calibrated from its ends rather than a center.
//
//  NON-CENTERING IS NARROW ON PURPOSE. Only a Slider, Dial or Throttle usage
//  (or DirectInput's slider type) counts, and a Z axis on a joystick-class
//  device, which by convention is its throttle. A wheel, rudder pedals, and
//  accelerator and brake pedals often spring back, to center or to one end,
//  so they stay centering; a user can still choose Position for one that
//  stays put, since the choice is offered on every DirectInput axis of a
//  device that is not a gamepad.
//
////////////////////////////////////////////////////////////////////////////////

class AxisRoleRules
{
public:

    // HID usage pages and usages, from the HID Usage Tables.
    static constexpr Word  kUsagePageGeneric    = 0x01;
    static constexpr Word  kUsagePageSimulation = 0x02;
    static constexpr Word  kUsageZ              = 0x32;
    static constexpr Word  kUsageSlider         = 0x36;
    static constexpr Word  kUsageDial           = 0x37;
    static constexpr Word  kUsageWheel          = 0x38;
    static constexpr Word  kUsageRudder         = 0xBA;
    static constexpr Word  kUsageThrottle       = 0xBB;
    static constexpr Word  kUsageAccelerator    = 0xC4;
    static constexpr Word  kUsageBrake          = 0xC5;
    static constexpr Word  kUsageSteering       = 0xC8;

    // The role of one DirectInput axis: its HID usage page and usage, zero
    // when the device reports none, whether DirectInput typed it a slider,
    // the sample slot it lands in, and the device's form factor.
    static AxisRole  Classify (Word                  usagePage,
                               Word                  usage,
                               bool                  isSliderType,
                               int                   axisIndex,
                               ControllerFormFactor  formFactor);

    static bool      IsNonCentering (AxisRole role);

    // A control's role as the device's own list gives it; Centering for a
    // control the list does not have.
    static AxisRole  GetRole (const std::vector<ControlId> & controls, const ControlId & control);

    // Which sample axes stay where they are left, and the first such axis.
    static std::bitset<ControllerSample::kAxisCount>  GetNonCenteringAxes  (const std::vector<ControlId> & controls);
    static std::optional<ControlId>                   FindNonCenteringAxis (const std::vector<ControlId> & controls);

private:

    static bool  IsSpringUsage (Word usagePage, Word usage);
};
