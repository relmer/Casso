#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/FileBlockSource.h"
#include "Devices/BlockDevice/MemoryBlockSource.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSourceTests
//
//  The two sources of a block device's blocks: an image file, whose writes
//  are committed by Flush and put back by a state load, and blocks in
//  memory, which a write doesn't change.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockSourceTests)
{
public:

    static constexpr uint32_t  kBlocks = 280;
    static constexpr size_t    kBlock  = BlockDeviceMedia::kBlockBytes;
    static constexpr auto      kPath   = "disk.po";


    //  Each block's bytes are its number, so the first byte of a read is the
    //  block's number.
    static std::vector<Byte> MakeBlocks()
    {
        std::vector<Byte>  bytes ((size_t) kBlocks * kBlock, 0);



        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (i / kBlock);
        }

        return bytes;
    }


    static std::unique_ptr<FileBlockSource> OpenFile (FakeDiskFileIo & io)
    {
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        io.files[kPath]  = MakeBlocks();
        io.stamps[kPath] = FileStamp { io.files[kPath].size(), 1 };

        Assert::AreEqual (S_OK, BlockImageFile::Open (io, kPath, file, error));
        return std::make_unique<FileBlockSource> (std::move (file));
    }


    static Byte ReadFirstByte (IBlockSource & source, uint32_t block)
    {
        std::vector<Byte>  bytes (kBlock, 0);



        Assert::AreEqual (S_OK, source.ReadBlock (block, bytes));
        return bytes[0];
    }



    //  A write changes the file only at Flush, and the blocks as attached
    //  come back with the written block's first content.
    TEST_METHOD (File_WriteThenFlush)
    {
        FakeDiskFileIo                            io;
        std::unique_ptr<FileBlockSource>          source  = OpenFile (io);
        std::vector<Byte>                         written (kBlock, 0xAA);
        std::shared_ptr<const std::vector<Byte>>  attached;



        Assert::AreEqual (S_OK, source->WriteBlock (5, written));
        Assert::IsTrue   (source->HasPendingWrites());
        Assert::AreEqual ((Byte) 0xAA, ReadFirstByte (*source, 5));
        Assert::AreEqual ((Byte) 5,    io.files[kPath][5 * kBlock], L"the file waits for the flush");

        Assert::AreEqual (S_OK, source->GetAttachedBlocks (attached));
        Assert::AreEqual ((Byte) 5, (*attached)[5 * kBlock], L"the attached blocks have the first content");

        Assert::AreEqual (S_OK, source->Flush());
        Assert::IsFalse  (source->HasPendingWrites());
        Assert::AreEqual ((Byte) 0xAA, io.files[kPath][5 * kBlock]);
    }


    //  A load puts back a block written since the save, and writes the
    //  blocks the save had written.
    TEST_METHOD (File_RestoreWritten)
    {
        FakeDiskFileIo                    io;
        std::unique_ptr<FileBlockSource>  source = OpenFile (io);
        IBlockSource::WrittenBlocks       saved;



        Assert::AreEqual (S_OK, source->WriteBlock (5, std::vector<Byte> (kBlock, 0xAA)));
        saved[9] = std::make_shared<const std::vector<Byte>> (kBlock, 0xBB);

        Assert::AreEqual (S_OK, source->RestoreWritten (saved));
        Assert::AreEqual ((Byte) 5,    ReadFirstByte (*source, 5), L"written after the save");
        Assert::AreEqual ((Byte) 0xBB, ReadFirstByte (*source, 9), L"written in the save");
        Assert::IsTrue   (source->HasPendingWrites());
    }


    //  A file changed on the host since it was attached fails the flush.
    TEST_METHOD (File_ChangedOnHost_FailsFlush)
    {
        FakeDiskFileIo                    io;
        std::unique_ptr<FileBlockSource>  source = OpenFile (io);



        Assert::AreEqual (S_OK, source->WriteBlock (1, std::vector<Byte> (kBlock, 0xAA)));
        io.stamps[kPath].modifiedUnix = 99;

        Assert::IsTrue   (source->HasChangedOnHost());
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_INVALID), source->Flush());
    }


    //  The blocks are shared, and a write doesn't change them.
    TEST_METHOD (Memory_SharedAndUnchangedByWrites)
    {
        auto                                      blocks = std::make_shared<const std::vector<Byte>> (MakeBlocks());
        MemoryBlockSource                         source (blocks);
        std::shared_ptr<const std::vector<Byte>>  attached;



        Assert::AreEqual (S_OK, source.WriteBlock (3, std::vector<Byte> (kBlock, 0xAA)));
        Assert::AreEqual ((Byte) 3, ReadFirstByte (source, 3));
        Assert::IsFalse  (source.HasPendingWrites());
        Assert::AreEqual (S_OK, source.Flush());

        Assert::AreEqual (S_OK, source.GetAttachedBlocks (attached));
        Assert::IsTrue   (attached == blocks);
    }
};
