#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTimingTests
//
//  Flux timing figures (FR-024, FR-044, FR-045) on a made-up flux track
//  whose first half has a transition every 30 ticks (3.75 microseconds) and whose
//  second half one every 33 (4.125 microseconds), so its cells run about 4% fast and
//  then about 5% slow against the nominal 3.91 microsecond cell.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FluxTimingTests)
{
public:

    static constexpr int  kTransitions = 25000;
    static constexpr int  kFastTicks   = 30;
    static constexpr int  kSlowTicks   = 33;



    static void AnalyzeMixed (TrackAnalysis & out)
    {
        auto          copy = std::make_shared<TrackCopy>();
        TrackContext  context;
        int           i    = 0;



        copy->kind = TrackKind::Flux;

        for (i = 0; i < kTransitions; i++)
        {
            copy->fluxBytes.push_back (static_cast<Byte> (i < kTransitions / 2 ? kFastTicks : kSlowTicks));
        }

        TrackAnalyzer::Analyze (*copy, context, DecodeSettings::MakeStandard(), out);
    }



    TEST_METHOD (CellsDeviateFromTheNominalCell)
    {
        TrackAnalysis  track;
        vector<Byte>   cells;



        AnalyzeMixed (track);
        FluxTiming::BuildDeviations (track, cells);

        Assert::AreEqual (static_cast<size_t> (track.framed.cellCount), cells.size());
        Assert::AreEqual (-0.041, FluxTiming::DecodeDeviation (cells[10]),               0.005, L"the first half runs fast");
        Assert::AreEqual ( 0.055, FluxTiming::DecodeDeviation (cells[cells.size() - 10]), 0.005, L"the second half runs slow");
        Assert::AreEqual (FluxTiming::kNominal, FluxTiming::EncodeDeviation (0.0));
        Assert::AreEqual (static_cast<Byte> (255), FluxTiming::EncodeDeviation (0.9), L"clamped to full scale");
    }



    TEST_METHOD (CoarserLevelsKeepTheFurthestDeviation)
    {
        vector<Byte>          cells (64, FluxTiming::kNominal);
        vector<vector<Byte>>  levels;



        cells[33] = FluxTiming::EncodeDeviation (-0.2);
        FluxTiming::BuildLevels (cells, levels);

        Assert::AreEqual (static_cast<size_t> (7), levels.size());
        Assert::AreEqual (cells[33], levels.back()[0], L"one fast cell shows at the coarsest level");
    }



    TEST_METHOD (TheHistogramPeaksAtTheWrittenIntervals)
    {
        TrackAnalysis         track;
        vector<FluxInterval>  intervals;
        FluxHistogram         whole;
        FluxHistogram         firstHalf;



        AnalyzeMixed (track);
        FluxTiming::BuildIntervals (track, intervals);
        whole     = FluxTiming::BuildHistogram (intervals, 0, UINT32_MAX);
        firstHalf = FluxTiming::BuildHistogram (intervals, 0, intervals[kTransitions / 2].cell);

        Assert::AreEqual (static_cast<size_t> (kTransitions), intervals.size());
        Assert::AreEqual (kTransitions, whole.total);
        Assert::AreEqual (kTransitions / 2, whole.counts[FluxTiming::GetBin (kFastTicks)]);
        Assert::AreEqual (kTransitions / 2, whole.counts[FluxTiming::GetBin (kSlowTicks)]);
        Assert::AreEqual (kTransitions / 2, firstHalf.total, L"a stretch of cells counts only its own transitions");
        Assert::AreEqual (0, firstHalf.counts[FluxTiming::GetBin (kSlowTicks)]);
        Assert::IsTrue (intervals[5].isWithinCell, L"30 ticks is less than one 31.3-tick cell");
        Assert::IsFalse (intervals[kTransitions - 5].isWithinCell);
    }



    TEST_METHOD (ABitTrackHasNoIntervals)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          track;
        TrackContext           context;
        vector<FluxInterval>   intervals;
        vector<Byte>           cells;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), track);
        FluxTiming::BuildIntervals  (track, intervals);
        FluxTiming::BuildDeviations (track, cells);

        Assert::IsTrue (intervals.empty());
        Assert::IsTrue (std::all_of (cells.begin(), cells.end(), [] (Byte b) { return b == FluxTiming::kNominal; }));
    }
};
