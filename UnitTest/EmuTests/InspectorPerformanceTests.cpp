#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "ComparisonTestImages.h"
#include "FileMapTestImages.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Images = FileMapTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPerformanceTests
//
//  The inspector's timing targets on this machine (SC-003, SC-015, SC-021):
//  analyzing a WOZ with 160 distinct flux quarter tracks, building the file
//  map of a damaged volume, and comparing two 140 KB disks and two such WOZ
//  images. Measured in Release only; each timing is logged.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorPerformanceTests)
{
public:

    static constexpr int  kQuarterTracks = 160;
    static constexpr int  kRuns          = 5;



    static double GetElapsedMs (const LARGE_INTEGER & start)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  end       = {};



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&end);

        return 1000.0 * static_cast<double> (end.QuadPart - start.QuadPart) / static_cast<double> (frequency.QuadPart);
    }



    //  A WOZ with a separate flux record for each of its 160 quarter tracks,
    //  with the sectors of a DOS 3.3 disk.
    static void LoadFluxWoz (DiskImage & outImage, size_t & outSize)
    {
        vector<Byte>               sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>               woz;
        DiskImage                  bits;
        vector<WozSyntheticTrack>  tracks;
        int                        track   = 0;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, bits));

        for (int qt = 0; qt < kQuarterTracks; qt++)
        {
            WozSyntheticTrack  entry;

            track               = std::min (qt / 4, bits.GetTrackCount() - 1);
            entry.isFlux        = true;
            entry.quarterTracks = { qt };
            entry.bitCount      = bits.GetTrackBitCount (track);
            entry.data          = FluxTestImages::BitsToNominalFlux (bits.GetTrackBits (track), entry.bitCount);
            tracks.push_back (std::move (entry));
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));
        AssertSucceeded (WozLoader::Load (woz, outImage));
        outSize = woz.size();
    }



    static void AnalyzeFluxWoz (DiskAnalysis & out)
    {
        DiskImage  image;
        size_t     size  = 0;



        LoadFluxWoz (image, size);
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "flux.woz", size, false), DecodeSettings::MakeStandard(), out);
    }



    //  SC-003: a WOZ with 160 distinct flux quarter tracks analyzes in
    //  under 1 s.
    TEST_METHOD (AnalyzingA160FluxQuarterTrackWozTakesUnderASecond)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-003 is measured in Release only");
#else
        DiskImage                        image;
        DiskAnalysis                     analysis;
        LARGE_INTEGER                    start    = {};
        size_t                           size     = 0;
        double                           ms       = 0;
        std::shared_ptr<const DiskCopy>  copy;



        LoadFluxWoz (image, size);
        copy = DiskCopy::MakeFromImage (image, 1, "flux.woz", size, false);

        QueryPerformanceCounter (&start);
        DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), analysis);
        ms = GetElapsedMs (start);

        Logger::WriteMessage (std::format (L"flux WOZ analysis took {:.1f} ms", ms).c_str());
        Assert::IsTrue (ms < 1000.0);
#endif
    }



    //  SC-015: the map of a damaged volume appears within 100 ms of the
    //  analysis finishing, for each file system. The target applies to the
    //  slowest of a few runs of each.
    TEST_METHOD (MappingADamagedVolumeTakesUnder100Milliseconds)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-015 is measured in Release only");
#else
        struct Volume
        {
            LPCWSTR       label;
            vector<Byte>  image;
            bool          isProDosOrder;
        };

        const Volume  volumes[] =
        {
            { L"DOS 3.3", Images::MakeDos33(),  false },
            { L"ProDOS",  Images::MakeProDos(), true  },
            { L"Pascal",  Images::MakePascal(), true  },
            { L"CP/M",    Images::MakeCpm(),    false },
        };

        LARGE_INTEGER  start   = {};
        double         ms      = 0;
        double         slowest = 0;
        double         worst   = 0;



        for (const Volume & volume : volumes)
        {
            DiskAnalysis  analysis;

            Images::Analyze (volume.image, volume.isProDosOrder, analysis);
            Images::BreakSector (analysis, 17, 3, false);

            slowest = 0;

            for (int run = 0; run < kRuns; run++)
            {
                QueryPerformanceCounter (&start);
                (void) FileMapBuilder::Build (analysis);
                ms      = GetElapsedMs (start);
                slowest = std::max (slowest, ms);
            }

            worst = std::max (worst, slowest);
            Logger::WriteMessage (std::format (L"{} map: slowest {:.2f} ms", volume.label, slowest).c_str());
        }

        Assert::IsTrue (worst < 100.0);
#endif
    }



    //  SC-021: two 140 KB disks compare in under 1 s, and two WOZ images
    //  with 160 distinct flux quarter tracks in under 5 s.
    TEST_METHOD (ComparingDisksMeetsItsTargets)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-021 is measured in Release only");
#else
        DiskAnalysis   dskA;
        DiskAnalysis   dskB;
        DiskAnalysis   fluxA;
        DiskAnalysis   fluxB;
        LARGE_INTEGER  start  = {};
        double         dskMs  = 0;
        double         fluxMs = 0;



        Images::Analyze (Images::MakeDos33(), false, dskA);
        Images::Analyze (Images::MakeDos33(), false, dskB);
        AnalyzeFluxWoz (fluxA);
        AnalyzeFluxWoz (fluxB);

        QueryPerformanceCounter (&start);
        (void) DiskComparer::Compare (dskA, dskB);
        dskMs = GetElapsedMs (start);

        QueryPerformanceCounter (&start);
        (void) DiskComparer::Compare (fluxA, fluxB);
        fluxMs = GetElapsedMs (start);

        Logger::WriteMessage (std::format (L"140 KB comparison took {:.1f} ms; flux WOZ comparison took {:.1f} ms", dskMs, fluxMs).c_str());
        Assert::IsTrue (dskMs < 1000.0);
        Assert::IsTrue (fluxMs < 5000.0);
#endif
    }
};
