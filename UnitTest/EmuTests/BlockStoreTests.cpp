#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/DosOrderBlockStore.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/FileBlockStore.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockStoreTests
//
//  The paged file store over the fake file system, the memory store, and
//  the DOS-order view of a 140K block device.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockStoreTests)
{
public:

    static constexpr size_t    kBlockBytes = IBlockStore::kBlockBytes;
    static constexpr uint32_t  kBlocks     = 300;
    static constexpr uint64_t  kOffset     = 64;

    static vector<Byte> MakeFile()
    {
        vector<Byte>  bytes (kOffset + kBlocks * kBlockBytes, 0);
        size_t        i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (i * 7);
        }

        return bytes;
    }


    TEST_METHOD (File_ReadsBlocksAtTheOffset)
    {
        FakeDiskFileIo  io;
        vector<Byte>    file = MakeFile();
        vector<Byte>    read (2 * kBlockBytes, 0);



        io.files["d.hdv"] = file;

        FileBlockStore  store (io, "d.hdv", kOffset, kBlocks);

        AssertSucceeded (store.ReadBlocks (129, 2, read));
        Assert::IsTrue (std::equal (read.begin(), read.end(), file.begin() + (ptrdiff_t) (kOffset + 129 * kBlockBytes)));
    }


    TEST_METHOD (File_WritesOnlyOnCommit)
    {
        FakeDiskFileIo  io;
        vector<Byte>    block (kBlockBytes, 0xAB);
        vector<Byte>    read (kBlockBytes, 0);



        io.files["d.hdv"] = MakeFile();

        FileBlockStore  store (io, "d.hdv", kOffset, kBlocks);

        AssertSucceeded (store.WriteBlocks (299, block));
        Assert::AreNotEqual ((Byte) 0xAB, io.files["d.hdv"][kOffset + 299 * kBlockBytes]);
        Assert::AreEqual ((size_t) 1, store.GetDirtyPageCount());

        AssertSucceeded (store.ReadBlocks (299, 1, read));
        Assert::IsTrue (read == block);

        AssertSucceeded (store.Commit());
        Assert::AreEqual ((Byte) 0xAB, io.files["d.hdv"][kOffset + 299 * kBlockBytes]);
        Assert::AreEqual ((size_t) 0, store.GetDirtyPageCount());
    }


    TEST_METHOD (File_RefusesARangePastTheEnd)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        FakeDiskFileIo  io;
        vector<Byte>    read (2 * kBlockBytes, 0);



        io.files["d.hdv"] = MakeFile();

        FileBlockStore  store (io, "d.hdv", kOffset, kBlocks);

        AssertFailed (store.ReadBlocks (kBlocks - 1, 2, read));
        expect.RequireCount (1);
    }


    TEST_METHOD (Memory_RoundTripsABlock)
    {
        MemoryBlockStore  store (vector<Byte> (4 * kBlockBytes, 0));
        vector<Byte>      block (kBlockBytes, 0x11);
        vector<Byte>      read (kBlockBytes, 0);



        AssertSucceeded (store.WriteBlocks (3, block));
        AssertSucceeded (store.ReadBlocks (3, 1, read));
        Assert::IsTrue (read == block);
        Assert::AreEqual ((uint32_t) 4, store.GetBlockCount());
    }


    //  A 140K device's blocks, in ProDOS order, read through the DOS-order
    //  view are the volume as the volume layer has it: a ProDOS volume with
    //  its file listed. A write through the view changes the halves of the
    //  device's blocks it covers.
    TEST_METHOD (DosOrder_ReadsAndWritesA140KDeviceAsTheVolumeLayerDoes)
    {
        static constexpr size_t  kBlock  = IBlockStore::kBlockBytes;
        std::vector<Byte>        dos (ProDosSkeleton::kTotalBlocks * kBlock, 0);
        std::vector<Byte>        next;
        std::vector<Byte>        blocks (dos.size(), 0);
        std::vector<Byte>        read (dos.size(), 0);
        std::vector<Byte>        slice (kBlock, 0xA5);
        FilePayload              payload;
        VolumeListing            listing;



        AssertSucceeded (ProDosSkeleton::Write (dos, "SEVENTY"));

        payload.bytes.assign (1200, 0x3C);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (dos).Write (FilePath::Parse ("HELLO"), payload, next));
        dos = std::move (next);

        for (int block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            for (size_t at = 0; at < kBlock; at++)
            {
                blocks[(size_t) block * kBlock + at] = dos[ProDosSkeleton::GetBlockByteOffset (block, at)];
            }
        }

        MemoryBlockStore    device (blocks);
        DosOrderBlockStore  view (device);
        BlockImage          image (view);

        AssertSucceeded (view.ReadBlocks (0, ProDosSkeleton::kTotalBlocks, read));
        Assert::IsTrue (read == dos, L"the view is the volume in DOS order");

        AssertSucceeded (ProDosVolume (image).Enumerate (listing));
        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::AreEqual (std::string ("HELLO"), listing.entries[0].name);

        AssertSucceeded (view.WriteBlocks (5, slice));
        std::fill_n (dos.begin() + 5 * kBlock, kBlock, (Byte) 0xA5);

        AssertSucceeded (view.ReadBlocks (0, ProDosSkeleton::kTotalBlocks, read));
        Assert::IsTrue (read == dos, L"the write changed that slice alone");
        Assert::IsTrue (device.GetBytes() != blocks, L"and the device's blocks");
    }
};