#include "Pch.h"
#include "DxuiTextRowLayout.h"
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
    m_rows       = std::move (rows);
    m_selection.Reset();
    m_topLine    = m_followEnd ? m_topLine : 0;
    m_hoverRow   = -1;
    m_focusRow   = -1;
    m_input.Reset();
    m_find.Reset();
    m_metrics.Clear();

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
        if (m_rows.empty())
        {
            at = Position();
            return;
        }

        at.row    = std::clamp (at.row, 0, (int) m_rows.size() - 1);
        at.offset = std::clamp (at.offset, 0, GetRowLength (m_rows[(size_t) at.row]));
    };



    m_rows = std::move (rows);

    m_selection.ClampTo (clamp);

    m_find.ClampTo ((int) m_rows.size(), clamp);

    m_focusRow = (std::min) (m_focusRow, (int) m_rows.size() - 1);
    m_metrics.Clear();

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
    m_rows = std::move (rows);

    m_selection.DropFront (dropped);

    m_find.DropFront (dropped);

    //  A link under the pointer or pressed moves up with its row.
    m_hoverRow = (m_hoverRow >= dropped) ? m_hoverRow - dropped : -1;
    m_input.DropFront (dropped);

    m_metrics.Clear();

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
    m_cellWidthPx  = (std::max) (widthPx,  0);
    m_cellHeightPx = (std::max) (heightPx, 0);
    m_cellAdvance  = (float) m_cellWidthPx;
    m_cellPinned   = true;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetProportional
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetProportional (bool proportional)
{
    if (proportional == m_proportional)
    {
        return;
    }

    m_proportional = proportional;
    m_leftColumn   = 0;
    m_metrics.Clear();

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
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
    int  used   = 0;
    int  count  = 0;
    int  line   = m_topLine;



    if (m_cellHeightPx <= 0)
    {
        return 1;
    }

    //  Past the end, a line is taken as a full one, so the count does not
    //  depend on how much text there is.
    for (;;)
    {
        int  lineHeight = (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;

        if (used + lineHeight > height)
        {
            break;
        }

        used += lineHeight;
        count++;
        line++;
    }

    return (std::max) (1, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineHeightPx
//
//  A full line, or the fraction of one a short row asks for, never less
//  than a pixel.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineHeightPx (int lineIndex) const
{
    float  fraction = m_rows[(size_t) m_lines[(size_t) lineIndex].row].height;



    if (fraction >= 1.0f)
    {
        return m_cellHeightPx;
    }

    return (std::max) (1, (int) ((float) m_cellHeightPx * fraction + 0.5f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineOffsetPx
//
//  How far below the first line shown a line's top is; negative for a line
//  above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOffsetPx (int lineIndex) const
{
    int  offset = 0;



    for (int line = m_topLine; line < lineIndex; line++)
    {
        offset += (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;
    }

    for (int line = lineIndex; line < m_topLine; line++)
    {
        offset -= GetLineHeightPx (line);
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTopForLast
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTopForLast (int lineIndex) const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;
    int  top    = lineIndex;
    int  used   = 0;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size() || m_cellHeightPx <= 0)
    {
        return 0;
    }

    used = GetLineHeightPx (lineIndex);

    while (top > 0 && used + GetLineHeightPx (top - 1) <= height)
    {
        top--;
        used += GetLineHeightPx (top);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFirstLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetFirstLineOfRow (int row) const
{
    for (size_t i = 0; i < m_lines.size(); i++)
    {
        if (m_lines[i].row == row)
        {
            return (int) i;
        }
    }

    return -1;
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



    if (!GetRowSide (row, side) || side != 0 || m_cellHeightPx <= 0 || !GetCellLeftPx (row, cell, outX))
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
    if (row < 0 || row >= (int) m_rows.size())
    {
        return false;
    }

    outX = (float) GetTextLeft() + GetCellX (m_rows[(size_t) row], cell);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinesSpanPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetLinesSpanPx (float & outTop, float & outBottom) const
{
    int  shown = (std::max) (0, (std::min) (GetLineCap(), (int) m_lines.size() - m_topLine));



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
    int  maxTop = GetTopForLast ((int) m_lines.size() - 1);



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
    return (m_cellWidthPx > 0) ? (std::max) (1, (int) (GetTextWidthPx (scrollbar) / m_cellAdvance)) : 1;
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
//  DxuiTextView::GetCellStart
//
//  The character column a cell begins in: past the warning mark, and past
//  every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellStart (const Row & row, int cell) const
{
    int  column = row.warning ? kWarningCells : 0;



    //  A cell longer than its capped column pushes the cells after it along.
    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        int   length = (c < (int) row.cells.size()) ? (int) row.cells[(size_t) c].size() : 0;
        auto  gap    = m_columnGaps.find (c);

        column += (std::max) (m_columnCells[(size_t) c], length) + ((gap != m_columnGaps.end()) ? gap->second : kColumnGapCells);
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellBase
//
//  Where a cell's first character is in the row's text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellBase (const Row & row, int cell) const
{
    int  base = 0;



    for (int c = 0; c < cell && c < (int) row.cells.size(); c++)
    {
        base += (int) row.cells[(size_t) c].size() + 1;
    }

    return base;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetRowLength (const Row & row) const
{
    return row.cells.empty() ? 0 : GetCellBase (row, (int) row.cells.size() - 1) + (int) row.cells.back().size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowExtent
//
//  The column just past a row's last character that counts toward the
//  sideways scroll. An empty cell adds nothing, not even the gap ahead of
//  it, and neither does a cell in a clipped column.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetRowExtent (const Row & row) const
{
    int  extent = row.warning ? kWarningCells : 0;



    for (int c = 0; c < (int) row.cells.size(); c++)
    {
        if (row.cells[(size_t) c].empty() || (m_firstClipped >= 0 && c >= m_firstClipped))
        {
            continue;
        }

        extent = (std::max) (extent, GetCellStart (row, c) + (int) row.cells[(size_t) c].size());
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetRowText (const Row & row) const
{
    std::wstring  text;



    for (size_t c = 0; c < row.cells.size(); c++)
    {
        text += (c > 0 ? L"\t" : L"") + row.cells[c];
    }

    return text;
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
    Line  top  = (m_topLine >= 0 && m_topLine < (int) m_lines.size()) ? m_lines[(size_t) m_topLine] : Line();
    int   line = 0;



    m_lines.clear();
    m_columnCells.clear();

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    //  Proportional text is laid out once its characters are measured, by
    //  their places rather than by cells, and always wraps.
    if (m_proportional)
    {
        m_widestColumns = 0;
        m_leftColumn    = 0;

        if (!m_metrics.IsMeasured (m_rows.size()))
        {
            return;
        }

        BuildProportionalLines (GetTextWidthPx (false));

        if (IsScrollbarVisible())
        {
            BuildProportionalLines (GetTextWidthPx (true));
        }

        //  A new wrap moves every line, so the top is the line that now has
        //  the text the old top started with.
        while (line + 1 < (int) m_lines.size()
               && (m_lines[(size_t) line + 1].row < top.row || (m_lines[(size_t) line + 1].row == top.row && m_lines[(size_t) line + 1].start <= top.start)))
        {
            line++;
        }

        SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : line);
        return;
    }

    for (const Row & row : m_rows)
    {
        for (size_t c = 0; c + 1 < row.cells.size(); c++)
        {
            if (m_columnCells.size() <= c)
            {
                m_columnCells.resize (c + 1, 0);
            }

            m_columnCells[c] = (std::max) (m_columnCells[c], (int) row.cells[c].size());
        }
    }

    for (const auto & [column, cells] : m_columnCaps)
    {
        if (column < (int) m_columnCells.size() && cells > 0)
        {
            m_columnCells[(size_t) column] = (std::min) (m_columnCells[(size_t) column], cells);
        }
    }

    BuildLines (GetTextColumns (false));

    if (IsScrollbarVisible())
    {
        BuildLines (GetTextColumns (true));
    }

    m_leftColumn = std::clamp (m_leftColumn, 0, (std::max) (0, m_widestColumns - GetTextColumns (IsScrollbarVisible())));

    SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : m_topLine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineColumn
//
//  The column a line of a row's last cell starts at: the cell's, then its
//  wrap indent on each wrapped line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineColumn (const Row & row, const Line & line) const
{
    return GetCellStart (row, (int) row.cells.size() - 1) + (line.first ? 0 : (std::max) (0, row.wrapIndent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::BuildLines
//
//  Wraps each row's last cell at the last space that fits, or mid-word when
//  none does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::BuildLines (int columns)
{
    m_lines.clear();
    m_widestColumns = 0;

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        int             start  = GetCellStart (row, (int) row.cells.size() - 1);
        int             indent = (std::max) (0, row.wrapIndent);
        int             avail  = (m_noWrap && !row.warning) ? INT_MAX : (std::max) (1, columns - start);
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

        if (row.picture != nullptr)
        {
            AddPictureLines (r, (float) columns * m_cellAdvance);
            continue;
        }

        m_widestColumns = row.warning ? m_widestColumns : (std::max) (m_widestColumns, GetRowExtent (row));

        do
        {
            int     take  = (std::min) (avail, length - pos);
            size_t  space = std::wstring::npos;

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, take, first });

            pos  += take;
            first = false;
            avail = (m_noWrap && !row.warning) ? INT_MAX : (std::max) (1, columns - start - indent);
        }
        while (pos < length);
    }
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
    info.nMax  = (std::max) (0, (int) m_lines.size() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);

    if (!IsHorzScrollbarVisible())
    {
        return;
    }

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, 1);
    m_horzScroll.SetTrack (RECT { GetTextBaseLeft(), m_boundsDip.bottom - barW, m_boundsDip.right - (IsScrollbarVisible() ? barW : 0), m_boundsDip.bottom });

    info.nMax  = (std::max) (0, m_widestColumns - 1);
    info.nPage = (UINT) GetTextColumns (IsScrollbarVisible());
    info.nPos  = m_leftColumn;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineOfPosition
//
//  The drawn line a position falls on: its row's first line, or a later one
//  when the position is in the part of the last cell that wrapped. -1 when
//  the row is not laid out.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOfPosition (Position pos) const
{
    int  line = GetFirstLineOfRow (pos.row);
    int  base = 0;



    if (line < 0)
    {
        return line;
    }

    base = GetCellBase (m_rows[(size_t) pos.row], (int) m_rows[(size_t) pos.row].cells.size() - 1);

    while (line + 1 < (int) m_lines.size() && m_lines[(size_t) line + 1].row == pos.row &&
           base + m_lines[(size_t) line + 1].start <= pos.offset)
    {
        line++;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindRowTexts
//
//  Each row's text, its cells joined by tabs, as find searches it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiTextView::GetFindRowTexts() const
{
    std::vector<std::wstring>  texts;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    return texts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLayoutLook
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::Look DxuiTextView::GetLayoutLook() const
{
    Look  look;



    look.cellAdvance    = m_cellAdvance;
    look.cellWidthPx    = m_cellWidthPx;
    look.cellHeightPx   = m_cellHeightPx;
    look.textTopPx      = m_textTopPx;
    look.padPx          = m_scaler.ToPx (s_kPadDip);
    look.topLine        = m_topLine;
    look.proportional   = m_proportional;
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
    int  line = GetLineOfPosition (pos);
    int  cap  = GetLineCap();



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
    if (m_noWrap && pos.row >= 0 && pos.row < (int) m_rows.size())
    {
        const Row &  row     = m_rows[(size_t) pos.row];
        int          column  = 0;
        int          visible = GetTextColumns (IsScrollbarVisible());

        for (int c = 0; c < (int) row.cells.size(); c++)
        {
            if (c + 1 == (int) row.cells.size() || pos.offset < GetCellBase (row, c + 1))
            {
                column = GetCellStart (row, c) + pos.offset - GetCellBase (row, c);
                break;
            }
        }

        if (column < m_leftColumn)
        {
            SetLeftColumn (column);
        }
        else if (column >= m_leftColumn + visible)
        {
            SetLeftColumn (column - visible + 1);
        }
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
    m_metrics.Clear();

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
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
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
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
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
        m_metrics.Clear();
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
    DxuiFontHandle  font   = m_proportional ? theme.BodyFont() : theme.MonospaceFont();
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_cellWidthPx > 0)
    {
        return;
    }

    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    m_cellWidthPx  = (std::max) (1, (int) (width / 8.0f + 0.5f));
    m_cellAdvance  = (std::max) (1.0f, width / 8.0f);
    m_cellHeightPx = (std::max) (1, (int) (height * m_lineSpacing + 0.5f));
    m_textTopPx    = (std::max) (0, (m_cellHeightPx - (int) (height + 0.5f)) / 2);
    m_measuredDpi  = m_scaler.GetDpi();

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
    DxuiFontHandle       font        = m_proportional ? theme.BodyFont() : theme.MonospaceFont();
    int                  last        = 0;
    float                clipLeft    = 0.0f;
    DxuiTextViewPainter  linePainter (*this);



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.TextViewBackground());

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);

    if (m_proportional && !m_metrics.IsMeasured (m_rows.size()))
    {
        MeasureCharacters (text, font);
    }

    last = (std::min) ((int) m_lines.size(), m_topLine + GetLineCap() + 1);

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
//  DxuiTextView::GetLinePart
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetLinePart (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom) const
{
    int  last = (int) row.cells.size() - 1;



    if (cell < 0 || cell > last)
    {
        return false;
    }

    if (cell == last)
    {
        outFlat     = GetCellBase (row, last) + line.start;
        outLength   = line.length;
        outCellFrom = line.start;
        return true;
    }

    if (!line.first)
    {
        return false;
    }

    outFlat     = GetCellBase (row, cell);
    outLength   = (int) row.cells[(size_t) cell].size();
    outCellFrom = 0;
    return true;
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
    return GetTextBaseLeft() - (int) std::lround ((float) m_leftColumn * m_cellAdvance);
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
    if (row < 0 || row >= (int) m_rows.size())
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
    int   rows   = (int) m_rows.size();
    int   page   = (std::max) (1, GetLineCap());
    int   last   = (int) m_lines.size() - 1;
    int   top    = 0;
    int   bottom = 0;
    int   from   = 0;
    int   next   = 0;
    bool  isMove = true;



    if (rows == 0 || last < 0)
    {
        return false;
    }

    top    = m_lines[(size_t) std::clamp (m_topLine, 0, last)].row;
    bottom = m_lines[(size_t) std::clamp (m_topLine + page - 1, 0, last)].row;
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
    return m_noWrap && !m_proportional && m_cellWidthPx > 0 && m_widestColumns > GetTextColumns (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLeftColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLeftColumn (int column)
{
    int  most = (std::max) (0, m_widestColumns - GetTextColumns (IsScrollbarVisible()));



    m_leftColumn = m_noWrap ? std::clamp (column, 0, most) : 0;
    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetColumnCap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetColumnCap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnCaps[column] = cells;
    }
    else
    {
        m_columnCaps.erase (column);
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetColumnGap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetColumnGap (int column, int cells)
{
    if (cells > 0)
    {
        m_columnGaps[column] = cells;
    }
    else
    {
        m_columnGaps.erase (column);
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetNoWrap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetNoWrap (bool noWrap)
{
    m_noWrap     = noWrap;
    m_leftColumn = noWrap ? m_leftColumn : 0;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFirstClippedColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFirstClippedColumn (int column)
{
    m_firstClipped = (column >= 0) ? column : -1;

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
//  DxuiTextView::BuildProportionalLines
//
//  As BuildLines, by the measured width of the characters rather than their
//  count.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::BuildProportionalLines (float widthPx)
{
    m_lines.clear();

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        int             cell   = (int) row.cells.size() - 1;
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        float           avail  = (std::max) (m_cellAdvance, widthPx - GetCellX (row, cell));
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

        if (row.picture != nullptr)
        {
            AddPictureLines (r, widthPx);
            continue;
        }

        do
        {
            int     take  = 1;
            size_t  space = std::wstring::npos;

            while (pos + take < length && GetCharsPx (r, cell, pos, pos + take + 1) <= avail)
            {
                take++;
            }

            take = (std::min) (take, length - pos);

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, (std::max) (take, 0), first });

            pos  += (std::max) (take, 1);
            first = false;
        }
        while (pos < length);
    }
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
    m_metrics.Measure (text, font);
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
    DxuiFontHandle  font = m_proportional ? theme.BodyFont() : theme.MonospaceFont();



    EnsureCellSize (text, theme);

    if (m_proportional && !m_metrics.IsMeasured (m_rows.size()) && m_cellWidthPx > 0)
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
    int  height = 0;



    if (m_cellHeightPx <= 0)
    {
        return 0;
    }

    for (int line = 0; line < (int) m_lines.size(); line++)
    {
        height += GetLineHeightPx (line);
    }

    return height + m_scaler.ToPx (s_kPadDip) * 2 + GetHorzBarPx();
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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::AddPictureLines
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::AddPictureLines (int row, float widthPx)
{
    int  count = DxuiTextRowLayout::GetPictureLines (*m_rows[(size_t) row].picture, widthPx, m_scaler.ToPxf (1.0f), m_cellHeightPx);



    for (int i = 0; i < count; i++)
    {
        m_lines.push_back (Line { row, 0, 0, i == 0 });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowStyles
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> DxuiTextView::GetRowStyles (const Row & row) const
{
    std::vector<uint8_t>  styles;



    for (const Span & span : row.spans)
    {
        uint8_t  bits   = (uint8_t) ((span.bold ? DxuiTextSpan::kStyleBold : 0) | (span.italic ? DxuiTextSpan::kStyleItalic : 0) | (span.underline ? DxuiTextSpan::kStyleUnderline : 0) | (span.inverse ? DxuiTextSpan::kStyleInverse : 0));
        int      base   = GetCellBase (row, span.cell);
        int      length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;

        if (bits == 0)
        {
            continue;
        }

        if (styles.empty())
        {
            styles.assign ((size_t) GetRowLength (row), 0);
        }

        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length; i++)
        {
            styles[(size_t) (base + i)] |= bits;
        }
    }

    return styles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineShift
//
//  How far a line of a centered or right-aligned row of one cell starts
//  from the left of the text, in pixels.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetLineShift (int lineIndex) const
{
    float  width = 0.0f;
    float  used  = 0.0f;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size())
    {
        return 0.0f;
    }

    const Line  & line = m_lines[(size_t) lineIndex];
    const Row   & row  = m_rows[(size_t) line.row];

    if (row.alignment == DxuiTextHAlign::Left || row.cells.size() != 1 || m_noWrap)
    {
        return 0.0f;
    }

    width = GetTextWidthPx (IsScrollbarVisible());
    used  = GetCharsPx (line.row, 0, line.start, line.start + line.length);

    return (row.alignment == DxuiTextHAlign::Right) ? (std::max) (0.0f, width - used) : (std::max) (0.0f, width - used) / 2.0f;
}
