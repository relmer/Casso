#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiDockDropZones.h"
#include "Core/DxuiPaneLayout.h"
#include "Widgets/DxuiDockDrag.h"
#include "Widgets/DxuiDockEdges.h"
#include "Widgets/DxuiDockGaps.h"
#include "Widgets/DxuiDockMenus.h"
#include "Widgets/DxuiDockMorph.h"
#include "Widgets/DxuiDockQuery.h"
#include "Widgets/DxuiTabGroup.h"
#include "Widgets/IDxuiDockDragHost.h"
#include "Widgets/IDxuiDockEdgesHost.h"
#include "Widgets/IDxuiDockMenuHost.h"
#include "Widgets/IDxuiDockQueryHost.h"





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
//  edge that holds one, and a press on its tab slides the pane out over the
//  others, which keep their places; a hover only lights the tab, as in
//  Visual Studio. A press anywhere else in the site, or the focus moving to
//  another pane, slides it back.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockSite : public IDxuiControl,
                     private IDxuiDockDragHost,
                     private IDxuiDockEdgesHost,
                     private IDxuiDockMenuHost,
                     private IDxuiDockQueryHost
{
public:
    using ChangedFn = std::function<void ()>;
    using FloatFn   = std::function<void (const std::wstring & pane, POINT pointDip)>;

    using MenuItem = DxuiDockMenuItem;

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
    void  SetNewTab           (DxuiTabGroup::NewTabShownFn shown, DxuiTabGroup::NewTabFn add);
    void  SetOnFloatRequested (FloatFn fn)            { m_onFloat   = std::move (fn); }

    //  A pane's tab dragged off its strip, or a lone pane's title bar dragged,
    //  floats the pane at once under the pointer instead of starting a drag
    //  here, so the floating window carries the rest of the drag. Without a
    //  handler, or in a floating window's site holding one pane, the drag
    //  stays in the site.
    void  SetOnTearOff        (FloatFn fn)            { m_onTearOff = std::move (fn); }

    //  The title bar of a group of several panes dragged: the whole group
    //  floats at once under the pointer, its tabs together in one window,
    //  `active` the one shown. Without a handler the group is dragged in the
    //  site, as before.
    using GroupFloatFn = std::function<void (const std::vector<std::wstring> & panes, const std::wstring & active, POINT pointDip)>;
    void  SetOnTearOffGroup   (GroupFloatFn fn)       { m_onTearOffGroup = std::move (fn); }

    //  All to new window on a group of several: every pane of the group
    //  floats in one window, and `active` is its selected tab. Without a
    //  handler the item is enabled only for a pane alone in its group.
    void  SetOnFloatGroupRequested (GroupFloatFn fn)  { m_onFloatGroup = std::move (fn); }

    //  The area of the group a docked pane is in, as drawn, title bar and
    //  tabs included: what a floating window must be to show it the same.
    bool  TryGetPaneGroupRect (const std::wstring & pane, RECT & rect) const { return m_query.TryGetPaneGroupRect (pane, rect); }

    //  Which panes are documents: a group holding one has its tabs along its
    //  top; any other group is a tool window, with a title bar. With no
    //  predicate every group is a document group.
    using PaneTestFn  = std::function<bool (const std::wstring & pane)>;
    using PaneFn      = std::function<void (const std::wstring & pane)>;
    using PanePointFn = std::function<void (const std::wstring & pane, POINT pointDip)>;

    void  SetDocumentFn   (PaneTestFn fn)  { m_isDocument = std::move (fn); }

    //  The pane the user is working in: its group shows the accent border. A
    //  slid-out pane slides back once another pane takes the focus.
    void  SetFocusedPane  (const std::wstring & pane);

    //  A pane docked back from an edge, or one whose title bar is pressed,
    //  is focused as a press on its tab focuses it: its group is drawn with
    //  the focus outline at once, and `fn` is called to move the keyboard
    //  focus to the pane.
    void  SetOnFocusRequested (PaneFn fn) { m_onFocusRequested = std::move (fn); }

    //  The room between docked panes and around them, as Visual Studio leaves
    //  it, filled with the theme's DockGap. Zero for both, the default, puts
    //  the panes edge to edge.
    void         SetPaneGap     (int gapDip, int marginDip);
    static RECT  GetInsetForGap (const RECT & rect, const RECT & paneArea, long gapPx) { return DxuiDockGaps::GetInsetForGap (rect, paneArea, gapPx); }

    //  Visual Studio's gap, 8 px at 125%, and the same room around the panes.
    static constexpr int  kPaneGapDip    = 6;
    static constexpr int  kPaneMarginDip = 6;

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

    //  The radius the window this site fills rounds its corners by. A group
    //  whose corner is a corner of the site is drawn to that radius there, so
    //  its outline follows the window's edge. Zero, the default, for a window
    //  with square corners.
    void  SetWindowCornerDip (int radiusDip);

    //  The pane a tear-off carries: while it is set, the group holding the
    //  pane shows its tab strip even for one pane, so the floating window
    //  moves with the tab the user grabbed. Empty when no tear-off is under
    //  way.
    void                  SetCarriedPane (const std::wstring & pane);
    const std::wstring &  GetCarriedPane () const { return m_carriedPane; }

    //  Where the carried pane's tab lies in this site, or an empty rect.
    RECT  GetCarriedTabRect () const { return m_query.GetCarriedTabRect (m_carriedPane); }

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
    void  SetTabTip      (const std::wstring & pane, const std::wstring & tip);

    //  Room a pane keeps in its tool window's title bar for a control of its
    //  own (see DxuiTabGroup::SetTitleExtra), and where that room is now:
    //  false while the pane is not the one its title bar shows, or its group
    //  has no title bar.
    void  SetTitleExtra        (const std::wstring & pane, int widthDip);
    bool  TryGetTitleExtraRect (const std::wstring & pane, RECT & rect) const { return m_query.TryGetTitleExtraRect (pane, rect); }

    //  The pane whose tab is under a point, with the tab's rect and tip, or
    //  empty.
    std::wstring  GetTabAt (POINT pointDip, RECT & tab, std::wstring & tip) const { return m_query.GetTabAt (pointDip, tab, tip); }

    //  What the title-bar button under a point does, with the button's rect,
    //  or empty off them: the menu's "Window position", the pin's "Auto hide"
    //  or, on a slid-out or floating pane, "Dock", and "Close".
    std::wstring  GetTitleButtonTipAt (POINT pointDip, RECT & button) const { return m_query.GetTitleButtonTipAt (pointDip, button); }

    //  The pane whose control holds `content`, or empty.
    std::wstring  GetPaneOf (const IDxuiControl * content) const;

    //  A pane's control, or null for a pane the site does not have.
    IDxuiControl * GetPaneContent (const std::wstring & pane) const;

    //  The pane on screen at a point, or the pane of the tab at the point;
    //  empty off every pane.
    std::wstring  GetPaneAt (POINT pointDip) const;

    //  The pane of the tab after pane's in its group, or the one before it
    //  when it is the last; empty when it is alone or its group is hidden.
    std::wstring  GetTabNeighbor (const std::wstring & pane) const { return m_query.GetTabNeighbor (pane); }

    //  Keyboard docking (FR-042): the Dock To menu for a pane, and a move by
    //  an arrow key into the group in that direction or against the edge.
    std::vector<MenuItem>  GetDockToMenu   (const std::wstring & pane) { return m_menus.GetDockToMenu (pane); }
    bool                   MovePaneByArrow (const std::wstring & pane, DxuiDockSide direction);

    //  Visual Studio's menu for a pane's tab, or for its group's title bar and
    //  menu button: Dock, Dock in tab group, Auto hide, Move to new window (a
    //  tab's only), All to new window, and Close.
    std::vector<MenuItem>  GetPaneMenu     (const std::wstring & pane, bool fromTab) { return m_menus.GetPaneMenu (pane, fromTab); }

    //  Auto-hidden panes: the one slid out, if any, and where it lies. A slid
    //  pane lies over the docked panes rather than taking room from them, so
    //  the window paints it above the page: its controls between
    //  PaintSlidUnder, its background, and PaintSlidOver, its title bar --
    //  menu, pin, close, and a drag to dock it -- and outline. `onSlid` hears
    //  which pane is slid out, or empty, each time that changes.
    const std::wstring &        GetSlidPane    () const { return m_edges.GetSlidPane(); }
    RECT                        GetSlidRect    () const { return m_edges.GetSlidRect(); }
    void                        SetOnSlid      (PaneFn fn) { m_onSlid = std::move (fn); }
    void                        PaintSlidUnder (IDxuiPainter & painter, const IDxuiTheme & theme);
    void                        PaintSlidOver  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                        SlideOut       (const std::wstring & pane);
    void                        SlideIn        ();
    RECT                        GetEdgeTabRect (const std::wstring & pane) const { return m_edges.GetTabRect (pane); }

    //  An edge band the site shares with something the window lays over it,
    //  as Visual Studio's toolbars share a band with its auto-hide tabs: the
    //  edge keeps a strip at least `thickness` deep, and its tabs run around
    //  the stretch from `start` to `end` along it, in site pixels (x across
    //  the top or bottom, y down a side). ClearEdgeShare gives the edge back.
    void                        SetEdgeShare   (DxuiDockSide edge, long thickness, long start, long end) { m_edges.SetShare (edge, thickness, start, end); }
    void                        ClearEdgeShare ()                                                        { m_edges.ClearShare(); }

    bool                      IsDragging     () const { return m_drag.IsDragging(); }
    const std::wstring      & GetDraggedPane () const { return m_drag.GetPane(); }
    const DxuiDockDropZone  * GetHoveredZone () const { return m_drag.GetHoveredZone(); }

    //  What a drag shows, in site pixels, top to bottom. A window whose
    //  floating windows would cover the site draws these in an overlay above
    //  them instead, and tells the site not to paint them.
    std::vector<DxuiDockDragMark>  GetDragMarks               (const IDxuiTheme & theme) const { return m_drag.GetMarks (theme); }
    void                           SetDragMarksDrawnElsewhere (bool elsewhere) { m_marksElsewhere = elsewhere; }

    //  The marks lie over everything in the panes, their pictures included,
    //  and a flush draws its pictures after all of its fills, so a shade
    //  painted with the page would lie under a pane's images. The window
    //  paints them in a layer flushed after the page instead: HasDragLayer
    //  is true while there are any to paint there, and PaintDragLayer paints
    //  them.
    bool                           HasDragLayer               () const { return (IsDragging() && !m_marksElsewhere) || IsMorphing(); }
    void                           PaintDragLayer             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    //  Pinning or unpinning a pane changes the layout at once. A rect then
    //  shrinks from where the pane was into its edge tab, or grows from the
    //  edge tab to the pane's group, drawn in the drag layer. Time comes from
    //  GetTickCount64 unless SetClock replaces it, and the system's animation
    //  setting applies unless SetAnimationsEnabled overrides it.
    //  TickAnimation returns true while a frame of the morph is due, and once
    //  more as it ends, so the frame without it is painted too.
    using ClockFn = DxuiDockMorph::ClockFn;

    void  SetClock             (ClockFn fn) { m_morph.SetClock (std::move (fn)); }
    void  SetAnimationsEnabled (bool on)    { m_morph.SetAnimationsEnabled (on); }
    bool  IsMorphing           () const     { return m_morph.IsOn(); }
    RECT  GetMorphRect         () const     { return m_morph.GetRect(); }
    bool  TickAnimation        ()           { return m_morph.Tick(); }

    size_t                      GetGroupCount  () const { return m_groups.size(); }
    DxuiTabGroup *              GetGroup       (size_t index) const { return m_groups[index].get(); }

    //  A drag of a pane that started somewhere else, a floating window's
    //  title bar for one: the zones show until EndDrag, or CancelDrag, which
    //  takes them down and changes nothing.
    void  BeginDrag  (const std::wstring & pane);
    bool  EndDrag    (POINT pointDip) { return m_drag.End (pointDip); }
    void  CancelDrag ();

    //  True during a drag that a press in the site started, from a tab or a
    //  title bar, during a sash drag, and while a title bar press is armed,
    //  which a move past the drag distance turns into a drag. The window
    //  sends every mouse event to the site until the button comes up,
    //  wherever the pointer is, so no control under the pointer receives a
    //  move or draws a tip. Escape cancels the drag, as does a capture loss
    //  with the button down: every pane goes back to where it was, and a sash
    //  stays where the drag put it. The window releases the capture itself as
    //  the button comes up, before the button-up arrives, so that is not a
    //  loss.
    bool  HasPointerDrag  () const;
    bool  OnDragKey       (const DxuiKeyEvent & ev);
    void  OnDragMouseLost (bool isButtonDown);

    //  A drag of a whole group, from its title bar: the panes move together,
    //  in order, with `active` still the one shown.
    void                               BeginGroupDrag  (const std::vector<std::wstring> & panes, const std::wstring & active);
    const std::vector<std::wstring> &  GetDraggedPanes () const { return m_drag.GetPanes(); }

    //  While a drag hovers a group's tabs or title bar, that group and where
    //  among its tabs the drop would land; -1 for neither. A drop there tabs
    //  the dragged panes into the group at that place.
    int   GetStripTargetGroup () const { return m_drag.GetStripGroup(); }
    int   GetStripTargetIndex () const { return m_drag.GetStripIndex(); }

    static constexpr int  kInsertGapDip  = DxuiDockDrag::kInsertGapDip;
    static constexpr int  kPreviewTabDip = DxuiDockDrag::kPreviewTabDip;

    void                Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint              (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                PaintAfterSiblings (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse            (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint  (POINT clientPx) const override;
    bool                IsHitAt            (POINT pointPx) const override;
    bool                TryGetTip          (POINT pointPx, DxuiTip & tip) const override;
    DxuiHitTestKind     ClassifyHit        (POINT clientDip) const override { return m_query.ClassifyHit (clientDip); }
    DxuiAccessibleRole  GetAccessibleRole  () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName  () const override { return L"Dock site"; }

    static bool  Contains (const RECT & rect, POINT point);

    //  The filled rectangles that draw an outlined mark, solid or dotted.
    static std::vector<RECT>  GetOutlineStrips (const DxuiDockDragMark & mark) { return DxuiDockDrag::GetOutlineStrips (mark); }

    static constexpr int  kSashDip         = DxuiDockGaps::kSashDip;
    static constexpr int  kSlideMinDip     = DxuiDockEdges::kSlideMinDip;
    static constexpr int  kEdgeStripDip    = DxuiDockEdges::kEdgeStripDip;
    static constexpr int  kEdgeBarDip      = DxuiDockEdges::kEdgeBarDip;
    static constexpr int  kEdgeBarInsetDip = DxuiDockEdges::kEdgeBarInsetDip;

    //  The edge tab under the pointer, whose bar is lit, or empty.
    const std::wstring &  GetHoveredEdgeTab () const { return m_edges.GetHoveredTab(); }

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

    void          Arrange        ();
    RECT          GetDockedArea  () const { return m_edges.GetDockedArea(); }
    RECT          GetPaneArea    () const;
    int           HitTestEdgeTab (POINT pointDip) const { return m_edges.HitTest (pointDip); }
    void          NotifyChanged  ();
    int           HitTestSash    (POINT pointDip) const { return m_gaps.HitTestSash (pointDip, m_scaler); }
    std::wstring  GetTitle       (const std::wstring & pane) const;

    void            WireGroup         (DxuiTabGroup * group);
    bool            TearOff           (DxuiTabGroup * group, const std::wstring & pane, POINT pointDip, bool fromTab);
    bool            TearOffGroup      (DxuiTabGroup * group, const std::vector<std::wstring> & panes, const std::wstring & active, POINT pointDip);
    void            OnTitleButton     (DxuiTabGroup::TitleButton button, const std::wstring & pane, POINT pointDip);
    bool            PinPane           (const std::wstring & pane, DxuiDockSide edge);
    bool            UnpinPane         (const std::wstring & pane);
    void            StartMorph        (const RECT & from, const RECT & to) { m_morph.Start (from, to); }
    RECT            GetPaneTabRect    (const std::wstring & pane) const { return m_query.GetPaneTabRect (pane); }
    bool            IsDocumentGroup   (const std::vector<std::wstring> & panes) const;
    DxuiTabGroup  * FindGroupOf       (const std::wstring & pane) const { return m_query.FindGroupOf (pane); }
    void            FloatAtCenter     (const std::wstring & pane);

    //  Each pane's minimum size, grown by the gap while one is set.
    DxuiPaneLayout::MinSizeFn  GetMinSizeWithGap() const { return m_gaps.GetMinSizeWithGap (m_minSize, m_scaler); }

    //  IDxuiDockDragHost: the site, for the drag.
    const DxuiPaneLayout                    & GetDragLayout       () const override                                    { return m_layout; }
    DxuiPaneLayout                          & EditDragLayout      () override                                          { return m_layout; }
    size_t                                    GetDragGroupCount   () const override                                    { return m_groups.size(); }
    DxuiTabGroup                            * GetDragGroup        (size_t index) const override                        { return m_groups[index].get(); }
    const DxuiTabGroup                      * FindDragGroupOf     (const std::wstring & pane) const override           { return FindGroupOf (pane); }
    std::wstring                              GetDragPaneOf       (const IDxuiControl * content) const override        { return GetPaneOf (content); }
    const DxuiDpiScaler                     & GetDragScaler       () const override                                    { return m_scaler; }
    RECT                                      GetDragDockedArea   () const override                                    { return GetDockedArea(); }
    RECT                                      GetDragPaneArea     () const override                                    { return GetPaneArea(); }
    long                                      GetDragGapPx        () const override                                    { return m_scaler.ToPx (m_gaps.GetGapDip()); }
    std::vector<DxuiPaneLayout::GroupRect>    ArrangeForDrag      (const DxuiPaneLayout & layout) const override       { return layout.Arrange (GetPaneArea(), m_shown, GetMinSizeWithGap()); }
    bool                                      IsDragDocumentGroup (const std::vector<std::wstring> & panes) const override { return IsDocumentGroup (panes); }
    void                                      SlideInForDrag      (const std::wstring & pane) override;
    void                                      RequestDragFloat    (const std::wstring & pane, POINT pointDip) override;
    void                                      CommitDragDrop      () override                                          { Arrange(); NotifyChanged(); }

    //  IDxuiDockEdgesHost: the site, for its edges.
    const DxuiPaneLayout          & GetEdgesLayout       () const override                          { return m_layout; }
    RECT                            GetEdgesBounds       () const override                          { return m_boundsDip; }
    const DxuiDpiScaler           & GetEdgesScaler       () const override                          { return m_scaler; }
    bool                            IsEdgePaneShown      (const std::wstring & pane) const override { return !m_shown || m_shown (pane); }
    std::wstring                    GetEdgePaneTitle     (const std::wstring & pane) const override { return GetTitle (pane); }
    bool                            HasEdgePaneIndicator (const std::wstring & pane) const override;

    //  IDxuiDockQueryHost: the site, for its lookups.
    const Groups                  & GetQueryGroups      () const override                              { return m_groups; }
    std::wstring                    GetQueryPaneOf      (const IDxuiControl * content) const override  { return GetPaneOf (content); }
    IDxuiControl                  * GetQueryPaneContent (const std::wstring & pane) const override     { return GetPaneContent (pane); }
    std::wstring                    GetQueryPaneTip     (const std::wstring & pane) const override;
    bool                            IsQueryFloatingSite () const override                              { return IsFloatingSite(); }
    bool                            IsQueryAutoHidden   (const std::wstring & pane) const override     { return m_layout.IsAutoHidden (pane); }

    //  IDxuiDockMenuHost: the site, for its menus.
    DxuiPaneLayout                          & EditMenuLayout        () override                                     { return m_layout; }
    std::vector<std::wstring>                 GetActivePanes        () const override;
    std::wstring                              GetMenuPaneTitle      (const std::wstring & pane) const override      { return GetTitle (pane); }
    RECT                                      GetMenuDockedArea     () const override                               { return GetDockedArea(); }
    std::vector<DxuiPaneLayout::GroupRect>    ArrangeForMenu        () const override                               { return m_layout.Arrange (GetDockedArea(), m_shown, m_minSize); }
    const PaneTestFn                        & GetMenuDocumentFn     () const override                               { return m_isDocument; }
    bool                                      IsMenuFloatingSite    () const override                               { return IsFloatingSite(); }
    bool                                      CommitMenuChange      (bool changed) override;
    bool                                      PinFromMenu           (const std::wstring & pane, DxuiDockSide edge) override { return PinPane (pane, edge); }
    bool                                      UnpinFromMenu         (const std::wstring & pane) override            { return UnpinPane (pane); }
    bool                                      CanFloatFromMenu      () const override                               { return m_onFloat != nullptr; }
    void                                      FloatFromMenu         (const std::wstring & pane) override            { FloatAtCenter (pane); }
    bool                                      CanFloatGroupFromMenu () const override                               { return m_onFloatGroup != nullptr; }
    void                                      FloatGroupFromMenu    (const std::vector<std::wstring> & panes, const std::wstring & active) override;
    bool                                      CanCloseFromMenu      (const std::wstring & pane) const override      { return m_onClosePane && (!m_canClosePane || m_canClosePane (pane)); }
    void                                      CloseFromMenu         (const std::wstring & pane) override            { m_onClosePane (pane); }

    //  A group's corners that are the window's, as DxuiPaneFrame's kCorner
    //  flags: none while the window's corners are square.
    UINT  GetWindowCorners (const RECT & group) const;

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
    PaneFn                                        m_onFocusRequested;
    std::wstring                                  m_focusedPane;
    DxuiTabGroup                                  m_slidGroup;
    PaneFn                                        m_onSlid;
    std::wstring                                  m_slidNotified;
    std::wstring                                  m_hoverEdge;
    int                                           m_cornerDip  = 0;   // the window's corner radius
    RECT                                          m_dockedArea = {};
    RECT                                          m_paneArea   = {};
    DxuiDpiScaler                                 m_scaler;
    ChangedFn                                     m_onChanged;
    DxuiTabGroup::NewTabShownFn                   m_newTabShown;
    DxuiTabGroup::NewTabFn                        m_newTab;
    FloatFn                                       m_onFloat;
    FloatFn                                       m_onTearOff;
    GroupFloatFn                                  m_onTearOffGroup;
    GroupFloatFn                                  m_onFloatGroup;

    bool                           m_marksElsewhere = false;
    int                            m_sashDrag       = -1;
    bool                           m_arranging      = false;
    std::wstring                   m_carriedPane;
    POINT                          m_tearGrab       = {};
    bool                           m_tearFromTab    = false;
    DxuiDockDrag                   m_drag           { *this };
    DxuiDockEdges                  m_edges          { *this };
    DxuiDockGaps                   m_gaps;
    DxuiDockMorph                  m_morph;
    DxuiDockMenus                  m_menus          { *this };
    DxuiDockQuery                  m_query          { *this };
};
