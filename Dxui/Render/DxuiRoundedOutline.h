#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRoundedOutline
//
//  A closed outline whose corners are rounded, as line segments for either
//  a painter or a text renderer to draw: each side stops a corner's radius
//  short of its ends, and a short curve through the corner joins it to the
//  next. The radius shrinks to half the shorter side at a corner.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiRoundedOutline
{
public:
    struct Segment
    {
        DxuiPointF  from;
        DxuiPointF  to;
    };

    static std::vector<Segment>  GetSegments (const std::vector<DxuiPointF> & points, float radius);
};
