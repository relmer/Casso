#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
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


    //  A track of disk bytes with a C0 in it, as a read at a write splice
    //  has, built into a WOZ: no run of zero bits is longer than a
    //  controller reads reliably.
    TEST_METHOD (ASpliceByte_IsWrittenWithNoLongRunOfZeros)
    {
        static constexpr size_t  kRevolution = 6000;
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr size_t  kMaxZeros   = 2;

        std::vector<Byte>                        revolution;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        uint32_t                                 seed    = 12345;
        size_t                                   run     = 0;
        size_t                                   longest = 0;



        for (size_t at = 0; at < kRevolution; at++)
        {
            seed = seed * 1103515245u + 12345u;
            revolution.push_back (s_kDiskBytes[(seed >> 16) % std::size (s_kDiskBytes)]);
        }

        revolution[kRevolution / 2] = 0xC0;

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                read.push_back (revolution[at % kRevolution]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int  slot = image.ResolveQuarterTrack (0);

        Assert::IsTrue (image.GetTrackBitCount (slot) > 0, L"Track 0 has a stream");

        for (size_t at = 0; at < image.GetTrackBitCount (slot); at++)
        {
            run     = image.ReadBit (slot, at) != 0 ? 0 : run + 1;
            longest = std::max (longest, run);
        }

        Assert::IsTrue (longest <= kMaxZeros);
    }


    //  A track whose address fields end in a checksum of $FF, read with
    //  its sync bytes marked as two zero bits follow them: the sync bytes
    //  are written with two zero bits, and the checksum's FF FF with none,
    //  so a read of the field doesn't read an FF twice.
    TEST_METHOD (AnAddressFieldsFFs_AreWrittenWithNoZeroBits)
    {
        static constexpr size_t  kSectors   = 16;
        static constexpr size_t  kSyncs     = 6;
        static constexpr size_t  kDataBytes = 343;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kMarked    = 0x7F;

        std::vector<Byte>                        revolution;
        std::vector<bool>                        sync;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 12345;
        size_t                                   checked = 0;



        auto  add = [&revolution, &sync] (std::initializer_list<Byte> values, bool isSync)
        {
            for (Byte value : values)
            {
                revolution.push_back (value);
                sync.push_back (isSync);
            }
        };

        for (size_t sector = 0; sector < kSectors; sector++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector), 0xFF, 0xFF, 0xDE, 0xAA, 0xEB }, false);

            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0xAD }, false);

            for (size_t each = 0; each < kDataBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add ({ s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)] }, false);
            }

            add ({ 0xDE, 0xAA, 0xEB }, false);
        }

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in = at % revolution.size();

                read.push_back ((delay == 1 && sync[in]) ? kMarked : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        for (size_t each = 1; each + 11 < framed.size(); each++)
        {
            if (framed[each].first == 0xD5 && framed[each + 1].first == 0xAA && framed[each + 2].first == 0x96)
            {
                Assert::AreEqual ((size_t) 2, framed[each - 1].second, L"The sync byte before the field");
                Assert::AreEqual ((size_t) 0, framed[each + 9].second,  L"The checksum's first FF");
                Assert::AreEqual ((size_t) 0, framed[each + 10].second, L"The checksum's second FF");
                checked++;
            }
        }

        Assert::IsTrue (checked > 0, L"The track has address fields");
    }


    //  A track of 16 sectors read three times, with a nibble of sector 5
    //  misread the same way in the first two reads, so most of the passes
    //  have it wrong: the sector is taken from the read that decodes it.
    TEST_METHOD (ASectorMisreadInMostReads_IsTakenFromTheReadThatDecodesIt)
    {
        static constexpr int     kSectors    = 16;
        static constexpr int     kBad        = 5;
        static constexpr size_t  kSyncs      = 8;
        static constexpr size_t  kMisread    = 100;
        static constexpr size_t  kAcrossEnd  = 70;      // past the misread, still in the field
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr Byte    kVolume     = 254;

        std::vector<Byte>                        revolution;
        std::vector<Byte>                        misread;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        std::vector<Byte>                        sector;
        uint32_t                                 seed    = 4321;
        size_t                                   badAt   = 0;
        bool                                     decoded = false;



        auto  oddEven = [&revolution] (Byte value)
        {
            revolution.push_back ((Byte) ((value >> 1) | 0xAA));
            revolution.push_back ((Byte) (value | 0xAA));
        };

        for (int number = 0; number < kSectors; number++)
        {
            std::vector<Byte>  data (NibblizationLayer::kSectorByteSize);
            std::vector<Byte>  nibbles;

            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
            oddEven (kVolume);
            oddEven (0);
            oddEven ((Byte) number);
            oddEven ((Byte) (kVolume ^ number));
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

            for (Byte & value : data)
            {
                seed  = seed * 1103515245u + 12345u;
                value = (Byte) (seed >> 16);
            }

            NibblizationLayer::EncodeDataNibbles (data.data(), nibbles);
            badAt = number == kBad ? revolution.size() + kMisread : badAt;
            revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
        }

        misread        = revolution;
        misread[badAt] = misread[badAt] == 0x96 ? 0x97 : 0x96;

        //  The reads start at the track's first sync byte, and then in the
        //  middle of sector 5's data field, so the field crosses the end of
        //  the revolution the stream has.
        for (size_t first : { (size_t) 0, badAt + kAcrossEnd })
        {
            positions.assign (1, CassoLinkCapture::Position());
            decoded = false;

            for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
            {
                const std::vector<Byte>  & source = read < 2 ? misread : revolution;
                std::vector<Byte>          bytes;

                for (size_t at = 0; at < kReadBytes; at++)
                {
                    bytes.push_back (source[(first + at) % source.size()]);
                }

                positions[0].reads.push_back (bytes);
                positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
            }

            AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
            AssertSucceeded (WozLoader::Load (result.woz, image));

            FrameTrack (image, image.ResolveQuarterTrack (0), framed);

            for (size_t at = 0; !decoded && at + 20 + NibblizationLayer::kDataNibbleCount < framed.size(); at++)
            {
                bool  address = framed[at] == 0xD5 && framed[at + 1] == 0xAA && framed[at + 2] == 0x96;
                int   number  = address ? (((framed[at + 7] << 1) | 1) & framed[at + 8]) : -1;

                if (number == kBad)
                {
                    size_t  data = at + 14 + kSyncs + 3;

                    decoded = NibblizationLayer::DecodeDataNibbles (&framed[data], sector);
                }
            }

            Assert::IsTrue (decoded, std::format (L"Sector 5 decodes, the reads starting at byte {}", first).c_str());
        }
    }

    //  A track's bytes as a controller frames them from its bits.
    static void  FrameTrack (DiskImage & image, int slot, std::vector<Byte> & outBytes)
    {
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;
        Byte    value = 0;



        outBytes.clear();

        for (size_t bit = 0; bit < 2 * count; bit++)
        {
            value = (Byte) ((value << 1) | image.ReadBit (slot, at));
            at    = (at + 1) % count;

            if (value & 0x80)
            {
                outBytes.push_back (value);
                value = 0;
            }
        }
    }

    //  A program just loaded has the arm's place as unknown: a first read of
    //  track 30 takes the arm to track 0 first and reads track 30, not the
    //  track the arm stops at.
    TEST_METHOD (AFirstRead_WithTheArmUnknown_ReadsItsTrack)
    {
        static constexpr Byte  kTrack = 30;

        ProgramRig         rig (LoadFixture());
        std::vector<Byte>  module;
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        CaptureTiming      timing;
        int                track    = -1;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, kTrack * 4, 0, 0, 0, CassoLinkHost::kNoSync, {}, bytes, status, timing));

        for (size_t at = 0; track < 0 && at + 7 <= bytes.size(); at++)
        {
            bool  prolog = (bytes[at] | 0x80) == 0xD5 && (bytes[at + 1] | 0x80) == 0xAA && (bytes[at + 2] | 0x80) == 0x96;

            if (prolog)
            {
                track = (((bytes[at + 5] | 0x80) << 1) | 1) & (bytes[at + 6] | 0x80);
            }
        }

        Assert::AreEqual ((int) kTrack, track);
    }


    TEST_METHOD (WholeTrack_IsReadBitForBit)    {
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
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 0, 0, 0, 100, CassoLinkHost::kNoSync, {}, bytes, status, timing));

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

    //  A read that waits on a marker the track doesn't have ends with the
    //  Status's not-found error, and a read after it, with no marker, works.
    TEST_METHOD (AMarkerThatDoesntPass_FailsTheRead_AndTheNextReadWorks)
    {
        ProgramRig         rig (LoadFixture());
        std::vector<Byte>  module;
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        CaptureTiming      timing;
        const Byte         marker[] = { 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFE };



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 4, 0, 0, 100, 0, marker, bytes, status, timing));
        Assert::AreEqual (CassoLinkHost::kErrorNotFound, status.error, L"The marker didn't pass");

        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 4, 0, 0, 100, CassoLinkHost::kNoSync, {}, bytes, status, timing));
        Assert::AreEqual ((Byte) 0, status.error);
        Assert::IsFalse (bytes.empty());
    }

    //  Where in `track` a window of `captured` is, as a share of a
    //  revolution: the angle of the captured track's bit 0 in the track's
    //  frame. -1 when the window isn't there once.
    static double  FindAngle (const std::vector<uint8_t> & captured, const std::vector<uint8_t> & track)
    {
        static constexpr uint8_t  kProlog[] = { 1,1,0,1,0,1,0,1, 1,0,1,0,1,0,1,0, 1,0,0,1,0,1,1,0 };
        size_t                    n         = track.size();
        size_t                    window    = 0;
        size_t                    found     = SIZE_MAX;
        int                       count     = 0;



        //  The window starts at the first address field, which has its sector's
        //  number, so it is in the track once.
        while (window + kWindowBits < captured.size() && !std::equal (std::begin (kProlog), std::end (kProlog), captured.begin() + (ptrdiff_t) window))
        {
            window++;
        }

        for (size_t start = 0; start < n && window + kWindowBits < captured.size(); start++)
        {
            size_t  at = 0;

            while (at < kWindowBits && track[(start + at) % n] == captured[window + at])
            {
                at++;
            }

            if (at == kWindowBits)
            {
                found = start;
                count++;
            }
        }

        return (count == 1) ? (double) ((found + n - window % n) % n) / (double) n : -1.0;
    }

    //  Six tracks captured with each waiting on a marker on the one before:
    //  every track's bit 0 is at the same angle of the drive's tracks, as an
    //  index-synchronized capture's are, within a few bytes.
    TEST_METHOD (SixTracks_ArePlacedAsTheDrivesTracksAre)
    {
        ProgramRig                 rig (LoadFixture());
        std::vector<Byte>          module;
        CassoLinkCapture::Result   result;
        DiskImage                  woz;
        std::vector<double>        angles;
        std::string                log;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-11T00:00:00Z", {}, result, 5));
        AssertSucceeded (WozLoader::Load (result.woz, woz));

        Assert::IsTrue (result.synchronized, L"Every track waited on its marker");

        for (int track = 0; track <= 5; track++)
        {
            std::vector<uint8_t>  captured;
            int                   slot  = woz.ResolveQuarterTrack (track * 4);
            double                angle = 0.0;

            for (size_t at = 0; at < woz.GetTrackBitCount (slot); at++)
            {
                captured.push_back (woz.ReadBit (slot, at));
            }

            angle = FindAngle (captured, GetTrackBits (rig, track * 4));
            Assert::IsTrue (angle >= 0.0, std::format (L"Track {} is found in the drive's track", track).c_str());
            angles.push_back (angle);
            log  += std::format ("track {}: angle {:.5f}, {:+.5f} from track 0\n", track, angle, angle - angles[0]);
        }

        Logger::WriteMessage (log.c_str());

        for (int track = 1; track <= 5; track++)
        {
            double  drift = std::abs (angles[(size_t) track] - angles[0]);

            Assert::IsTrue (std::min (drift, 1.0 - drift) < kAngleTolerance, std::format (L"Track {}", track).c_str());
        }
    }

    static constexpr size_t  kWindowBits     = 200;
    static constexpr double  kAngleTolerance = 0.005;    // about 30 bytes


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

        //  Casso's drive turns a bit in 4 cycles, so the revolution track 0's
        //  second read timed is its bits at 4 cycles each.
        double  expected = 4.0 * (double) woz.GetTrackBitCount (woz.ResolveQuarterTrack (0));

        Logger::WriteMessage (std::format ("revolution {:.1f} cycles, expected {:.1f}\n", result.revolutionCycles, expected).c_str());
        Assert::IsTrue (std::abs (result.revolutionCycles - expected) <= s_kRevolutionTolerance * expected, L"The drive's revolution is measured");

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

    static constexpr double  s_kRevolutionTolerance = 0.002;

    //  The disk bytes a 16-sector track's data fields use.
    static constexpr Byte  s_kDiskBytes[] =
    {
        0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
        0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
        0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    };
};