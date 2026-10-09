#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer
//
//  Turns one track copy into a TrackAnalysis: frames it as the drive does,
//  finds its fields with the decode settings for its track, gives every
//  nibble a kind, pairs fields into sectors, classifies the record, measures
//  it, and lists what it finds. Pure, so the hosts run it on worker threads
//  and the tests run it directly.
//
////////////////////////////////////////////////////////////////////////////////

class TrackAnalyzer
{
public:
    //  More than this share of a track's cells in noise or random-bit regions
    //  makes it unformatted rather than nonstandard.
    static constexpr double  kUnformattedShare = 0.5;

    //  The nominal cell, about 3.91 microseconds, in 125 ns flux ticks.
    static constexpr double  kNominalCellTicks = static_cast<double> (FluxTrack::kCellNumerator) / static_cast<double> (FluxTrack::kCellDenominator);

    static void    Analyze  (const TrackCopy & copy, const TrackContext & context, const DecodeSettings & settings, TrackAnalysis & out);
    static double  GetAngle (const TrackAnalysis & analysis, uint32_t cell);

private:
    static void  KindNibbles      (TrackAnalysis & inOut, const DecodeChecks & checks);
    static void  KindField        (TrackAnalysis & inOut, int fieldIndex, const DecodeChecks & checks);
    static void  BuildSectors     (TrackAnalysis & inOut, const DecodeChecks & checks);
    static void  MapLogical       (int physicalTrack, AnalyzedSector & inOut);
    static void  Classify         (TrackAnalysis & inOut);
    static void  CountSectors     (TrackAnalysis & inOut);
    static void  Measure          (TrackAnalysis & inOut);
    static void  MeasureGaps      (TrackAnalysis & inOut);
    static void  FindFieldIssues  (TrackAnalysis & inOut, const DecodeChecks & checks);
    static void  FindSectorIssues (TrackAnalysis & inOut);
    static void  FindStrayMarks   (TrackAnalysis & inOut);
    static void  AddFinding       (TrackAnalysis & inOut, FindingCategory category, FindingKind kind, int field, int value, int value2);
};
