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
//  DelayedByteStream
//
//  The PC's end of the link, which runs the machine a pseudorandom count of
//  cycles, up to a revolution, before each request is written, so each read
//  starts at another angle. The counts depend only on the seed.
//
////////////////////////////////////////////////////////////////////////////////

class DelayedByteStream : public IByteStream
{
public:
    DelayedByteStream (TestMachine & machine, std::shared_ptr<IByteStream> inner, uint32_t seed) : m_machine (machine), m_inner (std::move (inner)), m_state (seed * 2654435761u + 12345u) {}

    HRESULT  Write (const Byte * data, size_t count) override
    {
        m_state = m_state * 1664525u + 1013904223u;
        m_machine.RunCycles ((m_state >> 8) % s_kMostCycles);

        return m_inner->Write (data, count);
    }

    HRESULT  Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return m_inner->Read (buffer, capacity, timeoutMs, outRead); }
    size_t   GetReadableCount() override                                                    { return m_inner->GetReadableCount(); }
    bool     IsClosed() const override                                                      { return m_inner->IsClosed(); }
    void     Close() override                                                               { m_inner->Close(); }

private:
    static constexpr uint32_t  s_kMostCycles = 204'000;   // a revolution

    TestMachine                   & m_machine;
    std::shared_ptr<IByteStream>    m_inner;
    uint32_t                        m_state;
};





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
    //  With a seed, each request is delayed (DelayedByteStream), so the reads
    //  start at other angles.
    static void  CaptureGame (const wchar_t * name, uint32_t seed, int & outFound, int & outTracks, BootVerdict & outVerdict)
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

        if (seed != 0)
        {
            rig.host = std::make_unique<CassoLinkHost> (std::make_shared<DelayedByteStream> (rig.machine, rig.stream, seed), rig.folder, [] () { return std::array<Byte, 4> {}; });
        }

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

        Logger::WriteMessage (std::format ("{} (seed {}): {} of {} whole tracks captured, {} weak, {} unstable, {} reads; boot {}\n", TextEncoding::WideToUtf8 (name), seed, outFound, outTracks,
                                           result.weak.size(), result.unstable.size(), result.reads, BootProbe::GetVerdictName (outVerdict)).c_str());
    }


    //  A copy-protected disk captured, every whole track with data in the
    //  capture, and the capture booted, as the disk is (SC-055).
    static void  AssertCapturedAndBoots (const wchar_t * name, uint32_t seed = 0)
    {
        int           found   = 0;
        int           tracks  = 0;
        BootVerdict   verdict = BootVerdict::Unclear;
        std::wstring  label   = std::format (L"{} (seed {})", name, seed);



        CaptureGame (name, seed, found, tracks, verdict);

        Assert::AreEqual (tracks, found, (label + L": every whole track with data is in the capture").c_str());
        Assert::IsTrue   (verdict == BootVerdict::Booted, (label + L": the capture boots").c_str());
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


    //  A read starts wherever the disk is when its request arrives. These
    //  seeds start the reads at angles where Rocky's Boots' timing bits came
    //  out a bit off, and where Apple Stellar Invaders' tracks, whose every
    //  address field is followed by a stretch with no flux, lost a sector or
    //  the whole track.
    TEST_METHOD (ProtectedDisks_ReadFromOtherAngles_AreCaptured_AndBoot)
    {
        static constexpr uint32_t  kRockysBootsSeeds[] = { 12, 31, 37 };
        static constexpr uint32_t  kInvadersSeeds[]    = { 14, 34, 41 };



        for (uint32_t seed : kRockysBootsSeeds)
        {
            AssertCapturedAndBoots (L"RockysBoots.woz", seed);
        }

        for (uint32_t seed : kInvadersSeeds)
        {
            AssertCapturedAndBoots (L"AppleStellarInvaders.woz", seed);
        }
    }


    //  Karateka's track 0 is 4-and-4 data whose timing bytes are followed by
    //  zero bits; with 12 reads, these seeds timed a few of them wrong. Lode
    //  Runner's track 27 has stretches with no flux, and from these angles
    //  its first reads didn't align.
    TEST_METHOD (KaratekaAndLodeRunner_ReadFromOtherAngles_AreCaptured_AndBoot)
    {
        static constexpr uint32_t  kKaratekaSeeds[]   = { 108, 116 };
        static constexpr uint32_t  kLodeRunnerSeeds[] = { 101, 124 };



        for (uint32_t seed : kKaratekaSeeds)
        {
            AssertCapturedAndBoots (L"Karateka.woz", seed);
        }

        for (uint32_t seed : kLodeRunnerSeeds)
        {
            AssertCapturedAndBoots (L"LodeRunner.woz", seed);
        }
    }
};
