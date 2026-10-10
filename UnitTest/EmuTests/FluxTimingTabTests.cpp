#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/FluxTimingTab.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTabTests
//
//  What the Flux timing tab draws from (FR-044, FR-045): the cells a
//  selection covers, a histogram over a range that crosses the index, and
//  the clusters that get a count.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FluxTimingTabTests)
{
public:

    static void AnalyzeStandard (TrackAnalysis & out)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeFluxCopy(), context, DecodeSettings::MakeStandard(), out);
    }



    TEST_METHOD (SelectedNibblesGiveTheirCells)
    {
        TrackAnalysis  track;
        uint32_t       first = 0;
        uint32_t       end   = 0;



        AnalyzeStandard (track);

        Assert::IsTrue (FluxTiming::GetSelectionCells (track, 10, 4, -1, first, end));
        Assert::AreEqual (track.framed.nibbles[10].startCell, first);
        Assert::AreEqual (track.framed.nibbles[14].startCell, end);
    }



    TEST_METHOD (WithNoNibblesTheSectorIsTheSelection)
    {
        TrackAnalysis  track;
        uint32_t       first   = 0;
        uint32_t       end     = 0;
        int            address = 0;
        int            data    = 0;



        AnalyzeStandard (track);
        address = track.sectors[3].addressField;
        data    = track.sectors[3].dataField;

        Assert::IsTrue (FluxTiming::GetSelectionCells (track, 0, 0, 3, first, end));
        Assert::AreEqual (track.framed.nibbles[track.fields[address].firstNibble].startCell, first);
        Assert::AreEqual (track.framed.nibbles[track.fields[data].firstNibble + track.fields[data].nibbleCount].startCell, end);
        Assert::IsFalse (FluxTiming::GetSelectionCells (track, 0, 0, -1, first, end), L"nothing selected");
    }



    TEST_METHOD (AHistogramRangeCanCrossTheIndex)
    {
        vector<FluxInterval>  intervals;
        FluxHistogram         histogram;



        for (uint32_t cell : { 1u, 5u, 50u, 95u, 99u })
        {
            intervals.push_back (FluxInterval { 0.0, 32.0, cell, false });
        }

        histogram = FluxTiming::BuildHistogram (intervals, 90, 10);

        Assert::AreEqual (4, histogram.total, L"cells 95 and 99, then 1 and 5 past the index");
    }



    TEST_METHOD (EachClusterGetsOneCount)
    {
        FluxHistogram  histogram;
        vector<int>    peaks;



        histogram.counts[31] = 900;
        histogram.counts[32] = 1000;
        histogram.counts[33] = 1000;
        histogram.counts[63] = 400;
        histogram.counts[95] = 50;
        histogram.peak       = 1000;
        peaks                = FluxTimingTab::FindPeaks (histogram);

        Assert::AreEqual (static_cast<size_t> (2), peaks.size(), L"the cluster at 95 is under a tenth of the peak");
        Assert::AreEqual (32, peaks[0], L"a flat top counts once, at its first bin");
        Assert::AreEqual (63, peaks[1]);
    }
};
