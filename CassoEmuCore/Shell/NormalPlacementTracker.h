#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NormalPlacementTracker
//
//  Holds the visible rect a window has when it is neither maximized nor
//  minimized, so that rect can be saved while the window is maximized and
//  applied once the window is first shown restored.
//
////////////////////////////////////////////////////////////////////////////////

class NormalPlacementTracker
{
public:
    void  SetExpectedRect   (const RECT & visible);
    bool  TryTakePending    (bool restored, RECT & outVisible);
    bool  TryGetNormalRect  (bool restored, const RECT & visibleNow, RECT & outVisible);

private:
    RECT  m_normal     = {};
    bool  m_hasNormal  = false;
    bool  m_pending    = false;
};
