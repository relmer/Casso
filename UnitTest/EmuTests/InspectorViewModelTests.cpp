#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/InspectorViewModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModelTests
//
//  Selection (FR-038, FR-039), the platter's zoom and pan (FR-025), and the
//  state lasting only while the window shows one disk (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorViewModelTests)
{
public:

    static void AnalyzeDsk (DiskAnalysis & out)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 7, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    TEST_METHOD (ATrackChosenOnItsOwnSelectsItsFirstSectorInPassingOrder)
    {
        DiskAnalysis        analysis;
        InspectorViewModel  model;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.SelectQuarterTrack (4 * 17);

        Assert::AreEqual (68, model.GetQuarterTrack());
        Assert::IsNotNull (model.GetSector());
        Assert::AreEqual (0, model.GetSector()->passingIndex);

        model.SelectQuarterTrack (DiskImage::kQuarterTrackCount - 1);
        Assert::AreEqual (analysis.entries[DiskImage::kQuarterTrackCount - 1].slot < 0, model.GetSector() == nullptr, L"a quarter track that plays no record has no sector");
    }



    TEST_METHOD (ATrackWithNoStandardSectorsShowsWholeInTheStrip)
    {
        DiskAnalysis                    analysis;
        DiskAnalysis                    pending;
        InspectorViewModel              model;
        int                             slot   = 0;
        std::shared_ptr<TrackAnalysis>  track;



        AnalyzeDsk (analysis);
        slot                  = analysis.entries[0].slot;
        track                 = std::make_shared<TrackAnalysis> (*analysis.tracks[slot]);
        track->sectorsFound   = 0;
        analysis.tracks[slot] = track;

        model.SetDisk     (7);
        model.SetAnalysis (&pending);
        Assert::AreEqual (InspectorViewModel::kDefaultStripSpan, model.GetStripSpan(), L"no track analyzed yet");

        model.SetAnalysis (&analysis);
        Assert::AreEqual (1.0, model.GetStripSpan(), L"the first track, once analyzed");

        model.SelectQuarterTrack (8);
        Assert::AreEqual (InspectorViewModel::kDefaultStripSpan, model.GetStripSpan(), L"a track with sectors returns to the usual span");

        model.SelectQuarterTrack (0);
        Assert::AreEqual (1.0, model.GetStripSpan(), L"a track chosen later");
    }



    TEST_METHOD (RangesExtendFromWhereTheyStartedInEitherDirection)
    {
        DiskAnalysis        analysis;
        InspectorViewModel  model;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.SelectNibbles (0, 100, 1);
        model.ExtendNibbles (110);

        Assert::AreEqual (100, model.GetFirstNibble());
        Assert::AreEqual (11,  model.GetNibbleCount());

        model.ExtendNibbles (95);

        Assert::AreEqual (95, model.GetFirstNibble(), L"back past the start, the range turns around it");
        Assert::AreEqual (6,  model.GetNibbleCount());

        model.SelectAllNibbles();

        Assert::AreEqual (0, model.GetFirstNibble());
        Assert::AreEqual (static_cast<int> (model.GetTrack()->framed.nibbles.size()), model.GetNibbleCount());
    }



    TEST_METHOD (BytesSelectWithinTheSectorAndClearWithANewSector)
    {
        DiskAnalysis        analysis;
        InspectorViewModel  model;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.SelectSector (0, 2);
        model.SelectBytes  (0x40, 1, true);
        model.ExtendBytes  (0x3C);

        Assert::AreEqual (0x3C, model.GetFirstByte());
        Assert::AreEqual (5,    model.GetByteCount());
        Assert::IsTrue   (model.IsTextColumn());

        model.SelectAllBytes();
        Assert::AreEqual (256, model.GetByteCount());

        model.SelectSector (0, 3);
        Assert::AreEqual (0, model.GetByteCount());
    }


    TEST_METHOD (ClickingANibbleSelectsItsSector)
    {
        DiskAnalysis           analysis;
        InspectorViewModel     model;
        const TrackAnalysis *  track    = nullptr;
        int                    nibble   = 0;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.SelectQuarterTrack (0);

        track  = model.GetTrack();
        nibble = track->fields[track->sectors[5].dataField].bodyNibble + 10;
        model.SelectNibbles (0, nibble, 1);

        Assert::AreEqual (5, model.GetSectorIndex());
        Assert::AreEqual (nibble, model.GetFirstNibble());
    }



    TEST_METHOD (ReanalysisKeepsTheSectorNearestTheSameCell)
    {
        DiskAnalysis        analysis;
        DiskAnalysis        again;
        InspectorViewModel  model;



        AnalyzeDsk (analysis);
        AnalyzeDsk (again);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.SelectSector (8, 9);

        model.SetAnalysis (&again);
        Assert::AreEqual (9, model.GetSectorIndex());

        model.SetDisk (8);
        Assert::AreEqual (0, model.GetQuarterTrack(), L"another disk starts over");
        Assert::AreEqual (-1, model.GetSectorIndex());
    }



    TEST_METHOD (ZoomKeepsThePointUnderThePointerAndFitRecenters)
    {
        InspectorViewModel           model;
        InspectorViewModel::Point    anchor   = { 0.5, -0.5 };
        InspectorViewModel::Point    before;
        InspectorViewModel::Point    after;



        model.ZoomAbout (4.0, anchor);
        Assert::AreEqual (4.0, model.GetZoom());

        before = model.GetDiskPoint (0, 0.1);
        model.ZoomAbout (8.0, before);
        after  = model.GetDiskPoint (0, 0.1);

        Assert::AreEqual (before.x, after.x, 1e-9);
        Assert::AreEqual (before.y, after.y, 1e-9);

        model.ZoomAbout (1000000.0, anchor);
        Assert::AreEqual (InspectorViewModel::kMaxZoom, model.GetZoom());
        Assert::IsTrue   (InspectorViewModel::kMaxZoom >= 600.0, L"the platter zooms to at least 600 times");

        model.ZoomAbout (0.5, anchor);
        Assert::IsTrue (model.IsAtFit());
        Assert::AreEqual (0.0, model.GetPan().x);
        Assert::AreEqual (0.0, model.GetPan().y);
    }



    TEST_METHOD (PanningStopsWithTheDisksEdgeAtTheCenter)
    {
        InspectorViewModel  model;



        model.PanBy ({ 5.0, 0.0 });
        Assert::AreEqual (0.0, model.GetPan().x, L"no pan at fit");

        model.ZoomAbout (3.0, { 0.0, 0.0 });
        model.PanBy ({ 50.0, -50.0 });

        Assert::AreEqual ( 3.0, model.GetPan().x);
        Assert::AreEqual (-3.0, model.GetPan().y);
    }



    TEST_METHOD (ASelectionMovesTheStripButNotThePlatter)
    {
        DiskAnalysis                analysis;
        InspectorViewModel          model;
        InspectorViewModel::Point   pan;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.ZoomAbout   (50.0, model.GetDiskPoint (0, 0.0));
        pan = model.GetPan();
        model.SelectSector (0, 8);

        Assert::AreEqual (50.0,  model.GetZoom(), L"the zoom does not change");
        Assert::AreEqual (pan.x, model.GetPan().x, L"no pan before a flight");
        Assert::AreEqual (pan.y, model.GetPan().y);
        Assert::IsTrue   (model.GetStripStart() > 0.25, L"the strip moved to the sector");
    }



    //  The chord's middle at the center, and the chord three quarters of the
    //  line through the center along it, edge to edge.
    TEST_METHOD (AChordViewCentersTheChordAtThreeQuartersOfTheView)
    {
        InspectorViewModel::View  level    = InspectorViewModel::GetChordView ({ -0.1, 0.5 }, { 0.1, 0.5 });
        InspectorViewModel::View  diagonal = InspectorViewModel::GetChordView ({ 0.0, 0.0 }, { 0.1, 0.1 });
        InspectorViewModel::View  huge     = InspectorViewModel::GetChordView ({ -0.9, 0.0 }, { 0.9, 0.0 });



        Assert::AreEqual ( 7.5,  level.zoom,  1e-9, L"0.2 wide fills 1.5 of the 2 across");
        Assert::AreEqual ( 0.0,  level.pan.x, 1e-9);
        Assert::AreEqual (-3.75, level.pan.y, 1e-9);
        Assert::AreEqual (-0.75, level.pan.x + level.zoom * -0.1, 1e-9, L"the chord's ends at -0.75 and 0.75");

        Assert::AreEqual (15.0,  diagonal.zoom, 1e-9, L"along the diagonal, the line is corner to corner");
        Assert::AreEqual (-0.75, diagonal.pan.x, 1e-9);

        Assert::AreEqual (1.0, huge.zoom, L"a chord too long gets the view at fit");
        Assert::AreEqual (0.0, huge.pan.x);
    }



    //  Between two views the zoom moves evenly in scale about the point both
    //  put in the same place; at one zoom the pan moves evenly.
    TEST_METHOD (TheViewBetweenTwoZoomsAboutTheirSharedPoint)
    {
        InspectorViewModel::View  fit;
        InspectorViewModel::View  to   = { 4.0, { -2.0, 1.0 } };
        InspectorViewModel::View  half = InspectorViewModel::GetBetweenView (fit, to, 0.5);
        InspectorViewModel::View  end  = InspectorViewModel::GetBetweenView (fit, to, 1.0);
        InspectorViewModel::View  pan  = InspectorViewModel::GetBetweenView ({ 2.0, { 0.0, 0.0 } }, { 2.0, { 1.0, -1.0 } }, 0.25);
        double                    dX   = (to.pan.x - fit.pan.x) / (fit.zoom - to.zoom);



        Assert::AreEqual (2.0,  half.zoom, 1e-9, L"halfway in scale from 1 to 4");
        Assert::AreEqual (fit.pan.x + fit.zoom * dX, half.pan.x + half.zoom * dX, 1e-9, L"the shared point does not move");
        Assert::AreEqual (to.pan.x, end.pan.x, 1e-9);
        Assert::AreEqual (to.pan.y, end.pan.y, 1e-9);
        Assert::AreEqual (0.25, pan.pan.x, 1e-9);
        Assert::AreEqual (2.0,  pan.zoom);
    }



    TEST_METHOD (AFlightGoesOutToFitThenInToTheSector)
    {
        DiskAnalysis                analysis;
        InspectorViewModel          model;
        InspectorViewModel::Point   point;
        double                      end   = 0;
        double                      start = 0;
        constexpr double            kLeg  = InspectorViewModel::kFlightLegMs;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.ZoomAbout   (4.0, model.GetDiskPoint (68, 0.0));
        model.SelectSector (0, 8);
        model.StartFlight (1000.0);

        Assert::IsTrue (model.StepFlight (1000.0 + kLeg / 2));
        Assert::IsTrue (model.GetZoom() > 1.0 && model.GetZoom() < 4.0, L"on the way out");

        Assert::IsTrue   (model.StepFlight (1000.0 + kLeg));
        Assert::AreEqual (1.0, model.GetZoom(), 1e-9, L"the whole disk between the legs");

        Assert::IsFalse (model.StepFlight (1000.0 + 2 * kLeg), L"landed");

        start   = model.GetSelectionTurn (end);
        end     = (end < start) ? end + 1.0 : end;
        point.x = (model.GetDiskPoint (0, start).x + model.GetDiskPoint (0, end).x) / 2;
        point.y = (model.GetDiskPoint (0, start).y + model.GetDiskPoint (0, end).y) / 2;

        Assert::IsTrue (model.GetZoom() > 1.0, L"zoomed in on the sector");
        Assert::IsTrue (std::abs (point.x) < 1e-6 && std::abs (point.y) < 1e-6, L"its chord's middle at the center, on the outermost ring");

        model.StartFlight (5000.0);
        model.PanBy ({ 0.1, 0.0 });
        Assert::IsFalse (model.IsFlying(), L"a pan ends a flight");
    }};
