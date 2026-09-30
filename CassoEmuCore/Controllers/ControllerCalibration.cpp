#include "Pch.h"

#include "Controllers/ControllerCalibration.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureCenter
//
//  The controller just connected, so this reading is where it rests -- when
//  it is close enough to zero to be a rest at all. Further out it is a stick
//  being held over, and the center is left at zero. Limits learned before are
//  kept and widened to take the reading in; an axis with none takes it as
//  both limits, and the travel floor covers it until the stick moves.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerCalibration::CaptureCenter (const ControllerSample & sample)
{
    size_t  i = 0;



    if (mode == CalibrationMode::User)
    {
        return;
    }

    for (i = 0; i < axes.size(); i++)
    {
        AxisCalibration &  axis  = axes[i];
        float              value = sample.axes[i];

        axis.center       = (std::abs (value) <= kMaxRestOffset) ? value : 0.0f;
        connectReading[i] = value;
        hasMoved[i]       = false;

        if (axis.minimum < axis.maximum)
        {
            axis.minimum = std::min (axis.minimum, value);
            axis.maximum = std::max (axis.maximum, value);
        }
        else
        {
            axis.minimum = value;
            axis.maximum = value;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Observe
//
//  Widens each axis's limits to take in this reading. Outward only.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerCalibration::Observe (const ControllerSample & sample)
{
    size_t  i = 0;



    if (mode == CalibrationMode::User)
    {
        return;
    }

    for (i = 0; i < axes.size(); i++)
    {
        axes[i].minimum = std::min (axes[i].minimum, sample.axes[i]);
        axes[i].maximum = std::max (axes[i].maximum, sample.axes[i]);

        if (std::abs (sample.axes[i] - connectReading[i]) > kMovedThreshold)
        {
            hasMoved[i] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apply
//
//  The same reading with every axis calibrated, an axis that stays where it
//  is left from its limits alone. Triggers, buttons and hats have no rest
//  position to correct and pass through unchanged.
//
////////////////////////////////////////////////////////////////////////////////

ControllerSample ControllerCalibration::Apply (const ControllerSample & sample) const
{
    ControllerSample  calibrated = sample;
    size_t            i          = 0;



    for (i = 0; i < axes.size(); i++)
    {
        calibrated.axes[i] = nonCentering.test (i) ? ApplyFreeAxis (sample.axes[i], axes[i], mode)
                                                   : ApplyAxis     (sample.axes[i], axes[i], mode, hasMoved[i]);
    }

    return calibrated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetToAutomatic
//
//  Discards a user calibration (FR-007a). Everything is relearned, starting
//  with the center at the next connect.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerCalibration::ResetToAutomatic()
{
    mode           = CalibrationMode::Automatic;
    axes           = {};
    connectReading = {};
    hasMoved       = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsValid
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerCalibration::IsValid (const AxisCalibration & axis, CalibrationMode mode)
{
    bool  isInRange = std::isfinite (axis.center)  && std::abs (axis.center)  <= 1.0f &&
                      std::isfinite (axis.minimum) && std::abs (axis.minimum) <= 1.0f &&
                      std::isfinite (axis.maximum) && std::abs (axis.maximum) <= 1.0f;



    if (!isInRange)
    {
        return false;
    }

    if (mode == CalibrationMode::User)
    {
        return axis.minimum < axis.center && axis.center < axis.maximum;
    }

    return axis.minimum <= axis.maximum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAxis
//
//  Distance from the center over the travel on that side, and center for an
//  automatic axis that has not moved since it connected. Automatic travel
//  is never taken as less than the floor, nor as more than the reading can
//  physically go: a center near a rail leaves less than the floor on that
//  side, and an axis pinned AT a rail has none, so it reads center rather
//  than dividing by nothing.
//
////////////////////////////////////////////////////////////////////////////////

float ControllerCalibration::ApplyAxis (float value, const AxisCalibration & axis, CalibrationMode mode, bool hasMoved)
{
    constexpr float  kNoTravel  = 1.0e-6f;
    bool             isNegative = value < axis.center;
    float            observed   = isNegative ? axis.center - axis.minimum : axis.maximum - axis.center;
    float            available  = isNegative ? axis.center + 1.0f         : 1.0f - axis.center;
    float            travel     = observed;



    if (mode == CalibrationMode::Automatic)
    {
        if (!hasMoved)
        {
            return 0.0f;
        }

        travel = std::max (observed, std::min (kMinimumTravel, available));
    }

    if (travel <= kNoTravel)
    {
        return 0.0f;
    }

    return std::clamp ((value - axis.center) / travel, -1.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyFreeAxis
//
//  An axis that stays where it is left, from its limits alone: a user
//  calibration's measured minimum is -1 and its maximum 1, and an automatic
//  one reads as the device reports it, since DirectInput already scales the
//  device's own minimum and maximum to -1 and 1. A captured center or a
//  widening from where it sat at connect would read a throttle left at an end
//  as center.
//
////////////////////////////////////////////////////////////////////////////////

float ControllerCalibration::ApplyFreeAxis (float value, const AxisCalibration & axis, CalibrationMode mode)
{
    constexpr float  kNoTravel = 1.0e-6f;
    float            span      = axis.maximum - axis.minimum;



    if (mode == CalibrationMode::Automatic || span <= kNoTravel)
    {
        return std::clamp (value, -1.0f, 1.0f);
    }

    return std::clamp ((value - axis.minimum) / span * 2.0f - 1.0f, -1.0f, 1.0f);
}
