#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CassoLinkBlockTransfer.h"
#include "CassoLink/Session/CassoLinkResume.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransferTests
//
//  A whole block device read and written over a fake link: a transfer the
//  link drops at block 1,000 of 65,535 saves its record, and the next one
//  continues after the record's last block; a device error is in words with
//  its block; and a .2mg image is wrapped in its header.
//
////////////////////////////////////////////////////////////////////////////////

namespace CassoLinkBlockTransferTests
{
    static constexpr uint32_t  kBlocks = 65535;
    static constexpr size_t    kBlock  = 512;
    static constexpr uint32_t  kDropAt = 1000;


    //  A device's blocks in memory. A request that runs past dropAt fails as
    //  a dropped link does; a block at failAt has failError in the Status.
    struct FakeBlocks : public ICassoLinkBlockIo
    {
        std::vector<Byte>  disk;
        uint32_t           dropAt     = UINT32_MAX;
        uint32_t           failAt     = UINT32_MAX;
        Byte               failError  = 0;
        uint64_t           blocksRead = 0;
        uint64_t           blocksSent = 0;

        HRESULT  ReadBlocks (const CassoLinkBlockDevice & device, uint32_t first, Byte count, bool verify, std::vector<Byte> & outData, CassoLinkStatus & outStatus) override
        {
            UNREFERENCED_PARAMETER (device);
            UNREFERENCED_PARAMETER (verify);

            outData.clear();
            outStatus = {};

            if (first + count > dropAt)
            {
                return HRESULT_FROM_WIN32 (ERROR_CONNECTION_ABORTED);
            }

            for (uint32_t block = first; block < first + count; block++)
            {
                if (block == failAt)
                {
                    outStatus.error = failError;
                    break;
                }

                outData.insert (outData.end(), disk.begin() + (ptrdiff_t) (block * kBlock), disk.begin() + (ptrdiff_t) ((block + 1) * kBlock));
                outStatus.done++;
                blocksRead++;
            }

            return S_OK;
        }

        HRESULT  WriteBlocks (const CassoLinkBlockDevice & device, uint32_t first, std::span<const Byte> data, bool verify, CassoLinkStatus & outStatus) override
        {
            UNREFERENCED_PARAMETER (device);
            UNREFERENCED_PARAMETER (verify);

            outStatus = {};

            if (first + data.size() / kBlock > dropAt)
            {
                return HRESULT_FROM_WIN32 (ERROR_CONNECTION_ABORTED);
            }

            std::copy (data.begin(), data.end(), disk.begin() + (ptrdiff_t) (first * kBlock));
            outStatus.done  = (Byte) (data.size() / kBlock);
            blocksSent     += data.size() / kBlock;

            return S_OK;
        }
    };


    static CassoLinkBlockDevice  MakeDevice()
    {
        CassoLinkBlockDevice  device;



        device.slot        = 7;
        device.unit        = 1;
        device.isSmartPort = true;
        device.blocks      = kBlocks;
        device.kind        = 2;
        device.volume      = "BIG";

        return device;
    }


    static std::vector<Byte>  MakeDisk()
    {
        std::vector<Byte>  disk ((size_t) kBlocks * kBlock);



        for (size_t at = 0; at < disk.size(); at++)
        {
            disk[at] = (Byte) (at * 13 + at / kBlock);
        }

        return disk;
    }


    TEST_CLASS (CassoLinkBlockTransferTests)
    {
    public:

        TEST_METHOD (AResumeRecord_IsWrittenAndReadBack_AndMatchesItsDevice)
        {
            CassoLinkResumeRecord  record;
            CassoLinkResumeRecord  read;
            CassoLinkBlockDevice   device = MakeDevice();



            record.slot              = device.slot;
            record.unit              = device.unit;
            record.blockCount        = device.blocks;
            record.volumeName        = device.volume;
            record.directoryHash     = std::string (64, 'c');
            record.direction         = CassoLinkResumeRecord::kRead;
            record.lastVerifiedBlock = 991;
            record.imagePath         = "C:\\Images\\Big.po";

            AssertSucceeded (CassoLinkResume::Parse (CassoLinkResume::Format (record), read));

            Assert::AreEqual ((int64_t) 991, read.lastVerifiedBlock);
            Assert::AreEqual (record.imagePath, read.imagePath);
            Assert::IsTrue   (CassoLinkResume::Matches (read, device, record.directoryHash, CassoLinkResumeRecord::kRead));
            Assert::IsFalse  (CassoLinkResume::Matches (read, device, record.directoryHash, CassoLinkResumeRecord::kWrite), L"Another direction");
            Assert::IsFalse  (CassoLinkResume::Matches (read, device, std::string (64, 'd'), CassoLinkResumeRecord::kRead), L"Another directory");

            device.volume = "OTHER";
            Assert::IsFalse  (CassoLinkResume::Matches (read, device, record.directoryHash, CassoLinkResumeRecord::kRead), L"Another volume");

            HRESULT  hr = CassoLinkResume::Parse ("{ \"slot\": 7 }", read);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"Not a whole record");
        }


        TEST_METHOD (AReadTheLinkDrops_ContinuesAfterItsLastBlock)
        {
            FakeDiskFileIo                  io;
            FakeBlocks                      blocks;
            CassoLinkBlockDevice            device = MakeDevice();
            CassoLinkBlockTransfer::Result  result;
            std::string                     image  = "C:\\Images\\Big.po";
            uint32_t                        next   = 0;
            uint64_t                        before = 0;



            blocks.disk   = MakeDisk();
            blocks.dropAt = kDropAt;

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CONNECTION_ABORTED),
                              CassoLinkBlockTransfer::ReadDevice (blocks, io, device, image, false, true, nullptr, nullptr, result));
            Assert::IsTrue (io.files.contains (CassoLinkResume::GetPartialPath (image)), L"The partial image");
            Assert::IsTrue (io.files.contains (CassoLinkResume::GetRecordPath (image)),  L"And its record");
            Assert::IsFalse (io.files.contains (image));

            blocks.dropAt = UINT32_MAX;

            Assert::IsTrue   (CassoLinkBlockTransfer::FindResume (blocks, io, device, image, true, next), L"The offer to continue");
            Assert::IsTrue   (next > kDropAt - CassoLinkResume::kEveryBlocks && next <= kDropAt, std::format (L"From block {}", next).c_str());

            before = blocks.blocksRead;
            AssertSucceeded (CassoLinkBlockTransfer::ReadDevice (blocks, io, device, image, false, true, nullptr, nullptr, result));

            Assert::AreEqual (next, result.firstBlock);
            Assert::AreEqual ((uint64_t) (kBlocks - next + CassoLinkResume::kDirectoryCount), blocks.blocksRead - before, L"Only the blocks after it, and the directory's");
            Assert::IsTrue   (io.files[image] == blocks.disk, L"The image is the device's");
            Assert::IsFalse  (io.files.contains (CassoLinkResume::GetPartialPath (image)));
            Assert::IsFalse  (io.files.contains (CassoLinkResume::GetRecordPath (image)));
        }


        TEST_METHOD (AWriteTheLinkDrops_ContinuesAfterItsLastBlock)
        {
            FakeDiskFileIo                  io;
            FakeBlocks                      blocks;
            CassoLinkBlockDevice            device = MakeDevice();
            CassoLinkBlockTransfer::Result  result;
            std::string                     image  = "C:\\Images\\Big.po";
            uint32_t                        next   = 0;



            io.files[image] = MakeDisk();
            blocks.disk.assign ((size_t) kBlocks * kBlock, 0);
            blocks.dropAt   = kDropAt;

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CONNECTION_ABORTED),
                              CassoLinkBlockTransfer::WriteDevice (blocks, io, device, image, true, true, nullptr, nullptr, result));

            blocks.dropAt = UINT32_MAX;

            Assert::IsTrue  (CassoLinkBlockTransfer::FindResume (blocks, io, device, image, false, next));
            AssertSucceeded (CassoLinkBlockTransfer::WriteDevice (blocks, io, device, image, true, true, nullptr, nullptr, result));

            Assert::AreEqual (next, result.firstBlock);
            Assert::AreEqual ((uint64_t) kBlocks, blocks.blocksSent, L"Every block written once across both");
            Assert::IsTrue   (blocks.disk == io.files[image], L"The device has the image");
            Assert::IsFalse  (io.files.contains (CassoLinkResume::GetRecordPath (image)));
        }


        TEST_METHOD (ADeviceError_IsInWordsWithItsBlock)
        {
            FakeDiskFileIo                  io;
            FakeBlocks                      blocks;
            CassoLinkBlockDevice            device = MakeDevice();
            CassoLinkBlockTransfer::Result  result;



            blocks.disk      = MakeDisk();
            blocks.failAt    = 50;
            blocks.failError = CassoLinkHost::kBlockIoError;

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_READ_FAULT),
                              CassoLinkBlockTransfer::ReadDevice (blocks, io, device, "C:\\Images\\Big.po", false, false, nullptr, nullptr, result));
            Assert::AreEqual ((uint32_t) 50, result.failedBlock);
            Assert::AreEqual (std::string ("I/O error at block 50."), result.message);
        }


        TEST_METHOD (A2mgImage_IsWrappedInItsHeader)
        {
            static constexpr uint32_t  kSmall = 1600;

            FakeDiskFileIo                  io;
            FakeBlocks                      blocks;
            CassoLinkBlockDevice            device = MakeDevice();
            CassoLinkBlockTransfer::Result  result;
            TwoImgHeader                    header;
            std::vector<Byte>               payload;
            std::string                     error;



            device.blocks = kSmall;
            blocks.disk   = MakeDisk();
            blocks.disk.resize ((size_t) kSmall * kBlock);

            AssertSucceeded (CassoLinkBlockTransfer::ReadDevice (blocks, io, device, "C:\\Images\\Disk.2mg", false, false, nullptr, nullptr, result));
            AssertSucceeded (TwoImgContainer::Parse (io.files["C:\\Images\\Disk.2mg"], header, payload, error));

            Assert::AreEqual (kSmall, header.blockCount);
            Assert::IsTrue   (payload == blocks.disk);
        }
    };
}
