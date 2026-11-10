#include "Pch.h"

#include "../EhmTestHelper.h"
#include "PartitionCard.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Seams/InProcessByteStream.h"
#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Win32StorageCardProxy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDrives
//
//  The PC's drives in memory, by number, with the calls made on them
//  counted. Each FakeRawDevice the server makes opens one of them.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeDrives
{
    struct Drive
    {
        std::vector<Byte>         bytes;
        bool                      removable = true;
        std::vector<BlockExtent>  mounted;
        bool                      isOpen    = false;
        int                       opens     = 0;
        int                       reads     = 0;
        int                       writes    = 0;
        int                       locks     = 0;
        int                       unlocks   = 0;
        bool                      lockFails = false;
    };

    std::map<int, Drive>  drives;
};





class FakeRawDevice : public IRawDevice
{
public:
    explicit FakeRawDevice (FakeDrives & drives) : m_drives (drives) {}

    HRESULT ListDrives (std::vector<PhysicalDrive> & outDrives) override
    {
        outDrives.clear();

        for (auto & [number, drive] : m_drives.drives)
        {
            outDrives.push_back (MakeDrive (number, drive));
        }

        return S_OK;
    }

    HRESULT Open (int number, PhysicalDrive & outDrive) override
    {
        auto  found = m_drives.drives.find (number);

        if (found == m_drives.drives.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        m_drive  = &found->second;
        outDrive = MakeDrive (number, *m_drive);

        m_drive->isOpen = true;
        m_drive->opens++;

        return S_OK;
    }

    HRESULT ReadBlocks (uint64_t first, uint32_t count, std::span<Byte> outBytes) override
    {
        size_t  start = (size_t) first * kBlockBytes;

        m_drive->reads++;

        std::copy (m_drive->bytes.begin() + (ptrdiff_t) start, m_drive->bytes.begin() + (ptrdiff_t) (start + (size_t) count * kBlockBytes), outBytes.begin());

        return S_OK;
    }

    HRESULT WriteBlocks (uint64_t first, std::span<const Byte> blocks) override
    {
        m_drive->writes++;

        std::copy (blocks.begin(), blocks.end(), m_drive->bytes.begin() + (ptrdiff_t) ((size_t) first * kBlockBytes));

        return S_OK;
    }

    void Close() override
    {
        if (m_drive != nullptr)
        {
            m_drive->isOpen = false;
        }
    }

    HRESULT GetMountedExtents (std::vector<BlockExtent> & outExtents) override
    {
        outExtents = m_drive->mounted;

        return S_OK;
    }

    HRESULT LockVolumes() override
    {
        m_drive->locks++;

        return m_drive->lockFails ? HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED) : S_OK;
    }

    void UnlockVolumes() override
    {
        m_drive->unlocks++;
    }

    static PhysicalDrive MakeDrive (int number, const FakeDrives::Drive & drive)
    {
        PhysicalDrive  description;

        description.number    = number;
        description.model     = "Generic Card Reader";
        description.bus       = "USB";
        description.bytes     = drive.bytes.size();
        description.removable = drive.removable;

        return description;
    }

private:
    FakeDrives         & m_drives;
    FakeDrives::Drive  * m_drive = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceTests
//
//  A RawDeviceConnection and a RawDeviceServer on the two ends of an
//  in-process stream, the server on a thread and over drives in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawDeviceTests)
{
public:

    static constexpr size_t    kBlock      = IBlockStore::kBlockBytes;
    static constexpr uint32_t  kBlocks     = 400;
    static constexpr int       kCard       = 3;
    static constexpr int       kOtherCard  = 5;

    //  Blocks inside the card's second partition.
    static constexpr uint32_t  kInside     = PartitionCard::kSecondStart + 10;

    struct Session
    {
        FakeDrives                            drives;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        std::shared_ptr<InProcessByteStream>  serverEnd;
        std::unique_ptr<RawDeviceConnection>  connection;
        std::thread                           thread;
        HRESULT                               result = E_PENDING;

        void Start()
        {
            InProcessByteStream::CreatePair (clientEnd, serverEnd);

            connection = std::make_unique<RawDeviceConnection> (*clientEnd);

            thread = std::thread ([this]
            {
                RawDeviceServer  server ([this]() { return std::make_unique<FakeRawDevice> (drives); }, *serverEnd);

                result = server.Run();
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

    //  A MicroDrive card, a whole number of blocks.
    static std::vector<Byte> MakeCard()
    {
        std::vector<Byte>  card = PartitionCard::MakeCard();

        card.resize ((size_t) PartitionCard::kCardBlocks * kBlock);

        return card;
    }

    static HRESULT SendRequest (IByteStream & stream, Byte operation, int number, uint64_t first, uint32_t count)
    {
        std::vector<Byte>  bytes;

        bytes.push_back (operation);
        RawDeviceProtocol::AppendLong (bytes, (uint32_t) number);
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

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));

        Assert::AreEqual (kCard, drive.number);
        Assert::AreEqual (std::string ("Generic Card Reader"), drive.model);
        Assert::AreEqual (std::string ("USB"), drive.bus);
        Assert::AreEqual ((uint64_t) kBlocks * kBlock, drive.bytes);
        Assert::IsTrue (drive.removable);
        Assert::AreEqual (kBlocks, client.GetBlockCount());
    }


    TEST_METHOD (Connect_FailsWithAccessDeniedForADriveThatIsNotAStorageCard)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  block (kBlock, 0);

        session.drives.drives[kCard].bytes     = MakeBlocks (kBlocks, 1);
        session.drives.drives[kCard].removable = false;
        session.Start();

        RawDeviceClient  client (*session.connection);

        Assert::AreEqual (E_ACCESSDENIED, client.Connect (kCard, drive));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, kCard, 0, 1));
        Assert::AreEqual (E_ACCESSDENIED, ReadReply (*session.clientEnd));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kWrite, kCard, 0, 1));
        AssertSucceeded (session.clientEnd->Write (block.data(), block.size()));
        Assert::AreEqual (E_ACCESSDENIED, ReadReply (*session.clientEnd));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].reads);
        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsFalse (session.drives.drives[kCard].isOpen);
    }


    TEST_METHOD (Connect_FailsForADriveThatIsMissing)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.connection);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), client.Connect (kCard + 1, drive));
    }


    TEST_METHOD (Request_ForADriveThatIsNotOpenFails)
    {
        Session  session;

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, kCard, 0, 1));
        Assert::AreEqual (E_ACCESSDENIED, ReadReply (*session.clientEnd));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].opens);
    }


    TEST_METHOD (TwoCards_AreReadThroughOneConnection)
    {
        Session            session;
        PhysicalDrive      first;
        PhysicalDrive      second;
        std::vector<Byte>  readFirst (kBlock, 0);
        std::vector<Byte>  readSecond (kBlock, 0);

        session.drives.drives[kCard].bytes      = MakeBlocks (kBlocks, 1);
        session.drives.drives[kOtherCard].bytes = MakeBlocks (kBlocks, 0x40);
        session.Start();

        RawDeviceClient  cardClient  (*session.connection);
        RawDeviceClient  otherClient (*session.connection);

        AssertSucceeded (cardClient.Connect (kCard, first));
        AssertSucceeded (otherClient.Connect (kOtherCard, second));
        AssertSucceeded (cardClient.ReadBlocks (7, 1, readFirst));
        AssertSucceeded (otherClient.ReadBlocks (7, 1, readSecond));

        Assert::IsTrue (std::equal (readFirst.begin(), readFirst.end(), session.drives.drives[kCard].bytes.begin() + (ptrdiff_t) (7 * kBlock)));
        Assert::IsTrue (std::equal (readSecond.begin(), readSecond.end(), session.drives.drives[kOtherCard].bytes.begin() + (ptrdiff_t) (7 * kBlock)));
    }


    TEST_METHOD (Destroying_AClientClosesItsCardOnly)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes      = MakeBlocks (kBlocks, 1);
        session.drives.drives[kOtherCard].bytes = MakeBlocks (kBlocks, 2);
        session.Start();

        RawDeviceClient  open (*session.connection);

        AssertSucceeded (open.Connect (kOtherCard, drive));

        {
            RawDeviceClient  closed (*session.connection);

            AssertSucceeded (closed.Connect (kCard, drive));
        }

        session.Finish();

        Assert::AreEqual (1, session.drives.drives[kCard].opens);
        Assert::IsFalse (session.drives.drives[kCard].isOpen);
        Assert::IsFalse (session.drives.drives[kOtherCard].isOpen);
    }


    TEST_METHOD (Open_FailsPastTheLimitOfOpenCards)
    {
        Session        session;
        PhysicalDrive  drive;
        HRESULT        hr = S_OK;

        for (int number = 0; number <= (int) RawDeviceProtocol::kMaxDrives; number++)
        {
            session.drives.drives[number].bytes = MakeBlocks (4, 1);
        }

        session.Start();

        for (int number = 0; number < (int) RawDeviceProtocol::kMaxDrives; number++)
        {
            AssertSucceeded (session.connection->Open (number, drive));
        }

        hr = session.connection->Open ((int) RawDeviceProtocol::kMaxDrives, drive);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_TOO_MANY_OPEN_FILES), hr);
    }


    TEST_METHOD (ReadBlocks_ReturnsARunLongerThanOneRequest)
    {
        Session            session;
        PhysicalDrive      drive;
        uint32_t           first = 5;
        uint32_t           count = RawDeviceProtocol::kMaxBlocks * 2 + 30;
        std::vector<Byte>  read ((size_t) count * kBlock, 0);

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.ReadBlocks (first, count, read));

        Assert::IsTrue (std::equal (read.begin(), read.end(), session.drives.drives[kCard].bytes.begin() + (ptrdiff_t) ((size_t) first * kBlock)));
    }


    TEST_METHOD (Write_InsideAnApplePartitionChangesTheCardAtCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        uint32_t           count    = RawDeviceProtocol::kMaxBlocks + 20;
        std::vector<Byte>  original = MakeCard();
        std::vector<Byte>  written  = MakeBlocks (count, 0x80);

        session.drives.drives[kCard].bytes = original;
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (kInside, written));

        Assert::AreEqual ((size_t) count, client.GetPendingBlockCount());
        Assert::AreEqual (0, session.drives.drives[kCard].writes);

        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual ((size_t) 0, client.GetPendingBlockCount());
        Assert::AreEqual (2, session.drives.drives[kCard].writes);
        Assert::IsTrue (std::equal (written.begin(), written.end(), session.drives.drives[kCard].bytes.begin() + (ptrdiff_t) ((size_t) kInside * kBlock)));
    }


    TEST_METHOD (Write_OnADriveWithNoAppleLayoutFails)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeBlocks (kBlocks, 1);

        session.drives.drives[kCard].bytes = original;
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (10, MakeBlocks (1, 0x80)));

        Assert::AreEqual (E_ACCESSDENIED, client.Commit());

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Write_OutsideEveryApplePartitionFails)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeCard();

        session.drives.drives[kCard].bytes = original;
        session.Start();

        RawDeviceClient  client (*session.connection);

        //  Block 0 is the partition map.
        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (0, MakeBlocks (1, 0x80)));

        Assert::AreEqual (E_ACCESSDENIED, client.Commit());

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Write_PastTheEndOfAnApplePartitionFails)
    {
        Session            session;
        PhysicalDrive      drive;
        uint32_t           last = PartitionCard::kSecondStart + PartitionCard::kSecondBlocks - 1;

        session.drives.drives[kCard].bytes = MakeCard();
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (last, MakeBlocks (2, 0x80)));

        Assert::AreEqual (E_ACCESSDENIED, client.Commit());

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
    }


    TEST_METHOD (Write_OverAMountedWindowsVolumeFails)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeCard();

        session.drives.drives[kCard].bytes   = original;
        session.drives.drives[kCard].mounted = { BlockExtent { kInside + 5, 50 } };
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (kInside, MakeBlocks (10, 0x80)));

        Assert::AreEqual (E_ACCESSDENIED, client.Commit());

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Write_BesideAMountedWindowsVolumeSucceeds)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes   = MakeCard();
        session.drives.drives[kCard].mounted = { BlockExtent { kInside + 10, 50 } };
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (kInside, MakeBlocks (10, 0x80)));
        AssertSucceeded (client.Commit());

        session.Finish();

        Assert::AreEqual (1, session.drives.drives[kCard].writes);
    }


    TEST_METHOD (Commit_SendsSeparatedBlocksAsSeparateWrites)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  block = MakeBlocks (1, 0x55);

        session.drives.drives[kCard].bytes = MakeCard();
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (kInside, block));
        AssertSucceeded (client.WriteBlocks (kInside + 1, block));
        AssertSucceeded (client.WriteBlocks (kInside + 90, block));
        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual (2, session.drives.drives[kCard].writes);
        Assert::AreEqual ((Byte) 0x55, session.drives.drives[kCard].bytes[(size_t) kInside * kBlock]);
        Assert::AreEqual ((Byte) 0x55, session.drives.drives[kCard].bytes[(size_t) (kInside + 1) * kBlock]);
        Assert::AreEqual ((Byte) 0x55, session.drives.drives[kCard].bytes[(size_t) (kInside + 90) * kBlock]);
    }


    TEST_METHOD (ReadBlocks_ReturnsABlockWrittenBeforeCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeBlocks (kBlocks, 1);
        std::vector<Byte>  block    = MakeBlocks (1, 0xEE);
        std::vector<Byte>  read (3 * kBlock, 0);

        session.drives.drives[kCard].bytes = original;
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (21, block));
        AssertSucceeded (client.ReadBlocks (20, 3, read));

        Assert::IsTrue (std::equal (read.begin(), read.begin() + (ptrdiff_t) kBlock, original.begin() + (ptrdiff_t) (20 * kBlock)));
        Assert::IsTrue (std::equal (block.begin(), block.end(), read.begin() + (ptrdiff_t) kBlock));
        Assert::IsTrue (std::equal (read.begin() + (ptrdiff_t) (2 * kBlock), read.end(), original.begin() + (ptrdiff_t) (22 * kBlock)));
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Discard_DropsTheBlocksWrittenSinceTheLastCommit)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  original = MakeCard();

        session.drives.drives[kCard].bytes = original;
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (kInside, MakeBlocks (1, 0xEE)));

        client.Discard();

        AssertSucceeded (client.Commit());
        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Request_PastTheEndOfTheDriveFails)
    {
        Session            session;
        PhysicalDrive      drive;
        std::vector<Byte>  block (kBlock, 0);

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (session.connection->Open (kCard, drive));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, kCard, kBlocks, 1));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), ReadReply (*session.clientEnd));

        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kWrite, kCard, kBlocks, 1));
        AssertSucceeded (session.clientEnd->Write (block.data(), block.size()));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), ReadReply (*session.clientEnd));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
    }


    TEST_METHOD (Run_EndsTheSessionAtAnUnknownOperation)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (session.connection->Open (kCard, drive));
        AssertSucceeded (SendRequest (*session.clientEnd, 9, kCard, 0, 1));

        session.thread.join();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), session.result);
        Assert::IsFalse (session.drives.drives[kCard].isOpen);
    }


    TEST_METHOD (Run_EndsTheSessionAtARequestOverTheBlockLimit)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes = MakeBlocks (kBlocks, 1);
        session.Start();

        AssertSucceeded (session.connection->Open (kCard, drive));
        AssertSucceeded (SendRequest (*session.clientEnd, RawDeviceProtocol::kRead, kCard, 0, RawDeviceProtocol::kMaxBlocks + 1));

        session.thread.join();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), session.result);
        Assert::IsFalse (session.drives.drives[kCard].isOpen);
    }


    TEST_METHOD (Run_ClosesEveryCardWhenTheStreamCloses)
    {
        Session        session;
        PhysicalDrive  drive;

        session.drives.drives[kCard].bytes      = MakeBlocks (kBlocks, 1);
        session.drives.drives[kOtherCard].bytes = MakeBlocks (kBlocks, 2);
        session.Start();

        AssertSucceeded (session.connection->Open (kCard, drive));
        AssertSucceeded (session.connection->Open (kOtherCard, drive));
        AssertSucceeded (session.Finish());

        Assert::IsFalse (session.drives.drives[kCard].isOpen);
        Assert::IsFalse (session.drives.drives[kOtherCard].isOpen);
    }


    TEST_METHOD (Detect_FindsTheSamePartitionsThroughTheClient)
    {
        Session                 session;
        PhysicalDrive           drive;
        std::vector<Byte>       card         = MakeCard();
        MemoryBlockStore        store (card);
        PartitionLayoutKind     direct       = PartitionLayoutKind::None;
        PartitionLayoutKind     throughPipe  = PartitionLayoutKind::None;
        std::vector<Partition>  inMemory;
        std::vector<Partition>  found;

        session.drives.drives[kCard].bytes = card;
        session.Start();

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
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


    TEST_METHOD (Initialize_WritesCffaVolumesThatCanThenBeWritten)
    {
        Session                 session;
        PhysicalDrive           drive;
        uint32_t                blocks = RawDeviceServer::kSliceBlocks * 3 + 500;
        PartitionLayoutKind     kind   = PartitionLayoutKind::None;
        std::vector<Partition>  found;

        session.drives.drives[kCard].bytes.assign ((size_t) blocks * kBlock, 0xA5);
        session.Start();

        AssertSucceeded (session.connection->Initialize (kCard));

        {
            MemoryBlockStore  store (session.drives.drives[kCard].bytes);

            AssertSucceeded (PartitionLayout::Detect (store, kind, found));
        }

        Assert::IsTrue (kind == PartitionLayoutKind::Cffa);
        Assert::AreEqual ((size_t) 4, found.size());
        Assert::IsTrue (found[0].volumeKind == VolumeKind::ProDos);
        Assert::AreEqual (1, session.drives.drives[kCard].locks);
        Assert::IsTrue (session.drives.drives[kCard].unlocks >= 1);

        RawDeviceClient  client (*session.connection);

        AssertSucceeded (client.Connect (kCard, drive));
        AssertSucceeded (client.WriteBlocks (RawDeviceServer::kSliceBlocks + 10, MakeBlocks (1, 0x11)));
        AssertSucceeded (client.Commit());
    }


    TEST_METHOD (Initialize_WritesNothingWhenAVolumeFailsToLock)
    {
        Session            session;
        std::vector<Byte>  original ((size_t) RawDeviceServer::kSliceBlocks * 2 * kBlock, 0xA5);

        session.drives.drives[kCard].bytes     = original;
        session.drives.drives[kCard].lockFails = true;
        session.Start();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), session.connection->Initialize (kCard));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::IsTrue (session.drives.drives[kCard].bytes == original);
    }


    TEST_METHOD (Initialize_FailsForADriveThatIsNotAStorageCard)
    {
        Session  session;

        session.drives.drives[kCard].bytes.assign ((size_t) RawDeviceServer::kSliceBlocks * 2 * kBlock, 0xA5);
        session.drives.drives[kCard].removable = false;
        session.Start();

        Assert::AreEqual (E_ACCESSDENIED, session.connection->Initialize (kCard));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
        Assert::AreEqual (0, session.drives.drives[kCard].locks);
    }


    TEST_METHOD (Initialize_FailsForACardOfOneSliceOrLess)
    {
        Session  session;

        session.drives.drives[kCard].bytes.assign ((size_t) RawDeviceServer::kSliceBlocks * kBlock, 0xA5);
        session.Start();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), session.connection->Initialize (kCard));

        session.Finish();

        Assert::AreEqual (0, session.drives.drives[kCard].writes);
    }


    TEST_METHOD (BlockExtent_ContainsAndOverlapsAtTheEdges)
    {
        BlockExtent  extent { 100, 50 };

        Assert::IsTrue  (extent.Contains (100, 50));
        Assert::IsTrue  (extent.Contains (149, 1));
        Assert::IsFalse (extent.Contains (99, 2));
        Assert::IsFalse (extent.Contains (149, 2));
        Assert::IsFalse (extent.Contains (UINT64_MAX, 2));

        Assert::IsTrue  (extent.Overlaps (149, 10));
        Assert::IsTrue  (extent.Overlaps (90, 11));
        Assert::IsFalse (extent.Overlaps (150, 10));
        Assert::IsFalse (extent.Overlaps (90, 10));
    }


    TEST_METHOD (IsValidPipeName_AdmitsOnlyThePrefixAndHexDigits)
    {
        Assert::IsTrue  (Win32StorageCardProxy::IsValidPipeName (L"CassoStorageCard0123456789ABCDEF"));
        Assert::IsFalse (Win32StorageCardProxy::IsValidPipeName (L"CassoStorageCard"));
        Assert::IsFalse (Win32StorageCardProxy::IsValidPipeName (L"Other0123"));
        Assert::IsFalse (Win32StorageCardProxy::IsValidPipeName (L"CassoStorageCard01\\..\\x"));
        Assert::IsFalse (Win32StorageCardProxy::IsValidPipeName (L"CassoStorageCard01 23"));
        Assert::IsFalse (Win32StorageCardProxy::IsValidPipeName (std::wstring (L"CassoStorageCard") + std::wstring (80, L'A')));
    }
};
