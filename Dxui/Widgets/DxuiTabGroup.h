#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup
//
//  A strip of tabs over a set of controls, one shown at a time: a tab group
//  of a docking layout.
//
//  THE GROUP OWNS NOTHING BUT ITS STRIP. The controls belong to whatever
//  panel holds them; the group lays the active one out in its body and hides
//  the rest, the way DxuiSplitter lays out around its sash without owning
//  the panes. That is what lets a dock site move a control from one group to
//  another without either group copying it.
//
//  A tab can carry an indicator, a dot beside its title, for a pane whose
//  content changed while it was not shown.
//
//  Pressing a tab activates it; pressing and dragging past the system's drag
//  distance reports a drag start instead, so the dock site can take the pane.
//  Ctrl+Tab and Ctrl+Shift+Tab cycle while the group has focus.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabGroup : public IDxuiControl
{
public:
    using ActivatedFn = std::function<void (int index)>;
    using DragFn      = std::function<void (int index, POINT pointDip)>;

    DxuiTabGroup() { m_focusable = true; }
    ~DxuiTabGroup() override = default;

    void  AddTab       (const std::wstring & title, IDxuiControl * content);
    bool  RemoveTab    (IDxuiControl * content);
    void  SetTitle     (IDxuiControl * content, const std::wstring & title);
    void  SetIndicator (IDxuiControl * content, bool on);

    size_t          GetTabCount   () const { return m_tabs.size(); }
    int             GetActive     () const { return m_active; }
    IDxuiControl *  GetContent    (int index) const;
    IDxuiControl *  GetActiveContent () const { return GetContent (m_active); }
    bool            HasIndicator  (int index) const;
    int             IndexOf       (IDxuiControl * content) const;

    //  Shows the tab's control and clears its indicator.
    void  SetActive    (int index);

    void  SetOnActivated (ActivatedFn fn) { m_onActivated = std::move (fn); }
    void  SetOnDragStart (DragFn fn)      { m_onDragStart = std::move (fn); }

    //  The tab under a point in the bounds' coordinates, or -1.
    int   HitTestTab   (POINT pointDip) const;
    RECT  GetTabRect   (int index) const;
    RECT  GetBodyRect  () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Tabs"; }

    static constexpr int  kStripDip     = 24;
    static constexpr int  kTabPadDip    = 10;
    static constexpr int  kCharDip      = 7;
    static constexpr int  kIndicatorDip = 10;
    static constexpr int  kDragDip      = 4;

private:
    struct Tab
    {
        std::wstring    title;
        IDxuiControl  * content   = nullptr;
        bool            indicator = false;
    };

    void  LayoutContent ();
    int   GetTabWidthDip (int index) const;

    std::vector<Tab>  m_tabs;
    int               m_active     = -1;
    DxuiDpiScaler     m_scaler;
    ActivatedFn       m_onActivated;
    DragFn            m_onDragStart;
    int               m_pressedTab = -1;
    POINT             m_pressedAt  = {};
    bool              m_dragging   = false;
};
