#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListScrollbars.h"
#include "Widgets/ListView/DxuiListGroups.h"
#include "Widgets/ListView/DxuiListHeader.h"
#include "Widgets/ListView/DxuiListKeyboard.h"
#include "Widgets/ListView/DxuiListItemLayout.h"
#include "Widgets/ListView/DxuiListItemKeys.h"
#include "Widgets/ListView/DxuiListItemPainter.h"
#include "Widgets/ListView/DxuiListColumns.h"
#include "Widgets/ListView/DxuiListRows.h"
#include "Widgets/ListView/DxuiListBand.h"
#include "Widgets/ListView/DxuiListFlowLayout.h"
#include "Theme/DxuiTheme.h"

struct DxuiIconImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView
//
//  Multi-column scrollable themed list. One row per data item, optional
//  bold header row, per-cell text + optional dim color override.
//
//  Sizing model: each column has a fixed width in DIPs, except one
//  column (designated via stretch == true) which fills the remaining
//  horizontal space. Row height and gap are uniform.
//
//  Interaction: the various HitTest* helpers map a body-relative point
//  to a row / header column / scrollbar region. The consumer owns
//  hover / selection / focus state and pushes it back in via the
//  Set* accessors.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListView : public IDxuiControl, public DxuiListViewTypes
{
public:

    // Configuration.
    void  SetDpi          (UINT dpi);
    UINT  GetDpi          () const                         { return m_scaler.GetDpi(); }
    void  SetTheme        (const IDxuiTheme * theme)       { m_theme = theme; }
    void  SetShowHeader   (bool b)                         { m_showHeader = b; }

    //  Lays Details out as File Explorer's: rows and header set in from the
    //  left, the name's icon and each column's text where Explorer puts them,
    //  and a slightly shorter header. Off by default.
    void  SetExplorerDetails (bool on)                       { m_explorerDetails = on; }
    void  SetHoveredRow      (int row)                        { m_hovered = row; if (row >= 0) { m_grouping.ClearHover(); } }

    //  The row a drag would drop on, drawn as Explorer draws its drop target: in
    //  the hover fill, outlined in the accent. -1 draws none.
    void  SetDropRow      (int row)                        { m_dropRow = row; }
    int   GetDropRow      () const                         { return m_dropRow; }

    //  A column whose cells leave room for an icon whether they have one or
    //  not, so every name starts at the same place. -1 for none.
    void  SetIconColumn   (int column)                     { m_iconColumn = column; }

    //  Whether gaining focus with nothing selected selects the row in view, as
    //  a Tab into the list does. A host giving focus for a click turns it off
    //  for that call: a click on empty space selects nothing, as in Explorer.
    void  SetSeedRowOnFocus (bool seed)                    { m_seedRowOnFocus = seed; }
    void  SetSortIndicator  (int column, bool descending)   { m_header.m_sortColumn = column; m_header.m_sortDescending = descending; }
    void  SetRect           (const RECT & rect);
    void  SetColumns        (std::vector<Column> cols);
    void  SetRows           (std::vector<std::vector<Cell>> rows);
    void  SetRowIcon        (int row, std::shared_ptr<const DxuiIconImage> icon);

    //  A group set again under the same label keeps whether it was collapsed.
    void                        SetGroups          (std::vector<Group> groups) { m_grouping.SetGroups (groups); }
    const std::vector<Group> &  GetGroups          () const                    { return m_grouping.m_groups; }

    //  Collapsing hides a group's rows under its header, as Explorer's do.
    void                        SetGroupCollapsed      (int group, bool collapsed) { m_grouping.SetGroupCollapsed (group, collapsed); }
    void                        SetAllGroupsCollapsed  (bool collapsed) { m_grouping.SetAllGroupsCollapsed (collapsed); }
    bool                        IsGroupCollapsed       (int group) const { return m_grouping.IsGroupCollapsed (group); }

    //  Whether a group or a page is still sliding, so the host keeps painting.
    bool                        IsGroupSliding         () { return m_grouping.IsGroupSliding(); }

    //  The group whose header has the keyboard, or -1 when a row has it. A
    //  header takes focus but never selection: selecting it selects its rows.
    int                         GetFocusedGroup        () const { return m_focusGroup; }

    //  Whether an item view cut the item's name short with an ellipsis when it
    //  last drew it, so a host can show the whole of it in a tip.
    bool                        IsItemNameCut          (int row) const { return std::find (m_itemPainter.m_nameCut.begin(), m_itemPainter.m_nameCut.end(), row) != m_itemPainter.m_nameCut.end(); }
    //  The group whose header is under a point, in the same coordinates as
    //  HitTestRow, or -1.
    int   HitTestGroupHeader (int xPx, int yPx) const { return m_grouping.HitTestGroupHeader (xPx, yPx); }
    void  AppendRows         (std::vector<std::vector<Cell>> rows);
    void  SetRowProvider     (int rowCount, RowProvider provider);
    void  SetRowSource       (int rowCount, RowSource source);
    void  SetRowNameSource   (RowNameSource names)         { m_items.m_rowNames = std::move (names); }
    void  SetVirtualRowCount (int rowCount);
    bool  IsVirtual          () const                      { return m_virtual; }

    // Column visibility & widths.
    void  SetColumnVisible          (size_t idx, bool visible) { m_columnSizing.SetColumnVisible (idx, visible); }
    void  SetColumnTitle            (size_t idx, const std::wstring & title) { if (idx < m_columns.size()) { m_columns[idx].title = title; } }
    bool  IsColumnVisible           (size_t idx) const     { return (idx < m_columns.size()) && m_columns[idx].visible; }
    void  SetColumnOverrideWidthPx  (size_t idx, int px) { m_columnSizing.SetColumnOverrideWidthPx (idx, px); }

    //  Sets a column's width to what its content wants, as a double-click on
    //  its divider does in Explorer. The width becomes an override, so it is
    //  the column's width until something changes it again.
    //
    //  Measuring needs the text renderer, which only the paint pass holds, so
    //  the fit is applied on the next paint rather than here.
    void  FitColumnToContent        (size_t idx) { m_columnSizing.FitColumnToContent (idx); }

    //  As FitColumnToContent, measuring only the rows on screen, so a long
    //  list costs no more than a short one.
    void  FitColumnToVisibleContent (size_t idx) { m_columnSizing.FitColumnToVisibleContent (idx); }

    //  Every shown column fitted to its content, as FitColumnToContent does
    //  one.
    void  FitAllColumnsToContent    () { m_columnSizing.FitAllColumnsToContent(); }
    int   GetColumnOverrideWidthPx  (size_t idx) const { return m_columnSizing.GetColumnOverrideWidthPx (idx); }
    int   GetColumnEffectiveWidthPx (size_t idx) const { return m_columnSizing.GetColumnEffectiveWidthPx (idx); }
    int   GetTotalMeasuredWidthPx   () const { return m_columnSizing.GetTotalMeasuredWidthPx(); }
    void  MeasureColumnsPx          (IDxuiTextRenderer & text) const { m_columnSizing.MeasureColumnsPx (text); }

    //
    //  Monotonic content auto-fit for columns flagged auto (widthDip
    //  == 0, non-stretch). Call UpdateAutoFitFromRows after each
    //  SetRows / AppendRows: it grows each auto column's tracked width
    //  to fit its header + widest current cell and never shrinks, so a
    //  live-updating list (e.g. the debug event log) keeps wide values
    //  like cycle counts from clipping. Width is estimated from the
    //  glyph COUNT times a per-character fraction of the font size --
    //  no DWrite measurement, so it is cheap enough to run every frame
    //  and is DPI-independent (the pixel width is derived at layout
    //  time from the stored count). ResetAutoFit drops the tracked
    //  counts (e.g. on a data clear).
    //
    void  UpdateAutoFitFromRows     () { m_columnSizing.UpdateAutoFitFromRows(); }
    void  ResetAutoFit              () { m_columnSizing.ResetAutoFit(); }

    // Opt-in precise auto-fit. When on, auto (widthDip==0) columns size to the
    // DWrite-measured max(header + sort-glyph reserve, widest cell) instead of
    // the cheaper glyph-count estimate. The measurement runs once for the
    // view, not on every SetRows: widths belong to the view, and a divider
    // double-click re-fits one column on demand. Default off.
    void  SetPreciseAutoFit         (bool enabled)       { m_columnSizing.m_preciseAutoFit = enabled; }

    // Column / row queries.
    size_t         GetColumnCount () const                 { return m_columns.size(); }
    const Column & GetColumnAt    (size_t idx) const       { return m_columns[idx]; }

    // Row count honoring virtual (provider) mode. All scroll math, hit-test,
    // and paint bounds go through this so the two modes share one code path.
    int   GetRowCount              () const { return m_virtual ? m_virtualCount : (int) m_rowData.size(); }

    int   GetHoveredRow            () const                 { return m_hovered; }
    bool  IsHeaderShown            () const                 { return m_showHeader; }
    int   GetHeaderHeightPx        () const                 { return m_showHeader ? GetHeaderBarPx() : 0; }
    int   GetVisibleColumnCount    () const { return m_columnSizing.GetVisibleColumnCount(); }
    int   GetNthVisibleColumnIndex (int n) const { return m_columnSizing.GetNthVisibleColumnIndex (n); }
    int   GetVisibleIndexOfColumn  (size_t absCol) const { return m_columnSizing.GetVisibleIndexOfColumn (absCol); }

    // Selection & keyboard focus. The host (panel) drives these via Tab
    // navigation; the widget owns rendering only.
    int   GetSelectedRow          () const                 { return m_selectedRow; }
    bool  IsListFocused           () const                 { return m_listFocused; }
    int   GetFocusedHeaderColumn  () const                 { return m_header.m_focusedHeaderCol; }
    int   GetFocusedDividerColumn () const                 { return m_header.m_focusedDividerCol; }
    void  SetListFocused          (bool b)                 { m_listFocused = b; }
    void  SetFocusedHeaderColumn  (int c)                  { m_header.m_focusedHeaderCol  = (c < 0) ? -1 : c; }
    void  SetFocusedDividerColumn (int c)                  { m_header.m_focusedDividerCol = (c < 0) ? -1 : c; }
    void  SetSelectedRow          (int r);
    void  SetFocusedRow           (int r)                  { m_selectedRow = IsRowSelected (r) ? r : m_selectedRow; }

    // Multiple selection, off by default. With it on, a click selects one
    // row, Ctrl+click toggles a row, Shift+click and Shift+arrow extend from
    // the anchor, and Ctrl+A selects every row. The selected row reported by
    // GetSelectedRow is the one the keyboard is on; GetSelectedRows is the
    // whole set in ascending order, and holds that one row when multiple
    // selection is off.
    void                      SetMultiSelect  (bool enabled);
    bool                      IsMultiSelect   () const   { return m_multiSelect; }
    const std::vector<int> &  GetSelectedRows () const   { return m_selectedRows; }
    void                      SetSelectedRows (std::vector<int> rows, int anchor);
    bool                      IsRowSelected   (int row) const;
    int                       GetAnchorRow    () const   { return m_anchorRow; }

    // A click that selects, as the mouse delivers it: the modifiers decide
    // whether it replaces, toggles or extends the selection. Raises the
    // selection-changed callback with the row the click landed on.
    void                      ClickRow        (int row, bool ctrl, bool shift);

    //  Selects every row of a group, as a click on its header does.
    void                      SelectGroup     (int group) { m_grouping.SelectGroup (group); }

    // Selects every row, when multiple selection is on.
    void                      SelectAllRows   ();

    //  Copy is offered once a window to own the clipboard is set: the selected
    //  rows as text, one to a line, their cells separated by tabs.
    void          SetOwnerWindow   (HWND hwnd)    { m_ownerHwnd = hwnd; }
    std::wstring  GetSelectionText () const;

    // Opt-in keyboard column navigation. When enabled, OnKey walks the
    // header / divider sub-stops and the list body via Tab and acts on
    // them (sort on a header, resize on a divider, row moves in the body)
    // so a consumer no longer has to reimplement that routing. Default
    // off: OnKey is inert for every existing consumer.
    void  SetKeyboardColumnNav    (bool enabled)           { m_kbColNavEnabled = enabled; }

    // Selects the header keyboard model (only meaningful when column nav is
    // on). Off (default): the header is a single Tab sub-stop after the body
    // and Left/Right cycle the focused column -- File-Explorer details view.
    // On: each header and the divider trailing it are separate sub-stops and
    // Left/Right resize the column.
    void  SetKeyboardColumnResize (bool enabled)           { m_kbColResize = enabled; }

    // Vertical scroll. The widget keeps a top-row index into m_rowData;
    // Paint clips to [m_topRow, m_topRow + capacity). A list opens at its
    // top. "Sticky tail" is opt-in, for live logs: once enabled, it pins
    // the view to the bottom when new rows arrive while the user is already
    // parked at the tail.
    int   GetScrollbarWidthPx   () const                 { return m_scaler.ToPx (s_kScrollbarWidthDip); }

    //  Whether a point is on a scrollbar, and the hover that widens one. The
    //  bars' tracks are placed in widget coordinates for input and in client
    //  coordinates for painting, so either placement counts.
    bool  IsOverScrollbar   (POINT pt) const override { return IsOverBar (m_scrollbars.m_vertScroll, pt) || IsOverBar (m_scrollbars.m_horzScroll, pt); }
    bool  SetScrollbarHover (POINT pt)                { return ((int) m_scrollbars.m_vertScroll.SetHover (IsOverBar (m_scrollbars.m_vertScroll, pt), GetBarPoint (m_scrollbars.m_vertScroll, pt)) | (int) m_scrollbars.m_horzScroll.SetHover (IsOverBar (m_scrollbars.m_horzScroll, pt), GetBarPoint (m_scrollbars.m_horzScroll, pt))) != 0; }
    bool  TickScrollbars    (int64_t nowMs)           { return ((int) m_scrollbars.m_vertScroll.Tick (nowMs) | (int) m_scrollbars.m_horzScroll.Tick (nowMs)) != 0; }

    //  Whether a dragged header's neighbors are still sliding aside, so the
    //  host keeps painting until they settle.
    bool   IsHeaderSliding (int64_t nowMs) const { return m_header.IsHeaderSliding (nowMs); }
    bool   IsOverBar       (const DxuiScrollbar & bar, POINT pt) const { return bar.HitTest (pt.x, pt.y) || bar.HitTest (pt.x - m_boundsDip.left, pt.y - m_boundsDip.top); }
    POINT  GetBarPoint     (const DxuiScrollbar & bar, POINT pt) const { return bar.HitTest (pt.x, pt.y) ? pt : POINT { pt.x - m_boundsDip.left, pt.y - m_boundsDip.top }; }
    int    GetTopRow       () const                 { return m_topRow; }

    //  Draws the background and the header but no rows or bars while set, as
    //  File Explorer's list is drawn for a moment when it is refreshed.
    void  SetContentHidden      (bool hidden)            { m_contentHidden = hidden; }
    int   GetVisibleRowCapacity () const { return m_scrollbars.GetVisibleRowCapacity(); }
    int   GetMaxTopRow          () const { return m_scrollbars.GetMaxTopRow(); }
    bool  IsAtBottom            () const                 { return m_topRow >= m_scrollbars.GetMaxTopRow(); }
    void  EnableStickyTail      (bool b)                 { m_scrollbars.m_stickyTailEnabled = b; m_scrollbars.m_stickyTail = b; }
    bool  IsStickyTailEnabled   () const                 { return m_scrollbars.m_stickyTailEnabled; }
    void  SetTopRow             (int topRow) { m_scrollbars.SetTopRow (topRow); }
    void  MoveTopRow            (int topRow) { m_scrollbars.MoveTopRow (topRow); }            // by whole rows, keeping the rows' lift and any hold
    void  ScrollByRows          (int delta)              { m_scrollbars.SetTopRow (m_topRow + delta); }
    // Scroll just enough to bring `row` into the visible window, without
    // changing selection (SetSelectedRow does the same but also selects).
    void  EnsureVisible         (int row);
    // Scroll so `row` sits as close to the middle of the visible window as
    // the ends of the list allow, without changing selection.
    void  CenterOnRow           (int row);
    void  ScrollByWheelDelta    (int wheelDelta, int linesPerNotch) { m_scrollbars.ScrollByWheelDelta (wheelDelta, linesPerNotch); }
    int   GetWheelLinesPerNotch () const { return m_scrollbars.GetWheelLinesPerNotch(); }

    // Scrollbar geometry & thumb-drag. xPx/yPx are relative to the
    // widget rect. The caller starts a drag with BeginThumbDrag and
    // forwards subsequent mouse-move y values via UpdateThumbDrag.
    bool              IsScrollbarVisible        () const { return m_scrollbars.IsScrollbarVisible(); }
    ScrollbarMetrics  GetScrollbarGeometry      () const { return m_scrollbars.GetScrollbarGeometry(); }
    bool              HitTestScrollbarThumb     (int xPx, int yPx) const { return m_scrollbars.HitTestScrollbarThumb (xPx, yPx); }
    bool              HitTestScrollbarTrack     (int xPx, int yPx) const { return m_scrollbars.HitTestScrollbarTrack (xPx, yPx); }
    bool              HitTestScrollbarArrowUp   (int xPx, int yPx) const { return m_scrollbars.HitTestScrollbarArrowUp (xPx, yPx); }
    bool              HitTestScrollbarArrowDown (int xPx, int yPx) const { return m_scrollbars.HitTestScrollbarArrowDown (xPx, yPx); }
    void              GetPageFromTrackClick     (int yPx) { m_scrollbars.GetPageFromTrackClick (yPx); }
    void              BeginThumbDrag            (int grabYPx) { m_scrollbars.BeginThumbDrag (grabYPx); }
    void              UpdateThumbDrag           (int yPx) { m_scrollbars.UpdateThumbDrag (yPx); }
    void              EndThumbDrag              () { m_scrollbars.EndThumbDrag(); }
    bool              IsThumbDragging           () const { return m_scrollbars.IsThumbDragging(); }

    // Horizontal scroll (opt-in via SetHorizontalScrollEnabled; default
    // off so existing consumers are unaffected). When enabled and the
    // natural total column width exceeds the viewport, Paint offsets the
    // columns by -m_leftPx, clips them to the content viewport, and
    // shows a horizontal scrollbar along the bottom. GetContentWidthPx
    // is the natural total (no stretch fill); GetMaxLeftPx is the excess
    // of that over the viewport content width (which excludes the
    // vertical scrollbar). xPx/yPx for the hit-tests are widget-relative.
    //  A list of fixed-width text -- a hex dump, a disassembly -- sets the
    //  monospace face and a row height near the line height, instead of the
    //  proportional face and the roomy rows a file listing wants.
    void  SetMonospace                 (bool b)                { m_monospace = b; }
    bool  IsMonospace                  () const                { return m_monospace; }
    void  SetRowHeightDip              (int dip)               { m_rowHeightDip = (dip > 0) ? dip : s_kRowHeightDip; }

    //  The cells' font size; zero restores the default.
    void  SetFontDip                   (float dip)             { m_fontDip = (dip > 0.0f) ? dip : s_kFontDip; }
    int   GetRowHeightDip              () const                { return m_rowHeightDip; }

    //  A row height in pixels for a DPI, for a list that has to match one
    //  whose rows are not a single size scaled. Replaces the row height in
    //  dip while it is set.
    void  SetRowHeightPxFn             (std::function<int (UINT dpi)> fn) { m_rowHeightPxFn = std::move (fn); }

    void  SetHorizontalScrollEnabled   (bool b)                { m_hScrollEnabled = b; }
    bool  IsHorizontalScrollEnabled    () const                { return m_hScrollEnabled; }
    int   GetContentWidthPx            () const { return m_columnSizing.GetContentWidthPx(); }
    int   GetMaxLeftPx                 () const { return m_scrollbars.GetMaxLeftPx(); }
    int   GetLeftPx                    () const                { return m_leftPx; }
    void  SetLeftPx                    (int leftPx) { m_scrollbars.SetLeftPx (leftPx); }
    void  ScrollByWheelDeltaHorizontal (int wheelDelta, int pxPerNotch) { m_scrollbars.ScrollByWheelDeltaHorizontal (wheelDelta, pxPerNotch); }

    bool                  IsHorzScrollbarVisible         () const { return m_scrollbars.IsHorzScrollbarVisible(); }
    HorzScrollbarMetrics  GetHorzScrollbarGeometry       () const { return m_scrollbars.GetHorzScrollbarGeometry(); }
    bool                  HitTestHorzScrollbarThumb      (int xPx, int yPx) const { return m_scrollbars.HitTestHorzScrollbarThumb (xPx, yPx); }
    bool                  HitTestHorzScrollbarTrack      (int xPx, int yPx) const { return m_scrollbars.HitTestHorzScrollbarTrack (xPx, yPx); }
    bool                  HitTestHorzScrollbarArrowLeft  (int xPx, int yPx) const { return m_scrollbars.HitTestHorzScrollbarArrowLeft (xPx, yPx); }
    bool                  HitTestHorzScrollbarArrowRight (int xPx, int yPx) const { return m_scrollbars.HitTestHorzScrollbarArrowRight (xPx, yPx); }
    void                  GetPageFromHorzTrackClick      (int xPx) { m_scrollbars.GetPageFromHorzTrackClick (xPx); }
    void                  BeginHorzThumbDrag             (int grabXPx) { m_scrollbars.BeginHorzThumbDrag (grabXPx); }
    void                  UpdateHorzThumbDrag            (int xPx) { m_scrollbars.UpdateHorzThumbDrag (xPx); }
    void                  EndHorzThumbDrag               () { m_scrollbars.EndHorzThumbDrag(); }
    bool                  IsHorzThumbDragging            () const { return m_scrollbars.IsHorzThumbDragging(); }

    // Sizing helpers (the host dialog uses these to size itself).
    int   GetRequiredRowsForHeightPx (int heightPx) const { return m_scrollbars.GetRequiredRowsForHeightPx (heightPx); }
    int   GetRequiredHeightPx        () const { return m_scrollbars.GetRequiredHeightPx(); }

    // Hit testing (xPx/yPx relative to the list's rect.left/top).
    int   HitTestColumnResize (int xPx, int yPx, int tolerancePx) const { return m_header.HitTestColumnResize (xPx, yPx, tolerancePx); }
    int   HitTestHeaderColumn (int xPx, int yPx) const { return m_header.HitTestHeaderColumn (xPx, yPx); }
    int   HitTestRow          (int xPx, int yPx) const;

    //  One item's cell in dips (a zero width spans the list), its icon's size,
    //  whether items run down columns rather than along rows, whether the
    //  name sits under the icon, and how many text lines it has.
    //  A row's highlight, as Explorer's: this much shorter than the row at top
    //  and bottom, and ending this far short of the last column's right edge.
    //  Measured at 120 and 192 dpi.
    static constexpr int    s_kRowBoxInsetYDip   = 2;

    void                SetView          (View view);
    View                GetView          () const    { return m_view; }

    //  Under a big icon: how far into its cell the icon starts, and the gap
    //  between it and the name, as Explorer's are measured.
    static int          GetIconTopDip    (View view)  { return (view == View::MediumIcons || view == View::LargeIcons || view == View::ExtraLargeIcons) ? 2 : s_kItemPadDip; }
    static int          GetLabelGapDip   (View view)  { return (view == View::MediumIcons || view == View::LargeIcons || view == View::ExtraLargeIcons) ? 1 : s_kItemPadDip; }

    //  Explorer sets its icon views' items in from the pane's left edge and
    //  below its top. Under a big icon each item is a box of a fixed width,
    //  and a row spreads its boxes across the width left: as many as fit, the
    //  spare shared between them, the scrollbar's column kept back only while
    //  it shows.
    static float        GetItemsLeftDip  (View view)  { return (view == View::Details) ? 0.0f : s_kItemsLeftDip; }
    int                 GetItemsLeftPx   () const     { return (int) std::lround (m_scaler.ToPxf (GetItemsLeftDip (m_view))); }
    int                 GetItemBoxPx     () const { return m_items.GetItemBoxPx(); }
    static constexpr float  s_kItemsLeftDip      = 14.0f;
    static constexpr float  s_kItemRowGapDip     = 1.0f;
    static constexpr float  s_kBrokenBadgeMinDip = 10.0f;
    bool                GetItemRectPx    (int item, RECT & outRect) const { return m_items.GetItemRectPx (item, outRect); }

    //  Where a visible row's text sits in a column, after its icon, relative
    //  to the list's own top-left: what an edit box laid over the cell covers,
    //  as a rename in place does. False for a row scrolled out of view or a
    //  hidden column.
    bool  GetCellTextRectPx   (int row, size_t column, RECT & outRect) const { return m_columnSizing.GetCellTextRectPx (row, column, outRect); }

    // Self-contained mouse input. Forward widget-relative mouse events
    // (positionDip = the point minus the list's own origin) via OnMouse;
    // the list owns scrolling, thumb / column-resize drags, hover, and
    // selection, and reports semantic outcomes through these callbacks.
    // OnMouse returns true when it consumed the event so the host can
    // repaint and claim focus; IsInteracting is true mid-drag so a Win32
    // host knows to hold mouse capture.
    void  SetOnSelectionChanged (std::function<void (int)>  cb)  { m_onSelectionChanged = std::move (cb); }

    //  Selects nothing and reports it, as a click on the list's empty space
    //  does in Explorer.
    void  ClearSelection        ()                               { SetSelectedRows ({}, -1); if (m_onSelectionChanged) { m_onSelectionChanged (-1); } }
    void  SetOnActivateRow      (std::function<void (int)>  cb)  { m_onActivateRow      = std::move (cb); }
    void  SetOnSortColumn       (std::function<void (int)>  cb)  { m_header.m_onSortColumn       = std::move (cb); }
    //  A press on a filterable header's chevron, with the chevron's rect in
    //  the list's pixels, for the host to open its filter choices under.
    void  SetOnHeaderFilter     (std::function<void (int, RECT)>  cb)  { m_header.m_onFilter = std::move (cb); }
    void  SetColumnFilter       (size_t column, bool filterable, bool filtered);

    //  A press on a row's check box, or Space with the row selected; the host
    //  changes the cell's check and the list redraws it.
    void  SetOnCheckToggled     (std::function<void (int)>  cb)  { m_onCheckToggled = std::move (cb); }

    // Mouse activation policy. By default a click release over a row raises
    // onActivateRow; with double-click required, the release only activates
    // on the second click on the same row inside the system double-click
    // time. Selection still follows every click, and keyboard activation
    // (Enter / Space) is unaffected.
    void  SetActivateOnDoubleClick (bool enabled)                { m_rows.SetActivateOnDoubleClick (enabled); }
    void  SetClock                 (ClockFn clock)                               { m_keyboard.m_clock = std::move (clock); }

    //  Whether a page slides into view, as Explorer's does. Off by default.
    void  SetPageSlideEnabled (bool enabled)                     { m_items.m_pageSlideEnabled = enabled; }

    // By default the selected row only paints while the list itself holds
    // keyboard focus (its focus cue). File-picker-style consumers keep the
    // selection visible regardless, like a real list view.
    void  SetAlwaysShowSelection   (bool enabled)                { m_alwaysShowSelection = enabled; }

    // Selected rows in the accent selection color a text control uses, as a
    // preview of a file's contents does, rather than the neutral fill a file
    // list uses.
    void  SetTextSelectionColors   (bool enabled)                { m_textSelectionColors = enabled; }

    // Raised once when an interactive column-resize drag completes, with
    // the column index and its new effective width in physical pixels.
    // Lets a host that owns a persisted column model (e.g. the debug
    // panels) record the user's width without re-implementing the drag.
    void  SetOnColumnResized    (std::function<void (int, int)>  cb)  { m_onColumnResized = std::move (cb); }
    bool  IsInteracting         () const  { return m_scrollbars.IsInteracting() || m_header.IsInteracting() || m_rows.IsDragSelecting() || m_band.IsActive(); }

    //  Ends a drag that is extending the selection, keeping what it selected,
    //  for a host that turns the drag into something else, such as moving the
    //  rows, and so never passes on the release.
    void  EndDragSelect         ()        { m_rows.EndDragSelect(); }

    //  The order the columns are shown in, left to right, as indexes into the
    //  columns; SetColumns starts it in their own order. A header dragged along
    //  the strip moves its column, as in Explorer, and reports the new order.
    const std::vector<size_t>  & GetColumnOrder        () const { return m_header.GetColumnOrder(); }
    void                         SetColumnOrder        (const std::vector<size_t> & order) { m_header.SetColumnOrder (order); }
    void                         SetOnColumnsReordered (std::function<void (const std::vector<size_t> &)> cb) { m_header.m_onColumnsReordered = std::move (cb); }

    //  Where a column dropped at this point goes, as a position in the order.
    int   GetColumnDropPosition (int xPx) const { return m_header.GetColumnDropPosition (xPx); }

    //  The rubber band an item view draws while the pointer drags from empty
    //  space, in the list's pixels; empty when none is being drawn.
    RECT  GetSelectionBandPx    () const { return m_band.GetRectPx(); }
    bool  IsResizingColumn      () const  { return m_header.IsResizingColumn(); }

    // Auto-repeat for a held scrollbar arrow / track press (like key
    // repeat). The host drives this once per frame with a monotonic
    // millisecond clock; after the initial delay the pressed arrow / page
    // action fires at the repeat interval until the button is released.
    // No-op when no arrow / track press is active.
    void  Tick (int64_t nowMs) { m_scrollbars.Tick (nowMs); }

    // Rendering.
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides. OnMouse makes the list self-contained:
    //  the host forwards widget-relative mouse events and the list runs
    //  its own hit-test / scroll / drag / selection routing, raising the
    //  callbacks above for the semantic outcomes. OnKey drives the opt-in
    //  keyboard column navigation (see SetKeyboardColumnNav); it is inert
    //  unless that flag is set, so existing consumers are unaffected.
    //
    DxuiListView() { m_focusable = true; }
    ~DxuiListView () override = default;

    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;

    //  Windows' scrollbar menu when the point is on one of the bars, in the
    //  coordinates OnMouse takes; empty elsewhere. Its rows scroll the view.
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu  (POINT pt);
    bool                            ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, GetScrollbarMenu (POINT { pt.x - m_boundsDip.left, pt.y - m_boundsDip.top })); }
    LPCWSTR                         GetCursorForPoint (POINT clientPx) const       override;
    bool                            OnKey             (const DxuiKeyEvent   & ev) override;

    //  A dialog passes typed characters here rather than to OnKey.
    bool                            OnChar            (wchar_t ch) override { return OnKey (DxuiKeyEvent { DxuiKeyEventKind::Char, (WPARAM) ch }); }
    bool                            QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                            InvokeCommand     (DxuiStandardCommand command) override;
    void                            OnFocusChanged    (bool focused) override;
    DxuiAccessibleRole              GetAccessibleRole () const override { return DxuiAccessibleRole::ListView; }

private:
    static constexpr int    s_kRowHeightDip          = 30;
    static constexpr int    s_kHeaderHeightDip       = 32;
    static constexpr int    s_kHeaderGapDip          = 2;

    //  Explorer's Details, measured at 150%.
    static constexpr int    s_kExplorerHeaderDip     = 31;
    static constexpr int    s_kExplorerHeaderGapDip  = 1;
    static constexpr float  s_kExplorerIconGapDip    = 4.0f;     // from the icon to the name

    int   GetHeaderBarPx      () const { return m_scaler.ToPx (m_explorerDetails ? s_kExplorerHeaderDip : s_kHeaderHeightDip); }
    int   GetHeaderGapPx      () const { return m_scaler.ToPx (m_explorerDetails ? s_kExplorerHeaderGapDip : s_kHeaderGapDip); }
    int   GetCellIconGapPx    () const { return m_explorerDetails ? (int) std::lround (m_scaler.ToPxf (s_kExplorerIconGapDip)) : m_scaler.ToPx (DxuiListColumns::s_kCellIconGapDip); }
    int   GetDetailsLeftPx    () const { return m_explorerDetails ? (int) std::lround (m_scaler.ToPxf (s_kItemsLeftDip)) : 0; }
    int   GetCheckBoxSpanPx   () const { return m_scaler.ToPx (s_kCheckBoxDip + s_kCheckBoxGapDip); }
    static constexpr int    s_kCheckBoxDip       = 16;
    static constexpr int    s_kCheckBoxGapDip    = 8;
    static constexpr int    s_kGroupLabelDip     = 22;   // the header's label, from the list's left
    static constexpr int    s_kCellPadRightDip   = 16;
    static constexpr int    s_kSortGlyphWidthDip = 10;
    static constexpr int    s_kScrollbarWidthDip = 10;
    static constexpr int    s_kCellIconDip       = 16;
    static constexpr int    s_kMinColWidthDip    = 48;
    static constexpr int    s_kResizeGrabDip     = 4;
    static constexpr int    s_kHScrollStepDip    = 32;
    static constexpr float  s_kFontDip           = 13.0f;
    static constexpr float  s_kHeaderFontDip     = 13.0f;

    //  Lines are what Details view stacks and scrolls by: each row, and a
    //  header above each group. Without groups a line is a row.
    bool         HasGroupLines () const { return !m_grouping.m_groups.empty() && !IsItemsView(); }
    static bool  IsAtPageEdge  (int cur, int edge, bool down) { return cur >= 0 && (down ? cur >= edge : cur <= edge); }

    static constexpr int64_t  s_kPageSlideMs = 175;
    int                     GetShownScrollPx   () const { return m_items.GetRestScrollPx() - m_items.GetPageSlidePx(); }

    // Fill `out` with row `r`'s cells: from the provider in virtual mode, or
    // a copy of m_rowData[r] otherwise. Used by Paint's visible-window pull.
    void         ProvideRow          (int r, std::vector<Cell> & out) const;

    static constexpr int  s_kItemPadDip  = 6;
    static constexpr int  s_kItemLineDip = 18;

    bool          IsItemsView             () const { return m_view != View::Details; }

    //  Page Up, Page Down, Home and End over the lines of items.
    static bool   IsItemLine              (const ItemLine & line) { return line.group < 0 && line.count > 0; }

    //  The big icon views wrap a name to as many as four lines, as Explorer's
    //  do, and each row of items grows to fit its tallest name. A name is
    //  measured the first time it is drawn; until then it takes the two lines
    //  every cell has room for.
    static constexpr int    s_kItemNameMaxLines = 4;
    static constexpr float  s_kNameTextGapDip   = 2.0f;
    static constexpr float  s_kNameColumnGapDip = 11.33f;

    //  Explorer's grouped List, measured at 150%: its items 18 dip further
    //  left of their label than the header's own indent, and each column
    //  10.67 dip narrower than an ungrouped one.
    static constexpr float  s_kListGroupItemsPullDip = 18.0f;
    static constexpr int    s_kTileMaxLines          = 3;
    int           GetNameTextGapPx        () const { return (int) std::lround (m_scaler.ToPxf (s_kNameTextGapDip)); }

    //  What is left, in a grouped List column, between its widest name and
    //  the next column, after Explorer's tighter packing.
    int           GetListGroupGapPx       (const ItemGrid & grid) const
    {
        return (std::max) (0, (grid.cellW - grid.boxW) - (int) std::lround (m_scaler.ToPxf (DxuiListFlowLayout::s_kListGroupColumnTrimDip)));
    }

    static inline const ListLayout  s_kNoListLayout {};
    // The cell vector for row `r`: the provider scratch in virtual mode
    // (pulled + auto-fit noted as a side effect), m_rowData[r] otherwise.
    const std::vector<Cell> & GetRowCells (int r) const;
    // Clamp m_topRow / sticky-tail after the row count changes (shared by
    // SetRows / AppendRows / SetVirtualRowCount / SetRowProvider).
    void         ClampTopAfterCountChange (bool wasSticky);

    // Sets the selection to the rows from the anchor to `row`, inclusive.
    void    SelectRangeFromAnchor    (int row);

    // Drops selected rows past the end after the row count changes.
    void    PruneSelection           ();

    //  The colors the rows, items, header and bars are drawn in, from the theme.
    Palette  MakePalette () const;

    //  A press, the drag that follows and its release, routed to the bars, the
    //  header, the groups, the rows and the band in turn.
    bool  OnPress   (const DxuiMouseEvent & ev, int lx, int ly, bool inside);
    bool  OnDrag    (int lx, int ly, bool inside);
    bool  OnRelease (int lx, int ly, bool inside);
    const IDxuiTheme                * m_theme     = nullptr;
    std::vector<Column>               m_columns;
    std::vector<std::vector<Cell>>    m_rowData;
    HWND                              m_ownerHwnd = nullptr;
    // Per-column pixel width fitted to the header + widest cell via
    // MeasureColumnsPx (DWrite). Monotonic and persists across SetRows so
    // filter/sort don't collapse content-fit columns; reset by SetColumns.
    // Preferred over m_autoMaxChars wherever a non-zero entry exists.
    //  The cells' face and the height of a row, which a fixed-width list
    //  (a hex dump, a disassembly) changes together.
    const wchar_t *  GetBodyFace   () const  { return m_monospace ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace; }
    int              GetRowHeightPx() const  { return m_rowHeightPxFn ? m_rowHeightPxFn (m_scaler.GetDpi()) : (int) m_scaler.ToPxf ((float) m_rowHeightDip); }

    bool   m_monospace       = false;
    int    m_rowHeightDip    = s_kRowHeightDip;
    bool   m_explorerDetails = false;
    float  m_fontDip         = s_kFontDip;
    std::function<int (UINT)> m_rowHeightPxFn;
    DxuiDpiScaler             m_scaler;
    // Virtual (provider) row model — see SetRowProvider. When m_virtual is
    // true, m_rowData is empty and rows are pulled on demand into m_providerScratch.
    bool                       m_virtual             = false;
    RowSource                  m_rowSource;
    int                        m_virtualCount        = 0;
    RowProvider                m_rowProvider;
    mutable std::vector<Cell>  m_providerScratch;
    int                        m_iconColumn          = -1;
    bool                       m_seedRowOnFocus      = true;
    int                        m_selectedRow         = -1;
    bool                       m_multiSelect         = false;
    std::vector<int>           m_selectedRows;
    int                        m_anchorRow           = -1;
    int                        m_hovered             = -1;
    int                        m_dropRow             = -1;
    bool                       m_alwaysShowSelection = false;
    bool                       m_textSelectionColors = false;
    bool                       m_showHeader          = false;
    View                       m_view                = View::Details;
    bool                       m_detailsHeader       = false;   // the header Details had, while another view shows
    int                        m_topRow              = 0;
    bool                       m_flushBottom         = false;   // Details: the rows lifted so the last whole one ends at the bottom
    bool                       m_listFocused         = false;
    int                        m_focusGroup          = -1;
    int                        m_rowsVersion         = 0;   // counts row sets, so measurements of the last one are dropped
    bool                       m_kbColNavEnabled     = false;
    bool                       m_kbColResize         = false;
    bool                       m_hScrollEnabled      = false;
    int                        m_leftPx              = 0;
    bool                       m_contentHidden       = false;
    std::function<void (int)>         m_onSelectionChanged;
    std::function<void (int)>         m_onActivateRow;
    std::function<void (int, int)>    m_onColumnResized;
    std::function<void (int)>         m_onCheckToggled;

    int64_t  GetClockMs        () const;


    //  The list's parts, each with the state it uses.
    friend class         DxuiListScrollbars;
    friend class         DxuiListGroups;
    friend class         DxuiListHeader;
    friend class         DxuiListKeyboard;
    friend class         DxuiListItemLayout;
    friend class         DxuiListItemKeys;
    friend class         DxuiListItemPainter;
    friend class         DxuiListColumns;
    friend class         DxuiListRows;
    friend class         DxuiListBand;
    friend class         DxuiListFlowLayout;
    DxuiListScrollbars   m_scrollbars        { *this };
    DxuiListGroups       m_grouping          { *this };
    DxuiListHeader       m_header            { *this };
    DxuiListKeyboard     m_keyboard          { *this };
    DxuiListItemLayout   m_items             { *this };
    DxuiListItemKeys     m_itemKeys          { *this };
    DxuiListItemPainter  m_itemPainter       { *this };
    DxuiListColumns      m_columnSizing      { *this };
    DxuiListRows         m_rows              { *this };
    DxuiListBand         m_band              { *this };
    DxuiListFlowLayout   m_flow              { *this };
};

