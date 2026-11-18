#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiTabGroup.h"


class DxuiDockPanes;
class DxuiPaneLayout;
class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGroups
//
//  A dock site's tab groups: one for each group in the layout's arrangement,
//  reused by position from one arrangement to the next, and the group of
//  the pane slid out from an edge. A change to a pane's tab is made in every group
//  the pane could be in. The lookups cover the docked groups: the group a
//  pane is in, its tab and title bar, the tab or title-bar button under a
//  point and its tip, and which part of a group a point is on.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockGroups
{
public:
    using Groups = std::vector<std::unique_ptr<DxuiTabGroup>>;
    using WireFn = std::function<void (DxuiTabGroup * group)>;

    explicit DxuiDockGroups (const DxuiDockPanes & panes) : m_panes (panes) {}

    size_t                GetCount () const             { return m_groups.size(); }
    DxuiTabGroup        * Get      (size_t index) const { return m_groups[index].get(); }
    const Groups        & GetAll   () const             { return m_groups; }
    DxuiTabGroup        & GetSlid  ()                   { return m_slid; }
    const DxuiTabGroup  & GetSlid  () const             { return m_slid; }

    //  The docked groups grown to `count`, each new one passed to `wire`, or
    //  cut back to it; ClearRetired deletes the groups cut.
    void  SetCount     (size_t count, const WireFn & wire);
    void  ClearRetired ()       { m_retired.clear(); }

    //  Every group, the slid-out pane's included.
    void  SetTitle       (IDxuiControl * content, const std::wstring & title);
    void  SetIndicator   (IDxuiControl * content, bool on);
    void  SetLeadingMark (IDxuiControl * content, const DxuiTabGroup::LeadingMark & mark);
    void  SetTitleExtra  (IDxuiControl * content, int widthDip);
    void  CancelPress    ();
    bool  IsTitlePressed () const;

    //  The docked groups.
    void                       SetNewTab      (const DxuiTabGroup::NewTabShownFn & shown, const DxuiTabGroup::NewTabFn & add);
    void                       SetFocusedLook (IDxuiControl * content);
    void                       Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                       PaintFrames    (IDxuiPainter & painter, const IDxuiTheme & theme);
    std::vector<std::wstring>  GetActivePanes () const;

    DxuiTabGroup     * FindGroupOf          (const std::wstring & pane) const;
    RECT               GetPaneTabRect       (const std::wstring & pane) const;
    RECT               GetCarriedTabRect    (const std::wstring & pane) const;
    bool               TryGetPaneGroupRect  (const std::wstring & pane, RECT & rect) const;
    bool               TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const;
    std::wstring       GetTabAt             (POINT pointDip, RECT & tab, std::wstring & tip) const;
    std::wstring       GetTitleButtonTipAt  (POINT pointDip, RECT & button, bool isFloatingSite, const DxuiPaneLayout & layout) const;
    std::wstring       GetTabNeighbor       (const std::wstring & pane) const;
    std::wstring       GetGroupPaneAt       (POINT pointDip) const;
    bool               IsChromeAt           (POINT pointDip) const;
    DxuiHitTestKind    ClassifyHit          (POINT clientDip, bool isFloatingSite) const;

private:
    static bool  Contains (const RECT & rect, POINT point);

    const DxuiDockPanes  & m_panes;
    Groups                 m_groups;

    //  Groups a layout no longer needs, kept until the next event: the one
    //  whose button or tab changed the layout is still running its handler.
    Groups                 m_retired;
    DxuiTabGroup           m_slid;
};
