#include "Pch.h"

#include "Ui/DiskInspector/ZoomStops.h"





//  Zooms this close are the same stop, so a zoom that lands a hair off a stop
//  steps from it rather than to it.
static constexpr double  s_kSameStop = 1.001;





////////////////////////////////////////////////////////////////////////////////
//
//  ZoomStops::Build
//
////////////////////////////////////////////////////////////////////////////////

vector<double> ZoomStops::Build (double first, double ratio, const vector<double> & details, double minRatio)
{
    vector<double>  stops  = { first };
    vector<double>  sorted = details;
    double          next   = first * ratio;



    std::sort (sorted.begin(), sorted.end());

    //  The even steps stop short of the first detail zoom by at least
    //  minRatio, so the step onto it is as large as the others.
    while (ratio > 1.0 && !sorted.empty() && next * minRatio <= sorted.front())
    {
        stops.push_back (next);
        next *= ratio;
    }

    for (double detail : sorted)
    {
        if (detail >= stops.back() * minRatio)
        {
            stops.push_back (detail);
        }
    }

    return stops;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZoomStops::Step
//
////////////////////////////////////////////////////////////////////////////////

double ZoomStops::Step (const vector<double> & stops, double zoom, int steps)
{
    int  at    = 0;
    int  count = static_cast<int> (stops.size());



    if (count == 0)
    {
        return zoom;
    }

    if (steps > 0)
    {
        //  The last stop at or below the zoom, then that many in.
        for (at = count - 1; at > 0 && stops[at] > zoom * s_kSameStop; at--)
        {
        }

        at += steps;
    }
    else
    {
        //  The first stop at or above the zoom, then that many out.
        for (at = 0; at < count - 1 && stops[at] * s_kSameStop < zoom; at++)
        {
        }

        at += steps;
    }

    return stops[static_cast<size_t> (std::clamp (at, 0, count - 1))];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZoomStops::GetWheelSteps
//
////////////////////////////////////////////////////////////////////////////////

int ZoomStops::GetWheelSteps (float notches)
{
    int  steps = static_cast<int> (std::lround (notches));



    return (notches > 0.0f) ? std::max (steps, 1) : (notches < 0.0f) ? std::min (steps, -1) : 0;
}