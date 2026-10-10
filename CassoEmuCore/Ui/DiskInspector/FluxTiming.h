#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxInterval / FluxHistogram
//
//  One transition of a flux track: when it falls as a fraction of the turn,
//  the time since the one before it in 125 ns ticks, the cell it lands in,
//  and whether it came within one cell of the one before, which the drive
//  reads as a single transition. And a histogram of the intervals.
//
////////////////////////////////////////////////////////////////////////////////

struct FluxInterval
{
    double    turn         = 0.0;
    double    ticks        = 0.0;
    uint32_t  cell         = 0;
    bool      isWithinCell = false;
};


struct FluxHistogram
{
    static constexpr double  kBinMicroseconds = 0.125;
    static constexpr int     kBinCount        = 128;

    std::array<int, kBinCount>  counts = {};
    int                         total  = 0;
    int                         peak   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTiming
//
//  Timing figures for the inspector's Timing mode and Flux timing tab
//  (FR-024, FR-044, FR-045): each cell's deviation from the nominal 3.91 µs
//  cell, each transition's interval, and histograms of the intervals for
//  the whole track or a stretch of cells.
//
////////////////////////////////////////////////////////////////////////////////

class FluxTiming
{
public:
    //  Deviations are stored as a byte for the platter: 128 at nominal, and
    //  kFullScale (±25%) at 1 and 255.
    static constexpr double  kFullScale = 0.25;
    static constexpr Byte    kNominal   = 128;

    static double  GetDeviation      (double cellTicks);
    static Byte    EncodeDeviation   (double deviation);
    static double  DecodeDeviation   (Byte value);
    static void    BuildDeviations   (const TrackAnalysis & track, vector<Byte> & outCells);
    static void    BuildLevels       (const vector<Byte> & cells, vector<vector<Byte>> & outLevels);

    static void           BuildIntervals  (const TrackAnalysis & track, vector<FluxInterval> & outIntervals);
    static FluxHistogram  BuildHistogram  (const vector<FluxInterval> & intervals, uint32_t firstCell, uint32_t endCell);
    static int            GetBin          (double ticks);

    //  Each cell's start as a fraction of the turn, and one entry past the
    //  last; a flux track places cells by their times, a bit track evenly.
    static void      BuildCellTurns   (const TrackAnalysis & track, vector<double> & outTurns);
    static uint32_t  GetCellAt        (const vector<double> & cellTurns, double turn);
    static double    GetMeanDeviation (const TrackAnalysis & track, uint32_t firstCell, uint32_t endCell);
};
