#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapPainter.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/MemoryRegionKeys.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintBar
//
//  The row of views holds the window's strip of drop-downs (which view,
//  Blend, which ranges and which bank); while the heat is being rebuilt after
//  a move through history, a note in the room the strip leaves says so.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kGapDip = 10.0f;
    DxuiFontHandle   font    = theme.BodyFont();
    float            gap     = m_grid.GetScaler().ToPxf (kGapDip);
    HRESULT          hr      = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (!m_isRebuilding || m_noteRect.right - m_noteRect.left <= gap)
    {
        return;
    }

    hr = text.DrawString (kpszRebuildingNote, (float) m_noteRect.left + gap, (float) m_noteRect.top,
                          (float) (m_noteRect.right - m_noteRect.left) - gap, (float) (m_noteRect.bottom - m_noteRect.top),
                          theme.ForegroundMuted(), m_grid.GetScaler().ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintRowLabels
//
//  Each row's first address beside it, every row while the rows are tall
//  enough to tell the labels apart and every second, fourth and so on below
//  that, counted from each range's first row.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintRowLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int    kPadDip     = 4;
    DxuiFontHandle   font        = theme.MonospaceFont();
    float            size        = m_grid.GetScaler().ToPxf (font.sizeDip);
    long             pitch       = m_grid.GetPitch();
    float            gutter      = (float) (m_grid.GetGutterPx() - m_grid.GetScaler().ToPx (kPadDip));
    float            y           = 0.0f;
    std::wstring     label;
    HRESULT          hr          = S_OK;



    if (pitch <= 0 || gutter <= 0.0f)
    {
        return;
    }

    for (size_t band = 0; band < m_bands.GetPlaced().size(); band++)
    {
        const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[band];
        long                          step   = 1;
        long                          row    = 0;



        while (step < placed.rows && (float) (step * pitch) < size * HeatMapGrid::kLabelLine)
        {
            step *= 2;
        }

        //  The first labeled row whose top is at or below the area's top, so
        //  a row partly scrolled off has no label over the bar.
        row = (std::max (0L, m_grid.GetScrollTop() - placed.rowsTop) + pitch - 1) / pitch;
        row = ((row + step - 1) / step) * step;

        for (; row < placed.rows; row += step)
        {
            y = (float) (m_grid.GetMapRect().top + placed.rowsTop + row * pitch - m_grid.GetScrollTop());

            if (y + size > (float) m_grid.GetMapRect().bottom)
            {
                break;
            }

            label = std::format (L"${:04X}", (Word) (m_bands.GetAll()[band].first + row * placed.columns));

            hr = text.DrawString (label.c_str(), (float) m_grid.GetBounds().left, y, gutter, size * HeatMapGrid::kLabelLine,
                                  theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintHeaders
//
//  Each range's name and span across the top of its rows, "Hi-res page 1
//  $2000-$3FFF", where the whole header is in view; or, for a set with
//  nothing in it, a note saying so where the map would be. A header starts
//  at the pane's text inset, where the pane's title does.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintHeaders (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           size  = m_grid.GetScaler().ToPxf (font.sizeDip);
    float           left  = (float) (m_grid.GetBounds().left + DxuiPaneMetrics::GetContentTextInsetPx (m_grid.GetScaler()));
    float           width = std::max (0.0f, (float) m_grid.GetMapRect().right - left);
    std::wstring    title;
    HRESULT         hr    = S_OK;



    if (!m_bands.HasRanges() && !m_bands.IsSplit())
    {
        return;
    }

    if (m_bands.GetAll().empty())
    {
        hr = text.DrawString (kpszNoRangesNote, (float) m_grid.GetMapRect().left, (float) m_grid.GetMapRect().top, std::max (0.0f, (float) m_grid.GetBounds().right - (float) m_grid.GetMapRect().left),
                              m_grid.GetScaler().ToPxf ((float) HeatMapGrid::kHeaderDip), theme.ForegroundMuted(), size, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    for (size_t band = 0; band < m_bands.GetAll().size() && band < m_bands.GetPlaced().size(); band++)
    {
        RECT  header = m_grid.GetHeaderRect (band);



        if (m_bands.GetAll()[band].isSection || !m_bands.HasHeader (band) || header.top < m_grid.GetMapRect().top || header.bottom > m_grid.GetMapRect().bottom)
        {
            continue;
        }

        title = HeatMapSections::GetBandTitle (m_bands.GetAll()[band]);

        hr = text.DrawString (title.c_str(), left, (float) header.top, width, (float) (header.bottom - header.top), theme.Foreground(),
                              size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintOverlays
//
//  Over the cells in view, clipped to the map: each one's value and second
//  line where the cells are large enough, or its whole instruction, then
//  its outlines, around the whole instruction for its opcode, and last the
//  labeled ranges' outlines. A cell is checked against the marks
//  before its outlines are gathered, so a map with none costs a comparison
//  a cell.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintOverlays (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr size_t  kCount     = (size_t) HeatMapSections::kAddressCount;
    float             minFont    = m_grid.GetScaler().ToPxf (HeatMapCellText::kMinDetailFontDip);
    float             pad        = m_grid.GetScaler().ToPxf ((float) HeatMapCellText::kDetailPadDip);
    bool              hasDetail  = m_cells.GetValues().size() == kCount && (float) m_grid.GetCellPx() >= minFont * HeatMapCellText::kLinePerEm + pad * 2.0f;
    bool              hasMarks   = m_cells.GetPc().has_value() || m_cells.GetStack().has_value() || m_cells.GetFound().has_value() || !m_cells.GetBreakpoints().empty();
    bool              hasRegions = m_cells.GetRegions().size() == kCount && m_grid.GetCellPx() >= kRegionCellPx;
    int               covered    = 0;
    HRESULT           hr         = S_OK;
    auto              isMarked   = [this] (Word address) { return m_cells.IsMarked (address); };



    if (!hasDetail && !hasMarks && !hasRegions)
    {
        return;
    }

    //  A street wider on each side than the map, so the lines in the street
    //  left of the first column are drawn too.
    hr = text.PushClipRect ((float) (m_grid.GetMapRect().left - HeatMapSections::kStreetPx), (float) m_grid.GetMapRect().top, (float) (m_grid.GetMapRect().right - m_grid.GetMapRect().left + 2 * HeatMapSections::kStreetPx), (float) (m_grid.GetMapRect().bottom - m_grid.GetMapRect().top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_grid.ForEachVisible ([&] (const HeatMapBands::Place & place, Word address)
    {
        std::optional<HeatMapPicker::Merged>  merged;



        //  An instruction's operand cells are drawn as part of its opcode's,
        //  and so is the first one in view when its opcode is above it.
        if (covered > 0)
        {
            covered--;
        }
        else if (hasDetail)
        {
            merged = m_picker.GetMergedAt (place);

            if (merged.has_value())
            {
                PaintInstruction (text, theme, *merged);
                covered = (int) (merged->first.offset + merged->length - 1 - place.offset);
            }
            else
            {
                PaintDetail (text, theme, place, address);
            }
        }

        if (merged.has_value() && isMarked (merged->opcode))
        {
            PaintOutlines (text, m_picker.GetMergedRects (*merged), merged->opcode);
        }
        else if ((!merged.has_value() || merged->opcode != address) && isMarked (address))
        {
            PaintOutlines (text, { m_grid.GetPlaceRect (place) }, address);
        }
    });

    if (hasRegions)
    {
        PaintRegions (text);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintDetail
//
//  The value in hex, and under it the second line, centered in the cell. A
//  cell with a symbol has the symbol's name over the value, in place of the
//  second line where three lines do not fit, wherever the cell is tall enough
//  for two: the name is made smaller to fit across. A form or a name longer
//  than the cell is made smaller to fit, but no smaller than
//  HeatMapCellText::kMinDetailFontDip, and is clipped to its cell beyond
//  that. The value is in ROM's or I/O's color for a byte in either, and the
//  name in the symbol color, each moved until it reads on the cell.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintDetail (IDxuiTextRenderer & text, const IDxuiTheme & theme, const HeatMapBands::Place & place, Word address) const
{
    DxuiFontHandle           font     = theme.MonospaceFont();
    RECT                     cell     = m_grid.GetPlaceRect (place);
    long                     wide     = cell.right  - cell.left;
    long                     tall     = cell.bottom - cell.top;
    float                    maxFont  = m_grid.GetScaler().ToPxf (font.sizeDip);
    float                    minFont  = m_grid.GetScaler().ToPxf (HeatMapCellText::kMinDetailFontDip);
    float                    pad      = m_grid.GetScaler().ToPxf ((float) HeatMapCellText::kDetailPadDip);
    HeatMapCellText::Detail  detail   = HeatMapCellText::GetDetail (wide, tall, maxFont, minFont, pad);
    int16_t                  stored   = m_cells.GetValues()[address];
    Byte                     value    = (Byte) stored;
    bool                     isOpcode = m_cells.GetOpcodes().size() == (size_t) HeatMapSections::kAddressCount && m_cells.GetOpcodes()[address] != 0;
    uint32_t                 fill     = m_cells.GetCellColor (address) | 0xFF000000u;
    uint32_t                 plain    = DebuggerTextColors::GetReadable (HeatMapCellText::GetDetailTextColor (fill, theme.Foreground(), m_cells.GetPalette().background), fill);
    uint32_t                 ink      = m_cells.GetValueInk (address, fill, plain);
    float                    two      = std::min ({ maxFont, ((float) tall - pad * 2.0f) / (HeatMapCellText::kLinePerEm * 2.0f), ((float) wide - pad * 2.0f) / ((float) HeatMapCellText::kValueChars  * HeatMapCellText::kAdvancePerEm) });
    float                    three    = std::min ({ maxFont, ((float) tall - pad * 2.0f) / (HeatMapCellText::kLinePerEm * 3.0f), ((float) wide - pad * 2.0f) / ((float) HeatMapCellText::kDetailChars * HeatMapCellText::kAdvancePerEm) });
    std::wstring             name     = (two >= minFont) ? TextEncoding::NarrowToWide (m_cells.GetSymbolAt (address)) : std::wstring();
    int                      lines    = detail.lines;
    float                    fontPx   = detail.fontPx;
    float                    lineH    = 0.0f;
    float                    y        = 0.0f;
    std::wstring             second;
    HRESULT                  hr       = S_OK;
    auto            drawFit  = [&] (const std::wstring & line, float lineTop, uint32_t color)
    {
        float  fit = ((float) wide - pad * 2.0f) / ((float) std::max<size_t> (1, line.size()) * HeatMapCellText::kAdvancePerEm);



        hr = text.DrawString (line.c_str(), (float) cell.left, lineTop, (float) wide, lineH, color,
                              std::max (minFont, std::min (fontPx, fit)), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    };



    if (detail.lines == 0 || stored < 0)
    {
        return;
    }

    if (!name.empty() && three >= minFont)
    {
        lines  = 3;
        fontPx = three;
    }
    else if (!name.empty())
    {
        lines  = 2;
        fontPx = two;
    }

    lineH = fontPx * HeatMapCellText::kLinePerEm;
    y     = (float) cell.top + ((float) tall - lineH * (float) lines) / 2.0f;

    hr = text.PushClipRect ((float) cell.left, (float) cell.top, (float) wide, (float) tall);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!name.empty())
    {
        drawFit (name, y, DebuggerTextColors::GetReadable (m_cells.GetPalette().symbol | 0xFF000000u, fill));
        y += lineH;
        lines--;
    }

    hr = text.DrawString (std::format (L"{:02X}", value).c_str(), (float) cell.left, y, (float) wide, lineH, ink,
                          fontPx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (lines >= 2)
    {
        second = HeatMapCellText::GetDetailLine (value, isOpcode, (m_cells.GetOpcodeForms() != nullptr && m_cells.GetOpcodeForms()->size() > value) ? (*m_cells.GetOpcodeForms())[value] : std::string());
        drawFit (second, y + lineH, plain);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintRegions
//
//  Each cell in a labeled range has a line in the street on each side whose
//  neighbor is in another range, or in none, so the lines outline the whole
//  range: a range that ends partway along a row steps in there. The lines
//  are the range's color. A section above RAM is outlined by PaintSections
//  instead.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintRegions (IDxuiTextRenderer & text) const
{
    constexpr float  kOutlineMix = 0.8f;
    HRESULT          hr          = S_OK;



    m_grid.ForEachVisible ([&] (const HeatMapBands::Place & place, Word address)
    {
        uint16_t                      key    = m_cells.GetRegionAt (address);
        const HeatMapBands::Band    & band   = m_bands.GetAll()[place.band];
        const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[place.band];
        long                          column = place.offset % placed.columns;
        RECT                          cell   = {};
        float                         street = (float) HeatMapSections::kStreetPx;
        float                         wide   = 0.0f;
        float                         tall   = 0.0f;
        uint32_t                      stroke = 0;
        auto            isSame  = [&] (long offset)
        {
            return offset >= 0 && offset < band.count && m_cells.GetRegionAt (m_bands.GetAddressOf (HeatMapBands::Place { place.band, offset })) == key;
        };



        if (key == 0 || (band.isSection && band.first >= HeatMapSections::kSectionStarts.front()))
        {
            return;
        }

        cell   = m_grid.GetPlaceRect (place);
        wide   = (float) (cell.right  - cell.left);
        tall   = (float) (cell.bottom - cell.top);
        stroke = DxuiColor::Mix (m_cells.GetPalette().background | 0xFF000000u, MemoryRegionKeys::GetColor (key, m_cells.GetRegionColors()) | 0xFF000000u, kOutlineMix) | 0xFF000000u;
        stroke = DxuiColor::ComputeInkForContrast (stroke, m_cells.GetPalette().background | 0xFF000000u, kMinLineContrast) | 0xFF000000u;

        if (!isSame (place.offset - placed.columns))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.top - street, wide + street * 2.0f, street, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (!isSame (place.offset + placed.columns))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.bottom, wide + street * 2.0f, street, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (column == 0 || !isSame (place.offset - 1))
        {
            hr = text.FillRect ((float) cell.left - street, (float) cell.top - street, street, tall + street * 2.0f, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (column == placed.columns - 1 || !isSame (place.offset + 1))
        {
            hr = text.FillRect ((float) cell.right, (float) cell.top - street, street, tall + street * 2.0f, stroke);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintSections
//
//  Each section in view as a box: a rounded outline in the range's color the
//  width of the map around its rows, at least a row label's line tall,
//  HeatMapGrid::kSectionPadDip from the cells on every side, with its label
//  on the top edge from the left, past the corner's curve, in the header's
//  room above the cells, over a patch of the page that breaks the line
//  there. A section of main RAM, which has no range color, is in the muted
//  text color. A whole range of a set is labeled by its title and span, as
//  "Zero page  $0000-$00FF".
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintSections (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kOutlineMix = 0.6f;
    constexpr float  kCornerDip  = 4.0f;
    DxuiFontHandle   font        = theme.MonospaceFont();
    float            fontPx      = m_grid.GetScaler().ToPxf (kSectionLabelDip);
    float            labelH      = fontPx * HeatMapCellText::kLinePerEm;
    float            pad         = m_grid.GetScaler().ToPxf ((float) HeatMapCellText::kDetailPadDip);
    float            inside      = (float) m_grid.GetScaler().ToPx (HeatMapGrid::kSectionPadDip);
    float            radius      = m_grid.GetScaler().ToPxf (kCornerDip);
    float            thickness   = m_grid.GetScaler().ToPxf (1.0f);
    float            street      = (float) HeatMapSections::kStreetPx;
    long             pitch       = m_grid.GetPitch();
    uint32_t         back        = m_cells.GetPalette().background | 0xFF000000u;
    HRESULT          hr          = S_OK;



    hr = text.PushClipRect ((float) m_grid.GetBounds().left, (float) m_grid.GetMapRect().top, (float) (m_grid.GetBounds().right - m_grid.GetBounds().left), (float) (m_grid.GetMapRect().bottom - m_grid.GetMapRect().top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (size_t i = 0; i < m_bands.GetAll().size() && i < m_bands.GetPlaced().size(); i++)
    {
        const HeatMapBands::Band    & band   = m_bands.GetAll()[i];
        const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[i];
        bool                          isRam  = band.first < HeatMapSections::kSectionStarts.front();
        uint16_t                      key    = isRam ? (uint16_t) 0 : m_cells.GetRegionAt (band.first);
        std::wstring              label     = band.isTitled ? HeatMapSections::GetBandTitle (band)
                                            : isRam         ? HeatMapSections::GetRamLabel (band.first)
                                                            : MemoryRegionKeys::GetRangeLabel (key, band.first, band.count, m_cells.GetSlotCards(), m_cells.GetMachineId());
        uint32_t                  color     = isRam ? theme.ForegroundMuted() : MemoryRegionKeys::GetColor (key, m_cells.GetRegionColors());
        float                     top       = (float) (m_grid.GetMapRect().top + placed.rowsTop - m_grid.GetScrollTop());
        float                     left      = (float) m_grid.GetMapRect().left - street - inside;
        float                     right     = (float) (m_grid.GetMapRect().left + m_grid.GetRowPx()) + inside;
        float                     edge      = top - street - inside;
        float                     bottom    = top + (float) std::max (placed.rows * pitch, m_grid.GetLabelLinePx()) + inside;
        float                     labelW    = (float) label.size() * fontPx * HeatMapCellText::kAdvancePerEm + pad * 2.0f;
        float                     labelX    = left + radius + pad;
        std::vector<DxuiPointF>   points    = { { left, edge }, { right, edge }, { right, bottom }, { left, bottom } };



        if (!band.isSection || bottom < (float) m_grid.GetMapRect().top || edge - labelH > (float) m_grid.GetMapRect().bottom)
        {
            continue;
        }

        if (color != 0)
        {
            for (const DxuiRoundedOutline::Segment & segment : DxuiRoundedOutline::GetSegments (points, radius))
            {
                hr = text.DrawLine (segment.from.x, segment.from.y, segment.to.x, segment.to.y, thickness,
                                    DxuiColor::ComputeInkForContrast (DxuiColor::Mix (back, color | 0xFF000000u, kOutlineMix) | 0xFF000000u, back, kMinLineContrast) | 0xFF000000u);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        hr = text.FillRect (labelX, edge - labelH / 2.0f, labelW, labelH, back);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (label.c_str(), labelX, edge - labelH / 2.0f, labelW, labelH, DebuggerTextColors::GetReadable (color | 0xFF000000u, back),
                              fontPx, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintInstruction
//
//  The instruction with its operand as a symbol where there is one, and
//  under it the bytes in hex, smaller and dimmer, as the disassembly draws
//  a line's bytes under its source. An instruction on two rows is split
//  between its pieces by SplitInstruction, each piece's text centered in
//  it, at one font size for all of them.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintInstruction (IDxuiTextRenderer & text, const IDxuiTheme & theme, const HeatMapPicker::Merged & merged) const
{
    constexpr float     kBytesScale = 0.8f;
    constexpr float     kBytesDim   = 0.45f;
    DxuiFontHandle      font        = theme.MonospaceFont();
    float               minFont     = m_grid.GetScaler().ToPxf (HeatMapCellText::kMinDetailFontDip);
    float               pad         = m_grid.GetScaler().ToPxf ((float) HeatMapCellText::kDetailPadDip);
    std::vector<RECT>   pieces      = m_picker.GetMergedRects (merged);
    Byte                opcode      = (Byte) m_cells.GetValues()[merged.opcode];
    std::vector<Byte>   operands;
    std::wstring        bytes       = std::format (L"{:02X}", opcode);
    std::wstring                       instruction;
    std::vector<long>                  cells;
    HeatMapCellText::InstructionLines  lines;
    long                               pitch       = m_grid.GetPitch();
    long                               tall        = pieces.front().bottom - pieces.front().top;
    float                              fontPx      = 0.0f;
    float                              lineH       = 0.0f;
    uint32_t                           fill        = m_cells.GetCellColor (merged.opcode) | 0xFF000000u;
    uint32_t                           ink         = m_cells.GetValueInk (merged.opcode, fill, DebuggerTextColors::GetReadable (HeatMapCellText::GetDetailTextColor (fill, theme.Foreground(), m_cells.GetPalette().background), fill));
    HRESULT                            hr          = S_OK;



    for (int i = 1; i < merged.length; i++)
    {
        Byte  value = (Byte) m_cells.GetValues()[(Word) (merged.opcode + i)];



        operands.push_back (value);
        bytes += std::format (L" {:02X}", value);
    }

    for (const RECT & piece : pieces)
    {
        cells.push_back (std::max (1L, (piece.right - piece.left + HeatMapSections::kStreetPx) / std::max (1L, pitch)));
    }

    instruction = HeatMapCellText::GetInstructionText ((*m_cells.GetOpcodeForms())[opcode], HeatMapOptions::GetCpuAddress (m_cells.GetShownBank(), merged.opcode), operands, m_cells.GetSymbols().get());
    lines       = HeatMapCellText::SplitInstruction (instruction, bytes, cells);
    fontPx      = std::min (m_grid.GetScaler().ToPxf (font.sizeDip), ((float) tall - pad * 2.0f) / (HeatMapCellText::kLinePerEm * 2.0f));

    for (size_t i = 0; i < pieces.size(); i++)
    {
        size_t  longest = std::max (lines[i].first.size(), lines[i].second.size());



        if (longest > 0)
        {
            fontPx = std::min (fontPx, ((float) (pieces[i].right - pieces[i].left) - pad * 2.0f) / ((float) longest * HeatMapCellText::kAdvancePerEm));
        }
    }

    fontPx = std::max (minFont, fontPx);
    lineH  = fontPx * HeatMapCellText::kLinePerEm;

    for (size_t i = 0; i < pieces.size(); i++)
    {
        const RECT  & piece = pieces[i];
        float         left  = (float) piece.left;
        float         top   = (float) piece.top + ((float) tall - lineH * 2.0f) / 2.0f;
        float         width = (float) (piece.right - piece.left);



        hr = text.FillRect (left, (float) piece.top, width, (float) tall, fill);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (lines[i].first.empty() && lines[i].second.empty())
        {
            continue;
        }

        hr = text.PushClipRect (left, (float) piece.top, width, (float) tall);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (lines[i].first.c_str(), left, top, width, lineH, ink, fontPx, font.face,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (lines[i].second.c_str(), left, top + lineH, width, lineH, DebuggerTextColors::GetReadable (DxuiColor::Mix (ink, fill, kBytesDim) | 0xFF000000u, fill),
                              std::max (minFont, fontPx * kBytesScale), font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintOutlines
//
//  Each outline a ring, the first in the street around the cell like the
//  hover frame and each after it inside the one before; two pixels thick
//  once cells are large enough to keep their color inside them.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintOutlines (IDxuiTextRenderer & text, const std::vector<RECT> & rects, Word address) const
{
    constexpr int          kThickCellPx = 12;
    long                   line         = (m_grid.GetCellPx() >= kThickCellPx) ? 2 : 1;
    std::vector<uint32_t>  colors       = m_cells.GetOutlineColors (address);
    RECT                   ring         = {};
    float                  left         = 0.0f;
    float                  top          = 0.0f;
    float                  wide         = 0.0f;
    float                  tall         = 0.0f;
    float                  thick        = (float) line;
    HRESULT                hr           = S_OK;



    for (size_t k = 0; k < colors.size() * rects.size(); k++)
    {
        long  inset = (long) (k / rects.size()) * line;



        ring = rects[k % rects.size()];
        InflateRect (&ring, HeatMapSections::kStreetPx - inset, HeatMapSections::kStreetPx - inset);

        if (ring.right - ring.left <= line * 2 || ring.bottom - ring.top <= line * 2)
        {
            break;
        }

        left = (float) ring.left;
        top  = (float) ring.top;
        wide = (float) (ring.right  - ring.left);
        tall = (float) (ring.bottom - ring.top);

        hr = text.FillRect (left,                top,                wide,  thick, colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top + tall - thick, wide,  thick, colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,                top,                thick, tall,  colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left + wide - thick, top,                thick, tall,  colors[k / rects.size()] | 0xFF000000u);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter::PaintHover
//
//  A frame around the cell the mouse picks, drawn in the streets around it,
//  so a snap to a busier cell shows where it went. An instruction drawn as
//  one cell is framed whole, a piece on each row it is on.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPainter::PaintHover (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    std::optional<HeatMapPicker::Merged>  merged;
    std::vector<RECT>                     rects;
    float                                 tall   = (float) (m_grid.GetCellPx() + 2 * HeatMapSections::kStreetPx);
    float                                 line   = (float) HeatMapSections::kStreetPx;
    uint32_t                              color  = theme.Foreground();
    HRESULT                               hr     = S_OK;



    if (!m_grid.GetHoverPlace().has_value())
    {
        return;
    }

    merged = m_picker.GetMergedAt (*m_grid.GetHoverPlace());
    rects  = merged.has_value() ? m_picker.GetMergedRects (*merged) : std::vector<RECT> { m_grid.GetPlaceRect (*m_grid.GetHoverPlace()) };

    for (const RECT & cell : rects)
    {
        float  left = (float) (cell.left - HeatMapSections::kStreetPx);
        float  top  = (float) (cell.top  - HeatMapSections::kStreetPx);
        float  wide = (float) (cell.right - cell.left + 2 * HeatMapSections::kStreetPx);



        if (left + wide <= (float) m_grid.GetMapRect().left || left >= (float) m_grid.GetMapRect().right || top + tall <= (float) m_grid.GetMapRect().top || top >= (float) m_grid.GetMapRect().bottom)
        {
            continue;
        }

        hr = text.FillRect (left,               top,               wide, line, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,               top + tall - line, wide, line, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left,               top,               line, tall, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.FillRect (left + wide - line, top,               line, tall, color);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





