#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Core/TextEncoding.h"
#include "Debugger/HeatMapRangeSets.h"
#include "Ui/Debugger/MemoryRegionKeys.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::GetSectionStarts
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> HeatMapSections::GetSectionStarts (const std::string & machineId)
{
    constexpr uint32_t  kPage = 0x100;
    std::vector<Word>   starts (kSectionStarts.begin(), kSectionStarts.end());
    Word                rom   = kSectionStarts.back();



    for (const HeatMapRangeSets::Preset & preset : HeatMapRangeSets::GetPresets())
    {
        //  "My program" has no fixed span, and I/O and the language card are
        //  cut at kSectionStarts.
        if (preset.last <= preset.first || preset.last >= kSectionStarts.front())
        {
            continue;
        }

        starts.push_back (preset.first);
        starts.push_back ((Word) (preset.last + 1));
    }

    for (uint32_t page = (uint32_t) rom + kPage; page < (uint32_t) kAddressCount; page += kPage)
    {
        if (MemoryRegionKeys::GetRomPartKey (machineId, (Word) page) != MemoryRegionKeys::GetRomPartKey (machineId, (Word) (page - kPage)))
        {
            starts.push_back ((Word) page);
        }
    }

    std::ranges::sort (starts);
    starts.erase (std::unique (starts.begin(), starts.end()), starts.end());
    std::erase (starts, (Word) 0);
    return starts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::GetRamLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapSections::GetRamLabel (Word address)
{
    for (const HeatMapRangeSets::Preset & preset : HeatMapRangeSets::GetPresets())
    {
        if (preset.last > preset.first && preset.last < kSectionStarts.front() && address >= preset.first && address <= preset.last)
        {
            return TextEncoding::NarrowToWide (preset.name);
        }
    }

    return L"RAM";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::GetBandTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapSections::GetBandTitle (const Band & band)
{
    return std::format (L"{}  ${:04X}-${:04X}", band.title, band.first, (Word) (band.first + std::max (1, band.count) - 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::SplitAtSections
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapSections::Band> HeatMapSections::SplitAtSections (const std::vector<Band> & ranges, bool isAllMemory, const std::string & machineId)
{
    std::vector<Band>  bands;
    std::vector<Word>  starts = GetSectionStarts (machineId);



    for (const Band & range : ranges)
    {
        uint32_t               first = range.first;
        uint32_t               end   = first + (uint32_t) std::max (0, range.count);
        std::vector<uint32_t>  cuts;
        Band                   whole = range;



        for (Word start : starts)
        {
            if (start > first && start < end)
            {
                cuts.push_back (start);
            }
        }

        if (cuts.empty())
        {
            whole.isSection = true;
            whole.isTitled  = !isAllMemory;
            bands.push_back (whole);
            continue;
        }

        cuts.push_back (end);

        for (uint32_t cut : cuts)
        {
            bands.push_back (Band { range.title, (Word) first, (int) (cut - first), true });
            first = cut;
        }
    }

    return bands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::GetColumnsOf
//
//  A range holds no more addresses to a row than the next power of two up
//  from its size, so a small range is one row across the pane rather than
//  a row mostly empty.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapSections::GetColumnsOf (int columns, int count)
{
    int  fitted = 1;



    while (fitted < count && fitted < columns)
    {
        fitted *= 2;
    }

    return std::max (1, std::min (fitted, columns));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections::GetColumnsFor
//
//  Powers of two keep every row starting at a round address, so a row's label
//  is the first address in it. The row is then stretched to the width, by
//  less than twice over; past kMaxStretch twice the addresses are narrowed to
//  it instead, by no more than a quarter, so the cells are nearer the zoom's
//  size, as long as each cell is at least a pixel wide beside its street. A
//  width narrower than one cell at the pitch has a row of one address,
//  narrowed to the width, so a row never runs past it.
//
////////////////////////////////////////////////////////////////////////////////

int HeatMapSections::GetColumnsFor (long widthPx, long pitchPx)
{
    constexpr long  kNarrowestPitch = 1 + kStreetPx;
    int             columns         = kMinColumns;
    long            fitted          = 0;
    bool            isTooStretched  = false;
    bool            canDouble       = false;



    while (columns * 2 <= kMaxColumns && (long) columns * 2 * pitchPx <= widthPx)
    {
        columns *= 2;
    }

    fitted         = (long) columns * pitchPx;
    isTooStretched = fitted <= widthPx && (double) widthPx > (double) fitted * kMaxStretch;
    canDouble      = columns * 2 <= kMaxColumns && (long) columns * 2 * kNarrowestPitch <= widthPx;

    if (isTooStretched && canDouble)
    {
        columns *= 2;
    }

    return columns;
}





