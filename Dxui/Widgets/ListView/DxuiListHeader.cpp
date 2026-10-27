#include "Pch.h"

#include "Widgets/ListView/DxuiListHeader.h"
#include "Core/DxuiClipboard.h"
#include "Theme/DxuiTheme.h"
#include "Core/DxuiTextElide.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiColor.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::DxuiListHeader
//
////////////////////////////////////////////////////////////////////////////////

DxuiListHeader::DxuiListHeader (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestColumnResize
//
//  Returns the index of a column whose right-edge separator the point
//  is hovering, suitable for starting a column-resize drag, or -1 if
//  the point is outside the header strip or not near a resize handle.
//  tolerancePx is the half-width of the hit zone straddling the
//  separator. The last visible column has no right separator and is
//  excluded.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListHeader::HitTestColumnResize (int xPx, int yPx, int tolerancePx) const
{
    HRESULT           hr      = S_OK;
    int               result  = -1;
    int               headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int               xAdj    = m_list.m_hScrollEnabled ? (xPx + m_list.m_leftPx) : xPx;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    BAIL_OUT_IF (!m_list.m_showHeader || headerH <= 0, S_OK);
    BAIL_OUT_IF (yPx < 0 || yPx >= headerH, S_OK);
    BAIL_OUT_IF (xPx < 0 || xPx >= fullW, S_OK);

    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        int  rightEdge = 0;

        if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        //  EVERY COLUMN'S EDGE CAN BE GRABBED, a stretch column's included. A
        //  stretch column absorbs the spare width by default, but that is no
        //  reason it cannot be given a width of its own: the layout keeps a
        //  width dragged onto one. Excluding it made the file browser's Name
        //  column, the one most wanted wider, impossible to resize.
        rightEdge = colXPx[c] + colWPx[c];

        if (xAdj >= rightEdge - tolerancePx && xAdj <= rightEdge + tolerancePx)
        {
            result = (int) c;
            break;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHeaderColumn
//
//  xPx/yPx are relative to the list's rect.left/top. Returns the column
//  index of the header cell under the point (visible columns only), or
//  -1 if the point is not inside the header strip. Use this to
//  implement per-column sort on click.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListHeader::HitTestHeaderColumn (int xPx, int yPx) const
{
    HRESULT          hr      = S_OK;
    int              result  = -1;
    int              headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int              cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool             needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int              fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int              xAdj    = m_list.m_hScrollEnabled ? (xPx + m_list.m_leftPx) : xPx;
    std::vector<int> colXPx;
    std::vector<int> colWPx;



    BAIL_OUT_IF (!m_list.m_showHeader || headerH <= 0, S_OK);
    BAIL_OUT_IF (yPx < 0 || yPx >= headerH, S_OK);
    BAIL_OUT_IF (xPx < 0 || xPx >= fullW, S_OK);

    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if (xAdj >= colXPx[c] && xAdj < colXPx[c] + colWPx[c])
        {
            result = (int) c;
            break;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHeader
//
//  Paints the header background, the column titles, the active sort
//  glyph, the bottom border, and the faint vertical column separators.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::PaintHeader (
    IDxuiPainter           & painter,
    IDxuiTextRenderer    & text,
    const DxuiListViewTypes::Palette          & pal,
    float                    x,
    float                    y,
    float                    layoutW,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    HRESULT  hr        = S_OK;
    float    headerH   = (float) m_list.GetHeaderBarPx();
    float    cellPadR  = (float) m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    float    hdrFontPx = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    float    colOff    = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    //  Explorer sets its titles and dividers in a band shorter than the bar.
    float    titleH    = m_list.m_explorerDetails ? (std::min) (headerH, m_list.m_scaler.ToPxf (s_kExplorerTitleBandDip)) : headerH;



    painter.FillRect (x, y, layoutW, headerH, pal.bgHeader);

    //  The dragged header goes last, over the others; it follows the pointer
    //  along the header row and nowhere else.
    for (size_t n = 0; n <= m_list.m_columns.size(); ++n)
    {
        bool     dragged     = n == m_list.m_columns.size();
        size_t   c           = dragged ? (size_t) (std::max) (m_list.m_headerPressCol, 0) : n;
        bool     hasSort     = ((int) c == m_sortColumn) && m_list.m_columns[c].visible && (colWPx[c] > 0);
        float    sortReserve = 0.0f;
        float    titlePad    = (float) m_list.m_columnSizing.GetHeaderTitlePadPx (c);
        float    titleW      = (float) colWPx[c] - titlePad - cellPadR - sortReserve;
        float    shift       = 0.0f;

        if (dragged && !(m_list.m_headerDragging && m_list.m_headerPressCol >= 0))
        {
            continue;
        }

        if (!dragged && m_list.m_headerDragging && (int) c == m_list.m_headerPressCol)
        {
            continue;
        }

        if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if (dragged)
        {
            shift = std::clamp ((float) (m_list.m_headerDragXPx - m_list.m_headerPressXPx), -(float) colXPx[c], layoutW - (float) (colXPx[c] + colWPx[c]) - colOff);

            painter.FillRect (x + colOff + (float) colXPx[c] + shift, y, (float) colWPx[c], headerH, pal.bgHover);
        }
        else
        {
            shift = GetHeaderSlidePx (c, m_list.GetClockMs());
        }

        if (titleW < 0.0f)
        {
            titleW = 0.0f;
        }

        hr = text.DrawString (m_list.m_columns[c].title.c_str(),
                              x + colOff + (float) colXPx[c] + shift + titlePad,
                              y,
                              titleW,
                              titleH,
                              pal.hdrFg, hdrFontPx, DxuiTheme::kBodyFace,
                              m_list.m_explorerDetails ? DxuiTextHAlign::Left : m_list.m_columns[c].align,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  File Explorer's sort indicator: a small chevron centered over the
        //  column title, pointing up for ascending and down for descending.
        if (hasSort)
        {
            float  halfW = (float) m_list.m_scaler.ToPxf (3.0f);
            float  halfH = (float) m_list.m_scaler.ToPxf (1.5f);
            float  thick = (float) m_list.m_scaler.ToPxf (0.75f);
            float  cx    = x + colOff + (float) colXPx[c] + shift + (float) colWPx[c] / 2.0f;
            float  cy    = y + (float) m_list.m_scaler.ToPxf (6.0f);
            float  tipY  = m_sortDescending ? (cy + halfH) : (cy - halfH);
            float  endY  = m_sortDescending ? (cy - halfH) : (cy + halfH);

            painter.DrawLine (cx - halfW, endY, cx, tipY, thick, DxuiColor::ScaleAlpha (pal.hdrFg, 0.7f));
            painter.DrawLine (cx, tipY, cx + halfW, endY, thick, DxuiColor::ScaleAlpha (pal.hdrFg, 0.7f));
        }
    }

    // Faint vertical separators between header columns so the user
    // can see where each column ends (and where the resize handle
    // lives). The separators scroll with the columns, so CPU-clip any
    // that fall outside the content viewport (IDxuiPainter has no clip).
    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        float  sepX = 0.0f;

        if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        sepX = x + colOff + (float) colXPx[c] + (float) colWPx[c] - 1.0f + GetHeaderSlidePx (c, m_list.GetClockMs());

        if (m_list.m_headerDragging && (int) c == m_list.m_headerPressCol)
        {
            continue;
        }

        if (sepX < x || sepX >= x + layoutW)
        {
            continue;
        }

        if (m_list.m_explorerDetails)
        {
            painter.FillRect (sepX, y, 1.0f, titleH, DxuiColor::ScaleAlpha (pal.fg, s_kExplorerDividerAlpha));
        }
        else
        {
            painter.FillRect (sepX, y + 2.0f, 1.0f, headerH - 4.0f, pal.border);
        }
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateHeaderSlides
//
//  Where each column would sit if the dragged one landed at the pointer: the
//  ones between where it was and where it would go move over by its width.
//  A column whose place changes starts easing toward it from where it is now.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::UpdateHeaderSlides()
{
    int               pos     = GetColumnDropPosition (m_list.m_headerDragXPx);
    int64_t           now     = m_list.GetClockMs();
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    size_t            from    = 0;
    float             width   = 0.0f;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    if (pos == m_headerDropPos || m_list.m_headerPressCol < 0)
    {
        return;
    }

    m_headerDropPos = pos;
    m_headerSlides.resize (m_list.m_columns.size());

    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    from  = (size_t) (std::find (m_list.m_columnOrder.begin(), m_list.m_columnOrder.end(), (size_t) m_list.m_headerPressCol) - m_list.m_columnOrder.begin());
    width = (float) colWPx[(size_t) m_list.m_headerPressCol];

    for (size_t n = 0; n < m_list.m_columnOrder.size(); n++)
    {
        size_t  c      = m_list.m_columnOrder[n];
        float   target = 0.0f;

        if ((int) c == m_list.m_headerPressCol)
        {
            continue;
        }

        if ((size_t) pos > from + 1 && n > from && n < (size_t) pos)
        {
            target = -width;
        }
        else if ((size_t) pos < from && n >= (size_t) pos && n < from)
        {
            target = width;
        }

        if (target != m_headerSlides[c].toPx)
        {
            m_headerSlides[c].fromPx  = GetHeaderSlidePx (c, now);
            m_headerSlides[c].toPx    = target;
            m_headerSlides[c].startMs = now;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHeaderSlidePx
//
//  How far a column's header has slid at this moment, eased out.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListHeader::GetHeaderSlidePx (size_t column, int64_t nowMs) const
{
    float  t = 0.0f;



    if (!m_list.m_headerDragging || column >= m_headerSlides.size())
    {
        return 0.0f;
    }

    const HeaderSlide & slide = m_headerSlides[column];

    t = std::clamp ((float) (nowMs - slide.startMs) / (float) s_kHeaderSlideMs, 0.0f, 1.0f);
    t = 1.0f - (1.0f - t) * (1.0f - t);

    return slide.fromPx + (slide.toPx - slide.fromPx) * t;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHeaderSliding
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListHeader::IsHeaderSliding (int64_t nowMs) const
{
    if (!m_list.m_headerDragging)
    {
        return false;
    }

    return std::any_of (m_headerSlides.begin(), m_headerSlides.end(), [nowMs] (const HeaderSlide & slide)
    {
        return slide.fromPx != slide.toPx && nowMs - slide.startMs < s_kHeaderSlideMs;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHeaderFocusMarkers
//
//  Overlays the keyboard-focus markers: a 1px rectangle around the
//  header that Space will sort on, and a brighter 2px bar on the
//  column divider that Left/Right will resize.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::PaintHeaderFocusMarkers (
    IDxuiPainter           & painter,
    const DxuiListViewTypes::Palette          & pal,
    float                    x,
    float                    y,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    float     headerH   = (float) m_list.GetHeaderBarPx();
    uint32_t  focusArgb = (pal.fg & 0x00FFFFFFu) | 0xC0000000u;
    float     colOff    = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    float     clipR     = x + (float) m_list.m_scrollbars.ComputeScrollLayout().viewportW;



    if (m_focusedHeaderCol >= 0 
        && (size_t) m_focusedHeaderCol < m_list.m_columns.size()
        && m_list.m_columns[(size_t) m_focusedHeaderCol].visible
        && colWPx[(size_t) m_focusedHeaderCol] > 0)
    {
        float  fx    = x + colOff + (float) colXPx[(size_t) m_focusedHeaderCol];
        float  fw    = (float) colWPx[(size_t) m_focusedHeaderCol];
        float  left  = std::max (fx, x);
        float  right = std::min (fx + fw, clipR);

        if (right > left)
        {
            painter.FillRect (left, y,               right - left, 1.0f,    focusArgb);
            painter.FillRect (left, y + headerH - 1, right - left, 1.0f,    focusArgb);

            if (fx >= x && fx < clipR)
            {
                painter.FillRect (fx, y, 1.0f, headerH, focusArgb);
            }

            if (fx + fw - 1.0f >= x && fx + fw - 1.0f < clipR)
            {
                painter.FillRect (fx + fw - 1.0f, y, 1.0f, headerH, focusArgb);
            }
        }
    }

    if (m_focusedDividerCol >= 0 && (size_t) m_focusedDividerCol < m_list.m_columns.size()
        && m_list.m_columns[(size_t) m_focusedDividerCol].visible
        && colWPx[(size_t) m_focusedDividerCol] > 0)
    {
        float  dx = x + colOff + (float) colXPx[(size_t) m_focusedDividerCol]
                      + (float) colWPx[(size_t) m_focusedDividerCol] - 2.0f;

        if (dx >= x && dx < clipR)
        {
            painter.FillRect (dx, y + 1.0f, 2.0f, headerH - 2.0f, focusArgb);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumnOrder
//
//  Taken only when it names every column once, so a stored order from a
//  build with other columns falls back to their own order.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::SetColumnOrder (const std::vector<size_t> & order)
{
    std::vector<bool>  seen (m_list.m_columns.size(), false);



    if (order.size() != m_list.m_columns.size())
    {
        return;
    }

    for (size_t c : order)
    {
        if (c >= seen.size() || seen[c])
        {
            return;
        }

        seen[c] = true;
    }

    m_list.m_columnOrder = order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnDropPosition
//
//  The position in the order before the first shown column whose middle is
//  right of the point, or the end.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListHeader::GetColumnDropPosition (int xPx) const
{
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int               xAdj    = m_list.m_hScrollEnabled ? (xPx + m_list.m_leftPx) : xPx;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t n = 0; n < m_list.m_columnOrder.size(); n++)
    {
        size_t  c = m_list.m_columnOrder[n];

        if (m_list.m_columns[c].visible && colWPx[c] > 0 && xAdj < colXPx[c] + colWPx[c] / 2)
        {
            return (int) n;
        }
    }

    return (int) m_list.m_columnOrder.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndHeaderPress
//
//  A release on the header pressed sorts by it; a release after a drag
//  moves the pressed column to where it was dropped.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::EndHeaderPress (int lx, int ly)
{
    int     pressed = m_list.m_headerPressCol;
    bool    dragged = m_list.m_headerDragging;
    int     target  = 0;
    size_t  from    = 0;



    m_list.m_headerPressCol = -1;
    m_list.m_headerDragging = false;
    m_headerDropPos         = -1;
    m_headerSlides.clear();

    if (!dragged)
    {
        if (HitTestHeaderColumn (lx, ly) == pressed && m_onSortColumn)
        {
            m_onSortColumn (pressed);
        }

        return;
    }

    target = GetColumnDropPosition (lx);
    from   = (size_t) (std::find (m_list.m_columnOrder.begin(), m_list.m_columnOrder.end(), (size_t) pressed) - m_list.m_columnOrder.begin());

    if (from >= m_list.m_columnOrder.size())
    {
        return;
    }

    //  Dropped on either side of itself, it stays where it is.
    if ((size_t) target == from || (size_t) target == from + 1)
    {
        return;
    }

    m_list.m_columnOrder.erase (m_list.m_columnOrder.begin() + (ptrdiff_t) from);

    if ((size_t) target > from)
    {
        target--;
    }

    m_list.m_columnOrder.insert (m_list.m_columnOrder.begin() + target, (size_t) pressed);

    if (m_onColumnsReordered)
    {
        m_onColumnsReordered (m_list.m_columnOrder);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::ClearColumnFocusMarkers
//
//  Drops the header and divider focus markers without disturbing which
//  sub-stop the keyboard is on. Used where the sub-stop is the list body,
//  which has no marker of its own -- the selected row is its focus cue.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::ClearColumnFocusMarkers()
{
    m_list.SetFocusedHeaderColumn (-1);
    m_list.SetFocusedDividerColumn (-1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::ReleaseKeyboardColumnFocus
//
//  Leaves keyboard column navigation entirely: no sub-stop, no markers.
//  Called when focus leaves the list and when Tab walks off either end of
//  the sub-stop ring. The two must move together, or a marker keeps
//  painting a focus cue for a control that no longer has focus.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::ReleaseKeyboardColumnFocus()
{
    m_kbColFocus = -1;
    ClearColumnFocusMarkers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::ApplyKeyboardColumnFocus
//
//  Reflects the current keyboard sub-stop (m_kbColFocus) into the header /
//  divider focus markers the widget paints. Even sub-stops select a
//  header column, odd sub-stops select the divider trailing a column, and
//  the final sub-stop is the list body (no header / divider marker).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::ApplyKeyboardColumnFocus()
{
    int  n    = m_list.m_columnSizing.GetVisibleColumnCount();
    int  body = (2 * n) - 1;
    int  col  = -1;



    if (m_kbColFocus < 0 || m_kbColFocus == body)
    {
        // No sub-stop, or the body -- neither paints a header or divider cue.
        ClearColumnFocusMarkers();
    }
    else if ((m_kbColFocus % 2) == 0)
    {
        col = m_list.m_columnSizing.GetNthVisibleColumnIndex (m_kbColFocus / 2);
        m_list.SetFocusedHeaderColumn (col);
        m_list.SetFocusedDividerColumn (-1);
    }
    else
    {
        col = m_list.m_columnSizing.GetNthVisibleColumnIndex ((m_kbColFocus - 1) / 2);
        m_list.SetFocusedDividerColumn (col);
        m_list.SetFocusedHeaderColumn (-1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::HandleKeyboardColumnKey
//
//  Acts on a header or divider keyboard sub-stop. A header sub-stop sorts
//  its column on Enter / Space; a divider sub-stop nudges the width of the
//  column it trails left / right by a fixed step, clamped to the minimum
//  column width, and reports the new width through m_onColumnResized.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListHeader::HandleKeyboardColumnKey (WPARAM vk)
{
    bool  isHeaderStop = ((m_kbColFocus % 2) == 0);
    bool  handled      = false;
    int   col          = -1;
    int   stepPx       = 0;
    int   minPx        = 0;
    int   cur          = 0;
    int   next         = 0;



    if (isHeaderStop && (vk == VK_RETURN || vk == VK_SPACE))
    {
        col = m_list.m_columnSizing.GetNthVisibleColumnIndex (m_kbColFocus / 2);

        if (col >= 0 && m_onSortColumn)
        {
            m_onSortColumn (col);
        }

        // Claimed even with no column or no callback: the sub-stop is a
        // header, and Enter on a header is a header's key to eat.
        handled = true;
    }
    else if (!isHeaderStop && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        col = m_list.m_columnSizing.GetNthVisibleColumnIndex ((m_kbColFocus - 1) / 2);

        // Unlike the header stop, a divider with no column has nothing to
        // resize, so the key falls through instead.
        if (col >= 0)
        {
            stepPx = m_list.m_scaler.ToPx (s_kKbResizeStepDip);
            minPx  = m_list.m_scaler.ToPx (DxuiListView::s_kMinColWidthDip);
            cur    = m_list.m_columnSizing.GetColumnEffectiveWidthPx ((size_t) col);
            next   = cur + ((vk == VK_LEFT) ? -stepPx : stepPx);

            if (next < minPx)
            {
                next = minPx;
            }

            m_list.m_columnSizing.SetColumnOverrideWidthPx ((size_t) col, next);

            if (m_list.m_onColumnResized)
            {
                m_list.m_onColumnResized (col, next);
            }

            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::OnKeyColumnResizeNav
//
//  Column/divider model (opt-in via SetKeyboardColumnResize). Tab walks 2N
//  sub-stops for N visible columns -- header0, divider0, header1, divider1,
//  ..., header(N-1), body -- and Shift+Tab walks back. Tabbing past either end
//  releases the sub-focus and returns false so the host focus manager advances.
//  At a sub-stop, keys act on it: sort a header, resize a divider, or move rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListHeader::OnKeyColumnResizeNav (const DxuiKeyEvent & ev)
{
    int   n       = m_list.m_columnSizing.GetVisibleColumnCount();
    int   stops   = 2 * n;
    int   body    = stops - 1;
    bool  handled = false;



    if (n <= 0)
    {
        // No visible columns means no sub-stops to walk.
    }
    else if (ev.vk == VK_TAB)
    {
        // -1 is the neutral entry: the first Tab picks a direction into the
        // ring rather than resuming wherever the sub-focus last was.
        m_kbColFocus = ev.shift ? ((m_kbColFocus == -1) ? body : (m_kbColFocus - 1))
                                : ((m_kbColFocus == -1) ? 0    : (m_kbColFocus + 1));

        // Walking off either end releases the sub-focus and leaves the key
        // unhandled, so the host focus manager moves to the next control.
        if (m_kbColFocus < 0 || m_kbColFocus >= stops)
        {
            ReleaseKeyboardColumnFocus();
        }
        else
        {
            ApplyKeyboardColumnFocus();
            handled = true;
        }
    }
    else if (m_kbColFocus == body)
    {
        handled = m_list.m_keyboard.HandleKeyboardBodyRowNav (ev.vk, ev.shift, ev.ctrl);
    }
    else if (m_kbColFocus != -1)
    {
        handled = HandleKeyboardColumnKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::OnKeyBodyHeaderNav
//
//  Default model (File-Explorer details view). Two sub-stops: body then
//  header. Tab: body -> header -> release; Shift+Tab reverses. Body: arrows /
//  Home / End / Page move the selection, Space/Enter activate the selected row.
//  Header: Left/Right cycle the focused column, Space/Enter sort it. No
//  column-resize sub-stops (see OnKeyColumnResizeNav).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListHeader::OnKeyBodyHeaderNav (const DxuiKeyEvent & ev)
{
    static constexpr int  kBody   = 0;
    static constexpr int  kHeader = 1;
    static constexpr int  kStops  = 2;



    //  Space selects in a list that selects several, as Explorer's does;
    //  Enter opens.
    bool  isActivate = (ev.vk == VK_RETURN || (ev.vk == VK_SPACE && !m_list.m_multiSelect));
    bool  handled    = false;

    if (ev.vk == VK_TAB)
    {
        m_kbColFocus = ev.shift ? ((m_kbColFocus < 0) ? (kStops - 1) : (m_kbColFocus - 1))
                                : ((m_kbColFocus < 0) ? kBody        : (m_kbColFocus + 1));

        // Off either end: release and let the host focus manager take over.
        if (m_kbColFocus < 0 || m_kbColFocus >= kStops)
        {
            ReleaseKeyboardColumnFocus();
        }
        else
        {
            ApplyBodyHeaderFocus();
            handled = true;
        }
    }
    else if (m_kbColFocus == kBody)
    {
        if (isActivate)
        {
            // Only claim Enter/Space when there is a row AND somewhere to
            // report it; otherwise the dialog's default button should get it.
            handled = (m_list.GetSelectedRow() >= 0 && m_list.m_onActivateRow);

            if (handled)
            {
                m_list.m_onActivateRow (m_list.GetSelectedRow());
            }
        }
        else if (DxuiCommandRouter::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift) != DxuiStandardCommand::None)
        {
            handled = m_list.InvokeCommand (DxuiCommandRouter::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift));
        }
        else
        {
            handled = m_list.m_keyboard.HandleKeyboardBodyRowNav (ev.vk, ev.shift, ev.ctrl);
        }
    }
    else if (m_kbColFocus == kHeader)
    {
        if (ev.vk == VK_LEFT || ev.vk == VK_RIGHT)
        {
            MoveHeaderFocus ((ev.vk == VK_RIGHT) ? 1 : -1);
            handled = true;
        }
        else if (isActivate && m_focusedHeaderCol >= 0 && m_onSortColumn)
        {
            m_onSortColumn (m_focusedHeaderCol);
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::ApplyBodyHeaderFocus
//
//  Reflects the body/header sub-stop into the paint markers. Body clears the
//  header marker (the selected row is the focus cue) and selects a row if none
//  is; header selects the focused column, defaulting to the first visible one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::ApplyBodyHeaderFocus()
{
    m_list.SetFocusedDividerColumn (-1);

    if (m_kbColFocus == 1)          // header
    {
        bool  valid = (m_focusedHeaderCol >= 0)
                      && (m_list.m_columnSizing.GetVisibleIndexOfColumn ((size_t) m_focusedHeaderCol) >= 0);

        if (!valid)
        {
            m_list.SetFocusedHeaderColumn (m_list.m_columnSizing.GetNthVisibleColumnIndex (0));
        }

        return;
    }

    m_list.SetFocusedHeaderColumn (-1);    // body

    if (m_list.GetSelectedRow() < 0 && m_list.GetRowCount() > 0)
    {
        m_list.SetSelectedRow (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader::MoveHeaderFocus
//
//  Steps the focused header column one visible column left / right, clamped to
//  the ends (no wrap).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListHeader::MoveHeaderFocus (int dir)
{
    int  n    = m_list.m_columnSizing.GetVisibleColumnCount();
    int  ord  = 0;
    int  next = 0;



    if (n <= 0)
    {
        return;
    }

    ord = (m_focusedHeaderCol >= 0) ? m_list.m_columnSizing.GetVisibleIndexOfColumn ((size_t) m_focusedHeaderCol) : 0;
    if (ord < 0)
    {
        ord = 0;
    }

    next = ord + dir;
    if (next < 0)
    {
        next = 0;
    }

    if (next > n - 1)
    {
        next = n - 1;
    }

    m_list.SetFocusedHeaderColumn (m_list.m_columnSizing.GetNthVisibleColumnIndex (next));
}
