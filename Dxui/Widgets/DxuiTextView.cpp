#include "Pch.h"
#include "Core/DxuiSystemSettings.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "DxuiTextSearch.h"
#include "DxuiTextViewPainter.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiClipboard.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiPaneMetrics.h"
#include "Render/IDxuiTextRenderer.h"
#include "Render/IDxuiPainter.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiEvents.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_layout.SetRows (std::move (rows));
    m_selection.Reset();
    m_topLine    = m_followEnd ? m_topLine : 0;
    m_hoverRow   = -1;
    m_focusRow   = -1;
    m_input.Reset();
    m_find.Reset();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::UpdateRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::UpdateRows (std::vector<Row> rows)
{
    auto  clamp = [this] (Position & at)
    {
        const std::vector<Row>  & laid = m_layout.GetRows();

        if (laid.empty())
        {
            at = Position();
            return;
        }

        at.row    = std::clamp (at.row, 0, (int) laid.size() - 1);
        at.offset = std::clamp (at.offset, 0, DxuiTextLayout::GetRowLength (laid[(size_t) at.row]));
    };



    m_layout.SetRows (std::move (rows));

    m_selection.ClampTo (clamp);

    m_find.ClampTo ((int) m_layout.GetRows().size(), clamp);

    m_focusRow = (std::min) (m_focusRow, (int) m_layout.GetRows().size() - 1);

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRowsDroppingFront
//
//  A position in a row that went is dropped: a highlight there goes, and a
//  selection or a scope with an end there is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRowsDroppingFront (std::vector<Row> rows, int dropped)
{
    m_layout.SetRows (std::move (rows));

    m_selection.DropFront (dropped);

    m_find.DropFront (dropped);

    //  A link under the pointer or pressed moves up with its row.
    m_hoverRow = (m_hoverRow >= dropped) ? m_hoverRow - dropped : -1;
    m_input.DropFront (dropped);

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetCellSize
//
//  A size set here is kept across DPI changes, as a test's is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetCellSize (int widthPx, int heightPx)
{
    int  width = (std::max) (widthPx, 0);



    m_layout.SetCellSize (width, (float) width, (std::max) (heightPx, 0));
    m_cellPinned = true;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetProportional
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetProportional (bool proportional)
{
    if (proportional == m_layout.IsProportional())
    {
        return;
    }

    m_layout.SetProportional (proportional);
    m_leftColumn = 0;

    if (!m_cellPinned)
    {
        m_layout.ClearCellSize();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineCap() const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2 - GetHorzBarPx();



    return m_layout.GetLineCap (m_topLine, height);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTopForLast
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTopForLast (int lineIndex) const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;



    return m_layout.GetTopForLast (lineIndex, height);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellAnchorPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellAnchorPx (int row, int cell, float & outX, float & outY) const
{
    int  line = GetFirstLineOfRow (row);
    int  side = 0;



    if (!GetRowSide (row, side) || side != 0 || m_layout.GetCellHeightPx() <= 0 || !GetCellLeftPx (row, cell, outX))
    {
        return false;
    }

    outY = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip) + (float) GetLineOffsetPx (line) + (float) GetLineHeightPx (line) * 0.5f;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowSide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetRowSide (int row, int & outSide) const
{
    int  line = GetFirstLineOfRow (row);



    if (line < 0)
    {
        return false;
    }

    outSide = (line < m_topLine) ? -1 : (line >= m_topLine + GetLineCap()) ? 1 : 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellLeftPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellLeftPx (int row, int cell, float & outX) const
{
    if (row < 0 || row >= (int) m_layout.GetRows().size())
    {
        return false;
    }

    outX = (float) GetTextLeft() + m_layout.GetCellX (m_layout.GetRows()[(size_t) row], cell);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinesSpanPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetLinesSpanPx (float & outTop, float & outBottom) const
{
    int  shown = (std::max) (0, (std::min) (GetLineCap(), m_layout.GetLineCount() - m_topLine));



    outTop    = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip);
    outBottom = outTop + (float) GetLineOffsetPx (m_topLine + shown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetTopLine (int line)
{
    int  maxTop = GetTopForLast (m_layout.GetLineCount() - 1);



    m_topLine = (std::max) (0, (std::min) (line, maxTop));
    m_atEnd   = m_topLine >= maxTop;

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextColumns
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextColumns (bool scrollbar) const
{
    return (m_layout.GetCellWidthPx() > 0) ? (std::max) (1, (int) (GetTextWidthPx (scrollbar) / m_layout.GetCellAdvance())) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextWidthPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetTextWidthPx (bool scrollbar) const
{
    int  width = (int) (m_boundsDip.right - m_boundsDip.left) - GetGutterLeftPx() - m_scaler.ToPx (s_kPadDip) - GetGutterPx();



    width -= scrollbar ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;

    return (float) width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Rebuild
//
//  Column widths from every row, then the drawn lines, laid out again
//  narrower if they turn out to need the scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Rebuild()
{
    const std::vector<Line>  & lines = m_layout.GetLines();
    Line                       top   = (m_topLine >= 0 && m_topLine < (int) lines.size()) ? lines[(size_t) m_topLine] : Line();
    int                        line  = 0;



    m_layout.ClearLines();

    if (!m_layout.HasCellSize())
    {
        return;
    }

    m_layout.Wrap (GetTextWidthPx (false), GetTextColumns (false), GetPxPerDip());

    //  Proportional text is laid out once its characters are measured, by
    //  their places rather than by cells, and always wraps.
    if (m_layout.IsProportional())
    {
        m_leftColumn = 0;

        if (!m_layout.IsMeasured())
        {
            return;
        }

        if (IsScrollbarVisible())
        {
            m_layout.Wrap (GetTextWidthPx (true), GetTextColumns (true), GetPxPerDip());
        }

        //  A new wrap moves every line, so the top is the line that now has
        //  the text the old top started with.
        while (line + 1 < (int) lines.size()
               && (lines[(size_t) line + 1].row < top.row || (lines[(size_t) line + 1].row == top.row && lines[(size_t) line + 1].start <= top.start)))
        {
            line++;
        }

        SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : line);
        return;
    }

    if (IsScrollbarVisible())
    {
        m_layout.Wrap (GetTextWidthPx (true), GetTextColumns (true), GetPxPerDip());
    }

    m_leftColumn = std::clamp (m_leftColumn, 0, (std::max) (0, m_layout.GetWidestColumns() - GetTextColumns (IsScrollbarVisible())));

    SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : m_topLine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SyncScrollbar()
{
    int         barW = m_scaler.ToPx (s_kScrollbarWidthDip);
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, m_layout.GetLineCount() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);

    if (!IsHorzScrollbarVisible())
    {
        return;
    }

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, 1);
    m_horzScroll.SetTrack (RECT { GetTextBaseLeft(), m_boundsDip.bottom - barW, m_boundsDip.right - (IsScrollbarVisible() ? barW : 0), m_boundsDip.bottom });

    info.nMax  = (std::max) (0, m_layout.GetWidestColumns() - 1);
    info.nPage = (UINT) GetTextColumns (IsScrollbarVisible());
    info.nPos  = m_leftColumn;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLayoutLook
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::Look DxuiTextView::GetLayoutLook() const
{
    Look  look;



    look.cellAdvance    = m_layout.GetCellAdvance();
    look.cellWidthPx    = m_layout.GetCellWidthPx();
    look.cellHeightPx   = m_layout.GetCellHeightPx();
    look.textTopPx      = m_textTopPx;
    look.padPx          = m_scaler.ToPx (s_kPadDip);
    look.topLine        = m_topLine;
    look.proportional   = m_layout.IsProportional();
    look.textStrength   = m_textStrength;
    look.gutterIconDip  = m_gutterIconDip;
    look.glyphCenterDip = m_glyphCenterDip;
    look.bandLeftDip    = m_bandLeftDip;
    look.bandWidthDip   = m_bandWidthDip;
    look.iconFace       = m_iconFace;
    look.highlightRow   = m_highlightRow;
    look.highlightCell  = m_highlightCell;
    look.hoverRow       = m_hoverRow;
    look.hoverSpan      = m_hoverSpan;

    return look;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::ScrollToPosition
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ScrollToPosition (Position pos)
{
    int  line    = m_layout.GetLineOfPosition (pos);
    int  cap     = GetLineCap();
    int  column  = 0;
    int  visible = 0;



    if (line < 0)
    {
        return;
    }

    if (line < m_topLine)
    {
        SetTopLine (line);
    }
    else if (line >= m_topLine + cap)
    {
        SetTopLine (GetTopForLast (line));
    }

    //  Without wrapping, the position's column is brought into view too.
    if (!m_layout.IsNoWrap() || pos.row < 0 || pos.row >= (int) m_layout.GetRows().size())
    {
        return;
    }

    column  = m_layout.GetColumnOfPosition (pos);
    visible = GetTextColumns (IsScrollbarVisible());

    if (column < m_leftColumn)
    {
        SetLeftColumn (column);
    }
    else if (column >= m_leftColumn + visible)
    {
        SetLeftColumn (column - visible + 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetZoom
//
//  A new size is measured again on the next paint, unless the host set the
//  cells itself.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetZoom (float zoom)
{
    if (zoom == m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_zoom = zoom;
    m_layout.ClearMeasure();

    if (!m_cellPinned)
    {
        m_layout.ClearCellSize();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLineSpacing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLineSpacing (float spacing)
{
    if (spacing == m_lineSpacing || spacing < 1.0f)
    {
        return;
    }

    m_lineSpacing = spacing;

    if (!m_cellPinned)
    {
        m_layout.ClearCellSize();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    if (!m_cellPinned && m_measuredDpi != scaler.GetDpi())
    {
        m_layout.ClearCellSize();
        m_layout.ClearMeasure();
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::EnsureCellSize
//
//  Measured at the size the face is drawn at, so cells and glyphs agree.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font     = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();
    float           width    = 0.0f;
    float           height   = 0.0f;
    int             heightPx = 0;
    HRESULT         hr       = S_OK;



    if (m_layout.GetCellWidthPx() > 0)
    {
        return;
    }

    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    heightPx = (std::max) (1, (int) (height * m_lineSpacing + 0.5f));

    m_layout.SetCellSize ((std::max) (1, (int) (width / 8.0f + 0.5f)), (std::max) (1.0f, width / 8.0f), heightPx);
    m_textTopPx   = (std::max) (0, (heightPx - (int) (height + 0.5f)) / 2);
    m_measuredDpi = m_scaler.GetDpi();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT              hr          = S_OK;
    DxuiFontHandle       font        = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();
    int                  last        = 0;
    float                clipLeft    = 0.0f;
    DxuiTextViewPainter  linePainter (*this);



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.TextViewBackground());

    if (!m_layout.HasCellSize())
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);

    if (m_layout.IsProportional() && !m_layout.IsMeasured())
    {
        MeasureCharacters (text, font);
    }

    last = (std::min) (m_layout.GetLineCount(), m_topLine + GetLineCap() + 1);

    linePainter.Begin();

    //  The rows' fills and the gutter's icons, which do not scroll sideways,
    //  then the text, clipped to the text area once it has scrolled.
    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        linePainter.PaintLineGutter (painter, text, theme, lineIndex);
    }

    clipLeft = (m_leftColumn > 0) ? (float) GetTextBaseLeft() : (float) m_boundsDip.left;

    hr = text.PushClipRect (clipLeft, (float) m_boundsDip.top, (float) m_boundsDip.right - clipLeft, (float) (m_boundsDip.bottom - m_boundsDip.top - GetHorzBarPx()));
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.PushClip (clipLeft, (float) m_boundsDip.top, (float) m_boundsDip.right - clipLeft, (float) (m_boundsDip.bottom - m_boundsDip.top - GetHorzBarPx()));

    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        linePainter.PaintLine (painter, text, theme, lineIndex, font);
    }

    painter.PopClip();

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (IsScrollbarVisible() && !m_vertBarHidden)
    {
        SetFindMarks();
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (IsHorzScrollbarVisible())
    {
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetHoverPoint
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::SetHoverPoint (POINT point)
{
    int   row     = -1;
    int   span    = -1;
    RECT  rect    = {};
    bool  changed = false;



    if (!TryGetLinkAt (point, row, span, rect))
    {
        row  = -1;
        span = -1;
    }

    changed     = row != m_hoverRow || span != m_hoverSpan;
    m_hoverRow  = row;
    m_hoverSpan = span;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextLeft
//
//  Where the text's first column is drawn: past the pad and the gutter, and
//  left by the columns scrolled past.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextLeft() const
{
    return GetTextBaseLeft() - (int) std::lround ((float) m_leftColumn * m_layout.GetCellAdvance());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCursorForPoint
//
//  The I-beam over the text, where a press selects; IDC_HAND over a link;
//  the arrow over the gutter and the scroll bars.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiTextView::GetCursorForPoint (POINT clientPx) const
{
    const RECT  & bounds = GetBounds();
    bool          inside = clientPx.x >= bounds.left && clientPx.x < bounds.right && clientPx.y >= bounds.top && clientPx.y < bounds.bottom;
    int           row    = -1;
    int           span   = -1;
    RECT          rect   = {};



    if (!inside || clientPx.x < GetTextBaseLeft() || IsOverScrollbar (clientPx))
    {
        return nullptr;
    }

    if (TryGetLinkAt (clientPx, row, span, rect))
    {
        return IDC_HAND;
    }

    return IDC_IBEAM;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextBaseLeft
//
//  The left of the text area, which does not scroll.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextBaseLeft() const
{
    return (int) m_boundsDip.left + GetGutterLeftPx() + GetGutterPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetGutterLeftPx
//
//  How far in from the view's left edge the gutter starts: at the edge for a
//  glyph margin, and past the pad otherwise.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetGutterLeftPx() const
{
    return (m_glyphCenterDip > 0.0f) ? 0 : GetPadLeftPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsInGutterBand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsInGutterBand (POINT point) const
{
    int  left = (int) m_boundsDip.left + m_scaler.ToPx (m_bandLeftDip);



    return m_bandWidthDip > 0 && point.y >= m_boundsDip.top && point.y < m_boundsDip.bottom &&
           point.x >= left && point.x < left + m_scaler.ToPx (m_bandWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsInGutter
//
//  Left of the text and off the gutter's band.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsInGutter (POINT point) const
{
    return point.x >= m_boundsDip.left && point.x < GetTextBaseLeft() && point.y >= m_boundsDip.top && point.y < m_boundsDip.bottom &&
           !IsInGutterBand (point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextAreaLeftPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextAreaLeftPx() const
{
    return GetTextBaseLeft();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextAreaRightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextAreaRightPx() const
{
    return (int) m_boundsDip.right - (IsScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellRectPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellRectPx (int row, int cell, RECT & outRect) const
{
    int    first = GetFirstLineOfRow (row);
    float  x     = 0.0f;



    if (first < m_topLine || first >= m_topLine + GetLineCap() || !GetCellLeftPx (row, cell, x))
    {
        return false;
    }

    outRect.left   = (LONG) std::lround (x);
    outRect.top    = (LONG) (m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (first));
    outRect.right  = GetTextAreaRightPx();
    outRect.bottom = outRect.top + GetLineHeightPx (first);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryGetCellTextClipPx
//
//  DrawRun starts a cell's characters at its column, unrounded, and Paint
//  clips the text at the view's right edge. The scroll bar has no track,
//  so text under it is visible.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryGetCellTextClipPx (int row, int cell, float & left, float & right) const
{
    RECT  rect    = {};
    bool  isShown = GetCellRectPx (row, cell, rect) && GetCellLeftPx (row, cell, left);



    right = (float) m_boundsDip.right;

    return isShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFocusRow
//
//  The focused row replaces any selection, so it is drawn, and is scrolled
//  into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFocusRow (int row)
{
    if (row < 0 || row >= (int) m_layout.GetRows().size())
    {
        m_focusRow = -1;
        return;
    }

    m_focusRow = row;
    m_selection.Select (Position { row, 0 }, Position { row, 0 });

    ScrollToPosition (m_selection.GetAnchor());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryMoveFocusRow
//
//  With no row focused, the moves count from the top row in view. False
//  for a key that moves nothing, such as Ctrl+Up, which is the host's.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryMoveFocusRow (WPARAM vk, bool ctrl)
{
    const std::vector<Line>  & lines  = m_layout.GetLines();
    int                        rows   = (int) m_layout.GetRows().size();
    int                        page   = (std::max) (1, GetLineCap());
    int                        last   = (int) lines.size() - 1;
    int                        top    = 0;
    int                        bottom = 0;
    int                        from   = 0;
    int                        next   = 0;
    bool                       isMove = true;



    if (rows == 0 || last < 0)
    {
        return false;
    }

    top    = lines[(size_t) std::clamp (m_topLine, 0, last)].row;
    bottom = lines[(size_t) std::clamp (m_topLine + page - 1, 0, last)].row;
    from   = (m_focusRow >= 0 && m_focusRow < rows) ? m_focusRow : top;

    switch (vk)
    {
    case VK_UP:    next = from - 1;   isMove = !ctrl; break;
    case VK_DOWN:  next = from + 1;   isMove = !ctrl; break;
    case VK_PRIOR: next = ctrl ? top    : from - page; break;
    case VK_NEXT:  next = ctrl ? bottom : from + page; break;
    case VK_HOME:  next = ctrl ? 0        : top;       break;
    case VK_END:   next = ctrl ? rows - 1 : bottom;    break;
    default:       isMove = false;                     break;
    }

    if (isMove)
    {
        SetFocusRow (std::clamp (next, 0, rows - 1));
    }

    return isMove;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetHorzBarPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetHorzBarPx() const
{
    return IsHorzScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsHorzScrollbarVisible
//
//  Measured against the width left beside a vertical bar, so the answer does
//  not depend on whether that bar is visible, which depends on this one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsHorzScrollbarVisible() const
{
    return m_layout.IsNoWrap() && !m_layout.IsProportional() && m_layout.GetCellWidthPx() > 0 && m_layout.GetWidestColumns() > GetTextColumns (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLeftColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLeftColumn (int column)
{
    int  most = (std::max) (0, m_layout.GetWidestColumns() - GetTextColumns (IsScrollbarVisible()));



    m_leftColumn = m_layout.IsNoWrap() ? std::clamp (column, 0, most) : 0;
    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetNoWrap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetNoWrap (bool noWrap)
{
    m_layout.SetNoWrap (noWrap);
    m_leftColumn = noWrap ? m_leftColumn : 0;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetPadLeftPx
//
//  The space left of the gutter and the text: the pad every side has, or in
//  a view that fills a pane, the pane's text inset, so the text starts where
//  the pane's title does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetPadLeftPx() const
{
    return m_paneTextInset ? DxuiPaneMetrics::GetContentTextInsetPx (m_scaler) : m_scaler.ToPx (s_kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::MeasureCharacters
//
//  Each cell's characters measured, then the lines laid out by the places.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::MeasureCharacters (IDxuiTextRenderer & text, const DxuiFontHandle & font)
{
    m_layout.Measure (text, font);
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Measure
//
//  What Paint measures, done ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Measure (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();



    EnsureCellSize (text, theme);

    if (m_layout.IsProportional() && !m_layout.IsMeasured() && m_layout.GetCellWidthPx() > 0)
    {
        font.sizeDip = m_scaler.ToPxf (font.sizeDip * m_zoom);

        MeasureCharacters (text, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetContentHeightPx
//
//  Zero before the text is measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetContentHeightPx() const
{
    if (m_layout.GetCellHeightPx() <= 0)
    {
        return 0;
    }

    return m_layout.GetLinesHeightPx() + m_scaler.ToPx (s_kPadDip) * 2 + GetHorzBarPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiTextView::GetScrollbarMenu (POINT pt)
{
    bool  onVert = IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (pt.x, pt.y);
    bool  onHorz = IsHorzScrollbarVisible() && m_horzScroll.HitTest (pt.x, pt.y);



    m_vertScroll.SetOnScroll ([this] (int, int pos) { SetTopLine (pos); });
    m_horzScroll.SetOnScroll ([this] (int, int pos) { SetLeftColumn (pos); });

    if (!onVert && !onHorz)
    {
        return {};
    }

    SyncScrollbar();

    return onVert ? m_vertScroll.BuildContextMenu (pt.x, pt.y) : m_horzScroll.BuildContextMenu (pt.x, pt.y);
}
