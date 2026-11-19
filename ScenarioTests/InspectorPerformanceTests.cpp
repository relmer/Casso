#include "Pch.h"

#include "EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Shell/InspectorStatusPublisher.h"
#include "TestMachine.h"
#include "FileMapTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Images = FileMapTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  StoreDiskSource
//
//  The machine's disk store as the inspector's source, read the way
//  Casso's host reads it.
//
////////////////////////////////////////////////////////////////////////////////

class StoreDiskSource : public IInspectorDiskSource
{
public:
    explicit StoreDiskSource (TestMachine & machine) : m_machine (machine) {}

    const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
    {
        outIdentity.mediaId = static_cast<uint64_t> (drive) + 1;

        return m_machine.GetDiskStore().GetImage (6, drive);
    }

private:
    TestMachine  & m_machine;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPerformanceTests
//
//  The inspector's timing targets measured over a whole machine or a whole
//  volume (SC-005, SC-015): the drive state published each frame against
//  the cost of emulating that frame, and building the file map of each
//  file system's damaged volume. Measured in Release only; each timing is
//  logged. The per-track targets are in the unit tests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorPerformanceTests)
{
public:

    static constexpr int  kRuns = 5;



    static double GetElapsedMs (const LARGE_INTEGER & start)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  end       = {};



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&end);

        return 1000.0 * static_cast<double> (end.QuadPart - start.QuadPart) / static_cast<double> (frequency.QuadPart);
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



    //  SC-005: while the window is open, the emulation thread publishes both
    //  drives' state once a frame. That publish costs under 2% of the time
    //  to emulate a frame at Maximum speed.
    TEST_METHOD (PublishingTheDriveStateCostsUnder2PercentOfAFrame)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-005 is measured in Release only");
#else
        static constexpr uint32_t  kFrameCycles = 17030;
        static constexpr int       kFrames      = 600;

        TestMachine               machine   ("Apple2e");
        StoreDiskSource           source    (machine);
        InspectorStatusPublisher  publisher;
        vector<Byte>              sectors   (NibblizationLayer::kImageByteSize, 0);
        LARGE_INTEGER             start     = {};
        double                    frameMs   = 0;
        double                    publishMs = 0;
        double                    share     = 0;



        machine.PowerCycle();
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "boot.dsk", DiskFormat::Dsk, sectors));
        publisher.SetActive (true);
        machine.RunCycles (100 * kFrameCycles);

        QueryPerformanceCounter (&start);

        for (int frame = 0; frame < kFrames; frame++)
        {
            machine.RunCycles (kFrameCycles);
        }

        frameMs = GetElapsedMs (start) / kFrames;

        QueryPerformanceCounter (&start);

        for (int frame = 0; frame < kFrames; frame++)
        {
            publisher.Publish (machine.GetRefs().diskController, source);
        }

        publishMs = GetElapsedMs (start) / kFrames;
        share     = publishMs / frameMs;

        Logger::WriteMessage (std::format (L"frame {:.1f} us, publish {:.2f} us, {:.2f}% of a frame", frameMs * 1000.0, publishMs * 1000.0, share * 100.0).c_str());
        Assert::IsTrue (share < 0.02);
#endif
    }
};
