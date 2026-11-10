#include "Pch.h"

#include "../EhmTestHelper.h"
#include "PartitionCard.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Seams/InProcessByteStream.h"
#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Win32RawDeviceHelper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeRawDevice
//
//  One drive in memory, with the calls made on it counted.
//
////////////////////////////////////////////////////////////////////////////////

class FakeRawDevice : public IRawDevice
{
public:
    static constexpr int  kNumber = 3;

    HRESULT ListDrives (std::vector<PhysicalDrive> & outDrives) override
    {
        outDrives.assign (1, MakeDrive());

        return S_OK;
    }

    HRESULT Open (int number, PhysicalDrive & outDrive) override
    {
        if (number != kNumber)
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outDrive = MakeDrive();
        isOpen   = true;
        opens++;

        return S_OK;
    }

    HRESULT ReadBlocks (uint64_t first, uint32_t count, std::span<Byte> outBytes) override
    {
        size_t  start = (size_t) first * kBlockBytes;

        reads++;

        std::copy (bytes.begin() + (ptrdiff_t) start, bytes.begin() + (ptrdiff_t) (start + (size_t) count * kBlockBytes), outBytes.begin());

        return S_OK;
    }

    HRESULT WriteBlocks (uint64_t first, std::span<const Byte> blocks) override
    {
        writes++;

        std::copy (blocks.begin(), blocks.end(), bytes.begin() + (ptrdiff_t) ((size_t) first * kBlockBytes));

        return S_OK;
    }

    void Close() override
    {
        isOpen = false;
    }

    PhysicalDrive MakeDrive() const
    {
        PhysicalDrive  drive;

        drive.number    = kNumber;
        drive.model     = "Generic Card Reader";
        drive.bus       = "USB";
        drive.bytes     = bytes.size();
        drive.removable = removable;

        return drive;
    }

    std::vector<Byte>  bytes;
    bool               removable = true;
    bool               isOpen    = false;
    int                opens     = 0;
    int                reads     = 0;
    int                writes    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceTests
//
//  A RawDeviceClient and a RawDeviceServer on the two ends of an in-process
//  stream, the server on a thread and over a drive in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawDeviceTests)
{
public:

    static constexpr size_t    kBlock  = IBlockStore::kBlockBytes;
    static constexpr uint32_t  kBlocks = 400;

    struct Session
    {
        FakeRawDevice                         device;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        std::shared_ptr<InProcessByteStream>  serverEnd;
        std::thread                           thread;
        HRESULT                               result = E_PENDING;

        void Start (int drive = FakeRawDevice::kNumber)
        {
            InProcessByteStream::CreatePair (clientEnd, serverEnd);

            thread = std::thread ([this, drive]
            {
                RawDeviceServer  server (device, *serverEnd);

                result = server.Run (drive);
            });
        }

        HRESULT Finish()
        {
            clientEnd->Close();

            if (thread.joinable())
            {
                thread.join();
            }

            return result;
        }

        ~Session()
        {
            Finish();
        }
    };

    //  Each block filled with a byte that depends on its number.
    static std::vector<Byte> MakeBlocks (uint32_t count, Byte seed)
    {
        std::vector<Byte>  bytes ((size_t) count * kBlock, 0);

        for (uint32_t block = 0; block < count; block++)
        {
            std::fill_n (bytes.begin() + (ptrdiff_t) ((size_t) block * kBlock), kBlock, (Byte) (seed + block * 7));
        }

        return bytes;
    }

    static HRESULT SendRequest (IByteStream & stream, Byte operation, uint64_t first, uint32_t count)
    {
        std::vector<Byte>  bytes;

        bytes.push_back (operation);
        RawDeviceProtocol::AppendQuad (bytes, first);
        RawDeviceProtocol::AppendLong (bytes, count);

        return stream.Write (bytes.data(), bytes.size());
    }

    static HRESULT ReadReply (IByteStream & stream)
    {
        Byte  bytes[4] = {};

        AssertSucceeded (RawDeviceProtocol::ReadExact (stream, bytes, sizeof (bytes), RawDeviceProtocol::kReplyTimeoutMs));

        return (HRESULT) RawDeviceProtocol::GetLong (bytes);
    }

    TEST_METHOD (Connect_ReturnsTheDriveDescription)
    {
        Session        session;
        PhysicalDrive  drive;

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));

        Assert::AreEqual (FakeRawDevice::kNumber, drive.number);
        Assert::AreEqual (std::string ("Generic Card Reader"), drive.model);
        Assert::AreEqual (std::string ("USB"), drive.bus);
        Assert::AreEqual ((uint64_t) kBlocks * kBlock, drive.bytes);
        Assert::IsTrue (drive.removable);
        Assert::AreEqual (kBlocks, client.GetBlockCount());
    }


    TEST_METHOD (Connect_FailsWithAccessDeniedForADriveThatIsNotRemovable)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  block (kBlock, 0);

        session.device.bytes     = MakeBlocks (kBlocks, 1);
        session.device.removable = false;
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        Assert::AreEqual (E_ACCESSDENIED, client.Connect (drive));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, 0, 1));
        Assert::AreEqual (E_ACCESSDENIED, ReadReply (*session.clientEnd));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kWrite, 0, 1));
        AssertSucceeded (session.clientEnd->Write (block.data(), block.size()));
        Assert::AreEqual (E_ACCESSDENIED, ReadReply (*session.clientEnd));

        session.Finish();

        Assert::AreEqual (0, session.device.reads);
        Assert::AreEqual (0, session.device.writes);
        Assert::IsFalse (session.device.isOpen);
    }


    TEST_METHOD (Connect_FailsForADriveThatIsMissing)
    {
        Session        session;
        PhysicalDrive  drive;

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start (FakeRawDevice::kNumber + 1);

        RawDeviceClient  client (*session.clientEnd);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), client.Connect (drive));
    }


    TEST_METHOD (ReadBlocks_ReturnsARunLongerThanOneRequest)
    {
        Session            session;
        PhysicalDrive      drive;
        uint32_t           first = 5;
        uint32_t           count = RawDeviceProtocol::kMaxBlocks * 2 + 30;
        std::vector<Byte>  read ((size_t) count * kBlock, 0);

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (client.ReadBlocks (first, count, read));

        Assert::IsTrue (std::equal (read.begin(), read.end(), session.device.bytes.begin() + (ptrdiff_t) ((size_t) first * kBlock)));
        Assert::AreEqual (3, session.device.reads);
    }


    TEST_METHOD (WriteBlocks_ChangesTheDriveAtCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        uint32_t           first    = 10;
        uint32_t           count    = RawDeviceProtocol::kMaxBlocks + 20;
        std::vector<Byte>  original = MakeBlocks (kBlocks, 1);
        std::vector<Byte>  written  = MakeBlocks (count, 0x80);

        session.device.bytes = original;
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (client.WriteBlocks (first, written));

        Assert::AreEqual ((size_t) count, client.GetPendingBlockCount());
        Assert::AreEqual (0, session.device.writes);

        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual ((size_t) 0, client.GetPendingBlockCount());
        Assert::AreEqual (2, session.device.writes);
        Assert::IsTrue (std::equal (written.begin(), written.end(), session.device.bytes.begin() + (ptrdiff_t) ((size_t) first * kBlock)));

        //  The blocks on either side are as they were.
        Assert::IsTrue (std::equal (original.begin(), original.begin() + (ptrdiff_t) ((size_t) first * kBlock), session.device.bytes.begin()));
        Assert::IsTrue (std::equal (original.begin() + (ptrdiff_t) ((size_t) (first + count) * kBlock), original.end(), session.device.bytes.begin() + (ptrdiff_t) ((size_t) (first + count) * kBlock)));
    }


    TEST_METHOD (Commit_SendsSeparatedBlocksAsSeparateWrites)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  block = MakeBlocks (1, 0x55);

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (client.WriteBlocks (2, block));
        AssertSucceeded (client.WriteBlocks (3, block));
        AssertSucceeded (client.WriteBlocks (90, block));
        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual (2, session.device.writes);
        Assert::AreEqual ((Byte) 0x55, session.device.bytes[2 * kBlock]);
        Assert::AreEqual ((Byte) 0x55, session.device.bytes[3 * kBlock]);
        Assert::AreEqual ((Byte) 0x55, session.device.bytes[90 * kBlock]);
    }


    TEST_METHOD (ReadBlocks_ReturnsABlockWrittenBeforeCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeBlocks (kBlocks, 1);
        std::vector<Byte>  block    = MakeBlocks (1, 0xEE);
        std::vector<Byte>  read (3 * kBlock, 0);

        session.device.bytes = original;
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (client.WriteBlocks (21, block));
        AssertSucceeded (client.ReadBlocks (20, 3, read));

        Assert::IsTrue (std::equal (read.begin(), read.begin() + (ptrdiff_t) kBlock, original.begin() + (ptrdiff_t) (20 * kBlock)));
        Assert::IsTrue (std::equal (block.begin(), block.end(), read.begin() + (ptrdiff_t) kBlock));
        Assert::IsTrue (std::equal (read.begin() + (ptrdiff_t) (2 * kBlock), read.end(), original.begin() + (ptrdiff_t) (22 * kBlock)));
        Assert::IsTrue (session.device.bytes == original);
    }


    TEST_METHOD (Discard_DropsTheBlocksWrittenSinceTheLastCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeBlocks (kBlocks, 1);
        std::vector<Byte>  block    = MakeBlocks (1, 0xEE);

        session.device.bytes = original;
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (client.WriteBlocks (21, block));

        client.Discard();

        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual (0, session.device.writes);
        Assert::IsTrue (session.device.bytes == original);
    }


    TEST_METHOD (Request_PastTheEndOfTheDriveFails)
    {
        Session            session;
        std::vector<Byte>  block (kBlock, 0);

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, kBlocks, 1));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), ReadReply (*session.clientEnd));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kWrite, kBlocks, 1));
        AssertSucceeded (session.clientEnd->Write (block.data(), block.size()));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), ReadReply (*session.clientEnd));

        session.Finish();

        Assert::AreEqual (0, session.device.reads);
        Assert::AreEqual (0, session.device.writes);
    }


    TEST_METHOD (Run_EndsTheSessionAtAnUnknownOperation)
    {
        Session  session;

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (SendRequest (*session.clientEnd, 9, 0, 1));

        session.thread.join();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), session.result);
        Assert::IsFalse (session.device.isOpen);
    }


    TEST_METHOD (Run_EndsTheSessionAtARequestOverTheBlockLimit)
    {
        Session  session;

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, 0, RawDeviceProtocol::kMaxBlocks + 1));

        session.thread.join();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), session.result);
        Assert::AreEqual (0, session.device.reads);
    }


    TEST_METHOD (Run_ReturnsWhenTheStreamCloses)
    {
        Session        session;
        PhysicalDrive  drive;

        session.device.bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (session.Finish());

        Assert::AreEqual (1, session.device.opens);
        Assert::IsFalse (session.device.isOpen);
    }


    TEST_METHOD (Detect_FindsTheSamePartitionsThroughTheClient)
    {
        Session                 session;
        PhysicalDrive           drive;
        std::vector<Byte>       card         = PartitionCard::MakeCard();
        MemoryBlockStore        store (card);
        PartitionLayoutKind     direct       = PartitionLayoutKind::None;
        PartitionLayoutKind     throughPipe  = PartitionLayoutKind::None;
        std::vector<Partition>  inMemory;
        std::vector<Partition>  found;

        session.device.bytes = card;
        session.Start();

        RawDeviceClient  client (*session.clientEnd);

        AssertSucceeded (client.Connect (drive));
        AssertSucceeded (PartitionLayout::Detect (store, direct, inMemory));
        AssertSucceeded (PartitionLayout::Detect (client, throughPipe, found));

        Assert::IsTrue (direct != PartitionLayoutKind::None);
        Assert::IsTrue (direct == throughPipe);
        Assert::AreEqual (inMemory.size(), found.size());

        for (size_t at = 0; at < inMemory.size(); at++)
        {
            Assert::IsTrue (inMemory[at].volumeKind == found[at].volumeKind);
        }
    }


    TEST_METHOD (IsValidPipeName_AdmitsOnlyThePrefixAndHexDigits)
    {
        Assert::IsTrue  (Win32RawDeviceHelper::IsValidPipeName (L"CassoRawDevice0123456789ABCDEF"));
        Assert::IsFalse (Win32RawDeviceHelper::IsValidPipeName (L"CassoRawDevice"));
        Assert::IsFalse (Win32RawDeviceHelper::IsValidPipeName (L"Other0123"));
        Assert::IsFalse (Win32RawDeviceHelper::IsValidPipeName (L"CassoRawDevice01\\..\\x"));
        Assert::IsFalse (Win32RawDeviceHelper::IsValidPipeName (L"CassoRawDevice01 23"));
        Assert::IsFalse (Win32RawDeviceHelper::IsValidPipeName (std::wstring (L"CassoRawDevice") + std::wstring (80, L'A')));
    }


    TEST_METHOD (TryParseDrive_AdmitsOnlyDecimalDigits)
    {
        int  drive = 0;

        Assert::IsTrue (Win32RawDeviceHelper::TryParseDrive (L"12", drive));
        Assert::AreEqual (12, drive);

        Assert::IsFalse (Win32RawDeviceHelper::TryParseDrive (L"", drive));
        Assert::IsFalse (Win32RawDeviceHelper::TryParseDrive (L"-1", drive));
        Assert::IsFalse (Win32RawDeviceHelper::TryParseDrive (L"1 ", drive));
        Assert::IsFalse (Win32RawDeviceHelper::TryParseDrive (L"0x1", drive));
        Assert::IsFalse (Win32RawDeviceHelper::TryParseDrive (L"12345", drive));
    }
};
