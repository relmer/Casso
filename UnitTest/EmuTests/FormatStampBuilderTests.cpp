#include "Pch.h"

#include "StampDiskFixtures.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilderTests
//
//  Stamps built from in-memory disks: standard ones, and ones whose prologs
//  were rewritten to marks the standard settings do not match.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampBuilderTests)
{
public:

    static constexpr int   kSixteen  = 16;
    static constexpr int   kThirteen = 13;
    static constexpr Byte  kD5       = 0xD5;
    static constexpr Byte  kAA       = 0xAA;
    static constexpr Byte  k96       = 0x96;



    static FormatStamp Build (const DiskImage & image, const CaptureReport * capture = nullptr)
    {
        FormatStamp  stamp;



        FormatStampBuilder::Build (*StampDiskFixtures::MakeCopy (image), DecodeSettings::MakeStandard(), capture ? StampSource::Capture : StampSource::Inspector, capture, "2026-11-23T00:00:00Z", stamp);

        return stamp;
    }



    TEST_METHOD (Build_StandardSixteenSectorDisk_ListsEveryTrackAsVerified)
    {
        DiskImage    image;
        FormatStamp  stamp;



        StampDiskFixtures::MakeSixteenSector (image);
        stamp = Build (image);

        Assert::AreEqual ((size_t) StampDiskFixtures::kTracks, stamp.tracks.size());
        Assert::IsTrue   (stamp.source == StampSource::Inspector);
        Assert::IsTrue   (stamp.decodeRanges.empty(), L"standard settings are not recorded");
        Assert::AreEqual (std::string ("2026-11-23T00:00:00Z"), stamp.written);
        Assert::IsTrue   (stamp.writer.starts_with ("Casso "));

        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            const StampTrack &  entry = stamp.tracks[(size_t) track];

            Assert::AreEqual (track * DiskImage::kQuarterTracksPerWholeTrack, entry.position);
            Assert::IsTrue   (entry.encoding == StampEncoding::Sixteen);
            Assert::AreEqual (kSixteen, entry.sectorsFound);
            Assert::AreEqual (kSixteen, entry.sectorsGood);
            Assert::IsTrue   (entry.isVerified);
            Assert::IsTrue   (entry.addressPrologs == vector<vector<Byte>> { { kD5, kAA, k96 } });
            Assert::IsTrue   (entry.dataPrologs    == vector<vector<Byte>> { { kD5, kAA, 0xAD } });
            Assert::AreEqual ((size_t) kSixteen, entry.sectors.size());
            Assert::IsTrue   (entry.volumes == vector<Byte> { StampDiskFixtures::kVolume });
            Assert::IsTrue   (entry.addressChecksum == StampChecksum::Standard);
            Assert::IsTrue   (entry.dataChecksum    == StampChecksum::Standard);
            Assert::IsFalse  (entry.hasCapture);
        }
    }



    TEST_METHOD (Build_StandardThirteenSectorDisk_ListsTheThirteenSectorProlog)
    {
        DiskImage    image;
        FormatStamp  stamp;



        StampDiskFixtures::MakeThirteenSector (image);
        stamp = Build (image);

        Assert::AreEqual ((size_t) StampDiskFixtures::kTracks, stamp.tracks.size());
        Assert::IsTrue   (stamp.tracks[3].encoding == StampEncoding::Thirteen);
        Assert::AreEqual (kThirteen, stamp.tracks[3].sectorsGood);
        Assert::IsTrue   (stamp.tracks[3].addressPrologs == vector<vector<Byte>> { { kD5, kAA, 0xB5 } });
    }



    TEST_METHOD (Build_D4AddressProlog_FindsAndListsIt)
    {
        DiskImage    image;
        FormatStamp  stamp;
        Byte         from[] = { kD5, kAA, k96 };
        Byte         to[]   = { 0xD4, kAA, k96 };



        StampDiskFixtures::MakeSixteenSector (image);

        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            StampDiskFixtures::ReplaceProlog (image, track, from, to);
        }

        stamp = Build (image);

        Assert::IsTrue   (stamp.tracks[7].addressPrologs == vector<vector<Byte>> { { 0xD4, kAA, k96 } });
        Assert::AreEqual (kSixteen, stamp.tracks[7].sectorsGood);
        Assert::IsTrue   (stamp.tracks[7].isVerified);
        Assert::IsTrue   (stamp.tracks[7].encoding == StampEncoding::Sixteen, L"a track whose every sector decodes with the found marks is its encoding");
    }



    TEST_METHOD (Build_ADifferentPrologOnEachTrack_ListsEachTracksProlog)
    {
        constexpr Byte  kFirsts[] = { 0xD7, 0xDD, 0xDF };

        DiskImage    image;
        FormatStamp  stamp;
        Byte         from[] = { kD5, kAA, k96 };



        StampDiskFixtures::MakeSixteenSector (image);

        for (int track = 1; track < StampDiskFixtures::kTracks; track++)
        {
            Byte  to[] = { kFirsts[track % std::size (kFirsts)], kAA, k96 };

            StampDiskFixtures::ReplaceProlog (image, track, from, to);
        }

        stamp = Build (image);

        Assert::IsTrue (stamp.tracks[0].addressPrologs == vector<vector<Byte>> { { kD5, kAA, k96 } }, L"track 0 is standard");

        for (int track = 1; track < StampDiskFixtures::kTracks; track++)
        {
            Assert::IsTrue (stamp.tracks[(size_t) track].addressPrologs == vector<vector<Byte>> { { kFirsts[track % std::size (kFirsts)], kAA, k96 } });
            Assert::IsTrue (stamp.tracks[(size_t) track].isVerified);
        }
    }



    TEST_METHOD (Build_TwoPrologsOnOneTrack_ListsBoth)
    {
        constexpr int  kHalfTheSectors = 8;

        DiskImage    image;
        FormatStamp  stamp;
        Byte         from[] = { kD5, kAA, k96 };
        Byte         to[]   = { 0xD7, kAA, k96 };



        StampDiskFixtures::MakeSixteenSector (image);
        Assert::AreEqual (kHalfTheSectors, StampDiskFixtures::ReplaceProlog (image, 9, from, to, kHalfTheSectors));

        stamp = Build (image);

        Assert::AreEqual ((size_t) 2, stamp.tracks[9].addressPrologs.size());
        Assert::AreEqual (kSixteen, stamp.tracks[9].sectorsGood);
    }



    TEST_METHOD (Build_ATrackWithNoBits_IsEmptyAndUnverified)
    {
        DiskImage    image;
        FormatStamp  stamp;



        StampDiskFixtures::MakeSixteenSector (image);
        std::ranges::fill (image.GetTrackBitsForWrite (12), (Byte) 0);

        stamp = Build (image);

        Assert::AreEqual (0, stamp.tracks[12].sectorsGood);
        Assert::IsFalse  (stamp.tracks[12].isVerified);
        Assert::IsTrue   (stamp.tracks[12].addressPrologs.empty());
    }



    TEST_METHOD (Build_WithACaptureReport_HasItsFacts)
    {
        DiskImage      image;
        FormatStamp    stamp;
        CaptureReport  capture;



        StampDiskFixtures::MakeSixteenSector (image);

        capture.facts.reads            = 3;
        capture.facts.hasRevolution    = true;
        capture.facts.revolutionCycles = 200000.0;
        capture.facts.isSynchronized   = true;
        capture.weak                   = { 8 };
        capture.unstable               = { 12 };
        capture.replaced               = { { 8, 3 }, { 8, 11 } };

        stamp = Build (image, &capture);

        Assert::IsTrue   (stamp.source == StampSource::Capture);
        Assert::IsTrue   (stamp.hasCapture);
        Assert::AreEqual (3, stamp.capture.reads);
        Assert::IsTrue   (stamp.tracks[2].hasCapture);
        Assert::IsTrue   (stamp.tracks[2].capture.isWeak);
        Assert::IsTrue   (stamp.tracks[2].capture.replacedSectors == vector<int> { 3, 11 });
        Assert::IsTrue   (stamp.tracks[3].capture.isUnstable);
        Assert::IsFalse  (stamp.tracks[4].capture.isWeak);
    }



    TEST_METHOD (GetStampPositions_AHalfTrackWithASeparateRecord_IsListed)
    {
        DiskImage    image;
        int          slots = 0;
        vector<int>  positions;



        StampDiskFixtures::MakeSixteenSector (image);

        slots = image.GetTrackCount();
        image.EnsureTrackSlots (slots + 1);
        image.ResizeTrack (slots, image.GetTrackBitCount (0));
        image.SetQuarterTrackSlot (2, slots);

        positions = FormatStampBuilder::GetStampPositions (*StampDiskFixtures::MakeCopy (image));

        Assert::IsTrue   (std::ranges::find (positions, 2) != positions.end(), L"a half track with a record of its own has an entry");
        Assert::IsFalse  (std::ranges::find (positions, 1) != positions.end(), L"a quarter track that plays track 0 has none");
        Assert::IsFalse  (std::ranges::find (positions, 3) != positions.end(), L"a quarter track that plays track 1 has none");
    }
};
