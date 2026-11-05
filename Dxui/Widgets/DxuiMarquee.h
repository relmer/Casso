#pragma once

#include "Pch.h"


class IDxuiPainter;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMarquee
//
//  A selection's outline as marching dashes: dashes in two colors, one light
//  and one dark, so one of them is visible on any fill, moving a pixel every
//  kStepMs along the outline. The disk inspector draws its selected area the
//  same way.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiMarquee
{
public:
    static constexpr float     kDashDip   = 4.0f;
    static constexpr int       kStepMs    = 80;
    static constexpr uint32_t  kLightArgb = 0xFFFFFFFFu;
    static constexpr uint32_t  kDarkArgb  = 0xFF000000u;

    //  How far the dashes have marched at a moment, from 0 up to two dashes.
    static float  GetPhasePx (float dashPx, int64_t nowMs);

    //  The outline of a rectangle, clockwise from its top left corner, with
    //  the dashes counted along the whole outline so they run on round the
    //  corners. Returns the dashes drawn.
    static int    DrawRect   (IDxuiPainter & painter, float x, float y, float w, float h, float dashPx, float thicknessPx, int64_t nowMs);
};
