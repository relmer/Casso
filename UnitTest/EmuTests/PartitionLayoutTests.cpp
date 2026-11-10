#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionLayoutTests
//
//  Stores built in memory for each partition layout: the partitions found,
//  the volume kind of each, and a partition's bytes read and written.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PartitionLayoutTests)
{
public:

    static constexpr size_t    kBlock        = IBlockStore::kBlockBytes;
    static constexpr uint32_t  kSliceBlocks  = 65536;
    static constexpr uint32_t  kDiskBlocks   = 1600;
    static constexpr Byte      kVolumeNumber = 254;

    static vector<Byte> MakeProDos (uint32_t blocks)
    {
        vector<Byte>  buffer ((size_t) blocks * kBlock, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, "TEST"));

        return buffer;
    }

    static vector<Byte> MakeDos (int tracks, int sectorsPerTrack)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (tracks, sectorsPerTrack);
        vector<Byte>   buffer (geometry.GetImageByteSize(), 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber, geometry));

        return buffer;
    }

    static void Place (vector<Byte> & bytes, uint32_t block, const vector<Byte> & source)
    {
        std::copy (source.begin(), source.end(), bytes.begin() + (ptrdiff_t) ((size_t) block * kBlock));
    }

    static void WriteLong (vector<Byte> & bytes, size_t offset, uint32_t value)
    {
        bytes[offset]     = (Byte) (value & 0xFF);
        bytes[offset + 1] = (Byte) ((value >> 8) & 0xFF);
        bytes[offset + 2] = (Byte) ((value >> 16) & 0xFF);
        bytes[offset + 3] = (Byte) ((value >> 24) & 0xFF);
    }

    static void WriteWord (vector<Byte> & bytes, size_t offset, Word value)
    {
        bytes[offset]     = (Byte) (value & 0xFF);
        bytes[offset + 1] = (Byte) (value >> 8);
    }

    static void WriteText (vector<Byte> & bytes, size_t offset, const char * text)
    {
        std::copy (text, text + strlen (text), bytes.begin() + (ptrdiff_t) offset);
    }

    //  Two volumes of 50 tracks and 32 sectors, one after the other.
    static vector<Byte> MakeUniDosDisk()
    {
        vector<Byte>  volume = MakeDos (50, 32);
        vector<Byte>  bytes ((size_t) kDiskBlocks * kBlock, 0);

        Place (bytes, 0, volume);
        Place (bytes, kDiskBlocks / 2, volume);

        return bytes;
    }


    TEST_METHOD (Detect_BlankStore_HasNoPartitions)
    {
        MemoryBlockStore        store (vector<Byte> ((size_t) kDiskBlocks * kBlock, 0));
        vector<Partition>       partitions;
        PartitionLayoutKind     kind = PartitionLayoutKind::Cffa;

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::None);
        Assert::IsTrue (partitions.empty());
    }


    TEST_METHOD (Detect_PlainProDosVolume_HasNoPartitions)
    {
        MemoryBlockStore     store (MakeProDos (kDiskBlocks));
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::Cffa;

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::None);
        Assert::IsTrue (partitions.empty());
    }


    //  A map of two partitions in the first chunk and one in the second. The
    //  last begins inside the store and ends past it.
    TEST_METHOD (MicroDrive_MapOfBothChunks_ListsEveryPartition_AndClipsTheLast)
    {
        vector<Byte>         bytes ((size_t) 2000 * kBlock, 0);
        vector<Byte>         floppy (143360, 0);
        vector<Byte>         floppyBlocks;
        vector<Byte>         sectors;
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        AssertSucceeded (ProDosSkeleton::Write (floppy, "FLOPPY"));
        VolumeImage::DosLogicalToProDosFile (floppy, floppyBlocks);

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 2;
        bytes[0x0D] = 1;

        WriteLong (bytes, 0x20, 8);
        WriteLong (bytes, 0x40, 280);
        WriteLong (bytes, 0x24, 300);
        WriteLong (bytes, 0x44, 0xAB000000 | 800);
        WriteLong (bytes, 0x80, 1900);
        WriteLong (bytes, 0xA0, 500);

        Place (bytes, 8, floppyBlocks);
        Place (bytes, 300, MakeProDos (800));

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::MicroDrive);
        Assert::AreEqual (size_t (3), partitions.size());

        Assert::AreEqual (1, partitions[0].index);
        Assert::AreEqual (8u, partitions[0].firstBlock);
        Assert::AreEqual (280u, partitions[0].blockCount);
        Assert::IsTrue (partitions[0].volumeKind == VolumeKind::ProDos);
        Assert::IsFalse (partitions[0].damaged);

        Assert::AreEqual (300u, partitions[1].firstBlock);
        Assert::AreEqual (800u, partitions[1].blockCount);
        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::ProDos);

        Assert::AreEqual (3, partitions[2].index);
        Assert::AreEqual (1900u, partitions[2].firstBlock);
        Assert::AreEqual (100u, partitions[2].blockCount);
        Assert::IsTrue (partitions[2].damaged);
        Assert::IsTrue (partitions[2].volumeKind == VolumeKind::Unknown);

        //  A 140K volume is read in DOS logical order.
        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[0], sectors));

        Assert::IsTrue (floppy == sectors);
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);
    }


    TEST_METHOD (MicroDrive_EntryPastTheStore_IsListedEmptyAndDamaged)
    {
        vector<Byte>         bytes ((size_t) 100 * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 1;

        WriteLong (bytes, 0x20, 5000);
        WriteLong (bytes, 0x40, 280);

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::AreEqual (size_t (1), partitions.size());
        Assert::AreEqual (0u, partitions[0].blockCount);
        Assert::IsTrue (partitions[0].damaged);
    }


    TEST_METHOD (MicroDrive_CountOverEight_IsNotAMap)
    {
        vector<Byte>         bytes ((size_t) 100 * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 9;

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (partitions.empty());
    }


    TEST_METHOD (FocusDrive_MapWithNames_ListsEachPartitionWithItsName)
    {
        vector<Byte>         bytes ((size_t) 2000 * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        WriteText (bytes, 0, "Parsons Engin.");

        bytes[0x0F] = 2;

        WriteLong (bytes, 0x20, 16);
        WriteLong (bytes, 0x24, 800);
        WriteLong (bytes, 0x30, 816);
        WriteLong (bytes, 0x34, 400);

        WriteText (bytes, kBlock + 0x20, "WORK");
        WriteText (bytes, kBlock + 0x40, "GAMES");

        Place (bytes, 16, MakeProDos (800));

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::FocusDrive);
        Assert::AreEqual (size_t (2), partitions.size());

        Assert::AreEqual (16u, partitions[0].firstBlock);
        Assert::AreEqual (800u, partitions[0].blockCount);
        Assert::AreEqual (std::string ("WORK"), partitions[0].name);
        Assert::IsTrue (partitions[0].volumeKind == VolumeKind::ProDos);

        Assert::AreEqual (816u, partitions[1].firstBlock);
        Assert::AreEqual (400u, partitions[1].blockCount);
        Assert::AreEqual (std::string ("GAMES"), partitions[1].name);
        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::Unknown);
    }


    TEST_METHOD (FocusDrive_CountOfZero_IsNotAMap)
    {
        vector<Byte>         bytes ((size_t) 100 * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        WriteText (bytes, 0, "Parsons Engin.");

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (partitions.empty());
    }


    //  A volume in the first slice and a short one after it.
    TEST_METHOD (Cffa_StoreOfTwoSlices_ListsBoth)
    {
        vector<Byte>         bytes ((size_t) (kSliceBlocks + kDiskBlocks) * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        Place (bytes, 0, MakeProDos (kDiskBlocks));
        Place (bytes, kSliceBlocks, MakeProDos (kDiskBlocks));

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::Cffa);
        Assert::AreEqual (size_t (2), partitions.size());

        Assert::AreEqual (0u, partitions[0].firstBlock);
        Assert::AreEqual (kSliceBlocks, partitions[0].blockCount);
        Assert::IsTrue (partitions[0].volumeKind == VolumeKind::ProDos);

        Assert::AreEqual (2, partitions[1].index);
        Assert::AreEqual (kSliceBlocks, partitions[1].firstBlock);
        Assert::AreEqual (kDiskBlocks, partitions[1].blockCount);
        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::ProDos);
        Assert::IsFalse (partitions[1].damaged);
    }


    TEST_METHOD (Cffa_NoVolumeInAnySlice_HasNoPartitions)
    {
        MemoryBlockStore     store (vector<Byte> ((size_t) (kSliceBlocks + kDiskBlocks) * kBlock, 0));
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::Cffa;

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::None);
        Assert::IsTrue (partitions.empty());
    }


    //  The space past the first four slices is one large volume unless more
    //  volumes are found in it as slices.
    TEST_METHOD (Cffa_SpacePastFourSlices_IsOneLargeVolume)
    {
        vector<Byte>         bytes ((size_t) (4 * kSliceBlocks + kDiskBlocks) * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        Place (bytes, 0, MakeProDos (kDiskBlocks));
        Place (bytes, 4 * kSliceBlocks, MakeProDos (kDiskBlocks));

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::AreEqual (size_t (5), partitions.size());

        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::Unknown);
        Assert::AreEqual (5, partitions[4].index);
        Assert::AreEqual (4 * kSliceBlocks, partitions[4].firstBlock);
        Assert::AreEqual (kDiskBlocks, partitions[4].blockCount);
        Assert::IsTrue (partitions[4].volumeKind == VolumeKind::ProDos);
    }


    TEST_METHOD (UniDos_TwoVolumesOfWholeBlocks_AreFound)
    {
        MemoryBlockStore     store (MakeUniDosDisk());
        vector<Partition>    partitions;
        vector<Byte>         sectors;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::UniDos);
        Assert::AreEqual (size_t (2), partitions.size());
        Assert::AreEqual (800u, partitions[1].firstBlock);
        Assert::AreEqual (800u, partitions[1].blockCount);
        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::Dos33);
        Assert::IsTrue (partitions[1].dosSectors);

        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[1], sectors));

        Assert::IsTrue (MakeDos (50, 32) == sectors);
    }


    TEST_METHOD (AmDos_CatalogOnSectorFifteen_IsAmDos)
    {
        vector<Byte>         bytes    = MakeUniDosDisk();
        vector<Partition>    partitions;
        PartitionLayoutKind  kind     = PartitionLayoutKind::None;
        Dos33Geometry        geometry = Dos33Geometry::Make (50, 32);

        bytes[geometry.GetSectorOffset (Dos33Geometry::kVtocTrack, 0) + Dos33Geometry::kVtocOffCatalogSector] = 15;

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::AmDos);
        Assert::AreEqual (size_t (2), partitions.size());
        Assert::IsTrue (partitions[1].layout == PartitionLayoutKind::AmDos);
    }


    TEST_METHOD (UniDos_SecondHalfNotDos_HasNoPartitions)
    {
        vector<Byte>         bytes ((size_t) kDiskBlocks * kBlock, 0);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        Place (bytes, 0, MakeDos (50, 32));

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (partitions.empty());
    }


    //  Two volumes in the halves of each block. A write to the second lands
    //  in the second halves alone.
    TEST_METHOD (OzDos_TwoVolumesInHalfBlocks_AreFound_AndWrittenApart)
    {
        vector<Byte>         volume = MakeDos (50, 32);
        vector<Byte>         bytes ((size_t) kDiskBlocks * kBlock, 0);
        vector<Byte>         first;
        vector<Byte>         second;
        vector<Byte>         edited;
        vector<Partition>    partitions;
        PartitionLayoutKind  kind   = PartitionLayoutKind::None;
        size_t               block  = 0;

        for (block = 0; block < kDiskBlocks; block++)
        {
            std::copy_n (volume.begin() + (ptrdiff_t) (block * 256), 256, bytes.begin() + (ptrdiff_t) (block * kBlock));
            std::copy_n (volume.begin() + (ptrdiff_t) (block * 256), 256, bytes.begin() + (ptrdiff_t) (block * kBlock + 256));
        }

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::OzDos);
        Assert::AreEqual (size_t (2), partitions.size());
        Assert::IsTrue (partitions[0].span == PartitionSpan::FirstHalves);
        Assert::IsTrue (partitions[1].span == PartitionSpan::SecondHalves);

        edited = volume;

        for (Byte & value : edited)
        {
            value = (Byte) ~value;
        }

        AssertSucceeded (PartitionLayout::WritePartition (store, partitions[1], edited));
        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[0], first));
        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[1], second));

        Assert::IsTrue (volume == first);
        Assert::IsTrue (edited == second);
    }


    //  Two 140K DOS volumes at the end of an 800K ProDOS volume, in blocks
    //  the bitmap has in use and no file has.
    TEST_METHOD (DosMaster_TwoVolumesAtTheEndOfAProDosVolume_AreFound)
    {
        vector<Byte>         bytes  = MakeProDos (kDiskBlocks);
        vector<Byte>         volume = MakeDos (35, 16);
        vector<Byte>         sectors;
        vector<Partition>    partitions;
        PartitionLayoutKind  kind   = PartitionLayoutKind::None;
        uint32_t             block  = 0;

        for (block = 1040; block < kDiskBlocks; block++)
        {
            bytes[ProDosSkeleton::GetBitmapByteOffset (bytes, block)] &= (Byte) ~(0x80 >> (block & 7));
        }

        Place (bytes, 1040, volume);
        Place (bytes, 1320, volume);

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::DosMaster);
        Assert::AreEqual (size_t (2), partitions.size());
        Assert::AreEqual (1040u, partitions[0].firstBlock);
        Assert::AreEqual (1320u, partitions[1].firstBlock);
        Assert::AreEqual (280u, partitions[1].blockCount);
        Assert::IsTrue (partitions[1].volumeKind == VolumeKind::Dos33);

        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[1], sectors));

        Assert::IsTrue (volume == sectors);
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);
    }


    TEST_METHOD (DosMaster_RunWithoutDosVolumes_HasNoPartitions)
    {
        vector<Byte>         bytes = MakeProDos (kDiskBlocks);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind  = PartitionLayoutKind::None;
        uint32_t             block = 0;

        for (block = 1040; block < kDiskBlocks; block++)
        {
            bytes[ProDosSkeleton::GetBitmapByteOffset (bytes, block)] &= (Byte) ~(0x80 >> (block & 7));
        }

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (partitions.empty());
    }


    //  A PASCAL.AREA file of 1000 blocks at block 100 with a map of three
    //  volumes. The third ends past the ProDOS volume and is left out.
    TEST_METHOD (Ppm_AreaWithAMap_ListsTheVolumesInsideTheProDosVolume)
    {
        vector<Byte>         bytes = MakeProDos (kDiskBlocks);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind  = PartitionLayoutKind::None;
        size_t               entry = 2 * kBlock + 4 + 0x27;
        size_t               map   = 100 * kBlock;

        bytes[entry] = 0x4B;
        WriteText (bytes, entry + 1, "PASCAL.AREA");
        WriteWord (bytes, entry + 0x11, 100);
        WriteWord (bytes, entry + 0x13, 1000);

        WriteWord (bytes, map + 2, 3);
        bytes[map + 4] = 0x03;
        WriteText (bytes, map + 5, "PPM");

        WriteWord (bytes, map + 8, 102);
        WriteWord (bytes, map + 10, 280);
        WriteWord (bytes, map + 16, 382);
        WriteWord (bytes, map + 18, 400);
        WriteWord (bytes, map + 24, 1500);
        WriteWord (bytes, map + 26, 400);

        bytes[map + 0x110] = 3;
        WriteText (bytes, map + 0x111, "ONE");
        bytes[map + 0x120] = 3;
        WriteText (bytes, map + 0x121, "TWO");

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (kind == PartitionLayoutKind::Ppm);
        Assert::AreEqual (size_t (2), partitions.size());

        Assert::AreEqual (102u, partitions[0].firstBlock);
        Assert::AreEqual (280u, partitions[0].blockCount);
        Assert::AreEqual (std::string ("ONE"), partitions[0].name);

        Assert::AreEqual (2, partitions[1].index);
        Assert::AreEqual (382u, partitions[1].firstBlock);
        Assert::AreEqual (400u, partitions[1].blockCount);
        Assert::AreEqual (std::string ("TWO"), partitions[1].name);
    }


    TEST_METHOD (Ppm_AreaWithoutTheSignature_HasNoPartitions)
    {
        vector<Byte>         bytes = MakeProDos (kDiskBlocks);
        vector<Partition>    partitions;
        PartitionLayoutKind  kind  = PartitionLayoutKind::None;
        size_t               entry = 2 * kBlock + 4 + 0x27;

        bytes[entry] = 0x4B;
        WriteText (bytes, entry + 1, "PASCAL.AREA");
        WriteWord (bytes, entry + 0x11, 100);
        WriteWord (bytes, entry + 0x13, 1000);

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));

        Assert::IsTrue (partitions.empty());
    }


    TEST_METHOD (WritePartition_BufferOfAnotherSize_IsBadLength)
    {
        MemoryBlockStore     store (MakeUniDosDisk());
        vector<Partition>    partitions;
        vector<Byte>         sectors (1000, 0);
        PartitionLayoutKind  kind = PartitionLayoutKind::None;
        HRESULT              hr   = S_OK;

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));
        Assert::AreEqual (size_t (2), partitions.size());

        hr = PartitionLayout::WritePartition (store, partitions[0], sectors);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), hr);
    }


    //  A 140K ProDOS partition is read in DOS logical order and written back
    //  to its blocks in block order.
    TEST_METHOD (WritePartition_FloppySizedBlockVolume_ReadsBackAsWritten)
    {
        vector<Byte>         bytes ((size_t) 400 * kBlock, 0);
        vector<Byte>         floppy (143360, 0);
        vector<Byte>         floppyBlocks;
        vector<Byte>         back;
        vector<Partition>    partitions;
        PartitionLayoutKind  kind = PartitionLayoutKind::None;

        AssertSucceeded (ProDosSkeleton::Write (floppy, "FLOPPY"));
        VolumeImage::DosLogicalToProDosFile (floppy, floppyBlocks);

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 1;

        WriteLong (bytes, 0x20, 8);
        WriteLong (bytes, 0x40, 280);

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, kind, partitions));
        Assert::AreEqual (size_t (1), partitions.size());
        Assert::IsTrue (partitions[0].volumeKind == VolumeKind::Unknown);

        AssertSucceeded (PartitionLayout::WritePartition (store, partitions[0], floppy));
        AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[0], back));

        Assert::IsTrue (floppy == back);
        Assert::IsTrue (std::equal (floppyBlocks.begin(), floppyBlocks.end(), store.GetBytes().begin() + (ptrdiff_t) (8 * kBlock)));
    }


    TEST_METHOD (GetLayoutName_EveryLayout_HasAName)
    {
        Assert::AreEqual ("CFFA", PartitionLayout::GetLayoutName (PartitionLayoutKind::Cffa));
        Assert::AreEqual ("DOS MASTER", PartitionLayout::GetLayoutName (PartitionLayoutKind::DosMaster));
        Assert::AreEqual ("Pascal ProFile Manager", PartitionLayout::GetLayoutName (PartitionLayoutKind::Ppm));
    }
};