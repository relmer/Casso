#include "Pch.h"
#include "DiskStoreTestSupport.h"
#include "DeferredDiskIoQueue.h"
#include "EmuTests/TestMachine.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIoQueueStoreTests
//
//  The store's saves through an I/O queue that runs nothing until the test
//  runs it, so each test can look at the store between the post, the work
//  and the completion.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIoQueueStoreTests), public DiskStoreTestSupport
{
public:

    static constexpr const char *  kPath = "queued.woz";



    //  A store on the deferred queue, with a dirty WOZ mounted and every write
    //  captured. A WOZ serializes bit for bit, so a guest write is in the bytes.
    struct Rig
    {
        DiskImageStore                store;
        DeferredDiskIoQueue           queue;
        std::vector<vector<Byte>>     writes;
        HRESULT                       hrSink = S_OK;

        explicit Rig (const vector<Byte> & woz)
        {
            store.SetFlushSink ([this] (const string & path, const vector<Byte> & bytes)
            {
                if (path == kPath && SUCCEEDED (hrSink))
                {
                    writes.push_back (bytes);
                }

                return hrSink;
            });

            AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kPath, DiskFormat::Woz, woz));
            store.SetIoQueue (&queue);
            GuestWrite();
        }

        DiskImage & Image() { return *store.GetImage (kSlot, kDrive); }

        //  A guest write: a track's bits change, and the image is dirty.
        void GuestWrite()
        {
            Image().GetTrackBitsForWrite (0)[0] ^= 0x01;
            Image().SetLoadedForTest (true, true);
        }
    };



    TEST_METHOD (ASpinDownSaveIsPostedAndTheDiskIsDirtyUntilItsCompletion)
    {
        auto  rig = std::make_unique<Rig> (MakeWoz());



        rig->store.SaveAllUnlessHeld();

        Assert::AreEqual ((size_t) 1, rig->queue.GetPostedCount(), L"the save is posted");
        Assert::IsTrue   (rig->writes.empty(),    L"and nothing is written before its work runs");
        Assert::IsTrue   (rig->Image().IsDirty(), L"the disk is dirty");

        rig->queue.RunNextWork();

        Assert::AreEqual ((size_t) 1, rig->writes.size(), L"the work writes the file");
        Assert::IsTrue   (rig->Image().IsDirty(), L"and the disk is dirty until the completion");

        rig->store.DrainIo();

        Assert::IsFalse  (rig->Image().IsDirty(), L"the completion marks it saved");
    }


    //  The bytes a save took are older than a guest write made before its
    //  completion, so the disk stays dirty, and the next save writes it.
    TEST_METHOD (AGuestWriteDuringASaveKeepsTheDiskDirty)
    {
        auto  rig = std::make_unique<Rig> (MakeWoz());



        rig->store.SaveAllUnlessHeld();
        rig->queue.RunNextWork();

        rig->GuestWrite();
        rig->store.DrainIo();

        Assert::IsTrue (rig->Image().IsDirty(), L"the write after the bytes were taken is not saved yet");

        rig->store.SaveAllUnlessHeld();
        rig->queue.RunAll();

        Assert::AreEqual ((size_t) 2, rig->writes.size(), L"a second save wrote it");
        Assert::IsFalse  (rig->writes[0] == rig->writes[1], L"with the newer bytes");
        Assert::IsFalse  (rig->Image().IsDirty(), L"and the disk is saved");
    }


    //  One save of a disk at a time: a save requested while one is in flight
    //  is posted when that one is over, so the two land in order.
    TEST_METHOD (ASaveAskedForDuringASaveRunsAfterIt)
    {
        auto  rig = std::make_unique<Rig> (MakeWoz());



        rig->store.SaveAllUnlessHeld();
        rig->GuestWrite();
        rig->store.SaveAllUnlessHeld();

        Assert::AreEqual ((size_t) 1, rig->queue.GetPostedCount(), L"the second save waits rather than posting");

        rig->queue.RunAll();

        Assert::AreEqual ((size_t) 2, rig->writes.size(), L"both saves wrote");
        Assert::IsFalse  (rig->Image().IsDirty(), L"and the last one has the guest's last write");
    }


    TEST_METHOD (AFailedCommitIsReportedOnceFromTheCompletion)
    {
        ScopedFlushNotifyCapture  capture;
        auto                      rig     = std::make_unique<Rig> (MakeWoz());



        rig->hrSink = HRESULT_FROM_WIN32 (ERROR_DISK_FULL);

        rig->store.SaveAllUnlessHeld();
        rig->queue.RunNextWork();

        Assert::AreEqual (0, s_flushNotifyCount, L"nothing is reported before the completion");

        rig->store.DrainIo();

        Assert::AreEqual (1, s_flushNotifyCount, L"the completion reports the failure");
        Assert::IsTrue   (rig->Image().IsDirty(), L"and the disk keeps its writes");

        rig->store.SaveAllUnlessHeld();
        rig->queue.RunAll();

        Assert::AreEqual (1, s_flushNotifyCount, L"a second spin-down's failure is not reported again");
    }


    //  A disk taken out of its bay without a save, as reverse execution's
    //  SeatMedia does, before its save completes: no bay has that disk at the
    //  completion, and nothing is applied.
    TEST_METHOD (ACompletionForADiskThatLeftItsBayAppliesNothing)
    {
        auto  rig     = std::make_unique<Rig> (MakeWoz());
        bool  changed = false;



        rig->store.SetMediaRetention (true);
        rig->store.SaveAllUnlessHeld();
        rig->queue.RunNextWork();

        AssertSucceeded (rig->store.SeatMedia (kSlot, kDrive, 0, changed));
        Assert::IsTrue  (changed, L"the bay is empty");

        rig->store.DrainIo();

        Assert::IsFalse  (rig->store.IsMounted (kSlot, kDrive), L"the completion put nothing back");
        Assert::AreEqual ((size_t) 1, rig->store.GetRetainedMediaCount(), L"and the disk is kept as it left");
    }


    //  A caller that uses the result waits for the save it posts.
    TEST_METHOD (AFlushOnAQueueThatDefersWaitsForTheSave)
    {
        auto     rig = std::make_unique<Rig> (MakeWoz());
        HRESULT  hr  = S_OK;



        hr = rig->store.Flush (kSlot, kDrive);

        AssertSucceeded (hr, L"Flush");
        Assert::AreEqual ((size_t) 1, rig->writes.size(), L"the file was written before Flush returned");
        Assert::IsFalse  (rig->Image().IsDirty(), L"and the disk is saved");
    }


    //  The eject's save is posted, and the bay is emptied in its completion.
    TEST_METHOD (AnEjectEmptiesTheBayOnlyAfterItsSaveCompletes)
    {
        auto  rig       = std::make_unique<Rig> (MakeWoz());
        int   calls     = 0;
        bool  isEjected = false;



        rig->store.Eject (kSlot, kDrive, [&calls, &isEjected] (bool ejected) { calls++; isEjected = ejected; });

        Assert::IsTrue   (rig->store.IsMounted (kSlot, kDrive), L"the disk is in the drive while its save is posted");
        rig->queue.RunNextWork();
        Assert::IsTrue   (rig->store.IsMounted (kSlot, kDrive), L"and after the save's work");

        rig->store.DrainIo();

        Assert::AreEqual ((size_t) 1, rig->writes.size(), L"the disk was saved");
        Assert::IsFalse  (rig->store.IsMounted (kSlot, kDrive), L"the completion emptied the bay");
        Assert::AreEqual (1, calls,     L"done ran once");
        Assert::IsTrue   (isEjected,    L"with the bay empty");
    }


    TEST_METHOD (AnEjectWhoseSaveFailsKeepsTheDisk)
    {
        ScopedFlushNotifyCapture  capture;
        auto                      rig       = std::make_unique<Rig> (MakeWoz());
        bool                      isEjected = true;



        rig->hrSink = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

        rig->store.Eject (kSlot, kDrive, [&isEjected] (bool ejected) { isEjected = ejected; });
        rig->queue.RunAll();

        Assert::IsTrue  (rig->store.IsMounted (kSlot, kDrive), L"the disk is in the drive");
        Assert::IsFalse (isEjected, L"and done says so");
        Assert::IsTrue  (rig->Image().IsDirty(), L"with its writes");
    }


    //  A mount over a dirty bay, in its order: the new file is read, the disk
    //  in the drive is saved, and the two are swapped, each step after the
    //  one before it has completed.
    TEST_METHOD (AMountOverADirtyBayReadsThenSavesThenSwaps)
    {
        static constexpr const char *  kOther = "other.woz";

        auto            rig    = std::make_unique<Rig> (MakeWoz());
        vector<Byte>    other  = MakeWoz (true);
        HRESULT         result = E_PENDING;
        int             reads  = 0;



        rig->store.SetImageReader ([&other, &reads] (const string & path, vector<Byte> & bytes)
        {
            reads++;
            bytes = other;
            return (path == kOther) ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        });

        rig->store.Mount (kSlot, kDrive, kOther, [&result] (HRESULT hr, const MountDiagnosis &) { result = hr; });

        Assert::AreEqual (0, reads, L"nothing is read before the work runs");
        rig->queue.RunNextWork();
        Assert::AreEqual (1, reads, L"the work reads the new file");

        rig->store.DrainIo();
        Assert::IsTrue   (rig->writes.empty(), L"the save is posted after the read completes");
        Assert::AreEqual ((size_t) 1, rig->queue.GetPostedCount(), L"as a second job");

        rig->queue.RunNextWork();
        Assert::AreEqual ((size_t) 1, rig->writes.size(), L"the save writes the disk in the drive");
        Assert::AreEqual (std::string (kPath), rig->store.GetSourcePath (kSlot, kDrive), L"which is in the drive until the completion");

        rig->store.DrainIo();
        Assert::AreEqual (S_OK, result, L"the mount succeeds");
        Assert::AreEqual (std::string (kOther), rig->store.GetSourcePath (kSlot, kDrive), L"and the new disk is in the drive");
    }


    TEST_METHOD (AMountOverABayWhoseSaveFailsDeclines)
    {
        static constexpr const char *  kOther = "other.woz";

        auto            rig    = std::make_unique<Rig> (MakeWoz());
        vector<Byte>    other  = MakeWoz (true);
        HRESULT         result = S_OK;
        MountDiagnosis  seen;



        rig->hrSink = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);
        rig->store.SetImageReader ([&other] (const string &, vector<Byte> & bytes) { bytes = other; return S_OK; });

        rig->store.Mount (kSlot, kDrive, kOther, [&result, &seen] (HRESULT hr, const MountDiagnosis & diagnosis) { result = hr; seen = diagnosis; });
        rig->queue.RunAll();

        Assert::IsTrue   (FAILED (result), L"the mount declines");
        Assert::IsTrue   (seen.failure == MountFailure::UnsavedWrites, L"because the disk in the drive has writes outside a file");
        Assert::AreEqual (std::string (kPath), rig->store.GetSourcePath (kSlot, kDrive), L"which is in the drive");
    }


    //  A settled change to the bay's file, for which the writer stated a
    //  reload: the identity check and the read
    //  are one job, and while it is in flight the reload pass, which runs
    //  every frame, does not post a second one.
    TEST_METHOD (AReloadReadsTheChangedFileInOneJobAtATime)
    {
        static constexpr const char *  kFile = "reloaded.dsk";

        auto                 store    = std::make_unique<DiskImageStore>();
        DeferredDiskIoQueue  queue;
        vector<Byte>         file     = vector<Byte> (NibblizationLayer::kImageByteSize, 0x11);
        ImageIdentity        identity;
        int64_t              nowMs    = 0;
        int                  reads    = 0;
        vector<Byte>         before;



        identity.recorded           = true;
        identity.stamp.sizeBytes    = file.size();
        identity.stamp.modifiedUnix = 100;

        store->SetClock          ([&nowMs] { return nowMs; });
        store->SetImageReader    ([&file, &reads] (const string &, vector<Byte> & bytes) { reads++; bytes = file; return S_OK; });
        store->SetIdentityReader ([&identity] (const string &) { return identity; });
        store->SetFlushSink      ([] (const string &, const vector<Byte> &) { return S_OK; });

        AssertSucceeded (store->Mount (kSlot, kDrive, kFile));
        before = store->GetImage (kSlot, kDrive)->GetTrackBits (0);

        store->SetIoQueue (&queue);
        reads = 0;

        file                        = vector<Byte> (NibblizationLayer::kImageByteSize, 0x22);
        identity.stamp.modifiedUnix = 101;

        store->NoteExternalChange (kFile, ExternalChangeIntent::ReloadInPlace);
        nowMs += MountedImageState::kQuietPeriodMs;

        store->ApplyPendingReload();
        store->ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, queue.GetPostedCount(), L"one job for the bay");
        Assert::AreEqual (0, reads, L"and nothing is read on the store's thread");

        queue.RunNextWork();
        Assert::AreEqual (1, reads, L"the job reads the file");
        Assert::IsTrue   (before == store->GetImage (kSlot, kDrive)->GetTrackBits (0), L"and the disk in the drive is as mounted until the completion");

        store->DrainIo();
        Assert::IsFalse  (before == store->GetImage (kSlot, kDrive)->GetTrackBits (0), L"the completion reloads the changed file");

        store->SetIoQueue (nullptr);
    }


    //  Two mounts of one file, one into each drive, both posted before either
    //  has read it: the mount that swaps first puts the file in its drive, and
    //  the other mount declines at its swap.
    TEST_METHOD (AFileMountedIntoTheOtherDriveDuringAMountIsDeclinedAtTheSwap)
    {
        static constexpr const char *  kShared = "Q:/casso-no-such-root/shared.woz";

        auto                 store    = std::make_unique<DiskImageStore>();
        DeferredDiskIoQueue  queue;
        FakeDiskFileIo       io;
        HRESULT              first    = E_PENDING;
        HRESULT              second   = E_PENDING;
        MountDiagnosis       declined;



        UseFileIo (*store, io);
        io.SeedDurable (kShared, MakeWoz());
        store->SetIoQueue (&queue);

        store->Mount (kSlot, 0, kShared, [&first] (HRESULT hr, const MountDiagnosis &) { first = hr; });
        store->Mount (kSlot, 1, kShared, [&second, &declined] (HRESULT hr, const MountDiagnosis & diagnosis) { second = hr; declined = diagnosis; });
        queue.RunAll();

        AssertSucceeded (first, L"the first mount");
        Assert::IsTrue  (FAILED (second), L"the second declines");
        Assert::IsTrue  (declined.failure == MountFailure::AlreadyMounted, L"because the file is in the other drive");
        Assert::IsFalse (store->IsMounted (kSlot, 1), L"and that drive is empty");

        store->SetIoQueue (nullptr);
    }


    //  An eject takes the disk out of the drive before its save, as a real
    //  drive does, so the guest cannot write it while the save is posted.
    TEST_METHOD (AnEjectTakesTheDiskOutOfTheDriveBeforeItsSave)
    {
        TestMachine          machine    ("Apple2e");
        DiskImageStore     & store      = machine.GetDiskStore();
        Disk2Controller    * controller = machine.GetRefs().diskController;
        DeferredDiskIoQueue  queue;
        bool                 isEjected  = false;



        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return S_OK; });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kPath, DiskFormat::Woz, MakeWoz()));
        Assert::IsTrue  (controller->GetDisk (kDrive) == store.GetImage (kSlot, kDrive), L"the drive reads the disk");

        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        store.SetIoQueue (&queue);

        store.Eject (kSlot, kDrive, [&isEjected] (bool ejected) { isEjected = ejected; });

        Assert::IsTrue  (store.IsMounted (kSlot, kDrive), L"the disk is in its bay while its save is posted");
        Assert::IsFalse (controller->GetDisk (kDrive) == store.GetImage (kSlot, kDrive), L"and out of the drive");

        queue.RunAll();

        Assert::IsTrue  (isEjected, L"the eject empties the bay");

        store.SetIoQueue (nullptr);
    }


    TEST_METHOD (AnEjectWhoseSaveFailsPutsTheDiskBackInTheDrive)
    {
        ScopedFlushNotifyCapture  capture;
        TestMachine               machine    ("Apple2e");
        DiskImageStore          & store      = machine.GetDiskStore();
        Disk2Controller         * controller = machine.GetRefs().diskController;
        DeferredDiskIoQueue       queue;
        bool                      isEjected  = true;



        store.SetFlushSink ([] (const string &, const vector<Byte> &) { return HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED); });

        AssertSucceeded (store.MountFromBytes (kSlot, kDrive, kPath, DiskFormat::Woz, MakeWoz()));
        store.GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        store.SetIoQueue (&queue);

        store.Eject (kSlot, kDrive, [&isEjected] (bool ejected) { isEjected = ejected; });
        queue.RunAll();

        Assert::IsFalse (isEjected, L"the eject declines");
        Assert::IsTrue  (store.GetImage (kSlot, kDrive)->IsDirty(), L"the disk has its writes");
        Assert::IsTrue  (controller->GetDisk (kDrive) == store.GetImage (kSlot, kDrive), L"and is back in the drive");

        store.SetIoQueue (nullptr);
    }


    //  The user write-protects a WOZ, and the guest writes before the flag's
    //  patch has completed. The spin-down save after it compares the file
    //  with the identity after the patch, so the patch does not read as
    //  another program's change and nothing is moved to a preserved copy.
    TEST_METHOD (ASaveRequestedDuringAWriteProtectPatchFindsTheFileUnchanged)
    {
        static constexpr const char *  kFlagged = "Q:/casso-no-such-root/flagged.woz";

        auto                 store   = std::make_unique<DiskImageStore>();
        DeferredDiskIoQueue  queue;
        FakeDiskFileIo       io;
        HRESULT              patched = E_PENDING;



        UseFileIo (*store, io);
        io.SeedDurable (kFlagged, MakeWoz());

        AssertSucceeded (store->Mount (kSlot, kDrive, kFlagged));
        store->SetIoQueue (&queue);

        store->SetImageWriteProtect (kSlot, kDrive, true, [&patched] (HRESULT hr) { patched = hr; });
        WriteOneBit (*store);
        store->SaveAllUnlessHeld();
        queue.RunAll();

        AssertSucceeded  (patched, L"the flag is patched");
        Assert::AreEqual (string (kFlagged), store->GetSourcePath (kSlot, kDrive), L"the disk is on its file");
        Assert::AreEqual ((size_t) 1, io.files.size(), L"and no copy was written");

        store->SetIoQueue (nullptr);
    }


    //  A settled change found while a save of the disk is in flight is checked
    //  after the save, against the identity after the save's commit, so
    //  Casso's write does not read as another program's.
    TEST_METHOD (AReloadCheckIsNotPostedWhileASaveOfTheDiskIsInFlight)
    {
        static constexpr const char *  kWatched = "Q:/casso-no-such-root/watched.woz";

        auto                 store   = std::make_unique<DiskImageStore>();
        DeferredDiskIoQueue  queue;
        FakeDiskFileIo       io;
        int64_t              nowMs   = 0;
        uint64_t             imageId = 0;



        UseFileIo (*store, io);
        store->SetClock ([&nowMs] { return nowMs; });
        io.SeedDurable (kWatched, MakeWoz());

        AssertSucceeded (store->Mount (kSlot, kDrive, kWatched));
        store->SetIoQueue (&queue);
        imageId = store->GetImage (kSlot, kDrive)->GetImageId();

        WriteOneBit (*store);
        store->SaveAllUnlessHeld();

        store->NoteExternalChange (kWatched, ExternalChangeIntent::ReloadInPlace);
        nowMs += MountedImageState::kQuietPeriodMs;
        store->ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, queue.GetPostedCount(), L"the check is not posted during the save");

        queue.RunAll();
        store->ApplyPendingReload();
        queue.RunAll();

        Assert::AreEqual (imageId, store->GetImage (kSlot, kDrive)->GetImageId(), L"the disk was not reloaded from Casso's save");
        Assert::IsFalse  (store->GetImage (kSlot, kDrive)->IsDirty(), L"and its write is saved");

        store->SetIoQueue (nullptr);
    }


    //  With the threaded queue the shell uses, a spin-down save is written on
    //  the queue's thread, and its completion runs on the thread that drains.
    TEST_METHOD (OnTheThreadedQueueASpinDownSaveIsWrittenOffTheStoresThread)
    {
        auto                  store   = std::make_unique<DiskImageStore>();
        ThreadedDiskIoQueue   queue;
        std::thread::id       writer;
        int                   writes  = 0;
        HRESULT               hr      = S_OK;



        store->SetFlushSink ([&writer, &writes] (const string &, const vector<Byte> &)
        {
            writer = std::this_thread::get_id();
            writes++;
            return S_OK;
        });

        AssertSucceeded (store->MountFromBytes (kSlot, kDrive, kPath, DiskFormat::Woz, MakeWoz()));
        store->GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);
        store->SetIoQueue (&queue);

        store->SaveAllUnlessHeld();

        hr = queue.WaitUntilIdle (5000);
        AssertSucceeded (hr, L"WaitUntilIdle");

        Assert::AreEqual (1, writes, L"the save wrote once");
        Assert::IsTrue   (writer == queue.GetThreadId(), L"on the queue's thread");
        Assert::IsFalse  (store->GetImage (kSlot, kDrive)->IsDirty(), L"and the completion, drained here, marked the disk saved");

        store->SetIoQueue (nullptr);
    }


    //  A dirty disk whose file is deleted: the question about the lost file
    //  goes up, and the spin-down save, finding the file gone, writes the
    //  guest's version under the name the question reserved. The question on
    //  screen reads as though the disk exists in memory only, so a second
    //  dialog states the copy.
    TEST_METHOD (ACopySavedUnderALostFileQuestionIsStatedInASecondDialog)
    {
        static constexpr const char *  kFile = "lost.dsk";

        ScopedFlushNotifyCapture   capture;
        auto                       store    = std::make_unique<DiskImageStore>();
        ImageIdentity              identity;
        bool                       isGone   = false;
        int64_t                    nowMs    = 0;
        int                        asks     = 0;
        std::vector<std::string>   copies;



        identity.recorded           = true;
        identity.stamp.sizeBytes    = NibblizationLayer::kImageByteSize;
        identity.stamp.modifiedUnix = 100;

        store->SetClock          ([&nowMs] { return nowMs; });
        store->SetTimestampSource ([] { return (time_t) 1756500000; });
        store->SetImageReader    ([&isGone] (const string &, vector<Byte> & bytes)
        {
            bytes = vector<Byte> (NibblizationLayer::kImageByteSize, 0x11);
            return isGone ? HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) : S_OK;
        });
        store->SetIdentityReader ([&identity, &isGone] (const string &) { return isGone ? ImageIdentity() : identity; });
        store->SetFlushSink      ([&copies] (const string & path, const vector<Byte> &) { if (path != kFile) { copies.push_back (path); } return S_OK; });
        store->SetAskSink        ([&asks] (int, int, const ChangePrompt &) { asks++; return true; });

        AssertSucceeded (store->Mount (kSlot, kDrive, kFile));
        store->GetImage (kSlot, kDrive)->GetTrackBitsForWrite (0)[0] ^= 0x01;
        store->GetImage (kSlot, kDrive)->SetLoadedForTest (true, true);

        isGone = true;
        store->NoteExternalChange (kFile, ExternalChangeIntent::Unstated);
        nowMs += MountedImageState::kQuietPeriodMs;
        store->ApplyPendingReload();

        Assert::AreEqual (1, asks, L"the question about the lost file is put");
        Assert::AreEqual (0, s_flushNotifyCount, L"and nothing else is shown yet");

        store->SaveAllUnlessHeld();

        Assert::AreEqual ((size_t) 1, copies.size(), L"the save wrote the guest's version beside the file");
        Assert::AreEqual (1, s_flushNotifyCount, L"and a second dialog states it");
        Assert::IsTrue   (s_flushNotifyLast.find (TextEncoding::NarrowToWide (fs::path (copies[0]).filename().string())) != std::wstring::npos, s_flushNotifyLast.c_str());
    }
};