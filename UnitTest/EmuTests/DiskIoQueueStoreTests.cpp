#include "Pch.h"
#include "DiskStoreTestSupport.h"
#include "DeferredDiskIoQueue.h"

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
};