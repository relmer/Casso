#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapPicker.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Ui/Debugger/Panes/HeatMapGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::GetPickAt
//
//  While cells are smaller than kComfortCellDip, a point over a cold cell
//  takes the hottest cell within kSnapDip of it, the nearer of two as hot;
//  with none, or with cells large enough to land on, the cell under it.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapPicker::GetPickAt (POINT point) const
{
    std::optional<HeatMapBands::Place>  pick = GetPickPlaceAt (point);



    return pick.has_value() ? std::optional<Word> (m_bands.GetAddressOf (*pick)) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::GetPickPlaceAt
//
//  The search stays in the row under the mouse, so moving up or down always
//  lands on the next row, however cold; along the row the pick is the
//  nearest hot cell within kSnapDip. Over an instruction drawn as one cell,
//  the pick is its opcode.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapBands::Place> HeatMapPicker::GetPickPlaceAt (POINT point) const
{
    std::optional<HeatMapBands::Place>    under   = m_grid.GetPlaceAt (point);
    long                                  reach   = m_grid.GetScaler().ToPx (kSnapDip);
    long                                  atX     = point.x - m_grid.GetMapRect().left;
    const HeatMapBands::Placed          * placed  = nullptr;
    long                                  row     = 0;
    long                                  lastCol = 0;
    HeatMapBands::Place                   best;
    long                                  nearest = reach + 1;



    if (under.has_value() && GetMergedAt (*under).has_value())
    {
        return GetMergedAt (*under)->first;
    }

    if (!under.has_value() || m_grid.GetCellPx() >= m_grid.GetScaler().ToPx (kComfortCellDip) || m_model.GetShownLevel (m_bands.GetAddressOf (*under)) > 0)
    {
        return under;
    }

    best    = *under;
    placed  = &m_bands.GetPlaced()[under->band];
    row     = under->offset / placed->columns;
    lastCol = std::min ((long) placed->columns - 1, (long) m_grid.GetColumnAt (*placed, atX + reach));

    for (long column = m_grid.GetColumnAt (*placed, atX - reach); column <= lastCol; column++)
    {
        HeatMapBands::Place  place    = { under->band, row * placed->columns + column };
        Byte                 level    = (place.offset < m_bands.GetAll()[under->band].count) ? m_model.GetShownLevel (m_bands.GetAddressOf (place)) : (Byte) 0;
        long                 left     = m_grid.GetColumnLeft (*placed, column);
        long                 right    = m_grid.GetColumnLeft (*placed, column + 1) - HeatMapSections::kStreetPx;
        long                 distance = std::max ({ left - atX, 0L, atX - right });



        if (level > 0 && distance < nearest)
        {
            best    = place;
            nearest = distance;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::IsMergeZoom
//
//  Whether the cells are tall enough for an instruction's two lines, so an
//  instruction is drawn as one cell across its bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPicker::IsMergeZoom() const
{
    float  minFont = m_grid.GetScaler().ToPxf (HeatMapCellText::kMinDetailFontDip);
    float  pad     = m_grid.GetScaler().ToPxf ((float) HeatMapCellText::kDetailPadDip);



    return m_model.CanMerge() &&
           ((float) m_grid.GetCellPx() - pad * 2.0f) / (HeatMapCellText::kLinePerEm * 2.0f) >= minFont;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::GetMergeLength
//
//  The length of the instruction whose opcode is at a place, when it is
//  drawn as one cell: a byte run as an opcode, drawn in the code color, with its form
//  and every byte of it known, inside its range. Zero otherwise.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapPicker::GetMergeLength (const HeatMapBands::Place & place, Word address) const
{
    int  length = m_model.GetMergeLength (address);



    if (length == 0 || place.offset + length > m_bands.GetAll()[place.band].count)
    {
        return 0;
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::GetMergedAt
//
//  The instruction drawn as one cell that contains a place: the instruction
//  at the place when the place is an opcode, else the one whose opcode is up
//  to two bytes before the place and whose length covers it. None at a zoom
//  too small to merge.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapPicker::Merged> HeatMapPicker::GetMergedAt (const HeatMapBands::Place & place) const
{
    constexpr long  kMaxOperands = 2;



    if (!IsMergeZoom())
    {
        return std::nullopt;
    }

    for (long back = 0; back <= kMaxOperands && back <= place.offset; back++)
    {
        HeatMapBands::Place  start   = { place.band, place.offset - back };
        Word                 address = m_bands.GetAddressOf (start);
        int                  length  = GetMergeLength (start, address);



        if (length > back)
        {
            return Merged { start, address, length };
        }

        //  An opcode is never part of the instruction before it.
        if (m_model.IsOpcode (address))
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker::GetMergedRects
//
//  An instruction's cell, one rectangle for each row it is on: one that
//  passes the end of its row continues at the start of the next.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RECT> HeatMapPicker::GetMergedRects (const Merged & merged) const
{
    const HeatMapBands::Placed  & placed = m_bands.GetPlaced()[merged.first.band];
    std::vector<RECT>             rects;
    long                          offset = merged.first.offset;
    long                          end    = merged.first.offset + merged.length;



    while (offset < end)
    {
        long  rowEnd = std::min (end, (offset / placed.columns + 1) * placed.columns);
        RECT  first  = m_grid.GetPlaceRect (HeatMapBands::Place { merged.first.band, offset });
        RECT  last   = m_grid.GetPlaceRect (HeatMapBands::Place { merged.first.band, rowEnd - 1 });



        rects.push_back (RECT { first.left, first.top, last.right, last.bottom });
        offset = rowEnd;
    }

    return rects;
}
