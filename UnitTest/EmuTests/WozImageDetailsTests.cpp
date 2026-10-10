#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DamagedMountReport.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Inspector/ImageDetails.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "Machines/Apple2/Common/WozCompatibility.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozImageDetailsTests
//
//  What Casso keeps from a WOZ file for the disk inspector: every INFO field
//  through one parser, and how a track record a map points at loads (FR-053).
//  An all-zero record is an empty track and leaves the image writable; a
//  record that claims data at a location it misstates, or inside the header,
//  or more data than its blocks hold, is damage; and a map entry from 160 to
//  254 is damage to its quarter track.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozImageDetailsTests)
{
public:

    static constexpr size_t  kBitCount   = 51200;
    static constexpr size_t  kCrcOffset  = 8;
    static constexpr size_t  kHeaderSize = 8;



    static vector<Byte> MakeInfo (Byte version)
    {
        vector<Byte>  info (WozLoader::kInfoChunkSize, 0);



        info[WozLoader::kInfoOffsetVersion]          = version;
        info[WozLoader::kInfoOffsetDiskType]         = WozLoader::kDiskType525;
        info[WozLoader::kInfoOffsetWriteProtected]   = 1;
        info[WozLoader::kInfoOffsetSynchronized]     = 1;
        info[WozLoader::kInfoOffsetCleaned]          = 0;
        memcpy (info.data() + WozLoader::kInfoOffsetCreator, "Made up                         ", WozLoader::kInfoCreatorLength);
        info[WozLoader::kInfoOffsetDiskSides]        = 1;
        info[WozLoader::kInfoOffsetBootSectorFormat] = WozLoader::kBootSector13;
        info[WozLoader::kInfoOffsetBitTiming]        = 31;
        info[WozLoader::kInfoOffsetCompatibleHw]     = 0x34;
        info[WozLoader::kInfoOffsetRequiredRam]      = 48;
        info[WozLoader::kInfoOffsetLargestTrack]     = 13;
        info[WozLoader::kInfoOffsetFluxBlock]        = 7;
        info[WozLoader::kInfoOffsetLargestFlux]      = 76;

        return info;
    }



    TEST_METHOD (ReadInfoGivesEachVersionsFields)
    {
        WozInfo  v1;
        WozInfo  v2;
        WozInfo  v3;



        WozLoader::ReadInfo (MakeInfo (1), v1);
        WozLoader::ReadInfo (MakeInfo (2), v2);
        WozLoader::ReadInfo (MakeInfo (3), v3);

        Assert::IsTrue (v1.isPresent && v1.isWriteProtected && v1.isSynchronized && !v1.isCleaned);
        Assert::AreEqual (std::string ("Made up"), v1.creator);
        Assert::IsFalse (v1.hasVersion2Fields, L"version 1 stops after the creator");
        Assert::AreEqual (0, static_cast<int> (v1.sides));

        Assert::IsTrue (v2.hasVersion2Fields && !v2.hasVersion3Fields);
        Assert::AreEqual (1,    static_cast<int> (v2.sides));
        Assert::AreEqual (static_cast<int> (WozLoader::kBootSector13), static_cast<int> (v2.bootSectorFormat));
        Assert::AreEqual (31,   static_cast<int> (v2.optimalBitTiming));
        Assert::AreEqual (0x34, static_cast<int> (v2.compatibleHardware));
        Assert::AreEqual (48,   static_cast<int> (v2.requiredRamK));
        Assert::AreEqual (13,   static_cast<int> (v2.largestTrack));
        Assert::AreEqual (0,    static_cast<int> (v2.fluxBlock), L"version 2 has no flux fields");

        Assert::IsTrue (v3.hasVersion3Fields);
        Assert::AreEqual (7,  static_cast<int> (v3.fluxBlock));
        Assert::AreEqual (76, static_cast<int> (v3.largestFluxTrack));
    }



    TEST_METHOD (AShortPayloadIsAbsent)
    {
        WozInfo       info;
        vector<Byte>  shortPayload (WozLoader::kInfoChunkSize - 1, 2);



        WozLoader::ReadInfo (shortPayload, info);

        Assert::IsFalse (info.isPresent);
    }



    TEST_METHOD (LoadDescribeAndTheCompatibilityCheckAgree)
    {
        vector<Byte>            image;
        DiskImage               disk;
        WozLoader::Description  desc;
        WozRequirements         requirements;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0, 1 }) }, image));
        AssertSucceeded (WozLoader::Load (image, disk));

        WozLoader::Describe (image, desc);
        requirements = WozCompatibility::ReadRequirements (disk.GetWozMetadata());

        Assert::IsTrue (disk.GetWozMetadata().info.isPresent);
        Assert::AreEqual (static_cast<int> (desc.info.version), static_cast<int> (disk.GetWozMetadata().info.version));
        Assert::AreEqual (static_cast<int> (desc.infoVersion), static_cast<int> (desc.info.version));
        Assert::AreEqual (desc.creator, disk.GetWozMetadata().info.creator);
        Assert::AreEqual (static_cast<int> (requirements.requiredRamK), static_cast<int> (disk.GetWozMetadata().info.requiredRamK));
    }



    TEST_METHOD (TheChunksMetaAndChecksumAreKept)
    {
        vector<Byte>           image;
        DiskImage              disk;
        const WozFileLayout *  layout = nullptr;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }) }, image));
        AssertSucceeded (WozLoader::Load (image, disk));
        layout = &disk.GetWozMetadata().layout;

        Assert::IsTrue (layout->chunks.size() >= 3, L"INFO, TMAP and TRKS at least");
        Assert::AreEqual (0, memcmp (layout->chunks[0].id.data(), "INFO", 4));
        Assert::AreEqual (layout->computedCrc, layout->storedCrc == 0 ? layout->computedCrc : layout->storedCrc);
        Assert::IsFalse (layout->hasDataPastLastChunk);
    }



    static WozSyntheticTrack MakeBitTrack (vector<int> quarterTracks)
    {
        WozSyntheticTrack  track;



        track.data.assign ((kBitCount + 7) / 8, 0xFF);
        track.bitCount      = kBitCount;
        track.quarterTracks = quarterTracks;

        return track;
    }



    static size_t FindChunk (const vector<Byte> & image, const char * id)
    {
        size_t  at    = WozLoader::kHeaderSize;
        size_t  found = 0;



        for (at = WozLoader::kHeaderSize; at + 4 <= image.size() && found == 0; at++)
        {
            if (memcmp (image.data() + at, id, 4) == 0)
            {
                found = at + kHeaderSize;
            }
        }

        return found;
    }



    //  Two good tracks, then record 2 mapped at quarter track 8 with its fields
    //  set as given. The stored checksum is zeroed so only the record differs.
    static vector<Byte> MakeWithRecord (uint16_t startBlock, uint16_t blockCount, uint32_t bitCount)
    {
        vector<Byte>  image;
        size_t        trks  = 0;
        Byte *        rec   = nullptr;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }), MakeBitTrack ({ 4 }), MakeBitTrack ({ 8 }) }, image));

        trks = FindChunk (image, "TRKS");
        rec  = image.data() + trks + 2 * WozLoader::kV2TrkRecordSize;

        rec[0] = static_cast<Byte> (startBlock);
        rec[1] = static_cast<Byte> (startBlock >> 8);
        rec[2] = static_cast<Byte> (blockCount);
        rec[3] = static_cast<Byte> (blockCount >> 8);
        rec[4] = static_cast<Byte> (bitCount);
        rec[5] = static_cast<Byte> (bitCount >> 8);
        rec[6] = static_cast<Byte> (bitCount >> 16);
        rec[7] = static_cast<Byte> (bitCount >> 24);

        memset (image.data() + kCrcOffset, 0, 4);

        return image;
    }



    static void AssertDamage (uint16_t startBlock, uint16_t blockCount, uint32_t bitCount, DamageReason reason)
    {
        DiskImage  disk;



        AssertSucceeded (WozLoader::Load (MakeWithRecord (startBlock, blockCount, bitCount), disk));

        Assert::AreEqual (1, static_cast<int> (disk.GetDamagedTracks().size()));
        Assert::IsTrue (disk.GetDamagedTracks()[0].reason == reason);
        Assert::AreEqual (2, disk.GetDamagedTracks()[0].trkIndex);
        Assert::IsTrue (disk.IsWriteProtected(), L"a damaged image is held read-only");
        Assert::AreEqual (2, disk.GetMappedSlot (8), L"the damaged record stays mapped");
    }



    TEST_METHOD (AnAllZeroRecordIsAnEmptyTrackAndTheImageStaysWritable)
    {
        DiskImage  disk;



        AssertSucceeded (WozLoader::Load (MakeWithRecord (0, 0, 0), disk));

        Assert::IsFalse (disk.HasDamagedTracks());
        Assert::IsFalse (disk.IsWriteProtected());
        Assert::AreEqual (-1, disk.ResolveQuarterTrack (8), L"nothing recorded");
        Assert::AreEqual (2, static_cast<int> (disk.GetWozMetadata().layout.tmap[8]), L"the map entry is kept as stored");
    }



    TEST_METHOD (ACountWithAZeroStartBlockIsDamage)
    {
        AssertDamage (0, 100, static_cast<uint32_t> (kBitCount), DamageReason::RecordLocationMissing);
    }



    TEST_METHOD (ACountWithAZeroBlockCountIsDamage)
    {
        AssertDamage (3, 0, static_cast<uint32_t> (kBitCount), DamageReason::RecordLocationMissing);
    }



    TEST_METHOD (AStartBlockInsideTheHeaderIsDamage)
    {
        AssertDamage (2, 13, static_cast<uint32_t> (kBitCount), DamageReason::RecordInHeader);
    }



    TEST_METHOD (ABitCountLargerThanItsBlocksIsDamage)
    {
        AssertDamage (3, 1, static_cast<uint32_t> (kBitCount), DamageReason::CountExceedsBlocks);
    }



    TEST_METHOD (AMapEntryPastTheRecordsIsDamageToItsQuarterTrack)
    {
        vector<Byte>  image;
        DiskImage     disk;
        vector<int>   damaged;
        size_t        tmap    = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }) }, image));

        tmap               = FindChunk (image, "TMAP");
        image[tmap + 20]   = 200;
        image[tmap + 21]   = 0xFF;
        memset (image.data() + kCrcOffset, 0, 4);

        AssertSucceeded (WozLoader::Load (image, disk));

        Assert::AreEqual (1, static_cast<int> (disk.GetDamagedQuarterTrackEntries().size()));
        Assert::AreEqual (20, disk.GetDamagedQuarterTrackEntries()[0].quarterTrack);
        Assert::AreEqual (200, static_cast<int> (disk.GetDamagedQuarterTrackEntries()[0].entry));
        Assert::IsTrue (disk.IsWriteProtected());

        damaged = DamagedMountReport::GetDamagedQuarterTracks (disk);

        Assert::IsTrue (std::find (damaged.begin(), damaged.end(), 20) != damaged.end());
        Assert::AreEqual (-1, disk.ResolveQuarterTrack (21), L"an entry of 255 is simply unmapped");
    }



    //  The Image tab (FR-050, FR-054): every field the file's version has,
    //  WOZ 1's record trailers and the records no map refers to, a 3.5" disk
    //  shown as one, and the reason a file cannot be opened.
    static vector<TableRow> BuildRows (const vector<Byte> & bytes)
    {
        DiskImage  disk;



        AssertSucceeded (WozLoader::Load (bytes, disk));

        return InspectorTables::BuildImage (ImageDetails::MakeFromCopy (*DiskCopy::MakeFromImage (disk, 1, "test.woz", bytes.size(), false)));
    }



    static std::wstring GetValue (const vector<TableRow> & rows, const std::wstring & item)
    {
        std::wstring  value = L"(missing)";



        for (const TableRow & row : rows)
        {
            if (row.cells[0] == item && value == L"(missing)")
            {
                value = row.cells[1];
            }
        }

        return value;
    }



    static void AppendChunk (vector<Byte> & inOut, const char * id, const vector<Byte> & payload)
    {
        uint32_t  size = static_cast<uint32_t> (payload.size());



        inOut.insert (inOut.end(), id, id + 4);

        for (int b = 0; b < 4; b++)
        {
            inOut.push_back (static_cast<Byte> (size >> (8 * b)));
        }

        inOut.insert (inOut.end(), payload.begin(), payload.end());
    }



    //  A WOZ 1 file with two records: record 0 mapped at track 0, record 1
    //  holding 100 bytes that no map entry points at.
    static vector<Byte> MakeWoz1()
    {
        static constexpr size_t  kRecord  = WozLoader::kV1TrackRecordSize;
        static constexpr size_t  kTrailer = 6646;

        vector<Byte>  bytes = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A, 0, 0, 0, 0 };
        vector<Byte>  tmap  (160, 0xFF);
        vector<Byte>  trks  (2 * kRecord, 0);



        tmap[0] = 0;

        std::fill (trks.begin(), trks.begin() + 6400, Byte (0xFF));
        trks[kTrailer]     = 0x00;
        trks[kTrailer + 1] = 0x19;
        trks[kTrailer + 2] = 0x00;
        trks[kTrailer + 3] = 0xC8;
        trks[kTrailer + 4] = 0xFF;
        trks[kTrailer + 5] = 0xFF;

        std::fill (trks.begin() + kRecord, trks.begin() + kRecord + 100, Byte (0xD5));
        trks[kRecord + kTrailer]     = 100;
        trks[kRecord + kTrailer + 2] = 0x20;
        trks[kRecord + kTrailer + 3] = 0x03;

        AppendChunk (bytes, "INFO", MakeInfo (1));
        AppendChunk (bytes, "TMAP", tmap);
        AppendChunk (bytes, "TRKS", trks);

        return bytes;
    }



    TEST_METHOD (TheImageTabGivesEveryFieldTheVersionHas)
    {
        vector<Byte>      image;
        vector<TableRow>  rows;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0, 1 }) }, image));
        rows = BuildRows (image);

        for (LPCWSTR item : { L"File name", L"Format", L"Size", L"Read-only", L"Checksum", L"Version", L"Disk type", L"Write protected", L"Synchronized",
                              L"Cleaned", L"Creator", L"Sides", L"Boot sector format", L"Optimal bit timing", L"Compatible hardware", L"Required RAM",
                              L"Largest track", L"Flux block", L"Largest flux track", L"TMAP", L"Track records", L"Chunks", L"INFO", L"TRKS" })
        {
            Assert::AreNotEqual (std::wstring (L"(missing)"), GetValue (rows, item), item);
        }

        Assert::AreEqual (std::wstring (L"WOZ 2.1"), GetValue (rows, L"Format"));
        Assert::AreEqual (std::wstring (L"Record 0"), GetValue (rows, L"0-0.25"), L"a run of quarter tracks on one row");
        Assert::AreEqual (std::wstring (L"5.25\""), GetValue (rows, L"Disk type"));
    }



    TEST_METHOD (AWoz1ShowsItsRecordTrailersAndTheRecordsNoMapRefersTo)
    {
        vector<Byte>      image = MakeWoz1();
        DiskImage         disk;
        ImageDetails      details;
        vector<TableRow>  rows;



        AssertSucceeded (WozLoader::Load (image, disk));
        details = ImageDetails::MakeFromCopy (*DiskCopy::MakeFromImage (disk, 1, "old.woz", image.size(), false));
        rows    = InspectorTables::BuildImage (details);

        Assert::AreEqual (static_cast<size_t> (2), details.layout.v1Records.size());
        Assert::AreEqual (static_cast<int> (0xC800), static_cast<int> (details.layout.v1Records[0].bitCount));
        Assert::AreEqual (static_cast<size_t> (1), details.layout.unreferenced.size());
        Assert::AreEqual (1, details.layout.unreferenced[0].index);
        Assert::IsTrue   (std::any_of (details.problems.begin(), details.problems.end(), [] (const Finding & f) { return f.kind == FindingKind::UnreferencedRecord; }));
        Assert::AreEqual (std::wstring (L"WOZ 1"), GetValue (rows, L"Format"));
        Assert::IsTrue   (GetValue (rows, L"Record 0").starts_with (L"6,400 bytes used, 51,200 bits"));
        Assert::AreNotEqual (std::wstring (L"(missing)"), GetValue (rows, L"Records no map refers to"));
    }



    TEST_METHOD (A35InchDiskSaysSo)
    {
        vector<Byte>  image;
        size_t        info  = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ MakeBitTrack ({ 0 }) }, image));
        info = FindChunk (image, "INFO");
        image[info + WozLoader::kInfoOffsetDiskType] = WozLoader::kDiskType35;
        std::fill (image.begin() + kCrcOffset, image.begin() + kCrcOffset + 4, Byte (0));

        Assert::AreEqual (std::wstring (L"3.5\""), GetValue (BuildRows (image), L"Disk type"));
    }



    TEST_METHOD (AFileThatCannotBeOpenedSaysWhy)
    {
        const vector<Byte>  junk (300, 0x42);
        MountDiagnosis      diagnosis = DiskImageStore::ClassifyLoadFailure (DiskFormat::Woz, junk);



        Assert::IsTrue  (diagnosis.failure != MountFailure::None);
        Assert::IsFalse (DiskImageStore::FormatMountFailureMessage ("junk.woz", diagnosis).empty());
    }
};
