#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/FileBlockStore.h"
#include "Devices/Disk/MemoryBlockStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockStoreTests
//
//  The paged file store over the fake file system, and the memory store.
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
};