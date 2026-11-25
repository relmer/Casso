#include "Pch.h"

#include "Widgets/TextView/DxuiTextPainter.h"
#include "Widgets/TextView/DxuiTextViewInterfaces.h"
#include "Widgets/TextView/DxuiTextPictures.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/IDxuiPainter.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainter::Paint
//
//  The lines in sight, and the one partly below them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPainter::Paint (
    IDxuiPainter          & painter,
    IDxuiTextRenderer     & text,
    const IDxuiTheme      & theme,
    const DxuiFontHandle  & font,
    const wchar_t         * iconFace,
    float                   textStrength)
{
    HRESULT       hr     = S_OK;
    const RECT &  bounds = m_geometry.GetViewBounds();
    int           barW   = m_geometry.GetBarWidthPx();
    int           last   = (std::min) (m_layout.GetLineCount(), m_viewport.GetTopLine() + m_viewport.GetLineCap() + 1);
    float         clipW  = 0.0f;
    float         clipH  = 0.0f;



    m_paintIconFace = iconFace;
    m_paintStrength = textStrength;

    //  The selection fills go through the painter and the characters through
    //  the text renderer, so both are clipped: a line scrolled sideways
    //  otherwise fills past the view's edges.
    clipW = (float) (bounds.right - bounds.left - ((m_viewport.IsHorzScrollbarVisible() && m_viewport.IsScrollbarVisible()) ? barW : 0));
    clipH = (float) (bounds.bottom - bounds.top - (m_viewport.IsHorzScrollbarVisible() ? barW : 0));

    painter.PushClipRect ((float) bounds.left, (float) bounds.top, clipW, clipH);

    hr = text.PushClipRect ((float) bounds.left, (float) bounds.top, clipW, clipH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int lineIndex = m_viewport.GetTopLine(); lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.PopClipRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainter::GetTextLeft
//
//  Where a line's first character is drawn, which is off the view's left edge
//  once a row that doesn't wrap has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextPainter::GetTextLeft() const
{
    return (float) m_geometry.GetTextOrigin().x - (float) m_viewport.GetLeftColumn() * m_layout.GetCellAdvance();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainter::PaintLine
//
//  The selection fill under a line's characters, then the characters. On a
//  row's first line every cell is drawn; on the lines after, only the run of
//  the last cell that wraps there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPainter::PaintLine (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font)
{
    HRESULT                  hr       = S_OK;
    const Line             & line     = m_layout.GetLines()[(size_t) lineIndex];
    const Row              & row      = m_layout.GetRows()[(size_t) line.row];
    float                    left     = GetTextLeft();
    int                      y        = (int) m_geometry.GetTextOrigin().y + (lineIndex - m_viewport.GetTopLine()) * m_layout.GetCellHeightPx();
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = m_selection.GetFrom();
    Position                 to       = m_selection.GetTo();
    bool                     selected = m_selection.HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row);



    m_paintShift   = m_layout.GetLineShift (lineIndex, m_geometry.GetTextColumns (m_viewport.IsScrollbarVisible()));
    m_paintStyles  = DxuiTextLayout::GetRowStyles (row);
    m_paintPainter = &painter;

    //  A picture is drawn once, from its first line or from the top line
    //  when its first is scrolled above the view.
    if (row.picture != nullptr)
    {
        if (line.first || lineIndex == m_viewport.GetTopLine())
        {
            m_pictures.Paint (text, theme, line.row);
        }

        return;
    }

    if (row.rule && (lineIndex + 1 >= (int) m_layout.GetLines().size() || m_layout.GetLines()[(size_t) lineIndex + 1].row != line.row))
    {
        painter.FillRect (left, (float) (y + m_layout.GetCellHeightPx() - m_geometry.GetScaler().ToPx (1)), (float) m_geometry.GetTextColumns (m_viewport.IsScrollbarVisible()) * m_layout.GetCellAdvance(), (float) m_geometry.GetScaler().ToPx (1), theme.Divider());
    }

    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              left, (float) y, (float) (DxuiTextLayout::kWarningCells * m_layout.GetCellAdvance()), (float) m_layout.GetCellHeightPx(),
                              theme.WarningAccent(),
                              (float) m_layout.GetCellHeightPx() * 0.8f,
                              m_paintIconFace,
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
//  DxuiTextPainter::DrawRun
//
//  A run of characters at `column` on a line, whose first is at `flatStart`
//  in the row's text. The selected part is drawn in the full foreground, the
//  rest in its span's color or the preview's text color, each stretch of one
//  color as one string; the face is fixed-width, so each stretch starts
//  exactly where its first character's cell does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPainter::DrawRun (IDxuiTextRenderer           & text,
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
    uint32_t  normal = m_layout.GetRows()[(size_t) row].heading ? theme.Accent() : label ? theme.ForegroundMuted() : DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_paintStrength);
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
            m_paintPainter->FillRect (x, (float) y + (float) m_layout.GetCellHeightPx() - (float) m_geometry.GetScaler().ToPx (2), width, (float) (std::max) (1, m_geometry.GetScaler().ToPx (1)), colorAt (start));
        }

        start = i;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainter::GetRowColors
//
//  The color of each character in the row's text, by its spans; zero, the
//  preview's text color, where no span covers it. Empty for a row with none.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiTextPainter::GetRowColors (const Row & row) const
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
//  DxuiTextPainter::FillSelectedRange
//
//  The selected part of `count` characters starting at `flatStart` in the
//  row's text and at `column` on the line, and the `trailCells` after them
//  when the separator that follows is selected too: the gap after a column,
//  or the end of a row whose selection goes on to the next.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextPainter::FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const
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
