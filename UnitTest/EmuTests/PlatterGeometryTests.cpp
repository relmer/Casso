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



    TEST_METHOD (AViewHoldingTheCenterShowsEveryRingAndTheWholeTurn)
    {
        PlatterPlacement  view  = MakeView();
        RECT              rect  = { 0, 0, 1000, 1000 };
        int               first = -1;
        int               last  = -1;
        double            start = -1;
        double            end   = -1;



        PlatterGeometry::GetVisibleRings (view, rect, first, last);

        Assert::AreEqual (0, first);
        Assert::AreEqual (DiskImage::kQuarterTrackCount - 1, last);
        Assert::IsFalse  (PlatterGeometry::GetVisibleTurns (view, rect, start, end));
        Assert::AreEqual (0.0, start);
        Assert::AreEqual (1.0, end);
    }



    TEST_METHOD (AZoomedViewShowsOnlyTheRingsAndTurnsUnderIt)
    {
        PlatterPlacement  view  = MakeView();
        RECT              rect  = { 0, 0, 1000, 1000 };
        int               first = -1;
        int               last  = -1;
        double            start = -1;
        double            end   = -1;



        //  The disk 400 times larger with its rim at the top of the view, so
        //  the view straddles 12 o'clock on the outer rings.
        view.outerRadiusPx = 480.0f * 400.0f;
        view.centerYPx     = 100.0f + view.outerRadiusPx;
        view.rotation      = 0.25f;

        PlatterGeometry::GetVisibleRings (view, rect, first, last);

        Assert::AreEqual (0, first);
        Assert::IsTrue   (last > 0 && last < 8, L"only the outermost rings reach the view");
        Assert::IsTrue   (PlatterGeometry::GetVisibleTurns (view, rect, start, end));
        Assert::AreEqual (0.75, start + (end - start) / 2, 0.002, L"centered on 12 o'clock less the rotation");
        Assert::IsTrue   (end - start < 0.01, L"a sliver of the turn");
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


    //  SC-031: tilted to 0, 30 and 60 degrees, at fit and zoomed eight
    //  times, a hit at each of 100 points the projection draws a ring's
    //  middle at addresses that ring and turn, and Unproject undoes Project.
    TEST_METHOD (TiltedHitsAddressTheCellTheProjectionDrawsThere)
    {
        static constexpr double  kDegrees[] = { 0.0, 30.0, 60.0 };
        static constexpr double  kZooms[]   = { 1.0, 8.0 };
        static constexpr int     kPoints    = 100;
        static constexpr float   kFitPx     = 3000.0f;

        PlatterPlacement  view;
        PlatterHit        hit;
        float             x     = 0.0f;
        float             y     = 0.0f;
        double            dx    = 0.0;
        double            dy    = 0.0;
        double            turn  = 0.0;
        double            delta = 0.0;
        int               qt    = 0;
        int               k     = 0;



        for (double degrees : kDegrees)
        {
            for (double zoom : kZooms)
            {
                view.centerXPx     = 4000.0f;
                view.centerYPx     = 4000.0f;
                view.outerRadiusPx = static_cast<float> (kFitPx * zoom);
                view.rotation      = 0.25f;
                view.tilt          = static_cast<float> (degrees * 3.141592653589793 / 180.0);

                for (k = 0; k < kPoints; k++)
                {
                    qt   = (k * 13) % (DiskImage::kQuarterTrackCount - 1);
                    turn = std::fmod (k * 0.6180339887, 1.0);

                    PlatterGeometry::ProjectAt (view, turn, PlatterGeometry::GetRingMiddle (qt) * view.outerRadiusPx, x, y);
                    hit   = PlatterGeometry::HitTest (view, { static_cast<LONG> (std::floor (x)), static_cast<LONG> (std::floor (y)) });
                    delta = hit.turn - turn;
                    delta -= std::round (delta);

                    Assert::IsTrue   (hit.isOnDisk);
                    Assert::AreEqual (qt, hit.quarterTrack, std::format (L"{} degrees, {}x, point {}", degrees, zoom, k).c_str());
                    Assert::IsTrue   (std::abs (delta) < 1e-3, std::format (L"turn at {} degrees, {}x, point {}", degrees, zoom, k).c_str());

                    Assert::IsTrue   (PlatterGeometry::Unproject (view, x, y, dx, dy));
                    PlatterGeometry::Project (view, dx, dy, x, y);
                    Assert::IsTrue   (PlatterGeometry::Unproject (view, x, y, dx, dy));
                }
            }
        }
    }



    //  Tilted, the near side is drawn larger than the far side.
    TEST_METHOD (TheTiltedNearSideIsLarger)
    {
        PlatterPlacement  view;
        float             x     = 0.0f;
        float             nearY = 0.0f;
        float             farY  = 0.0f;



        view.centerXPx     = 500.0f;
        view.centerYPx     = 500.0f;
        view.outerRadiusPx = 400.0f;
        view.tilt          = 0.5f;

        PlatterGeometry::Project (view, 0.0, 400.0,  x, nearY);
        PlatterGeometry::Project (view, 0.0, -400.0, x, farY);

        Assert::IsTrue (nearY - 500.0f > 500.0f - farY, L"the near rim is farther from the center on screen");
        Assert::IsTrue (PlatterGeometry::GetScaleAt (view, 400.0) > 1.0);
        Assert::IsTrue (PlatterGeometry::GetScaleAt (view, -400.0) < 1.0);
    }



    TEST_METHOD (APixelOfTheDiskIsDrawnSmallerOnTheFarSideWhenTilted)
    {
        PlatterPlacement  view;



        view.centerXPx     = 500.0f;
        view.centerYPx     = 500.0f;
        view.outerRadiusPx = 400.0f;

        Assert::AreEqual (1.0, PlatterGeometry::GetLocalScale (view, 0.0, 300.0), 1e-9, L"flat");

        view.tilt = 0.5f;

        Assert::IsTrue (PlatterGeometry::GetLocalScale (view, 0.0, 300.0) < PlatterGeometry::GetLocalScale (view, 0.5, 300.0),
                        L"the top of the disk is its far side");
    }
};
