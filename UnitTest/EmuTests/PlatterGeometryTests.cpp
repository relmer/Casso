#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/PlatterGeometry.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometryTests
//
//  The platter's rings and angles (FR-022) and what a point on it hits
//  (FR-028): a ring per quarter track with track 0 at the rim, the angle
//  clockwise from 12 o'clock, and the sector whose field is under the point.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PlatterGeometryTests)
{
public:

    static PlatterPlacement MakeView()
    {
        PlatterPlacement  view;



        view.centerXPx     = 500;
        view.centerYPx     = 500;
        view.outerRadiusPx = 480;

        return view;
    }



    //  The pixel at a quarter track's middle and the given turn.
    static POINT PointAt (const PlatterPlacement & view, int quarterTrack, double turn)
    {
        double  r = PlatterGeometry::GetRingMiddle (quarterTrack) * view.outerRadiusPx;
        double  a = turn * 6.283185307179586;



        return { static_cast<LONG> (std::floor (view.centerXPx + r * std::sin (a))), static_cast<LONG> (std::floor (view.centerYPx - r * std::cos (a))) };
    }



    TEST_METHOD (Track0IsAtTheRimAndTheLastQuarterTrackAtTheHub)
    {
        Assert::AreEqual (0, PlatterGeometry::GetQuarterTrackAt (0.9999));
        Assert::AreEqual (DiskImage::kQuarterTrackCount - 1, PlatterGeometry::GetQuarterTrackAt (PlatterRenderer::kInnerFraction + 1e-6));
        Assert::AreEqual (-1, PlatterGeometry::GetQuarterTrackAt (1.01), L"off the rim");
        Assert::AreEqual (-1, PlatterGeometry::GetQuarterTrackAt (0.2),  L"in the hub");

        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            Assert::AreEqual (qt, PlatterGeometry::GetQuarterTrackAt (PlatterGeometry::GetRingMiddle (qt)));
        }
    }



    TEST_METHOD (TheTurnRunsClockwiseFrom12OClock)
    {
        Assert::AreEqual (0.0,  PlatterGeometry::GetTurnAt ( 0, -1), 1e-9);
        Assert::AreEqual (0.25, PlatterGeometry::GetTurnAt ( 1,  0), 1e-9, L"3 o'clock");
        Assert::AreEqual (0.5,  PlatterGeometry::GetTurnAt ( 0,  1), 1e-9, L"6 o'clock");
        Assert::AreEqual (0.75, PlatterGeometry::GetTurnAt (-1,  0), 1e-9, L"9 o'clock");
    }



    TEST_METHOD (AHitGivesTheQuarterTrackAndTurnUnderThePoint)
    {
        PlatterPlacement  view = MakeView();
        PlatterHit        hit;



        hit = PlatterGeometry::HitTest (view, PointAt (view, 68, 0.3));
        Assert::IsTrue (hit.isOnDisk);
        Assert::AreEqual (68, hit.quarterTrack);
        Assert::AreEqual (0.3, hit.turn, 0.005);

        view.rotation = 0.25;
        hit = PlatterGeometry::HitTest (view, PointAt (view, 68, 0.3));
        Assert::AreEqual (0.05, hit.turn, 0.005, L"a turned platter moves the track under the point");

        Assert::IsFalse (PlatterGeometry::HitTest (view, { 500, 500 }).isOnDisk, L"the hub");
    }



    TEST_METHOD (GroovesRunOutsideEachWholeTrack)
    {
        Assert::IsTrue  (PlatterGeometry::IsGrooveOutside (0));
        Assert::IsFalse (PlatterGeometry::IsGrooveOutside (1));
        Assert::IsTrue  (PlatterGeometry::IsGrooveOutside (68));
    }



    TEST_METHOD (AHitSelectsTheSectorWhoseFieldIsUnderIt)
    {
        InspectorTrackBuilder                            builder;
        TrackAnalysis                                    analysis;
        TrackContext                                     context;
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes    = {};
        uint32_t                                         cell     = 0;
        int                                              s        = 0;



        //  Two fields with sector number 3: a hit on the second gives the second.
        builder.AppendSync (48);

        for (s = 0; s < 3; s++)
        {
            builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 17, s == 2 ? 3 : static_cast<Byte> (s + 3));
            builder.AppendSync         (6);
            builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
            builder.AppendSync         (20);
        }

        context.physicalTrack = 17;
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), analysis);

        cell = analysis.fields[analysis.sectors[2].dataField].startCell + 40;

        Assert::AreEqual (2, PlatterGeometry::GetSectorAt (analysis, cell));
        Assert::AreEqual (3, static_cast<int> (analysis.sectors[PlatterGeometry::GetSectorAt (analysis, cell)].sector));
        Assert::AreEqual (-1, PlatterGeometry::GetSectorAt (analysis, 4), L"the sync before the first field");
        Assert::AreEqual (cell, PlatterGeometry::GetCellAtTurn (analysis, TrackAnalyzer::GetAngle (analysis, cell)));
        Assert::AreEqual (0, PlatterGeometry::GetNibbleAt (analysis, 3));
        Assert::AreEqual (static_cast<int> (analysis.framed.nibbles.size()) - 1, PlatterGeometry::GetNibbleAt (analysis, analysis.framed.cellCount - 1));
    }
};
