#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
#include "Render/IDxuiTextRenderer.h"
#include "DxuiScrollbar.h"





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

class DxuiListView : public IDxuiControl
{
public:
    struct Column
    {
        std::wstring    title;
        int             widthDip = 0;   // 0 = auto-fit content (or stretch if stretch=true)
        bool            stretch  = false;   // when true, column absorbs any remaining width after fixed/auto
        DxuiTextHAlign  align    = DxuiTextHAlign::Left;
        bool            visible  = true;
    };

    struct Cell
    {
        std::wstring  text;
        bool          dim = false;     // muted color (e.g. "(Download)" hint)

        // Half-open character ranges [first, second) within `text` to paint
        // as search-match highlights. Empty = none. Supplied sorted and
        // non-overlapping; honored only for left-aligned columns.
        std::vector<std::pair<int, int>>  matches;

        //  Half-open character ranges drawn in the muted color within text
        //  that is otherwise not. Sorted, non-overlapping, left-aligned
        //  columns only, as matches are.
        std::vector<std::pair<int, int>>  dimRanges;

        //  Drawn at the start of the cell, the text moved along past it.
        std::shared_ptr<const DxuiIconImage>  icon;

        //  The icon drawn at half opacity, as Explorer draws a hidden item's.
        bool          iconGhosted = false;

        //  A red badge with a cross at the icon's lower left, for an item
        //  that cannot be opened.
        bool          iconBroken  = false;

        //  The text's color in place of the theme's; zero keeps the theme's.
        uint32_t      argb        = 0;

        //  A usage bar drawn in place of the text, filled to this fraction;
        //  negative draws none. Explorer's drive tiles have one.
        float         meter       = -1.0f;

        //  The lines Tiles and Content draw under the name in place of the
        //  other columns, for a row whose tile shows something else.
        std::vector<Cell>  tileLines;

        //  Tiles shows the name alone, as Explorer's does for a folder; the
        //  Content view still shows the other columns.
        bool          tileNameOnly = false;

        //  The Content view's lines: under the name (the type), and in the
        //  column to its right (the date and the size). Empty draws none.
        std::vector<Cell>  contentLeft;
        std::vector<Cell>  contentRight;
    };

    // Geometry of every interactive scrollbar region, in coordinates
    // relative to the widget rect's top-left. arrowH == 0 means the bar
    // is too short for arrow buttons.
    struct ScrollbarMetrics
    {
        bool   visible      = false;
        int    barX         = 0;
        int    barW         = 0;
        int    arrowH       = 0;
        int    upArrowTop   = 0;
        int    downArrowTop = 0;
        int    trackTop     = 0;
        int    trackH       = 0;
        float  thumbTop     = 0.0f;
        float  thumbH       = 0.0f;
    };

    // Horizontal-scroll counterpart to ScrollbarMetrics, in coordinates
    // relative to the widget rect's top-left. arrowW == 0 means the bar
    // is too short for arrow buttons.
    struct HorzScrollbarMetrics
    {
        bool   visible     = false;
        int    barY        = 0;
        int    barH        = 0;
        int    arrowW      = 0;
        int    leftArrowX  = 0;
        int    rightArrowX = 0;
        int    trackLeft   = 0;
        int    trackW      = 0;
        float  thumbLeft   = 0.0f;
        float  thumbW      = 0.0f;
    };

    // Configuration.
    void  SetDpi          (UINT dpi);
    UINT  GetDpi          () const                         { return m_scaler.GetDpi(); }
    void  SetTheme        (const IDxuiTheme * theme)       { m_theme = theme; }
    void  SetShowHeader   (bool b)                         { m_showHeader = b; }

    //  Lays Details out as File Explorer's: rows and header set in from the
    //  left, the name's icon and each column's text where Explorer puts them,
    //  and a slightly shorter header. Off by default.
    void  SetExplorerDetails (bool on)                       { m_explorerDetails = on; }
    void  SetHoveredRow   (int row)                        { m_hovered = row; if (row >= 0) { m_hoverGroup = -1; } }

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
    void  SetSortIndicator (int column, bool descending)   { m_sortColumn = column; m_sortDescending = descending; }
    void  SetRect         (const RECT & rect);
    void  SetColumns      (std::vector<Column> cols);
    void  SetRows         (std::vector<std::vector<Cell>> rows);
    void  SetRowIcon      (int row, std::shared_ptr<const DxuiIconImage> icon);

    //  Groups over the rows, as Explorer's Group by draws them: a header line
    //  above each group's first row, which the rows must already be ordered
    //  for. Row indices everywhere stay the rows' own; only the lines they
    //  are drawn on move. Details view only; empty for none.
    struct Group
    {
        std::wstring  label;
        int           firstRow  = 0;
        bool          collapsed = false;   // its rows hidden, the header kept
    };

    //  A group set again under the same label keeps whether it was collapsed.
    void                        SetGroups          (std::vector<Group> groups);
    const std::vector<Group> &  GetGroups          () const                    { return m_groups; }

    //  Collapsing hides a group's rows under its header, as Explorer's do.
    void                        SetGroupCollapsed      (int group, bool collapsed);
    void                        SetAllGroupsCollapsed  (bool collapsed);
    bool                        IsGroupCollapsed       (int group) const;

    //  Whether a group or a page is still sliding, so the host keeps painting.
    bool                        IsGroupSliding         ();

    //  The group whose header has the keyboard, or -1 when a row has it. A
    //  header takes focus but never selection: selecting it selects its rows.
    int                         GetFocusedGroup        () const { return m_focusGroup; }

    //  Whether an item view cut the item's name short with an ellipsis when it
    //  last drew it, so a host can show the whole of it in a tip.
    bool                        IsItemNameCut          (int row) const { return std::find (m_nameCut.begin(), m_nameCut.end(), row) != m_nameCut.end(); }
    //  The group whose header is under a point, in the same coordinates as
    //  HitTestRow, or -1.
    int                         HitTestGroupHeader (int xPx, int yPx) const;
    void  AppendRows      (std::vector<std::vector<Cell>> rows);

    // Virtual (provider) row model. Instead of materializing every row up
    // front via SetRows, the host supplies a total row count plus a callback
    // that fills one row's cells on demand. Paint pulls only the rows in the
    // visible window, so a 100k-row live log costs O(visible) per frame
    // instead of O(total) allocations (GH #88). Mutually exclusive with
    // SetRows/AppendRows: installing a provider drops any pushed rows, and
    // SetRows/AppendRows clears provider mode. The provider is invoked during
    // Paint (const) for rows in [GetTopRow(), +capacity); it must be a pure
    // read of host state valid for the ListView's lifetime.
    using RowProvider = std::function<void (int row, std::vector<Cell> & out)>;
    void  SetRowProvider     (int rowCount, RowProvider provider);

    // A row source is a provider that hands back the host's own cells by
    // reference, for a host that keeps them: nothing is copied per paint, and
    // the host builds a row only when the list first asks for it. The
    // reference must stay valid until the next call.
    using RowSource = std::function<const std::vector<Cell> & (int row)>;
    void  SetRowSource       (int rowCount, RowSource source);

    // Each row's name alone, for a host that builds its rows' cells only as
    // they show: Small icons and List measure every name to size their
    // columns, and read it from here rather than building every row.
    using RowNameSource = std::function<std::wstring (int row)>;
    void  SetRowNameSource   (RowNameSource names)         { m_rowNames = std::move (names); }
    void  SetVirtualRowCount (int rowCount);
    bool  IsVirtual          () const                      { return m_virtual; }

    // Column visibility & widths.
    void  SetColumnVisible          (size_t idx, bool visible);
    void  SetColumnTitle            (size_t idx, const std::wstring & title) { if (idx < m_columns.size()) { m_columns[idx].title = title; } }
    bool  IsColumnVisible           (size_t idx) const     { return (idx < m_columns.size()) && m_columns[idx].visible; }
    void  SetColumnOverrideWidthPx  (size_t idx, int px);

    //  Sets a column's width to what its content wants, as a double-click on
    //  its divider does in Explorer. The width becomes an override, so it is
    //  the column's width until something changes it again.
    //
    //  Measuring needs the text renderer, which only the paint pass holds, so
    //  the fit is applied on the next paint rather than here.
    void  FitColumnToContent        (size_t idx);

    //  Every shown column fitted to its content, as FitColumnToContent does
    //  one.
    void  FitAllColumnsToContent    ();
    int   GetColumnOverrideWidthPx  (size_t idx) const;
    int   GetColumnEffectiveWidthPx (size_t idx) const;
    int   GetTotalMeasuredWidthPx   () const;
    void  MeasureColumnsPx          (IDxuiTextRenderer & text) const;

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
    void  UpdateAutoFitFromRows     ();
    void  ResetAutoFit              ();

    // Opt-in precise auto-fit. When on, auto (widthDip==0) columns size to the
    // DWrite-measured max(header + sort-glyph reserve, widest cell) instead of
    // the cheaper glyph-count estimate. The measurement runs once for the
    // view, not on every SetRows: widths belong to the view, and a divider
    // double-click re-fits one column on demand. Default off.
    void  SetPreciseAutoFit         (bool enabled)       { m_preciseAutoFit = enabled; }

    // Column / row queries.
    size_t         GetColumnCount () const                 { return m_columns.size(); }
    const Column & GetColumnAt    (size_t idx) const       { return m_columns[idx]; }

    // Row count honoring virtual (provider) mode. All scroll math, hit-test,
    // and paint bounds go through this so the two modes share one code path.
    int   GetRowCount              () const { return m_virtual ? m_virtualCount : (int) m_rows.size(); }

    int   GetHoveredRow            () const                 { return m_hovered; }
    bool  IsHeaderShown            () const                 { return m_showHeader; }
    int   GetHeaderHeightPx        () const                 { return m_showHeader ? GetHeaderBarPx() : 0; }
    int   GetVisibleColumnCount    () const;
    int   GetNthVisibleColumnIndex (int n) const;
    int   GetVisibleIndexOfColumn  (size_t absCol) const;

    // Selection & keyboard focus. The host (panel) drives these via Tab
    // navigation; the widget owns rendering only.
    int   GetSelectedRow          () const                 { return m_selectedRow; }
    bool  IsListFocused           () const                 { return m_listFocused; }
    int   GetFocusedHeaderColumn  () const                 { return m_focusedHeaderCol; }
    int   GetFocusedDividerColumn () const                 { return m_focusedDividerCol; }
    void  SetListFocused          (bool b)                 { m_listFocused = b; }
    void  SetFocusedHeaderColumn  (int c)                  { m_focusedHeaderCol  = (c < 0) ? -1 : c; }
    void  SetFocusedDividerColumn (int c)                  { m_focusedDividerCol = (c < 0) ? -1 : c; }
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
    void                      SelectGroup     (int group);

    // Selects every row, when multiple selection is on.
    void                      SelectAllRows   ();

    //  Copy is offered once a window to own the clipboard is set: the selected
    //  rows as text, one to a line, their cells separated by tabs.
    void                      SetOwnerWindow  (HWND hwnd)    { m_ownerHwnd = hwnd; }
    std::wstring              GetSelectionText () const;

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

    // Vertical scroll. The widget keeps a top-row index into m_rows;
    // Paint clips to [m_topRow, m_topRow + capacity). A list opens at its
    // top. "Sticky tail" is opt-in, for live logs: once enabled, it pins
    // the view to the bottom when new rows arrive while the user is already
    // parked at the tail.
    int   GetScrollbarWidthPx   () const                 { return m_scaler.ToPx (s_kScrollbarWidthDip); }

    //  Whether a point is on a scrollbar, and the hover that widens one. The
    //  bars' tracks are placed in widget coordinates for input and in client
    //  coordinates for painting, so either placement counts.
    bool  IsOverScrollbar   (POINT pt) const override { return IsOverBar (m_vertScroll, pt) || IsOverBar (m_horzScroll, pt); }
    bool  SetScrollbarHover (POINT pt)                { return ((int) m_vertScroll.SetHover (IsOverBar (m_vertScroll, pt), GetBarPoint (m_vertScroll, pt)) | (int) m_horzScroll.SetHover (IsOverBar (m_horzScroll, pt), GetBarPoint (m_horzScroll, pt))) != 0; }
    bool  TickScrollbars    (int64_t nowMs)           { return ((int) m_vertScroll.Tick (nowMs) | (int) m_horzScroll.Tick (nowMs)) != 0; }

    //  Whether a dragged header's neighbors are still sliding aside, so the
    //  host keeps painting until they settle.
    bool  IsHeaderSliding   (int64_t nowMs) const;
    bool  IsOverBar         (const DxuiScrollbar & bar, POINT pt) const { return bar.HitTest (pt.x, pt.y) || bar.HitTest (pt.x - m_boundsDip.left, pt.y - m_boundsDip.top); }
    POINT GetBarPoint       (const DxuiScrollbar & bar, POINT pt) const { return bar.HitTest (pt.x, pt.y) ? pt : POINT { pt.x - m_boundsDip.left, pt.y - m_boundsDip.top }; }
    int   GetTopRow             () const                 { return m_topRow; }
    int   GetVisibleRowCapacity () const;
    int   GetMaxTopRow          () const;
    bool  IsAtBottom            () const                 { return m_topRow >= GetMaxTopRow(); }
    void  EnableStickyTail      (bool b)                 { m_stickyTailEnabled = b; m_stickyTail = b; }
    bool  IsStickyTailEnabled   () const                 { return m_stickyTailEnabled; }
    void  SetTopRow             (int topRow);
    void  MoveTopRow            (int topRow);            // by whole rows, keeping the rows' lift and any hold
    void  ScrollByRows          (int delta)              { SetTopRow (m_topRow + delta); }
    // Scroll just enough to bring `row` into the visible window, without
    // changing selection (SetSelectedRow does the same but also selects).
    void  EnsureVisible         (int row);
    // Scroll so `row` sits as close to the middle of the visible window as
    // the ends of the list allow, without changing selection.
    void  CenterOnRow           (int row);
    void  ScrollByWheelDelta    (int wheelDelta, int linesPerNotch);
    int   GetWheelLinesPerNotch () const;

    // Scrollbar geometry & thumb-drag. xPx/yPx are relative to the
    // widget rect. The caller starts a drag with BeginThumbDrag and
    // forwards subsequent mouse-move y values via UpdateThumbDrag.
    bool             IsScrollbarVisible        () const;
    ScrollbarMetrics GetScrollbarGeometry      () const;
    bool             HitTestScrollbarThumb     (int xPx, int yPx) const;
    bool             HitTestScrollbarTrack     (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowUp   (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowDown (int xPx, int yPx) const;
    void             GetPageFromTrackClick        (int yPx);
    void             BeginThumbDrag            (int grabYPx);
    void             UpdateThumbDrag           (int yPx);
    void             EndThumbDrag              ()                            { m_vertDragging = false; m_vertDragGrab = 0.0f; m_vertScroll.SetDragOffset (std::nullopt); }
    bool             IsThumbDragging           () const                      { return m_vertDragging; }

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
    int   GetContentWidthPx            () const;
    int   GetMaxLeftPx                 () const;
    int   GetLeftPx                    () const                { return m_leftPx; }
    void  SetLeftPx                    (int leftPx);
    void  ScrollByWheelDeltaHorizontal (int wheelDelta, int pxPerNotch);

    bool              IsHorzScrollbarVisible         () const;
    HorzScrollbarMetrics GetHorzScrollbarGeometry       () const;
    bool              HitTestHorzScrollbarThumb      (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarTrack      (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarArrowLeft  (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarArrowRight (int xPx, int yPx) const;
    void              GetPageFromHorzTrackClick         (int xPx);
    void              BeginHorzThumbDrag             (int grabXPx);
    void              UpdateHorzThumbDrag            (int xPx);
    void              EndHorzThumbDrag               ()                          { m_horzDragging = false; m_horzDragGrab = 0.0f; }
    bool              IsHorzThumbDragging            () const                    { return m_horzDragging; }

    // Sizing helpers (the host dialog uses these to size itself).
    int   GetRequiredRowsForHeightPx (int heightPx) const;
    int   GetRequiredHeightPx        () const;

    // Hit testing (xPx/yPx relative to the list's rect.left/top).
    int   HitTestColumnResize (int xPx, int yPx, int tolerancePx) const;
    int   HitTestHeaderColumn (int xPx, int yPx) const;
    int   HitTestRow          (int xPx, int yPx) const;

    //  Explorer's eight views. Details is the table of columns; the others
    //  lay the rows out as items with the first cell's icon and text, and
    //  Tiles and Content add the next cells on the lines below. Selection,
    //  keyboard movement, hit testing and the callbacks mean the same in each.
    enum class View { Details, ExtraLargeIcons, LargeIcons, MediumIcons, SmallIcons, List, Tiles, Content };

    //  One item's cell in dips (a zero width spans the list), its icon's size,
    //  whether items run down columns rather than along rows, whether the
    //  name sits under the icon, and how many text lines it has.
    //  A row's highlight, as Explorer's: this much shorter than the row at top
    //  and bottom, and ending this far short of the last column's right edge.
    //  Measured at 120 and 192 dpi.
    static constexpr int    s_kRowBoxInsetYDip   = 2;
    static constexpr int    s_kRowBoxEndInsetDip = 4;

    //  How opaque a ghosted icon is: Explorer's hidden items, measured.
    static constexpr float  s_kGhostedIconAlpha = 0.5f;

    //  Explorer's drive usage bar, measured: its size, its fill, track and
    //  edge, and the red it turns once the drive is nearly full.
    static constexpr int       s_kMeterWidthDip  = 190;
    static constexpr int       s_kMeterHeightDip = 14;
    static constexpr float     s_kMeterFullAt    = 0.9f;
    static constexpr uint32_t  s_kMeterFillArgb  = 0xFF0070CBu;
    static constexpr uint32_t  s_kMeterFullArgb  = 0xFFDA2626u;
    static constexpr uint32_t  s_kMeterTrackArgb = 0xFFE6E6E6u;
    static constexpr uint32_t  s_kMeterEdgeArgb  = 0xFFBCBCBCu;

    struct ItemMetrics
    {
        int   cellWDip   = 0;
        int   cellHDip   = 0;
        int   iconDip    = 0;
        bool  columns    = false;
        bool  labelBelow = false;
        int   textLines  = 1;

        //  The pitch of the lines beside the icon.
        int   lineDip    = 18;
    };

    void                SetView          (View view);
    View                GetView          () const    { return m_view; }
    static ItemMetrics  GetItemMetrics   (View view);

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
    int                 GetItemBoxPx     () const;
    static constexpr float  s_kItemsLeftDip        = 14.0f;
    static constexpr int    s_kItemsTopDip         = 6;
    static constexpr float  s_kItemsBarDip         = 17.33f;
    static constexpr int    s_kMeasureAllItemsMax  = 500;
    //  Measured at 100%, 125% and 150%. Medium's and Large's boxes are widths
    //  in dip, a part pixel dropped, and Large's is wider from 150% up; Extra large's is its icon, which stops at 256 px, and a margin
    //  part pixels and part dip. A box is as tall as its icon, its caption
    //  lines and 5.33 dip, and a row is a dip taller than its boxes.
    static constexpr float  s_kMediumBoxDip        = 74.0f;
    static constexpr float  s_kLargeBoxDip         = 107.33f;
    static constexpr float  s_kLargeBoxLowDpiDip   = 105.0f;   // Large's box below 150%
    static constexpr int    s_kXLargeBoxPadPx      = 9;
    static constexpr float  s_kXLargeBoxPadDip     = 6.0f;
    static constexpr float  s_kItemBoxPadDip       = 5.33f;
    static constexpr float  s_kItemRowGapDip       = 1.0f;

    //  The badge on an item that cannot be opened, over its icon's lower-left
    //  corner, for the tree's icons as well as the list's. On the text layer,
    //  which icons are drawn on, so it goes over its icon.
    static void         PaintBrokenBadge (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx);
    static constexpr float  s_kBrokenBadgeMinDip = 10.0f;

    //  An item's icon in pixels: the view's size, scaled, but never past the
    //  256 pixels of the shell's largest icon, as Explorer draws Extra large.
    static constexpr int  s_kMaxItemIconPx = 256;
    static int          GetItemIconPx    (View view, UINT dpi);
    bool                GetItemRectPx    (int item, RECT & outRect) const;

    //  Where a visible row's text sits in a column, after its icon, relative
    //  to the list's own top-left: what an edit box laid over the cell covers,
    //  as a rename in place does. False for a row scrolled out of view or a
    //  hidden column.
    bool  GetCellTextRectPx   (int row, size_t column, RECT & outRect) const;

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
    void  SetOnSortColumn       (std::function<void (int)>  cb)  { m_onSortColumn       = std::move (cb); }

    // Mouse activation policy. By default a click release over a row raises
    // onActivateRow; with double-click required, the release only activates
    // on the second click on the same row inside the system double-click
    // time. Selection still follows every click, and keyboard activation
    // (Enter / Space) is unaffected.
    void  SetActivateOnDoubleClick (bool enabled)                { m_activateOnDoubleClick = enabled; }

    //  Milliseconds since some fixed point, for type-ahead's reset. Defaults
    //  to the system tick count; a test supplies its own.
    using ClockFn = std::function<int64_t()>;
    void  SetClock (ClockFn clock)                               { m_clock = std::move (clock); }

    //  Whether a page slides into view, as Explorer's does. Off by default.
    void  SetPageSlideEnabled (bool enabled)                     { m_pageSlideEnabled = enabled; }

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
    bool  IsInteracting         () const  { return m_vertDragging || m_horzDragging || m_resizeColumn >= 0 || m_scrollRepeat != ScrollRepeat::None || m_dragSelecting || m_bandActive || m_headerPressCol >= 0; }

    //  Ends a drag that is extending the selection, keeping what it selected,
    //  for a host that turns the drag into something else, such as moving the
    //  rows, and so never passes on the release.
    void  EndDragSelect         ()        { m_dragSelecting = false; }

    //  The order the columns are shown in, left to right, as indexes into the
    //  columns; SetColumns starts it in their own order. A header dragged along
    //  the strip moves its column, as in Explorer, and reports the new order.
    const std::vector<size_t> &  GetColumnOrder () const { return m_columnOrder; }
    void                         SetColumnOrder (const std::vector<size_t> & order);
    void                         SetOnColumnsReordered (std::function<void (const std::vector<size_t> &)> cb) { m_onColumnsReordered = std::move (cb); }

    //  Where a column dropped at this point goes, as a position in the order.
    int   GetColumnDropPosition (int xPx) const;

    //  The rubber band an item view draws while the pointer drags from empty
    //  space, in the list's pixels; empty when none is being drawn.
    RECT  GetSelectionBandPx    () const;
    bool  IsResizingColumn      () const  { return m_resizeColumn >= 0; }

    // Auto-repeat for a held scrollbar arrow / track press (like key
    // repeat). The host drives this once per frame with a monotonic
    // millisecond clock; after the initial delay the pressed arrow / page
    // action fires at the repeat interval until the button is released.
    // No-op when no arrow / track press is active.
    void  Tick (int64_t nowMs);

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
    LPCWSTR             GetCursorForPoint (POINT clientPx) const       override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    bool                QueryCommand   (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                InvokeCommand  (DxuiStandardCommand command) override;
    void                OnFocusChanged (bool focused) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::ListView; }

private:
    static constexpr int    s_kRowHeightDip          = 30;
    static constexpr int    s_kHeaderHeightDip       = 32;
    static constexpr int    s_kHeaderGapDip          = 2;

    //  Explorer's Details, measured at 150%.
    static constexpr int    s_kExplorerHeaderDip     = 31;
    static constexpr int    s_kExplorerHeaderGapDip  = 1;
    static constexpr float  s_kExplorerIconPadDip    = 4.67f;    // the name's icon, into its column
    static constexpr float  s_kExplorerCellPadDip    = 6.67f;    // every other column's text
    static constexpr float  s_kExplorerNameTitleDip  = 16.67f;   // the name column's title
    static constexpr float  s_kExplorerIconGapDip    = 4.0f;     // from the icon to the name
    static constexpr float  s_kExplorerTitleBandDip  = 27.33f;   // the titles and dividers, from the bar's top
    static constexpr float  s_kExplorerDividerAlpha  = 0.32f;    // the dividers: the text color at this strength

    int   GetHeaderBarPx      () const { return m_scaler.ToPx (m_explorerDetails ? s_kExplorerHeaderDip : s_kHeaderHeightDip); }
    int   GetHeaderGapPx      () const { return m_scaler.ToPx (m_explorerDetails ? s_kExplorerHeaderGapDip : s_kHeaderGapDip); }
    int   GetCellPadLeftPx    (size_t column) const;
    int   GetHeaderTitlePadPx (size_t column) const;
    int   GetCellIconGapPx    () const { return m_explorerDetails ? (int) std::lround (m_scaler.ToPxf (s_kExplorerIconGapDip)) : m_scaler.ToPx (s_kCellIconGapDip); }
    int   GetDetailsLeftPx    () const { return m_explorerDetails ? (int) std::lround (m_scaler.ToPxf (s_kItemsLeftDip)) : 0; }
    static constexpr int    s_kHeaderDragDip         = 5;   // how far a header moves before it is a drag
    static constexpr int    s_kCellPadLeftDip        = 12;
    static constexpr int    s_kGroupChevronCenterDip = 12;   // a group header's chevron, from the list's left, as Explorer's
    static constexpr float  s_kGroupChevronArmDip    = 3.5f;   // half its height
    static constexpr int    s_kGroupLabelDip         = 22;   // the header's label, from the list's left
    static constexpr int    s_kListGroupLabelDip     = 8;    // List's, which has no chevron before it
    //  Explorer's Content, measured at 100, 125 and 150% over a range of
    //  widths: the icon 33.33 dip into the row and the name 8 past it. The
    //  second column, from the list's left, is 60% of the way to the
    //  scrollbar's column and 32.67 dip more, but never more than 303.33 dip
    //  short of that column nor past 685 dip. Rows are 52.67 dip apart, and
    //  the rule between them 4 dip in from the left and 6.67 short of the
    //  right.
    static constexpr float  s_kContentIconLeftDip    = 33.33f;
    static constexpr float  s_kContentNameGapDip     = 8.0f;
    static constexpr float  s_kContentRightShare     = 0.6f;
    static constexpr float  s_kContentRightNudgeDip  = 32.67f;
    static constexpr float  s_kContentRightRoomDip   = 303.33f;
    static constexpr float  s_kContentRightMaxDip    = 685.0f;
    static constexpr float  s_kContentEdgeDip        = 9.33f;   // from the row's end to where those widths are measured
    static constexpr float  s_kContentRowDip         = 52.67f;
    static constexpr float  s_kContentRowLowDpiDip   = 48.0f;   // the row below 150%
    static constexpr float  s_kContentRuleInsetDip   = 4.0f;
    static constexpr float  s_kContentRuleEndDip     = 6.67f;
    static constexpr float  s_kContentEndGapDip      = 2.0f;
    static constexpr int    s_kCellPadRightDip       = 16;
    static constexpr int    s_kSortGlyphWidthDip     = 10;
    static constexpr int    s_kScrollbarWidthDip     = 10;
    static constexpr int    s_kCellIconDip           = 16;
    static constexpr int    s_kCellIconGapDip        = 6;
    static constexpr int    s_kMinColWidthDip        = 48;
    static constexpr int    s_kResizeGrabDip         = 4;

    //  A pause this long between characters starts a new search.
    static constexpr int64_t  s_kTypeAheadResetMs = 1000;
    static constexpr int      s_kHScrollStepDip   = 32;
    static constexpr int      s_kKbResizeStepDip  = 8;
    static constexpr int      s_kMinThumbPx       = 16;
    static constexpr float    s_kFontDip          = 13.0f;
    static constexpr float    s_kHeaderFontDip    = 13.0f;

    // Scrollbar auto-repeat cadence (ms), mirroring typical key-repeat:
    // a longer delay before the first repeat, then a steady interval.
    static constexpr int64_t  s_kScrollRepeatDelayMs    = 400;
    static constexpr int64_t  s_kScrollRepeatIntervalMs = 60;

    // Which held scrollbar element is auto-repeating (None when idle).
    enum class ScrollRepeat
    {
        None,
        VertArrowUp,
        VertArrowDown,
        VertTrack,
        HorzArrowLeft,
        HorzArrowRight,
        HorzTrack,
    };

    // Per-character width estimate for content auto-fit, as a fraction
    // of the font em. Segoe UI averages well under this for the digit /
    // punctuation / short-label columns the debug panels auto-fit;
    // erring high trades a little extra column width for a guarantee
    // that values never clip.
    static constexpr float  s_kAutoCharWidthEm   = 0.62f;

    // Per-element ARGB colors derived once per paint from the theme.
    struct Palette
    {
        uint32_t  fg       = 0;
        uint32_t  fgDim    = 0;
        uint32_t  hdrFg    = 0;
        uint32_t  bgRow    = 0;
        uint32_t  bgHover  = 0;
        uint32_t  bgSel    = 0;
        uint32_t  edgeSel  = 0;   // outline on the focused selected row; zero draws none
        uint32_t  bgHeader = 0;
        uint32_t  border   = 0;
        uint32_t  matchBg  = 0;
    };

    // Resolved scrollbar state for the current rect, columns, and rows.
    // vBar / hBar account for one another (the horizontal bar steals row
    // capacity; the vertical bar steals viewport width), so they are
    // resolved together. viewportW is the width available to columns
    // (full width minus the vertical bar); contentW is the natural total
    // column width (no stretch fill).
    struct ScrollLayout
    {
        bool  vBar      = false;
        bool  hBar      = false;
        int   rowCap    = 0;
        int   viewportW = 0;
        int   contentW  = 0;
        int   partialPx = 0;   // the part of a row the body shows past its last whole one
    };

    //  How far the rows are drawn above their places, and whether they can be.
    int   GetRowShiftPx () const;
    bool  CanLiftRows   () const;

    //  Rows the body shows at least part of: the whole ones, and a cut one.
    int   GetShownRowCount () const;

    //  Lines are what Details view stacks and scrolls by: each row, and a
    //  header above each group. Without groups a line is a row.
    bool  HasGroupLines  () const { return !m_groups.empty() && !IsItemsView(); }
    void  BuildLines     () const;
    int   GetLineOfGroup (int group) const;
    void  EnsureLineVisible (int line);
    bool  HitTestGroupChevron (int xPx, int group) const;
    bool  HandleKeyboardGroupedNav (WPARAM vk, bool shift, bool ctrl = false);
    bool  HandleKeyboardDetailsJump (WPARAM vk, bool shift, bool ctrl);

    //  Page Up and Page Down, as Explorer's.
    bool         HandleKeyboardPage     (WPARAM vk, bool shift, bool ctrl);
    void         ApplyKeyboardMove      (int row, bool shift, bool ctrl);
    static bool  IsAtPageEdge           (int cur, int edge, bool down) { return cur >= 0 && (down ? cur >= edge : cur <= edge); }
    int          GetPageLines           () const;
    int          GetGroupOfRow          (int row) const;
    bool         TryGetWholeLineSpan    (int & first, int & last) const;
    bool         TryGetWholeRowLineSpan (int & first, int & last) const;
    bool         IsLineWhollyShown      (int line) const;
    int          StepRowLines           (int line, int steps) const;
    int          GetEdgeRowLine         (bool last) const;
    void         ScrollLineToBottom     (int line);
    void         ScrollLineToTop        (int line);

    //  Space on the focused item, as in Explorer: Ctrl toggles it, alone it
    //  becomes the selection; on a focused header, the group's rows do.
    bool  SelectFocused (bool ctrl);
    int   GetLineCount   () const;
    int   GetLineOfRow   (int row) const;
    int   GetRowOfLine   (int line) const;     // -1 on a header line
    int   GetGroupOfLine (int line) const;     // the group whose header is on the line, or -1
    int   HitTestLine    (int xPx, int yPx) const;

    void  PaintGroupHeader (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal, int group, float x, float rowX, float ry, float layoutW) const;

    //  A group opening or closing slides its rows out from under its header,
    //  or back under it, and everything below moves with them, as Explorer's
    //  do: an ease in and out over a quarter second.
    struct GroupSlide
    {
        int      group      = -1;
        int64_t  startMs    = 0;
    };

    //  Where Details draws a line: a row, or a group's header. A slid row is
    //  one of the sliding group's, drawn inside the slide's clip.
    struct LineSpot
    {
        int    row   = -1;
        int    group = -1;
        float  y     = 0.0f;
        bool   slid  = false;
    };

    static constexpr int64_t  s_kGroupSlideMs = 250;

    //  A page sliding into view from where the view was drawn; see BeginPageSlide.
    struct PageSlide
    {
        bool     active   = false;
        int      fromPx   = 0;    // from the list's top, or from fromLine's top
        int      fromLine = -1;   // an item view's line the start is kept against, or -1
        int64_t  startMs  = 0;
    };

    static constexpr int64_t  s_kPageSlideMs = 175;

    void                    BeginPageSlide     (const PageSlide & running, int fromPx, int restBeforePx);
    int                     GetPageSlidePx     () const;
    bool                    UsesPageSlide      () const;
    int                     GetRestScrollPx    () const;
    int                     GetShownScrollPx   () const { return GetRestScrollPx() - GetPageSlidePx(); }
    int                     GetScrollViewPx    () const;

    static float            EaseGroupSlide     (float t);
    float                   GetGroupSlideShown () const;
    void                    GetGroupRowSpan    (int group, int & start, int & end) const;
    std::vector<LineSpot>   PlaceDataLines     (float top, int firstLine, int lastLine, float & clipTop, float & clipH) const;


    static constexpr uint32_t  s_kBrokenBadgeArgb   = 0xFFD13438;   // Windows' error red

    // Fill `out` with row `r`'s cells: from the provider in virtual mode, or
    // a copy of m_rows[r] otherwise. Used by Paint's visible-window pull.
    void         ProvideRow          (int r, std::vector<Cell> & out) const;

    //  The item views' layout in pixels: cell size, items to a line, lines in
    //  all, and lines in sight.
    struct ItemGrid
    {
        int  cellW   = 0;   // the pitch from one item to the next along a line
        int  boxW    = 0;   // the item itself, narrower when a row is spread
        int  cellH   = 0;
        int  perLine = 1;
        int  lines   = 0;
        int  visible = 1;
    };

    static constexpr int  s_kItemPadDip  = 6;
    static constexpr int  s_kItemLineDip = 18;

    bool          IsItemsView             () const { return m_view != View::Details; }
    ItemGrid      GetItemGrid             () const;
    ScrollLayout  ComputeItemScrollLayout () const;
    void          SpreadItemBoxes         (ItemGrid & grid, int fullW, int fullH, int rows) const;
    int           HitTestItem             (int xPx, int yPx) const;
    void          EnsureItemVisible       (int item);
    bool          HandleKeyboardItemNav   (WPARAM vk, bool shift, bool ctrl = false);
    int           GetItemIconLeftPx       () const;
    RECT          GetItemLabelRectPx      (const RECT & cell) const;
    void          PaintItems              (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal, float x, float y) const;
    void          PaintMeter              (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const;
    POINT         GetItemScrollOffsetPx   () const;

    //  Grouped item views: each line a group's header or a row of its items,
    //  scrolled by lines, as Details is.
    struct ItemLine
    {
        int  group  = -1;   // the header's group; -1 for a row of items
        int  first  = 0;    // the row's first item
        int  count  = 0;    // items on the row
        int  top    = 0;    // in the layout's own pixels
        int  height = 0;
    };

    struct ItemLayout
    {
        std::vector<ItemLine>  lines;
        std::vector<int>       rowLine;   // each item's line; -1 under a collapsed header
        int                    totalH  = 0;
    };

    //  A grouped item view mid-slide paints from a layout with the sliding
    //  group open: the lines past its header lifted by `lift`, every line
    //  moved by `shift` to the scroll of the layout the list keeps, and the
    //  group's items clipped to [clipTop, clipTop + clipH).
    struct ItemSlide
    {
        ItemLayout  layout;
        bool        active  = false;
        int         header  = -1;
        int         lift    = 0;
        int         shift   = 0;
        int         clipTop = 0;
        int         clipH   = 0;
        int         start   = 0;
        int         end     = 0;
    };

    ItemSlide     PlaceItemSlide          () const;
    void          CollectItemsToMeasure   (std::vector<std::pair<int, RECT>> & out) const;
    void          CollectLineItemsInSight (const ItemLayout & layout, const ItemLine & line, const ItemSlide & slide, int pageY,
                                           std::vector<std::pair<int, RECT>> & out) const;
    bool          HasItemGroups           () const;
    bool          UsesItemLayout          () const;
    bool          IsFirstOfGroup          (int item) const;
    const ItemLayout &  GetItemLayout     () const;
    ItemLayout    BuildItemLayout         (int openGroup = -1) const;
    static int    FindItemLine            (const ItemLayout & layout, int y);
    int           GetItemTopPx            (const ItemLayout & layout) const;
    bool          GetGroupedItemRectPx    (const ItemLayout & layout, int item, RECT & outRect) const;
    int           GetMaxItemTopLine       (const ItemLayout & layout) const;
    void          EnsureItemLineVisible   (const ItemLayout & layout, int line);
    bool          HandleKeyboardGroupedItemNav (WPARAM vk, bool shift, bool ctrl);

    //  Page Up, Page Down, Home and End over the lines of items.
    static bool   IsItemLine              (const ItemLine & line) { return line.group < 0 && line.count > 0; }
    static int    GetLastItemLine         (const ItemLayout & layout);
    static int    StepItemLines           (const ItemLayout & layout, int line, int steps);
    bool          TryGetWholeItemLineSpan (const ItemLayout & layout, int & first, int & last) const;
    bool          IsItemLineWhollyShown   (const ItemLayout & layout, int line) const;
    void          AnchorItemLine          (int line, bool bottom);
    void          ScrollItemLineToBottom  (int line);
    void          ScrollItemLineToTop     (int line);
    bool          HandleKeyboardItemPage  (const ItemLayout & layout, WPARAM vk, int line, int slot, bool shift, bool ctrl);
    bool          HandleKeyboardItemJump  (const ItemLayout & layout, WPARAM vk, bool shift, bool ctrl);

    //  The big icon views wrap a name to as many as four lines, as Explorer's
    //  do, and each row of items grows to fit its tallest name. A name is
    //  measured the first time it is drawn; until then it takes the two lines
    //  every cell has room for.
    static constexpr int    s_kItemNameMaxLines    = 4;
    static constexpr float  s_kContentNameScale    = 11.0f / 9.0f;
    static constexpr float  s_kMediumLabelWDip     = 60.67f;
    static constexpr float  s_kLargeLabelWDip      = 92.0f;
    static constexpr int    s_kXLargeLabelInsetPx  = 1;   // Extra large's names, as wide as its icon less this, at any scale
    static constexpr int    s_kLargeLabelInsetDip  = 18;

    struct ItemTextKey
    {
        int  view         = -1;
        int  cellW        = 0;
        int  rows         = 0;
        int  rowsVersion  = 0;
        int  fontCentiDip = 0;

        bool operator== (const ItemTextKey &) const = default;
    };

    void          SyncItemTextLines       () const;

    //  Small icons and List size their columns to the names, as Explorer's
    //  do: List each column to its own widest name, Small icons every column
    //  to the widest of all, up to a cap past which a name is cut short. The
    //  box around them Explorer's, measured at 150%: the icon well into the
    //  box, the name just past it, a little room after the name, and a gap
    //  between one box and the next.
    static constexpr float  s_kNameIconLeftDip   = 32.67f;
    static constexpr float  s_kNameTextGapDip    = 2.0f;
    static constexpr float  s_kNameRightPadDip   = 5.33f;
    static constexpr float  s_kNameColumnGapDip  = 11.33f;
    static constexpr float  s_kSmallIconsGapDip  = 1.0f;   // between Small icons' boxes once a name reaches the cap

    //  Explorer's tiles, measured at 100, 125 and 150%: a box 250 dip wide on
    //  the pitch, the icon 4 dip into it and the text 5.33 past the icon. The
    //  text is the name on one or two lines, then the details while the three
    //  lines last, every line one pixel more than the captions' pitch. A row
    //  is its icon or its text, whichever is taller, and 14 dip less 6 pixels
    //  more, which no single unit gives at all three scales; the box is 6 dip
    //  short of the row.
    static constexpr float  s_kTileGapDip        = 4.0f;
    static constexpr float  s_kTileIconLeftDip   = 4.0f;
    static constexpr float  s_kTileTextGapDip    = 5.33f;
    static constexpr float  s_kTilePadDip        = 14.0f;
    static constexpr int    s_kTilePadTrimPx     = 6;
    static constexpr float  s_kTileBoxShortDip   = 6.0f;
    static constexpr int    s_kTileMaxLines      = 3;
    int           GetSmallRowPx           () const;
    int           GetItemLinesHPx         (const ItemGrid & grid, int rows) const;
    static int    GetFirstItemLine        (const ItemLayout & layout);
    static void   PaintSmallArtFrame      (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg);
    static uint32_t ShiftGray             (uint32_t argb, int delta);
    int           GetTileLinePx           () const;
    int           GetTileRowPx            (int lines) const;
    static constexpr int  s_kSmallIconsMaxWDip = 309;

    void          MeasureItemNames        (IDxuiTextRenderer & text) const;
    int           GetNameCellWPx          (int textPx) const;
    int           GetNameTextGapPx        () const { return (int) std::lround (m_scaler.ToPxf (s_kNameTextGapDip)); }
    const std::vector<int> &  GetListColumnLefts () const;
    int           FindListColumn          (int x) const;
    bool          MeasureItemTextLines    (IDxuiTextRenderer & text, const std::vector<std::pair<int, RECT>> & onScreen) const;
    int           GetItemExtraPx          (int item) const;
    int           GetCaptionLinePx        () const;

    //  Grouped List: a block of columns per group, side by side.
    struct ListBlock
    {
        int  group = -1;
        int  first = 0;
        int  count = 0;    // items shown; none under a collapsed header
        int  left  = 0;    // in the content's own pixels
        int  width = 0;
    };

    struct ListLayout
    {
        std::vector<ListBlock>  blocks;
        std::vector<int>        blockOf;   // each item's block; -1 under a collapsed header
        int                     headerH = 0;
        int                     indent  = 0;
        int                     perCol  = 1;
        int                     totalW  = 0;
    };

    bool          HasListGroups           () const;
    const ListLayout &  GetListLayout     () const;
    ListLayout    BuildListLayout         () const;
    bool          GetListItemRectPx       (const ListLayout & layout, int item, RECT & outRect) const;
    static int    FindListBlock           (const ListLayout & layout, int x);
    int           HitTestListItem         (const ListLayout & layout, int xPx, int yPx) const;
    bool          HandleKeyboardListGroupNav (WPARAM vk, bool shift, bool ctrl);

    //  A column of List, grouped or not, as its page keys move across them.
    struct ListColumn
    {
        int  left  = 0;   // the item box, in the content's own pixels
        int  right = 0;
        int  first = 0;   // the items it holds
        int  count = 0;
    };

    std::vector<ListColumn>  GetListColumns            () const;
    static int               FindColumnOfItem          (const std::vector<ListColumn> & cols, int item);
    bool                     TryGetWholeListColumnSpan (const std::vector<ListColumn> & cols, int & first, int & last) const;
    bool                     IsListColumnWhollyShown   (const ListColumn & col) const;
    int                      GetListPageStartColumn    (const std::vector<ListColumn> & cols, bool down) const;
    bool                     HandleKeyboardListJump    (WPARAM vk, bool shift, bool ctrl);
    bool                     HandleKeyboardListPage    (const std::vector<ListColumn> & cols, bool down, bool shift, bool ctrl);

    //  What a group layout is built from. The layouts are kept until one of
    //  these changes, so a hit test or a paint reads them rather than
    //  rebuilding them, which walks every item.
    struct LayoutKey
    {
        int                               cellW   = 0;
        int                               cellH   = 0;
        int                               perLine = 0;
        int                               fullH   = 0;
        int                               barW    = 0;
        int                               headerH = 0;
        int                               indent  = 0;
        int                               rows    = 0;
        int                               names   = 0;   // the item name measurements' version
        int                               caption = 0;   // the caption line pitch
        std::vector<std::pair<int, bool>> groups;    // each group's first row and whether it is collapsed

        bool operator== (const LayoutKey &) const = default;
    };

    LayoutKey     GetLayoutKey            () const;

    static inline const ItemLayout  s_kNoItemLayout {};
    static inline const ListLayout  s_kNoListLayout {};

    void          BeginSelectionBand      (int lx, int ly, bool ctrl);
    void          UpdateSelectionBand     (int lx, int ly);
    // Grow the monotonic auto-fit glyph counts from one row's cells (the
    // per-row half of UpdateAutoFitFromRows, used for the visible window in
    // virtual mode where m_rows is empty).
    void         NoteAutoFitRow      (const std::vector<Cell> & cells) const;
    // The cell vector for row `r`: the provider scratch in virtual mode
    // (pulled + auto-fit noted as a side effect), m_rows[r] otherwise.
    const std::vector<Cell> & GetRowCells (int r) const;
    // Clamp m_topRow / sticky-tail after the row count changes (shared by
    // SetRows / AppendRows / SetVirtualRowCount / SetRowProvider).
    void         ClampTopAfterCountChange (bool wasSticky);

    Palette      MakePalette         () const;
    ScrollLayout ComputeScrollLayout () const;
    int          GetColumnNaturalWidthPx (size_t c) const;

    //  What a column's content wants, ignoring any override already on it, so
    //  fitting a column that has been dragged still measures the content.
    int          GetColumnContentWidthPx (IDxuiTextRenderer & text, size_t c) const;

    //  Applies a width a fit asked for, once the paint pass has measured it.
    void         ApplyPendingFit         (IDxuiTextRenderer & text);

    //  Selects the next row whose first column starts with what has been
    //  typed, as Explorer's list does. True when the character was taken.
    bool         HandleTypeAhead         (wchar_t ch);
    void    ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const;

    // Mouse-event dispatch helpers (lx / ly are widget-relative px).
    bool    DispatchMouseDown      (const DxuiMouseEvent & ev, int lx, int ly, bool inside);
    bool    DispatchScrollbarPress (int lx, int ly);
    bool    DispatchMouseMove      (int lx, int ly, bool inside);
    void    DragSelectTo           (int ly);
    bool    DispatchMouseUp        (int lx, int ly, bool inside);
    bool    DispatchMouseWheel     (const DxuiMouseEvent & ev, bool inside);

    void    EndHeaderPress          (int lx, int ly);
    void    PaintHeader             (IDxuiPainter           & painter,
                                     IDxuiTextRenderer    & text,
                                     const Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     float                    layoutW,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;
    void    PaintHeaderFocusMarkers (IDxuiPainter           & painter,
                                     const Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;
    void    PaintDataRows           (IDxuiPainter           & painter,
                                     IDxuiTextRenderer    & text,
                                     const Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     float                    layoutW,
                                     int                      firstRow,
                                     int                      lastRow,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;
    void    PaintScrollbar          (IDxuiPainter  & painter,
                                     const Palette & pal,
                                     float           x,
                                     float           y) const;
    void    PaintHScrollbar         (IDxuiPainter  & painter,
                                     const Palette & pal,
                                     float           x,
                                     float           y) const;
    void    SyncVertScroll          () const;
    void    SyncHorzScroll          () const;
    void    ApplyKeyboardColumnFocus ();
    // The sub-focus and the paint markers move as a unit: a marker left
    // behind paints a focus cue on a control that no longer has focus.
    void    ClearColumnFocusMarkers  ();
    void    ReleaseKeyboardColumnFocus ();
    bool    HandleKeyboardColumnKey  (WPARAM vk);
    bool    HandleKeyboardBodyRowNav (WPARAM vk, bool shift = false, bool ctrl = false);
    bool    HandleKeyboardRowNav     (WPARAM vk, bool shift, bool ctrl);

    // Sets the selection to the rows from the anchor to `row`, inclusive.
    void    SelectRangeFromAnchor    (int row);

    // Drops selected rows past the end after the row count changes.
    void    PruneSelection           ();
    bool    OnKeyColumnResizeNav     (const DxuiKeyEvent & ev);
    bool    OnKeyBodyHeaderNav       (const DxuiKeyEvent & ev);
    void    ApplyBodyHeaderFocus     ();
    void    MoveHeaderFocus          (int dir);
    const IDxuiTheme                * m_theme     = nullptr;
    std::vector<Column>               m_columns;
    std::vector<std::vector<Cell>>    m_rows;
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
    mutable std::vector<int>  m_measuredWPx;
    std::vector<int>          m_overrideWPx;
    // Monotonic max glyph count per auto column (header + widest cell);
    // the cheap fallback used when no DWrite measurement exists (e.g. the
    // debug panels). ComputeColumnLayout turns it into a pixel width at the
    // current DPI; persists across SetRows.
    // Mutable: grown from the visible window during const Paint in virtual
    // mode (mirrors m_measuredWPx, which is likewise refreshed from Paint).
    mutable std::vector<int>  m_autoMaxChars;
    bool                      m_preciseAutoFit = false;
    mutable bool              m_measureDirty   = false;
    DxuiDpiScaler             m_scaler;
    // Virtual (provider) row model — see SetRowProvider. When m_virtual is
    // true, m_rows is empty and rows are pulled on demand into m_providerScratch.
    bool                       m_virtual           = false;
    RowSource                  m_rowSource;
    int                        m_virtualCount      = 0;
    RowProvider                m_rowProvider;
    mutable std::vector<Cell>  m_providerScratch;
    int                        m_hovered           = -1;
    int                        m_dropRow           = -1;
    int                        m_iconColumn        = -1;
    bool                       m_seedRowOnFocus    = true;
    int                        m_selectedRow       = -1;
    bool                       m_multiSelect       = false;
    int                        m_anchorRow         = -1;
    std::vector<int>           m_selectedRows;
    int                        m_sortColumn        = -1;
    bool                       m_sortDescending    = false;
    bool                       m_showHeader        = false;
    View                       m_view              = View::Details;
    bool                       m_detailsHeader     = false;   // the header Details had, while another view shows
    int                        m_topRow            = 0;
    bool                       m_flushBottom       = false;   // Details: the rows lifted so the last whole one ends at the bottom
    int                        m_itemAnchorLine    = -1;      // an item view's scroll held at this line's edge; -1 for m_topRow's
    bool                       m_itemAnchorBottom  = false;   // its bottom edge rather than its top
    bool                       m_pageSlideEnabled  = false;
    bool                       m_stickyTail        = false;

    // Whether the host opted into sticky-tail behavior. Off by default: a
    // list opens at its top, and following the last row is for live logs.
    // m_stickyTail is re-derived on every resize and row change, and this
    // gate keeps that re-derivation from switching it on for a list that
    // never asked for it.
    bool                          m_stickyTailEnabled = false;
    bool                          m_listFocused       = false;
    std::vector<Group>            m_groups;
    int                           m_focusGroup        = -1;
    mutable std::vector<int>      m_nameCut;   // the items drawn cut short in the last paint
    mutable ItemLayout            m_itemLayout;   // see LayoutKey
    mutable LayoutKey             m_itemLayoutKey;
    mutable bool                  m_itemLayoutBuilt   = false;
    mutable ListLayout            m_listLayout;
    mutable LayoutKey             m_listLayoutKey;
    mutable bool                  m_listLayoutBuilt   = false;
    mutable std::vector<uint8_t>  m_itemTextLines;   // each item's name lines; 0 until measured
    mutable ItemTextKey           m_itemTextKey;   // what the measurements were taken for
    mutable int                   m_itemTextVersion   = 0;
    mutable std::array<int, 5>    m_linesHKey         = { -1, -1, -1, -1, -1 };   // perLine, cellH, rows, rows version, text version
    mutable int                   m_linesHPx          = 0;
    mutable int                   m_captionLinePx     = 0;   // GDI's line height for captions; 0 until a paint measures it
    RowNameSource                 m_rowNames;
    mutable std::vector<int>      m_itemNameWPx;   // each item's name width; empty until measured
    mutable int                   m_itemNameWidestPx  = 0;
    mutable ItemTextKey           m_itemNameKey;   // what the widths were measured for
    mutable std::vector<int>      m_listColumnLefts;   // List's column edges, one past the last
    mutable std::pair<int, int>   m_listColumnsKey    = { -1, -1 };   // the items per column and name widths they were built for
    int                           m_rowsVersion       = 0;   // counts row sets, so measurements of the last one are dropped
    bool                          m_pressModified     = false;   // the row press held Ctrl or Shift
    int                           m_hoverGroup        = -1;
    int                           m_lastHeaderGroup   = -1;
    int64_t                       m_lastHeaderMs      = 0;

    //  Each line's row, or -(group + 1) for a header; and each row's line, or
    //  -1 under a collapsed header. Rebuilt when the groups or the row count
    //  change.
    mutable std::vector<int>   m_lines;
    mutable std::vector<int>   m_rowLines;
    GroupSlide                 m_groupSlide;
    PageSlide                  m_pageSlide;
    int                        m_keyMoveRow        = -1;      // a key's move, reported once the key is handled
    bool                       m_keyMovePending    = false;
    mutable bool               m_linesDirty        = true;
    mutable int                m_linesRowCount     = -1;    int                        m_focusedHeaderCol  = -1;
    int                        m_focusedDividerCol = -1;
    bool                       m_kbColNavEnabled   = false;
    bool                       m_kbColResize       = false;
    int                        m_kbColFocus        = -1;
    bool                       m_vertDragging      = false;
    float                      m_vertDragGrab      = 0.0f;
    bool                       m_hScrollEnabled    = false;
    int                        m_leftPx            = 0;
    // High-resolution wheel accumulators. Touchpads emit many sub-notch
    // deltas; we bank them and act only when a whole unit is due, so fine
    // scrolls move proportionally instead of snapping a full line/step per
    // event. m_wheelAccumV is in raw WHEEL_DELTA units (acted per notch);
    // m_wheelAccumH is in pixels (acted per whole pixel, for smooth H-scroll).
    int                    m_wheelAccumV        = 0;
    float                  m_wheelAccumH        = 0.0f;
    bool                   m_horzDragging       = false;
    float                  m_horzDragGrab       = 0.0f;
    int                    m_resizeColumn       = -1;
    int                    m_resizeStartXPx     = 0;
    int                    m_resizeStartWPx     = 0;
    ScrollRepeat           m_scrollRepeat       = ScrollRepeat::None;
    int                    m_scrollRepeatXPx    = 0;
    int                    m_scrollRepeatYPx    = 0;
    int64_t                m_scrollRepeatNextMs = 0;
    mutable DxuiScrollbar  m_vertScroll;
    mutable DxuiScrollbar  m_horzScroll;
    std::function<void (int)>         m_onSelectionChanged;
    std::function<void (int)>         m_onActivateRow;
    std::function<void (int)>         m_onSortColumn;
    std::function<void (int, int)>    m_onColumnResized;
    std::function<void (const std::vector<size_t> &)>  m_onColumnsReordered;

    //  A header press: sorts on release in place, or moves its column once
    //  the pointer has travelled far enough to be a drag.
    std::vector<size_t>  m_columnOrder;
    int                  m_headerPressCol     = -1;
    int                  m_headerPressXPx     = 0;
    int                  m_headerDragXPx      = 0;
    bool                 m_headerDragging     = false;

    //  While a header is dragged, the others slide aside, as Explorer's do,
    //  to open a gap where it would land: each column eases from where it
    //  was to its new offset over a few frames.
    struct HeaderSlide
    {
        float    fromPx  = 0.0f;
        float    toPx    = 0.0f;
        int64_t  startMs = 0;
    };

    static constexpr int64_t  s_kHeaderSlideMs = 150;

    std::vector<HeaderSlide>  m_headerSlides;
    int                       m_headerDropPos     = -1;

    void   UpdateHeaderSlides  ();
    float  GetHeaderSlidePx    (size_t column, int64_t nowMs) const;
    int64_t  GetClockMs        () const;

    bool     m_activateOnDoubleClick = false;
    bool     m_alwaysShowSelection   = false;
    bool     m_textSelectionColors   = false;
    bool     m_dragSelecting         = false;

    //  A rubber band in an item view: where the press was, in content pixels
    //  so it stays put as the view scrolls, where the pointer is now, and the
    //  selection a Ctrl press began with, which the band adds to.
    bool                 m_bandActive     = false;
    POINT                m_bandStart      = {};
    POINT                m_bandEnd        = {};
    std::vector<int>     m_bandBase;
    int                  m_lastClickRow   = -1;
    int64_t              m_lastClickMs    = 0;
    int                  m_lastDividerCol = -1;
    int64_t              m_lastDividerMs  = 0;
    std::vector<size_t>  m_pendingFits;

    //  Characters typed toward a row, and when the last one arrived.
    std::wstring  m_typeAhead;
    int64_t       m_typeAheadMs      = 0;
    ClockFn       m_clock;
};
