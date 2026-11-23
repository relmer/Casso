#include "Pch.h"

#include "EhmTestHelper.h"

#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImage.h"
#include "CassoLink/ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureProgramScenarioTests
//
//  The capture module on an emulated //c: a track of a 16-sector disk read
//  raw, cut to one revolution, and compared bit for bit with the track the
//  emulated drive reads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CaptureProgramScenarioTests)
{
public:

    //  An empty DOS 3.3 disk's sectors, built in memory.
    static std::vector<Byte>  MakeDisk()
    {
        std::vector<Byte>  bytes (NibbleFormat::kImageByteSize, 0);



        AssertSucceeded (Dos33Skeleton::Write (bytes, 254));

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


    //  A disk byte as a drive's latch takes it, and the zero bits after it.
    struct TrackByte
    {
        Byte  value = 0;
        int   zeros = 0;
    };


    //  A track's bytes in one revolution. The bits are taken twice round, so
    //  the second revolution's bytes are framed in step.
    static std::vector<TrackByte>  GetTrackBytes (const std::vector<uint8_t> & bits)
    {
        std::vector<TrackByte>  bytes;
        Byte                    shift = 0;
        size_t                  n     = bits.size();



        for (size_t at = 0; at < 2 * n; at++)
        {
            uint8_t  bit = bits[at % n];

            if (shift == 0 && bit == 0)
            {
                if (!bytes.empty() && at >= n)
                {
                    bytes.back().zeros++;
                }

                continue;
            }

            shift = (Byte) ((shift << 1) | bit);

            if ((shift & 0x80) != 0)
            {
                if (at >= n)
                {
                    bytes.push_back ({ shift, 0 });
                }

                shift = 0;
            }
        }

        return bytes;
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


    //  A 13-sector disk written over the link by Write nibble track, track by
    //  track: the emulated drive's disk decodes to its sectors, and read
    //  back over the link, it has them too.
    TEST_METHOD (AThirteenSectorDisk_IsWrittenAndReadBack_OverTheLink)
    {
        ProgramRig          rig (MakeDisk());
        std::vector<Byte>   module;
        std::vector<Byte>   sectors (ThirteenSectorCodec::kImageByteSize);
        std::vector<Byte>   onDisk;
        std::vector<Byte>   read;
        SectorDecodeReport  diskReport;
        SectorDecodeReport  readReport;
        CassoLinkStatus     status;



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 7 + at / 256);
        }

        AssertSucceeded (LinkProgram::BuildThirteenSectorModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (CassoLinkThirteenSector::WriteDisk (*rig.host, 1, sectors, nullptr, nullptr, status));
        Assert::AreEqual ((Byte) 0, status.error);

        ThirteenSectorCodec::Denibblize (*rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0), onDisk, diskReport);
        Assert::IsTrue (onDisk == sectors, L"The drive's disk has the sectors written");

        AssertSucceeded (CassoLinkThirteenSector::ReadDisk (*rig.host, 1, nullptr, nullptr, read, readReport, status));
        Assert::AreEqual ((Byte) 0, status.error);
        Assert::IsTrue (read == sectors, L"The sectors read back over the link");
    }


    //  A track with two sectors that have no data field, as on a track DOS
    //  3.2's INIT formatted: the two are zeros in the read, and the track is
    //  whole. The first two whole data fields in the stream are each self-sync
    //  bytes of the same bits, as a longer track would overwrite its start.
    TEST_METHOD (SectorsWithNoDataField_ReadAsZeros_OverTheLink)
    {
        static constexpr int     kTrack        = 5;
        static constexpr size_t  kFieldNibbles = 3 + CassoLinkCodec::kThirteenField + 3;

        ProgramRig          rig (MakeDisk());
        std::vector<Byte>   module;
        std::vector<Byte>   sectors (ThirteenSectorCodec::kImageByteSize);
        std::vector<Byte>   expected;
        std::vector<Byte>   stream;
        std::vector<Byte>   read;
        DiskImage           image;
        SectorDecodeReport  report;
        CassoLinkStatus     status;
        int                 blanked = 0;



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 5 + at / 256 + 1);
        }

        AssertSucceeded (LinkProgram::BuildThirteenSectorModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (CassoLinkThirteenSector::WriteDisk (*rig.host, 1, sectors, nullptr, nullptr, status));

        AssertSucceeded (ThirteenSectorCodec::Nibblize (sectors, CassoLinkThirteenSector::kVolume, image));
        CassoLinkThirteenSector::MakeStream (image, image.ResolveWholeTrack (kTrack), stream);

        expected = sectors;

        for (size_t at = 0; at + 9 < stream.size() && blanked < 2; at++)
        {
            Byte  sector = 0;
            bool  erased = false;

            if (stream[at] != 0xD5 || stream[at + 1] != 0xAA || stream[at + 2] != 0xB5)
            {
                continue;
            }

            sector = DiskFieldFormat::Decode44 (stream[at + 7], stream[at + 8]);

            for (size_t data = at + 9; !erased && data + kFieldNibbles <= stream.size(); data++)
            {
                if (stream[data] == 0xD5 && stream[data + 1] == 0xAA && stream[data + 2] == 0xAD)
                {
                    stream.erase  (stream.begin() + (ptrdiff_t) data, stream.begin() + (ptrdiff_t) (data + kFieldNibbles));
                    stream.insert (stream.begin() + (ptrdiff_t) data, kFieldNibbles * 8 / 10, (Byte) 0);
                    erased = true;
                }
            }

            if (!erased)
            {
                continue;
            }

            std::fill_n (expected.begin() + (ptrdiff_t) ((kTrack * ThirteenSectorCodec::kSectorsPerTrack + sector) * ThirteenSectorCodec::kSectorByteSize),
                         ThirteenSectorCodec::kSectorByteSize, (Byte) 0);
            blanked++;
        }

        Assert::AreEqual (2, blanked);

        AssertSucceeded (rig.host->WriteNibbleTrack (1, (Byte) kTrack, stream, status));
        Assert::AreEqual ((Byte) 0, status.error);

        AssertSucceeded (CassoLinkThirteenSector::ReadDisk (*rig.host, 1, nullptr, nullptr, read, report, status));
        Assert::AreEqual ((Byte) 0, status.error);
        Assert::IsTrue (read == expected, L"The sectors with no data field are zeros");
        Assert::AreEqual ((int) TrackDecodeOutcome::Complete, (int) report.GetOutcome (kTrack));
    }


    //  A whole track read at 8 cycles, whose revolutions mark different
    //  bytes, as a read that finds each byte at another moment in each
    //  revolution does: after the first revolution, the values are copies, so
    //  the encoding is the first revolution, the marks and a few tokens.
    TEST_METHOD (ARead_WhoseMarksDifferFromTheRevolutionBefore_IsSentAsCopies)
    {
        static constexpr size_t  kTokenBytes = 512;

        ProgramRig         rig (MakeDisk());
        std::vector<Byte>  module;
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        CaptureTiming      timing;
        size_t             length   = 0;
        size_t             differ   = 0;
        size_t             limit    = 0;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, 0, 0, 0, 100, CassoLinkHost::kNoSync, {}, bytes, status, timing));
        Assert::IsTrue  (CaptureStream::FindRevolution (bytes, length), L"The read repeats");

        for (size_t at = 0; at + length < bytes.size(); at++)
        {
            differ += (CaptureStream::IsSync (bytes[at]) != CaptureStream::IsSync (bytes[at + length])) ? 1 : 0;
        }

        limit = length + bytes.size() / 8 + kTokenBytes;

        Logger::WriteMessage (std::format ("{} bytes stored, {} sent, revolution {}, {} marks differ from the revolution before\n", bytes.size(), timing.sentBytes, length, differ).c_str());

        Assert::IsTrue (differ > 0, L"The revolutions mark different bytes");
        Assert::IsTrue (timing.sentBytes <= limit, std::format (L"{} sent, at most {}", timing.sentBytes, limit).c_str());
    }


    //  A write-protected disk's capture is a write-protected WOZ, as a
    //  program that writes to the disk it runs from takes another path when the write
    //  fails; an unprotected disk's isn't; and the reads file has it.
    TEST_METHOD (AWriteProtectedDisk_IsAWriteProtectedWoz)
    {
        static constexpr size_t  kInfoProtected = 22;

        for (bool protect : { true, false })
        {
            ProgramRig                               rig (MakeDisk());
            std::vector<Byte>                        module;
            std::vector<Byte>                        file;
            std::vector<CassoLinkCapture::Position>  parsed;
            CassoLinkCapture::Result                 result;

            rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0)->SetWriteProtected (protect);

            AssertSucceeded (LinkProgram::BuildCaptureModule (module));
            AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-12T00:00:00Z", {}, result, 0));

            Assert::AreEqual (protect, result.writeProtected, L"The capture found the disk's protection");
            Assert::AreEqual ((Byte) (protect ? 1 : 0), result.woz[kInfoProtected], L"The WOZ's INFO has it");

            CassoLinkCapture::SerializeReads (result.positions, file);
            AssertSucceeded (CassoLinkCapture::ParseReads (file, parsed));
            Assert::AreEqual (protect, parsed[0].writeProtected, L"The reads file has it");
        }
    }


    //  A program just loaded has the arm's place as unknown: a first read of
    //  track 30 takes the arm to track 0 first and reads track 30, not the
    //  track the arm stops at.
    TEST_METHOD (AFirstRead_WithTheArmUnknown_ReadsItsTrack)
    {
        static constexpr Byte  kTrack = 30;

        ProgramRig         rig (MakeDisk());
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


    //  A whole track read once: every byte of it, in order, and every sync
    //  byte marked. A read marks a byte when the latch has it again 8 cycles
    //  after the loop finds it, so a byte found the cycle it finishes can be
    //  marked too; the PC sorts those out across reads.
    TEST_METHOD (WholeTrack_IsReadByteForByte_WithEverySyncMarked)
    {
        ProgramRig              rig (MakeDisk());
        std::vector<Byte>       module;
        std::vector<Byte>       bytes;
        std::vector<uint8_t>    bits;
        std::vector<uint8_t>    track;
        std::vector<TrackByte>  trackBytes;
        CassoLinkStatus         status;
        CaptureTiming           timing;
        size_t                  length  = 0;
        size_t                  syncs   = 0;
        size_t                  start   = 0;
        size_t                  missed  = 0;
        bool                    aligned = false;



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
        track      = GetTrackBits (rig, 0);
        trackBytes = GetTrackBytes (track);

        Logger::WriteMessage (std::format ("revolution {} bytes, {} bits; the track has {} bytes, {} bits; {} syncs\n", length, bits.size(), trackBytes.size(), track.size(), syncs).c_str());

        Assert::AreEqual (trackBytes.size(), length, L"A revolution has the track's bytes");

        for (start = 0; start < length && !aligned; start++)
        {
            size_t  at = 0;

            while (at < length && (Byte) (bytes[at] | 0x80) == trackBytes[(start + at) % length].value)
            {
                at++;
            }

            aligned = at == length;
        }

        Assert::IsTrue (aligned, L"The read has every byte of the track, in order");

        start--;

        for (size_t at = 0; at < length; at++)
        {
            if (trackBytes[(start + at) % length].zeros >= 2)
            {
                missed += CaptureStream::IsSync (bytes[at]) ? 0 : 1;
            }
        }

        Assert::AreEqual ((size_t) 0, missed, L"Every byte followed by two zero bits is marked");
    }


    //  A read of track 1 that waits on a marker passing on track 0 first. The
    //  disk is volume 254, so the address field of track 0, sector 0 with
    //  volume 254 passes and the read works. The same field with volume 17
    //  isn't on the disk, so that read ends with the Status's not-found
    //  error, and a read after it, with no marker, works.
    TEST_METHOD (AMarkerThatDoesntPass_FailsTheRead_AndTheNextReadWorks)
    {
        static constexpr Byte  kRead    = 4;    // track 1
        static constexpr Byte  kWatched = 0;    // track 0

        ProgramRig         rig (MakeDisk());
        std::vector<Byte>  module;
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        CaptureTiming      timing;
        const Byte         onDisk[] = { 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, 0xAA, 0xAA, 0xFF, 0xFE };   // volume 254
        const Byte         absent[] = { 0xD5, 0xAA, 0x96, 0xAA, 0xBB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xBB };   // volume 17



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, kRead, 0, 0, 100, kWatched, onDisk, bytes, status, timing));
        Assert::AreEqual ((Byte) 0, status.error, L"The marker on the disk passed");

        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, kRead, 0, 0, 100, kWatched, absent, bytes, status, timing));
        Assert::AreEqual (CassoLinkHost::kErrorNotFound, status.error, L"The marker not on the disk didn't pass");

        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (rig.host->CaptureQuarterTrack (1, kRead, 0, 0, 100, CassoLinkHost::kNoSync, {}, bytes, status, timing));
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
        ProgramRig                 rig (MakeDisk());
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


    //  A capture stopped after its first read ends with ERROR_CANCELLED and
    //  no image, and the program takes the next request.
    TEST_METHOD (ACaptureStopped_EndsCanceled_AndTheLinkGoesOn)
    {
        ProgramRig                rig (MakeDisk());
        std::vector<Byte>         module;
        CassoLinkCapture::Result  result;
        int                       checks   = 0;
        const Byte                sector[] = { 0 };
        std::vector<Byte>         data;
        CassoLinkStatus           status;



        AssertSucceeded (LinkProgram::BuildCaptureModule (module));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED),
                          CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-11T00:00:00Z", {}, result, 1, [&checks] () { return ++checks > 1; }));
        Assert::IsTrue (result.woz.empty(), L"No image");

        AssertSucceeded (rig.host->ReadSectors (1, 0, sector, data, status));
        Assert::AreEqual ((Byte) 0, status.error);
    }


    //  Tracks 0 and 1 captured into a WOZ: both are standard, so the
    //  positions between them aren't read; each quarter track reads what the
    //  emulated drive reads there, and the half track has no stream, as a
    //  standard WOZ's doesn't.
    TEST_METHOD (TwoTracks_CaptureToAWozOfTheSameTracks)
    {
        ProgramRig                               rig (MakeDisk());
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


    //  A few bits with no flux right after sector 0's address checksum on
    //  track 0, which the drive reads as noise, as Ultima IV's protection
    //  has. The stretch is shorter than a write splice's, but it starts at
    //  an address field's ending, where no splice falls. The
    //  capture has them as weak bits, so the WOZ has a run of zeros there.
    TEST_METHOD (AShortWeakStretch_AfterAnAddressField_IsCapturedWeak)
    {
        ProgramRig                rig (MakeDisk());
        std::vector<Byte>         module;
        CassoLinkCapture::Result  result;
        DiskImage                 woz;
        DiskImage               * image  = rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0);
        size_t                    ending = 0;
        size_t                    zeros  = 0;
        size_t                    run    = 0;
        int                       slot   = 0;



        Assert::IsNotNull (image);
        slot   = image->ResolveQuarterTrack (0);
        ending = FindAddressEnding (*image, slot, 0);
        Assert::AreNotEqual (SIZE_MAX, ending, L"Sector 0's address field is on track 0");

        //  From the second byte of the ending, as Ultima IV's weak bits start.
        for (size_t at = ending + 8; at < ending + 8 + s_kWeakBits; at++)
        {
            image->WriteBit (slot, at % image->GetTrackBitCount (slot), 0);
        }

        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-21T00:00:00Z", {}, result, 1));
        AssertSucceeded (WozLoader::Load (result.woz, woz));

        Assert::IsTrue (std::find (result.weak.begin(), result.weak.end(), 0) != result.weak.end(), L"Track 0 has weak bits");

        slot = woz.ResolveQuarterTrack (0);

        for (size_t at = 0; at < woz.GetTrackBitCount (slot); at++)
        {
            run   = woz.ReadBit (slot, at) == 0 ? run + 1 : 0;
            zeros = std::max (zeros, run);
        }

        Assert::IsTrue (zeros >= s_kNoiseZeros, L"The WOZ has a run of zeros the drive reads as noise");
    }


    //  An address field that ends E2 AA, as Apventure to Atlantis's do and its
    //  RWTS checks: E2 has three zero bits in a row, which a drive reads as
    //  zeros. The captured byte is E2, where a rule that allowed only two
    //  made it E6.
    TEST_METHOD (AnEndingWithThreeZeroBits_IsCapturedAsRead)
    {
        ProgramRig                rig (MakeDisk());
        std::vector<Byte>         module;
        CassoLinkCapture::Result  result;
        DiskImage                 woz;
        DiskImage               * image  = rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0);
        size_t                    ending = 0;
        int                       slot   = 0;
        Byte                      got    = 0;



        Assert::IsNotNull (image);
        slot   = image->ResolveQuarterTrack (0);
        ending = FindAddressEnding (*image, slot, 0);
        Assert::AreNotEqual (SIZE_MAX, ending, L"Sector 0's address field is on track 0");

        for (int bit = 0; bit < 8; bit++)
        {
            image->WriteBit (slot, (ending + (size_t) bit) % image->GetTrackBitCount (slot), (uint8_t) ((s_kThreeZeros >> (7 - bit)) & 1));
        }

        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-21T00:00:00Z", {}, result, 1));
        AssertSucceeded (WozLoader::Load (result.woz, woz));

        slot   = woz.ResolveQuarterTrack (0);
        ending = FindAddressEnding (woz, slot, 0);
        Assert::AreNotEqual (SIZE_MAX, ending, L"The capture has sector 0's address field");

        for (int bit = 0; bit < 8; bit++)
        {
            got = (Byte) ((got << 1) | woz.ReadBit (slot, (ending + (size_t) bit) % woz.GetTrackBitCount (slot)));
        }

        Assert::AreEqual (s_kThreeZeros, got, L"The ending's first byte is E2");
    }


    //  The bit where the address field of `sector` on a track ends, after its
    //  checksum, or SIZE_MAX.
    static size_t  FindAddressEnding (const DiskImage & image, int slot, int sector)
    {
        size_t               n      = image.GetTrackBitCount (slot);
        std::vector<Byte>    nibs;
        std::vector<size_t>  starts;
        Byte                 reg    = 0;
        size_t               start  = 0;



        for (size_t at = 0; at < n * 2; at++)
        {
            if (reg == 0)
            {
                start = at;
            }

            reg = (Byte) ((reg << 1) | image.ReadBit (slot, at % n));

            if (reg & 0x80)
            {
                nibs.push_back (reg);
                starts.push_back (start);
                reg = 0;
            }
        }

        for (size_t k = 0; k + 11 < nibs.size(); k++)
        {
            int  found = ((nibs[k + 7] << 1) | 1) & nibs[k + 8];

            if (nibs[k] == 0xD5 && nibs[k + 1] == 0xAA && nibs[k + 2] == 0x96 && found == sector)
            {
                return starts[k + 11] % n;
            }
        }

        return SIZE_MAX;
    }


    static constexpr double  s_kRevolutionTolerance = 0.002;
    static constexpr size_t  s_kWeakBits            = 24;    // three bytes
    static constexpr size_t  s_kNoiseZeros          = 3;     // the MC3470 reads noise after this many
    static constexpr Byte    s_kThreeZeros          = 0xE2;
};
