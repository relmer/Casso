#include "Pch.h"

#include "Controllers/ControlCapture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Begin
//
//  Starts waiting. Every control the device reports is a candidate, armed
//  only if it is at rest right now.
//
////////////////////////////////////////////////////////////////////////////////

void ControlCapture::Begin (const ControllerSample & baseline, const std::vector<ControlId> & controls)
{
    bool  isNegative = false;



    m_candidates.clear();

    for (const ControlId & control : controls)
    {
        Candidate  candidate;

        candidate.control = control;
        candidate.isArmed = ReadActivation (baseline, control, isNegative) < kReleaseThreshold;

        m_candidates.push_back (candidate);
    }

    m_isActive = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Feed
//
//  One reading while waiting. Returns the first armed control, in the order
//  the device lists them, that this reading activates, and ends the capture.
//  A control that was active at the start arms once it is let go.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<CapturedControl> ControlCapture::Feed (const ControllerSample & sample)
{
    std::optional<CapturedControl>  captured;
    bool                            isNegative = false;
    float                           activation = 0.0f;



    if (!m_isActive)
    {
        return captured;
    }

    for (Candidate & candidate : m_candidates)
    {
        activation = ReadActivation (sample, candidate.control, isNegative);

        if (!candidate.isArmed)
        {
            candidate.isArmed = activation < kReleaseThreshold;
            continue;
        }

        if (!captured.has_value() && activation >= kActivateThreshold)
        {
            captured = CapturedControl { candidate.control, isNegative };
        }
    }

    if (captured.has_value())
    {
        m_isActive = false;
    }

    return captured;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ControlCapture::Cancel()
{
    m_isActive = false;
    m_candidates.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsActive
//
////////////////////////////////////////////////////////////////////////////////

bool ControlCapture::IsActive() const
{
    return m_isActive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadActivation
//
//  How active one control is, from 0 at rest to 1 fully activated: an axis's
//  distance from center in either direction, a trigger's pull, and 1 or 0 for
//  a button or a D-pad direction.
//
////////////////////////////////////////////////////////////////////////////////

float ControlCapture::ReadActivation (const ControllerSample & sample, const ControlId & control, bool & outNegative)
{
    Byte   hatBits = 0;
    Byte   bit     = 0;
    float  value   = 0.0f;



    outNegative = false;

    switch (control.kind)
    {
        case ControlKind::Axis:
            if (control.index < 0 || control.index >= ControllerSample::kAxisCount)
            {
                return 0.0f;
            }

            value       = sample.axes[(size_t) control.index];
            outNegative = value < 0.0f;
            return std::abs (value);

        case ControlKind::Trigger:
            if (control.index < 0 || control.index >= ControllerSample::kTriggerCount)
            {
                return 0.0f;
            }

            return sample.triggers[(size_t) control.index];

        case ControlKind::Button:
            if (control.index < 0 || control.index >= ControllerSample::kButtonCount)
            {
                return 0.0f;
            }

            return sample.buttons.test ((size_t) control.index) ? 1.0f : 0.0f;

        default:
            break;
    }

    if (control.index < 0 || control.index >= ControllerSample::kHatCount)
    {
        return 0.0f;
    }

    hatBits = sample.hats[(size_t) control.index];

    switch (control.kind)
    {
        case ControlKind::DpadUp:    bit = ControllerSample::kHatUp;    break;
        case ControlKind::DpadDown:  bit = ControllerSample::kHatDown;  break;
        case ControlKind::DpadLeft:  bit = ControllerSample::kHatLeft;  break;
        case ControlKind::DpadRight: bit = ControllerSample::kHatRight; break;
        default:                     break;
    }

    return (hatBits & bit) != 0 ? 1.0f : 0.0f;
}
