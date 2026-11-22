#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiClipboard.h"
#include "Core/DxuiUnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_layout.SetRows (std::move (rows));

    m_anchor     = Position();
    m_caret      = Position();
    m_topLine    = 0;
    m_dragging   = false;
    m_pictures.ClearOutline();

    m_leftColumn = 0;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetProportional
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetProportional (bool proportional)
{
    if (m_layout.SetProportional (proportional))
    {
        Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetNoWrap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetNoWrap (bool noWrap)
{
    if (!m_layout.SetNoWrap (noWrap))
    {
        return;
    }

    m_leftColumn = 0;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetCellSize
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetCellSize (int widthPx, int heightPx)
{
    m_layout.SetCellSize (widthPx, heightPx);

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineCap() const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2 - (m_horzVisible ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0);



    return (m_layout.GetCellHeightPx() > 0) ? (std::max) (1, height / m_layout.GetCellHeightPx()) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetTopLine (int line)
{
    int  maxTop = (std::max) (0, (int) m_layout.GetLines().size() - GetLineCap());



    m_topLine = (std::max) (0, (std::min) (line, maxTop));

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLeftColumn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLeftColumn (int column)
{
    int  maxLeft = m_horzVisible ? (std::max) (0, m_layout.GetContentColumns() - GetTextColumns (IsScrollbarVisible())) : 0;



    m_leftColumn = (std::max) (0, (std::min) (column, maxLeft));

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextLeft
//
//  Where a line's first character is drawn, which is off the view's left edge
//  once a row that doesn't wrap has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetTextLeft() const
{
    return (float) ((int) m_boundsDip.left + m_scaler.ToPx (s_kPadDip)) - (float) m_leftColumn * m_layout.GetCellAdvance();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextColumns
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextColumns (bool scrollbar) const
{
    int  width = (int) (m_boundsDip.right - m_boundsDip.left) - m_scaler.ToPx (s_kPadDip) * 2;



    width -= scrollbar ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;

    return (m_layout.GetCellWidthPx() > 0) ? (std::max) (1, (int) ((float) width / m_layout.GetCellAdvance())) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Rebuild
//
//  The drawn lines, laid out again narrower if they turn out to need the
//  scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Rebuild()
{
    const std::vector<DxuiTextLayout::Line> &  lines  = m_layout.GetLines();
    DxuiTextLayout::Line                       top    = (m_topLine >= 0 && m_topLine < (int) lines.size()) ? lines[(size_t) m_topLine] : DxuiTextLayout::Line();
    float                                      perDip = m_scaler.ToPxf (1.0f);



    m_layout.ClearLines();

    m_horzVisible = false;

    if (!m_layout.HasCells())
    {
        return;
    }

    //  Proportional text is laid out once its characters are measured, by
    //  their places rather than by cells.
    if (m_layout.IsProportional())
    {
        if (!m_layout.IsMeasured())
        {
            return;
        }

        m_layout.BuildProportionalLines ((float) GetTextColumns (false) * m_layout.GetCellAdvance(), perDip);

        if (IsScrollbarVisible())
        {
            m_layout.BuildProportionalLines ((float) GetTextColumns (true) * m_layout.GetCellAdvance(), perDip);
        }
    }
    else
    {
        m_layout.BuildColumnCells();
        m_layout.BuildLines (GetTextColumns (false), perDip);

        if (IsScrollbarVisible())
        {
            m_layout.BuildLines (GetTextColumns (true), perDip);
        }

        //  The horizontal bar takes a line of height, which can bring on the
        //  vertical bar, which takes columns, so this is decided twice.
        for (int pass = 0; m_layout.IsNoWrap() && pass < 2; pass++)
        {
            m_horzVisible = m_layout.GetContentColumns() > GetTextColumns (IsScrollbarVisible());
        }
    }

    //  A new wrap moves every line, so the top is the line that now has the
    //  text the old top started with.
    SetTopLine (m_layout.FindLine (top));
    SetLeftColumn (m_leftColumn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SyncScrollbar()
{
    int         barW = m_scaler.ToPx (s_kScrollbarWidthDip);
    bool        vert = IsScrollbarVisible();
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom - (m_horzVisible ? barW : 0) });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, (int) m_layout.GetLines().size() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, 1);
    m_horzScroll.SetTrack (RECT { m_boundsDip.left, m_boundsDip.bottom - barW, m_boundsDip.right - (vert ? barW : 0), m_boundsDip.bottom });

    info.nMax  = (std::max) (0, m_layout.GetContentColumns() - 1);
    info.nPage = (UINT) GetTextColumns (vert);
    info.nPos  = m_leftColumn;
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::HitTest
//
//  The character nearest a point, rounding to the closer side of a cell. A
//  point in the gap after a column lands at the end of that column's cell.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::Position DxuiTextView::HitTest (POINT point) const
{
    Position  pos;
    int       pad       = m_scaler.ToPx (s_kPadDip);
    int       y         = point.y - (int) m_boundsDip.top - pad;
    int       x         = point.x - (int) m_boundsDip.left - pad + (int) ((float) m_leftColumn * m_layout.GetCellAdvance());
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_layout.GetLines().empty() || m_layout.GetCellWidthPx() <= 0 || m_layout.GetCellHeightPx() <= 0)
    {
        return pos;
    }

    lineIndex = m_topLine + ((y < 0) ? -1 : y / m_layout.GetCellHeightPx());
    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) m_layout.GetLines().size() - 1));
    column    = (x < 0) ? 0 : (int) (((float) x + m_layout.GetCellAdvance() / 2.0f) / m_layout.GetCellAdvance());

    const Line &  line = m_layout.GetLines()[(size_t) lineIndex];
    const Row &   row  = m_layout.GetRows()[(size_t) line.row];

    x      -= (int) m_layout.GetLineShift (lineIndex, GetTextColumns (IsScrollbarVisible()));
    column  = (x < 0) ? 0 : (int) (((float) x + m_layout.GetCellAdvance() / 2.0f) / m_layout.GetCellAdvance());
    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    //  Proportional text: the cell whose column the point is in, then the
    //  character boundary nearest it.
    if (m_layout.IsProportional())
    {
        for (int c = 0; line.first && c < last; c++)
        {
            if ((float) x < m_layout.GetCellX (row, c + 1))
            {
                pos.offset = DxuiTextLayout::GetCellBase (row, c) + m_layout.GetNearestChar (line.row, c, 0, (int) row.cells[(size_t) c].size(), (float) x - m_layout.GetCellX (row, c));
                return pos;
            }
        }

        pos.offset = DxuiTextLayout::GetCellBase (row, last) + m_layout.GetNearestChar (line.row, last, line.start, line.start + line.length, (float) x - m_layout.GetCellX (row, last));
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = m_layout.GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < m_layout.GetCellStart (row, c + 1))
        {
            pos.offset = DxuiTextLayout::GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = DxuiTextLayout::GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - m_layout.GetCellStart (row, last), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Select
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Select (Position anchor, Position caret)
{
    m_anchor = anchor;
    m_caret  = caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectAll()
{
    m_anchor = Position();
    m_caret  = m_layout.GetRows().empty() ? Position() : Position { (int) m_layout.GetRows().size() - 1, DxuiTextLayout::GetRowLength (m_layout.GetRows().back()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ClearSelection()
{
    m_anchor = m_caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectWordAt
//
//  The run of characters around a position that holds no space or tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectWordAt (Position pos)
{
    GetWordBounds (pos, m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetWordBounds
//
//  The start and end of the run of characters around a position, bounded by
//  spaces and the tabs between cells.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetWordBounds (Position pos, Position & outFirst, Position & outLast) const
{
    std::wstring  text  = (pos.row < (int) m_layout.GetRows().size()) ? DxuiTextLayout::GetRowText (m_layout.GetRows()[(size_t) pos.row]) : std::wstring();
    int           size  = (int) text.size();
    int           first = (std::min) (pos.offset, size);
    int           last  = first;
    int           kind  = 0;



    //  The run is of whatever kind is under the position, or just before it at
    //  the end of the row, so a run of spaces is taken as one too.
    if (first < size)
    {
        kind = GetCharClass (text[(size_t) first]);
    }
    else if (first > 0)
    {
        kind = GetCharClass (text[(size_t) first - 1]);
    }

    while (first > 0 && GetCharClass (text[(size_t) first - 1]) == kind)
    {
        first--;
    }

    while (last < size && GetCharClass (text[(size_t) last]) == kind)
    {
        last++;
    }

    outFirst = Position { pos.row, first };
    outLast  = Position { pos.row, last };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetSelectionText
//
//  The selected characters, a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetSelectionText() const
{
    Position      from = (std::min) (m_anchor, m_caret);
    Position      to   = (std::max) (m_anchor, m_caret);
    std::wstring  out;



    for (int r = from.row; HasSelection() && r <= to.row && r < (int) m_layout.GetRows().size(); r++)
    {
        std::wstring  text  = DxuiTextLayout::GetRowText (m_layout.GetRows()[(size_t) r]);
        int           begin = (r == from.row) ? (std::min) (from.offset, (int) text.size()) : 0;
        int           end   = (r == to.row)   ? (std::min) (to.offset,   (int) text.size()) : (int) text.size();

        out += text.substr ((size_t) begin, (size_t) (std::max) (0, end - begin));

        if (r < to.row)
        {
            out += L"\r\n";
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::CopySelection
//
//  With nothing selected the clipboard is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::CopySelection() const
{
    if (HasSelection())
    {
        DxuiClipboard::SetText (m_hwnd, GetSelectionText());
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
    m_layout.ClearMeasurement();
    m_layout.ResetCellSize();

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

    if (!m_layout.IsCellPinned() && m_layout.GetMeasuredDpi() != scaler.GetDpi())
    {
        m_layout.ResetCellSize();
        m_layout.ClearMeasurement();
    }

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

        m_layout.MeasureCharacters (text, font);
        Rebuild();
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
    int  bar = m_horzVisible ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;



    return (m_layout.GetCellHeightPx() > 0) ? (int) m_layout.GetLines().size() * m_layout.GetCellHeightPx() + m_scaler.ToPx (s_kPadDip) * 2 + bar : 0;
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
    DxuiFontHandle  font = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();



    if (m_layout.MeasureCellSize (text, m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, m_scaler.GetDpi()))
    {
        Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr    = S_OK;
    DxuiFontHandle  font  = m_layout.IsProportional() ? theme.BodyFont() : theme.MonospaceFont();
    int             last  = 0;
    int             barW  = 0;
    float           clipW = 0.0f;
    float           clipH = 0.0f;



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.ContentBackground());

    if (m_layout.GetCellWidthPx() <= 0 || m_layout.GetCellHeightPx() <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);

    if (m_layout.IsProportional() && !m_layout.IsMeasured() && m_layout.GetCellWidthPx() > 0)
    {
        m_layout.MeasureCharacters (text, font);
        Rebuild();
    }

    last = (std::min) ((int) m_layout.GetLines().size(), m_topLine + GetLineCap() + 1);

    barW = m_scaler.ToPx (s_kScrollbarWidthDip);

    //  The selection fills go through the painter and the characters through
    //  the text renderer, so both are clipped: a line scrolled sideways
    //  otherwise fills past the view's edges.
    clipW = (float) (m_boundsDip.right - m_boundsDip.left - ((m_horzVisible && IsScrollbarVisible()) ? barW : 0));
    clipH = (float) (m_boundsDip.bottom - m_boundsDip.top - (m_horzVisible ? barW : 0));

    painter.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top, clipW, clipH);

    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top, clipW, clipH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.PopClipRect();

    if (IsScrollbarVisible() && !m_vertBarHidden)
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (m_horzVisible)
    {
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::PaintLine
//
//  The selection fill under a line's characters, then the characters. On a
//  row's first line every cell is drawn; on the lines after, only the run of
//  the last cell that wraps there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::PaintLine (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font)
{
    HRESULT                  hr       = S_OK;
    const Line             & line     = m_layout.GetLines()[(size_t) lineIndex];
    const Row              & row      = m_layout.GetRows()[(size_t) line.row];
    float                    left     = GetTextLeft();
    int                      y        = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + (lineIndex - m_topLine) * m_layout.GetCellHeightPx();
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = (std::min) (m_anchor, m_caret);
    Position                 to       = (std::max) (m_anchor, m_caret);
    bool                     selected = HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row);



    m_paintShift   = m_layout.GetLineShift (lineIndex, GetTextColumns (IsScrollbarVisible()));
    m_paintStyles  = DxuiTextLayout::GetRowStyles (row);
    m_paintPainter = &painter;

    //  A picture is drawn once, from its first line or from the top line
    //  when its first is scrolled above the view.
    if (row.picture != nullptr)
    {
        if (line.first || lineIndex == m_topLine)
        {
            m_pictures.Paint (text, theme, line.row);
        }

        return;
    }

    if (row.rule && (lineIndex + 1 >= (int) m_layout.GetLines().size() || m_layout.GetLines()[(size_t) lineIndex + 1].row != line.row))
    {
        painter.FillRect (left, (float) (y + m_layout.GetCellHeightPx() - m_scaler.ToPx (1)), (float) GetTextColumns (IsScrollbarVisible()) * m_layout.GetCellAdvance(), (float) m_scaler.ToPx (1), theme.Divider());
    }

    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              left, (float) y, (float) (DxuiTextLayout::kWarningCells * m_layout.GetCellAdvance()), (float) m_layout.GetCellHeightPx(),
                              theme.WarningAccent(),
                              (float) m_layout.GetCellHeightPx() * 0.8f,
                              m_iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell  = row.cells[(size_t) c];
        int                   count = (int) cell.size();

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, c, 0, DxuiTextLayout::GetCellBase (row, c), count,
                               m_layout.GetCellX (row, c + 1) - m_layout.GetCellX (row, c) - m_layout.GetCharsPx (line.row, c, 0, count), selFrom, selTo, selArgb);
        }

        DrawRun (text, theme, font, y, line.row, c, 0, DxuiTextLayout::GetCellBase (row, c), cell, selected, selFrom, selTo, colors);
    }

    if (last >= 0)
    {
        std::wstring  run = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, last, line.start, DxuiTextLayout::GetCellBase (row, last) + line.start, line.length,
                               0.0f, selFrom, selTo, selArgb);
        }

        DrawRun (text, theme, font, y, line.row, last, line.start, DxuiTextLayout::GetCellBase (row, last) + line.start, run, selected, selFrom, selTo, colors);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::DoColumnsFit
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::DoColumnsFit() const
{
    return m_layout.DoColumnsFit ((float) GetTextColumns (IsScrollbarVisible()) * m_layout.GetCellAdvance());
}
////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::DrawRun
//
//  A run of characters at `column` on a line, whose first is at `flatStart`
//  in the row's text. The selected part is drawn in the full foreground, the
//  rest in its span's color or the preview's text color, each stretch of one
//  color as one string; the face is fixed-width, so each stretch starts
//  exactly where its first character's cell does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::DrawRun (IDxuiTextRenderer           & text,
                            const IDxuiTheme            & theme,
                            const DxuiFontHandle        & font,
                            int                           y,
                            int                           row,
                            int                           cell,
                            int                           cellFrom,
                            int                           flatStart,
                            const std::wstring          & chars,
                            bool                          selected,
                            int                           selFrom,
                            int                           selTo,
                            const std::vector<uint32_t> & colors) const
{
    HRESULT   hr     = S_OK;
    float     left   = GetTextLeft() + m_layout.GetCellX (m_layout.GetRows()[(size_t) row], cell) + m_paintShift;
    int       count  = (int) chars.size();
    int       first  = selected ? std::clamp (selFrom - flatStart, 0, count) : count;
    int       end    = selected ? std::clamp (selTo - flatStart, first, count) : count;
    bool      label  = m_layout.IsProportional() && cell + 1 < (int) m_layout.GetRows()[(size_t) row].cells.size();
    uint32_t  normal = m_layout.GetRows()[(size_t) row].heading ? theme.Accent() : label ? theme.ForegroundMuted() : DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_textStrength);
    int       start  = 0;



    auto  colorAt = [&] (int i)
    {
        size_t  flat = (size_t) (flatStart + i);

        return (i >= first && i < end)                    ? theme.Foreground()
             : (flat < colors.size() && colors[flat] != 0) ? colors[flat]
                                                            : normal;
    };

    auto  styleAt = [&] (int i)
    {
        size_t  flat = (size_t) (flatStart + i);

        return (flat < m_paintStyles.size()) ? m_paintStyles[flat] : (uint8_t) 0;
    };

    for (int i = 1; i <= count; i++)
    {
        uint8_t         style   = styleAt (start);
        float           x       = left + m_layout.GetCharsPx (row, cell, cellFrom, cellFrom + start);
        float           width   = m_layout.GetCharsPx (row, cell, cellFrom + start, cellFrom + i);
        bool            inverse = (style & DxuiTextLayout::kStyleInverse) != 0 && m_paintPainter != nullptr;

        if (i < count && colorAt (i) == colorAt (start) && styleAt (i) == style)
        {
            continue;
        }

        if (inverse)
        {
            m_paintPainter->FillRect (x, (float) y, width, (float) m_layout.GetCellHeightPx(), colorAt (start));
        }

        hr = text.DrawString (chars.substr ((size_t) start, (size_t) (i - start)).c_str(),
                              x, (float) y,
                              width + m_layout.GetCellAdvance(), (float) m_layout.GetCellHeightPx(),
                              inverse ? theme.ContentBackground() : colorAt (start), font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiTextLayout::GetWeight ((style & DxuiTextLayout::kStyleBold) != 0, (style & DxuiTextLayout::kStyleItalic) != 0), false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if ((style & DxuiTextLayout::kStyleUnderline) && m_paintPainter != nullptr)
        {
            m_paintPainter->FillRect (x, (float) y + (float) m_layout.GetCellHeightPx() - (float) m_scaler.ToPx (2), width, (float) (std::max) (1, m_scaler.ToPx (1)), colorAt (start));
        }

        start = i;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowColors
//
//  The color of each character in the row's text, by its spans; zero, the
//  preview's text color, where no span covers it. Empty for a row with none.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiTextView::GetRowColors (const Row & row) const
{
    std::vector<uint32_t>  colors;



    if (row.spans.empty())
    {
        return colors;
    }

    colors.assign ((size_t) DxuiTextLayout::GetRowLength (row), 0u);

    for (const Span & span : row.spans)
    {
        int  base   = DxuiTextLayout::GetCellBase (row, span.cell);
        int  length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;

        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length; i++)
        {
            colors[(size_t) (base + i)] = span.argb;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FillSelectedRange
//
//  The selected part of `count` characters starting at `flatStart` in the
//  row's text and at `column` on the line, and the `trailCells` after them
//  when the separator that follows is selected too: the gap after a column,
//  or the end of a row whose selection goes on to the next.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const
{
    float  left      = GetTextLeft() + m_layout.GetCellX (m_layout.GetRows()[(size_t) row], cell) + m_paintShift;
    int    first     = (std::max) (selFrom, flatStart);
    int    end       = (std::min) (selTo, flatStart + count);
    int    separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect (left + m_layout.GetCharsPx (row, cell, cellFrom, cellFrom + first - flatStart), (float) y,
                          m_layout.GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_layout.GetCellHeightPx(), argb);
    }

    if (trailPx > 0.0f && selFrom <= separator && separator < selTo)
    {
        painter.FillRect (left + m_layout.GetCharsPx (row, cell, cellFrom, cellFrom + count), (float) y,
                          trailPx, (float) m_layout.GetCellHeightPx(), argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnMouse
//
//  A press places both ends of the selection, or moves only the caret with
//  Shift, and a drag moves the caret, scrolling past either edge. A second
//  press at the same place within the double-click time selects the word.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT     point   = ev.positionDip;
    int64_t   nowMs   = (int64_t) GetTickCount64();
    bool      handled = false;
    bool      twice   = false;
    bool      thrice  = false;
    Position  pos;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        //  The wheel over a zoomable picture zooms it.
        if (!ev.wheelHorizontal && !ev.shift && m_pictures.Zoom (point, ev.wheelDelta))
        {
            handled = true;
            break;
        }

        //  A horizontal wheel, or Shift with the wheel, scrolls sideways.
        if (m_horzVisible && (ev.wheelHorizontal || ev.shift))
        {
            m_wheelColumns += (ev.wheelHorizontal ? ev.wheelDelta : -ev.wheelDelta) * (float) kWheelColumns;
            ScrollColumns ((int) m_wheelColumns);
            m_wheelColumns -= (float) (int) m_wheelColumns;
            handled = true;
            break;
        }

        //  A precision touchpad sends part notches, which add up until they
        //  move a whole line.
        m_wheelLines += -ev.wheelDelta * (float) kWheelLines;
        ScrollLines ((int) m_wheelLines);
        m_wheelLines -= (float) (int) m_wheelLines;
        handled = true;
        break;

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Right)
        {
            if (m_onContextMenu)
            {
                m_onContextMenu (point);
            }

            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && IsOverVertScrollbar (point))
        {
            handled = m_vertScroll.OnMouseDown (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left && IsOverHorzScrollbar (point))
        {
            handled = m_horzScroll.OnMouseDown (point.x, point.y);
            SetLeftColumn (m_horzScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left && m_pictures.TryStartPan (point))
        {
            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left)
        {
            pos    = HitTest (point);
            twice  = (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime() && pos == m_lastClick;
            thrice = twice && m_clicks == 2;

            m_clicks      = twice ? m_clicks + 1 : 1;
            m_lastClickMs = nowMs;
            m_lastClick   = pos;

            m_wordDrag = twice && !thrice;

            if (thrice)
            {
                //  A triple-click selects everything, as in a browser's text.
                SelectAll();
                m_clicks   = 0;
                m_dragging = false;
            }
            else if (twice)
            {
                //  A double-click selects the word, and dragging on from it
                //  takes whole words until the button comes up.
                GetWordBounds (pos, m_wordFirst, m_wordLast);

                m_anchor   = m_wordFirst;
                m_caret    = m_wordLast;
                m_dragging = true;
            }
            else if (ev.shift)
            {
                m_caret    = pos;
                m_dragging = true;
            }
            else
            {
                m_anchor   = pos;
                m_caret    = pos;
                m_dragging = true;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_pictures.IsPanning())
        {
            m_pictures.Pan (point);
            handled = true;
        }
        else if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseMove (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (m_horzScroll.IsDragging())
        {
            handled = m_horzScroll.OnMouseMove (point.x, point.y);
            SetLeftColumn (m_horzScroll.GetScrollPos());
        }
        else if (m_dragging)
        {
            if (point.x < m_boundsDip.left)
            {
                ScrollColumns (-1);
            }
            else if (point.x >= m_boundsDip.right)
            {
                ScrollColumns (1);
            }

            if (point.y < m_boundsDip.top)
            {
                ScrollLines (-1);
            }
            else if (point.y >= m_boundsDip.bottom)
            {
                ScrollLines (1);
            }

            pos = HitTest (point);

            if (m_wordDrag)
            {
                Position  first;
                Position  last;

                GetWordBounds (pos, first, last);

                m_anchor = (pos < m_wordFirst) ? m_wordLast : m_wordFirst;
                m_caret  = (pos < m_wordFirst) ? first      : (std::max) (last, m_wordLast);
            }
            else
            {
                m_caret = pos;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_pictures.IsPanning())
        {
            m_pictures.EndPan();
            handled = true;
        }
        else if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseUp();
        }
        else if (m_horzScroll.IsDragging())
        {
            handled = m_horzScroll.OnMouseUp();
        }
        else if (m_dragging)
        {
            m_dragging = false;
            handled    = true;
        }

        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnKey
//
//  The arrow and page keys scroll; there is no caret to move in read-only text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = ev.kind == DxuiKeyEventKind::Down;



    if (!handled)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_UP:    ScrollLines (-1);             break;
    case VK_DOWN:  ScrollLines (1);              break;
    case VK_PRIOR: ScrollLines (-GetLineCap());  break;
    case VK_NEXT:  ScrollLines (GetLineCap());   break;
    case VK_HOME:  SetTopLine (0);               break;
    case VK_END:   SetTopLine (m_layout.GetLineCount());  break;
    case VK_LEFT:  ScrollColumns (-1);           handled = m_horzVisible; break;
    case VK_RIGHT: ScrollColumns (1);            handled = m_horzVisible; break;
    default:       handled = false;              break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    bool  handled = false;



    if (command == DxuiStandardCommand::Copy)
    {
        outEnabled = HasSelection();
        handled    = true;
    }
    else if (command == DxuiStandardCommand::SelectAll)
    {
        outEnabled = !m_layout.GetRows().empty();
        handled    = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        CopySelection();
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        SelectAll();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::SetScrollbarHover (POINT pt)
{
    bool  vert = m_vertScroll.SetHover (IsOverVertScrollbar (pt), pt);
    bool  horz = m_horzScroll.SetHover (IsOverHorzScrollbar (pt), pt);



    return vert || horz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TickScrollbars (int64_t nowMs)
{
    bool  vert = m_vertScroll.Tick (nowMs);
    bool  horz = m_horzScroll.Tick (nowMs);



    return vert || horz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiTextView::GetScrollbarMenu (POINT pt)
{
    m_vertScroll.SetOnScroll ([this] (int, int pos) { SetTopLine (pos); });
    m_horzScroll.SetOnScroll ([this] (int, int pos) { SetLeftColumn (pos); });

    if (!IsOverScrollbar (pt))
    {
        return {};
    }

    SyncScrollbar();

    return IsOverVertScrollbar (pt) ? m_vertScroll.BuildContextMenu (pt.x, pt.y) : m_horzScroll.BuildContextMenu (pt.x, pt.y);
}
