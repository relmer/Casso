#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
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
        std::wstring                  title;
        int                           widthDip = 0;     // 0 = auto-fit content (or stretch if stretch=true)
        bool                          stretch = false; // when true, column absorbs any remaining width after fixed/auto
        DxuiTextHAlign    align   = DxuiTextHAlign::Left;
        bool                          visible = true;
    };

    struct Cell
    {
        std::wstring  text;
        bool          dim = false;     // muted color (e.g. "(Download)" hint)
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
    void  SetDpi          (UINT dpi)                       { m_scaler.SetDpi (dpi); }
    void  SetTheme        (const IDxuiTheme * theme)       { m_theme = theme; }
    void  SetShowHeader   (bool b)                         { m_showHeader = b; }
    void  SetHoveredRow   (int row)                        { m_hovered = row; }
    void  SetSortIndicator (int column, bool descending)   { m_sortColumn = column; m_sortDescending = descending; }
    void  SetRect         (const RECT & rect);
    void  SetColumns      (std::vector<Column> cols);
    void  SetRows         (std::vector<std::vector<Cell>> rows);
    void  AppendRows      (std::vector<std::vector<Cell>> rows);

    // Column visibility & widths.
    void  SetColumnVisible          (size_t idx, bool visible);
    bool  IsColumnVisible           (size_t idx) const     { return (idx < m_columns.size()) && m_columns[idx].visible; }
    void  SetColumnOverrideWidthPx  (size_t idx, int px);
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

    // Column / row queries.
    size_t         GetColumnCount () const                 { return m_columns.size(); }
    const Column & GetColumnAt    (size_t idx) const       { return m_columns[idx]; }

    int   GetRowCount              () const                 { return (int) m_rows.size(); }
    int   GetHoveredRow            () const                 { return m_hovered; }
    bool  IsHeaderShown            () const                 { return m_showHeader; }
    int   GetHeaderHeightPx        () const                 { return m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0; }
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

    // Vertical scroll. The widget keeps a top-row index into m_rows;
    // Paint clips to [m_topRow, m_topRow + capacity). "Sticky tail"
    // auto-pins the view to the bottom when new rows arrive while the
    // user is already parked at the tail.
    int   GetScrollbarWidthPx   () const                 { return m_scaler.Px (s_kScrollbarWidthDip); }
    int   GetTopRow             () const                 { return m_topRow; }
    int   GetVisibleRowCapacity () const;
    int   GetMaxTopRow          () const;
    bool  IsAtBottom            () const                 { return m_topRow >= GetMaxTopRow(); }
    void  EnableStickyTail      (bool b)                 { m_stickyTail = b; }
    bool  IsStickyTailEnabled   () const                 { return m_stickyTail; }
    void  SetTopRow             (int topRow);
    void  ScrollByRows          (int delta)              { SetTopRow (m_topRow + delta); }
    void  ScrollByWheelDelta    (int wheelDelta, int linesPerNotch = 3);

    // Scrollbar geometry & thumb-drag. xPx/yPx are relative to the
    // widget rect. The caller starts a drag with BeginThumbDrag and
    // forwards subsequent mouse-move y values via UpdateThumbDrag.
    bool             IsScrollbarVisible        () const;
    ScrollbarMetrics GetScrollbarGeometry      () const;
    bool             HitTestScrollbarThumb     (int xPx, int yPx) const;
    bool             HitTestScrollbarTrack     (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowUp   (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowDown (int xPx, int yPx) const;
    void             PageFromTrackClick        (int yPx);
    void             BeginThumbDrag            (int grabYPx);
    void             UpdateThumbDrag           (int yPx);
    void             EndThumbDrag              ()                            { m_vertDragging = false; m_vertDragGrab = 0.0f; }
    bool             IsThumbDragging           () const                      { return m_vertDragging; }

    // Horizontal scroll (opt-in via SetHorizontalScrollEnabled; default
    // off so existing consumers are unaffected). When enabled and the
    // natural total column width exceeds the viewport, Paint offsets the
    // columns by -m_leftPx, clips them to the content viewport, and
    // shows a horizontal scrollbar along the bottom. GetContentWidthPx
    // is the natural total (no stretch fill); GetMaxLeftPx is the excess
    // of that over the viewport content width (which excludes the
    // vertical scrollbar). xPx/yPx for the hit-tests are widget-relative.
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
    void              PageFromHorzTrackClick         (int xPx);
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

    // Self-contained mouse input. Forward widget-relative mouse events
    // (positionDip = the point minus the list's own origin) via OnMouse;
    // the list owns scrolling, thumb / column-resize drags, hover, and
    // selection, and reports semantic outcomes through these callbacks.
    // OnMouse returns true when it consumed the event so the host can
    // repaint and claim focus; IsInteracting is true mid-drag so a Win32
    // host knows to hold mouse capture.
    void  SetOnSelectionChanged (std::function<void (int)>  cb)  { m_onSelectionChanged = std::move (cb); }
    void  SetOnActivateRow      (std::function<void (int)>  cb)  { m_onActivateRow      = std::move (cb); }
    void  SetOnSortColumn       (std::function<void (int)>  cb)  { m_onSortColumn       = std::move (cb); }

    // Raised once when an interactive column-resize drag completes, with
    // the column index and its new effective width in physical pixels.
    // Lets a host that owns a persisted column model (e.g. the debug
    // panels) record the user's width without re-implementing the drag.
    void  SetOnColumnResized    (std::function<void (int, int)>  cb)  { m_onColumnResized = std::move (cb); }
    bool  IsInteracting         () const  { return m_vertDragging || m_horzDragging || m_resizeColumn >= 0 || m_scrollRepeat != ScrollRepeat::None; }
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
    //  callbacks above for the semantic outcomes. OnKey stays a shim
    //  (key handling remains host-driven via the focus accessors above).
    //
    DxuiListView() { m_focusable = true; }
    ~DxuiListView () override = default;

    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    LPCWSTR             CursorForPoint (POINT clientPx) const       override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override { (void) ev; return false; }
    void                OnFocusChanged (bool focused) override { SetListFocused (focused); }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::ListView; }

private:
    static constexpr int    s_kRowHeightDip      = 30;
    static constexpr int    s_kHeaderHeightDip   = 26;
    static constexpr int    s_kHeaderGapDip      = 2;
    static constexpr int    s_kCellPadLeftDip    = 12;
    static constexpr int    s_kCellPadRightDip   = 16;
    static constexpr int    s_kSortGlyphWidthDip = 10;
    static constexpr int    s_kScrollbarWidthDip = 10;
    static constexpr int    s_kMinColWidthDip    = 48;
    static constexpr int    s_kResizeGrabDip     = 4;
    static constexpr int    s_kHScrollStepDip    = 32;
    static constexpr int    s_kMinThumbPx       = 16;
    static constexpr float  s_kFontDip           = 13.0f;
    static constexpr float  s_kHeaderFontDip     = 13.0f;

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
        uint32_t  bgHeader = 0;
        uint32_t  border   = 0;
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
    };

    Palette      MakePalette         () const;
    ScrollLayout ComputeScrollLayout () const;
    int          ColumnNaturalWidthPx (size_t c) const;
    void    ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const;

    // Mouse-event dispatch helpers (lx / ly are widget-relative px).
    bool    DispatchMouseDown      (const DxuiMouseEvent & ev, int lx, int ly, bool inside);
    bool    DispatchScrollbarPress (int lx, int ly);
    bool    DispatchMouseMove      (int lx, int ly, bool inside);
    bool    DispatchMouseUp        (int lx, int ly, bool inside);
    bool    DispatchMouseWheel     (const DxuiMouseEvent & ev, bool inside);

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
    const IDxuiTheme                * m_theme      = nullptr;
    std::vector<Column>               m_columns;
    std::vector<std::vector<Cell>>    m_rows;
    // Per-column pixel width fitted to the header + widest cell via
    // MeasureColumnsPx (DWrite). Monotonic and persists across SetRows so
    // filter/sort don't collapse content-fit columns; reset by SetColumns.
    // Preferred over m_autoMaxChars wherever a non-zero entry exists.
    mutable std::vector<int>          m_measuredWPx;
    std::vector<int>                  m_overrideWPx;
    // Monotonic max glyph count per auto column (header + widest cell);
    // the cheap fallback used when no DWrite measurement exists (e.g. the
    // debug panels). ComputeColumnLayout turns it into a pixel width at the
    // current DPI; persists across SetRows.
    std::vector<int>                  m_autoMaxChars;
    DxuiDpiScaler                         m_scaler;
    int                               m_hovered           = -1;
    int                               m_selectedRow       = -1;
    int                               m_sortColumn        = -1;
    bool                              m_sortDescending    = false;
    bool                              m_showHeader        = false;
    int                               m_topRow            = 0;
    bool                              m_stickyTail        = true;
    bool                              m_listFocused       = false;
    int                               m_focusedHeaderCol  = -1;
    int                               m_focusedDividerCol = -1;
    bool                              m_vertDragging          = false;
    float                             m_vertDragGrab        = 0.0f;
    bool                              m_hScrollEnabled    = false;
    int                               m_leftPx            = 0;
    bool                              m_horzDragging         = false;
    float                             m_horzDragGrab       = 0.0f;
    int                               m_resizeColumn      = -1;
    int                               m_resizeStartXPx    = 0;
    int                               m_resizeStartWPx    = 0;
    ScrollRepeat                      m_scrollRepeat      = ScrollRepeat::None;
    int                               m_scrollRepeatXPx   = 0;
    int                               m_scrollRepeatYPx   = 0;
    int64_t                           m_scrollRepeatNextMs = 0;
    mutable DxuiScrollbar             m_vertScroll;
    mutable DxuiScrollbar             m_horzScroll;
    std::function<void (int)>         m_onSelectionChanged;
    std::function<void (int)>         m_onActivateRow;
    std::function<void (int)>         m_onSortColumn;
    std::function<void (int, int)>    m_onColumnResized;
};
