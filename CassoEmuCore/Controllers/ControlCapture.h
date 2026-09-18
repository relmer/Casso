#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedControl
//
//  The control a capture assigned. For an axis, which way it was pushed, so a
//  button binding on an axis knows which side of center counts.
//
////////////////////////////////////////////////////////////////////////////////

struct CapturedControl
{
    ControlId  control;
    bool       negativeDirection = false;

    bool operator== (const CapturedControl &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControlCapture
//
//  Press-to-assign: while a target waits for input, the first control the
//  user activates is the one assigned (FR-022).
//
//  A CONTROL ALREADY ACTIVE WHEN WAITING BEGAN DOES NOT COUNT until it has
//  been let go. Otherwise a stick resting a little off center, a trigger
//  sitting partly pulled, or the button the user just clicked with would be
//  assigned before the user touched anything.
//
////////////////////////////////////////////////////////////////////////////////

class ControlCapture
{
public:

    // How far an axis or trigger must travel to count as activated, and how
    // far back it must come to count as let go. The gap keeps a control
    // hovering at one value from flickering between the two.
    static constexpr float  kActivateThreshold = 0.5f;
    static constexpr float  kReleaseThreshold  = 0.25f;

    void                            Begin    (const ControllerSample & baseline, const std::vector<ControlId> & controls);
    std::optional<CapturedControl>  Feed     (const ControllerSample & sample);
    void                            Cancel   ();
    bool                            IsActive () const;

private:

    struct Candidate
    {
        ControlId  control;
        bool       isArmed = false;
    };

    static float  ReadActivation (const ControllerSample & sample, const ControlId & control, bool & outNegative);

    std::vector<Candidate>  m_candidates;
    bool                    m_isActive = false;
};
