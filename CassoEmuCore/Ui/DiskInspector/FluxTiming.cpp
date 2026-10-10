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
//  measured from the index.
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

            cells += std::max<uint64_t> (1, (gap * FluxTrack::kCellDenominator + FluxTrack::kCellNumerator / 2) / FluxTrack::kCellNumerator);

            interval.turn         = static_cast<double> (tick) / framed.turnTicks;
            interval.ticks        = static_cast<double> (gap);
            interval.cell         = static_cast<uint32_t> (cells - 1);
            interval.isWithinCell = gap < TrackAnalyzer::kNominalCellTicks;

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
//  when endCell is past its last cell.
//
////////////////////////////////////////////////////////////////////////////////

FluxHistogram FluxTiming::BuildHistogram (const vector<FluxInterval> & intervals, uint32_t firstCell, uint32_t endCell)
{
    FluxHistogram  histogram;



    for (const FluxInterval & interval : intervals)
    {
        if (interval.cell >= firstCell && interval.cell < endCell)
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
