#include "Pch.h"

#include "../EhmTestHelper.h"
#include "TestMachine.h"
#include "HResultAssert.h"
#include "FakeDiskFileIo.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DriveStatus.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Shell/DriveStatusPublisher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kStatusSlot     = 6;
static constexpr int     s_kStatusDrive    = 0;
static constexpr Word    s_kMotorOn        = 0xC0E9;
static constexpr size_t  s_kStatusBitIndex = 120;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatusPublisherTests
//
//  The drive status published for the window: built on the thread that has
//  the disk store's token, published whole, and rebuilt only when the store's
//  status generation moves.
//
//  Every read and write of an image file goes through the store's seams, into
//  the rig's own file table.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveStatusPublisherTests)
{
public:

    //  A machine whose store reads, writes and stats files in memory. Each
    //  write moves the written file's modified time, as a real write does.
    struct Rig
    {
        TestMachine                                     machine { "Apple2e" };
        DiskImageStore                                & store   = machine.GetDiskStore();
        FakeDiskFileIo                                  fileIo;
        std::map<std::string, std::vector<Byte>>        files;
        std::map<std::string, int64_t>                  modified;
        DriveStatusPublisher                            publisher;

        Rig()
        {
            store.SetFileIo         (&fileIo);
            store.SetImageReader    ([this] (const std::string & path, std::vector<Byte> & bytes) { bytes = files[path]; return S_OK; });
            store.SetIdentityReader ([this] (const std::string & path) { return Identity (path); });
            store.SetFlushSink      ([this] (const std::string & path, const std::vector<Byte> & bytes) { Write (path, bytes); return S_OK; });
        }

        void  Write (const std::string & path, const std::vector<Byte> & bytes)
        {
            files[path]          = bytes;
            fileIo.files[path]   = bytes;
            modified[path]++;
        }

        ImageIdentity  Identity (const std::string & path)
        {
            ImageIdentity  identity;

            identity.recorded           = files.count (path) != 0;
            identity.stamp.sizeBytes    = files[path].size();
            identity.stamp.modifiedUnix = modified[path];
            return identity;
        }

        DriveStatusChange  Publish (bool isActivityDue = true)
        {
            return publisher.Publish (store, machine.GetRefs().diskController, isActivityDue);
        }
    };


    TEST_METHOD (TakeBeforeAnyPublishIsAnEmptyMachine)
    {
        DriveStatusPublisher                publisher;
        std::shared_ptr<const DriveStatus>  status       = publisher.Take();
        bool                                isAnyMounted = false;
        int                                 slot         = 0;
        int                                 drive        = 0;



        Assert::IsTrue (status != nullptr, L"Take is never null");

        for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
        {
            for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
            {
                isAnyMounted = isAnyMounted || status->GetBay (slot, drive).isMounted;
            }
        }

        Assert::IsFalse (isAnyMounted,                             L"every bay is empty");
        Assert::IsFalse (status->HasController(),                  L"there is no controller");
        Assert::AreEqual (-1, status->GetActivity (0).headQuarterTrack, L"and no head position");
    }


    TEST_METHOD (PublishWithNothingChangedPublishesNothing)
    {
        Rig                                 rig;
        std::shared_ptr<const DriveStatus>  first;



        Assert::IsTrue (rig.Publish() == DriveStatusChange::Bays, L"the first publish builds the bays");
        first = rig.publisher.Take();

        Assert::IsTrue (rig.Publish() == DriveStatusChange::None, L"nothing changed");
        Assert::IsTrue (rig.publisher.Take() == first,             L"so the same status is still published");
    }


    TEST_METHOD (AnActivityChangeRepublishesAndSharesTheBays)
    {
        Rig                                 rig;
        std::shared_ptr<const DriveStatus>  before;
        std::shared_ptr<const DriveStatus>  after;



        (void) rig.Publish();
        before = rig.publisher.Take();

        (void) rig.machine.GetRefs().diskController->Read (s_kMotorOn);

        Assert::IsTrue (rig.Publish() == DriveStatusChange::Activity, L"only activity changed");
        after = rig.publisher.Take();

        Assert::AreNotEqual (before->GetSequence(), after->GetSequence(), L"a new status");
        Assert::IsTrue (after->GetActivity (s_kStatusDrive).isMotorOn,  L"with the motor on");
        Assert::IsTrue (&before->GetBay (s_kStatusSlot, s_kStatusDrive) == &after->GetBay (s_kStatusSlot, s_kStatusDrive),
                        L"sharing the bay table it had");
    }


    TEST_METHOD (AnActivityChangeNotDueWaitsForABayChangeToBringIt)
    {
        Rig                                 rig;
        std::shared_ptr<const DriveStatus>  before;
        std::vector<Byte>                   raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT                             hr = S_OK;



        (void) rig.Publish();
        before = rig.publisher.Take();

        (void) rig.machine.GetRefs().diskController->Read (s_kMotorOn);

        Assert::IsTrue (rig.Publish (false) == DriveStatusChange::None, L"activity alone waits when it is not due");
        Assert::IsTrue (rig.publisher.Take() == before,                  L"so nothing new is published");

        hr = rig.store.MountFromBytes (s_kStatusSlot, s_kStatusDrive, "status.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        Assert::IsTrue (rig.Publish (false) == DriveStatusChange::Bays,                    L"a bay change publishes");
        Assert::IsTrue (rig.publisher.Take()->GetActivity (s_kStatusDrive).isMotorOn,      L"with the activity as it is now");
    }


    TEST_METHOD (ABayChangeReportsBaysAndShowsTheDisk)
    {
        Rig                 rig;
        std::vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT             hr = S_OK;



        (void) rig.Publish();

        hr = rig.store.MountFromBytes (s_kStatusSlot, s_kStatusDrive, "status.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        Assert::IsTrue (rig.Publish() == DriveStatusChange::Bays, L"the mount is a bay change");

        const BayStatus &  bay = rig.publisher.Take()->GetBay (s_kStatusSlot, s_kStatusDrive);

        Assert::IsTrue  (bay.isMounted,                    L"the bay has the disk");
        Assert::AreEqual (std::string ("status.dsk"),      bay.path);
        Assert::IsTrue  (bay.format == DiskFormat::Dsk,    L"in its format");
    }


    //  A status the window took before an eject has its own copy of the bay,
    //  so it has the old disk's path and causes after the image is gone.
    TEST_METHOD (AHeldStatusOutlivesTheEjectAndTheNextPublish)
    {
        Rig                                 rig;
        std::vector<Byte>                   raw (NibblizationLayer::kImageByteSize, 0);
        std::shared_ptr<const DriveStatus>  taken;
        HRESULT                             hr = S_OK;



        hr = rig.store.MountFromBytes (s_kStatusSlot, s_kStatusDrive, "status.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        rig.store.SetUserWriteProtectFlag (s_kStatusSlot, s_kStatusDrive, true);
        (void) rig.Publish();
        taken = rig.publisher.Take();

        rig.store.Eject (s_kStatusSlot, s_kStatusDrive);
        (void) rig.Publish();

        Assert::AreEqual (std::string ("status.dsk"), taken->GetBay (s_kStatusSlot, s_kStatusDrive).path, L"the taken status keeps its path");
        Assert::IsTrue  (taken->GetBay (s_kStatusSlot, s_kStatusDrive).writeProtect.userSetting,          L"and its causes");
        Assert::IsFalse (rig.publisher.Take()->GetBay (s_kStatusSlot, s_kStatusDrive).isMounted,         L"while the newest status has the bay empty");
    }


    //  Every write to a bay moves the generation, and the next publish has the
    //  bay as the store has it.
    TEST_METHOD (EveryBayWriterMovesTheGeneration)
    {
        Rig                 rig;
        std::vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>   saved;
        uint64_t            mediaId = 0;
        bool                changed = false;
        HRESULT             hr      = S_OK;



        rig.Write ("C:\\work\\a.dsk",   raw);
        rig.Write ("C:\\work\\a.woz",   MakeWoz());

        Step (rig, L"MountFromBytes", [&] { hr = rig.store.MountFromBytes (s_kStatusSlot, s_kStatusDrive, "bytes.dsk", DiskFormat::Dsk, raw); AssertSucceeded (hr, L"MountFromBytes"); });
        Step (rig, L"Mount", [&] { hr = rig.store.Mount (s_kStatusSlot, s_kStatusDrive, "C:\\work\\a.dsk"); AssertSucceeded (hr, L"Mount"); });
        Step (rig, L"Eject", [&] { rig.store.Eject (s_kStatusSlot, s_kStatusDrive); });

        rig.store.SetMediaRetention (true);
        hr = rig.store.Mount (s_kStatusSlot, s_kStatusDrive, "C:\\work\\a.dsk");
        AssertSucceeded (hr, L"Mount for retention");
        mediaId = rig.store.GetMediaId (s_kStatusSlot, s_kStatusDrive);
        rig.store.Eject (s_kStatusSlot, s_kStatusDrive);

        Step (rig, L"SeatMedia, a retained disk", [&] { hr = rig.store.SeatMedia (s_kStatusSlot, s_kStatusDrive, mediaId, changed); AssertSucceeded (hr, L"SeatMedia"); });
        Step (rig, L"SeatMedia (0)", [&] { hr = rig.store.SeatMedia (s_kStatusSlot, s_kStatusDrive, 0, changed); AssertSucceeded (hr, L"SeatMedia (0)"); });
        rig.store.SetMediaRetention (false);

        hr = rig.store.Mount (s_kStatusSlot, s_kStatusDrive, "C:\\work\\a.woz");
        AssertSucceeded (hr, L"Mount the WOZ");

        Step (rig, L"SetImageWriteProtect", [&] { hr = rig.store.SetImageWriteProtect (s_kStatusSlot, s_kStatusDrive, true); AssertSucceeded (hr, L"SetImageWriteProtect"); });

        hr = rig.store.SetImageWriteProtect (s_kStatusSlot, s_kStatusDrive, false);
        AssertSucceeded (hr, L"SetImageWriteProtect off");

        Step (rig, L"a flush, which clears the CRC mismatch", [&] { Dirty (rig.store); hr = rig.store.Flush (s_kStatusSlot, s_kStatusDrive); AssertSucceeded (hr, L"Flush"); });

        Step (rig, L"a conflict flush, which moves the bay to the copy", [&]
        {
            Dirty (rig.store);
            rig.modified["C:\\work\\a.woz"]++;
            hr = rig.store.Flush (s_kStatusSlot, s_kStatusDrive);
            AssertSucceeded (hr, L"Flush over a changed file");
        });

        Assert::AreNotEqual (std::string ("C:\\work\\a.woz"), rig.store.GetSourcePath (s_kStatusSlot, s_kStatusDrive), L"the conflict moved the bay");

        Step (rig, L"DiscardHeldWrites", [&]
        {
            Dirty (rig.store);
            rig.store.SetFlushHold (true);
            hr = rig.store.DiscardHeldWrites();
            AssertSucceeded (hr, L"DiscardHeldWrites");
            rig.store.SetFlushHold (false);
        });

        Step (rig, L"SetUserWriteProtectFlag", [&] { rig.store.SetUserWriteProtectFlag (s_kStatusSlot, s_kStatusDrive, true); });
        Step (rig, L"SetFileWriteProtect",     [&] { rig.store.SetFileWriteProtect (s_kStatusSlot, s_kStatusDrive, true, false); });

        saved = Save (rig.machine);
        rig.store.SetUserWriteProtectFlag (s_kStatusSlot, s_kStatusDrive, false);
        (void) rig.Publish();

        Step (rig, L"MachineHost::LoadState", [&] { hr = Load (rig.machine, saved); AssertSucceeded (hr, L"LoadState"); });
    }


    //  The Debug verifier: an image flag written past the store does not move
    //  the generation, and the next publish asserts.
    TEST_METHOD (AWriteThatSkipsTheGenerationAssertsInDebug)
    {
#if defined(_DEBUG)
        Rig                 rig;
        std::vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT             hr = S_OK;



        hr = rig.store.MountFromBytes (s_kStatusSlot, s_kStatusDrive, "status.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        (void) rig.Publish();

        rig.store.GetImage (s_kStatusSlot, s_kStatusDrive)->SetUserWriteProtected (true);

        {
            UnitTestHelpers::ExpectedEhmAssert  expect;

            (void) rig.Publish();
        }
#endif
    }


    TEST_METHOD (MountedSourcesMatchTheStore)
    {
        Rig                                          rig;
        std::vector<Byte>                            raw (NibblizationLayer::kImageByteSize, 0);
        std::vector<DiskImageStore::MountedSource>   expected;
        std::vector<DiskImageStore::MountedSource>   actual;
        size_t                                       i  = 0;
        HRESULT                                      hr = S_OK;



        hr = rig.store.MountFromBytes (s_kStatusSlot, 0, "one.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes one");
        hr = rig.store.MountFromBytes (s_kStatusSlot, 1, "", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes with no path");
        hr = rig.store.MountFromBytes (5, 0, "two.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes two");

        (void) rig.Publish();

        expected = rig.store.GetMountedSourcePaths();
        actual   = rig.publisher.Take()->GetMountedSources();

        Assert::AreEqual (expected.size(), actual.size(), L"the same mounted sources");

        for (i = 0; i < expected.size(); i++)
        {
            Assert::AreEqual (expected[i].path,  actual[i].path);
            Assert::AreEqual (expected[i].slot,  actual[i].slot);
            Assert::AreEqual (expected[i].drive, actual[i].drive);
        }
    }


private:

    //  One writer, then a publish: the generation moved, the publish reports
    //  a bay change, and the published bay equals the store's.
    static void Step (Rig & rig, const wchar_t * name, const std::function<void()> & write)
    {
        uint64_t           before = 0;
        DriveStatusChange  change = DriveStatusChange::None;



        (void) rig.Publish();
        before = rig.store.GetStatusGeneration();

        write();

        change = rig.Publish();

        Assert::AreNotEqual (before, rig.store.GetStatusGeneration(),                                            (std::wstring (name) + L": the generation moved").c_str());
        Assert::IsTrue (change == DriveStatusChange::Bays,                                                       (std::wstring (name) + L": the publish reports a bay change").c_str());
        Assert::IsTrue (rig.publisher.Take()->GetBay (s_kStatusSlot, s_kStatusDrive) == rig.store.GetBayStatus (s_kStatusSlot, s_kStatusDrive),
                        (std::wstring (name) + L": the published bay is the store's").c_str());
    }


    static void Dirty (DiskImageStore & store)
    {
        DiskImage  * image = store.GetImage (s_kStatusSlot, s_kStatusDrive);



        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (0, s_kStatusBitIndex, image->ReadBit (0, s_kStatusBitIndex) ^ 1);
    }


    static std::vector<Byte> MakeWoz()
    {
        DiskImage          src;
        std::vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  woz;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, src));
        src.SetSourceFormat (DiskFormat::Woz);
        AssertSucceeded (WozLoader::Serialize (src, woz));

        return woz;
    }


    static std::vector<Byte> Save (const MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        return writer.GetBytes();
    }


    static HRESULT Load (MachineHost & machine, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);



        return machine.LoadState (reader);
    }
};
