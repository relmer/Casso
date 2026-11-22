#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiUnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_layout.SetRows (std::move (rows));
    m_selection.Reset();
    m_pictures.ClearOutline();
    m_scroll.ResetTopLine();
    m_scroll.ResetLeftColumn();

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

    m_scroll.ResetLeftColumn();

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
//  DxuiTextView::GetTextLeft
//
//  Where a line's first character is drawn, which is off the view's left edge
//  once a row that doesn't wrap has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextView::GetTextLeft() const
{
    return (float) ((int) m_boundsDip.left + m_scaler.ToPx (s_kPadDip)) - (float) m_scroll.GetLeftColumn() * m_layout.GetCellAdvance();
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
    const std::vector<DxuiTextLayout::Line> &  lines   = m_layout.GetLines();
    int                                        topLine = m_scroll.GetTopLine();
    DxuiTextLayout::Line                       top     = (topLine >= 0 && topLine < (int) lines.size()) ? lines[(size_t) topLine] : DxuiTextLayout::Line();
    float                                      perDip  = m_scaler.ToPxf (1.0f);



    m_layout.ClearLines();
    m_scroll.SetHorzScrollbarVisible (false);

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

        if (m_scroll.IsScrollbarVisible())
        {
            m_layout.BuildProportionalLines ((float) GetTextColumns (true) * m_layout.GetCellAdvance(), perDip);
        }
    }
    else
    {
        m_layout.BuildColumnCells();
        m_layout.BuildLines (GetTextColumns (false), perDip);

        if (m_scroll.IsScrollbarVisible())
        {
            m_layout.BuildLines (GetTextColumns (true), perDip);
        }

        //  The horizontal bar takes a line of height, which can bring on the
        //  vertical bar, which takes columns, so this is decided twice.
        for (int pass = 0; m_layout.IsNoWrap() && pass < 2; pass++)
        {
            m_scroll.SetHorzScrollbarVisible (m_layout.GetContentColumns() > GetTextColumns (m_scroll.IsScrollbarVisible()));
        }
    }

    //  A new wrap moves every line, so the top is the line that now has the
    //  text the old top started with.
    m_scroll.SetTopLine (m_layout.FindLine (top));
    m_scroll.SetLeftColumn (m_scroll.GetLeftColumn());
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
    int  bar   = m_scroll.IsHorzScrollbarVisible() ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;
    int  lineH = m_layout.GetCellHeightPx();



    return (lineH > 0) ? m_layout.GetLineCount() * lineH + m_scaler.ToPx (s_kPadDip) * 2 + bar : 0;
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

    last = (std::min) ((int) m_layout.GetLines().size(), m_scroll.GetTopLine() + m_scroll.GetLineCap() + 1);

    barW = m_scaler.ToPx (s_kScrollbarWidthDip);

    //  The selection fills go through the painter and the characters through
    //  the text renderer, so both are clipped: a line scrolled sideways
    //  otherwise fills past the view's edges.
    clipW = (float) (m_boundsDip.right - m_boundsDip.left - ((m_scroll.IsHorzScrollbarVisible() && m_scroll.IsScrollbarVisible()) ? barW : 0));
    clipH = (float) (m_boundsDip.bottom - m_boundsDip.top - (m_scroll.IsHorzScrollbarVisible() ? barW : 0));

    painter.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top, clipW, clipH);

    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top, clipW, clipH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int lineIndex = m_scroll.GetTopLine(); lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.PopClipRect();

    m_scroll.Paint (painter, theme);
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
    int                      y        = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + (lineIndex - m_scroll.GetTopLine()) * m_layout.GetCellHeightPx();
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = m_selection.GetFrom();
    Position                 to       = m_selection.GetTo();
    bool                     selected = m_selection.HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row);



    m_paintShift   = m_layout.GetLineShift (lineIndex, GetTextColumns (m_scroll.IsScrollbarVisible()));
    m_paintStyles  = DxuiTextLayout::GetRowStyles (row);
    m_paintPainter = &painter;

    //  A picture is drawn once, from its first line or from the top line
    //  when its first is scrolled above the view.
    if (row.picture != nullptr)
    {
        if (line.first || lineIndex == m_scroll.GetTopLine())
        {
            m_pictures.Paint (text, theme, line.row);
        }

        return;
    }

    if (row.rule && (lineIndex + 1 >= (int) m_layout.GetLines().size() || m_layout.GetLines()[(size_t) lineIndex + 1].row != line.row))
    {
        painter.FillRect (left, (float) (y + m_layout.GetCellHeightPx() - m_scaler.ToPx (1)), (float) GetTextColumns (m_scroll.IsScrollbarVisible()) * m_layout.GetCellAdvance(), (float) m_scaler.ToPx (1), theme.Divider());
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
    return m_layout.DoColumnsFit ((float) GetTextColumns (m_scroll.IsScrollbarVisible()) * m_layout.GetCellAdvance());
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
//  The wheel and a press on a scrollbar scroll, the wheel and a drag on a
//  map picture zoom and pan it, and a press and drag elsewhere select.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point   = ev.positionDip;
    bool   handled = false;
    bool   onBar   = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        //  The wheel over a zoomable picture zooms it.
        handled = (!ev.wheelHorizontal && !ev.shift && m_pictures.Zoom (point, ev.wheelDelta)) || m_scroll.OnWheel (ev);
        break;

    case DxuiMouseEventKind::Down:
        onBar = ev.button == DxuiMouseButton::Left && m_scroll.TryStartDrag (point, handled);

        if (ev.button == DxuiMouseButton::Right)
        {
            if (m_onContextMenu)
            {
                m_onContextMenu (point);
            }

            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && !onBar && m_pictures.TryStartPan (point))
        {
            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && !onBar)
        {
            m_selection.OnPress (point, ev.shift, (int64_t) GetTickCount64());
            handled = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_pictures.IsPanning())
        {
            m_pictures.Pan (point);
            handled = true;
        }
        else if (!m_scroll.TryDrag (point, handled) && m_selection.OnDrag (point))
        {
            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_pictures.IsPanning())
        {
            m_pictures.EndPan();
            handled = true;
        }
        else if (!m_scroll.TryEndDrag (handled) && m_selection.OnRelease())
        {
            handled = true;
        }

        break;

    default:
        break;
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
        outEnabled = m_selection.HasSelection();
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
        m_selection.CopySelection (m_hwnd);
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        m_selection.SelectAll();
    }

    return handled;
}
