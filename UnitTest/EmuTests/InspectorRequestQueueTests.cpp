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
        InspectorDriveMotion              motion;
        InspectorSaveResult               saveResult   { S_OK, "fake.dsk", false, L"" };
        int                               saves        = 0;
        vector<bool>                      pauses;
        size_t                            pausesAtSave = 0;
        std::shared_ptr<const TrackCopy>  atSave;

        DiskImage *           GetImageForEdit  (int drive) override         { return (hasDisk && drive == 0) ? &image : nullptr; }
        InspectorDriveMotion  GetMotion        (int drive) override         { (void) drive; return motion; }
        void                  SetMachinePaused (bool isPaused) override     { pauses.push_back (isPaused); }

        //  The record as the save finds it, and how many times the machine was
        //  run or paused before it.
        InspectorSaveResult   SaveDrive        (int drive) override
        {
            (void) drive;

            saves++;
            pausesAtSave = pauses.size();
            atSave       = TrackCopy::MakeFromImage (image, image.ResolveWholeTrack (3));

            return saveResult;
        }
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



    TEST_METHOD (AGuestWriteEndingAWaitPausesAMachineRunUntilTheSafePoint)
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
        Assert::IsTrue (source.pauses == vector<bool> { false }, L"run until the safe point");

        source.image.CountGuestWrite (slot);
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::WaitEnded);
        Assert::IsTrue (source.pauses == (vector<bool> { false, true }), L"paused again, as after a cancel");
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



    TEST_METHOD (AnApplyChangesAndSavesTheRecordInOnePassWithTheMachinePausedAfterward)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  before;



        MakeSource (source);
        before                 = TrackCopy::MakeFromImage (source.image, source.image.ResolveWholeTrack (3));
        source.motion.isPaused = true;

        queue.Post (MakeApply (source.image, ApplyMode::RunToSafePointStayPaused));
        reply = DrainOne (queue, source);

        Assert::IsTrue    (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());
        Assert::AreEqual  (1, source.saves);
        Assert::IsTrue    (source.atSave->bits != before->bits, L"the record has the edit when it is saved");
        Assert::AreEqual  (static_cast<size_t> (0), source.pausesAtSave, L"the machine was not run or paused between the change and the save");
        Assert::IsTrue    (source.pauses == vector<bool> { true }, L"and it is paused after the save");
    }



    TEST_METHOD (AFailedSaveRollsTheRecordBackAndPausesTheMachineAfterward)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorRequest                  request;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  before;
        int                               slot = 0;



        MakeSource (source);
        slot                    = source.image.ResolveWholeTrack (3);
        before                  = TrackCopy::MakeFromImage (source.image, slot);
        request                 = MakeApply (source.image, ApplyMode::RunToSafePointStayPaused);
        request.spans           = { { slot, 0.2, 0.3 } };
        source.motion.isTurning = true;
        source.motion.isPaused  = true;
        source.motion.slot      = slot;
        source.motion.angle     = 0.25;
        source.saveResult       = { E_ACCESSDENIED, "fake.dsk", false, L"Access is denied." };

        queue.Post (request);
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);

        source.motion.isPaused = false;
        source.motion.angle    = 0.9;
        reply                  = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::IsTrue   (source.atSave->bits != before->bits, L"the record had the edit when the save was attempted");
        Assert::IsTrue   (TrackCopy::MakeFromImage (source.image, slot)->bits == before->bits, L"the record is back as it was");
        Assert::IsTrue   (source.pauses == (vector<bool> { false, true }), L"the machine is paused after the rollback");
        Assert::AreEqual (static_cast<size_t> (1), source.pausesAtSave, L"the machine was not run or paused between the change and the save");
    }



    TEST_METHOD (AFailedSaveReportsTheReasonAndNoAppliedRecords)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorReply         reply;



        MakeSource (source);
        source.saveResult = { E_ACCESSDENIED, "fake.dsk", false, L"Access is denied." };

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        reply = DrainOne (queue, source);

        Assert::AreEqual (std::wstring (L"Not applied: the file could not be saved. Access is denied."), reply.reason);
        Assert::IsTrue   (reply.after.empty(), L"no record is reported as applied");
        Assert::IsTrue   (reply.countsAfter.empty());
        Assert::IsTrue   (reply.savedPath.empty());
        Assert::AreEqual (static_cast<size_t> (1), reply.before.size(), L"the reply has the record as it was");
    }



    TEST_METHOD (AWriteThatCannotBeMadeChangesNothingAndSavesNothing)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorRequest                  request;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  before;
        constexpr Byte                    kNoSuchSector = 99;



        MakeSource (source);
        before                   = TrackCopy::MakeFromImage (source.image, source.image.ResolveWholeTrack (3));
        request                  = MakeApply (source.image, ApplyMode::WaitForStop);
        request.writes[0].sector = kNoSuchSector;

        queue.Post (request);
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::IsTrue   (reply.reason.starts_with (L"Not applied: "), reply.reason.c_str());
        Assert::IsFalse  (reply.writeErrors.empty());
        Assert::AreEqual (0, source.saves);
        Assert::IsTrue   (TrackCopy::MakeFromImage (source.image, before->slot)->bits == before->bits, L"the record is as it was");
    }



    TEST_METHOD (ASaveToAPreservedCopyCountsAsAppliedToTheCopy)
    {
        EditSource             source;
        InspectorRequestQueue  queue;
        InspectorReply         reply;



        MakeSource (source);
        source.saveResult = { S_OK, "fake (preserved).dsk", true, L"" };

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy, reply.reason.c_str());
        Assert::AreEqual (std::string ("fake (preserved).dsk"), reply.savedPath);
        Assert::AreEqual (static_cast<size_t> (1), reply.after.size());
        Assert::AreEqual (static_cast<size_t> (1), reply.countsAfter.size(), L"the counts for the history are against the copy");
        Assert::AreEqual (1, source.saves);
    }



    TEST_METHOD (RedoPutsTheAppliedRecordBackBitForBit)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorReply                    applied;
        InspectorReply                    undone;
        InspectorReply                    redone;
        InspectorRequest                  request;
        std::shared_ptr<const TrackCopy>  before;
        std::shared_ptr<const TrackCopy>  now;
        int                               slot = 0;



        MakeSource (source);
        slot   = source.image.ResolveWholeTrack (3);
        before = TrackCopy::MakeFromImage (source.image, slot);

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        applied = DrainOne (queue, source);
        Assert::IsTrue (applied.applyOutcome == ApplyOutcome::Applied, applied.reason.c_str());

        request           = MakeApply (source.image, ApplyMode::WaitForStop);
        request.applyKind = ApplyKind::Undo;
        request.writes.clear();
        request.restore   = applied.before;
        queue.Post (request);
        undone = DrainOne (queue, source);

        now = TrackCopy::MakeFromImage (source.image, slot);
        Assert::IsTrue (undone.applyOutcome == ApplyOutcome::Applied, undone.reason.c_str());
        Assert::IsTrue (now->bits == before->bits && now->bitCount == before->bitCount, L"the undo restores every bit");

        request                = MakeApply (source.image, ApplyMode::WaitForStop);
        request.applyKind      = ApplyKind::Redo;
        request.writes.clear();
        request.restore        = applied.after;
        request.expectedCounts = undone.countsAfter;
        queue.Post (request);
        redone = DrainOne (queue, source);

        now = TrackCopy::MakeFromImage (source.image, slot);
        Assert::IsTrue   (redone.applyOutcome == ApplyOutcome::Applied, redone.reason.c_str());
        Assert::IsTrue   (now->bits == applied.after[0]->bits && now->bitCount == applied.after[0]->bitCount, L"the redo restores every bit of the applied record");
        Assert::AreEqual (3, source.saves, L"each of the three saves the disk");
    }



    TEST_METHOD (AnUndoOrRedoAfterAGuestWriteIsNotAppliedWithTheReason)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorReply                    applied;
        InspectorReply                    reply;
        InspectorRequest                  request;
        std::shared_ptr<const TrackCopy>  atGuestWrite;
        int                               slot = 0;



        MakeSource (source);
        slot = source.image.ResolveWholeTrack (3);

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        applied = DrainOne (queue, source);
        Assert::IsTrue (applied.applyOutcome == ApplyOutcome::Applied, applied.reason.c_str());

        source.image.CountGuestWrite (slot);
        atGuestWrite = TrackCopy::MakeFromImage (source.image, slot);

        request                = MakeApply (source.image, ApplyMode::WaitForStop);
        request.applyKind      = ApplyKind::Undo;
        request.writes.clear();
        request.restore        = applied.before;
        request.expectedCounts = applied.countsAfter;
        queue.Post (request);
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied, L"an undo does not wait");
        Assert::IsTrue   (reply.reason.starts_with (L"The guest wrote to a track"), reply.reason.c_str());

        request.applyKind      = ApplyKind::Redo;
        request.restore        = applied.after;
        queue.Post (request);
        reply = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied, L"a redo does not wait");
        Assert::IsTrue   (reply.reason.starts_with (L"The guest wrote to a track"), reply.reason.c_str());
        Assert::IsTrue   (TrackCopy::MakeFromImage (source.image, slot)->bits == atGuestWrite->bits, L"the record is as the guest left it");
        Assert::AreEqual (1, source.saves, L"only the first apply is saved");
    }



    TEST_METHOD (AGuestWriteEndsTheWaitWithTheReasonAndChangesNothing)
    {
        EditSource                        source;
        InspectorRequestQueue             queue;
        InspectorReply                    reply;
        std::shared_ptr<const TrackCopy>  atGuestWrite;
        int                               slot = 0;



        MakeSource (source);
        slot                    = source.image.ResolveWholeTrack (3);
        source.motion.isTurning = true;

        queue.Post (MakeApply (source.image, ApplyMode::WaitForStop));
        Assert::IsTrue (DrainOne (queue, source).applyOutcome == ApplyOutcome::Waiting);

        source.image.CountGuestWrite (slot);

        atGuestWrite            = TrackCopy::MakeFromImage (source.image, slot);
        source.motion.isTurning = false;
        reply                   = DrainOne (queue, source);

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::WaitEnded, L"the drive stopped, but the guest wrote first");
        Assert::IsTrue   (reply.reason.starts_with (L"The guest wrote to a track"), reply.reason.c_str());
        Assert::IsTrue   (reply.after.empty());
        Assert::AreEqual (0, source.saves);
        Assert::IsTrue   (TrackCopy::MakeFromImage (source.image, slot)->bits == atGuestWrite->bits);
    }
};