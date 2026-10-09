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

        model.ZoomAbout (0.5, anchor);
        Assert::IsTrue (model.IsAtFit());
        Assert::AreEqual (0.0, model.GetPan().x);
        Assert::AreEqual (0.0, model.GetPan().y);
    }



    TEST_METHOD (PanningStopsBeforeTheDiskLeavesTheView)
    {
        InspectorViewModel  model;



        model.PanBy ({ 5.0, 0.0 });
        Assert::AreEqual (0.0, model.GetPan().x, L"no pan at fit");

        model.ZoomAbout (3.0, { 0.0, 0.0 });
        model.PanBy ({ 50.0, -50.0 });

        Assert::AreEqual ( 2.0, model.GetPan().x);
        Assert::AreEqual (-2.0, model.GetPan().y);
    }



    TEST_METHOD (ASelectionOutOfViewPansTheZoomedPlatterAndTheStrip)
    {
        DiskAnalysis                analysis;
        InspectorViewModel          model;
        InspectorViewModel::Point   point;



        AnalyzeDsk (analysis);
        model.SetDisk     (7);
        model.SetAnalysis (&analysis);
        model.ZoomAbout   (50.0, model.GetDiskPoint (0, 0.0));
        model.SelectSector (0, 8);

        point = model.GetDiskPoint (0, TrackAnalyzer::GetAngle (*model.GetTrack(), model.GetTrack()->fields[model.GetSector()->addressField].startCell));

        Assert::AreEqual (50.0, model.GetZoom(), L"the zoom is kept");
        Assert::IsTrue (std::abs (point.x) <= 1.0 && std::abs (point.y) <= 1.0, L"the sector is in view");
        Assert::IsTrue (model.GetStripStart() > 0.25, L"the strip moved to the sector");
    }
};
