#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlide
//
//  A distance still to travel, run down to zero over the time and the curve
//  a menu opens with. A default-constructed slide has nothing to travel and
//  is done.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSlide
{
public:

    static DxuiSlide  Start     (float distancePx, int64_t startMs, bool isAnimated);

    float             GetOffset (int64_t nowMs) const;
    bool              IsDone    (int64_t nowMs) const;
    int64_t           GetEndMs  () const;

private:

    float    m_distancePx = 0.0f;
    int64_t  m_startMs    = 0;
    bool     m_isAnimated = false;
};
