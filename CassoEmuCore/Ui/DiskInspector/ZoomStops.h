#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZoomStops
//
//  The zooms a view steps through: even steps by a ratio while each step
//  adds detail, then straight to each zoom at which more detail appears,
//  such as a nibble's value or a cell's bit. The last stop is the most the
//  view zooms. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class ZoomStops
{
public:
    //  Steps from `first` by `ratio` up to the first detail zoom, then each
    //  detail zoom in order. A stop within `minRatio` of the one before is
    //  left out, so no step is too small to see.
    static vector<double>  Build (double first, double ratio, const vector<double> & details, double minRatio);

    //  The stop `steps` stops in (positive) or out (negative) from a zoom; a
    //  zoom between two stops steps to the nearer one in the step's
    //  direction. Clamped to the first and last stops.
    static double  Step (const vector<double> & stops, double zoom, int steps);

    //  The stops for a wheel's notches: one a notch, and at least one for any
    //  turn, so a fraction of a notch from a touchpad is a step too.
    static int  GetWheelSteps (float notches);
};
