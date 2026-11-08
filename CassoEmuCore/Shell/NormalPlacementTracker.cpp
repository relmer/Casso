#include "Pch.h"

#include "NormalPlacementTracker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NormalPlacementTracker::SetExpectedRect
//
//  A saved placement, waiting to be applied once the window is shown
//  restored. The invisible border that turns a visible rect into a window
//  rect is certain only then: a maximized window reports a wider one than it
//  has when restored, and a window whose frame was never recalculated
//  reports none, so either measurement shrinks the window by the difference
//  on every restart.
//
////////////////////////////////////////////////////////////////////////////////

void NormalPlacementTracker::SetExpectedRect (const RECT & visible)
{
    m_normal    = visible;
    m_hasNormal = true;
    m_pending   = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalPlacementTracker::TryTakePending
//
//  Hands back the expected rect the first time the window is shown restored,
//  and only that once.
//
////////////////////////////////////////////////////////////////////////////////

bool NormalPlacementTracker::TryTakePending (bool restored, RECT & outVisible)
{
    bool  take = m_pending && restored;



    if (take)
    {
        outVisible = m_normal;
        m_pending  = false;
    }

    return take;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalPlacementTracker::TryGetNormalRect
//
//  The visible rect to save. A restored window's own rect is the answer, and
//  is remembered. A maximized or minimized window's is not: its rect is the
//  monitor's or the parking spot's, and converting its normal WINDOW rect to
//  a visible one needs the restored border, which cannot be measured until
//  the window is restored. So the last rect seen while restored is used, or
//  the expected one if the window has not been restored yet.
//
////////////////////////////////////////////////////////////////////////////////

bool NormalPlacementTracker::TryGetNormalRect (bool restored, const RECT & visibleNow, RECT & outVisible)
{
    if (restored && !m_pending)
    {
        m_normal    = visibleNow;
        m_hasNormal = true;
    }

    if (m_hasNormal)
    {
        outVisible = m_normal;
    }

    return m_hasNormal;
}
