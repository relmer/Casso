#include "Pch.h"

#include "DxuiTextViewPainter.h"
#include "DxuiTextLayout.h"
#include "DxuiTextRowLayout.h"
#include "DxuiTextSearch.h"
#include "DxuiTextViewFind.h"
#include "DxuiTextViewport.h"
#include "DxuiTextViewSelection.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::PaintLineGutter
//
//  A row's fill, which runs across the view on each of its lines, gutter and
//  all, over the focused row's selection or the highlighted row's hover,
//  the gutter band's fill on each of them too, and on its first line the
//  gutter's icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::PaintLineGutter (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex)
{
    HRESULT                 hr       = S_OK;
    const Line            & line     = m_layout.GetLines()[(size_t) lineIndex];
    const Row             & row      = m_layout.GetRows()[(size_t) line.row];
    const RECT            & bounds   = m_viewport.bounds;
    const DxuiDpiScaler   & scaler   = m_viewport.scaler;
    int                     left     = m_viewport.textBaseLeft;
    int                     y        = GetLineY (lineIndex);
    int                     height   = m_layout.GetLineHeightPx (lineIndex);
    int                     gutter   = m_viewport.gutterPx;
    float                   iconPx   = scaler.ToPxf ((float) m_viewport.gutterIconDip);
    float                   iconX    = (float) (left - gutter) + ((float) gutter - iconPx) * 0.5f;
    float                   width    = (float) (m_viewport.textAreaRight - bounds.left);
    float                   radius   = scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    float                   cellLeft = 0.0f;



    //  The focused row, under the row's fill, as a list draws its selected
    //  row.
    if (line.row == m_viewport.focusRow)
    {
        painter.FillRoundedRect ((float) bounds.left, (float) y, width, (float) height, radius, theme.ContentSelection());

        if (theme.ContentSelectionEdge() != 0)
        {
            painter.OutlineRoundedRect ((float) bounds.left, (float) y, width, (float) height, radius,
                                        (std::max) (1.0f, scaler.ToPxf (1.0f)), theme.ContentSelectionEdge());
        }
    }

    if (row.background != 0)
    {
        painter.FillRect ((float) bounds.left, (float) y, (float) (bounds.right - bounds.left), (float) height, row.background);
    }

    //  The row an in-place tip is up for, in the tip's color from its
    //  highlight cell to the text area's right, over the row's fill.
    if (line.first && line.row == m_viewport.highlightRow && GetCellLeftPx (line.row, m_viewport.highlightCell, cellLeft))
    {
        painter.FillRect (cellLeft, (float) y, (float) m_viewport.textAreaRight - cellLeft, (float) height, theme.ContentSelection());
    }

    if (row.gutterFill != 0 && m_viewport.bandWidthDip > 0)
    {
        painter.FillRect ((float) (bounds.left + scaler.ToPx (m_viewport.bandLeftDip)), (float) y, (float) scaler.ToPx (m_viewport.bandWidthDip),
                          (float) height, row.gutterFill);
    }

    if (!line.first || row.icon == nullptr || row.icon->bgraPremul.empty() || gutter <= 0)
    {
        return;
    }

    //  A glyph margin centers its icon on a set line.
    if (m_viewport.glyphCenterDip > 0.0f)
    {
        iconX = (float) bounds.left + scaler.ToPxf (m_viewport.glyphCenterDip) - iconPx * 0.5f;
    }

    //  An icon wider than it is tall runs on to the right of its first square.
    hr = text.DrawIconBitmap (row.icon->bgraPremul.data(), row.icon->width, row.icon->height,
                              iconX,
                              (float) y + ((float) m_layout.GetCellHeightPx() - iconPx) * 0.5f,
                              iconPx * (float) row.icon->width / (float) (std::max) (1, row.icon->height), iconPx);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::PaintLine
//
//  The selection fill under a line's characters, then the characters. On a
//  row's first line every cell is drawn; on the lines after, only the run of
//  the last cell that wraps there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::PaintLine (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font)
{
    HRESULT        hr      = S_OK;
    const Line   & line    = m_layout.GetLines()[(size_t) lineIndex];
    const Row    & row     = m_layout.GetRows()[(size_t) line.row];
    int            left    = m_viewport.textLeft;
    int            y       = GetLineY (lineIndex);
    int            last    = (int) row.cells.size() - 1;
    float          advance = m_layout.GetCellAdvance();
    int            height  = m_layout.GetCellHeightPx();
    LineMarks      marks;



    m_paintShift  = m_layout.GetLineShift (lineIndex, m_viewport.textWidthPx);
    m_paintStyles = DxuiTextLayout::GetRowStyles (row);
    m_painter     = &painter;

    //  A picture is drawn once, from its first line or from the top line
    //  when its first is scrolled above the view.
    if (row.picture != nullptr)
    {
        if (line.first || lineIndex == m_viewport.topLine)
        {
            PaintPicture (text, lineIndex);
        }

        return;
    }

    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              (float) left, (float) y, (float) (DxuiTextLayout::kWarningCells * advance), (float) height,
                              theme.WarningAccent(),
                              (float) height * 0.8f,
                              m_viewport.iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    marks = GetLineMarks (theme, line.row);

    FillSpans (painter, line, y);

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell  = row.cells[(size_t) c];
        float                 trail = m_layout.GetCellX (row, c + 1) - m_layout.GetCellX (row, c) - m_layout.GetCharsPx (line.row, c, 0, (int) cell.size());

        PaintCellRun (painter, text, theme, font, y, line.row, c, 0, DxuiTextLayout::GetCellBase (row, c), cell, trail, marks);
    }

    if (last >= 0)
    {
        std::wstring  run = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);

        PaintCellRun (painter, text, theme, font, y, line.row, last, line.start, DxuiTextLayout::GetCellBase (row, last) + line.start, run, 0.0f, marks);
    }

    if (line.row == m_viewport.hoverRow)
    {
        PaintLinkUnderline (painter, theme, lineIndex, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::GetLineY
//
//  The top of a drawn line, past the pad and counted from the top line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextViewPainter::GetLineY (int lineIndex) const
{
    return (int) m_viewport.bounds.top + m_viewport.padPx + m_layout.GetLineOffsetPx (m_viewport.topLine, lineIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::GetCellLeftPx
//
//  The left of a cell, drawn or not; false for a row not laid out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextViewPainter::GetCellLeftPx (int row, int cell, float & outX) const
{
    if (row < 0 || row >= (int) m_layout.GetRows().size())
    {
        return false;
    }

    outX = (float) m_viewport.textLeft + m_layout.GetCellX (m_layout.GetRows()[(size_t) row], cell);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::GetLineMarks
//
//  The selection's range on a row and the find's matched characters, with
//  the colors their runs are drawn in.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextViewPainter::LineMarks DxuiTextViewPainter::GetLineMarks (const IDxuiTheme & theme, int row) const
{
    const Row  & rowData = m_layout.GetRows()[(size_t) row];
    Position     from    = (std::min) (m_selection.GetAnchor(), m_selection.GetCaret());
    Position     to      = (std::max) (m_selection.GetAnchor(), m_selection.GetCaret());
    uint32_t     fore    = theme.Foreground()         | 0xFF000000u;
    uint32_t     back    = theme.TextViewBackground() | 0xFF000000u;
    uint32_t     current = m_find.GetCurrentArgb();
    LineMarks    marks;



    marks.selected = m_selection.HasSelection() && from.row <= row && row <= to.row;
    marks.selFrom  = (row == from.row) ? from.offset : 0;
    marks.selTo    = (row == to.row)   ? to.offset   : INT_MAX;
    marks.selArgb  = theme.SelectionBackground();
    marks.colors   = GetRowColors (rowData, theme);
    marks.litText  = DxuiTextSearch::GetFindHighlightText (theme);
    marks.litEdge  = (marks.litText == fore) ? theme.TextViewBackground() : theme.Foreground();

    //  The current match's own fill is opaque, so its characters take the
    //  color with more contrast on it; the other matches' is a tint, which
    //  leaves their text its own colors.
    if (current != 0)
    {
        marks.litText = (DxuiColor::ComputeContrastRatio (fore, current | 0xFF000000u) >= DxuiColor::ComputeContrastRatio (back, current | 0xFF000000u)) ? fore : back;
    }

    //  A highlighted match's characters are drawn in the color that reads on
    //  its fill.
    for (const FindMatch & match : m_find.GetHighlights())
    {
        bool  ownTint = (current != 0 && !m_find.IsCurrentMatch (match)) || (current == 0 && m_find.GetMatchArgb() != 0);

        if (match.start.row == row && !ownTint)
        {
            marks.lit.resize ((size_t) DxuiTextLayout::GetRowLength (rowData) + 1, false);

            for (int i = match.start.offset; i < match.start.offset + match.length && i < (int) marks.lit.size(); i++)
            {
                marks.lit[(size_t) i] = true;
            }
        }
    }

    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::FillSpans
//
//  A span's fill, under the selection's, on its row's first line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::FillSpans (IDxuiPainter & painter, const Line & line, int y) const
{
    const Row  & row  = m_layout.GetRows()[(size_t) line.row];
    float        left = (float) m_viewport.textLeft;



    for (const Span & span : row.spans)
    {
        if (line.first && span.background != 0 && span.cell < (int) row.cells.size())
        {
            painter.FillRect (left + m_layout.GetPartPx (line.row, span.cell, 0, span.start), (float) y,
                              m_layout.GetCharsPx (line.row, span.cell, span.start, span.start + span.length), (float) m_layout.GetCellHeightPx(), span.background);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::PaintCellRun
//
//  A run of a cell's characters on a line: its selection fill and the
//  `trailPx` after it, its find marks, then the characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::PaintCellRun (
    IDxuiPainter          & painter,
    IDxuiTextRenderer     & text,
    const IDxuiTheme      & theme,
    const DxuiFontHandle  & font,
    int                     y,
    int                     row,
    int                     cell,
    int                     cellFrom,
    int                     flatStart,
    const std::wstring    & chars,
    float                   trailPx,
    const LineMarks       & marks) const
{
    int  count = (int) chars.size();



    if (marks.selected)
    {
        FillSelectedRange (painter, y, row, cell, cellFrom, flatStart, count, trailPx, marks.selFrom, marks.selTo, marks.selArgb);
    }

    FillHighlights (painter, theme, y, row, cell, cellFrom, flatStart, count, marks.litEdge);
    DrawRun (text, theme, font, y, row, cell, cellFrom, flatStart, chars, marks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::PaintLinkUnderline
//
//  The hovered link's underline on one drawn line, under the part of the
//  link drawn on that line, at the bottom of the text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::PaintLinkUnderline (IDxuiPainter & painter, const IDxuiTheme & theme, int lineIndex, int y) const
{
    const Line  & line      = m_layout.GetLines()[(size_t) lineIndex];
    const Row   & row       = m_layout.GetRows()[(size_t) line.row];
    const Span  * span      = nullptr;
    int           flat      = 0;
    int           length    = 0;
    int           cellFrom  = 0;
    int           first     = 0;
    int           end       = 0;
    int           spanFlat  = 0;
    float         thickness = m_viewport.scaler.ToPxf (1.0f);



    if (m_viewport.hoverSpan < 0 || m_viewport.hoverSpan >= (int) row.spans.size())
    {
        return;
    }

    span = &row.spans[(size_t) m_viewport.hoverSpan];

    if (!span->isLink || !DxuiTextLayout::GetLinePart (row, line, span->cell, flat, length, cellFrom))
    {
        return;
    }

    spanFlat = DxuiTextLayout::GetCellBase (row, span->cell) + span->start;
    first    = (std::max) (spanFlat, flat);
    end      = (std::min) (spanFlat + span->length, flat + length);

    if (end <= first)
    {
        return;
    }

    painter.FillRect ((float) m_viewport.textLeft + m_layout.GetPartPx (line.row, span->cell, cellFrom, first - flat),
                      (float) (y + m_layout.GetCellHeightPx() - m_viewport.textTopPx) - thickness,
                      m_layout.GetCharsPx (line.row, span->cell, cellFrom + first - flat, cellFrom + end - flat),
                      thickness,
                      (span->argb != 0) ? span->argb : theme.TextColor (DxuiTextRole::Link).argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::DrawRun
//
//  A run of characters on a line, from `cellFrom` in its cell, whose first
//  is at `flatStart` in the row's text. The selected part is drawn in the
//  full foreground and the rest in its span's color or the view's text color:
//  a heading's in the accent and, in proportional text, every cell but the
//  last in the muted color. Each stretch of one color starts where its first
//  character is placed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::DrawRun (
    IDxuiTextRenderer     & text,
    const IDxuiTheme      & theme,
    const DxuiFontHandle  & font,
    int                     y,
    int                     row,
    int                     cell,
    int                     cellFrom,
    int                     flatStart,
    const std::wstring    & chars,
    const LineMarks       & marks) const
{
    const Row  & rowData = m_layout.GetRows()[(size_t) row];
    float        left    = (float) m_viewport.textLeft + m_paintShift;
    int          count   = (int) chars.size();
    int          first   = marks.selected ? std::clamp (marks.selFrom - flatStart, 0, count) : count;
    int          end     = marks.selected ? std::clamp (marks.selTo - flatStart, first, count) : count;
    bool         label   = m_layout.IsProportional() && cell + 1 < (int) rowData.cells.size();
    uint32_t     normal  = rowData.heading ? theme.Accent()
                         : label           ? theme.ForegroundMuted()
                                           : DxuiColor::Mix (theme.TextViewBackground(), theme.Foreground(), m_viewport.textStrength);
    int          start   = 0;
    auto         colorAt = [&] (int i)
                           {
                               size_t  flat = (size_t) (flatStart + i);

                               if (flat < marks.lit.size() && marks.lit[flat])
                               {
                                   return marks.litText;
                               }

                               if (i >= first && i < end)
                               {
                                   return theme.Foreground();
                               }

                               return (flat < marks.colors.size() && marks.colors[flat] != 0) ? marks.colors[flat] : normal;
                           };



    auto  styleAt = [&] (int i)
                    {
                        size_t  flat = (size_t) (flatStart + i);

                        return (flat < m_paintStyles.size()) ? m_paintStyles[flat] : (uint8_t) 0;
                    };



    //  Each run of one color and style is drawn on its own, from its first
    //  cell. An inverse run is filled with its color, its text in the
    //  background's, and an underlined run has a line under it.
    while (start < count)
    {
        uint32_t  argb  = colorAt (start);
        uint8_t   style = styleAt (start);
        int       stop  = start + 1;

        while (stop < count && colorAt (stop) == argb && styleAt (stop) == style)
        {
            stop++;
        }

        DrawStretch (text, theme, font,
                     left + m_layout.GetPartPx (row, cell, cellFrom, start), y,
                     m_layout.GetCharsPx (row, cell, cellFrom + start, cellFrom + stop),
                     chars.substr ((size_t) start, (size_t) (stop - start)), argb, style);

        start = stop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::DrawStretch
//
//  Characters of one color and style, `width` wide from `x`: filled in the
//  color with the text in the background's when inverse, and with a line
//  under them when underlined.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::DrawStretch (
    IDxuiTextRenderer     & text,
    const IDxuiTheme      & theme,
    const DxuiFontHandle  & font,
    float                   x,
    int                     y,
    float                   width,
    const std::wstring    & chars,
    uint32_t                argb,
    uint8_t                 style) const
{
    HRESULT                 hr      = S_OK;
    const DxuiDpiScaler   & scaler  = m_viewport.scaler;
    int                     height  = m_layout.GetCellHeightPx();
    int                     textTop = m_viewport.textTopPx;
    bool                    inverse = (style & DxuiTextSpan::kStyleInverse) != 0 && m_painter != nullptr;



    if (inverse)
    {
        m_painter->FillRect (x, (float) y, width, (float) height, argb);
    }

    hr = text.DrawString (chars.c_str(),
                          x, (float) (y + textTop),
                          width + m_layout.GetCellAdvance(), (float) (height - textTop),
                          inverse ? theme.TextViewBackground() : argb, font.sizeDip, font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                          DxuiTextRowLayout::GetWeight ((style & DxuiTextSpan::kStyleBold) != 0, (style & DxuiTextSpan::kStyleItalic) != 0), false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if ((style & DxuiTextSpan::kStyleUnderline) != 0 && m_painter != nullptr)
    {
        m_painter->FillRect (x, (float) y + (float) height - (float) scaler.ToPx (2), width, (float) (std::max) (1, scaler.ToPx (1)), argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::GetRowColors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiTextViewPainter::GetRowColors (const Row & row, const IDxuiTheme & theme) const
{
    std::vector<uint32_t>  colors;
    uint32_t               link   = theme.TextColor (DxuiTextRole::Link);



    if (row.spans.empty())
    {
        return colors;
    }

    colors.assign ((size_t) DxuiTextLayout::GetRowLength (row), 0u);

    for (const Span & span : row.spans)
    {
        int       base   = DxuiTextLayout::GetCellBase (row, span.cell);
        int       length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;
        uint32_t  argb   = (span.argb == 0 && span.isLink) ? link : span.argb;



        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length && argb != 0; i++)
        {
            colors[(size_t) (base + i)] = argb;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::FillHighlights
//
//  Each highlighted match on the row that falls in the `count` characters
//  starting at `flatStart`, the way the selection is filled. The selected
//  match, the one the search is at, is also outlined in `edge`, unless it
//  has a fill of its own to set it apart.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::FillHighlights (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int row, int cell, int cellFrom, int flatStart, int count, uint32_t edge) const
{
    float  left = (float) m_viewport.textLeft;



    for (const FindMatch & match : m_find.GetHighlights())
    {
        int  first = (std::max) (match.start.offset, flatStart);
        int  end   = (std::min) (match.start.offset + match.length, flatStart + count);

        if (match.start.row != row || end <= first)
        {
            continue;
        }

        FillSelectedRange (painter, y, row, cell, cellFrom, flatStart, count, 0.0f, match.start.offset, match.start.offset + match.length, m_find.GetFillOf (match, theme));

        if (m_find.GetCurrentArgb() == 0 && m_find.IsCurrentMatch (match))
        {
            painter.OutlineRect (left + m_layout.GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                                 m_layout.GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_layout.GetCellHeightPx(),
                                 m_viewport.scaler.ToPxf (2.0f), edge);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::FillSelectedRange
//
//  The selected part of `count` characters starting at `flatStart` in the
//  row's text and at `cellFrom` in the cell, and the `trailPx` after them
//  when the separator that follows is selected too: the gap after a column,
//  or the end of a row whose selection goes on to the next.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const
{
    float  left      = (float) m_viewport.textLeft;
    int    first     = (std::max) (selFrom, flatStart);
    int    end       = (std::min) (selTo, flatStart + count);
    int    separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect (left + m_layout.GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                          m_layout.GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_layout.GetCellHeightPx(), argb);
    }

    if (trailPx > 0.0f && selFrom <= separator && separator < selTo)
    {
        painter.FillRect (left + m_layout.GetPartPx (row, cell, cellFrom, count), (float) y,
                          trailPx, (float) m_layout.GetCellHeightPx(), argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter::PaintPicture
//
//  At the top of the picture's first line, which can be above the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextViewPainter::PaintPicture (IDxuiTextRenderer & text, int lineIndex) const
{
    const std::vector<Line>  & lines   = m_layout.GetLines();
    int                        first   = lineIndex;
    const Picture            & picture = *m_layout.GetRows()[(size_t) lines[(size_t) lineIndex].row].picture;
    float                      top     = 0.0f;



    while (first > 0 && lines[(size_t) first - 1].row == lines[(size_t) lineIndex].row)
    {
        first--;
    }

    top = (float) GetLineY (first);

    DxuiTextRowLayout::PaintPicture (text, picture, (float) m_viewport.textLeft, top, m_viewport.textWidthPx, m_viewport.scaler.ToPxf (1.0f));
}
