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
};
