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
};
