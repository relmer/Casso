#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FileMapTestImages.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Images = FileMapTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPerformanceTests
//
//  The inspector's timing targets (SC-003, SC-004, SC-021), as budgets for
//  one track. Each quarter track is analyzed and compared separately, so a
//  whole disk costs its track count times one track's cost. If one track is
//  within its share of the disk's target, the whole disk is within the
//  target.
//  Measured in Release only; each timing is logged. The drive state publish
//  (SC-005) and the file map (SC-015) are measured in ScenarioTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorPerformanceTests)
{
public:

    static constexpr int     kDiskQuarterTracks  = 160;
    static constexpr int     kDiskTracks         = 35;
    static constexpr int     kTrackQuarterTracks = 4;
    static constexpr double  kFluxAnalyzeMs      = 1000.0;
    static constexpr double  kDskCompareMs       = 1000.0;
    static constexpr double  kFluxCompareMs      = 5000.0;



    static double GetElapsedMs (const LARGE_INTEGER & start)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  end       = {};



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&end);

        return 1000.0 * static_cast<double> (end.QuadPart - start.QuadPart) / static_cast<double> (frequency.QuadPart);
    }



    //  A WOZ with a separate flux record for each of track 0's quarter
    //  tracks, with the sectors of a DOS 3.3 disk.
    static void LoadFluxTrack (DiskImage & outImage, size_t & outSize)
    {
        vector<Byte>               sectors (NibbleFormat::kImageByteSize, 0);
        vector<Byte>               woz;
        DiskImage                  bits;
        vector<WozSyntheticTrack>  tracks;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, bits));

        for (int qt = 0; qt < kTrackQuarterTracks; qt++)
        {
            WozSyntheticTrack  entry;

            entry.isFlux        = true;
            entry.quarterTracks = { qt };
            entry.bitCount      = bits.GetTrackBitCount (0);
            entry.data          = FluxTestImages::BitsToNominalFlux (bits.GetTrackBits (0), entry.bitCount);
            tracks.push_back (std::move (entry));
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));
        AssertSucceeded (WozLoader::Load (woz, outImage));
        outSize = woz.size();
    }



    //  The DOS 3.3 volume the views paint, analyzed once for the class.
    static const DiskAnalysis & GetDos33()
    {
        static DiskAnalysis  s_analysis;
        static bool          s_isBuilt  = false;



        if (!s_isBuilt)
        {
            Images::Analyze (Images::MakeDos33(), false, s_analysis);
            s_isBuilt = true;
        }

        return s_analysis;
    }



    TEST_CLASS_INITIALIZE (BuildSharedInput)
    {
#ifndef _DEBUG
        (void) GetDos33();
#endif
    }



    static std::shared_ptr<const DiskCopy> MakeFluxTrackCopy()
    {
        DiskImage  image;
        size_t     size  = 0;



        LoadFluxTrack (image, size);

        return DiskCopy::MakeFromImage (image, 1, "flux.woz", size, false);
    }



    //  SC-003: a WOZ with 160 distinct flux quarter tracks analyzes in
    //  under 1 s, so a track of four analyzes in its share of that.
    TEST_METHOD (AnalyzingAFluxTrackFitsItsShareOfASecond)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-003 is measured in Release only");
#else
        DiskAnalysis                     analysis;
        LARGE_INTEGER                    start    = {};
        double                           ms       = 0;
        double                           budget   = kFluxAnalyzeMs * kTrackQuarterTracks / kDiskQuarterTracks;
        std::shared_ptr<const DiskCopy>  copy     = MakeFluxTrackCopy();



        QueryPerformanceCounter (&start);
        DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), analysis);
        ms = GetElapsedMs (start);

        Logger::WriteMessage (std::format (L"flux track analysis took {:.2f} ms of {:.2f} ms", ms, budget).c_str());
        Assert::AreEqual (kTrackQuarterTracks, static_cast<int> (std::count_if (analysis.entries.begin(), analysis.entries.end(), [] (const auto & entry) { return entry.slot >= 0; })), L"each quarter track was analyzed");
        Assert::IsTrue (ms < budget);
#endif
    }



    //  SC-021: two 140 KB disks compare in under 1 s, and two WOZ images
    //  with 160 distinct flux quarter tracks in under 5 s. One flux track
    //  compares within the smaller of its two shares.
    TEST_METHOD (ComparingAFluxTrackFitsItsShareOfTheTargets)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-021 is measured in Release only");
#else
        DiskAnalysis   a;
        DiskAnalysis   b;
        LARGE_INTEGER  start   = {};
        double         ms      = 0;
        double         budget  = std::min (kDskCompareMs / kDiskTracks, kFluxCompareMs * kTrackQuarterTracks / kDiskQuarterTracks);



        DiskAnalyzer::Analyze (MakeFluxTrackCopy(), DecodeSettings::MakeStandard(), a);
        DiskAnalyzer::Analyze (MakeFluxTrackCopy(), DecodeSettings::MakeStandard(), b);

        QueryPerformanceCounter (&start);
        (void) DiskComparer::Compare (a, b);
        ms = GetElapsedMs (start);

        Logger::WriteMessage (std::format (L"flux track comparison took {:.2f} ms of {:.2f} ms", ms, budget).c_str());
        Assert::IsTrue (ms < budget);
#endif
    }



    //  SC-004: at fit, at the maximum zoom and panned across the zoomed disk
    //  and the track, painting the platter's overlays and the strip takes
    //  well under a frame. Each call is stored in the mock painter's list, so
    //  this overstates the cost.
    TEST_METHOD (PaintingThePlatterAndStripFitsAFrameAtEveryZoom)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-004 is measured in Release only");
#else
        static constexpr double  kFrameMs = 1000.0 / 60.0;

        DiskAnalysis          analysis  = GetDos33();
        InspectorViewModel    model;
        InspectorViewContext  context;
        DxuiTheme             theme     = DxuiTheme::Dark();
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        LARGE_INTEGER         start     = {};
        vector<double>        frames;
        double                median    = 0;
        double                worst     = 0;
        size_t                calls     = 0;



        model.SetDisk (1);
        model.SetAnalysis (&analysis);
        model.SelectSector (17 * 4, 3);

        context.analysis = &analysis;
        context.model    = &model;
        context.hasDisk  = true;

        PlatterView     platter (context);
        TrackStripView  strip   (context);

        platter.Layout ({ 0, 0, 1000, 1000 }, scaler);
        strip.Layout   ({ 0, 1000, 1900, 1100 }, scaler);
        platter.SetAlignmentShown (true);

        auto  paintFrame = [&] ()
        {
            MockDxuiPainter  painter;

            QueryPerformanceCounter (&start);
            platter.Paint (painter, text, theme);
            strip.Paint   (painter, text, theme);
            frames.push_back (GetElapsedMs (start));
            calls = std::max (calls, painter.Calls().size());
        };

        paintFrame();

        platter.ZoomAboutCenter (InspectorViewModel::kMaxZoom);
        strip.ZoomAboutCenter   (InspectorViewModel::kMaxZoom);
        paintFrame();

        model.PanBy ({ 0.5, 0.2 });
        strip.PanBy (2.5);
        paintFrame();

        std::sort (frames.begin(), frames.end());
        median = frames[frames.size() / 2];
        worst  = frames.back();

        Logger::WriteMessage (std::format (L"paint over {} frames: median {:.2f} ms, slowest {:.2f} ms, at most {} calls a frame", frames.size(), median, worst, calls).c_str());
        Assert::IsTrue (calls > 1000, L"the views drew their overlays");
        Assert::IsTrue (median < kFrameMs);
        Assert::IsTrue (worst < 33.0);
#endif
    }
};
