#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiDockDropZones.h"
#include "Core/DxuiPaneLayout.h"
#include "Widgets/DxuiTabGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSite
//
//  A docking area: it makes a set of pane controls match a DxuiPaneLayout.
//  Each tab group of the layout becomes a DxuiTabGroup over the panes'
//  controls, each split a sash that can be dragged, and a tab dragged out of
//  its strip shows the drop zones and docks where it is dropped.
//
//  THE SITE OWNS ITS STRIPS, NOT THE PANES. Pane controls belong to the
//  window's panel, which paints them and hands them input; the site places
//  them, shows the active tab of each group and hides the rest. The window
//  offers every mouse event to the site first, since strips, sashes and a
//  drag in progress take precedence over the panes beneath them.
//
//  EVERY CHANGE GOES THROUGH THE LAYOUT. A click on a tab, a sash drag, a
//  drop, a Dock To choice or an arrow-key move is a DxuiPaneLayout operation,
//  after which the site lays itself out again and reports the change, so an
//  application saves one object and restores the arrangement from it.
//
//  A drop outside the site's area asks the application to float the pane;
//  where a floating window comes from is the application's concern.
//
//  AN AUTO-HIDDEN PANE IS A TAB ON AN EDGE. The site keeps a strip along each
//  edge that holds one, and a hover or a press on its tab slides the pane out
//  over the others, which keep their places; a press anywhere else in the
//  site slides it back.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockSite : public IDxuiControl
{
public:
    using ChangedFn = std::function<void ()>;
    using FloatFn   = std::function<void (const std::wstring & pane, POINT pointDip)>;

    struct MenuItem
    {
        std::wstring           label;
        std::function<bool ()> action;
    };

    DxuiDockSite  ();
    ~DxuiDockSite () override = default;

    void  AddPane      (const std::wstring & pane, const std::wstring & title, IDxuiControl * content);
    void  SetTitle     (const std::wstring & pane, const std::wstring & title);

    void                    SetPaneLayout  (const DxuiPaneLayout & layout);
    const DxuiPaneLayout &  GetPaneLayout  () const { return m_layout; }

    //  For an application's own edits; call Relayout after.
    DxuiPaneLayout &        EditPaneLayout () { return m_layout; }

    void  SetShownFn   (DxuiPaneLayout::ShownFn fn)   { m_shown   = std::move (fn); }
    void  SetMinSizeFn (DxuiPaneLayout::MinSizeFn fn) { m_minSize = std::move (fn); }
    void  SetOnChanged (ChangedFn fn)                 { m_onChanged = std::move (fn); }

    //  A + on the tab groups `shown` picks, running `add` when pressed.
    void  SetNewTab    (DxuiTabGroup::NewTabShownFn shown, DxuiTabGroup::NewTabFn add);
    void  SetOnFloatRequested (FloatFn fn)            { m_onFloat   = std::move (fn); }

    //  Which panes are documents: a group holding one has its tabs along its
    //  top; any other group is a tool window, with a title bar. With no
    //  predicate every group is a document group.
    using PaneTestFn  = std::function<bool (const std::wstring & pane)>;
    using PaneFn      = std::function<void (const std::wstring & pane)>;
    using PanePointFn = std::function<void (const std::wstring & pane, POINT pointDip)>;

    void  SetDocumentFn   (PaneTestFn fn)  { m_isDocument = std::move (fn); }

    //  The pane the user is working in: its group shows the accent border.
    void  SetFocusedPane  (const std::wstring & pane);

    //  A tool window's menu button, and a close from a document tab or a
    //  tool window's title bar. A pane closes only while `canClose` says so;
    //  without it, every pane can.
    void  SetOnPaneMenu   (PanePointFn fn) { m_onPaneMenu = std::move (fn); }
    void  SetOnClosePane  (PaneFn fn, PaneTestFn canClose);

    //  Lays the panes out again in the current bounds.
    void  Relayout     ();

    //  Brings a pane's tab to the front of its group.
    void  ActivatePane (const std::wstring & pane);
    void  SetIndicator (const std::wstring & pane, bool on);

    //  A mark ahead of a pane's tab title (see DxuiTabGroup::LeadingMark), and
    //  a tip for its tab. Both stay with the pane as it moves between groups.
    void  SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark);
    void  SetTabTip     (const std::wstring & pane, const std::wstring & tip);

    //  The pane whose tab is under a point, with the tab's rect and tip, or
    //  empty.
    std::wstring  GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const;

    //  The pane whose control holds `content`, or empty.
    std::wstring  GetPaneOf (const IDxuiControl * content) const;

    //  The pane shown at a point, or empty.
    std::wstring  GetPaneAt (POINT pointDip) const;

    //  Keyboard docking (FR-042): the Dock To menu for a pane, and a move by
    //  an arrow key into the group in that direction or against the edge.
    std::vector<MenuItem>  GetDockToMenu   (const std::wstring & pane);
    bool                   MovePaneByArrow (const std::wstring & pane, DxuiDockSide direction);

    //  Auto-hidden panes: the one slid out, if any, and where it lies. A slid
    //  pane lies over the docked panes rather than taking room from them, so
    //  the window paints it above the page: its controls between
    //  PaintSlidUnder, its background, and PaintSlidOver, its title bar --
    //  menu, pin, close, and a drag to dock it -- and outline. `onSlid` hears
    //  which pane is slid out, or empty, each time that changes.
    const std::wstring &        GetSlidPane    () const { return m_slidPane; }
    RECT                        GetSlidRect    () const { return m_slidRect; }
    void                        SetOnSlid      (PaneFn fn) { m_onSlid = std::move (fn); }
    void                        PaintSlidUnder (IDxuiPainter & painter, const IDxuiTheme & theme);
    void                        PaintSlidOver  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                        SlideOut       (const std::wstring & pane);
    void                        SlideIn        ();
    RECT                        GetEdgeTabRect (const std::wstring & pane) const;

    bool                        IsDragging     () const { return !m_dragPane.empty(); }
    const std::wstring &        GetDraggedPane () const { return m_dragPane; }
    const DxuiDockDropZone *    GetHoveredZone () const;
    size_t                      GetGroupCount  () const { return m_groups.size(); }
    DxuiTabGroup *              GetGroup       (size_t index) const { return m_groups[index].get(); }

    //  A drag of a pane that started somewhere else, a floating window's
    //  title bar for one: the zones show until EndDrag.
    void  BeginDrag (const std::wstring & pane);
    bool  EndDrag   (POINT pointDip);

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Dock site"; }

    static bool  Contains (const RECT & rect, POINT point);

    static constexpr int  kSashDip      = 6;
    static constexpr int  kSlideMinDip  = 240;

private:
    struct Pane
    {
        std::wstring                 title;
        IDxuiControl               * content   = nullptr;
        bool                         indicator = false;
        DxuiTabGroup::LeadingMark    leadMark;
        std::wstring                 tip;
    };

    struct EdgeTab
    {
        std::wstring  pane;
        DxuiDockSide  edge = DxuiDockSide::Left;
        RECT          rect = {};
    };

    void          Arrange       ();
    RECT          GetDockedArea () const;
    void          ArrangeEdges  (const RECT & area);
    int           HitTestEdgeTab (POINT pointDip) const;
    void          PaintEdges    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void          NotifyChanged ();
    int           HitTestSash   (POINT pointDip) const;
    RECT          GetSashRect   (const DxuiPaneLayout::SplitRect & split) const;
    std::wstring  GetTitle      (const std::wstring & pane) const;
    static constexpr wchar_t  kAutoHideLabel[] = L"Auto Hide";

    void          WireGroup     (DxuiTabGroup * group);
    void          OnTitleButton (DxuiTabGroup::TitleButton button, const std::wstring & pane, POINT pointDip);

    DxuiPaneLayout                                m_layout;
    DxuiPaneLayout::ShownFn                       m_shown;
    DxuiPaneLayout::MinSizeFn                     m_minSize;
    std::map<std::wstring, Pane>                  m_panes;
    std::vector<std::unique_ptr<DxuiTabGroup>>    m_groups;

    //  Groups a layout no longer needs, kept until the next event: the one
    //  whose button or tab changed the layout is still running its handler.
    std::vector<std::unique_ptr<DxuiTabGroup>>    m_retired;
    PaneTestFn                                    m_isDocument;
    PanePointFn                                   m_onPaneMenu;
    PaneFn                                        m_onClosePane;
    PaneTestFn                                    m_canClosePane;
    std::wstring                                  m_focusedPane;
    DxuiTabGroup                                  m_slidGroup;
    PaneFn                                        m_onSlid;
    std::wstring                                  m_slidNotified;
    std::vector<DxuiPaneLayout::SplitRect>        m_splits;
    DxuiDpiScaler                                 m_scaler;
    ChangedFn                                     m_onChanged;
    DxuiTabGroup::NewTabShownFn                   m_newTabShown;
    DxuiTabGroup::NewTabFn                        m_newTab;
    FloatFn                                       m_onFloat;

    std::wstring                                  m_dragPane;
    std::vector<DxuiDockDropZone>                 m_zones;
    int                                           m_hoverZone  = -1;
    int                                           m_sashDrag   = -1;
    std::vector<EdgeTab>                          m_edgeTabs;
    std::wstring                                  m_slidPane;
    RECT                                          m_slidRect   = {};
    DxuiDockSide                                  m_slidEdge   = DxuiDockSide::Left;
    bool                                          m_arranging  = false;
};
