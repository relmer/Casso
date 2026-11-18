#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/ProgramRig.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootCheck.h"
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
//  CiderPress II's samples, fetched by scripts/FetchCiderPressTestFiles.ps1,
//  and a copy-protected game from the game images, whose capture is booted.
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


    static HRESULT  BuildFromFixtures (const std::string & machineId, BootCheck::Built & outBuilt, std::string & outError)
    {
        std::shared_ptr<TestMachine>  machine = std::make_shared<TestMachine> (machineId);



        outError.clear();

        outBuilt.builder = &machine->GetBuilder();
        outBuilt.host    = machine;

        return S_OK;
    }

    //  A copy-protected disk captured raw on an emulated //c: returns how
    //  many of its whole tracks with data are in the capture and how many
    //  it has, and the capture's boot verdict. Its sectors aren't compared,
    //  as its tracks aren't standard.
    static void  CaptureGame (const wchar_t * name, int & outFound, int & outTracks, BootVerdict & outVerdict)
    {
        std::vector<Byte>          source  = GuestSession::FindGameImage (name);
        std::vector<Byte>          module;
        DiskImage                  original;
        DiskImage                  captured;
        CassoLinkCapture::Result   result;
        BootReport                 report;
        std::string                machine;
        std::vector<std::string>   missing;



        outFound  = 0;
        outTracks = 0;

        Assert::IsFalse (source.empty(), (std::wstring (name) + L" is in the game images").c_str());

        ProgramRig  rig (std::vector<Byte> (143360, 0));

        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, source));
        AssertSucceeded (WozLoader::Load (source, original));
        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-17T00:00:00Z", {}, result));
        AssertSucceeded (WozLoader::Load (result.woz, captured));

        for (int quarter = 0; quarter <= CassoLinkCapture::kLastPosition; quarter += 4)
        {
            bool  hasData = !GetBits (original, quarter).empty();

            outTracks += hasData ? 1 : 0;
            outFound  += (hasData && !GetBits (captured, quarter).empty()) ? 1 : 0;
        }

        AssertSucceeded (BootCheck::Run (result.woz, DiskFormat::Woz, "Apple //e enhanced", {}, BuildFromFixtures, report, machine, missing));

        outVerdict = report.verdict;

        Logger::WriteMessage (std::format ("{}: {} of {} whole tracks captured, {} weak, {} unstable, {} reads; boot {}\n", TextEncoding::WideToUtf8 (name), outFound, outTracks,
                                           result.weak.size(), result.unstable.size(), result.reads, BootProbe::GetVerdictName (outVerdict)).c_str());
    }


    //  A copy-protected disk captured, every whole track with data in the
    //  capture, and the capture booted, as the disk is (SC-055).
    static void  AssertCapturedAndBoots (const wchar_t * name)
    {
        int          found   = 0;
        int          tracks  = 0;
        BootVerdict  verdict = BootVerdict::Unclear;



        CaptureGame (name, found, tracks, verdict);

        Assert::AreEqual (tracks, found, (std::wstring (name) + L": every whole track with data is in the capture").c_str());
        Assert::IsTrue   (verdict == BootVerdict::Booted, (std::wstring (name) + L": the capture boots").c_str());
    }


    TEST_METHOD (CopyProtectedDisks_AreCapturedTrackForTrack_AndBoot)
    {
        AssertCapturedAndBoots (L"HardHatMack.woz");
        AssertCapturedAndBoots (L"AppleStellarInvaders.woz");
    }


    //  Choplifter's tracks are 4-and-4 bytes with no sync bytes and long
    //  stretches with no flux, which the drive reads as random bits, so each
    //  revolution is a different count of bytes.
    TEST_METHOD (Choplifter_TracksWithNoFlux_AreCaptured_AndBoot)
    {
        AssertCapturedAndBoots (L"Choplifter.woz");
    }


    //  Rocky's Boots' track 34 has a sector whose data field has E7 bytes
    //  followed by one or two zero bits, which the protection times.
    TEST_METHOD (RockysBoots_TimingBits_AreCaptured_AndBoot)
    {
        AssertCapturedAndBoots (L"RockysBoots.woz");
    }
};
