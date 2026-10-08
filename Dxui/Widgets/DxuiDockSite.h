#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiDockDropZones.h"
#include "Core/DxuiPaneLayout.h"
#include "Widgets/DxuiTabGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDragMark
//
//  One rectangle a drag shows over the site: a drop target, the area the
//  hovered target would give the panes, or a hovered strip's tint and gap.
//  `outlinePx` of 0 fills the rectangle; any other width outlines it, in
//  dots of that size when `dotted`, as a split target's picture is drawn.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockDragMark
{
    RECT      rect      = {};
    uint32_t  argb      = 0;
    int       outlinePx = 0;
    bool      dotted    = false;
};





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

    //  An empty label is a separator.
    struct MenuItem
    {
        std::wstring           label;
        std::function<bool ()> action;
        bool                   enabled = true;
        std::wstring           accelerator;
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

    //  A pane's tab dragged off its strip, or a lone pane's title bar dragged,
    //  floats the pane at once under the pointer instead of starting a drag
    //  here, so the floating window carries the rest of the drag. Without a
    //  handler, or in a floating window's site, the drag stays in the site.
    void  SetOnTearOff        (FloatFn fn)            { m_onTearOff = std::move (fn); }

    //  Which panes are documents: a group holding one has its tabs along its
    //  top; any other group is a tool window, with a title bar. With no
    //  predicate every group is a document group.
    using PaneTestFn  = std::function<bool (const std::wstring & pane)>;
    using PaneFn      = std::function<void (const std::wstring & pane)>;
    using PanePointFn = std::function<void (const std::wstring & pane, POINT pointDip)>;

    void  SetDocumentFn   (PaneTestFn fn)  { m_isDocument = std::move (fn); }

    //  The pane the user is working in: its group shows the accent border.
    void  SetFocusedPane  (const std::wstring & pane);

    //  The room between docked panes and around them, as Visual Studio leaves
    //  it, filled with the theme's DockGap. Zero for both, the default, puts
    //  the panes edge to edge.
    void         SetPaneGap     (int gapDip, int marginDip);
    static RECT  GetInsetForGap (const RECT & rect, const RECT & paneArea, long gapPx);

    //  Visual Studio's gap, 8 px at 125%, and a margin of 5 px there.
    static constexpr int  kPaneGapDip    = 6;
    static constexpr int  kPaneMarginDip = 4;

    //  A tool window's menu button, and a close from a document tab or a
    //  tool window's title bar. A pane closes only while `canClose` says so;
    //  without it, every pane can.
    void  SetOnPaneMenu   (PanePointFn fn) { m_onPaneMenu = std::move (fn); }
    void  SetOnClosePane  (PaneFn fn, PaneTestFn canClose);

    //  The site of a floating window, as Visual Studio draws one: each group
    //  is a tool window, so its title bar is the window's only one and a
    //  single pane has no tab; the title bar off its buttons moves the
    //  window; and its pin docks the pane back, through `dock`.
    void  SetFloating     (PaneFn dock);
    bool  IsFloatingSite  () const { return m_onDock != nullptr; }

    //  The pane a tear-off carries: while it is set, the group holding the
    //  pane shows its tab strip even for one pane, so the floating window
    //  moves with the tab the user grabbed. Empty when no tear-off is under
    //  way.
    void                  SetCarriedPane (const std::wstring & pane);
    const std::wstring &  GetCarriedPane () const { return m_carriedPane; }

    //  Where the carried pane's tab lies in this site, or an empty rect.
    RECT  GetCarriedTabRect () const;

    //  Where in the grabbed tab, or the grabbed title bar, the last tear-off
    //  was pressed, from that rect's top left; and whether it was a tab.
    POINT  GetTearOffGrab      () const { return m_tearGrab; }
    bool   WasTearOffFromTab   () const { return m_tearFromTab; }

    //  Lays the panes out again in the current bounds.
    void  Relayout     ();

    //  Brings a pane's tab to the front of its group.
    void  ActivatePane (const std::wstring & pane);
    void  SetIndicator (const std::wstring & pane, bool on);

    //  A mark ahead of a pane's tab title (see DxuiTabGroup::LeadingMark), and
    //  a tip for its tab. Both stay with the pane as it moves between groups.
    void  SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark);
    void  SetTabTip     (const std::wstring & pane, const std::wstring & tip);

    //  Room a pane keeps in its tool window's title bar for a control of its
    //  own (see DxuiTabGroup::SetTitleExtra), and where that room is now:
    //  false while the pane is not the one its title bar shows, or its group
    //  has no title bar.
    void  SetTitleExtra        (const std::wstring & pane, int widthDip);
    bool  TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const;

    //  The pane whose tab is under a point, with the tab's rect and tip, or
    //  empty.
    std::wstring  GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const;

    //  What the title-bar button under a point does, with the button's rect,
    //  or empty off them: the menu's "Window position", the pin's "Auto hide"
    //  or, on a slid-out or floating pane, "Dock", and "Close".
    std::wstring  GetTitleButtonTipAt (POINT pointDip, RECT & button) const;

    //  The pane whose control holds `content`, or empty.
    std::wstring  GetPaneOf (const IDxuiControl * content) const;

    //  The pane shown at a point, or empty.
    std::wstring  GetPaneAt (POINT pointDip) const;

    //  Keyboard docking (FR-042): the Dock To menu for a pane, and a move by
    //  an arrow key into the group in that direction or against the edge.
    std::vector<MenuItem>  GetDockToMenu   (const std::wstring & pane);
    bool                   MovePaneByArrow (const std::wstring & pane, DxuiDockSide direction);

    //  Visual Studio's menu for a pane's tab, or for its group's title bar and
    //  menu button: Dock, Dock in tab group, Auto hide, Move to new window (a
    //  tab's only), All to new window, and Close.
    std::vector<MenuItem>  GetPaneMenu     (const std::wstring & pane, bool fromTab);

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

    //  An edge band the site shares with something the window lays over it,
    //  as Visual Studio's toolbars share a band with its auto-hide tabs: the
    //  edge keeps a strip at least `thickness` deep, and its tabs run around
    //  the stretch from `start` to `end` along it, in site pixels (x across
    //  the top or bottom, y down a side). ClearEdgeShare gives the edge back.
    void                        SetEdgeShare   (DxuiDockSide edge, long thickness, long start, long end);
    void                        ClearEdgeShare ();

    bool                        IsDragging    () const { return !m_dragPane.empty(); }
    const std::wstring &        GetDraggedPane () const { return m_dragPane; }
    const DxuiDockDropZone *    GetHoveredZone () const;

    //  What a drag shows, in site pixels, top to bottom. A window whose
    //  floating windows would cover the site draws these in an overlay above
    //  them instead, and tells the site not to paint them.
    std::vector<DxuiDockDragMark>  GetDragMarks               (const IDxuiTheme & theme) const;
    void                           SetDragMarksDrawnElsewhere (bool elsewhere) { m_marksElsewhere = elsewhere; }

    size_t                      GetGroupCount  () const { return m_groups.size(); }
    DxuiTabGroup *              GetGroup       (size_t index) const { return m_groups[index].get(); }

    //  A drag of a pane that started somewhere else, a floating window's
    //  title bar for one: the zones show until EndDrag, or CancelDrag, which
    //  takes them down and changes nothing.
    void  BeginDrag  (const std::wstring & pane);
    bool  EndDrag    (POINT pointDip);
    void  CancelDrag ();

    //  A drag of a whole group, from its title bar: the panes move together,
    //  in order, with `active` still the one shown.
    void                               BeginGroupDrag  (const std::vector<std::wstring> & panes, const std::wstring & active);
    const std::vector<std::wstring> &  GetDraggedPanes () const { return m_dragPanes; }

    //  While a drag hovers a group's tabs or title bar, that group and where
    //  among its tabs the drop would land; -1 for neither. A drop there tabs
    //  the dragged panes into the group at that place.
    int   GetStripTargetGroup () const { return m_stripGroup; }
    int   GetStripTargetIndex () const { return m_stripIndex; }

    //  The gap a hovered strip opens for the dropped tab.
    static constexpr int  kInsertGapDip = 96;

    void                Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint              (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                PaintAfterSiblings (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse            (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint  (POINT clientPx) const override;
    DxuiHitTestKind     ClassifyHit        (POINT clientDip) const override;
    DxuiAccessibleRole  GetAccessibleRole  () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName  () const override { return L"Dock site"; }

    static bool  Contains (const RECT & rect, POINT point);

    //  The filled rectangles that draw an outlined mark, solid or dotted.
    static std::vector<RECT>  GetOutlineStrips (const DxuiDockDragMark & mark);

    static constexpr int  kSashDip      = 6;
    static constexpr int  kSlideMinDip  = 240;

    //  An edge tab's bar, against the window's outer edge, and how far it
    //  stops short of each end so the bars of neighboring tabs stay apart.
    static constexpr int  kEdgeBarDip      = 3;
    static constexpr int  kEdgeBarInsetDip = 2;

    //  The edge tab under the pointer, whose bar is lit, or empty.
    const std::wstring &  GetHoveredEdgeTab () const { return m_hoverEdge; }

private:
    struct Pane
    {
        std::wstring                 title;
        IDxuiControl               * content       = nullptr;
        bool                         indicator     = false;
        DxuiTabGroup::LeadingMark    leadMark;
        std::wstring                 tip;
        int                          titleExtraDip = 0;
    };

    struct EdgeTab
    {
        std::wstring  pane;
        DxuiDockSide  edge = DxuiDockSide::Left;
        RECT          rect = {};
    };

    void          Arrange       ();
    RECT          GetDockedArea () const;
    RECT          GetPaneArea   () const;
    void          ArrangeEdges  (const RECT & dockedArea, const RECT & paneArea);
    int           HitTestEdgeTab (POINT pointDip) const;
    void          PaintEdges    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void          NotifyChanged ();
    int           HitTestSash   (POINT pointDip) const;
    RECT          GetSashRect   (const DxuiPaneLayout::SplitRect & split) const;
    void          PaintGaps     (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    std::wstring  GetTitle      (const std::wstring & pane) const;
    static constexpr wchar_t  kAutoHideLabel[] = L"Auto hide";

    void          WireGroup     (DxuiTabGroup * group);
    bool          TearOff       (DxuiTabGroup * group, const std::wstring & pane, POINT pointDip, bool fromTab);
    void          UpdateStripTarget (POINT pointDip);
    void          ClearStripTarget  ();
    bool          DropOnStrip   (int group, int index);
    bool          DropOnZone    (const DxuiDockDropZone & zone);
    void          OnTitleButton (DxuiTabGroup::TitleButton button, const std::wstring & pane, POINT pointDip);
    bool          IsDocumentGroup (const std::vector<std::wstring> & panes) const;
    DxuiTabGroup * FindGroupOf  (const std::wstring & pane) const;
    void          AddDottedHalf (std::vector<DxuiDockDragMark> & marks, const DxuiDockDropZone & zone, uint32_t argb, int line) const;
    void          AddGlyph      (std::vector<DxuiDockDragMark> & marks, const DxuiDockDropZone & zone, const IDxuiTheme & theme, int line) const;
    void          PaintDragMarks (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

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
    PaneFn                                        m_onDock;
    std::wstring                                  m_focusedPane;
    DxuiTabGroup                                  m_slidGroup;
    PaneFn                                        m_onSlid;
    std::wstring                                  m_slidNotified;
    std::wstring                                  m_hoverEdge;
    std::vector<DxuiPaneLayout::SplitRect>        m_splits;
    int                                           m_gapDip     = 0;
    int                                           m_marginDip  = 0;
    RECT                                          m_dockedArea = {};
    RECT                                          m_paneArea   = {};
    DxuiDpiScaler                                 m_scaler;
    ChangedFn                                     m_onChanged;
    DxuiTabGroup::NewTabShownFn                   m_newTabShown;
    DxuiTabGroup::NewTabFn                        m_newTab;
    FloatFn                                       m_onFloat;
    FloatFn                                       m_onTearOff;

    std::wstring                   m_dragPane;
    std::vector<std::wstring>      m_dragPanes;
    int                            m_stripGroup     = -1;
    int                            m_stripIndex     = -1;
    std::vector<DxuiDockDropZone>  m_zones;
    int                            m_hoverZone      = -1;
    bool                           m_marksElsewhere = false;
    int                            m_sashDrag       = -1;
    std::vector<EdgeTab>           m_edgeTabs;
    bool                           m_shareOn        = false;
    DxuiDockSide                   m_shareEdge      = DxuiDockSide::Top;
    long                           m_shareDepth     = 0;
    long                           m_shareStart     = 0;
    long                           m_shareEnd       = 0;
    std::wstring                   m_slidPane;
    RECT                           m_slidRect       = {};
    DxuiDockSide                   m_slidEdge       = DxuiDockSide::Left;
    bool                           m_arranging      = false;
    std::wstring                   m_carriedPane;
    POINT                          m_tearGrab       = {};
    bool                           m_tearFromTab    = false;
};
