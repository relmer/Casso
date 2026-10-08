#include "Pch.h"

#include "Window/DxuiToolbarDockGroup.h"
#include "Window/DxuiToolbarHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::Add
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::Add (DxuiToolbarHost * host)
{
    Member  member;



    Remove (host);

    member.host = host;
    m_members.push_back (member);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::Remove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::Remove (DxuiToolbarHost * host)
{
    std::erase_if (m_members, [host] (const Member & member) { return member.host == host; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::Layout
//
//  Every host readies its toolbar for the layout and says whether it is
//  docked; the docked places get their bands, the bands are laid out, and
//  each docked toolbar takes its rectangle.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDockGroup::Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler)
{
    std::vector<DxuiToolbarDock *>       docks;
    std::vector<DxuiToolbarBands::Bar>   bars;
    std::vector<bool>                    docked;
    RECT                                 inner = area;



    m_area     = area;
    m_marginPx = scaler.ToPx (DxuiToolbarHost::kMarginDp);
    m_dpi      = (std::max) ((int) scaler.GetDpi(), 1);

    for (Member & member : m_members)
    {
        bool  isDocked = member.host->BeginLayout (area, hostClient, scaler);

        docked.push_back (isDocked);

        if (isDocked)
        {
            docks.push_back (&member.host->m_dock);
        }
    }

    DxuiToolbarBands::AssignBands (docks);

    for (size_t i = 0; i < m_members.size(); i++)
    {
        DxuiToolbarBands::Bar  bar = MakeBar (m_members[i], scaler);

        bar.floating = !docked[i];
        bars.push_back (bar);
    }

    m_placement = DxuiToolbarBands::Arrange (bars, area, m_marginPx);
    inner       = m_placement.inner;

    for (size_t i = 0; i < m_members.size(); i++)
    {
        Member  & member = m_members[i];

        member.placed = docked[i];

        if (!docked[i])
        {
            continue;
        }

        member.edge = bars[i].edge;
        member.band = bars[i].band;
        member.rect = m_placement.bars[i];

        member.host->PlaceDocked (member.rect, GetBandRect (member.edge, member.band));
    }

    ShareEdge (inner);

    return inner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::MakeBar
//
//  A host's toolbar as a bar to lay out. Its order in its band comes from
//  where it was laid out last, its middle along the edge, so a toolbar
//  dragged along the band passes another as its own middle passes that
//  one's. The one dragged goes by where the drag holds it; one new to its
//  band goes by its offset.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarBands::Bar DxuiToolbarDockGroup::MakeBar (Member & member, const DxuiDpiScaler & scaler)
{
    DxuiToolbarBands::Bar     bar;
    const DxuiToolbarDock  &  dock     = member.host->m_dock;
    bool                      vertical = dock.IsVertical();
    int                       lastLen  = vertical ? member.rect.bottom - member.rect.top : member.rect.right - member.rect.left;
    int                       lastAt   = vertical ? member.rect.top : member.rect.left;
    bool                      isSame   = member.placed && member.edge == dock.edge && member.band == dock.band;



    bar.edge        = dock.edge;
    bar.band        = (std::max) (dock.band, 0);
    bar.offsetPx    = scaler.ToPx (dock.offsetDip);
    bar.fills       = member.host->m_fillsEdge;
    bar.thicknessPx = scaler.ToPx (member.host->GetBandDipOfBar());
    bar.lengthPx    = (member.host->m_toolbar != nullptr) ? member.host->m_toolbar->GetNaturalLengthPx (scaler) : 0;

    if (member.host->m_dragging)
    {
        bar.hasOrderKey = true;
        bar.orderKeyPx  = bar.offsetPx + ((lastLen > 0) ? lastLen : bar.lengthPx) / 2;
    }
    else if (isSame && lastLen > 0)
    {
        bar.hasOrderKey = true;
        bar.orderKeyPx  = lastAt - GetEdgeStartPx (dock.edge) + lastLen / 2;
    }

    return bar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::ShareEdge
//
//  A host with a dock site shares the innermost band of its edge with that
//  site's auto-hidden tabs, as long as nothing in the band fills it: the
//  band stays in the area the site is laid out in, and the tabs run beside
//  the band's toolbars. Otherwise the site has its own strip for its tabs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::ShareEdge (RECT & inner)
{
    for (const Member & member : m_members)
    {
        DxuiDockSite                   * site  = member.host->m_dockSite;
        const DxuiToolbarBands::Band   * band  = nullptr;
        bool                             down  = false;
        long                             start = LONG_MAX;
        long                             end   = LONG_MIN;
        int                              thick = 0;

        if (site == nullptr)
        {
            continue;
        }

        for (const DxuiToolbarBands::Band & b : m_placement.bands)
        {
            if (member.placed && b.edge == member.edge && b.index == member.band)
            {
                band = &b;
            }
        }

        if (band == nullptr || !band->isInnermost || band->hasFill)
        {
            site->ClearEdgeShare();
            continue;
        }

        down  = member.edge == Edge::Left || member.edge == Edge::Right;
        thick = band->thicknessPx;

        for (size_t index : band->bars)
        {
            const RECT  & rect = m_placement.bars[index];

            start = (std::min) (start, down ? rect.top    : rect.left);
            end   = (std::max) (end,   down ? rect.bottom : rect.right);
        }

        //  The site keeps the whole band on every edge: its own margin, not
        //  a gap left outside it, keeps its panes off the toolbars.
        site->SetEdgeShare (DxuiToolbarHost::EdgeToDockSide (member.edge), thick, start, end);

        switch (member.edge)
        {
        case Edge::Bottom: inner.bottom += thick; break;
        case Edge::Left:   inner.left   -= thick; break;
        case Edge::Right:  inner.right  += thick; break;
        default:           inner.top    -= thick; break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetDepthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarDockGroup::GetDepthPx (Edge edge) const
{
    return m_placement.depthPx[(size_t) edge];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetEdgeArea
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDockGroup::GetEdgeArea (Edge edge) const
{
    return DxuiToolbarBands::GetEdgeArea (edge, m_area, m_placement.depthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetEdgeStartPx
//
//  Where along an edge its offsets start: a margin in from the left across
//  the top or bottom, below the top bands down a side.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarDockGroup::GetEdgeStartPx (Edge edge) const
{
    RECT  edgeArea = GetEdgeArea (edge);



    return (edge == Edge::Left || edge == Edge::Right) ? edgeArea.top : edgeArea.left + m_marginPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetBandThicknesses
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> DxuiToolbarDockGroup::GetBandThicknesses (Edge edge) const
{
    return DxuiToolbarBands::GetThicknesses (m_placement, edge);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetBandRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDockGroup::GetBandRect (Edge edge, int band) const
{
    for (const DxuiToolbarBands::Band & b : m_placement.bands)
    {
        if (b.edge == edge && b.index == band)
        {
            return b.rect;
        }
    }

    return RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetDropRect
//
//  A new band runs the edge's whole stretch at the boundary it is made at,
//  as the band it becomes will.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDockGroup::GetDropRect (Edge edge, int band, bool isNewBand, int thicknessPx) const
{
    std::vector<int>  thicknesses = GetBandThicknesses (edge);
    RECT              edgeArea    = GetEdgeArea (edge);
    bool              isSide      = edge == Edge::Left || edge == Edge::Right;
    int               across      = 0;
    int               start       = isSide ? edgeArea.top : edgeArea.left + m_marginPx;
    int               length      = isSide ? edgeArea.bottom - edgeArea.top : edgeArea.right - edgeArea.left - m_marginPx * 2;



    if (!isNewBand)
    {
        return GetBandRect (edge, band);
    }

    for (int i = 0; i < band && i < (int) thicknesses.size(); i++)
    {
        across += thicknesses[(size_t) i];
    }

    return DxuiToolbarBands::GetBandRect (edge, m_area, across, thicknessPx, start, length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::IsAloneInBand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDockGroup::IsAloneInBand (const DxuiToolbarHost * host) const
{
    const DxuiToolbarDock  & dock   = host->m_dock;
    int                      others = 0;



    for (const Member & member : m_members)
    {
        if (member.host != host && member.placed && member.edge == dock.edge && member.band == dock.band)
        {
            others++;
        }
    }

    return others == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::InsertBand
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::InsertBand (Edge edge, int band, const DxuiToolbarHost * mover)
{
    std::vector<DxuiToolbarDock *>  docks;



    for (Member & member : m_members)
    {
        if (member.host != mover && !member.host->m_dock.floating)
        {
            docks.push_back (&member.host->m_dock);
        }
    }

    DxuiToolbarBands::InsertBand (docks, edge, band);

    //  The members' last places say which band they were laid out in, so a
    //  member moved in keeps its order key in its new band.
    for (Member & member : m_members)
    {
        if (member.host != mover && member.placed && member.edge == edge && member.band >= band)
        {
            member.band++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::Commit (const DxuiToolbarHost * mover)
{
    const DxuiToolbarDock  & moved = mover->m_dock;



    for (Member & member : m_members)
    {
        DxuiToolbarDock  & dock     = member.host->m_dock;
        bool               vertical = dock.IsVertical();
        int                at       = vertical ? member.rect.top : member.rect.left;

        if (dock.floating || moved.floating || !member.placed || dock.edge != moved.edge || dock.band != moved.band ||
            member.edge != dock.edge || member.band != dock.band)
        {
            continue;
        }

        dock.offsetDip = (std::max) (0, MulDiv (at - GetEdgeStartPx (dock.edge), USER_DEFAULT_SCREEN_DPI, m_dpi));
    }

    SaveAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::SaveAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::SaveAll()
{
    for (Member & member : m_members)
    {
        member.host->Save();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::GetDocks
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbarDock> DxuiToolbarDockGroup::GetDocks() const
{
    std::vector<DxuiToolbarDock>  docks;



    for (const Member & member : m_members)
    {
        docks.push_back (member.host->m_dock);
    }

    return docks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroup::RestoreDocks
//
//  Where each toolbar was laid out during the drag no longer orders it, so
//  the places put back lay out as they did before it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDockGroup::RestoreDocks (const std::vector<DxuiToolbarDock> & docks)
{
    size_t  count = (std::min) (docks.size(), m_members.size());



    for (size_t i = 0; i < count; i++)
    {
        m_members[i].host->m_dock = docks[i];
        m_members[i].placed       = false;
    }
}





