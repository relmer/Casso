#include "Pch.h"

#include "Cassque/Model/FocusRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FocusRing::BuildStops
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FocusStop> FocusRing::BuildStops (const std::vector<bool> & toolbarEnabled, bool previewVisible)
{
    std::vector<FocusStop>  stops;



    for (size_t i = 0; i < toolbarEnabled.size(); i++)
    {
        if (toolbarEnabled[i])
        {
            stops.push_back (FocusStop { FocusStop::Kind::ToolbarEntry, (int) i });
        }
    }

    stops.push_back (FocusStop { FocusStop::Kind::Tabs });
    stops.push_back (FocusStop { FocusStop::Kind::Tree });
    stops.push_back (FocusStop { FocusStop::Kind::List });

    if (previewVisible)
    {
        stops.push_back (FocusStop { FocusStop::Kind::Preview });
    }

    return stops;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusRing::GetNext
//
////////////////////////////////////////////////////////////////////////////////

FocusStop FocusRing::GetNext (const std::vector<FocusStop> & stops, const FocusStop & current, bool forward)
{
    size_t  count = stops.size();
    size_t  at    = count;



    if (count == 0)
    {
        return current;
    }

    for (size_t i = 0; i < count && at == count; i++)
    {
        if (stops[i] == current)
        {
            at = i;
        }
    }

    if (at == count)
    {
        return forward ? stops.front() : stops.back();
    }

    return forward ? stops[(at + 1) % count] : stops[(at + count - 1) % count];
}
