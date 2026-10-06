#include "Pch.h"

#include "Widgets/DxuiToolbarBands.h"





//  The top and bottom are laid out first, so their bands run the whole
//  width and the sides' bands run between them.
static constexpr DxuiToolbarDock::Edge  s_kEdgeOrder[] =
{
    DxuiToolbarDock::Edge::Top,
    DxuiToolbarDock::Edge::Bottom,
    DxuiToolbarDock::Edge::Left,
    DxuiToolbarDock::Edge::Right,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::AssignBands
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarBands::AssignBands (const std::vector<DxuiToolbarDock *> & docks)
{
    bool  changed = false;



    for (Edge edge : s_kEdgeOrder)
    {
        int               maxBand = DxuiToolbarDock::kNoBand;
        std::vector<int>  used;

        for (const DxuiToolbarDock * dock : docks)
        {
            if (!dock->floating && dock->edge == edge && dock->HasBand())
            {
                maxBand = (std::max) (maxBand, dock->band);
            }
        }

        for (DxuiToolbarDock * dock : docks)
        {
            if (!dock->floating && dock->edge == edge && !dock->HasBand())
            {
                dock->band = ++maxBand;
                changed    = true;
            }
        }

        for (const DxuiToolbarDock * dock : docks)
        {
            if (!dock->floating && dock->edge == edge)
            {
                used.push_back (dock->band);
            }
        }

        std::sort (used.begin(), used.end());
        used.erase (std::unique (used.begin(), used.end()), used.end());

        //  The bands in use, in order, numbered from 0.
        for (DxuiToolbarDock * dock : docks)
        {
            int  rank = 0;

            if (dock->floating || dock->edge != edge)
            {
                continue;
            }

            rank    = (int) std::distance (used.begin(), std::lower_bound (used.begin(), used.end(), dock->band));
            changed = changed || rank != dock->band;

            dock->band = rank;
        }
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::InsertBand
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarBands::InsertBand (const std::vector<DxuiToolbarDock *> & docks, Edge edge, int band)
{
    for (DxuiToolbarDock * dock : docks)
    {
        if (!dock->floating && dock->edge == edge && dock->band >= band)
        {
            dock->band++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::Arrange
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarBands::Placement DxuiToolbarBands::Arrange (const std::vector<Bar> & bars, const RECT & area, int marginPx)
{
    Placement  placement;



    placement.bars.assign (bars.size(), RECT {});
    placement.inner = area;

    for (Edge edge : s_kEdgeOrder)
    {
        ArrangeEdge (bars, edge, marginPx, placement);
    }

    return placement;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::ArrangeEdge
//
//  One edge's bands, from the edge in, out of what the edges before it
//  left: each as thick as its thickest bar, its bars placed along it and
//  centered across it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarBands::ArrangeEdge (const std::vector<Bar> & bars, Edge edge, int marginPx, Placement & placement)
{
    RECT              inner    = placement.inner;
    bool              vertical = edge == Edge::Left || edge == Edge::Right;
    int               start    = vertical ? inner.top : inner.left + marginPx;
    int               length   = (std::max) (0, vertical ? (int) (inner.bottom - inner.top) : (int) (inner.right - inner.left) - marginPx * 2);
    int               across   = 0;
    std::vector<int>  indexes;



    for (const Bar & bar : bars)
    {
        if (!bar.floating && bar.edge == edge)
        {
            indexes.push_back (bar.band);
        }
    }

    std::sort (indexes.begin(), indexes.end());
    indexes.erase (std::unique (indexes.begin(), indexes.end()), indexes.end());

    for (int index : indexes)
    {
        Band              band;
        std::vector<int>  pos;
        std::vector<int>  len;

        band.edge  = edge;
        band.index = index;

        for (size_t i = 0; i < bars.size(); i++)
        {
            if (!bars[i].floating && bars[i].edge == edge && bars[i].band == index)
            {
                band.bars.push_back (i);
                band.thicknessPx = (std::max) (band.thicknessPx, bars[i].thicknessPx);
                band.hasFill     = band.hasFill || bars[i].fills;
            }
        }

        std::stable_sort (band.bars.begin(), band.bars.end(), [&bars] (size_t a, size_t b)
        {
            int  keyA = bars[a].hasOrderKey ? bars[a].orderKeyPx : bars[a].offsetPx;
            int  keyB = bars[b].hasOrderKey ? bars[b].orderKeyPx : bars[b].offsetPx;

            return keyA < keyB;
        });

        band.rect        = GetBandRect (edge, inner, across, band.thicknessPx, start, length);
        band.isInnermost = index == indexes.back();

        PlaceAlong (bars, band.bars, length, pos, len);

        for (size_t k = 0; k < band.bars.size(); k++)
        {
            const Bar  & bar = bars[band.bars[k]];
            RECT         own = GetBandRect (edge, inner, across + (band.thicknessPx - bar.thicknessPx) / 2, bar.thicknessPx, start + pos[k], len[k]);

            placement.bars[band.bars[k]] = own;
        }

        across += band.thicknessPx;
        placement.bands.push_back (band);
    }

    placement.depthPx[(size_t) edge] = across;

    switch (edge)
    {
    case Edge::Bottom: placement.inner.bottom -= across; break;
    case Edge::Left:   placement.inner.left   += across; break;
    case Edge::Right:  placement.inner.right  -= across; break;
    default:           placement.inner.top    += across; break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::GetBandRect
//
//  A stretch `acrossPx` in from `edge` of `inner` and `thicknessPx` thick,
//  running `lengthPx` along it from `startPx`.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarBands::GetBandRect (Edge edge, const RECT & inner, int acrossPx, int thicknessPx, int startPx, int lengthPx)
{
    int  end = startPx + lengthPx;



    switch (edge)
    {
    case Edge::Bottom: return RECT { startPx, inner.bottom - acrossPx - thicknessPx, end, inner.bottom - acrossPx };
    case Edge::Left:   return RECT { inner.left + acrossPx, startPx, inner.left + acrossPx + thicknessPx, end };
    case Edge::Right:  return RECT { inner.right - acrossPx - thicknessPx, startPx, inner.right - acrossPx, end };
    default:           return RECT { startPx, inner.top + acrossPx, end, inner.top + acrossPx + thicknessPx };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::PlaceAlong
//
//  Where each of a band's bars, in order, starts along it and how long it
//  is. With a bar that fills, the bars run end to end from the start, each
//  that does not fill at its natural length and those that fill sharing
//  what is left, the first taking any pixel over. Without one, each bar
//  keeps its offset where it can: pushed along past the one before it, and
//  back from the band's end, so none overlaps and none runs off the end
//  unless the band is too short for them all.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarBands::PlaceAlong (const std::vector<Bar> & bars, const std::vector<size_t> & members, int lengthPx, std::vector<int> & outPos, std::vector<int> & outLength)
{
    size_t  count   = members.size();
    int     fills   = 0;
    int     fixed   = 0;
    int     spare   = 0;
    int     at      = 0;
    int     limit   = lengthPx;
    int     filled  = 0;



    outPos.assign    (count, 0);
    outLength.assign (count, 0);

    for (size_t k = 0; k < count; k++)
    {
        const Bar  & bar = bars[members[k]];

        fills += bar.fills ? 1 : 0;
        fixed += bar.fills ? 0 : bar.lengthPx;
    }

    if (fills > 0)
    {
        spare = (std::max) (0, lengthPx - fixed);

        for (size_t k = 0; k < count; k++)
        {
            const Bar  & bar   = bars[members[k]];
            int          share = spare / fills + ((filled < spare % fills) ? 1 : 0);

            filled       += bar.fills ? 1 : 0;
            outPos[k]     = at;
            outLength[k]  = std::clamp (bar.fills ? share : bar.lengthPx, 0, (std::max) (0, lengthPx - at));
            at           += outLength[k];
        }

        return;
    }

    for (size_t k = 0; k < count; k++)
    {
        outPos[k]    = (std::max) ((std::max) (bars[members[k]].offsetPx, 0), at);
        outLength[k] = bars[members[k]].lengthPx;
        at           = outPos[k] + outLength[k];
    }

    for (size_t k = count; k > 0; k--)
    {
        outPos[k - 1] = (std::min) (outPos[k - 1], limit - outLength[k - 1]);
        limit         = outPos[k - 1];
    }

    at = 0;

    for (size_t k = 0; k < count; k++)
    {
        outPos[k]    = (std::max) (outPos[k], at);
        outLength[k] = std::clamp (outLength[k], 0, (std::max) (0, lengthPx - outPos[k]));
        at           = outPos[k] + outLength[k];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::PickTarget
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarBands::Target DxuiToolbarBands::PickTarget (int acrossPx, const std::vector<int> & thicknessesPx, int ownBand, bool isOwnAlone, int splitPx)
{
    int               count    = (int) thicknessesPx.size();
    int               boundary = -1;
    int               end      = 0;
    int               over     = count - 1;
    std::vector<int>  starts;
    Target            target;



    for (int thickness : thicknessesPx)
    {
        starts.push_back (end);
        end += thickness;
    }

    if (count == 0 || acrossPx < splitPx)
    {
        boundary = 0;
    }
    else if (acrossPx >= end - splitPx)
    {
        boundary = count;
    }

    for (int k = 1; k < count && boundary < 0; k++)
    {
        if (std::abs (acrossPx - starts[k]) < splitPx)
        {
            boundary = k;
        }
    }

    if (boundary < 0)
    {
        while (over > 0 && acrossPx < starts[over])
        {
            over--;
        }

        target.band = over;
        return target;
    }

    if (isOwnAlone && ownBand >= 0 && (boundary == ownBand || boundary == ownBand + 1))
    {
        target.band = ownBand;
        return target;
    }

    target.band      = boundary;
    target.isNewBand = true;
    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::GetAcrossPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarBands::GetAcrossPx (POINT pointer, Edge edge, const RECT & area)
{
    switch (edge)
    {
    case Edge::Bottom: return area.bottom - pointer.y;
    case Edge::Left:   return pointer.x - area.left;
    case Edge::Right:  return area.right - pointer.x;
    default:           return pointer.y - area.top;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::TryPickEdge
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarBands::TryPickEdge (POINT pointer, const RECT & area, const int (&depthPx)[kEdgeCount], int reachPx, Edge & outEdge)
{
    bool  found     = false;
    int   bestPast  = 0;
    int   bestIn    = 0;



    for (Edge edge : s_kEdgeOrder)
    {
        if (GetAcrossPx (pointer, edge, area) < 0)
        {
            return false;
        }
    }

    for (Edge edge : s_kEdgeOrder)
    {
        int   inward = GetAcrossPx (pointer, edge, area);
        int   depth  = depthPx[(size_t) edge];
        int   past   = (std::max) (0, inward - depth);
        bool  better = !found || past < bestPast || (past == bestPast && inward < bestIn);

        if (inward > depth + reachPx || !better)
        {
            continue;
        }

        found    = true;
        bestPast = past;
        bestIn   = inward;
        outEdge  = edge;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::GetEdgeArea
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarBands::GetEdgeArea (Edge edge, const RECT & area, const int (&depthPx)[kEdgeCount])
{
    RECT  edgeArea = area;



    if (edge == Edge::Left || edge == Edge::Right)
    {
        edgeArea.top    += depthPx[(size_t) Edge::Top];
        edgeArea.bottom -= depthPx[(size_t) Edge::Bottom];
    }

    return edgeArea;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBands::GetThicknesses
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> DxuiToolbarBands::GetThicknesses (const Placement & placement, Edge edge)
{
    std::vector<int>  thicknesses;



    for (const Band & band : placement.bands)
    {
        if (band.edge == edge)
        {
            thicknesses.push_back (band.thicknessPx);
        }
    }

    return thicknesses;
}





