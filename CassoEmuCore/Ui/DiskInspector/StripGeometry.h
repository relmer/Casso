#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StripSegment / StripGeometry
//
//  The strip shows a track unrolled: the part of the turn from start to
//  start + span across its width, wrapping past the index. A stretch of the
//  turn can show as two segments when it crosses the view's own start, as a
//  selected sector that spans the index does (FR-036).
//
////////////////////////////////////////////////////////////////////////////////

struct StripSegment
{
    float  x0 = 0.0f;
    float  x1 = 0.0f;
};


class StripGeometry
{
public:
    //  About 20 cells of a nominal 51,200-cell track across the strip.
    static constexpr double  kMinSpan = 20.0 / 51200.0;

    StripGeometry (double start, double span, float leftPx, float widthPx);

    float   GetX        (double turn) const;
    double  GetTurn     (float xPx) const;
    int     GetSegments (double turnStart, double turnLength, std::array<StripSegment, 2> & outSegments) const;

    //  The start that keeps the turn under anchorPx where it is after the
    //  span changes to the one given.
    static double  GetStartForZoom   (double start, double span, double newSpan, double anchorFraction);

    //  Each nibble's start as a fraction of the turn, and one entry past the
    //  last for where it ends; computed once per track, since a flux track
    //  needs a running sum of its cells' times.
    static void    BuildNibbleTurns  (const TrackAnalysis & track, vector<double> & outTurns);

private:
    double  m_start  = 0.0;
    double  m_span   = 1.0;
    float   m_left   = 0.0f;
    float   m_width  = 1.0f;
};
