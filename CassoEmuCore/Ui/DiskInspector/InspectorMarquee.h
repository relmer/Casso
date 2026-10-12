#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee
//
//  The marquee around the selected file's sectors (FR-090): dashes in the
//  selection color and its contrasting black or white, marching along the
//  outline, and the color of everything else dimmed by half.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorMarquee
{
public:
    static constexpr float  kDashDip     = 4.0f;
    static constexpr int    kStepMs      = 80;

    //  One device pixel at any scale, as thin as the platter's and the grid's
    //  own lines.
    static constexpr float  kThicknessPx = 1.0f;

    InspectorMarquee (const DiskInspectorPalette & palette, float dashPx, float thicknessPx, int64_t nowMs);

    //  A dashed segment whose dashes start at whole multiples of the dash
    //  length along it, counted from `startPx`; returns the distance at its
    //  end, so a polyline's next segment continues the pattern.
    float  DrawLine (IDxuiPainter & painter, float x0, float y0, float x1, float y1, float startPx) const;

    //  A color dimmed by half toward the palette's background.
    static uint32_t  Dim (const DiskInspectorPalette & palette, uint32_t argb);

private:
    float     m_dashPx      = 1.0f;
    float     m_thicknessPx = 1.0f;
    float     m_phasePx     = 0.0f;
    uint32_t  m_colorA      = 0;
    uint32_t  m_colorB      = 0;
};
