#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Containers/ImageContainer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LargeVolumeTests
//
//  ProDOS volumes larger than a floppy, in block order, and the 2IMG and Disk
//  Copy 4.2 containers, over buffers built in the test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LargeVolumeTests)
{
public:

    static constexpr size_t  kBlockBytes   = 512;
    static constexpr size_t  k800KBlocks   = 1600;
    static constexpr size_t  k32MBBlocks   = 65536;
    static constexpr size_t  kFileBytes    = 70'000;
    static constexpr Byte    kFill         = 0x5A;

    static vector<Byte> MakeVolume (size_t blocks)
    {
        vector<Byte>  buffer (blocks * kBlockBytes, 0xEE);

        AssertSucceeded (ProDosSkeleton::Write (buffer, "BIG"));

        return buffer;
    }

    static FilePayload MakePayload (size_t bytes = kFileBytes)
    {
        FilePayload  payload;



        payload.bytes.assign (bytes, kFill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }


    TEST_METHOD (Write_FormatsAn800KVolume)
    {
        vector<Byte>   buffer = MakeVolume (k800KBlocks);
        ProDosVolume   volume (buffer);
        VolumeListing  listing;



        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (std::string ("BIG"), listing.volumeName);
        Assert::AreEqual ((uint32_t) k800KBlocks, listing.totalUnits);
        Assert::AreEqual ((uint32_t) (k800KBlocks - 7), listing.freeUnits);
        Assert::IsTrue (VolumeImage::DetectFilesystem (buffer) == VolumeKind::ProDos);
    }


    TEST_METHOD (Write_FormatsA32MBVolumeWithABitmapOfSixteenBlocks)
    {
        constexpr uint32_t  kBitmapBlocks = 16;

        vector<Byte>   buffer = MakeVolume (k32MBBlocks);
        ProDosVolume   volume (buffer);
        VolumeListing  listing;



        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual ((uint32_t) ProDosSkeleton::kMaxBlocks, listing.totalUnits);
        Assert::AreEqual ((uint32_t) ProDosSkeleton::kMaxBlocks - 6 - kBitmapBlocks, listing.freeUnits);
    }


    TEST_METHOD (Write_ReadsBackAFileOnAn800KVolume)
    {
        vector<Byte>           buffer = MakeVolume (k800KBlocks);
        ProDosVolume           volume (buffer);
        vector<Byte>           written;
        FilePayload            read;
        VolumeIntegrityReport  report;



        AssertSucceeded (volume.Write (FilePath::Parse ("DATA"), MakePayload(), written));

        ProDosVolume  after (written);

        AssertSucceeded (after.Read (FilePath::Parse ("DATA"), read));
        Assert::AreEqual (kFileBytes, read.bytes.size());
        Assert::IsTrue (std::all_of (read.bytes.begin(), read.bytes.end(), [] (Byte b) { return b == kFill; }));

        AssertSucceeded (after.BuildIntegrityReport (report));
        Assert::IsTrue (report.IsClean());
    }


    TEST_METHOD (Write_AllocatesPastTheFirstBitmapBlock)
    {
        constexpr size_t  kFiles     = 10;
        constexpr size_t  kLargeFile = 300'000;

        vector<Byte>   buffer = MakeVolume (10'000);
        VolumeListing  listing;
        size_t         i      = 0;



        for (i = 0; i < kFiles; i++)
        {
            ProDosVolume  volume (buffer);
            vector<Byte>  written;

            AssertSucceeded (volume.Write (FilePath::Parse (std::format ("F{}", i)), MakePayload (kLargeFile), written));
            buffer = std::move (written);
        }

        ProDosVolume  after (buffer);

        AssertSucceeded (after.Enumerate (listing));
        Assert::AreEqual (kFiles, listing.entries.size());
        Assert::IsTrue (listing.freeUnits < 10'000u - 4096u);
    }


    TEST_METHOD (BlockOffset_IsLinearOutsideTheFloppySize)
    {
        Assert::AreEqual ((size_t) (5 * kBlockBytes + 3), ProDosSkeleton::GetBlockByteOffset (k800KBlocks * kBlockBytes, 5, 3));
        Assert::AreEqual (ProDosSkeleton::GetBlockByteOffset (5, 3),
                          ProDosSkeleton::GetBlockByteOffset (NibblizationLayer::kImageByteSize, 5, 3));
    }


    TEST_METHOD (TwoImg_RoundTripsItsHeader)
    {
        TwoImgHeader  header;
        TwoImgHeader  parsed;
        vector<Byte>  payload (k800KBlocks * kBlockBytes, kFill);
        vector<Byte>  file;
        vector<Byte>  readBack;
        std::string   error;



        header.comment     = "A comment";
        header.creatorData = { 1, 2, 3 };
        header.flags       = TwoImgHeader::kFlagLocked;

        TwoImgContainer::Build (header, payload, file);

        AssertSucceeded (TwoImgContainer::Parse (file, parsed, readBack, error));
        Assert::IsTrue (readBack == payload);
        Assert::AreEqual (std::string ("A comment"), parsed.comment);
        Assert::IsTrue (parsed.creatorData == header.creatorData);
        Assert::AreEqual (TwoImgHeader::kFlagLocked, parsed.flags);
        Assert::AreEqual ((uint32_t) k800KBlocks, parsed.blockCount);
    }


    TEST_METHOD (TwoImg_RefusesDataPastTheEnd)
    {
        TwoImgHeader  header;
        vector<Byte>  payload (kBlockBytes * 8, 0);
        vector<Byte>  file;
        vector<Byte>  readBack;
        std::string   error;



        TwoImgContainer::Build (header, payload, file);
        file.resize (file.size() - 1);

        AssertFailed (TwoImgContainer::Parse (file, header, readBack, error));
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (DiskCopy42_ChecksumsMatchAndRoundTrip)
    {
        DiskCopy42Header  header;
        DiskCopy42Header  parsed;
        vector<Byte>      data (k800KBlocks * kBlockBytes, 0);
        vector<Byte>      file;
        vector<Byte>      readBack;
        std::string       error;



        data[0]      = 0x12;
        data[1]      = 0x34;
        header.name  = "-not a Macintosh disk-";
        header.tags.assign (k800KBlocks * 12, 0x01);

        DiskCopy42Container::Build (header, data, file);

        Assert::IsTrue (DiskCopy42Container::IsDiskCopy42 (file));
        AssertSucceeded (DiskCopy42Container::Parse (file, parsed, readBack, error));
        Assert::IsFalse (parsed.isDataChecksumBad);
        Assert::IsTrue (readBack == data);
        Assert::AreEqual (header.name, parsed.name);
        Assert::IsTrue (parsed.tags == header.tags);
    }


    TEST_METHOD (DiskCopy42_ChecksumRotatesAfterEachWord)
    {
        vector<Byte>  bytes = { 0x00, 0x01, 0x00, 0x01 };

        //  1, rotated right to $80000000; plus 1, rotated to $C0000000.
        Assert::AreEqual ((uint32_t) 0xC0000000, DiskCopy42Container::ComputeChecksum (bytes));
    }


    TEST_METHOD (DiskCopy42_ABadChecksumIsReported)
    {
        DiskCopy42Header  header;
        vector<Byte>      data (k800KBlocks * kBlockBytes, 0);
        vector<Byte>      file;
        vector<Byte>      readBack;
        vector<Byte>      saved;
        std::string       error;



        DiskCopy42Container::Build (header, data, file);
        file[DiskCopy42Container::kHeaderSize + 100] ^= 0xFF;

        AssertSucceeded (DiskCopy42Container::Parse (file, header, readBack, error));
        Assert::IsTrue (header.isDataChecksumBad);

        AssertFailed (VolumeImage::Save (file, "disk.dc42", readBack, saved, error));
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (ContainerKind_FollowsExtensionAndSize)
    {
        Assert::IsTrue (ImageContainer::GetKind ("a.hdv", 1) == ImageContainerKind::Blocks);
        Assert::IsTrue (ImageContainer::GetKind ("a.PO", k800KBlocks * kBlockBytes) == ImageContainerKind::Blocks);
        Assert::IsTrue (ImageContainer::GetKind ("a.po", NibblizationLayer::kImageByteSize) == ImageContainerKind::None);
        Assert::IsTrue (ImageContainer::GetKind ("a.2mg", 1) == ImageContainerKind::TwoImg);
        Assert::IsTrue (ImageContainer::GetKind ("a.dc42", 1) == ImageContainerKind::DiskCopy42);
        Assert::IsTrue (ImageContainer::GetKind ("a.woz", 1) == ImageContainerKind::None);
    }


    TEST_METHOD (Load_And_Save_KeepAHardDiskImageInBlockOrder)
    {
        vector<Byte>        file = MakeVolume (k800KBlocks);
        vector<Byte>        sectors;
        vector<Byte>        saved;
        SectorDecodeReport  report;
        std::string         reason;



        AssertSucceeded (VolumeImage::Load (file, "disk.hdv", sectors, report));
        Assert::IsTrue (sectors == file);

        AssertSucceeded (VolumeImage::Save (file, "disk.hdv", sectors, saved, reason));
        Assert::IsTrue (saved == file);
    }


    TEST_METHOD (Load_And_Save_KeepATwoImgHeader)
    {
        vector<Byte>        volume = MakeVolume (k800KBlocks);
        TwoImgHeader        header;
        TwoImgHeader        parsed;
        vector<Byte>        file;
        vector<Byte>        sectors;
        vector<Byte>        saved;
        vector<Byte>        payload;
        SectorDecodeReport  report;
        std::string         reason;



        header.comment = "kept";
        TwoImgContainer::Build (header, volume, file);

        AssertSucceeded (VolumeImage::Load (file, "disk.2mg", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);

        AssertSucceeded (VolumeImage::Save (file, "disk.2mg", sectors, saved, reason));
        AssertSucceeded (TwoImgContainer::Parse (saved, parsed, payload, reason));
        Assert::AreEqual (std::string ("kept"), parsed.comment);
        Assert::IsTrue (payload == volume);
    }


    TEST_METHOD (Load_ReadsA140KTwoImgInProDosOrder)
    {
        vector<Byte>        volume (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>        blocks;
        TwoImgHeader        header;
        vector<Byte>        file;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        AssertSucceeded (ProDosSkeleton::Write (volume, "SMALL"));
        VolumeImage::DosLogicalToProDosFile (volume, blocks);
        TwoImgContainer::Build (header, blocks, file);

        AssertSucceeded (VolumeImage::Load (file, "disk.2mg", sectors, report));
        Assert::IsTrue (sectors == volume);
    }
};
