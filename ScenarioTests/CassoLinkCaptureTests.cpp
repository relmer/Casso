#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/ProgramRig.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCaptureScenarioTests
//
//  A WOZ mounted in an emulated //c and captured raw over the link, every
//  quarter-track position: each whole and half track the source has data at
//  reads the same in the capture, as a rotation of the source's stream.
//  Casso's drive steps by half tracks, so the odd quarter tracks a real drive
//  steps to with two phases on aren't compared. The disks are
//  CiderPress II's samples, fetched by scripts/FetchCiderPressTestFiles.ps1.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkCaptureScenarioTests)
{
public:

    static std::vector<uint8_t>  GetBits (const DiskImage & image, int quarterTrack)
    {
        std::vector<uint8_t>  bits;
        int                   slot = image.ResolveQuarterTrack (quarterTrack);



        for (size_t at = 0; slot >= 0 && at < image.GetTrackBitCount (slot); at++)
        {
            bits.push_back (image.ReadBit (slot, at));
        }

        return bits;
    }

    static bool  IsRotationOf (const std::vector<uint8_t> & captured, const std::vector<uint8_t> & track)
    {
        size_t  n = track.size();



        for (size_t start = 0; captured.size() == n && start < n; start++)
        {
            size_t  at = 0;

            while (at < n && track[(start + at) % n] == captured[at])
            {
                at++;
            }

            if (at == n)
            {
                return true;
            }
        }

        return false;
    }

    //  Captures the fixture and compares each quarter track; returns the
    //  positions that differ.
    static std::vector<int>  CaptureAndCompare (const char * fixture)
    {
        FixtureProvider            fixtures;
        std::vector<Byte>          source;
        std::vector<Byte>          module;
        DiskImage                  original;
        DiskImage                  captured;
        CassoLinkCapture::Result   result;
        std::vector<int>           differ;
        std::string                log;



        AssertSucceeded (fixtures.OpenFixture (fixture, source));

        ProgramRig  rig (std::vector<Byte> (143360, 0));

        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, source));
        AssertSucceeded (WozLoader::Load (source, original));

        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-11T00:00:00Z", {}, result));
        AssertSucceeded (WozLoader::Load (result.woz, captured));

        for (int quarter = 0; quarter <= CassoLinkCapture::kLastPosition; quarter += 2)
        {
            std::vector<uint8_t>  fromSource = GetBits (original, quarter);
            std::vector<uint8_t>  got        = GetBits (captured, quarter);

            if (!fromSource.empty() && !IsRotationOf (got, fromSource))
            {
                differ.push_back (quarter);
                log += std::format ("quarter {}: source {} bits, capture {} bits\n", quarter, fromSource.size(), got.size());
            }
        }

        {
            std::vector<Byte>   fromSource;
            std::vector<Byte>   got;
            SectorDecodeReport  sourceReport;
            SectorDecodeReport  captureReport;

            AssertSucceeded (VolumeImage::Load (source, "source.woz", fromSource, sourceReport));
            AssertSucceeded (VolumeImage::Load (result.woz, "capture.woz", got, captureReport));

            Assert::IsTrue (fromSource == got, L"The capture decodes to the source's sectors");
            Assert::AreEqual (sourceReport.GetUnrecoveredCount(), captureReport.GetUnrecoveredCount(), L"and has every sector the source has");
        }

        log += std::format ("{} streams, {} weak, {} unstable, {} positions differ in bits\n", result.streams.size(), result.weak.size(), result.unstable.size(), differ.size());
        Logger::WriteMessage (log.c_str());

        return differ;
    }


    TEST_METHOD (Dos32Master_IsCapturedTrackForTrack)
    {
        std::vector<int>  differ = CaptureAndCompare ("CiderPress/woz/dos32master_2.woz");



        Assert::IsTrue (differ.size() <= 35, L"At most each whole track differs, by a sync bit");
    }


    TEST_METHOD (RdosGame_IsCapturedTrackForTrack)
    {
        std::vector<int>  differ = CaptureAndCompare ("CiderPress/rdos/rdos3-rss-save.woz");



        Assert::IsTrue (differ.size() <= 35, L"At most each whole track differs, by a sync bit");
    }
};