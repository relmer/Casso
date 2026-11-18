#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapBands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::SetRanges
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapBands::SetRanges (std::vector<Band> ranges)
{
    if (m_hasRanges && ranges == m_ranges)
    {
        return false;
    }

    m_ranges    = std::move (ranges);
    m_bands     = m_ranges;
    m_isSplit   = false;
    m_hasRanges = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::Clear
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapBands::Clear()
{
    if (!m_hasRanges)
    {
        return false;
    }

    m_ranges    = { Band { {}, 0, HeatMapSections::kAddressCount } };
    m_bands     = m_ranges;
    m_isSplit   = false;
    m_hasRanges = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::Reset
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapBands::Reset()
{
    m_isSplit = false;
    m_bands   = m_ranges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::Sync
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapBands::Sync (bool canSplit, const std::string & machineId)
{
    if (canSplit == m_isSplit)
    {
        return false;
    }

    m_isSplit = canSplit;
    m_bands   = canSplit ? HeatMapSections::SplitAtSections (m_ranges, !m_hasRanges, machineId) : m_ranges;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::Arrange
//
//  Each range's header and rows, one after another down the map. All of
//  memory is one range with no header, until sections cut it. A range under
//  a header takes at least a row label's line, so the label of a range of
//  one short row ends before the next header. A section's rows are as wide
//  as the map's, so its cells line up with every other section's, and room
//  after them takes the bottom of its box.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapBands::Arrange (
    int   columns,
    long  pitch,
    long  headerPx,
    long  sectionHeaderPx,
    long  afterPx,
    long  labelLinePx)
{
    long  y = 0;



    m_placed.assign (m_bands.size(), Placed {});

    for (size_t i = 0; i < m_bands.size(); i++)
    {
        Placed  & placed   = m_placed[i];
        int       count    = std::max (1, m_bands[i].count);
        bool      isHeaded = HasHeader (i);



        placed.columns   = m_bands[i].isSection ? columns : HeatMapSections::GetColumnsOf (columns, count);
        placed.headerTop = y;
        placed.rowsTop   = y + (!isHeaded ? 0L : m_bands[i].isSection ? sectionHeaderPx : headerPx);
        placed.rows      = (count + placed.columns - 1) / placed.columns;

        y = placed.rowsTop + std::max (placed.rows * pitch, isHeaded ? labelLinePx : 0L) + (m_bands[i].isSection ? afterPx : 0L);
    }

    m_contentH = y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::GetRows
//
//  Every range's rows together.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapBands::GetRows (int columns) const
{
    int  rows = 0;



    for (const Band & band : m_bands)
    {
        int  count = std::max (1, band.count);
        int  wide  = band.isSection ? columns : HeatMapSections::GetColumnsOf (columns, count);



        rows += (count + wide - 1) / wide;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::GetBandAt
//
//  The range whose rows contain a height in the map, or -1 over a header or
//  past the last range.
//
////////////////////////////////////////////////////////////////////////////////

long HeatMapBands::GetBandAt (long y, long pitch) const
{
    for (size_t i = 0; i < m_placed.size(); i++)
    {
        if (y >= m_placed[i].rowsTop && y < m_placed[i].rowsTop + m_placed[i].rows * pitch)
        {
            return (long) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::GetPlaceOf
//
//  The first range that contains an address.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapBands::Place> HeatMapBands::GetPlaceOf (Word address) const
{
    for (size_t i = 0; i < m_bands.size(); i++)
    {
        long  offset = (long) (Word) (address - m_bands[i].first);



        if (offset < m_bands[i].count)
        {
            return Place { i, offset };
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands::GetAddressOf
//
////////////////////////////////////////////////////////////////////////////////

Word HeatMapBands::GetAddressOf (const Place & place) const
{
    return (Word) (m_bands[place.band].first + place.offset);
}
