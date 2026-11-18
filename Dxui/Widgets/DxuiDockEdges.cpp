#include "Pch.h"

#include "DxuiDockEdges.h"
#include "IDxuiDockEdgesHost.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiTabGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::SetShare
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockEdges::SetShare (DxuiDockSide edge, long thickness, long start, long end)
{
    m_shareOn    = true;
    m_shareEdge  = edge;
    m_shareDepth = thickness;
    m_shareStart = start;
    m_shareEnd   = end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::GetDockedArea
//
//  The site less a strip along each edge that holds an auto-hidden pane.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockEdges::GetDockedArea() const
{
    const DxuiDpiScaler  & scaler   = m_host.GetEdgesScaler();
    RECT                   area     = m_host.GetEdgesBounds();
    bool                   used[4]  = {};
    long                   depth[4] = {};



    for (const DxuiPaneLayout::AutoHidden & hidden : m_host.GetEdgesLayout().GetAutoHidden())
    {
        if (m_host.IsEdgePaneShown (hidden.pane))
        {
            used[(size_t) hidden.edge] = true;
        }
    }

    //  Every edge's strip is one tab high: along a side the tabs run down it,
    //  their titles turned to read top to bottom.
    //  A shared edge is as deep as the deeper of its tabs and what shares it.
    for (size_t edge = 0; edge < 4; edge++)
    {
        depth[edge] = used[edge] ? scaler.ToPx (kEdgeStripDip) : 0;

        if (m_shareOn && (size_t) m_shareEdge == edge)
        {
            depth[edge] = std::max (depth[edge], m_shareDepth);
        }
    }

    area.left   += depth[(size_t) DxuiDockSide::Left];
    area.top    += depth[(size_t) DxuiDockSide::Top];
    area.right  -= depth[(size_t) DxuiDockSide::Right];
    area.bottom -= depth[(size_t) DxuiDockSide::Bottom];

    area.right  = std::max (area.right,  area.left);
    area.bottom = std::max (area.bottom, area.top);

    return area;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::Arrange
//
//  The edge tabs, in the order the panes were hidden, between the site's edge
//  and the docked area, and the area a slid-out pane covers: a third of the
//  pane area from its edge, and never less than it needs to be usable. The
//  pane area is the docked area less its margin, so the slid-out pane lines
//  up with the docked ones.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockEdges::Arrange (const RECT & dockedArea, const RECT & paneArea)
{
    const DxuiDpiScaler  & scaler    = m_host.GetEdgesScaler();
    RECT                   bounds    = m_host.GetEdgesBounds();
    long                   along[4]  = { dockedArea.top, dockedArea.left, dockedArea.top, dockedArea.left };
    long                   minSlide  = scaler.ToPx (kSlideMinDip);
    long                   width     = paneArea.right - paneArea.left;
    long                   height    = paneArea.bottom - paneArea.top;
    bool                   slidFound = false;
    DxuiDockSide           slidEdge  = DxuiDockSide::Left;



    m_tabs.clear();

    for (const DxuiPaneLayout::AutoHidden & hidden : m_host.GetEdgesLayout().GetAutoHidden())
    {
        EdgeTab  tab;
        long     length = scaler.ToPx (2 * DxuiTabGroup::kTabPadDip + (int) m_host.GetEdgePaneTitle (hidden.pane).size() * DxuiTabGroup::kCharDip);
        long   & at     = along[(size_t) hidden.edge];

        if (!m_host.IsEdgePaneShown (hidden.pane))
        {
            continue;
        }

        tab.pane = hidden.pane;
        tab.edge = hidden.edge;

        //  A tab that would run into the shared stretch goes on past it.
        if (m_shareOn && hidden.edge == m_shareEdge && at < m_shareEnd && at + length > m_shareStart)
        {
            at = m_shareEnd;
        }

        switch (hidden.edge)
        {
        case DxuiDockSide::Left:   tab.rect = RECT { bounds.left,  at, dockedArea.left,   at + length };  at += length;  break;
        case DxuiDockSide::Right:  tab.rect = RECT { dockedArea.right,  at, bounds.right, at + length };  at += length;  break;
        case DxuiDockSide::Top:    tab.rect = RECT { at, bounds.top, at + length, dockedArea.top };       at += length;  break;
        case DxuiDockSide::Bottom: tab.rect = RECT { at, dockedArea.bottom, at + length, bounds.bottom }; at += length;  break;
        }

        m_tabs.push_back (tab);

        if (tab.pane == m_slidPane)
        {
            slidFound = true;
            slidEdge  = tab.edge;
        }
    }

    if (!slidFound)
    {
        m_slidPane.clear();
        m_slidRect = RECT {};
        return;
    }

    m_slidEdge = slidEdge;

    switch (slidEdge)
    {
    case DxuiDockSide::Left:   m_slidRect = RECT { paneArea.left, paneArea.top, paneArea.left + std::min (width, std::max (minSlide, width / 3)), paneArea.bottom };       break;
    case DxuiDockSide::Right:  m_slidRect = RECT { paneArea.right - std::min (width, std::max (minSlide, width / 3)), paneArea.top, paneArea.right, paneArea.bottom };     break;
    case DxuiDockSide::Top:    m_slidRect = RECT { paneArea.left, paneArea.top, paneArea.right, paneArea.top + std::min (height, std::max (minSlide, height / 3)) };       break;
    case DxuiDockSide::Bottom: m_slidRect = RECT { paneArea.left, paneArea.bottom - std::min (height, std::max (minSlide, height / 3)), paneArea.right, paneArea.bottom }; break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::HitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockEdges::HitTest (POINT pointDip) const
{
    for (size_t i = 0; i < m_tabs.size(); i++)
    {
        if (pointDip.x >= m_tabs[i].rect.left && pointDip.x < m_tabs[i].rect.right && pointDip.y >= m_tabs[i].rect.top && pointDip.y < m_tabs[i].rect.bottom)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::GetTabRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockEdges::GetTabRect (const std::wstring & pane) const
{
    for (const EdgeTab & tab : m_tabs)
    {
        if (tab.pane == pane)
        {
            return tab.rect;
        }
    }

    return RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges::Paint
//
//  The edge strips and their tabs. The slid-out pane itself is drawn by
//  DxuiDockSite::PaintSlidUnder and PaintSlidOver.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockEdges::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    const DxuiDpiScaler  & scaler = m_host.GetEdgesScaler();
    DxuiFontHandle         font   = theme.BodyFont();
    float                  pad    = scaler.ToPxf ((float) DxuiTabGroup::kTabPadDip / 2);
    float                  bar    = scaler.ToPxf ((float) kEdgeBarDip);
    float                  inset  = scaler.ToPxf ((float) kEdgeBarInsetDip);
    HRESULT                hr     = S_OK;



    //  As Visual Studio draws them: no box, only the title and a bar above
    //  it, gray at rest and in the focus accent while the pointer is over the
    //  tab or its pane is out.
    for (const EdgeTab & tab : m_tabs)
    {
        const RECT  & r        = tab.rect;
        bool          lit      = (tab.pane == m_slidPane) || (tab.pane == m_hoverTab);
        bool          sideways = tab.edge == DxuiDockSide::Left || tab.edge == DxuiDockSide::Right;
        float         cx       = (float) (r.left + r.right) / 2;
        float         cy       = (float) (r.top + r.bottom) / 2;
        float         along    = (float) (sideways ? r.bottom - r.top : r.right - r.left);
        float         across   = (float) (sideways ? r.right - r.left : r.bottom - r.top);
        uint32_t      barArgb  = lit ? theme.FocusAccent() : theme.Border();

        painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top), theme.Background());

        //  Against the window's outer edge, as Visual Studio draws it: left
        //  of a tab on the left edge, below one on the bottom edge.
        switch (tab.edge)
        {
        case DxuiDockSide::Left:
            painter.FillRect ((float) r.left, (float) r.top + inset, bar, along - 2 * inset, barArgb);
            break;

        case DxuiDockSide::Right:
            painter.FillRect ((float) r.right - bar, (float) r.top + inset, bar, along - 2 * inset, barArgb);
            break;

        case DxuiDockSide::Top:
            painter.FillRect ((float) r.left + inset, (float) r.top, along - 2 * inset, bar, barArgb);
            break;

        case DxuiDockSide::Bottom:
            painter.FillRect ((float) r.left + inset, (float) r.bottom - bar, along - 2 * inset, bar, barArgb);
            break;
        }

        //  A side tab is laid out level about its center and turned a
        //  quarter clockwise, so its title reads down the edge.
        if (sideways)
        {
            text.PushTextRotation (90.0f, cx, cy);
        }

        hr = text.DrawString (m_host.GetEdgePaneTitle (tab.pane).c_str(), cx - along / 2 + pad, cy - across / 2,
                              along - 2 * pad, across,
                              lit ? theme.Foreground() : theme.ForegroundMuted(), scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (sideways)
        {
            text.PopTextRotation();
        }

        if (m_host.HasEdgePaneIndicator (tab.pane))
        {
            painter.FillCircle ((float) r.right - pad, (float) r.top + pad, scaler.ToPxf (3.0f), theme.Accent());
        }
    }
}
