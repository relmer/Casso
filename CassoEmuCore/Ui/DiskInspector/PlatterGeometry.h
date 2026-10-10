#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry
//
//  Where things are on the platter. Radii are fractions of the rim's: track
//  0's ring is outermost and quarter track 39.75's innermost, each ring the
//  same width, with the hub inside PlatterRenderer::kInnerFraction. Angles
//  are fractions of a turn clockwise from 12 o'clock, where each track
//  starts while the platter has not turned. Matches the platter shader.
//
////////////////////////////////////////////////////////////////////////////////

struct PlatterHit
{
    bool    isOnDisk     = false;
    int     quarterTrack = -1;
    double  turn         = 0.0;
};


class PlatterGeometry
{
public:
    static double      GetRingWidth      ();
    static double      GetRingOuter      (int quarterTrack);
    static double      GetRingMiddle     (int quarterTrack);
    static int         GetQuarterTrackAt (double radius);
    static double      GetTurnAt         (double dx, double dy);
    static bool        IsGrooveOutside   (int quarterTrack);
    static PlatterHit  HitTest           (const PlatterPlacement & view, POINT pointPx);

    //  The rings and the stretch of the turn a view shows: every ring the
    //  rectangle reaches, and when it does not hold the center, the turns
    //  between its corners, as data turns that may run past 1.
    static void        GetVisibleRings   (const PlatterPlacement & view, const RECT & boundsPx, int & outFirst, int & outLast);
    static bool        GetVisibleTurns   (const PlatterPlacement & view, const RECT & boundsPx, double & outStart, double & outEnd);

    //  What lies at a place along a track.
    static uint32_t    GetCellAtTurn     (const TrackAnalysis & track, double turn);
    static int         GetFieldAt        (const TrackAnalysis & track, uint32_t cell);
    static int         GetSectorAt       (const TrackAnalysis & track, uint32_t cell);
    static int         GetNibbleAt       (const TrackAnalysis & track, uint32_t cell);

private:
    static bool        IsCellInSpan      (uint32_t cell, uint32_t start, uint32_t end, uint32_t count);
};
