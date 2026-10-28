#include "Pch.h"

#include "HResultAssert.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FluxTestImages.h"
#include "Shell/CpuManager.h"
#include "Shell/DeploySave.h"
#include "Shell/DriveStatusPublisher.h"
#include "Core/ThreadOwnership.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kHoldSlot     = 6;
static constexpr int     s_kHoldDrive    = 0;
static constexpr size_t  s_kHoldPattern  = 41;
static constexpr Byte    s_kHoldHighBit  = 0x80;
static constexpr size_t  s_kHoldBitIndex = 100;
static constexpr DWORD   s_kDeployWaitMs = 5000;
static constexpr int     s_kSettleMs     = 60;
static constexpr int     s_kPollMs       = 5;





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


    //  A .nib saves its clean tracks from the bytes it was last saved as, so a
    //  commit after an earlier save writes the track as the earlier save wrote it.
    TEST_METHOD (ACommitAfterAnEarlierSaveKeepsTheTrackItSaved)
    {
        auto               store = std::make_unique<DiskImageStore>();
        Files              files;
        std::vector<Byte>  first;



        PrepareDirtyDisk (*store, files);

        AssertSucceeded (store->FlushAll(), L"FlushAll");
        first = files.last;

        DirtyTrack (*store, 5);
        store->SetFlushHold (true);

        AssertSucceeded (store->FlushAllUnlessHeld(), L"FlushAllUnlessHeld");
        Assert::AreEqual<size_t> (1, files.writes, L"FlushAllUnlessHeld wrote nothing");

        AssertSucceeded (store->CommitHeldWrites(), L"CommitHeldWrites");

        Assert::AreEqual<size_t> (2, files.writes, L"the commit wrote");
        Assert::IsTrue (IsTrackEqual (files.last, first, 0), L"and track 0 is as the first save wrote it");
    }


    //  The same for a disk that left its bay and was seated again: its eject
    //  saved it, and its next save writes that save's tracks.
    TEST_METHOD (AReseatedDiskKeepsTheTrackItsEjectSaved)
    {
        auto               store   = std::make_unique<DiskImageStore>();
        Files              files;
        std::vector<Byte>  first;
        uint64_t           mediaId = 0;
        uint64_t           now     = 0;
        bool               changed = false;



        PrepareDirtyDisk (*store, files);

        store->SetPositionSource (&now);
        store->SetMediaRetention (true);
        mediaId = store->GetMediaId (s_kHoldSlot, s_kHoldDrive);

        store->Eject (s_kHoldSlot, s_kHoldDrive);
        first = files.last;

        AssertSucceeded (store->SeatMedia (s_kHoldSlot, s_kHoldDrive, mediaId, changed), L"SeatMedia");
        DirtyTrack (*store, 5);

        AssertSucceeded (store->FlushAll(), L"FlushAll");

        Assert::AreEqual<size_t> (2, files.writes, L"the eject and the flush wrote");
        Assert::IsTrue (IsTrackEqual (files.last, first, 0), L"and track 0 is as the eject saved it");
    }


    //  A WOZ with a bit track at quarter track 0 and a flux track at quarter
    //  track 4, the flux track's bits all `fill`.
    static std::vector<Byte> MakeFluxWoz (Byte fill)
    {
        static constexpr size_t  kBits = 51200;

        WozSyntheticTrack  bitTrack;
        WozSyntheticTrack  fluxTrack;
        std::vector<Byte>  bits ((kBits + 7) / 8, fill);
        std::vector<Byte>  image;
        HRESULT            hr = S_OK;



        bitTrack.data.assign ((kBits + 7) / 8, 0xFF);
        bitTrack.bitCount      = kBits;
        bitTrack.quarterTracks = { 0 };

        fluxTrack.data          = FluxTestImages::BitsToNominalFlux (bits, kBits);
        fluxTrack.isFlux        = true;
        fluxTrack.quarterTracks = { 4 };

        hr = WozLoader::BuildSyntheticV21 ({ bitTrack, fluxTrack }, image);
        Assert::IsTrue (SUCCEEDED (hr), L"the flux WOZ builds");

        return image;
    }



    //  Puts the engine in write mode with a byte loaded and shifts it out for
    //  a while, so a flux write is open afterward.
    static void WriteSome (Disk2NibbleEngine & eng, uint32_t cycles)
    {
        eng.SetWriteMode     (true);
        eng.SetShiftLoadMode (true);
        eng.WriteLatch       (0xD5);
        eng.Tick             (8);
        eng.SetShiftLoadMode (false);
        eng.Tick             (cycles);
    }



    //  The flux track of `bytes`, loaded.
    static std::vector<Byte> FluxTrackOf (const std::vector<Byte> & bytes)
    {
        DiskImage  disk;
        HRESULT    hr = S_OK;



        hr = WozLoader::Load (bytes, disk);
        Assert::IsTrue (SUCCEEDED (hr), L"the WOZ loads");

        return disk.GetFluxTrack (1).GetBytes();
    }


    //  A discard throws away an open flux write too: the
    //  disk is the file as it is afterwards.
    TEST_METHOD (DiscardTakesAnOpenFluxWriteWithIt)
    {
        DiskImageStore     store;
        Disk2NibbleEngine  eng;
        std::vector<Byte>  file   = MakeFluxWoz (0xFF);
        size_t             writes = 0;



        store.SetImageReader    ([&file] (const std::string &, std::vector<Byte> & bytes) { bytes = file; return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([&writes] (const std::string &, const std::vector<Byte> &) { writes++; return S_OK; });
        store.SetBayChangeSink  ([&store, &eng] (int, int, BayChange) { eng.SetDiskImage (store.GetImage (s_kHoldSlot, s_kHoldDrive)); });

        AssertSucceeded (store.Mount (s_kHoldSlot, s_kHoldDrive, "hold.woz"), L"Mount");
        eng.SetDiskImage    (store.GetImage (s_kHoldSlot, s_kHoldDrive));
        eng.SetCurrentTrack (4);
        eng.SetMotorOn      (true);

        WriteSome (eng, 400);

        store.SetFlushHold (true);
        AssertSucceeded (store.DiscardHeldWrites(), L"DiscardHeldWrites");

        eng.SetWriteMode (false);

        Assert::IsFalse (store.GetImage (s_kHoldSlot, s_kHoldDrive)->IsDirty(), L"the write was discarded");
        Assert::IsTrue (store.GetImage (s_kHoldSlot, s_kHoldDrive)->GetFluxTrack (1).GetBytes() == FluxTrackOf (file),
                        L"and the disk is the file as it is");
        Assert::AreEqual<size_t> (0, writes, L"nothing was saved");
    }



    //  A mount that fails retires nothing and saves nothing.
    TEST_METHOD (AFailedMountRetiresNothingWhileRetained)
    {
        auto      store = std::make_unique<DiskImageStore>();
        Files     files;
        uint64_t  now   = 0;



        PrepareDirtyDisk (*store, files);
        store->SetPositionSource (&now);
        store->SetMediaRetention (true);

        AssertFailed (store->MountFromBytes (s_kHoldSlot, s_kHoldDrive, "junk.dsk", DiskFormat::Dsk, std::vector<Byte> (16, 0)),
                      L"bytes that do not load");

        Assert::AreEqual<size_t> (0, store->GetRetainedMediaCount(), L"nothing is retired");
        Assert::AreEqual<size_t> (0, files.writes, L"and nothing is saved");
        Assert::IsTrue (store->HasUnsavedWrites(), L"the disk with its writes is in the drive");
    }


    //  A mount declined for unsaved writes is no event for the history.
    TEST_METHOD (DeclinedMountForUnsavedWritesRecordsNothing)
    {
        DiskImageStore  store;
        uint64_t        now     = 0;
        uint64_t        mediaId = 0;
        int             changes = 0;



        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([] (const std::string &, const std::vector<Byte> &) { return HRESULT_FROM_WIN32 (ERROR_DISK_FULL); });

        AssertSucceeded (store.Mount (s_kHoldSlot, s_kHoldDrive, "hold.nib"), L"Mount");
        Dirty (store);

        store.SetPositionSource      (&now);
        store.SetMediaRetention      (true);
        store.SetMediaChangeListener ([&changes] { changes++; });
        mediaId = store.GetMediaId (s_kHoldSlot, s_kHoldDrive);

        AssertFailed (store.MountFromBytes (s_kHoldSlot, s_kHoldDrive, "other.nib", DiskFormat::Nib, MakeImage()),
                      L"the insert is declined");

        Assert::AreEqual (0, changes, L"no media change");
        Assert::AreEqual<size_t> (0, store.GetRetainedMediaCount(), L"nothing retired");
        Assert::AreEqual<uint64_t> (mediaId, store.GetMediaId (s_kHoldSlot, s_kHoldDrive), L"the same disk");
    }


    //  After a save a replay skipped, the disk is dirty, and the mount over it
    //  is declined rather than retiring the writes.
    TEST_METHOD (ReplaySkippedSaveDoesNotLetAMountRetireTheDisk)
    {
        auto          store = std::make_unique<DiskImageStore>();
        Files         files;
        DiskImage   * image = nullptr;



        PrepareDirtyDisk (*store, files);
        image = store->GetImage (s_kHoldSlot, s_kHoldDrive);

        store->SetReplaying (true);

        AssertFailed (store->MountFromBytes (s_kHoldSlot, s_kHoldDrive, "other.nib", DiskFormat::Nib, MakeImage()),
                      L"the mount is declined");

        Assert::IsTrue (store->GetImage (s_kHoldSlot, s_kHoldDrive) == image, L"the disk is in the drive");
        Assert::IsTrue (image->IsDirty(), L"with its writes");
        Assert::AreEqual<size_t> (0, files.writes, L"and the replay wrote nothing");
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


    //  An update installed now saves every disk with the CPU thread held.
    TEST_METHOD (DeployFlushLosesNoGuestWrite)
    {
        RunDeployFlush (false);
    }


    //  The deploy flush saves as quit does, so writes the automatic flushes
    //  skip under SetFlushHold are saved too.
    TEST_METHOD (DeployFlushStillSavesHeldWrites)
    {
        RunDeployFlush (true);
    }


private:

    //  The guest writes on every frame of a running CPU thread, and an eject
    //  is posted after the deploy flush. The flush holds the thread first,
    //  so the one write it makes has every guest write, no write lands after
    //  it, and the eject waits for the hold to end.
    static void RunDeployFlush (bool isFlushHeld)
    {
        auto                 store      = std::make_unique<DiskImageStore>();
        ThreadOwnership    & ownership  = store->GetThreadOwnership();
        CpuManager           cpu;
        Files                files;
        DeployFlushResult    result;
        std::atomic<int>     frames     = 0;
        std::atomic<bool>    isEjected  = false;
        bool                 isDirty    = true;
        bool                 isMounted  = false;
        HRESULT              hr         = S_OK;



        PrepareDirtyDisk (*store, files);
        store->SetFlushHold (isFlushHeld);
        ownership.Release();

        cpu.SetDeployHoldHooks ([&] { ownership.Release(); }, [&] { ownership.Claim(); });

        hr = cpu.Start ([&] { ownership.Claim(); },
                        [&] (const EmulatorCommand & cmd)
                        {
                            if (cmd.id == IDM_DISK_EJECT1)
                            {
                                store->Eject (s_kHoldSlot, s_kHoldDrive);
                                isEjected = true;
                            }
                        },
                        [&]
                        {
                            DiskImage  * image = store->GetImage (s_kHoldSlot, s_kHoldDrive);

                            if (image != nullptr)
                            {
                                image->WriteBit (0, s_kHoldBitIndex, image->ReadBit (0, s_kHoldBitIndex) ^ 1);
                            }

                            frames++;
                        },
                        [&] { ownership.Release(); });
        AssertSucceeded (hr, L"Start");

        WaitFor ([&] { return frames.load() > 2; });

        hr = DeploySave::Flush (cpu, *store, s_kDeployWaitMs, result);
        AssertSucceeded (hr, L"DeploySave::Flush");
        Assert::AreEqual<size_t> (1, files.writes, L"the flush wrote the disk once");
        Assert::AreEqual (isFlushHeld, result.wasBehindLive, L"and reports SetFlushHold as behind live");

        cpu.PostCommand (IDM_DISK_EJECT1);
        std::this_thread::sleep_for (std::chrono::milliseconds (s_kSettleMs));

        isDirty   = store->GetImage (s_kHoldSlot, s_kHoldDrive)->IsDirty();
        isMounted = store->IsMounted (s_kHoldSlot, s_kHoldDrive);

        DeploySave::Resume (cpu, *store, result);
        WaitFor ([&] { return isEjected.load(); });

        cpu.Stop();
        ownership.Claim();

        Assert::IsFalse (isDirty,           L"no guest write landed after the flush");
        Assert::IsTrue  (isMounted,         L"the eject posted during the hold did not run");
        Assert::IsTrue  (isEjected.load(),  L"and runs once the hold ends");
        Assert::AreEqual<size_t> (1, files.writes, L"with nothing left to write");
    }


    template <typename Predicate>
    static void WaitFor (Predicate isDone)
    {
        auto  deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (s_kDeployWaitMs);



        while (!isDone() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (s_kPollMs));
        }
    }

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


    //  The guest writes one bit of `track`.
    static void DirtyTrack (DiskImageStore & store, int track)
    {
        DiskImage  * image = store.GetImage (s_kHoldSlot, s_kHoldDrive);



        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (track, s_kHoldBitIndex, image->ReadBit (track, s_kHoldBitIndex) ^ 1);
    }


    static bool IsTrackEqual (const std::vector<Byte> & left, const std::vector<Byte> & right, int track)
    {
        size_t  first = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;



        return std::equal (left.begin()  + first, left.begin() + first + NibbleImageCodec::kNibTrackSize,
                           right.begin() + first);
    }


    static void Dirty (DiskImageStore & store)
    {
        DiskImage  * image = store.GetImage (s_kHoldSlot, s_kHoldDrive);



        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (0, s_kHoldBitIndex, image->ReadBit (0, s_kHoldBitIndex) ^ 1);

        Assert::IsTrue (image->IsDirty(), L"the guest wrote to the disk");
    }
};
