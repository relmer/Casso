#include "Pch.h"

#include "HResultAssert.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Shell/DriveStatusPublisher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kHoldSlot     = 6;
static constexpr int     s_kHoldDrive    = 0;
static constexpr size_t  s_kHoldPattern  = 41;
static constexpr Byte    s_kHoldHighBit  = 0x80;
static constexpr size_t  s_kHoldBitIndex = 100;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFlushHoldTests
//
//  The disk store's flush hold, which reverse execution turns on while it
//  records. The automatic flushes (the motor stopping, a reset, a power cycle)
//  write nothing under it; an eject, a machine switch, exit and a commit still
//  save by default. A store marked as replaying writes nothing at all.
//  Discard puts the file's contents back, and a disk that leaves its bay is
//  kept while retention is on.
//
//  Every read and write of an image file goes through the store's seams.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFlushHoldTests)
{
public:

    struct Files
    {
        size_t             writes = 0;
        std::vector<Byte>  last;
    };


    TEST_METHOD (HoldSkipsTheAutomaticFlushes)
    {
        auto   store = std::make_unique<DiskImageStore>();
        Files  files;



        PrepareDirtyDisk (*store, files);

        store->SetFlushHold (true);

        AssertSucceeded (store->FlushAllUnlessHeld(), L"FlushAllUnlessHeld");
        store->SoftReset();

        Assert::AreEqual<size_t> (0, files.writes, L"the motor stopping and a reset wrote nothing");
        Assert::IsTrue (store->HasUnsavedWrites(), L"the writes are held");

        store->SetFlushHold (false);

        AssertSucceeded (store->FlushAllUnlessHeld(), L"FlushAllUnlessHeld after the hold");

        Assert::AreEqual<size_t> (1, files.writes, L"without the hold the automatic flush writes");
    }


    //  A disk whose writes cannot be stored in its format gets no recovery
    //  copy under SetFlushHold: the save writes the copy, and
    //  FlushAllUnlessHeld skips the save.
    TEST_METHOD (AHeldFlushWritesNoRecoveryCopy)
    {
        DiskImageStore  store;
        size_t          writes = 0;



        store.SetFlushSink ([&writes] (const std::string &, const std::vector<Byte> &)
        {
            writes++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (s_kHoldSlot, s_kHoldDrive, "C:\\disks\\Held.dsk", DiskFormat::Dsk,
                                               std::vector<Byte> (NibblizationLayer::kImageByteSize, 0x5A)),
                         L"MountFromBytes");
        BreakAddressChecksum (*store.GetImage (s_kHoldSlot, s_kHoldDrive));
        store.GetImage (s_kHoldSlot, s_kHoldDrive)->SetLoadedForTest (true, true);

        store.SetFlushHold (true);

        AssertSucceeded (store.FlushAllUnlessHeld(), L"FlushAllUnlessHeld");
        store.SoftReset();

        Assert::AreEqual<size_t> (0, writes, L"no recovery copy and no save");
        Assert::IsTrue (store.HasUnsavedWrites(), L"the writes are unsaved");
    }


    //  The owner's answer: held writes are saved by default when the disk
    //  leaves, the machine is switched, or the emulator exits.
    TEST_METHOD (HoldStillSavesOnEjectSwitchAndExit)
    {
        auto   store = std::make_unique<DiskImageStore>();
        Files  files;



        PrepareDirtyDisk (*store, files);
        store->SetFlushHold (true);

        AssertSucceeded (store->FlushAll(), L"FlushAll, as a machine switch does");
        Assert::AreEqual<size_t> (1, files.writes, L"a machine switch saves");

        Dirty (*store);
        AssertSucceeded (store->FlushAllForShutdown(), L"FlushAllForShutdown");
        Assert::AreEqual<size_t> (2, files.writes, L"exit saves");

        Dirty (*store);
        AssertSucceeded (store->CommitHeldWrites(), L"CommitHeldWrites");
        Assert::AreEqual<size_t> (3, files.writes, L"a commit saves");

        Dirty (*store);
        store->Eject (s_kHoldSlot, s_kHoldDrive);
        Assert::AreEqual<size_t> (4, files.writes, L"an eject saves");
        Assert::IsFalse (store->HasUnsavedWrites());
    }


    TEST_METHOD (ReplayingStoreWritesNothingAtAll)
    {
        auto   store = std::make_unique<DiskImageStore>();
        Files  files;



        PrepareDirtyDisk (*store, files);

        store->SetReplaying (true);

        AssertSucceeded (store->FlushAll(), L"FlushAll");
        AssertSucceeded (store->FlushAllForShutdown(), L"FlushAllForShutdown");
        store->SoftReset();

        Assert::AreEqual<size_t> (0, files.writes, L"no flush reaches a file during a replay");
        Assert::IsTrue (store->HasUnsavedWrites(), L"and the writes are still there afterward");
    }


    TEST_METHOD (DiscardPutsTheFileContentsBack)
    {
        auto               store    = std::make_unique<DiskImageStore>();
        Files              files;
        std::vector<Byte>  original = MakeImage();
        std::vector<Byte>  after;



        PrepareDirtyDisk (*store, files);
        store->SetFlushHold (true);

        AssertSucceeded (store->DiscardHeldWrites(), L"DiscardHeldWrites");

        Assert::IsFalse (store->HasUnsavedWrites(), L"nothing is held any more");
        Assert::AreEqual<size_t> (0, files.writes, L"and nothing was written");

        AssertSucceeded (store->GetImage (s_kHoldSlot, s_kHoldDrive)->Serialize (after), L"Serialize");
        Assert::IsTrue (original == after, L"the disk holds the file's contents again");
    }


    TEST_METHOD (EjectedDiskIsKeptWhileRetained)
    {
        auto      store   = std::make_unique<DiskImageStore>();
        Files     files;
        uint64_t  mediaId = 0;
        uint64_t  now     = 0;
        bool      changed = false;



        PrepareDirtyDisk (*store, files);

        store->SetPositionSource (&now);
        store->SetMediaRetention (true);

        mediaId = store->GetMediaId (s_kHoldSlot, s_kHoldDrive);
        now     = 10;

        store->Eject (s_kHoldSlot, s_kHoldDrive);

        Assert::AreEqual<size_t> (1, store->GetRetainedMediaCount(), L"the ejected disk is kept");
        Assert::IsTrue (store->CanSeatMedia (s_kHoldSlot, s_kHoldDrive, mediaId));

        AssertSucceeded (store->SeatMedia (s_kHoldSlot, s_kHoldDrive, mediaId, changed), L"SeatMedia");

        Assert::IsTrue (changed);
        Assert::AreEqual<uint64_t> (mediaId, store->GetMediaId (s_kHoldSlot, s_kHoldDrive), L"the same disk is back");
        Assert::AreEqual<size_t> (0, store->GetRetainedMediaCount());

        AssertSucceeded (store->SeatMedia (s_kHoldSlot, s_kHoldDrive, 0, changed), L"SeatMedia to empty");

        Assert::AreEqual<size_t> (1, files.writes, L"only the eject wrote; seating moves disks without flushing");

        store->PruneRetainedMedia (now - 1);
        Assert::AreEqual<size_t> (1, store->GetRetainedMediaCount(), L"history before the eject still holds it");

        store->PruneRetainedMedia (now);
        Assert::AreEqual<size_t> (1, store->GetRetainedMediaCount(), L"history that starts where it left may still hold it");

        store->PruneRetainedMedia (now + 1);
        Assert::AreEqual<size_t> (0, store->GetRetainedMediaCount(), L"history that starts after it does not");
        Assert::IsFalse (store->CanSeatMedia (s_kHoldSlot, s_kHoldDrive, mediaId));
    }


    //  An eject, seating a retained disk and discarding unsaved writes each
    //  change the bay, and each publish after them has the bay as the store
    //  has it.
    TEST_METHOD (ThePublishedStatusFollowsSeatingDiscardAndEject)
    {
        auto                  store     = std::make_unique<DiskImageStore>();
        DriveStatusPublisher  publisher;
        Files                 files;
        uint64_t              mediaId   = 0;
        uint64_t              now       = 0;
        bool                  changed   = false;



        PrepareDirtyDisk (*store, files);

        store->SetPositionSource (&now);
        store->SetMediaRetention (true);
        mediaId = store->GetMediaId (s_kHoldSlot, s_kHoldDrive);

        (void) publisher.Publish (*store, nullptr, true);
        Assert::IsTrue (publisher.Take()->GetBay (s_kHoldSlot, s_kHoldDrive).isMounted, L"the disk is published");

        store->Eject (s_kHoldSlot, s_kHoldDrive);

        Assert::IsTrue (publisher.Publish (*store, nullptr, true) == DriveStatusChange::Bays, L"the eject is a bay change");
        Assert::IsFalse (publisher.Take()->GetBay (s_kHoldSlot, s_kHoldDrive).isMounted,     L"and the bay is published empty");

        AssertSucceeded (store->SeatMedia (s_kHoldSlot, s_kHoldDrive, mediaId, changed), L"SeatMedia");

        Assert::IsTrue (publisher.Publish (*store, nullptr, true) == DriveStatusChange::Bays, L"seating the disk is a bay change");
        Assert::IsTrue (publisher.Take()->GetBay (s_kHoldSlot, s_kHoldDrive).isMounted,      L"and the disk is published again");

        Dirty (*store);
        store->SetFlushHold (true);
        AssertSucceeded (store->DiscardHeldWrites(), L"DiscardHeldWrites");

        Assert::IsTrue (publisher.Publish (*store, nullptr, true) == DriveStatusChange::Bays, L"discarding is a bay change");
        Assert::IsTrue (publisher.Take()->GetBay (s_kHoldSlot, s_kHoldDrive) == store->GetBayStatus (s_kHoldSlot, s_kHoldDrive),
                        L"and the published bay is the store's");
    }

    TEST_METHOD (WithoutRetentionAnEjectedDiskIsGone)
    {
        auto   store = std::make_unique<DiskImageStore>();
        Files  files;



        PrepareDirtyDisk (*store, files);

        store->Eject (s_kHoldSlot, s_kHoldDrive);

        Assert::AreEqual<size_t> (0, store->GetRetainedMediaCount());
    }


private:

    static std::vector<Byte> MakeImage()
    {
        std::vector<Byte>  bytes (NibbleImageCodec::kNibImageSize, 0);
        size_t             i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * s_kHoldPattern) | s_kHoldHighBit);
        }

        return bytes;
    }


    //  A nibble image mounted from a file only the seams know, with one bit
    //  written by the guest.
    static void PrepareDirtyDisk (DiskImageStore & store, Files & files)
    {
        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([&files] (const std::string &, const std::vector<Byte> & bytes)
        {
            files.writes++;
            files.last = bytes;
            return S_OK;
        });

        AssertSucceeded (store.Mount (s_kHoldSlot, s_kHoldDrive, "hold.nib"), L"Mount");

        Dirty (store);
    }


    //  Breaks the checksum of the first address field on track 0, so the track
    //  no longer decodes and a sector format cannot store the disk.
    static void BreakAddressChecksum (DiskImage & image)
    {
        std::vector<Byte> &  bits = image.GetTrackBitsForWrite (0);
        size_t               i    = 0;
        bool                 done = false;



        for (i = 0; i + 10 < bits.size() && !done; i++)
        {
            if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0x96)
            {
                bits[i + 9]  = 0xAA;
                bits[i + 10] = 0xAA;
                done         = true;
            }
        }

        Assert::IsTrue (done, L"track 0 has an address field");
    }


    static void Dirty (DiskImageStore & store)
    {
        DiskImage  * image = store.GetImage (s_kHoldSlot, s_kHoldDrive);



        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (0, s_kHoldBitIndex, image->ReadBit (0, s_kHoldBitIndex) ^ 1);

        Assert::IsTrue (image->IsDirty(), L"the guest wrote to the disk");
    }
};
