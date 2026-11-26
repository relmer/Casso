#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/StripHitTest.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTestTests
//
//  What is under a point on the strip: a nibble, a sector's bracket, the
//  write seam, and the strip's zooms.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StripHitTestTests)
{
public:

    static constexpr float  kWidth     = 1000.0f;
    static constexpr float  kWideWidth = 1000000.0f;   // wide enough for every nibble to span pixels

    //  A standard 16-sector track, analyzed.
    static void Analyze (TrackAnalysis & outAnalysis)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeFluxCopy(), context, DecodeSettings::MakeStandard(), outAnalysis);
    }


    TEST_METHOD (NibbleAt_FindsTheNibbleUnderThePoint)
    {
        TrackAnalysis  track;
        StripHitTest   hits;
        StripGeometry  g (0.0, 1.0, 0.0f, kWideWidth);
        size_t         i = 0;



        Analyze (track);
        hits.Update (track);

        for (i = 0; i + 1 < hits.GetTurns().size(); i += 211)
        {
            Assert::AreEqual ((int) i, hits.GetNibbleAt (g, g.GetX ((hits.GetTurns()[i] + hits.GetTurns()[i + 1]) / 2)));
        }
    }


    TEST_METHOD (SectorAt_FindsEachSectorsBracket)
    {
        TrackAnalysis                track;
        StripHitTest                 hits;
        StripGeometry                g (0.0, 1.0, 0.0f, kWidth);
        std::array<StripSegment, 2>  parts = {};
        double                       a     = 0;
        double                       b     = 0;



        Analyze (track);
        hits.Update (track);

        Assert::IsFalse (track.sectors.empty());

        for (int s = 0; s < (int) track.sectors.size(); s++)
        {
            Assert::IsTrue (hits.GetSectorParts (track, g, s, a, b, parts) > 0);
            Assert::AreEqual (s, hits.GetSectorAt (track, g, (parts[0].x0 + parts[0].x1) / 2));
        }
    }


    TEST_METHOD (Seam_IsTheLongestSyncRun)
    {
        TrackAnalysis                track;
        StripHitTest                 hits;
        StripGeometry                g (0.0, 1.0, 0.0f, kWidth);
        std::array<StripSegment, 2>  parts = {};



        Analyze (track);
        hits.Update (track);

        Assert::IsTrue (hits.GetSeamSegments (track, g, parts) > 0);
        Assert::IsTrue (hits.IsOverSeam (track, g, (parts[0].x0 + parts[0].x1) / 2));
    }


    TEST_METHOD (CellsInView_CoverTheWholeTrack)
    {
        TrackAnalysis  track;
        StripHitTest   hits;
        uint32_t       first = 1;
        uint32_t       last  = 0;



        Analyze (track);
        hits.Update (track);
        hits.GetCellsInView (track, 0.0, 1.0, first, last);

        Assert::AreEqual (0u, first);
        Assert::AreEqual (track.framed.cellCount - 1, last);
    }


    TEST_METHOD (ZoomStops_StartAtTheWholeTrack)
    {
        TrackAnalysis   track;
        DxuiDpiScaler   scaler;
        vector<double>  stops;



        Analyze (track);
        scaler.SetDpi (96);
        stops = StripHitTest::GetZoomStops (track, kWidth, 0.0f, scaler);

        Assert::IsTrue  (stops.size() > 2);
        Assert::AreEqual (1.0, stops.front());
        Assert::IsTrue  (std::is_sorted (stops.begin(), stops.end()));
    }
};
