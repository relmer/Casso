#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DamagedMountReport.h"
#include "Devices/Disk/DiskImage.h"
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
};
