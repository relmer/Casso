#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "Core/DxuiClipboard.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_rows     = std::move (rows);
    m_anchor   = Position();
    m_caret    = Position();
    m_topLine  = 0;
    m_dragging = false;

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
    m_cellPinned   = true;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineCap() const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;



    return (m_cellHeightPx > 0) ? (std::max) (1, height / m_cellHeightPx) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetTopLine (int line)
{
    int  maxTop = (std::max) (0, (int) m_lines.size() - GetLineCap());



    m_topLine = (std::max) (0, (std::min) (line, maxTop));

    SyncScrollbar();
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

    return (m_cellWidthPx > 0) ? (std::max) (1, width / m_cellWidthPx) : 1;
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



    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        column += m_columnCells[(size_t) c] + kColumnGapCells;
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
    m_lines.clear();
    m_columnCells.clear();

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
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

    BuildLines (GetTextColumns (false));

    if (IsScrollbarVisible())
    {
        BuildLines (GetTextColumns (true));
    }

    SetTopLine (m_topLine);
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

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        int             start  = GetCellStart (row, (int) row.cells.size() - 1);
        int             avail  = (std::max) (1, columns - start);
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

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
    int       x         = point.x - (int) m_boundsDip.left - pad;
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_lines.empty() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return pos;
    }

    lineIndex = m_topLine + ((y < 0) ? -1 : y / m_cellHeightPx);
    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) m_lines.size() - 1));
    column    = (x < 0) ? 0 : (x + m_cellWidthPx / 2) / m_cellWidthPx;

    const Line &  line = m_lines[(size_t) lineIndex];
    const Row &   row  = m_rows[(size_t) line.row];

    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < GetCellStart (row, c + 1))
        {
            pos.offset = GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - GetCellStart (row, last), line.length));

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
    m_caret  = m_rows.empty() ? Position() : Position { (int) m_rows.size() - 1, GetRowLength (m_rows.back()) };
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
    std::wstring  text  = (pos.row < (int) m_rows.size()) ? GetRowText (m_rows[(size_t) pos.row]) : std::wstring();
    int           first = (std::min) (pos.offset, (int) text.size());
    int           last  = first;



    while (first > 0 && text[(size_t) first - 1] != L' ' && text[(size_t) first - 1] != L'\t')
    {
        first--;
    }

    while (last < (int) text.size() && text[(size_t) last] != L' ' && text[(size_t) last] != L'\t')
    {
        last++;
    }

    m_anchor = Position { pos.row, first };
    m_caret  = Position { pos.row, last };
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



    for (int r = from.row; HasSelection() && r <= to.row && r < (int) m_rows.size(); r++)
    {
        std::wstring  text  = GetRowText (m_rows[(size_t) r]);
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
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_cellWidthPx > 0)
    {
        return;
    }

    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    m_cellWidthPx  = (std::max) (1, (int) (width / 8.0f + 0.5f));
    m_cellHeightPx = (std::max) (1, (int) (height + 0.5f));
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
    HRESULT         hr   = S_OK;
    DxuiFontHandle  font = theme.MonospaceFont();
    int             last = 0;



    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.ContentBackground());

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);
    last         = (std::min) ((int) m_lines.size(), m_topLine + GetLineCap() + 1);

    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                            (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (IsScrollbarVisible())
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
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
    HRESULT       hr        = S_OK;
    const Line &  line      = m_lines[(size_t) lineIndex];
    const Row &   row       = m_rows[(size_t) line.row];
    int           left      = (int) m_boundsDip.left + m_scaler.ToPx (s_kPadDip);
    int           y         = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + (lineIndex - m_topLine) * m_cellHeightPx;
    int           last      = (int) row.cells.size() - 1;
    Position      from      = (std::min) (m_anchor, m_caret);
    Position      to        = (std::max) (m_anchor, m_caret);
    bool          selected  = HasSelection() && from.row <= line.row && line.row <= to.row;
    int           selFrom   = (line.row == from.row) ? from.offset : 0;
    int           selTo     = (line.row == to.row)   ? to.offset   : INT_MAX;
    bool          finalLine = lineIndex + 1 == (int) m_lines.size() || m_lines[(size_t) lineIndex + 1].row != line.row;
    uint32_t      selArgb   = theme.SelectionBackground();



    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              (float) left, (float) y, (float) (kWarningCells * m_cellWidthPx), (float) m_cellHeightPx,
                              theme.WarningAccent(),
                              (float) m_cellHeightPx * 0.8f,
                              m_iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell   = row.cells[(size_t) c];
        int                   column = GetCellStart (row, c);

        if (selected)
        {
            FillSelectedRange (painter, y, column, GetCellBase (row, c), (int) cell.size(),
                               GetCellStart (row, c + 1) - column - (int) cell.size(), selFrom, selTo, selArgb);
        }

        hr = text.DrawString (cell.c_str(),
                              (float) (left + column * m_cellWidthPx), (float) y,
                              (float) (((int) cell.size() + 1) * m_cellWidthPx), (float) m_cellHeightPx,
                              theme.Foreground(), font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (last >= 0)
    {
        std::wstring  run    = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);
        int           column = GetCellStart (row, last);

        if (selected)
        {
            FillSelectedRange (painter, y, column, GetCellBase (row, last) + line.start, line.length,
                               (finalLine && line.row < to.row) ? 1 : 0, selFrom, selTo, selArgb);
        }

        hr = text.DrawString (run.c_str(),
                              (float) (left + column * m_cellWidthPx), (float) y,
                              (float) (((int) run.size() + 1) * m_cellWidthPx), (float) m_cellHeightPx,
                              theme.Foreground(), font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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

void DxuiTextView::FillSelectedRange (IDxuiPainter & painter, int y, int column, int flatStart, int count, int trailCells, int selFrom, int selTo, uint32_t argb) const
{
    int  left      = (int) m_boundsDip.left + m_scaler.ToPx (s_kPadDip);
    int  first     = (std::max) (selFrom, flatStart);
    int  end       = (std::min) (selTo, flatStart + count);
    int  separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect ((float) (left + (column + first - flatStart) * m_cellWidthPx), (float) y,
                          (float) ((end - first) * m_cellWidthPx), (float) m_cellHeightPx, argb);
    }

    if (trailCells > 0 && selFrom <= separator && separator < selTo)
    {
        painter.FillRect ((float) (left + (column + count) * m_cellWidthPx), (float) y,
                          (float) (trailCells * m_cellWidthPx), (float) m_cellHeightPx, argb);
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
    Position  pos;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        ScrollLines ((int) (-ev.wheelDelta * (float) kWheelLines));
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
        else if (ev.button == DxuiMouseButton::Left && IsScrollbarVisible() && m_vertScroll.HitTest (point.x, point.y))
        {
            handled = m_vertScroll.OnMouseDown (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left)
        {
            pos   = HitTest (point);
            twice = (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime() && pos == m_lastClick;

            m_lastClickMs = twice ? 0 : nowMs;
            m_lastClick   = pos;

            if (twice)
            {
                SelectWordAt (pos);
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
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseMove (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (m_dragging)
        {
            if (point.y < m_boundsDip.top)
            {
                ScrollLines (-1);
            }
            else if (point.y >= m_boundsDip.bottom)
            {
                ScrollLines (1);
            }

            m_caret = HitTest (point);
            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseUp();
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
    case VK_END:   SetTopLine (GetLineCount());  break;
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
        outEnabled = !m_rows.empty();
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
