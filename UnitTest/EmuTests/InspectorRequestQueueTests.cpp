#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Shell/InspectorRequestQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueueTests
//
//  Copy requests from the inspector window to the emulation thread
//  (contracts/emulation-thread.md): answered in order at Drain, a reply for
//  each, DiskChanged when the drive has another disk, and one wake-up per
//  Drain that answered something.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorRequestQueueTests)
{
public:

    class FakeSource : public IInspectorDiskSource
    {
    public:
        DiskImage  image;
        uint64_t   mediaId  = 42;
        bool       hasDisk  = true;

        const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
        {
            outIdentity.mediaId  = mediaId;
            outIdentity.fileName = "fake.dsk";

            return (hasDisk && drive == 0) ? &image : nullptr;
        }
    };



    static void MakeSource (FakeSource & source)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0x11);



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, source.image));
    }



    TEST_METHOD (EachRequestGetsItsReplyAtDrain)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;
        int                     notified = 0;
        uint64_t                first    = 0;
        uint64_t                second   = 0;



        MakeSource (source);
        queue.SetNotify ([&notified] () { notified++; });

        first  = queue.Post ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
        second = queue.Post ({ InspectorRequestKind::CopyTracks, 0, 42, { 3, 4 }, -1 });

        queue.TakeReplies (replies);
        Assert::IsTrue (replies.empty(), L"nothing is answered before Drain");
        Assert::IsTrue (queue.HasRequests());

        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::AreEqual (1, notified);
        Assert::AreEqual (static_cast<size_t> (2), replies.size());
        Assert::AreEqual (first, replies[0].requestId);
        Assert::IsNotNull (replies[0].disk.get());
        Assert::AreEqual (42ull, replies[0].disk->mediaId);
        Assert::AreEqual (second, replies[1].requestId);
        Assert::AreEqual (static_cast<size_t> (2), replies[1].tracks.size());
        Assert::AreEqual (3, replies[1].tracks[0]->slot);
        Assert::IsFalse (queue.HasRequests());
    }



    TEST_METHOD (AsInsertedGivesTheBytesTheDiskWentInWith)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;
        vector<Byte>            sectors (NibblizationLayer::kImageByteSize, 0x22);



        source.image.LoadFromBytes (DiskFormat::Dsk, sectors, "fake.dsk");
        source.image.ClearDirty();

        queue.Post ({ InspectorRequestKind::CopyAsInserted, 0, 0, {}, -1 });
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::IsNotNull (replies[0].sourceBytes.get());
        Assert::IsTrue (*replies[0].sourceBytes == sectors, L"the bytes as loaded, a save notwithstanding");
        Assert::AreEqual (std::string ("fake.dsk"), replies[0].fileName);
        Assert::IsNull (replies[0].disk.get(), L"no copy of the disk now");
    }



    TEST_METHOD (AnotherDiskOrNoDiskDoesNothing)    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;



        MakeSource (source);

        queue.Post ({ InspectorRequestKind::CopyDisk, 0, 41, {}, -1 });
        queue.Post ({ InspectorRequestKind::CopyDisk, 1, 0, {}, -1 });
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::IsTrue (replies[0].status == InspectorReplyStatus::DiskChanged);
        Assert::IsNull (replies[0].disk.get());
        Assert::IsTrue (replies[1].status == InspectorReplyStatus::NoDisk);
    }



    TEST_METHOD (ExportCopiesTheRecordAQuarterTrackPlays)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;



        MakeSource (source);

        queue.Post ({ InspectorRequestKind::Export, 0, 42, {}, 4 * 7 });
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::AreEqual (static_cast<size_t> (1), replies[0].tracks.size());
        Assert::AreEqual (source.image.ResolveQuarterTrack (4 * 7), replies[0].tracks[0]->slot);
    }


    //  A source that can change its disk, with the drive's motion, the save's
    //  result and the machine's pause state set by the test.
    class EditSource : public FakeSource
    {
    public:
        InspectorDriveMotion  motion;
        InspectorSaveResult   saveResult { S_OK, "fake.dsk", false, L"" };
        int                   saves      = 0;
        vector<bool>          pauses;

        DiskImage *           GetImageForEdit  (int drive) override         { return (hasDisk && drive == 0) ? &image : nullptr; }
        InspectorDriveMotion  GetMotion        (int drive) override         { (void) drive; return motion; }
        InspectorSaveResult   SaveDrive        (int drive) override         { (void) drive; saves++; return saveResult; }
        void                  SetMachinePaused (bool isPaused) override     { pauses.push_back (isPaused); }
    };



    //  An apply of sector 5 of track 3, all $A5.
    static InspectorRequest MakeApply (const DiskImage & image, ApplyMode mode)
    {
        InspectorRequest  request;
        SectorWrite       write;



        write.track  = 3;
        write.slot   = image.ResolveWholeTrack (3);
        write.sector = 5;
        write.bytes.fill (0xA5);

        request.kind           = InspectorRequestKind::Apply;
        request.drive          = 0;
        request.mediaId        = 42;
        request.applyKind      = ApplyKind::Apply;
        request.applyMode      = mode;
        request.writes         = { write };
        request.expectedCounts = { { write.slot, 0 } };

        return request;
    }



    static InspectorReply DrainOne (InspectorRequestQueue & queue, EditSource & source)
    {
        vector<InspectorReply>  replies;



        queue.Drain (source);
        queue.TakeReplies (replies);
        Assert::AreEqual (static_cast<size_t> (1), replies.size());

        return replies[0];
    }



    TEST_METHOD (AnApplyWritesSavesAndAnUndoPutsTheRecordBack)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorReply         reply;
        InspectorRequest       undo;
        int                    slot = 0;



        MakeSource (source);
        slot = source.image.ResolveWholeTrack (3);

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());
        Assert::AreEqual (1, source.saves);
        Assert::AreEqual (static_cast<size_t> (1), reply.before.size());
        Assert::AreEqual (static_cast<size_t> (1), reply.after.size());
        Assert::IsTrue   (reply.before[0]->bits != reply.after[0]->bits);
        Assert::IsTrue   (TrackCopy::MakeFromImage (source.image, slot)->bits == reply.after[0]->bits);
        Assert::AreEqual (0ull, reply.countsAfter[0].guestWriteCount, L"an edit is not a guest write");

        undo                = MakeApply (source.image, ApplyMode::WaitForStop);
        undo.applyKind      = ApplyKind::Undo;
        undo.writes.clear();
        undo.restore        = reply.before;
        queue.Post (undo);
        reply = DrainOne (queue, source);

        Assert::IsTrue (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());
        Assert::IsTrue (TrackCopy::MakeFromImage (source.image, slot)->bits == reply.after[0]->bits);
        Assert::AreEqual (2, source.saves);
    }



    TEST_METHOD (AFailedSaveRestoresTheRecordAndAppliesNothing)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  before;



        MakeSource (source);
        before            = TrackCopy::MakeFromImage (source.image, source.image.ResolveWholeTrack (3));
        source.saveResult = { E_ACCESSDENIED, "fake.dsk", false, L"Access is denied." };

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        reply = DrainOne (queue, source);

        Assert::IsTrue (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::IsTrue (reply.reason.starts_with (L"Not applied: the file could not be saved"), reply.reason.c_str());
        Assert::IsTrue (TrackCopy::MakeFromImage (source.image, before->slot)->bits == before->bits, L"the drive has the record as it was");
    }



    TEST_METHOD (AnApplyWaitsForTheDriveToStop)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        vector<InspectorReply> replies;



        MakeSource (source);
        source.motion.isTurning = true;

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);
        Assert::IsTrue (queue.HasWaiting());

        queue.Drain (source);
        queue.TakeReplies (replies);
        Assert::IsTrue (replies.empty(), L"still turning, still waiting");
        Assert::AreEqual (0, source.saves);

        source.motion.isTurning = false;
        Assert::IsTrue  (DrainOne (queue, source).applyOutcome == ApplyOutcome::Applied);
        Assert::IsFalse (queue.HasWaiting());
    }



    TEST_METHOD (PauseAndApplyWaitsUntilTheHeadIsOutsideTheField)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorRequest       request;
        int                    slot = 0;



        MakeSource (source);
        slot                    = source.image.ResolveWholeTrack (3);
        request                 = MakeApply (source.image, ApplyMode::PauseAtSafePoint);
        request.spans           = { { slot, 0.2, 0.3 } };
        source.motion.isTurning = true;
        source.motion.slot      = slot;
        source.motion.angle     = 0.25;

        queue.Post (request);
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting, L"the head is over the field");

        source.motion.angle = 0.6;
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Applied);
        Assert::IsTrue (source.pauses.empty(), L"the machine runs on");
    }



    TEST_METHOD (RunToASafePointRunsThePausedMachineAndPausesItAfter)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorRequest       request;
        int                    slot = 0;



        MakeSource (source);
        slot                    = source.image.ResolveWholeTrack (3);
        request                 = MakeApply (source.image, ApplyMode::RunToSafePointStayPaused);
        request.spans           = { { slot, 0.2, 0.3 } };
        source.motion.isTurning = true;
        source.motion.isPaused  = true;
        source.motion.slot      = slot;
        source.motion.angle     = 0.25;

        queue.Post (request);
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);
        Assert::IsTrue (source.pauses == vector<bool> { false }, L"run to reach the safe point");

        source.motion.isPaused = false;
        source.motion.angle    = 0.9;
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Applied);
        Assert::IsTrue (source.pauses == (vector<bool> { false, true }), L"and paused again after");
    }



    TEST_METHOD (AGuestWriteEndsTheWaitAndCancelEndsItToo)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorRequest       request;
        InspectorRequest       cancel;



        MakeSource (source);
        source.motion.isTurning = true;

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);

        source.image.CountGuestWrite (source.image.ResolveWholeTrack (3));
        Assert::IsTrue  (DrainOne (queue, source).applyOutcome == ApplyOutcome::WaitEnded);
        Assert::IsFalse (queue.HasWaiting());

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::WaitEnded, L"the request's counts are older than the guest's write");

        //  A wait with the current counts, ended by Cancel.
        request                                   = MakeApply (source.image, ApplyMode::WaitForStop);
        request.expectedCounts[0].guestWriteCount = 1;
        cancel.kind                               = InspectorRequestKind::CancelApply;

        queue.Post (request);
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);
        queue.Post (cancel);
        Assert::IsTrue   (DrainOne (queue, source).applyOutcome == ApplyOutcome::Cancelled);
        Assert::IsFalse  (queue.HasWaiting());
        Assert::AreEqual (0, source.saves);
    }



    TEST_METHOD (AWriteProtectedDiskIsNotChanged)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorReply         reply;



        MakeSource (source);
        source.image.SetUserWriteProtected (true);

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::AreEqual (0, source.saves);
    }


    TEST_METHOD (ASavedCopyHasTheEditsAndTheDriveDiskHasNot)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorRequest                  request;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  before;
        vector<Byte>                      run (DiskFieldFormat::kSectorBytes, 0xA5);



        MakeSource (source);
        before       = TrackCopy::MakeFromImage (source.image, source.image.ResolveWholeTrack (3));
        request      = MakeApply (source.image, ApplyMode::WaitForStop);
        request.kind = InspectorRequestKind::SaveCopy;

        queue.Post (request);
        reply = DrainOne (queue, source);

        Assert::IsNotNull (reply.sourceBytes.get(), reply.reason.c_str());
        Assert::AreEqual  (static_cast<size_t> (NibblizationLayer::kImageByteSize), reply.sourceBytes->size());
        Assert::IsTrue    (std::search (reply.sourceBytes->begin(), reply.sourceBytes->end(), run.begin(), run.end()) != reply.sourceBytes->end(), L"the copy has the sector's new bytes");
        Assert::IsTrue    (TrackCopy::MakeFromImage (source.image, before->slot)->bits == before->bits, L"the drive's disk is unchanged");
        Assert::IsFalse   (source.image.IsDirty());
        Assert::AreEqual  (0, source.saves);
    }
};