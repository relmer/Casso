#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
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
    //  The time the shimmer's band takes to cross any object, small or
    //  large, from its top left to its bottom right, and the rest between
    //  one sweep and the next.
    static constexpr int    kSweepMs     = 2000;
    static constexpr int    kRestMs      = 1000;
    static constexpr int    kFadeMs      = 200;

    //  The shimmer: its peak alpha; the share of it the glow outside the band
    //  has; the band's width along (x + y), a share of the object's shorter
    //  side within limits in pixels; and how many widths on each side of the
    //  band's center its stripes span.
    static constexpr double  kShimmerPeakAlpha = 150.0;
    static constexpr double  kShimmerGlowShare = 0.2;
    static constexpr double  kShimmerBandShare = 0.6;
    static constexpr double  kShimmerMinBandPx = 12.0;
    static constexpr double  kShimmerMaxBandPx = 160.0;
    static constexpr double  kShimmerReach     = 3.0;

    //  One device pixel at any scale, as thin as the platter's and the grid's
    //  own lines.
    static constexpr float  kThicknessPx = 1.0f;

    //  A stretch of a track between two turns; `b` passes 1 when the
    //  stretch crosses the index.
    struct Span
    {
        double  a = 0;
        double  b = 0;
    };

    InspectorMarquee (const DiskInspectorPalette & palette, float dashPx, float thicknessPx, int64_t nowMs);

    //  Segments wholly outside the rectangle are skipped; their length counts
    //  toward the dash pattern all the same.
    void   SetCullRect (const RECT & rectPx) { m_cull = rectPx; m_isCulled = true; }

    //  A dashed segment whose dashes start at whole multiples of the dash
    //  length along it, counted from `startPx`; returns the distance at its
    //  end, so a polyline's next segment continues the pattern.
    float  DrawLine (IDxuiPainter & painter, float x0, float y0, float x1, float y1, float startPx) const;

    //  The light a selected object shimmers with at a point: white,
    //  brightest in a band at 45 degrees that sweeps across the object's box
    //  from its top left to its bottom right every kSweepMs.
    static uint32_t  GetShimmer (double nowMs, float xPx, float yPx, const RECT & boxPx);

    //  The band alone, with none of the glow: clear away from it, for light
    //  laid over a line.
    static uint32_t  GetShimmerBand (double nowMs, float xPx, float yPx, const RECT & boxPx);

    //  The shimmer painted over a rectangle of an object with the given box,
    //  the band a stripe at 45 degrees.
    static void  PaintShimmer (IDxuiPainter & painter, float x, float y, float w, float h, double nowMs, const RECT & boxPx);

    //  The band for an object's box: its width, its center along the box's
    //  diagonal, and its strength at a distance from that center.
    static double  GetBandPx     (const RECT & boxPx);
    static double  GetBandCenter (double nowMs, const RECT & boxPx);
    static double  GetBand       (double distance, const RECT & boxPx);

    //  How far into a sweep the moment is, from 0 at rest to 1 within it,
    //  rising over the sweep's first kFadeMs and falling over its last, for
    //  a change that follows the sweep without a jump.
    static double  GetSweepWeight (double nowMs);

    //  A clock in milliseconds fine enough that the shimmer moves a little
    //  every frame; GetTickCount64 advances only every 15.6 ms or so.
    static double    GetClockMs ();

    //  The selection color, or white, at the given alpha, for a hover mark.
    static uint32_t  GetHoverFill () { return 0x38FFFFFFu; }
    static uint32_t  GetHoverLine () { return 0xD0FFFFFFu; }

    //  A color dimmed by half toward the palette's background.
    static uint32_t  Dim (const DiskInspectorPalette & palette, uint32_t argb);

    //  The stretches of a whole track that a file's sectors cover, as turns
    //  of `nibbleTurns`: each sector from its address field to the end of its
    //  data field, and a run of the file's sectors that follow one another
    //  around the track as one stretch, `a` within the first turn.
    static vector<Span>  GetFileSpans (const TrackAnalysis & track, const vector<double> & nibbleTurns, const FileMap & map, int wholeTrack, int file);

private:
    float     m_dashPx      = 1.0f;
    float     m_thicknessPx = 1.0f;
    float     m_phasePx     = 0.0f;
    uint32_t  m_colorA      = 0;
    uint32_t  m_colorB      = 0;
    RECT      m_cull        = {};
    bool      m_isCulled    = false;
};
