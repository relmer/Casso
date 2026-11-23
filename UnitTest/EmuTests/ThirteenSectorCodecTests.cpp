#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodecTests
//
//  A sector buffer written as 13-sector tracks and read back, and a .d13 file
//  through VolumeImage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ThirteenSectorCodecTests)
{
public:

    static constexpr Byte  kVolumeNumber = 254;

    //  Every byte differs from its neighbors and from the same byte of the
    //  next sector, so a sector decoded into the wrong place is a mismatch.
    static vector<Byte> MakePatternedSectors()
    {
        vector<Byte>  sectors (ThirteenSectorCodec::kImageByteSize);
        size_t        i = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) ((i * 7 + (i / ThirteenSectorCodec::kSectorByteSize) * 13) & 0xFF);
        }

        return sectors;
    }


    TEST_METHOD (Nibblize_ThenDenibblize_ReturnsEverySector)
    {
        vector<Byte>        sectors = MakePatternedSectors();
        vector<Byte>        back;
        DiskImage           image;
        SectorDecodeReport  report;
        int                 track   = 0;

        AssertSucceeded (ThirteenSectorCodec::Nibblize (sectors, kVolumeNumber, image));

        Assert::IsTrue (ThirteenSectorCodec::IsThirteenSector (image));

        ThirteenSectorCodec::Denibblize (image, back, report);

        Assert::IsTrue (sectors == back);
        Assert::IsFalse (report.HasDataLoss());

        for (track = 0; track < ThirteenSectorCodec::kTrackCount; track++)
        {
            Assert::IsTrue (report.GetOutcome (track) == TrackDecodeOutcome::Complete);
            Assert::AreEqual ((int) SectorDecodeReport::kFullCoverage, (int) report.GetCoverage (track));
        }
    }


    TEST_METHOD (Nibblize_BufferOfSixteenSectorSize_IsBadLength)
    {
        vector<Byte>  sectors (143360, 0);
        DiskImage     image;
        HRESULT       hr = S_OK;

        hr = ThirteenSectorCodec::Nibblize (sectors, kVolumeNumber, image);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), hr);
    }


    TEST_METHOD (CollectChangedTracks_OneByteChanged_ListsItsTrack)
    {
        vector<Byte>         prior  = MakePatternedSectors();
        vector<Byte>         edited = prior;
        vector<int>          tracks;
        vector<SectorWrite>  writes;
        size_t               at     = ((size_t) 20 * ThirteenSectorCodec::kSectorsPerTrack + 5) * ThirteenSectorCodec::kSectorByteSize;

        edited[at] = (Byte) ~edited[at];

        ThirteenSectorCodec::CollectChangedTracks (prior, edited, tracks);
        ThirteenSectorCodec::CollectSectorWrites (prior, edited, writes);

        Assert::AreEqual (size_t (1), tracks.size());
        Assert::AreEqual (20, tracks[0]);
        Assert::AreEqual (size_t (1), writes.size());
    }


    TEST_METHOD (D13File_LoadsAsItsBytes_AndSavesAsTheEditedBuffer)
    {
        vector<Byte>        file   = MakePatternedSectors();
        vector<Byte>        sectors;
        vector<Byte>        edited;
        vector<Byte>        saved;
        SectorDecodeReport  report;
        std::string         reason;

        AssertSucceeded (VolumeImage::Load (file, "disk.d13", sectors, report));

        Assert::IsTrue (file == sectors);

        edited    = sectors;
        edited[0] = (Byte) ~edited[0];

        AssertSucceeded (VolumeImage::Save (file, "disk.d13", edited, saved, reason));

        Assert::IsTrue (edited == saved);
    }


    TEST_METHOD (D13File_OfAnotherSize_FailsToLoad)
    {
        vector<Byte>        file (143360, 0);
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        HRESULT             hr = S_OK;

        hr = VolumeImage::Load (file, "disk.d13", sectors, report);

        Assert::IsTrue (FAILED (hr));
    }


    static Byte ReadByteAt (const std::vector<Byte> & bits, size_t bitAt)
    {
        Byte  value = 0;

        for (size_t i = 0; i < 8; i++)
        {
            value = (Byte) ((value << 1) | ((bits[(bitAt + i) / 8] >> (7 - (bitAt + i) % 8)) & 1));
        }

        return value;
    }


    static void WriteByteAt (std::vector<Byte> & bits, size_t bitAt, Byte value)
    {
        for (size_t i = 0; i < 8; i++)
        {
            size_t  at   = bitAt + i;
            Byte    mask = (Byte) (0x80 >> (at % 8));

            bits[at / 8] = (Byte) (((value >> (7 - i)) & 1) ? (bits[at / 8] | mask) : (bits[at / 8] & ~mask));
        }
    }


    //  Each prologue on the track, D5 AA third, becomes first AA newThird, as
    //  on the 1981 Ultima's program disk.
    static void AlterPrologues (DiskImage & image, int track, Byte third, Byte first, Byte newThird)
    {
        std::vector<Byte> &  bits  = image.GetTrackBitsForWrite (track);
        size_t               count = image.GetTrackBitCount (track);

        for (size_t at = 0; at + 24 <= count; at++)
        {
            if (ReadByteAt (bits, at) == 0xD5 && ReadByteAt (bits, at + 8) == 0xAA && ReadByteAt (bits, at + 16) == third)
            {
                WriteByteAt (bits, at,      first);
                WriteByteAt (bits, at + 16, newThird);
                at += 23;
            }
        }
    }


    TEST_METHOD (Denibblize_AlteredPrologues_ReturnsEverySector)
    {
        vector<Byte>        flat = MakePatternedSectors();
        vector<Byte>        read;
        DiskImage           image;
        SectorDecodeReport  report;

        AssertSucceeded (ThirteenSectorCodec::Nibblize (flat, kVolumeNumber, image));

        for (int track = 1; track < ThirteenSectorCodec::kTrackCount; track++)
        {
            AlterPrologues (image, track, 0xB5, track % 2 ? 0xDF : 0xDD, track % 2 ? 0xF7 : 0xB7);
            AlterPrologues (image, track, 0xAD, track % 2 ? 0xD7 : 0xDF, track % 2 ? 0xFD : 0xED);
        }

        Assert::IsTrue (ThirteenSectorCodec::IsThirteenSector (image));

        ThirteenSectorCodec::Denibblize (image, read, report);

        Assert::IsFalse (report.HasDataLoss());
        Assert::IsTrue  (read == flat);
    }
};
