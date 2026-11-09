#include "Pch.h"

#include "Widgets/ListView/DxuiListScrollbars.h"
#include "Core/DxuiSystemSettings.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::DxuiListScrollbars
//
////////////////////////////////////////////////////////////////////////////////

DxuiListScrollbars::DxuiListScrollbars (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeScrollLayout
//
//  Resolves the mutually-dependent vertical / horizontal scrollbar
//  state for the current rect. The horizontal bar (when enabled) steals
//  one bar-thickness of row capacity from the bottom; the vertical bar
//  steals one bar-thickness of viewport width from the right. Two
//  passes settle the dependency. With horizontal scroll disabled the
//  result is identical to the legacy vertical-only math.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ScrollLayout DxuiListScrollbars::ComputeScrollLayout() const
{
    DxuiListViewTypes::ScrollLayout  layout;
    int                              fullW  = m_list.m_boundsDip.right  - m_list.m_boundsDip.left;
    int                              fullH  = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int                              barW   = m_list.GetScrollbarWidthPx();
    int                              rowH   = m_list.GetRowHeightPx();
    int                              hgTop  = m_list.m_showHeader ? (m_list.GetHeaderBarPx() + m_list.GetHeaderGapPx()) : 0;
    int                              rows   = m_list.m_grouping.GetLineCount();
    int                              pass   = 0;



    if (m_list.IsItemsView())
    {
        return m_list.m_items.ComputeItemScrollLayout();
    }

    layout.contentW  = m_list.m_columnSizing.GetContentWidthPx();
    layout.viewportW = fullW;

    for (pass = 0; pass < 2; ++pass)
    {
        int  viewportW = fullW - (layout.vBar ? barW : 0);
        int  bodyH     = 0;
        int  cap       = 0;

        layout.hBar = m_list.m_hScrollEnabled && (viewportW > 0) && (layout.contentW > viewportW);
        bodyH       = (fullH - hgTop) - (layout.hBar ? barW : 0);
        cap              = (rowH > 0 && bodyH > 0) ? (bodyH / rowH) : 0;
        layout.rowCap    = cap;
        layout.partialPx = (rowH > 0 && bodyH > 0) ? (bodyH % rowH) : 0;
        layout.vBar     = (cap > 0) && (rows > cap);
        layout.viewportW = fullW - (layout.vBar ? barW : 0);
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVisibleRowCapacity
//
//  Number of data rows that fit in the current body height (minus the
//  horizontal scrollbar's reserved strip when it is showing).
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetVisibleRowCapacity() const
{
    return ComputeScrollLayout().rowCap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxLeftPx
//
//  Largest valid horizontal offset: the excess of the natural content
//  width over the viewport content width (which excludes the vertical
//  scrollbar). Zero when the content fits.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetMaxLeftPx() const
{
    DxuiListViewTypes::ScrollLayout  layout = ComputeScrollLayout();
    int                              excess = layout.contentW - layout.viewportW;



    return (excess > 0) ? excess : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLeftPx
//
//  Sets the horizontal scroll offset, clamped to [0, GetMaxLeftPx()].
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::SetLeftPx (int leftPx)
{
    int  maxLeft = GetMaxLeftPx();



    if (leftPx < 0)
    {
        leftPx = 0;
    }

    if (leftPx > maxLeft)
    {
        leftPx = maxLeft;
    }

    m_list.m_leftPx = leftPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollByWheelDeltaHorizontal
//
//  Shift-wheel horizontal scroll: a positive wheelDelta scrolls left
//  (toward the start), mirroring the vertical wheel's up-scrolls-up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::ScrollByWheelDeltaHorizontal (int wheelDelta, int pxPerNotch)
{
    HRESULT  hr        = S_OK;
    float    pixels    = 0.0f;
    int      wholePx   = 0;



    BAIL_OUT_IF (wheelDelta == 0, S_OK);

    // Horizontal scroll is pixel-granular, so scroll pixels in direct
    // proportion to the delta (one full notch == pxPerNotch) and bank the
    // fractional remainder. This makes a touchpad's stream of small deltas
    // glide smoothly instead of snapping a whole pxPerNotch step per event.
    pixels          = (float) wheelDelta * (float) pxPerNotch / (float) WHEEL_DELTA;
    m_wheelAccumH  += pixels;
    wholePx         = (int) m_wheelAccumH;      // truncates toward zero
    BAIL_OUT_IF (wholePx == 0, S_OK);

    m_wheelAccumH  -= (float) wholePx;
    SetLeftPx (m_list.m_leftPx - wholePx);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxTopRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetMaxTopRow() const
{
    int                          cap  = GetVisibleRowCapacity();
    int                          rows = m_list.m_grouping.GetLineCount();
    DxuiListViewTypes::ItemGrid  grid;



    if (m_list.m_items.UsesItemLayout())
    {
        return m_list.m_items.GetMaxItemTopLine (m_list.m_items.GetItemLayout());
    }

    //  An item view scrolls by whole rows of items; List does not scroll down.
    if (m_list.IsItemsView())
    {
        grid = m_list.m_items.GetItemGrid();

        if (DxuiListItemLayout::GetItemMetrics (m_list.m_view).columns || grid.lines <= grid.visible)
        {
            return 0;
        }

        return (grid.lines - grid.visible) * grid.perLine;
    }

    //  A body that shows part of one more row scrolls one further, the last
    //  row then whole at the bottom and the top one cut off.
    if (rows <= cap)
    {
        return 0;
    }

    return rows - cap - ((ComputeScrollLayout().partialPx > 0) ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowShiftPx
//
//  How far Details draws its rows above their places. A body that ends
//  partway through a row lifts them only while the view holds its bottom
//  edge, after a page down or at the very end of the list, so the last row
//  in sight ends flush with the bottom and the top one is cut off. Anywhere
//  else, the top row included, the top row shows whole.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetRowShiftPx() const
{
    if (!m_list.m_flushBottom || !CanLiftRows())
    {
        return 0;
    }

    return m_list.GetRowHeightPx() - ComputeScrollLayout().partialPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanLiftRows
//
//  Whether Details' body ends partway through a row and the list has more
//  lines than the whole rows that fit, so lifting the rows can show its last
//  line whole.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::CanLiftRows() const
{
    DxuiListViewTypes::ScrollLayout  layout;



    if (m_list.IsItemsView())
    {
        return false;
    }

    layout = ComputeScrollLayout();

    return layout.partialPx > 0 && m_list.m_grouping.GetLineCount() > layout.rowCap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetShownRowCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetShownRowCount() const
{
    DxuiListViewTypes::ScrollLayout  layout = ComputeScrollLayout();



    return layout.rowCap + ((layout.partialPx > 0) ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTopRow
//
//  Every scroll but a page's comes through here, so a page's hold on the
//  bottom of Details or on an item line's edge ends here too, and so does a
//  page still sliding into view. The row asked for shows whole at the top;
//  a row past the last that can be the top asks for the very end, where
//  Details lifts its rows so the last one shows whole.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::SetTopRow (int topRow)
{
    int  maxTop = GetMaxTopRow();



    m_list.m_flushBottom    = false;
    m_list.m_items.ReleaseAnchor();
    m_list.m_items.EndPageSlide();

    if (topRow < 0)
    {
        topRow = 0;
    }

    if (topRow > maxTop)
    {
        topRow               = maxTop;
        m_list.m_flushBottom = CanLiftRows();
    }

    //  An item view's top is the first item of a row; grouped, it is a line.
    if (m_list.IsItemsView() && !m_list.m_items.UsesItemLayout())
    {
        topRow -= topRow % (std::max) (1, m_list.m_items.GetItemGrid().perLine);
    }

    m_list.m_topRow = topRow;
    m_stickyTail = m_stickyTailEnabled && (m_list.m_topRow >= maxTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveTopRow
//
//  Moves the view by whole rows to `topRow`, for a host putting the view
//  back after the rows under it change. Rows that a page down or the list's
//  end lifted stay lifted, so nothing on screen moves by part of a row. The
//  top row the view already has leaves it exactly as it is, a page's hold
//  on an item line's edge and a page still sliding included.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::MoveTopRow (int topRow)
{
    bool  lifted = m_list.m_flushBottom;



    if (topRow == m_list.m_topRow)
    {
        return;
    }

    SetTopRow (topRow);

    m_list.m_flushBottom = m_list.m_flushBottom || (lifted && CanLiftRows());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollByWheelDelta
//
//  Converts raw wheel deltas into row scrolling, banking the remainder.
//
//  Accumulating rather than scrolling per event is what makes a TOUCHPAD feel
//  right. A precision touchpad sends many sub-notch deltas; treating each as a
//  scroll event advanced a whole linesPerNotch block per message, which read
//  as a chunky jump. Banking the delta and moving ONE row per accumulated step
//  turns the same input into smooth single-row motion.
//
//  A real mouse notch is unchanged: a full WHEEL_DELTA banks linesPerNotch
//  steps at once, so it still advances exactly linesPerNotch rows.
//
//  The remainder is CARRIED rather than discarded, so a slow drag accumulates
//  to a row instead of being rounded away to nothing.
//
//  The sign is inverted because a positive wheel delta means scrolling up,
//  which is a decrease in row index.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::ScrollByWheelDelta (int wheelDelta, int linesPerNotch)
{
    HRESULT  hr      = S_OK;
    int      notches = 0;



    BAIL_OUT_IF (wheelDelta == 0, S_OK);

    // Bank the raw delta and scroll ONE row per accumulated step, carrying
    // the remainder. A full mouse notch (WHEEL_DELTA) still advances
    // linesPerNotch rows -- unchanged feel -- because a notch banks
    // linesPerNotch steps at once; but a touchpad's sub-notch deltas now
    // advance a single row at a time as they accumulate, instead of snapping
    // a whole linesPerNotch block per event (which read as a chunky jump).
    {
        int  unitPerRow = (linesPerNotch > 0) ? (WHEEL_DELTA / linesPerNotch) : WHEEL_DELTA;

        if (unitPerRow < 1)
        {
            unitPerRow = 1;
        }

        m_wheelAccumV += wheelDelta;
        notches        = m_wheelAccumV / unitPerRow;      // whole rows to move
        BAIL_OUT_IF (notches == 0, S_OK);

        m_wheelAccumV -= notches * unitPerRow;

        //  An item view moves a row of items per step, or a column in List.
        if (m_list.IsItemsView() && DxuiListItemLayout::GetItemMetrics (m_list.m_view).columns)
        {
            SetLeftPx (m_list.m_leftPx - notches * m_list.m_items.GetItemGrid().cellW);
        }
        else
        {
            m_list.ScrollByRows (-notches * ((m_list.IsItemsView() && !m_list.m_items.UsesItemLayout()) ? m_list.m_items.GetItemGrid().perLine : 1));
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsScrollbarVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::IsScrollbarVisible() const
{
    int  cap = GetVisibleRowCapacity();



    return (cap > 0) && (m_list.m_grouping.GetLineCount() > cap);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncVertScroll
//
//  Pushes the current rect, row count, capacity, and top-row position into
//  the owned vertical DxuiScrollbar so geometry / paint / drag all read
//  from the one component. Track is widget-relative; the bar sits to the
//  right of the body, between the header and any horizontal bar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::SyncVertScroll() const
{
    DxuiListViewTypes::ScrollLayout  layout  = ComputeScrollLayout();
    int                              fullW   = m_list.m_boundsDip.right - m_list.m_boundsDip.left;
    int                              barW    = m_list.GetScrollbarWidthPx();
    int                              hBarH   = layout.hBar ? barW : 0;
    int                              headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int                              hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int                              by      = headerH + hdrGap;
    int                              bh      = (m_list.m_boundsDip.bottom - m_list.m_boundsDip.top) - by - hBarH;
    DxuiScrollInfo                   info;



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, s_kMinThumbPx, 1);
    m_vertScroll.SetTrack (RECT{ fullW - barW, by, fullW, by + bh });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = m_list.m_grouping.GetLineCount();
    info.nPage = layout.rowCap;
    info.nPos  = m_list.m_topRow;
    m_vertScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScrollbarGeometry
//
//  Computes the geometry of every interactive scrollbar region in
//  coordinates relative to the widget rect's top-left. Up/down arrow
//  buttons bracket the track; the thumb travels within the track only.
//  arrowH == 0 means the bar is too short for arrow buttons and the
//  track spans the full bar height.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ScrollbarMetrics DxuiListScrollbars::GetScrollbarGeometry() const
{
    HRESULT                              hr = S_OK;
    DxuiListViewTypes::ScrollbarMetrics  m;
    DxuiScrollbar::Metrics               g;



    SyncVertScroll();
    g = m_vertScroll.GetMetrics();

    BAIL_OUT_IF (!g.visible, S_OK);

    m.visible      = true;
    m.barX         = g.bar.left;
    m.barW         = g.bar.right - g.bar.left;
    m.arrowH       = g.arrowLess.bottom - g.arrowLess.top;
    m.upArrowTop   = g.arrowLess.top;
    m.downArrowTop = g.arrowMore.top;
    m.trackTop     = g.track.top;
    m.trackH       = g.track.bottom - g.track.top;
    m.thumbTop     = g.thumbStart;
    m.thumbH       = g.thumbLength;

Error:
    return m;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarThumb
//
//  True if (xPx, yPx), relative to the widget rect, lies on the thumb.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestScrollbarThumb (int xPx, int yPx) const
{
    HRESULT                              hr     = S_OK;
    bool                                 result = false;
    DxuiListViewTypes::ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (float) yPx >= m.thumbTop && (float) yPx < m.thumbTop + m.thumbH;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarTrack
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestScrollbarTrack (int xPx, int yPx) const
{
    HRESULT                              hr     = S_OK;
    bool                                 result = false;
    DxuiListViewTypes::ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.trackTop && yPx < m.trackTop + m.trackH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarArrowUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestScrollbarArrowUp (int xPx, int yPx) const
{
    HRESULT                              hr     = S_OK;
    bool                                 result = false;
    DxuiListViewTypes::ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowH <= 0, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.upArrowTop && yPx < m.upArrowTop + m.arrowH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarArrowDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestScrollbarArrowDown (int xPx, int yPx) const
{
    HRESULT                              hr     = S_OK;
    bool                                 result = false;
    DxuiListViewTypes::ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowH <= 0, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.downArrowTop && yPx < m.downArrowTop + m.arrowH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPageFromTrackClick
//
//  Pages the view by one visible-row capacity toward a track click
//  above (page up) or below (page down) the thumb.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::GetPageFromTrackClick (int yPx)
{
    HRESULT                              hr      = S_OK;
    DxuiListViewTypes::ScrollbarMetrics  m       = GetScrollbarGeometry();
    int                                  cap     = GetVisibleRowCapacity();
    DxuiListViewTypes::PageSlide         running = m_list.m_items.GetPageSlide();
    int                                  shownPx = 0;
    int                                  restPx  = 0;



    BAIL_OUT_IF (!m.visible || cap <= 0, S_OK);

    shownPx = m_list.GetShownScrollPx();
    restPx  = m_list.m_items.GetRestScrollPx();

    if ((float) yPx < m.thumbTop)
    {
        SetTopRow (m_list.m_topRow - cap);
    }
    else
    {
        SetTopRow (m_list.m_topRow + cap);
    }

    m_list.m_items.BeginPageSlide (running, shownPx, restPx);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginThumbDrag
//
//  grabYPx is the y inside the widget where the user grabbed the thumb;
//  we remember the offset between the click and the thumb's current top
//  so the thumb doesn't jump on the first move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::BeginThumbDrag (int grabYPx)
{
    DxuiListViewTypes::ScrollbarMetrics  m = GetScrollbarGeometry();



    m_vertDragging   = true;
    m_vertDragGrab = (float) grabYPx - m.thumbTop;
    m_vertScroll.SetDragOffset (m.thumbTop - (float) m.trackTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::UpdateThumbDrag (int yPx)
{
    HRESULT                              hr       = S_OK;
    DxuiListViewTypes::ScrollbarMetrics  m        = GetScrollbarGeometry();
    int                                  maxTop   = GetMaxTopRow();
    float                                travel   = 0.0f;
    float                                thumbTop = 0.0f;
    float                                ratio    = 0.0f;



    BAIL_OUT_IF (!m_vertDragging || !m.visible, S_OK);

    //  The puck follows the pointer by the pixel; the rows follow in whole
    //  rows as the puck reaches each one. At the bottom of its travel, the
    //  view goes to the very end, past the last top row.
    travel   = (std::max) ((float) m.trackH - m.thumbH, 0.0f);
    thumbTop = std::clamp ((float) yPx - m_vertDragGrab - (float) m.trackTop, 0.0f, travel);
    ratio    = (travel > 0.0f) ? (thumbTop / travel) : 0.0f;

    m_vertScroll.SetDragOffset (thumbTop);
    SetTopRow ((travel > 0.0f && thumbTop >= travel) ? maxTop + 1 : (int) std::lround (ratio * (float) maxTop));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHorzScrollbarVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::IsHorzScrollbarVisible() const
{
    return ComputeScrollLayout().hBar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHorzScrollbarGeometry
//
//  Horizontal counterpart to GetScrollbarGeometry: the bar runs along
//  the bottom of the list, spanning the viewport content width (full
//  width minus the vertical scrollbar). Left/right arrow buttons bracket
//  the track; the thumb travels within the track only. arrowW == 0 means
//  the bar is too short for arrow buttons.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  SyncHorzScroll
//
//  Pushes the current viewport / content width and left-offset into the
//  owned horizontal DxuiScrollbar. nMax is zeroed when the bar is not
//  enabled or content fits, so the bar reports hidden in that case.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::SyncHorzScroll() const
{
    DxuiListViewTypes::ScrollLayout  layout = ComputeScrollLayout();
    int                              fullH  = m_list.m_boundsDip.bottom - m_list.m_boundsDip.top;
    int                              barH   = m_list.GetScrollbarWidthPx();
    int                              viewW  = layout.viewportW;
    DxuiScrollInfo                   info;



    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barH, s_kMinThumbPx, m_list.m_scaler.ToPx (DxuiListView::s_kHScrollStepDip));
    m_horzScroll.SetTrack (RECT{ 0, fullH - barH, viewW, fullH });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = layout.hBar ? layout.contentW : 0;
    info.nPage = viewW;
    info.nPos  = m_list.m_leftPx;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHorzScrollbarGeometry
//
//  Horizontal counterpart to GetScrollbarGeometry, derived from the owned
//  horizontal DxuiScrollbar.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::HorzScrollbarMetrics DxuiListScrollbars::GetHorzScrollbarGeometry() const
{
    HRESULT                                  hr = S_OK;
    DxuiListViewTypes::HorzScrollbarMetrics  m;
    DxuiScrollbar::Metrics                   g;



    SyncHorzScroll();
    g = m_horzScroll.GetMetrics();

    BAIL_OUT_IF (!g.visible, S_OK);

    m.visible     = true;
    m.barY        = g.bar.top;
    m.barH        = g.bar.bottom - g.bar.top;
    m.arrowW      = g.arrowLess.right - g.arrowLess.left;
    m.leftArrowX  = g.arrowLess.left;
    m.rightArrowX = g.arrowMore.left;
    m.trackLeft   = g.track.left;
    m.trackW      = g.track.right - g.track.left;
    m.thumbLeft   = g.thumbStart;
    m.thumbW      = g.thumbLength;

Error:
    return m;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHorzScrollbarThumb
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestHorzScrollbarThumb (int xPx, int yPx) const
{
    HRESULT                                  hr     = S_OK;
    bool                                     result = false;
    DxuiListViewTypes::HorzScrollbarMetrics  m      = GetHorzScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (float) xPx >= m.thumbLeft && (float) xPx < m.thumbLeft + m.thumbW;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHorzScrollbarTrack
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestHorzScrollbarTrack (int xPx, int yPx) const
{
    HRESULT                                  hr     = S_OK;
    bool                                     result = false;
    DxuiListViewTypes::HorzScrollbarMetrics  m      = GetHorzScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.trackLeft && xPx < m.trackLeft + m.trackW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHorzScrollbarArrowLeft
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestHorzScrollbarArrowLeft (int xPx, int yPx) const
{
    HRESULT                                  hr     = S_OK;
    bool                                     result = false;
    DxuiListViewTypes::HorzScrollbarMetrics  m      = GetHorzScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowW <= 0, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.leftArrowX && xPx < m.leftArrowX + m.arrowW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHorzScrollbarArrowRight
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::HitTestHorzScrollbarArrowRight (int xPx, int yPx) const
{
    HRESULT                                  hr     = S_OK;
    bool                                     result = false;
    DxuiListViewTypes::HorzScrollbarMetrics  m      = GetHorzScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowW <= 0, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.rightArrowX && xPx < m.rightArrowX + m.arrowW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPageFromHorzTrackClick
//
//  Pages the view by one viewport width toward a horizontal track click
//  left (page left) or right (page right) of the thumb.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::GetPageFromHorzTrackClick (int xPx)
{
    HRESULT                                  hr    = S_OK;
    DxuiListViewTypes::HorzScrollbarMetrics  m     = GetHorzScrollbarGeometry();
    int                                      viewW = ComputeScrollLayout().viewportW;



    BAIL_OUT_IF (!m.visible || viewW <= 0, S_OK);

    if ((float) xPx < m.thumbLeft)
    {
        SetLeftPx (m_list.m_leftPx - viewW);
    }
    else
    {
        SetLeftPx (m_list.m_leftPx + viewW);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginHorzThumbDrag
//
//  grabXPx is the x inside the widget where the user grabbed the thumb;
//  we remember the offset between the click and the thumb's current left
//  so the thumb doesn't jump on the first move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::BeginHorzThumbDrag (int grabXPx)
{
    DxuiListViewTypes::HorzScrollbarMetrics  m = GetHorzScrollbarGeometry();



    m_horzDragging   = true;
    m_horzDragGrab = (float) grabXPx - m.thumbLeft;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateHorzThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::UpdateHorzThumbDrag (int xPx)
{
    HRESULT                                  hr        = S_OK;
    DxuiListViewTypes::HorzScrollbarMetrics  m         = GetHorzScrollbarGeometry();
    int                                      maxLeft   = GetMaxLeftPx();
    float                                    travel    = 0.0f;
    float                                    thumbLeft = 0.0f;
    float                                    ratio     = 0.0f;



    BAIL_OUT_IF (!m_horzDragging || !m.visible, S_OK);

    travel    = (float) m.trackW - m.thumbW;
    thumbLeft = (float) xPx - m_horzDragGrab;
    ratio     = (travel > 0.0f) ? ((thumbLeft - (float) m.trackLeft) / travel) : 0.0f;

    SetLeftPx ((int) std::lround (ratio * (float) maxLeft));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequiredRowsForHeightPx
//
//  Number of data rows that fit within the given pixel height
//  (subtracting header + header gap). The caller uses this for manual
//  tail-virtualization: push only the most recent N rows.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetRequiredRowsForHeightPx (int heightPx) const
{
    HRESULT  hr      = S_OK;
    int      result  = 0;
    int      rowH    = m_list.GetRowHeightPx();
    int      headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int      hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int      body    = heightPx - headerH - hdrGap;



    BAIL_OUT_IF (rowH <= 0 || body <= 0, S_OK);

    result = body / rowH;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequiredHeightPx
//
//  Body height required to host the current header + rows at the
//  current DPI. The caller uses this to size customBodyMinSizePx.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetRequiredHeightPx() const
{
    int  rows    = m_list.m_grouping.GetLineCount();
    int  rowH    = m_list.GetRowHeightPx();
    int  headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int  hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;



    return headerH + hdrGap + rows * rowH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintScrollbar
//
//  Paints the scrollbar track, thumb, and (when the bar is tall
//  enough) the up/down arrow buttons. No-op when the bar is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::PaintScrollbar (
    IDxuiPainter  & painter,
    const DxuiListViewTypes::Palette & pal,
    float           x,
    float           y) const
{
    HRESULT                              hr      = S_OK;
    DxuiListViewTypes::ScrollbarMetrics  m       = GetScrollbarGeometry();
    int                                  headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int                                  hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int                                  hBarH   = ComputeScrollLayout().hBar ? m_list.GetScrollbarWidthPx() : 0;
    int                                  by      = headerH + hdrGap;
    int                                  bh      = (m_list.m_boundsDip.bottom - m_list.m_boundsDip.top) - by - hBarH;



    BAIL_OUT_IF (!m.visible, S_OK);

    m_vertScroll.SetTrack (RECT{ (int) x + m.barX, (int) y + by, (int) x + m.barX + m.barW, (int) y + by + bh });
    m_vertScroll.Paint (painter, pal.fg);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHScrollbar
//
//  Paints the horizontal scrollbar track, thumb, and (when the bar is
//  wide enough) the left/right arrow buttons along the bottom of the
//  list. No-op when the bar is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::PaintHScrollbar (
    IDxuiPainter  & painter,
    const DxuiListViewTypes::Palette & pal,
    float           x,
    float           y) const
{
    HRESULT                                  hr    = S_OK;
    DxuiListViewTypes::HorzScrollbarMetrics  m     = GetHorzScrollbarGeometry();
    int                                      viewW = ComputeScrollLayout().viewportW;



    BAIL_OUT_IF (!m.visible, S_OK);

    m_horzScroll.SetTrack (RECT{ (int) x, (int) y + m.barY, (int) x + viewW, (int) y + m.barY + m.barH });
    m_horzScroll.Paint (painter, pal.fg);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::OnPress
//
//  Tries the horizontal then vertical scrollbar regions (arrows, thumb,
//  track) at the press point, acting on the first hit: arrows step, the
//  thumb starts a drag, the track pages. Returns true when one was hit.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::OnPress (int lx, int ly)
{
    bool  handled = true;
    int   hStep   = m_list.m_scaler.ToPx (DxuiListView::s_kHScrollStepDip);



    // A press on an arrow or the track does its one-shot scroll now and
    // arms auto-repeat (Tick fires the rest while the button is held);
    // grabbing the thumb starts a drag and does not repeat.
    m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
    m_scrollRepeatXPx     = lx;
    m_scrollRepeatYPx     = ly;
    m_scrollRepeatNextMs  = 0;

    if      (HitTestHorzScrollbarArrowLeft  (lx, ly)) { SetLeftPx (m_list.m_leftPx - hStep);       m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::HorzArrowLeft;  }
    else if (HitTestHorzScrollbarArrowRight (lx, ly)) { SetLeftPx (m_list.m_leftPx + hStep);       m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::HorzArrowRight; }
    else if (HitTestHorzScrollbarThumb      (lx, ly)) { BeginHorzThumbDrag (lx); }
    else if (HitTestHorzScrollbarTrack      (lx, ly)) { GetPageFromHorzTrackClick (lx);        m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::HorzTrack;      }
    else if (HitTestScrollbarArrowUp     (lx, ly)) { m_list.ScrollByRows (-1);                     m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::VertArrowUp;    }
    else if (HitTestScrollbarArrowDown   (lx, ly)) { m_list.ScrollByRows (1);                      m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::VertArrowDown;  }
    else if (HitTestScrollbarThumb       (lx, ly)) { BeginThumbDrag (ly); }
    else if (HitTestScrollbarTrack       (lx, ly)) { GetPageFromTrackClick (ly);               m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::VertTrack;      }
    else                                           { handled = false; }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::OnDrag
//
//  A thumb being dragged follows the pointer. Returns whether one was.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::OnDrag (int lx, int ly)
{
    if (m_vertDragging)
    {
        UpdateThumbDrag (ly);
        return true;
    }

    if (m_horzDragging)
    {
        UpdateHorzThumbDrag (lx);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::OnRelease
//
//  Ends a held arrow or track, or a thumb drag. Returns whether one was in
//  progress.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::OnRelease()
{
    if (m_scrollRepeat != DxuiListViewTypes::ScrollRepeat::None)
    {
        m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
        return true;
    }

    if (m_vertDragging)
    {
        EndThumbDrag();
        return true;
    }

    if (m_horzDragging)
    {
        EndHorzThumbDrag();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::EndThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::EndThumbDrag()
{
    m_vertDragging = false;
    m_vertDragGrab = 0.0f;
    m_vertScroll.SetDragOffset (std::nullopt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::EndHorzThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::EndHorzThumbDrag()
{
    m_horzDragging = false;
    m_horzDragGrab = 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::OnWheel
//
//  Scrolls the list when the pointer is inside: Shift+wheel scrolls
//  horizontally, otherwise vertically. ev.wheelDelta is in notches and
//  is converted to the raw WHEEL_DELTA units the scroll helpers expect.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListScrollbars::OnWheel (const DxuiMouseEvent & ev, bool inside)
{
    HRESULT  hr       = S_OK;
    bool     handled  = false;
    int      hStep    = m_list.m_scaler.ToPx (DxuiListView::s_kHScrollStepDip);
    int      rawDelta = (int) (ev.wheelDelta * (float) WHEEL_DELTA);



    BAIL_OUT_IF (!inside, S_OK);

    if (ev.wheelHorizontal)
    {
        // Trackpad / tilt-wheel horizontal (WM_MOUSEHWHEEL): a positive
        // delta means "scroll right", the opposite of the Shift+wheel
        // convention below (positive = toward the start), so negate.
        ScrollByWheelDeltaHorizontal (-rawDelta, hStep);
    }
    else if (ev.shift)
    {
        ScrollByWheelDeltaHorizontal (rawDelta, hStep);
    }
    else
    {
        ScrollByWheelDelta (rawDelta, GetWheelLinesPerNotch());
    }

    handled = true;

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::Tick
//
//  Drives scrollbar auto-repeat for a held arrow / track press. The host
//  calls this once per frame with a monotonic millisecond clock: after
//  the initial delay the pressed action fires at the repeat interval
//  until OnRelease clears the press. Track paging stops when the
//  thumb reaches the held point (matching the OS scrollbar).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListScrollbars::Tick (int64_t nowMs)
{
    int   hStep      = 0;
    bool  reschedule = true;



    if (m_scrollRepeat == DxuiListViewTypes::ScrollRepeat::None)
    {
        // Nothing held. The common case, so it is the first test.
    }
    else if (m_scrollRepeatNextMs == 0)
    {
        // Arm the initial delay on the first tick after the press.
        m_scrollRepeatNextMs = nowMs + s_kScrollRepeatDelayMs;
    }
    else if (nowMs >= m_scrollRepeatNextMs)
    {
        hStep = m_list.m_scaler.ToPx (DxuiListView::s_kHScrollStepDip);

        switch (m_scrollRepeat)
        {
            case DxuiListViewTypes::ScrollRepeat::VertArrowUp:    m_list.ScrollByRows (-1);            break;
            case DxuiListViewTypes::ScrollRepeat::VertArrowDown:  m_list.ScrollByRows (1);             break;
            case DxuiListViewTypes::ScrollRepeat::HorzArrowLeft:  SetLeftPx (m_list.m_leftPx - hStep); break;
            case DxuiListViewTypes::ScrollRepeat::HorzArrowRight: SetLeftPx (m_list.m_leftPx + hStep); break;

            // Track paging stops once the thumb reaches the held point --
            // the repeat ends outright, so there is nothing to reschedule.
            case DxuiListViewTypes::ScrollRepeat::VertTrack:
                if (HitTestScrollbarThumb (m_scrollRepeatXPx, m_scrollRepeatYPx))
                {
                    m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
                    reschedule     = false;
                }
                else
                {
                    GetPageFromTrackClick (m_scrollRepeatYPx);
                }

                break;

            case DxuiListViewTypes::ScrollRepeat::HorzTrack:
                if (HitTestHorzScrollbarThumb (m_scrollRepeatXPx, m_scrollRepeatYPx))
                {
                    m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
                    reschedule     = false;
                }
                else
                {
                    GetPageFromHorzTrackClick (m_scrollRepeatXPx);
                }

                break;

            default:
                break;
        }

        if (reschedule)
        {
            m_scrollRepeatNextMs = nowMs + s_kScrollRepeatIntervalMs;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars::GetWheelLinesPerNotch
//
//  How many rows one wheel notch moves, from the user's mouse settings.
//
//  "One screen at a time" does not report a line count -- it reports
//  WHEEL_PAGESCROLL, which is UINT_MAX and would scroll four billion rows if
//  it were taken at face value. It is resolved here against the list's own
//  visible capacity, which is what a screen means for THIS list, and it can
//  change with every resize, so it is asked for per notch rather than cached.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListScrollbars::GetWheelLinesPerNotch() const
{
    int  lines = DxuiSystemSettings::Instance().GetWheelLinesPerNotch();
    int  cap   = 0;



    if (lines != DxuiSystemSettings::kWheelPageScroll)
    {
        return lines;
    }

    cap = GetVisibleRowCapacity();

    return (cap > 0) ? cap : 1;
}
