#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"


class DxuiTabGroup;
class IDxuiDockQueryHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockQuery
//
//  What lies where among a dock site's tab groups: the group a pane is in,
//  its tab and title bar, the tab or title-bar button under a point and its
//  tip, and which part of a group a point is on.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockQuery
{
public:
    explicit DxuiDockQuery (const IDxuiDockQueryHost & host) : m_host (host) {}

    DxuiTabGroup     * FindGroupOf          (const std::wstring & pane) const;
    RECT               GetPaneTabRect       (const std::wstring & pane) const;
    RECT               GetCarriedTabRect    (const std::wstring & pane) const;
    bool               TryGetPaneGroupRect  (const std::wstring & pane, RECT & rect) const;
    bool               TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const;
    std::wstring       GetTabAt             (POINT pointDip, RECT & tab, std::wstring & tip) const;
    std::wstring       GetTitleButtonTipAt  (POINT pointDip, RECT & button) const;
    std::wstring       GetTabNeighbor       (const std::wstring & pane) const;
    std::wstring       GetGroupPaneAt       (POINT pointDip) const;
    bool               IsChromeAt           (POINT pointDip) const;
    DxuiHitTestKind    ClassifyHit          (POINT clientDip) const;

    static bool  Contains (const RECT & rect, POINT point);

private:
    const IDxuiDockQueryHost  & m_host;
};
