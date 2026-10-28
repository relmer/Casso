#include "Pch.h"

#include "Ui/DiskInspector/SectorAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess::CountReads
//
////////////////////////////////////////////////////////////////////////////////

vector<uint32_t> SectorAccess::CountReads (const TrackAnalysis & track, const vector<CellSpan> & spans)
{
    vector<uint32_t>  counts (track.sectors.size(), 0);
    uint32_t          length = track.measurements.lengthCells;



    for (size_t s = 0; s < track.sectors.size(); s++)
    {
        int  dataField = track.sectors[s].dataField;

        for (const CellSpan & span : spans)
        {
            if (dataField >= 0)
            {
                const LocatedField &  field  = track.fields[static_cast<size_t> (dataField)];
                uint32_t              fieldW = (field.endCell >= field.startCell) ? field.endCell - field.startCell : field.endCell + length - field.startCell;

                counts[s] += (GetOverlap (length, span.start, span.end, field.startCell, field.endCell) * 2 >= std::max (fieldW, 1u)) ? span.count : 0;
            }
        }
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess::CountWrites
//
////////////////////////////////////////////////////////////////////////////////

vector<uint32_t> SectorAccess::CountWrites (const TrackAnalysis & track, const vector<CellSpan> & spans)
{
    vector<uint32_t>  counts (track.sectors.size(), 0);
    uint32_t          length = track.measurements.lengthCells;



    for (size_t s = 0; s < track.sectors.size(); s++)
    {
        for (const CellSpan & span : spans)
        {
            bool  isOver = false;

            for (int f : { track.sectors[s].addressField, track.sectors[s].dataField })
            {
                isOver = isOver || (f >= 0 && GetOverlap (length, span.start, span.end, track.fields[static_cast<size_t> (f)].startCell, track.fields[static_cast<size_t> (f)].endCell) > 0);
            }

            counts[s] += isOver ? span.count : 0;
        }
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess::CountAll
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SectorAccess::CountAll (const vector<CellSpan> & spans)
{
    uint32_t  total = 0;



    for (const CellSpan & span : spans)
    {
        total += span.count;
    }

    return total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess::GetOverlap
//
//  How many cells a span and a field share on a track of the given length,
//  either of them crossing the index.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SectorAccess::GetOverlap (uint32_t length, uint32_t spanStart, uint32_t spanEnd, uint32_t fieldStart, uint32_t fieldEnd)
{
    using Range = std::pair<uint32_t, uint32_t>;



    auto  split = [length] (uint32_t s, uint32_t e) { return (e >= s || length == 0) ? vector<Range> { { s, e } } : vector<Range> { { s, length }, { 0, e } }; };

    uint32_t  overlap = 0;



    for (const Range & a : split (spanStart, spanEnd))
    {
        for (const Range & b : split (fieldStart, fieldEnd))
        {
            uint32_t  lo = std::max (a.first, b.first);
            uint32_t  hi = std::min (a.second, b.second);

            overlap += (hi > lo) ? hi - lo : 0;
        }
    }

    return overlap;
}
