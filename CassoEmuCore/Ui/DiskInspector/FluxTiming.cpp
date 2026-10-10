#include "Pch.h"

#include "Ui/DiskInspector/FluxTiming.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr double  s_kTicksPerMicrosecond = 8.0;
static constexpr double  s_kHalfScale           = 127.0;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::GetDeviation
//
//  A cell's time against the nominal cell: -0.05 is 5% fast, 0.05 5% slow.
//
////////////////////////////////////////////////////////////////////////////////

double FluxTiming::GetDeviation (double cellTicks)
{
    return cellTicks / TrackAnalyzer::kNominalCellTicks - 1.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::EncodeDeviation
//
////////////////////////////////////////////////////////////////////////////////

Byte FluxTiming::EncodeDeviation (double deviation)
{
    double  scaled = std::clamp (deviation / kFullScale, -1.0, 1.0) * s_kHalfScale;



    return static_cast<Byte> (std::lround (kNominal + scaled));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::DecodeDeviation
//
////////////////////////////////////////////////////////////////////////////////

double FluxTiming::DecodeDeviation (Byte value)
{
    return (static_cast<double> (value) - kNominal) / s_kHalfScale * kFullScale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::BuildDeviations
//
//  A byte per cell, laid out as PlatterCells lays out kinds, so the two
//  share their level offsets. A track without cell times is all nominal.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTiming::BuildDeviations (const TrackAnalysis & track, vector<Byte> & outCells)
{
    const FramedTrack &  framed = track.framed;
    uint32_t             cell   = 0;



    outCells.assign (framed.cellCount, kNominal);

    if (framed.isFlux && framed.cellTicks.size() >= framed.cellCount)
    {
        for (cell = 0; cell < framed.cellCount; cell++)
        {
            outCells[cell] = EncodeDeviation (GetDeviation (framed.cellTicks[cell]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::BuildLevels
//
//  Coarser levels hold the deviation furthest from nominal in each pair, so
//  one odd cell still shows at fit, as PlatterCells keeps the most important
//  kind.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTiming::BuildLevels (const vector<Byte> & cells, vector<vector<Byte>> & outLevels)
{
    size_t  i = 0;



    outLevels.clear();
    outLevels.push_back (cells);

    while (outLevels.back().size() > 1 && static_cast<int> (outLevels.size()) < PlatterCells::kMaxLevels)
    {
        const vector<Byte> &  below = outLevels.back();
        vector<Byte>          level ((below.size() + 1) / 2);

        for (i = 0; i < level.size(); i++)
        {
            Byte  a = below[2 * i];
            Byte  b = (2 * i + 1 < below.size()) ? below[2 * i + 1] : a;

            level[i] = (std::abs (a - kNominal) >= std::abs (b - kNominal)) ? a : b;
        }

        outLevels.push_back (std::move (level));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::BuildIntervals
//
//  Each transition at its recorded time, not rounded to cells; the first is
//  measured from the index. A transition under half a cell after the one
//  before rounds into the same cell, which a drive reads as one transition
//  and Casso's drive moves on to the next cell.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTiming::BuildIntervals (const TrackAnalysis & track, vector<FluxInterval> & outIntervals)
{
    const FramedTrack &  framed = track.framed;
    uint64_t             prev   = 0;
    uint64_t             cells  = 0;



    outIntervals.clear();

    if (framed.isFlux && framed.turnTicks > 0)
    {
        for (uint64_t tick : framed.transitionTicks)
        {
            FluxInterval  interval;
            uint64_t      gap      = tick - prev;
            uint64_t      rounded  = (gap * FluxTrack::kCellDenominator + FluxTrack::kCellNumerator / 2) / FluxTrack::kCellNumerator;

            cells += std::max<uint64_t> (1, rounded);

            interval.turn         = static_cast<double> (tick) / framed.turnTicks;
            interval.ticks        = static_cast<double> (gap);
            interval.cell         = static_cast<uint32_t> (cells - 1);
            interval.isWithinCell = rounded == 0;

            outIntervals.push_back (interval);
            prev = tick;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::BuildHistogram
//
//  Intervals whose transition lands in [firstCell, endCell); the whole track
//  when endCell is past its last cell, and a range across the index when
//  endCell is below firstCell.
//
////////////////////////////////////////////////////////////////////////////////

FluxHistogram FluxTiming::BuildHistogram (const vector<FluxInterval> & intervals, uint32_t firstCell, uint32_t endCell)
{
    FluxHistogram  histogram;
    bool           isWrapped = endCell < firstCell;



    for (const FluxInterval & interval : intervals)
    {
        if (isWrapped ? (interval.cell >= firstCell || interval.cell < endCell) : (interval.cell >= firstCell && interval.cell < endCell))
        {
            int  bin = GetBin (interval.ticks);

            histogram.counts[bin]++;
            histogram.total++;
            histogram.peak = std::max (histogram.peak, histogram.counts[bin]);
        }
    }

    return histogram;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::GetBin
//
////////////////////////////////////////////////////////////////////////////////

int FluxTiming::GetBin (double ticks)
{
    int  bin = static_cast<int> (ticks / s_kTicksPerMicrosecond / FluxHistogram::kBinMicroseconds);



    return std::clamp (bin, 0, FluxHistogram::kBinCount - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::BuildCellTurns
//
////////////////////////////////////////////////////////////////////////////////

void FluxTiming::BuildCellTurns (const TrackAnalysis & track, vector<double> & outTurns)
{
    const FramedTrack &  framed  = track.framed;
    bool                 isTimed = framed.isFlux && framed.turnTicks > 0 && framed.cellTicks.size() >= framed.cellCount;
    double               elapsed = 0;
    uint32_t             cell    = 0;



    outTurns.resize (static_cast<size_t> (framed.cellCount) + 1);

    for (cell = 0; cell <= framed.cellCount; cell++)
    {
        outTurns[cell] = isTimed ? elapsed / framed.turnTicks : static_cast<double> (cell) / std::max<uint32_t> (framed.cellCount, 1);

        if (isTimed && cell < framed.cellCount)
        {
            elapsed += framed.cellTicks[cell];
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::GetCellAt
//
//  The cell under a point of the turn, wrapped into the track.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FluxTiming::GetCellAt (const vector<double> & cellTurns, double turn)
{
    double    wrapped = turn - std::floor (turn);
    uint32_t  cell    = 0;



    if (cellTurns.size() > 1)
    {
        auto  after = std::upper_bound (cellTurns.begin(), cellTurns.end() - 1, wrapped);

        cell = (after == cellTurns.begin()) ? 0 : static_cast<uint32_t> (after - cellTurns.begin()) - 1;
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::GetMeanDeviation
//
//  The mean cell's deviation over [firstCell, endCell), which may wrap past
//  the index; zero on a track that records no timing.
//
////////////////////////////////////////////////////////////////////////////////

double FluxTiming::GetMeanDeviation (const TrackAnalysis & track, uint32_t firstCell, uint32_t endCell)
{
    const FramedTrack &  framed = track.framed;
    uint32_t             count  = 0;
    uint32_t             k      = 0;
    double               ticks  = 0;
    double               mean   = 0;



    if (framed.isFlux && framed.cellCount > 0 && framed.cellTicks.size() >= framed.cellCount)
    {
        count = (endCell + framed.cellCount - firstCell % framed.cellCount) % framed.cellCount;
        count = (count == 0) ? 1 : count;

        for (k = 0; k < count; k++)
        {
            ticks += framed.cellTicks[(firstCell + k) % framed.cellCount];
        }

        mean = GetDeviation (ticks / count);
    }

    return mean;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming::GetSelectionCells
//
//  The cells the selection covers: the selected nibbles, or the selected
//  sector from its address prologue to the end of its last field when no
//  nibbles are selected (FR-045). The range wraps when it crosses the index.
//
////////////////////////////////////////////////////////////////////////////////

bool FluxTiming::GetSelectionCells (const TrackAnalysis & track, int firstNibble, int nibbleCount, int sectorIndex, uint32_t & outFirst, uint32_t & outEnd)
{
    const FramedTrack &  framed  = track.framed;
    int                  n       = static_cast<int> (framed.nibbles.size());
    int                  first   = -1;
    int                  end     = -1;
    bool                 isFound = false;



    if (n > 0 && framed.cellCount > 0 && nibbleCount > 0 && firstNibble >= 0 && firstNibble < n)
    {
        first = firstNibble;
        end   = firstNibble + nibbleCount;
    }
    else if (n > 0 && framed.cellCount > 0 && sectorIndex >= 0 && sectorIndex < static_cast<int> (track.sectors.size()))
    {
        const AnalyzedSector &  sector = track.sectors[sectorIndex];
        const LocatedField &    last   = track.fields[sector.dataField >= 0 ? sector.dataField : sector.addressField];

        first = track.fields[sector.addressField].firstNibble;
        end   = last.firstNibble + last.nibbleCount;
    }

    if (first >= 0)
    {
        outFirst = framed.nibbles[first % n].startCell % framed.cellCount;
        outEnd   = framed.nibbles[end % n].startCell % framed.cellCount;

        //  A selection of the whole turn starts and ends on the same cell.
        if (outEnd == outFirst)
        {
            outFirst = 0;
            outEnd   = UINT32_MAX;
        }

        isFound  = true;
    }

    return isFound;
}
