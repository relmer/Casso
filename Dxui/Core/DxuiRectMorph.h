#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph
//
//  A rectangle moving and resizing from one place to another over a short
//  time on the ease-out curve, as a pane shrinks into an edge tab when it is
//  pinned. A default-constructed morph is done.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiRectMorph
{
public:
    static constexpr int64_t  kDurationMs = 180;

    static DxuiRectMorph  Start       (const RECT & from, const RECT & to, int64_t startMs, bool isAnimated);
    static RECT           Interpolate (const RECT & from, const RECT & to, float t);

    RECT                  GetRect     (int64_t nowMs) const;
    bool                  IsDone      (int64_t nowMs) const;
    int64_t               GetEndMs    () const;

private:
    static bool           IsEmpty     (const RECT & rect);

    RECT     m_from       = {};
    RECT     m_to         = {};
    int64_t  m_startMs    = 0;
    bool     m_isAnimated = false;
};
