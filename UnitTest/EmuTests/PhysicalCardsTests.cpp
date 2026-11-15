#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "PartitionCard.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PhysicalCardFileIo.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Seams/RawDevice/RawDeviceProtocol.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeCardHost
//
//  Drive 0 is fixed, and drive 2 is removable with a MicroDrive card in it.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeCardHost : public IPhysicalCardHost
{
    std::vector<Byte>    card;
    MemoryBlockStore   * store = nullptr;
    int                  opens = 0;

    FakeCardHost()
    {
        card = PartitionCard::MakeCard();
        card.resize ((size_t) PartitionCard::kCardBlocks * PartitionCard::kBlock);
    }

    HRESULT ListDrives (std::vector<PhysicalDrive> & outDrives) override
    {
        outDrives = { MakeDrive (0, false), MakeDrive (2, true) };
        return S_OK;
    }

    HRESULT OpenDrive (int number, HWND, PhysicalDrive & outDrive, std::unique_ptr<IBlockStore> & outStore) override
    {
        auto  memory = std::make_unique<MemoryBlockStore> (card);

        opens++;
        store    = memory.get();
        outDrive = MakeDrive (number, true);
        outStore = std::move (memory);

        return S_OK;
    }

    HRESULT HasUnmountedPartition (int, bool & outHasUnmounted) override
    {
        outHasUnmounted = hasUnmounted;
        return S_OK;
    }

    HRESULT ListVolumes (int, std::vector<WindowsVolume> & outVolumes) override
    {
        outVolumes = volumes;
        return S_OK;
    }

    HRESULT FormatFirstPartition (int number, const WriteProgress &, bool & outDone) override
    {
        outDone = firstPartition;

        if (firstPartition)
        {
            formatted.push_back (number);
        }

        return S_OK;
    }

    bool              firstPartition = false;
    std::vector<int>  formatted;

    HRESULT InitializeDrive (int number, HWND, uint64_t, const WriteProgress & progress) override
    {
        initialized.push_back (number);

        if (onInitialize)
        {
            onInitialize (progress);
        }

        return S_OK;
    }

    std::function<void (const WriteProgress &)>  onInitialize;

    bool IsBrokerRunning() override { return brokerRunning; }

    bool  brokerRunning = false;

    HRESULT FindVolumeDrive (const std::wstring &, int & outNumber) override
    {
        outNumber = volumeDrive;
        return outNumber >= 0 ? S_OK : HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);
    }

    int                         volumeDrive  = -1;
    bool                        hasUnmounted = true;
    std::vector<WindowsVolume>  volumes;
    std::vector<int>            initialized;

    static PhysicalDrive MakeDrive (int number, bool removable)
    {
        return PhysicalDrive { number, "Test Card", (uint64_t) PartitionCard::kCardBlocks * PartitionCard::kBlock, "USB", removable };
    }
};





TEST_CLASS (PhysicalCardsTests)
{
public:
    TEST_METHOD (ParsePath_ReturnsTheKindAndNumbers)
    {
        int  drive     = 0;
        int  partition = 0;

        Assert::IsTrue (PhysicalCards::ParsePath ("\\\\.\\PhysicalDrive2", drive, partition) == PhysicalCards::PathKind::Card);
        Assert::AreEqual (2, drive);
        Assert::IsTrue (PhysicalCards::ParsePath ("\\\\.\\PhysicalDrive12|#3.po", drive, partition) == PhysicalCards::PathKind::Partition);
        Assert::AreEqual (12, drive);
        Assert::AreEqual (3, partition);
        Assert::IsTrue (PhysicalCards::ParsePath ("\\\\.\\PhysicalDrive2|#3.po.casso-x.tmp", drive, partition) == PhysicalCards::PathKind::Other);
        Assert::IsTrue (PhysicalCards::ParsePath ("C:\\Disks\\card.po", drive, partition) == PhysicalCards::PathKind::None);
        Assert::IsTrue (PhysicalCards::ParsePath ("\\\\.\\PhysicalDrive", drive, partition) == PhysicalCards::PathKind::None);
    }


    TEST_METHOD (ListRemovableDrives_LeavesOutTheFixedDrive)
    {
        FakeCardHost                host;
        PhysicalCards               cards (host);
        std::vector<PhysicalDrive>  drives;

        AssertSucceeded (cards.ListRemovableDrives (drives));
        Assert::AreEqual ((size_t) 1, drives.size());
        Assert::AreEqual (2, drives[0].number);
    }


    TEST_METHOD (Open_FailsWithAccessDeniedForAFixedDrive)
    {
        FakeCardHost   host;
        PhysicalCards  cards (host);

        Assert::AreEqual (E_ACCESSDENIED, cards.Open (0, nullptr));
        Assert::AreEqual (0, host.opens);
    }


    TEST_METHOD (ReadPartition_FailsForACardThatIsNotOpen)
    {
        FakeCardHost       host;
        PhysicalCards      cards (host);
        std::vector<Byte>  bytes;

        HRESULT            hr = cards.ReadPartition (2, 2, bytes);

        Assert::IsTrue (FAILED (hr));
        Assert::AreEqual (0, host.opens);
    }


    TEST_METHOD (ReadPartition_ReturnsThePartitionOfAnOpenCard)
    {
        FakeCardHost                 host;
        PhysicalCards                cards (host);
        std::vector<Byte>            bytes;
        std::vector<PartitionEntry>  entries;
        PartitionLayoutKind          kind = PartitionLayoutKind::None;

        AssertSucceeded (cards.Open (2, nullptr));
        AssertSucceeded (cards.ListPartitions (2, kind, entries));
        Assert::IsTrue (entries.size() >= 2);

        AssertSucceeded (cards.ReadPartition (2, 2, bytes));
        Assert::IsTrue (bytes == PartitionCard::GetSecond (host.card));
    }


    TEST_METHOD (WritePartition_IsCanceledWhenTheConfirmationDeclines)
    {
        FakeCardHost       host;
        PhysicalCards      cards (host);
        std::vector<Byte>  bytes (PartitionCard::kSecondBytes, 0x77);

        cards.SetConfirmWrite ([] (const PhysicalDrive &) { return false; });
        AssertSucceeded (cards.Open (2, nullptr));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), cards.WritePartition (2, 2, bytes));
        Assert::IsTrue (host.store->GetBytes() == host.card);
    }


    TEST_METHOD (WritePartition_ChangesTheCardWhenConfirmed)
    {
        FakeCardHost       host;
        PhysicalCards      cards (host);
        std::vector<Byte>  bytes (PartitionCard::kSecondBytes, 0x77);
        std::vector<Byte>  readBack;
        std::string        model;

        cards.SetConfirmWrite ([&model] (const PhysicalDrive & drive) { model = drive.model; return true; });
        AssertSucceeded (cards.Open (2, nullptr));
        AssertSucceeded (cards.WritePartition (2, 2, bytes));

        Assert::AreEqual (std::string ("Test Card"), model);
        Assert::IsTrue (PartitionCard::GetSecond (host.store->GetBytes()) == bytes);

        AssertSucceeded (cards.ReadPartition (2, 2, readBack));
        Assert::IsTrue (readBack == bytes);
    }


    TEST_METHOD (FileIo_ReadsAPartitionPathFromTheCard)
    {
        FakeCardHost        host;
        PhysicalCards       cards (host);
        FakeDiskFileIo      unused;
        std::vector<Byte>   bytes;

        AssertSucceeded (cards.Open (2, nullptr));

        {
            PhysicalCardFileIo  io (unused, cards);

            AssertSucceeded (io.ReadAllBytes ("\\\\.\\PhysicalDrive2|#2.po", bytes));
            Assert::IsTrue (bytes == PartitionCard::GetSecond (host.card));
        }
    }


    //  A partition of a card in a reader opens in the disk inspector's block
    //  mode through the card's file access, its blocks read through the
    //  card's cache, and a block written there is on the card after a commit.
    TEST_METHOD (BlockImageFile_OpensAPartitionOfAnOpenCard)
    {
        static constexpr const char *  kPath = "\\\\.\\PhysicalDrive2|#2.po";



        FakeCardHost                     host;
        PhysicalCards                    cards (host);
        FakeDiskFileIo                   unused;
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;
        std::vector<Byte>                block (IBlockStore::kBlockBytes, 0);
        std::vector<Byte>                second;

        cards.SetConfirmWrite ([] (const PhysicalDrive &) { return true; });
        AssertSucceeded (cards.Open (2, nullptr));

        {
            PhysicalCardFileIo  io (unused, cards);

            Assert::IsTrue   (BlockImageFile::IsBlockImage (io, kPath), L"an 800K partition opens in block mode");
            AssertSucceeded  (BlockImageFile::Open (io, kPath, file, error));
            Assert::AreEqual (PartitionCard::kSecondBlocks, file->GetBlockCount());

            second = PartitionCard::GetSecond (host.card);

            AssertSucceeded (file->GetStore().ReadBlocks (2, 1, block));
            Assert::IsTrue  (std::equal (block.begin(), block.end(), second.begin() + 2 * IBlockStore::kBlockBytes), L"block 2 is the volume's directory");

            block.assign (IBlockStore::kBlockBytes, 0xA5);
            AssertSucceeded (file->GetStore().WriteBlocks (700, block));
            AssertSucceeded (file->GetStore().Commit());

            file.reset();
        }

        Assert::AreEqual ((Byte) 0xA5, host.store->GetBytes()[(size_t) (PartitionCard::kSecondStart + 700) * IBlockStore::kBlockBytes], L"the block is on the card");
    }


    TEST_METHOD (ListStorageCards_ListsRemovableDrivesWithTheirLayouts)
    {
        FakeCardHost                              host;
        PhysicalCards                             cards (host);
        std::vector<PhysicalCards::StorageCard>   found;

        host.hasUnmounted = false;

        AssertSucceeded (cards.ListStorageCards (found));
        Assert::AreEqual ((size_t) 1, found.size());
        Assert::AreEqual (2, found[0].drive.number);
        Assert::IsFalse (found[0].hasUnmounted);
        Assert::AreEqual (0, host.opens);
    }


    TEST_METHOD (FormatCardLabel_ReturnsTheModelAndSize)
    {
        PhysicalDrive  drive = FakeCardHost::MakeDrive (2, true);

        drive.bytes = 32ull * 1024 * 1024 * 1024;
        Assert::AreEqual (std::wstring (L"Storage card (Test Card, 32.0 GB)"), PhysicalCards::FormatCardLabel (drive));

        drive.model.clear();
        Assert::AreEqual (std::wstring (L"Storage card (32.0 GB)"), PhysicalCards::FormatCardLabel (drive));
    }


    TEST_METHOD (Initialize_FailsForAFixedDriveAndClosesAnOpenCard)
    {
        FakeCardHost   host;
        PhysicalCards  cards (host);

        Assert::AreEqual (E_ACCESSDENIED, cards.Initialize (0, nullptr, 4));
        Assert::IsTrue (host.initialized.empty());

        AssertSucceeded (cards.Open (2, nullptr));
        AssertSucceeded (cards.Initialize (2, nullptr, 4));

        Assert::IsFalse (cards.IsOpen (2));
        Assert::AreEqual ((size_t) 1, host.initialized.size());
    }


    TEST_METHOD (Initialize_Fat32OverAFirstPartitionSkipsTheBroker)
    {
        FakeCardHost   host;
        PhysicalCards  cards (host);

        host.firstPartition = true;
        AssertSucceeded (cards.Initialize (2, nullptr, RawDeviceProtocol::kLayoutFat32));
        Assert::AreEqual ((size_t) 1, host.formatted.size());
        Assert::IsTrue (host.initialized.empty());

        host.firstPartition = false;
        AssertSucceeded (cards.Initialize (2, nullptr, RawDeviceProtocol::kLayoutFat32));
        Assert::AreEqual ((size_t) 1, host.initialized.size());
    }


    TEST_METHOD (Initialize_ReportsProgressAndIsBusyUntilDone)
    {
        FakeCardHost   host;
        PhysicalCards  cards (host);
        uint64_t       done      = 0;
        uint64_t       total     = 0;
        uint64_t       reported  = 0;
        bool           wasBusy   = false;
        HRESULT        openWhile = S_OK;

        host.onInitialize = [&] (const WriteProgress & progress)
        {
            progress (64, 256);
            wasBusy   = cards.TryGetWork (2, done, total);
            openWhile = cards.Open (2, nullptr);
        };

        AssertSucceeded (cards.Initialize (2, nullptr, 4, [&] (uint64_t doneBytes, uint64_t) { reported = doneBytes; }));

        Assert::IsTrue (wasBusy);
        Assert::AreEqual ((uint64_t) 64,  done);
        Assert::AreEqual ((uint64_t) 256, total);
        Assert::AreEqual ((uint64_t) 64,  reported);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BUSY), openWhile);
        Assert::IsFalse (cards.TryGetWork (2, done, total));
        AssertSucceeded (cards.Open (2, nullptr));
    }


    TEST_METHOD (Open_ConfirmsBeforeStartingTheBrokerAndCancelsWhenDeclined)
    {
        FakeCardHost   host;
        PhysicalCards  cards (host);
        int            calls  = 0;
        bool           accept = false;

        cards.SetConfirmElevation ([&] (const PhysicalDrive &) { calls++; return accept; });

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), cards.Open (2, nullptr));
        Assert::AreEqual (1, calls);

        accept = true;
        AssertSucceeded (cards.Open (2, nullptr));
        Assert::AreEqual (2, calls);

        host.brokerRunning = true;
        AssertSucceeded (cards.Open (2, nullptr));
        Assert::AreEqual (2, calls);
    }


    TEST_METHOD (FormatDrive_ReturnsTheModelAndSize)
    {
        PhysicalDrive  drive = FakeCardHost::MakeDrive (2, true);
        std::wstring   text  = PhysicalCards::FormatDrive (drive);

        Assert::IsTrue (text.find (L"Test Card") != std::wstring::npos);
    }
};