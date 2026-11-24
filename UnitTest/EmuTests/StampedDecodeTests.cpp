#include "Pch.h"

#include "StampDiskFixtures.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozMetadata.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StampedDecodeTests
//
//  Host-side reads of in-memory disks whose prologs no built-in list has,
//  with a format stamp listing them, with a wrong stamp, and with none.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StampedDecodeTests)
{
public:

    static constexpr Byte  kD5     = 0xD5;
    static constexpr Byte  kD7     = 0xD7;
    static constexpr Byte  kDA     = 0xDA;
    static constexpr Byte  kAA     = 0xAA;
    static constexpr Byte  k96     = 0x96;
    static constexpr Byte  kAD     = 0xAD;
    static constexpr Byte  kB5     = 0xB5;



    //  Every track's address and data prologs rewritten with another first
    //  nibble.
    static void ReplaceFirstNibble (DiskImage & image, Byte addressThird, Byte first)
    {
        Byte  address[] = { kD5, kAA, addressThird };
        Byte  data[]    = { kD5, kAA, kAD };
        Byte  newAddr[] = { first, kAA, addressThird };
        Byte  newData[] = { first, kAA, kAD };



        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            StampDiskFixtures::ReplaceProlog (image, track, address, newAddr);
            StampDiskFixtures::ReplaceProlog (image, track, data,    newData);
        }
    }



    static void SetStamp (DiskImage & image, StampEncoding encoding, Byte addressFirst, Byte addressThird, Byte dataFirst)
    {
        FormatStamp  stamp;
        WozMetadata  meta = image.GetWozMetadata();



        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            StampTrack  entry;

            entry.position       = track * DiskImage::kQuarterTracksPerWholeTrack;
            entry.encoding       = encoding;
            entry.addressPrologs = { { addressFirst, kAA, addressThird } };
            entry.dataPrologs    = { { dataFirst, kAA, kAD } };
            stamp.tracks.push_back (entry);
        }

        FormatStampChunk::SetStamp (meta, stamp);
        image.SetWozMetadata (meta);
    }



    static void SetUnreadableStamp (DiskImage & image)
    {
        WozMetadata  meta = image.GetWozMetadata();
        WozChunk     chunk;
        std::string  text = "{ \"format\": \"casso-woz-stamp\", \"version\": \"9.0\" }";



        memcpy (chunk.id, FormatStampChunk::kId, sizeof (chunk.id));
        chunk.payload.assign (text.begin(), text.end());
        meta.passThrough.push_back (chunk);
        image.SetWozMetadata (meta);
    }



    static HRESULT ReadSixteen (const DiskImage & image, vector<Byte> & outSectors, DenibblizeReport & outReport)
    {
        return Denibblizer::Denibblize (image, DiskFormat::Dsk, outSectors, outReport);
    }



    TEST_METHOD (Sixteen_MarksOnNoList_ReadWithTheStamp)
    {
        DiskImage         plain;
        DiskImage         image;
        vector<Byte>      expected;
        vector<Byte>      sectors;
        DenibblizeReport  report;
        HRESULT           hr = S_OK;



        StampDiskFixtures::MakeSixteenSector (plain);
        ReadSixteen (plain, expected, report);

        StampDiskFixtures::MakeSixteenSector (image);
        ReplaceFirstNibble (image, k96, kD7);
        SetStamp (image, StampEncoding::Sixteen, kD7, k96, kD7);

        hr = ReadSixteen (image, sectors, report);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (StampDiskFixtures::kTracks, report.tracksComplete);
        Assert::IsTrue   (sectors == expected, L"the stamped read has the disk's bytes");
    }



    TEST_METHOD (Sixteen_MarksOnNoList_FailWithoutTheStamp)
    {
        DiskImage         image;
        vector<Byte>      sectors;
        DenibblizeReport  report;



        StampDiskFixtures::MakeSixteenSector (image);
        ReplaceFirstNibble (image, k96, kD7);

        ReadSixteen (image, sectors, report);

        Assert::AreEqual (0, report.tracksComplete);
    }



    TEST_METHOD (Sixteen_WrongStamp_ReadsAsWithoutOne)
    {
        DiskImage           image;
        DiskImage           wrong;
        vector<Byte>        expected;
        vector<Byte>        sectors;
        SectorDecodeReport  expectedReport;
        SectorDecodeReport  report;



        StampDiskFixtures::MakeSixteenSector (image);
        ReplaceFirstNibble (image, k96, kD7);
        Denibblizer::Denibblize (image, DiskFormat::Dsk, expected, expectedReport);

        StampDiskFixtures::MakeSixteenSector (wrong);
        ReplaceFirstNibble (wrong, k96, kD7);
        SetStamp (wrong, StampEncoding::Sixteen, kDA, k96, kDA);
        Denibblizer::Denibblize (wrong, DiskFormat::Dsk, sectors, report);

        Assert::IsTrue (sectors == expected);

        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            Assert::IsTrue (report.GetOutcome (track) == expectedReport.GetOutcome (track));
        }
    }



    TEST_METHOD (Sixteen_StandardDiskWithAWrongStamp_ReadsAsWithoutOne)
    {
        DiskImage         image;
        DiskImage         stamped;
        vector<Byte>      expected;
        vector<Byte>      sectors;
        DenibblizeReport  report;



        StampDiskFixtures::MakeSixteenSector (image);
        ReadSixteen (image, expected, report);

        StampDiskFixtures::MakeSixteenSector (stamped);
        SetStamp (stamped, StampEncoding::Sixteen, kDA, k96, kDA);
        ReadSixteen (stamped, sectors, report);

        Assert::IsTrue   (sectors == expected);
        Assert::AreEqual (StampDiskFixtures::kTracks, report.tracksComplete);
    }



    TEST_METHOD (Sixteen_UnreadableStamp_ReadsAsWithoutOne)
    {
        DiskImage         image;
        vector<Byte>      sectors;
        DenibblizeReport  report;



        StampDiskFixtures::MakeSixteenSector (image);
        ReplaceFirstNibble (image, k96, kD7);
        SetUnreadableStamp (image);

        ReadSixteen (image, sectors, report);

        Assert::AreEqual (0, report.tracksComplete);
    }



    TEST_METHOD (Thirteen_MarksOnNoList_ReadWithTheStamp)
    {
        DiskImage           plain;
        DiskImage           image;
        vector<Byte>        expected;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        StampDiskFixtures::MakeThirteenSector (plain);
        ThirteenSectorCodec::Denibblize (plain, expected, report);

        StampDiskFixtures::MakeThirteenSector (image);
        ReplaceFirstNibble (image, kB5, kDA);
        SetStamp (image, StampEncoding::Thirteen, kDA, kB5, kDA);

        ThirteenSectorCodec::Denibblize (image, sectors, report);

        Assert::IsTrue (sectors == expected, L"the stamped read has the disk's bytes");
        Assert::IsTrue (report.GetOutcome (5) == TrackDecodeOutcome::Complete);
    }



    TEST_METHOD (Thirteen_MarksOnNoList_FailWithoutTheStamp)
    {
        DiskImage           image;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        StampDiskFixtures::MakeThirteenSector (image);
        ReplaceFirstNibble (image, kB5, kDA);

        ThirteenSectorCodec::Denibblize (image, sectors, report);

        Assert::IsFalse (report.GetOutcome (5) == TrackDecodeOutcome::Complete);
    }



    TEST_METHOD (Thirteen_WrongStamp_ReadsAsWithoutOne)
    {
        DiskImage           image;
        DiskImage           wrong;
        vector<Byte>        expected;
        vector<Byte>        sectors;
        SectorDecodeReport  expectedReport;
        SectorDecodeReport  report;



        StampDiskFixtures::MakeThirteenSector (image);
        ReplaceFirstNibble (image, kB5, kDA);
        ThirteenSectorCodec::Denibblize (image, expected, expectedReport);

        StampDiskFixtures::MakeThirteenSector (wrong);
        ReplaceFirstNibble (wrong, kB5, kDA);
        SetStamp (wrong, StampEncoding::Thirteen, kD7, kB5, kD7);
        ThirteenSectorCodec::Denibblize (wrong, sectors, report);

        Assert::IsTrue (sectors == expected);

        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            Assert::IsTrue (report.GetOutcome (track) == expectedReport.GetOutcome (track));
        }
    }



    TEST_METHOD (IsThirteenSector_MarksOnNoList_UsesTheStampsEncoding)
    {
        DiskImage  image;



        StampDiskFixtures::MakeThirteenSector (image);
        ReplaceFirstNibble (image, kB5, kDA);

        Assert::IsFalse (ThirteenSectorCodec::IsThirteenSector (image), L"without a stamp, no address field matches");

        SetStamp (image, StampEncoding::Thirteen, kDA, kB5, kDA);

        Assert::IsTrue (ThirteenSectorCodec::IsThirteenSector (image));
    }
};
