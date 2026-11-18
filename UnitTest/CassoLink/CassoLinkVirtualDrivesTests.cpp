#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/Host/CassoLinkVirtualDrives.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrivesTests
//
//  The CassoLink ProDOS disk's drives on the PC: blocks read and written in
//  place, and a write cut short at any step leaving the block whole or as
//  it was.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkVirtualDrivesTests)
{
public:

    static constexpr size_t  kImageSize = 280 * CassoLinkVirtualDrives::kBlockSize;

    //  The fake's files, with every change after `steps` of them failing,
    //  as they are after the PC goes down mid-write.
    struct CrashingFileIo : FakeDiskFileIo
    {
        int  steps = INT_MAX;

        bool  Step()
        {
            return steps-- > 0;
        }

        HRESULT  WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes) override
        {
            return Step() ? FakeDiskFileIo::WriteAllBytes (path, bytes) : E_FAIL;
        }

        HRESULT  WriteAt (const std::string & path, uint64_t offset, std::span<const Byte> bytes) override
        {
            return Step() ? FakeDiskFileIo::WriteAt (path, offset, bytes) : E_FAIL;
        }

        HRESULT  FlushToStorage (const std::string & path) override
        {
            return Step() ? FakeDiskFileIo::FlushToStorage (path) : E_FAIL;
        }

        HRESULT  Remove (const std::string & path) override
        {
            return Step() ? FakeDiskFileIo::Remove (path) : E_FAIL;
        }
    };


    static void  AddImage (FakeDiskFileIo & io, const std::string & path, size_t size, Byte fill)
    {
        io.files[path]  = std::vector<Byte> (size, fill);
        io.stamps[path] = FileStamp { size, 1 };
    }


    static std::vector<Byte>  MakeBlock (Byte seed)
    {
        std::vector<Byte>  block (CassoLinkVirtualDrives::kBlockSize);

        for (size_t at = 0; at < block.size(); at++)
        {
            block[at] = (Byte) (seed + at);
        }

        return block;
    }


    TEST_METHOD (ABlockWritten_IsInTheImageAtItsOffset_AndReadsBack)
    {
        FakeDiskFileIo          io;
        CassoLinkVirtualDrives  drives (io);
        std::vector<Byte>       block = MakeBlock (7);
        std::vector<Byte>       read (CassoLinkVirtualDrives::kBlockSize);
        uint32_t                blocks = 0;



        AddImage (io, "C:\\Disks\\One.po", kImageSize, 0);
        drives.SetImage (1, "C:\\Disks\\One.po");

        Assert::AreEqual (CassoLinkVirtualDrives::kNoError, drives.GetBlocks (1, blocks));
        Assert::AreEqual (280u, blocks);
        Assert::AreEqual (CassoLinkVirtualDrives::kNoError, drives.WriteBlock (1, 5, block));
        Assert::IsTrue (std::equal (block.begin(), block.end(), io.files["C:\\Disks\\One.po"].begin() + 5 * 512), L"the block is at its offset");
        Assert::IsFalse (io.Exists (CassoLinkVirtualDrives::GetJournalPath ("C:\\Disks\\One.po")), L"the journal is removed after the write");
        Assert::AreEqual (CassoLinkVirtualDrives::kNoError, drives.ReadBlock (1, 5, read));
        Assert::IsTrue (read == block);
    }


    //  A 140K .dsk is in DOS 3.3 order: block 1's halves are sectors 13 and
    //  12 of track 0.
    TEST_METHOD (ADosOrderImage_HasABlocksHalvesWhereDosPutsThem)
    {
        FakeDiskFileIo          io;
        CassoLinkVirtualDrives  drives (io);
        std::vector<Byte>       block = MakeBlock (3);
        std::vector<Byte> &     file  = io.files["C:\\Disks\\Two.dsk"];



        AddImage (io, "C:\\Disks\\Two.dsk", kImageSize, 0);
        drives.SetImage (2, "C:\\Disks\\Two.dsk");

        Assert::AreEqual (CassoLinkVirtualDrives::kNoError, drives.WriteBlock (2, 1, block));
        Assert::IsTrue (std::equal (block.begin(), block.begin() + 256, file.begin() + 13 * 256), L"the first half is sector 13");
        Assert::IsTrue (std::equal (block.begin() + 256, block.end(), file.begin() + 12 * 256), L"the second half is sector 12");
    }


    TEST_METHOD (ADriveWithNoImage_OrABlockPastItsEnd_IsNoDevice)
    {
        FakeDiskFileIo          io;
        CassoLinkVirtualDrives  drives (io);
        std::vector<Byte>       read (CassoLinkVirtualDrives::kBlockSize);



        AddImage (io, "C:\\Disks\\One.po", kImageSize, 0);
        drives.SetImage (1, "C:\\Disks\\One.po");

        Assert::AreEqual (CassoLinkVirtualDrives::kNoDevice, drives.ReadBlock (2, 0, read));
        Assert::AreEqual (CassoLinkVirtualDrives::kNoDevice, drives.ReadBlock (1, 280, read));
        Assert::AreEqual (CassoLinkVirtualDrives::kNoDevice, drives.ReadBlock (3, 0, read));
    }


    //  The PC goes down after each step of a write in turn; with the image
    //  opened again, the block is the old one or the new one, never a mix.
    TEST_METHOD (AWriteCutShortAtAnyStep_LeavesTheBlockWholeOrAsItWas)
    {
        std::vector<Byte>  block = MakeBlock (9);
        std::vector<Byte>  old (CassoLinkVirtualDrives::kBlockSize, 0xEE);



        for (int steps = 0; steps <= 5; steps++)
        {
            CrashingFileIo          io;
            CassoLinkVirtualDrives  before (io);
            Byte                    error = 0;
            std::vector<Byte>       read (CassoLinkVirtualDrives::kBlockSize);

            AddImage (io, "C:\\Disks\\One.po", kImageSize, 0xEE);
            before.SetImage (1, "C:\\Disks\\One.po");

            io.steps = steps;
            error    = before.WriteBlock (1, 9, block);
            io.steps = INT_MAX;

            {
                CassoLinkVirtualDrives  after (io);

                after.SetImage (1, "C:\\Disks\\One.po");
                Assert::AreEqual (CassoLinkVirtualDrives::kNoError, after.ReadBlock (1, 9, read));
            }

            Assert::IsTrue (read == block || read == old, std::format (L"after {} steps the block is whole or as it was", steps).c_str());
            Assert::IsTrue (error != CassoLinkVirtualDrives::kNoError || read == block, std::format (L"after {} steps a write that succeeded is in the image", steps).c_str());
            Assert::IsFalse (io.Exists (CassoLinkVirtualDrives::GetJournalPath ("C:\\Disks\\One.po")), L"the journal is applied or removed");
        }
    }


    //  A journal half written, as from a write cut short before it was
    //  flushed, is removed without touching the image.
    TEST_METHOD (ADamagedJournal_IsRemoved_AndTheImageIsAsItWas)
    {
        FakeDiskFileIo          io;
        std::vector<Byte>       block = MakeBlock (1);
        std::vector<Byte>       read (CassoLinkVirtualDrives::kBlockSize);
        std::vector<Byte>       old (CassoLinkVirtualDrives::kBlockSize, 0x11);



        AddImage (io, "C:\\Disks\\One.po", kImageSize, 0x11);

        {
            CassoLinkVirtualDrives  before (io);

            before.SetImage (1, "C:\\Disks\\One.po");
            io.failNextWrite = true;
            Assert::AreEqual (CassoLinkVirtualDrives::kIoError, before.WriteBlock (1, 2, block));
        }

        {
            CassoLinkVirtualDrives  after (io);

            after.SetImage (1, "C:\\Disks\\One.po");
            Assert::AreEqual (CassoLinkVirtualDrives::kNoError, after.ReadBlock (1, 2, read));
        }

        Assert::IsTrue (read == old);
        Assert::IsFalse (io.Exists (CassoLinkVirtualDrives::GetJournalPath ("C:\\Disks\\One.po")));
    }
};
