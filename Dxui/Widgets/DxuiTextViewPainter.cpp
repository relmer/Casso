#include "Pch.h"

#include "DxuiTextViewPainter.h"
#include "DxuiTextRowLayout.h"
#include "DxuiTextSearch.h"
#include "DxuiTextView.h"
#include "DxuiTextViewFind.h"
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
    HRESULT       hr       = S_OK;
    const Line  & line     = m_view.GetLayoutLines()[(size_t) lineIndex];
    const Row   & row      = m_view.GetLayoutRows()[(size_t) line.row];
    int           left     = m_view.GetTextBaseLeft();
    int           y        = (int) m_view.GetLayoutBounds().top + m_look.padPx + m_view.GetLineOffsetPx (lineIndex);
    int           gutter   = m_view.GetGutterPx();
    float         iconPx   = m_view.GetLayoutScaler().ToPxf ((float) m_look.gutterIconDip);
    float         iconX    = (float) (left - gutter) + ((float) gutter - iconPx) * 0.5f;
    float         width    = (float) (m_view.GetTextAreaRightPx() - m_view.GetLayoutBounds().left);
    float         radius   = m_view.GetLayoutScaler().ToPxf (DxuiTheme::kCornerRadiusDip);
    float         cellLeft = 0.0f;



    //  The focused row, under the row's fill, as a list draws its selected
    //  row.
    if (m_view.IsRowSelectionShown (line.row))
    {
        painter.FillRoundedRect ((float) m_view.GetLayoutBounds().left, (float) y, width, (float) m_view.GetLineHeightPx (lineIndex), radius, theme.ContentSelection());

        if (theme.ContentSelectionEdge() != 0)
        {
            painter.OutlineRoundedRect ((float) m_view.GetLayoutBounds().left, (float) y, width, (float) m_view.GetLineHeightPx (lineIndex), radius,
                                        (std::max) (1.0f, m_view.GetLayoutScaler().ToPxf (1.0f)), theme.ContentSelectionEdge());
        }
    }

    if (row.background != 0)
    {
        painter.FillRect ((float) m_view.GetLayoutBounds().left, (float) y, (float) (m_view.GetLayoutBounds().right - m_view.GetLayoutBounds().left), (float) m_view.GetLineHeightPx (lineIndex),
                          row.background);
    }

    //  The row an in-place tip is up for, in the tip's color from its
    //  highlight cell to the text area's right, over the row's fill.
    if (line.first && line.row == m_look.highlightRow && m_view.GetCellLeftPx (line.row, m_look.highlightCell, cellLeft))
    {
        painter.FillRect (cellLeft, (float) y, (float) m_view.GetTextAreaRightPx() - cellLeft, (float) m_view.GetLineHeightPx (lineIndex), theme.ContentSelection());
    }

    if (row.gutterFill != 0 && m_look.bandWidthDip > 0)
    {
        painter.FillRect ((float) (m_view.GetLayoutBounds().left + m_view.GetLayoutScaler().ToPx (m_look.bandLeftDip)), (float) y, (float) m_view.GetLayoutScaler().ToPx (m_look.bandWidthDip),
                          (float) m_view.GetLineHeightPx (lineIndex), row.gutterFill);
    }

    if (!line.first || row.icon == nullptr || row.icon->bgraPremul.empty() || gutter <= 0)
    {
        return;
    }

    //  A glyph margin centers its icon on a set line.
    if (m_look.glyphCenterDip > 0.0f)
    {
        iconX = (float) m_view.GetLayoutBounds().left + m_view.GetLayoutScaler().ToPxf (m_look.glyphCenterDip) - iconPx * 0.5f;
    }

    //  An icon wider than it is tall runs on to the right of its first square.
    hr = text.DrawIconBitmap (row.icon->bgraPremul.data(), row.icon->width, row.icon->height,
                              iconX,
                              (float) y + ((float) m_look.cellHeightPx - iconPx) * 0.5f,
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
    HRESULT                  hr       = S_OK;
    const Line             & line     = m_view.GetLayoutLines()[(size_t) lineIndex];
    const Row              & row      = m_view.GetLayoutRows()[(size_t) line.row];
    int                      left     = m_view.GetTextLeft();
    int                      y        = (int) m_view.GetLayoutBounds().top + m_look.padPx + m_view.GetLineOffsetPx (lineIndex);
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = (std::min) (m_view.GetLayoutAnchor(), m_view.GetLayoutCaret());
    Position                 to       = (std::max) (m_view.GetLayoutAnchor(), m_view.GetLayoutCaret());
    bool                     selected = m_view.HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row, theme);
    int                      gutter   = m_view.GetGutterPx();
    uint32_t                 litText  = DxuiTextSearch::GetFindHighlightText (theme);
    uint32_t                 litEdge  = (litText == (theme.Foreground() | 0xFF000000u)) ? theme.TextViewBackground() : theme.Foreground();
    uint32_t                 fore     = theme.Foreground()         | 0xFF000000u;
    uint32_t                 back     = theme.TextViewBackground() | 0xFF000000u;
    std::vector<bool>        lit;



    m_paintShift  = m_view.GetLineShift (lineIndex);
    m_paintStyles = m_view.GetRowStyles (row);
    m_painter     = &painter;

    //  A picture is drawn once, from its first line or from the top line
    //  when its first is scrolled above the view.
    if (row.picture != nullptr)
    {
        if (line.first || lineIndex == m_look.topLine)
        {
            PaintPicture (text, lineIndex);
        }

        return;
    }

    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              (float) left, (float) y, (float) (DxuiTextView::kWarningCells * m_look.cellAdvance), (float) m_look.cellHeightPx,
                              theme.WarningAccent(),
                              (float) m_look.cellHeightPx * 0.8f,
                              m_look.iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The current match's own fill is opaque, so its characters take the
    //  color with more contrast on it; the other matches' is a tint, which
    //  leaves their text its own colors.
    if (m_view.GetLayoutFind().GetCurrentArgb() != 0)
    {
        litText = (DxuiColor::ComputeContrastRatio (fore, m_view.GetLayoutFind().GetCurrentArgb() | 0xFF000000u) >= DxuiColor::ComputeContrastRatio (back, m_view.GetLayoutFind().GetCurrentArgb() | 0xFF000000u)) ? fore : back;
    }

    //  A highlighted match's characters are drawn in the color that reads on
    //  its fill.
    for (const FindMatch & match : m_view.GetLayoutFind().GetHighlights())
    {
        bool  ownTint = (m_view.GetLayoutFind().GetCurrentArgb() != 0 && !m_view.GetLayoutFind().IsCurrentMatch (match)) || (m_view.GetLayoutFind().GetCurrentArgb() == 0 && m_view.GetLayoutFind().GetMatchArgb() != 0);

        if (match.start.row == line.row && !ownTint)
        {
            lit.resize ((size_t) m_view.GetRowLength (row) + 1, false);

            for (int i = match.start.offset; i < match.start.offset + match.length && i < (int) lit.size(); i++)
            {
                lit[(size_t) i] = true;
            }
        }
    }

    //  A span's fill, under the selection's.
    for (const Span & span : row.spans)
    {
        if (line.first && span.background != 0 && span.cell < (int) row.cells.size())
        {
            painter.FillRect ((float) left + m_view.GetPartPx (line.row, span.cell, 0, span.start), (float) y,
                              m_view.GetCharsPx (line.row, span.cell, span.start, span.start + span.length), (float) m_look.cellHeightPx, span.background);
        }
    }

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell  = row.cells[(size_t) c];
        int                   count = (int) cell.size();

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, c, 0, m_view.GetCellBase (row, c), count,
                               m_view.GetCellX (row, c + 1) - m_view.GetCellX (row, c) - m_view.GetCharsPx (line.row, c, 0, count), selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, line.row, c, 0, m_view.GetCellBase (row, c), count, litEdge);
        DrawRun (text, theme, font, y, line.row, c, 0, m_view.GetCellBase (row, c), cell, selected, selFrom, selTo, colors, lit, litText);
    }

    if (last >= 0)
    {
        std::wstring  run = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);

        if (selected)
        {
            FillSelectedRange (painter, y, line.row, last, line.start, m_view.GetCellBase (row, last) + line.start, line.length,
                               0.0f, selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, line.row, last, line.start, m_view.GetCellBase (row, last) + line.start, line.length, litEdge);
        DrawRun (text, theme, font, y, line.row, last, line.start, m_view.GetCellBase (row, last) + line.start, run, selected, selFrom, selTo, colors, lit, litText);
    }

    if (line.row == m_look.hoverRow)
    {
        PaintLinkUnderline (painter, theme, lineIndex, y);
    }
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
    const Line  & line      = m_view.GetLayoutLines()[(size_t) lineIndex];
    const Row   & row       = m_view.GetLayoutRows()[(size_t) line.row];
    const Span  * span      = nullptr;
    int           flat      = 0;
    int           length    = 0;
    int           cellFrom  = 0;
    int           first     = 0;
    int           end       = 0;
    int           spanFlat  = 0;
    float         thickness = m_view.GetLayoutScaler().ToPxf (1.0f);



    if (m_look.hoverSpan < 0 || m_look.hoverSpan >= (int) row.spans.size())
    {
        return;
    }

    span = &row.spans[(size_t) m_look.hoverSpan];

    if (!span->isLink || !m_view.GetLinePart (row, line, span->cell, flat, length, cellFrom))
    {
        return;
    }

    spanFlat = m_view.GetCellBase (row, span->cell) + span->start;
    first    = (std::max) (spanFlat, flat);
    end      = (std::min) (spanFlat + span->length, flat + length);

    if (end <= first)
    {
        return;
    }

    painter.FillRect ((float) m_view.GetTextLeft() + m_view.GetPartPx (line.row, span->cell, cellFrom, first - flat),
                      (float) (y + m_look.cellHeightPx - m_look.textTopPx) - thickness,
                      m_view.GetCharsPx (line.row, span->cell, cellFrom + first - flat, cellFrom + end - flat),
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

void DxuiTextViewPainter::DrawRun (IDxuiTextRenderer    & text,
                            const IDxuiTheme     & theme,
                            const DxuiFontHandle & font,
                            int                    y,
                            int                    row,
                            int                    cell,
                            int                    cellFrom,
                            int                    flatStart,
                            const std::wstring   & chars,
                            bool                   selected,
                            int                    selFrom,
                            int                    selTo,
                            const std::vector<uint32_t> & colors,
                            const std::vector<bool>     & lit,
                            uint32_t                      litArgb) const
{
    HRESULT   hr     = S_OK;
    float     left   = (float) m_view.GetTextLeft() + m_paintShift;
    int       count  = (int) chars.size();
    int       first  = selected ? std::clamp (selFrom - flatStart, 0, count) : count;
    int       end    = selected ? std::clamp (selTo - flatStart, first, count) : count;
    bool      label  = m_look.proportional && cell + 1 < (int) m_view.GetLayoutRows()[(size_t) row].cells.size();
    uint32_t  normal = m_view.GetLayoutRows()[(size_t) row].heading ? theme.Accent()
                     : label                         ? theme.ForegroundMuted()
                                                     : DxuiColor::Mix (theme.TextViewBackground(), theme.Foreground(), m_look.textStrength);
    int       start  = 0;
    auto      colorAt = [&] (int i)
                        {
                            size_t  flat = (size_t) (flatStart + i);

                            if (flat < lit.size() && lit[flat])
                            {
                                return litArgb;
                            }

                            if (i >= first && i < end)
                            {
                                return theme.Foreground();
                            }

                            return (flat < colors.size() && colors[flat] != 0) ? colors[flat] : normal;
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
        uint32_t  argb    = colorAt (start);
        uint8_t   style   = styleAt (start);
        int       stop    = start + 1;
        float     x       = 0.0f;
        float     width   = 0.0f;
        bool      inverse = false;

        while (stop < count && colorAt (stop) == argb && styleAt (stop) == style)
        {
            stop++;
        }

        x       = left + m_view.GetPartPx (row, cell, cellFrom, start);
        width   = m_view.GetCharsPx (row, cell, cellFrom + start, cellFrom + stop);
        inverse = (style & DxuiTextSpan::kStyleInverse) != 0 && m_painter != nullptr;

        if (inverse)
        {
            m_painter->FillRect (x, (float) y, width, (float) m_look.cellHeightPx, argb);
        }

        hr = text.DrawString (chars.substr ((size_t) start, (size_t) (stop - start)).c_str(),
                              x, (float) (y + m_look.textTopPx),
                              width + m_look.cellAdvance, (float) (m_look.cellHeightPx - m_look.textTopPx),
                              inverse ? theme.TextViewBackground() : argb, font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                              DxuiTextRowLayout::GetWeight ((style & DxuiTextSpan::kStyleBold) != 0, (style & DxuiTextSpan::kStyleItalic) != 0), false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if ((style & DxuiTextSpan::kStyleUnderline) != 0 && m_painter != nullptr)
        {
            m_painter->FillRect (x, (float) y + (float) m_look.cellHeightPx - (float) m_view.GetLayoutScaler().ToPx (2), width, (float) (std::max) (1, m_view.GetLayoutScaler().ToPx (1)), argb);
        }

        start = stop;
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

    colors.assign ((size_t) m_view.GetRowLength (row), 0u);

    for (const Span & span : row.spans)
    {
        int       base   = m_view.GetCellBase (row, span.cell);
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
    float  left = (float) m_view.GetTextLeft();



    for (const FindMatch & match : m_view.GetLayoutFind().GetHighlights())
    {
        int  first = (std::max) (match.start.offset, flatStart);
        int  end   = (std::min) (match.start.offset + match.length, flatStart + count);

        if (match.start.row != row || end <= first)
        {
            continue;
        }

        FillSelectedRange (painter, y, row, cell, cellFrom, flatStart, count, 0.0f, match.start.offset, match.start.offset + match.length, m_view.GetLayoutFind().GetFillOf (match, theme));

        if (m_view.GetLayoutFind().GetCurrentArgb() == 0 && m_view.GetLayoutFind().IsCurrentMatch (match))
        {
            painter.OutlineRect (left + m_view.GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                                 m_view.GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_look.cellHeightPx,
                                 m_view.GetLayoutScaler().ToPxf (2.0f), edge);
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
    float  left      = (float) m_view.GetTextLeft();
    int    first     = (std::max) (selFrom, flatStart);
    int    end       = (std::min) (selTo, flatStart + count);
    int    separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect (left + m_view.GetPartPx (row, cell, cellFrom, first - flatStart), (float) y,
                          m_view.GetCharsPx (row, cell, cellFrom + first - flatStart, cellFrom + end - flatStart), (float) m_look.cellHeightPx, argb);
    }

    if (trailPx > 0.0f && selFrom <= separator && separator < selTo)
    {
        painter.FillRect (left + m_view.GetPartPx (row, cell, cellFrom, count), (float) y,
                          trailPx, (float) m_look.cellHeightPx, argb);
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
    int              first   = lineIndex;
    const Picture  & picture = *m_view.GetLayoutRows()[(size_t) m_view.GetLayoutLines()[(size_t) lineIndex].row].picture;
    float            top     = 0.0f;



    while (first > 0 && m_view.GetLayoutLines()[(size_t) first - 1].row == m_view.GetLayoutLines()[(size_t) lineIndex].row)
    {
        first--;
    }

    top = (float) m_view.GetLayoutBounds().top + (float) m_look.padPx + (float) (m_view.GetLineOffsetPx (first));

    DxuiTextRowLayout::PaintPicture (text, picture, (float) m_view.GetTextLeft(), top, m_view.GetTextWidthPx (m_view.IsScrollbarVisible()), m_view.GetLayoutScaler().ToPxf (1.0f));
}
