#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"


class IDxuiDockEdgesHost;
class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdges
//
//  A dock site's auto-hidden panes: a strip along each edge with one or more
//  of them, a tab for each pane, and the pane slid out over the others from
//  its tab. An edge band can be shared with something the window lays over
//  it, as Visual Studio's toolbars share a band with its auto-hide tabs.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockEdges
{
public:
    static constexpr int  kSlideMinDip  = 240;
    static constexpr int  kEdgeStripDip = 28;   // an edge's strip, which keeps its depth when a group's band changes

    //  An edge tab's bar, against the window's outer edge, and how far it
    //  stops short of each end so the bars of neighboring tabs stay apart.
    static constexpr int  kEdgeBarDip      = 3;
    static constexpr int  kEdgeBarInsetDip = 2;

    explicit DxuiDockEdges (IDxuiDockEdgesHost & host) : m_host (host) {}

    void  SetShare   (DxuiDockSide edge, long thickness, long start, long end);
    void  ClearShare ()                                { m_shareOn = false; }

    RECT          GetDockedArea () const;
    void          Arrange       (const RECT & dockedArea, const RECT & paneArea);
    int           HitTest       (POINT pointDip) const;
    RECT          GetTabRect    (const std::wstring & pane) const;
    std::wstring  GetTabPane    (int index) const      { return m_tabs[(size_t) index].pane; }
    void          Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    const std::wstring  & GetSlidPane    () const                       { return m_slidPane; }
    void                  SetSlidPane    (const std::wstring & pane)    { m_slidPane = pane; }
    void                  ClearSlidPane  ()                             { m_slidPane.clear(); }
    RECT                  GetSlidRect    () const                       { return m_slidRect; }
    const std::wstring  & GetHoveredTab  () const                       { return m_hoverTab; }
    void                  SetHoveredTab  (const std::wstring & pane)    { m_hoverTab = pane; }

private:
    struct EdgeTab
    {
        std::wstring  pane;
        DxuiDockSide  edge = DxuiDockSide::Left;
        RECT          rect = {};
    };

    IDxuiDockEdgesHost    & m_host;
    std::vector<EdgeTab>    m_tabs;
    bool                    m_shareOn    = false;
    DxuiDockSide            m_shareEdge  = DxuiDockSide::Top;
    long                    m_shareDepth = 0;
    long                    m_shareStart = 0;
    long                    m_shareEnd   = 0;
    std::wstring            m_slidPane;
    RECT                    m_slidRect   = {};
    DxuiDockSide            m_slidEdge   = DxuiDockSide::Left;
    std::wstring            m_hoverTab;
};
