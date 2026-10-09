#include "Pch.h"

#include "Ui/DiskInspector/PlatterCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::Encode
//
////////////////////////////////////////////////////////////////////////////////

Byte PlatterCells::Encode (PlatterKind kind)
{
    Byte  priority = 0;



    switch (kind)
    {
        case PlatterKind::FailedChecksum: priority = 3; break;
        case PlatterKind::Noise:          priority = 2; break;
        case PlatterKind::RandomBits:     priority = 1; break;
        default:                          priority = 0; break;
    }

    return static_cast<Byte> ((priority << kKindBits) | static_cast<Byte> (kind));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::Decode
//
////////////////////////////////////////////////////////////////////////////////

PlatterKind PlatterCells::Decode (Byte value)
{
    return static_cast<PlatterKind> (value & kKindMask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::GetKindOf
//
////////////////////////////////////////////////////////////////////////////////

PlatterKind PlatterCells::GetKindOf (NibbleKind kind)
{
    PlatterKind  result = PlatterKind::Other;



    switch (kind)
    {
        case NibbleKind::Sync:            result = PlatterKind::Sync;         break;
        case NibbleKind::AddressPrologue: result = PlatterKind::AddressMark;  break;
        case NibbleKind::AddressField:    result = PlatterKind::AddressField; break;
        case NibbleKind::AddressEpilogue: result = PlatterKind::AddressMark;  break;
        case NibbleKind::DataPrologue:    result = PlatterKind::DataMark;     break;
        case NibbleKind::DataField:       result = PlatterKind::DataField;    break;
        case NibbleKind::DataEpilogue:    result = PlatterKind::DataMark;     break;
        case NibbleKind::Noise:           result = PlatterKind::Noise;        break;
        case NibbleKind::Other:           result = PlatterKind::Other;        break;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::BuildCells
//
//  A nibble runs from its start cell to the next nibble's, wrapping at the
//  index, so the cells before the first nibble belong to the last. A nibble
//  in a field whose checksum failed takes the failed-checksum kind; a cell in
//  a random-bit region takes that kind unless something of higher priority
//  covers it. A track with no nibbles is nothing recorded.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCells::BuildCells (const TrackAnalysis & analysis, vector<Byte> & outCells)
{
    const FramedTrack &  framed  = analysis.framed;
    size_t               count   = framed.nibbles.size();
    uint32_t             cells   = framed.cellCount;
    size_t               i       = 0;
    uint32_t             cell    = 0;
    uint32_t             end     = 0;
    Byte                 value   = 0;
    Byte                 random  = Encode (PlatterKind::RandomBits);



    outCells.assign (cells, Encode (PlatterKind::NothingRecorded));

    for (i = 0; i < count && cells > 0; i++)
    {
        value = Encode (GetKindOf (analysis.nibbleKinds[i]));

        if (i < analysis.isFailedChecksum.size() && analysis.isFailedChecksum[i] != 0)
        {
            value = Encode (PlatterKind::FailedChecksum);
        }

        cell = framed.nibbles[i].startCell % cells;
        end  = framed.nibbles[(i + 1) % count].startCell % cells;

        do
        {
            outCells[cell] = value;
            cell           = (cell + 1) % cells;
        }
        while (cell != end);
    }

    for (const RandomRegion & region : framed.randomRegions)
    {
        for (cell = 0; cell < region.cellCount && cells > 0; cell++)
        {
            Byte &  target = outCells[(region.startCell + cell) % cells];

            target = std::max (target, random);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::BuildLevels
//
//  Level 0 is the cells; each level after holds the larger of each pair in
//  the one before, until a level holds one byte.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCells::BuildLevels (const vector<Byte> & cells, vector<vector<Byte>> & outLevels)
{
    size_t  i = 0;



    outLevels.clear();
    outLevels.push_back (cells);

    while (outLevels.back().size() > 1 && static_cast<int> (outLevels.size()) < kMaxLevels)
    {
        const vector<Byte> &  below = outLevels.back();
        vector<Byte>          level ((below.size() + 1) / 2);

        for (i = 0; i < level.size(); i++)
        {
            level[i] = (2 * i + 1 < below.size()) ? std::max (below[2 * i], below[2 * i + 1]) : below[2 * i];
        }

        outLevels.push_back (std::move (level));
    }
}
