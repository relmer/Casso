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
        case PlatterKind::FailedChecksum: priority = 4; break;
        case PlatterKind::Noise:          priority = 3; break;
        case PlatterKind::RandomBits:     priority = 2; break;
        case PlatterKind::Other:          priority = 0; break;
        default:                          priority = 1; break;
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
//  a random-bit region takes that kind whatever nibble covers it, as on the
//  strip and in the tooltip. A track with no nibbles has the empty kind.
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
            outCells[(region.startCell + cell) % cells] = random;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::MapToTime
//
//  The platter finds a cell by its share of the turn, but a flux track's cells
//  differ in length, so each entry becomes the cell at that share of the
//  track's time. A bit track's cells are all one length and stay as they are.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCells::MapToTime (const TrackAnalysis & analysis, vector<Byte> & inOutCells)
{
    const FramedTrack &  framed  = analysis.framed;
    size_t               count   = inOutCells.size();
    size_t               i       = 0;
    uint32_t             cell    = 0;
    double               elapsed = 0;
    double               target  = 0;
    vector<Byte>         mapped  (count);



    if (framed.isFlux && framed.turnTicks > 0 && framed.cellTicks.size() >= count)
    {
        for (i = 0; i < count; i++)
        {
            target = static_cast<double> (i) / count * framed.turnTicks;

            while (cell + 1 < count && elapsed + framed.cellTicks[cell] <= target)
            {
                elapsed += framed.cellTicks[cell];
                cell++;
            }

            mapped[i] = inOutCells[cell];
        }

        inOutCells = std::move (mapped);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::BuildEdges
//
//  MapToTime can only change kind between entries, up to a cell away from where
//  the kind changes on a flux track. Taking `cells` before that mapping, each
//  entry gets how far along it the first change lies, in 255ths, or kNoEdge. The
//  platter draws the next entry's kind past that point.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCells::BuildEdges (const TrackAnalysis & analysis, const vector<Byte> & cells, vector<Byte> & outEdges)
{
    const FramedTrack &  framed  = analysis.framed;
    size_t               count   = cells.size();
    size_t               i       = 0;
    size_t               k       = 0;
    uint32_t             cell    = 0;
    double               elapsed = 0;
    double               width   = 0;
    double               target  = 0;
    double               end     = 0;



    outEdges.assign (count, kNoEdge);

    if (framed.isFlux && framed.turnTicks > 0 && framed.cellTicks.size() >= count)
    {
        width = static_cast<double> (framed.turnTicks) / count;

        for (i = 0; i < count; i++)
        {
            target = i * width;

            while (cell + 1 < count && elapsed + framed.cellTicks[cell] <= target)
            {
                elapsed += framed.cellTicks[cell];
                cell++;
            }

            end = elapsed + framed.cellTicks[cell];

            for (k = cell; k + 1 < count && end < target + width; k++)
            {
                if (cells[k + 1] != cells[cell])
                {
                    outEdges[i] = static_cast<Byte> ((end - target) / width * kNoEdge);
                    break;
                }

                end += framed.cellTicks[k + 1];
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::BuildLevels
//
//  Level 0 is the cells; each level after has the larger of each pair in
//  the one before, until a level has one byte.
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





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::IsEchoRing
//
//  Neighboring rings with one slot have one recording, which is centered on
//  its whole track, or on the run's middle ring when the run has no whole
//  track. Every other ring of the run is an echo of it.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterCells::IsEchoRing (std::span<const int> slots, int ring)
{
    return slots[ring] >= 0 && ring != GetCenterRing (slots, ring);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::GetCenterRing
//
//  The center ring of the recording on a ring: the whole track in the run
//  of rings with its slot, or the run's middle ring when the run has no whole
//  track.
//  A ring with no recording is returned as it is.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterCells::GetCenterRing (std::span<const int> slots, int ring)
{
    static constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;



    int  count  = static_cast<int> (slots.size());
    int  first  = ring;
    int  last   = ring;
    int  whole  = 0;
    int  center = ring;



    if (ring >= 0 && ring < count && slots[ring] >= 0)
    {
        while (first > 0 && slots[first - 1] == slots[ring])
        {
            first--;
        }

        while (last + 1 < count && slots[last + 1] == slots[ring])
        {
            last++;
        }

        whole  = (first + kPerTrack - 1) / kPerTrack * kPerTrack;
        center = (whole <= last) ? std::max (ring - ring % kPerTrack, whole) : (first + last) / 2;
    }

    return center;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells::StepRecording
//
//  The center ring of the next recording from a ring in the direction of
//  `step`, past rings that repeat a recording and rings with none; from a
//  ring that repeats a recording, that recording's center when it lies in
//  that direction. The ring itself when there is no next recording.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterCells::StepRecording (std::span<const int> slots, int ring, int step)
{
    int  count  = static_cast<int> (slots.size());
    int  from   = GetCenterRing (slots, ring);
    int  next   = ring + step;
    int  result = ring;



    while (next >= 0 && next < count && result == ring)
    {
        if (slots[next] >= 0 && !IsEchoRing (slots, next) && (next != from || ring != from))
        {
            result = next;
        }

        next += step;
    }

    return result;
}
