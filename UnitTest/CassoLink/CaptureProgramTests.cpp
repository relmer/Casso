#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImage.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureProgramTests
//
//  The capture module on an emulated //c: a track of a DOS 3.3 disk read
//  raw, cut to one revolution, and compared bit for bit with the track the
//  emulated drive reads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CaptureProgramTests)
{
public:

    static std::vector<Byte>  LoadFixture()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));

        return bytes;
    }

    //  The drive's track at a quarter-track position, one bit to a byte.
    static std::vector<uint8_t>  GetTrackBits (ProgramRig & rig, int quarterTrack)
    {
        DiskImage             * image = rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0);
        int                     slot  = image->ResolveQuarterTrack (quarterTrack);
        std::vector<uint8_t>    bits;



        for (size_t at = 0; at < image->GetTrackBitCount (slot); at++)
        {
            bits.push_back (image->ReadBit (slot, at));
        }

        return bits;
    }

    //  Whether `captured` is `track` started at some bit.
    static bool  IsRotationOf (const std::vector<uint8_t> & captured, const std::vector<uint8_t> & track)
    {
        size_t  n = track.size();



        if (captured.size() != n)
        {
            return false;
        }

        for (size_t start = 0; start < n; start++)
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


    //  Each token: bytes as they are, a repeat, and a copy of a revolution
    //  before, with the padding after the last ignored.
    TEST_METHOD (Decode_TokensGiveTheBytes)
    {
        const Byte         encoded[]  = { 3, 0, 0xD5, 0xAA, 0x96, 0x00, 0x81, 0x7F, 0x00, 0x02, 0x00, 0x00 };
        const Byte         expected[] = { 0xD5, 0xAA, 0x96, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x00 };
        std::vector<Byte>  bytes;



        Assert::IsTrue  (CaptureStream::Decode (encoded, std::size (expected), bytes));
        Assert::IsTrue  (bytes == std::vector<Byte> (std::begin (expected), std::end (expected)));
        Assert::IsFalse (CaptureStream::Decode (encoded, std::size (expected) + 3, bytes), L"Too few bytes");

        const Byte         early[] = { 3, 0, 0x00, 0x01 };

        Assert::IsFalse (CaptureStream::Decode (early, 3, bytes), L"A copy from before the first byte");
    }


    TEST_METHOD (WholeTrack_IsReadBitForBit)
    {
        ProgramRig            rig (LoadFixture());
        std::vector<Byte>     module;
        std::vector<Byte>     bytes;
        std::vector<uint8_t>  bits;
        std::vector<uint8_t>  track;
        CassoLinkStatus       status;
        CaptureTiming         timing;
        size_t                length = 0;
        size_t                syncs  = 0;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 0, 0, 0, 100, bytes, status, timing));

        Assert::AreEqual ((Byte) 0, status.error);

        for (Byte b : bytes)
        {
            syncs += CaptureStream::IsSync (b) ? 1 : 0;
        }

        Assert::IsTrue (syncs > 100, L"Sync bytes are marked");
        Assert::IsTrue (CaptureStream::FindRevolution (bytes, length), L"The read repeats");

        CaptureStream::BuildRevolution (bytes, length, bits);
        track = GetTrackBits (rig, 0);

        Logger::WriteMessage (std::format ("revolution {} bytes, {} bits; the track has {} bits; {} syncs\n", length, bits.size(), track.size(), syncs).c_str());

        Assert::IsTrue (IsRotationOf (bits, track), L"One revolution is the drive's track");
    }

    //  Tracks 0 and 1 captured into a WOZ: both are standard, so the
    //  positions between them aren't read; each quarter track reads what the
    //  emulated drive reads there, and the half track has no stream, as a
    //  standard WOZ's doesn't.
    TEST_METHOD (TwoTracks_CaptureToAWozOfTheSameTracks)
    {
        ProgramRig                               rig (LoadFixture());
        std::vector<Byte>                        module;
        CassoLinkCapture::Result                 result;
        DiskImage                                woz;
        std::vector<CassoLinkCapture::Progress>  reports;
        std::vector<Byte>                        file;
        std::vector<CassoLinkCapture::Position>  parsed;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-11T00:00:00Z", [&reports] (const CassoLinkCapture::Progress & where) { reports.push_back (where); }, result, 1));
        AssertSucceeded (WozLoader::Load (result.woz, woz));

        Assert::AreEqual ((size_t) 6, reports.size(),  L"Three reads of each whole track");
        Assert::AreEqual (99, reports.back().percent,  L"The share stops short of 100 until the capture ends");
        Assert::AreEqual (0, result.betweenRead,       L"Nothing between two standard tracks is read");
        Assert::AreEqual (3, result.betweenTotal);
        Assert::IsTrue   (result.sentBytes < result.storedBytes, L"The encoding is shorter");
        Assert::AreEqual ((size_t) 2, result.streams.size(), L"The two whole tracks");
        Assert::IsTrue   (result.unstable.empty());
        Assert::IsTrue   (result.weak.empty());
        Assert::AreEqual (-1, woz.ResolveQuarterTrack (2), L"The half track");

        CassoLinkCapture::SerializeReads (result.positions, file);
        AssertSucceeded (CassoLinkCapture::ParseReads (file, parsed));
        Assert::AreEqual (result.positions.size(), parsed.size(), L"The reads round-trip");
        Assert::IsTrue   (parsed[1].reads == result.positions[1].reads);

        for (int quarter : { 0, 1, 3, 4 })
        {
            std::vector<uint8_t>  captured;
            int                   slot = woz.ResolveQuarterTrack (quarter);

            Assert::IsTrue (slot >= 0);

            for (size_t at = 0; at < woz.GetTrackBitCount (slot); at++)
            {
                captured.push_back (woz.ReadBit (slot, at));
            }

            //  A WOZ has a quarter track as its nearer whole track; Casso's image
            //  has quarter tracks 0 to 3 as track 0.
            Assert::IsTrue (IsRotationOf (captured, GetTrackBits (rig, (quarter + 1) / 4 * 4)), std::format (L"Quarter track {}", quarter).c_str());
        }
    }
};