#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kDiskWarmupCycles  = 60000;
static constexpr uint64_t  s_kDiskAfterCycles   = 30000;
static constexpr uint64_t  s_kPastSpindown      = Disk2Controller::kMotorSpindownCycles + 100000;
static constexpr size_t    s_kNibPattern        = 29;
static constexpr Byte      s_kHighBit           = 0x80;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHistoryTests
//
//  Disk media under reverse execution: the image file on the host is never
//  written by a replay or while the machine is behind live, a flush writes
//  the disk as it stands at the current position, and stepping back across a
//  guest write, an eject or a mount puts back the disk the machine had there.
//
//  The disks are nibble images, which serialize whatever bits the guest
//  wrote, and every write to the host goes to a counting sink.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskHistoryTests)
{
public:

    struct FlushLog
    {
        size_t             count = 0;
        std::vector<Byte>  last;
    };


    //  Recording alone holds nothing back: live, the motor stopping writes the
    //  guest's changes to the file as it does without history.
    TEST_METHOD (MotorStoppingWhileLiveAndRecordingWritesTheFile)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        HRESULT            hr         = S_OK;



        PrepareMotorProgram (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPastSpindown);

        Assert::IsFalse (controller.IsInHistory(), L"the machine is live");
        Assert::AreEqual<size_t> (1, log.count, L"the motor stopped and the file was written");
        Assert::IsFalse (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"nothing is left unwritten");
    }


    //  Behind live, the guest's writes are machine state a step forward can
    //  change again, so an automatic flush leaves the file alone.
    TEST_METHOD (FlushingBehindLiveLeavesTheFileAlone)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             liveCount  = 0;



        PrepareMotorProgram (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        liveCount = log.count;

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition back to the start");

        Assert::IsTrue (controller.IsInHistory(), L"the machine is behind live");

        hr = machine.GetDiskStore().FlushAllUnlessHeld();
        AssertSucceeded (hr, L"FlushAllUnlessHeld behind live");

        Assert::AreEqual<size_t> (liveCount, log.count, L"nothing reached the file behind live");
    }


    //  Going live again lifts the hold, and the next flush writes the disk as
    //  it stands at the live position.
    TEST_METHOD (FlushingAfterGoingLiveWritesTheLiveDisk)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           liveEnd    = 0;
        std::vector<Byte>  atLive;



        PrepareMotorProgram (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        Assert::AreEqual<size_t> (0, log.count, L"the motor has not stopped yet");
        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest wrote to the disk");

        liveEnd = machine.GetPosition();

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (atLive);
        AssertSucceeded (hr, L"Serialize at the live position");

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition back to the start");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition back to live");

        Assert::IsFalse (controller.IsInHistory(), L"the machine is live again");

        hr = machine.GetDiskStore().FlushAllUnlessHeld();
        AssertSucceeded (hr, L"FlushAllUnlessHeld at live");

        Assert::AreEqual<size_t> (1, log.count, L"the flush at live wrote the file");
        Assert::IsTrue (atLive == log.last, L"with the disk as it stood at the live position");
    }


    //  A replay runs through the motor stopping again; it must not flush.
    TEST_METHOD (ReplayingPastTheMotorStoppingWritesNothing)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           liveEnd    = 0;
        size_t             liveCount  = 0;



        PrepareMotorProgram (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPastSpindown);

        liveEnd   = machine.GetPosition();
        liveCount = log.count;

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition back to the start");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition forward past the motor stopping");

        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::AreEqual<size_t>   (liveCount, log.count, L"the replay wrote nothing to the file");
    }


    //  A replayed power cycle must not run the power cycle's flush.
    TEST_METHOD (ReplayingAPowerCycleWritesNothing)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           liveEnd    = 0;
        size_t             liveCount  = 0;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest wrote to the disk");

        machine.RecordInput (InputKind::PowerCycle, 0, 0, {});
        machine.PowerCycle();
        machine.RunCycles (s_kDiskAfterCycles);

        liveEnd   = machine.GetPosition();
        liveCount = log.count;

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition back to the start");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition forward past the power cycle");

        Assert::AreEqual<size_t>   (liveCount, log.count, L"the replayed power cycle wrote nothing to the file");
        Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, L"the replay matched every keyframe");
        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::AreEqual<size_t>   (1, liveCount, L"the live power cycle wrote once, as it does without history");
    }


    //  The disk is machine state: a step back across a guest write gives the
    //  track bits as they were before it.
    TEST_METHOD (SteppingBackAcrossAGuestWriteRestoresTheOldTrackBits)
    {
        TestMachine                      machine    ("Apple2e");
        ReverseController                controller (machine);
        FlushLog                         log;
        ReverseResult                    result;
        HRESULT                          hr         = S_OK;
        uint64_t                         before     = 0;
        std::vector<std::vector<Byte>>   oldBits;
        std::vector<std::vector<Byte>>   newBits;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        RunUntilPc (machine, ReverseSessionRig::kWriteRoutine);

        before  = machine.GetPosition();
        oldBits = CopyTracks (machine);

        RunUntilPc (machine, ReverseSessionRig::kReturnSite);

        newBits = CopyTracks (machine);

        Assert::IsFalse (oldBits == newBits, L"the routine wrote to the disk");

        hr = controller.SeekToPosition (before, result);
        AssertSucceeded (hr, L"SeekToPosition before the write");

        Assert::IsTrue (oldBits == CopyTracks (machine), L"the track bits from before the write");
    }


    //  The file is written with the disk as it stands at the current position,
    //  even when that is behind a later flush.
    TEST_METHOD (CommitWritesTheDiskAtTheCurrentPosition)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           start      = 0;
        std::vector<Byte>  atStart;
        std::vector<Byte>  atEnd;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        start = machine.GetPosition();

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (atStart);
        AssertSucceeded (hr, L"Serialize at the start");

        machine.RunCycles (s_kDiskWarmupCycles);

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (atEnd);
        AssertSucceeded (hr, L"Serialize at the end");

        Assert::IsFalse (atStart == atEnd, L"the guest wrote to the disk");

        hr = machine.GetDiskStore().FlushAll();
        AssertSucceeded (hr, L"FlushAll at the end");

        Assert::AreEqual<size_t> (1, log.count, L"the commit at the end wrote once");
        Assert::IsTrue (atEnd == log.last, L"and wrote the disk as it stood there");

        hr = controller.SeekToPosition (start, result);
        AssertSucceeded (hr, L"SeekToPosition to the start");

        hr = machine.GetDiskStore().FlushAll();
        AssertSucceeded (hr, L"FlushAll at the start");

        Assert::AreEqual<size_t> (2, log.count, L"the file was ahead of the machine, so the commit wrote again");
        Assert::IsTrue (atStart == log.last, L"the disk as it stood at the start, in one write");
    }


    //  Stepping back across an eject puts the disk back in the drive with what
    //  it held there, and stepping forward again takes it out.
    TEST_METHOD (SeekingBackAcrossAnEjectPutsTheDiskBack)
    {
        TestMachine        machine      ("Apple2e");
        ReverseController  controller   (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr           = S_OK;
        uint64_t           before       = 0;
        uint64_t           checksum     = 0;
        uint64_t           liveEnd      = 0;
        uint64_t           liveChecksum = 0;
        size_t             liveCount    = 0;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        before   = machine.GetPosition();
        checksum = ReverseSessionRig::Checksum (machine);

        // A change made at a position replaces what the machine held there.
        machine.StepOne();

        Eject (machine);

        Assert::AreEqual<size_t> (1, log.count, L"an eject saves the disk by default");

        machine.RunCycles (s_kDiskAfterCycles);

        liveEnd      = machine.GetPosition();
        liveChecksum = ReverseSessionRig::Checksum (machine);
        liveCount    = log.count;

        hr = controller.SeekToPosition (before, result);
        AssertSucceeded (hr, L"SeekToPosition before the eject");

        Assert::IsTrue (machine.GetDiskStore().IsMounted (6, 0), L"the disk is back in the drive");
        Assert::IsTrue (machine.GetRefs().diskController->GetDisk (0) == machine.GetDiskStore().GetImage (6, 0), L"and the drive reads it");
        Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"the whole machine as it was before the eject");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition past the eject");

        Assert::IsFalse (machine.GetDiskStore().IsMounted (6, 0), L"the replay took the disk out again");
        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::AreEqual<uint64_t> (liveChecksum, ReverseSessionRig::Checksum (machine), L"the whole machine as it was at the live end");
        Assert::AreEqual<size_t> (liveCount, log.count, L"and wrote nothing to the file");
    }


    //  A mount replays from memory, never from the file: going forward across
    //  it gives the disk the machine had, with the writes made after it.
    TEST_METHOD (SeekingForwardAcrossAMountPutsTheSameDiskIn)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           empty      = 0;
        uint64_t           liveEnd    = 0;
        uint64_t           checksum   = 0;
        size_t             liveCount  = 0;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        Eject (machine);

        machine.RunCycles (s_kDiskAfterCycles);

        empty = machine.GetPosition();

        machine.StepOne();

        Mount (machine, "second.nib");

        machine.RunCycles (s_kDiskWarmupCycles);

        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest wrote to the second disk");

        liveEnd   = machine.GetPosition();
        checksum  = ReverseSessionRig::Checksum (machine);
        liveCount = log.count;

        hr = controller.SeekToPosition (empty, result);
        AssertSucceeded (hr, L"SeekToPosition to the empty drive");

        Assert::IsFalse (machine.GetDiskStore().IsMounted (6, 0), L"the drive is empty there");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition past the mount");

        Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, L"the replay matched every keyframe");
        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::IsTrue             (machine.GetDiskStore().IsMounted (6, 0), L"the replay mounted the disk again");
        Assert::AreEqual<std::string> ("second.nib", machine.GetDiskStore().GetSourcePath (6, 0));
        Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"with what it held at the live end");
        Assert::AreEqual<size_t>   (liveCount, log.count, L"and nothing was written to a file");
    }


    //  A step from the past marks the disk store as replaying until the
    //  machine is live again. Quitting there stops recording first, and the
    //  last flush on the way out must still write the disk.
    TEST_METHOD (QuittingAfterAStepFromThePastWritesTheDisk)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        HRESULT            hr         = S_OK;
        std::vector<Byte>  held;



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        StepFromThePast (machine, controller);

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (held);
        AssertSucceeded (hr, L"Serialize where the machine stands");

        controller.Stop();

        hr = machine.GetDiskStore().FlushAllForShutdown();
        AssertSucceeded (hr, L"FlushAllForShutdown");

        Assert::AreEqual<size_t> (1, log.count, L"the last flush wrote the disk");
        Assert::IsTrue (held == log.last, L"as it stood where recording stopped");
    }


    //  A machine switch after a step from the past stops recording, saves the
    //  disk, and mounts it again from its file; the automatic flushes on the
    //  new machine then write as they do without history.
    TEST_METHOD (SwitchingMachinesAfterAStepFromThePastSavesAndRemountsTheDisk)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        HRESULT            hr         = S_OK;
        std::vector<Byte>  held;
        std::vector<Byte>  remounted;



        PrepareRigLoop (machine, log);

        //  The file holds what was last written to it.
        machine.GetDiskStore().SetImageReader ([&log] (const std::string & path, std::vector<Byte> & bytes)
        {
            HRESULT  hrRead = ReadPatternImage (path, bytes);



            if (log.count > 0)
            {
                bytes = log.last;
            }

            return hrRead;
        });

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        StepFromThePast (machine, controller);

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (held);
        AssertSucceeded (hr, L"Serialize where the machine stands");

        controller.Stop();

        hr = machine.GetDiskStore().FlushAll();
        AssertSucceeded (hr, L"FlushAll, as the switch saves the disks");

        Assert::AreEqual<size_t> (1, log.count, L"the switch wrote the disk");
        Assert::IsTrue (held == log.last, L"as it stood where recording stopped");

        hr = machine.GetDiskStore().Mount (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive, "history.nib");
        AssertSucceeded (hr, L"Mount, as the switch mounts the disk again");

        hr = machine.GetDiskStore().GetImage (6, 0)->Serialize (remounted);
        AssertSucceeded (hr, L"Serialize the remounted disk");

        Assert::IsTrue (held == remounted, L"the remounted disk holds the guest's writes");

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start, as the new machine starts recording");

        machine.RunCycles (s_kDiskWarmupCycles);

        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest wrote to the remounted disk");

        hr = machine.GetDiskStore().FlushAllUnlessHeld();
        AssertSucceeded (hr, L"FlushAllUnlessHeld on the new machine");

        Assert::AreEqual<size_t> (2, log.count, L"the automatic flush wrote the disk");
    }


    //  A reset with a disk in the drive reads the disk back from its file
    //  before the reset itself. Seeking back across it and forward again must
    //  run the reset, and land on the machine the live run left.
    TEST_METHOD (SeekingForwardAcrossAResetWithADiskInGivesTheLiveMachine)
    {
        TestMachine        machine      ("Apple2e");
        ReverseController  controller   (machine);
        FlushLog           log;
        ReverseResult      result;
        HRESULT            hr           = S_OK;
        uint64_t           before       = 0;
        uint64_t           liveEnd      = 0;
        uint64_t           liveCycles   = 0;
        uint64_t           liveChecksum = 0;
        Cpu6502Registers   live         = {};
        Cpu6502Registers   replayed     = {};



        PrepareRigLoop (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        before = machine.GetPosition();

        machine.StepOne();

        Reset (machine);

        machine.RunCycles (s_kDiskAfterCycles);

        liveEnd      = machine.GetPosition();
        liveCycles   = machine.GetCpu()->GetTotalCycles();
        liveChecksum = ReverseSessionRig::Checksum (machine);
        live         = machine.GetCpu()->GetCpu6502()->GetRegisters();

        hr = controller.SeekToPosition (before, result);
        AssertSucceeded (hr, L"SeekToPosition back before the reset");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition forward across the reset");

        replayed = machine.GetCpu()->GetCpu6502()->GetRegisters();

        Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, L"the replay matched every keyframe");
        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::AreEqual<uint64_t> (liveCycles, machine.GetCpu()->GetTotalCycles(), L"at the live run's cycle");
        Assert::AreEqual<Word>     (live.pc, replayed.pc, L"the live run's PC");
        Assert::AreEqual<Byte>     (live.a,  replayed.a,  L"the live run's A");
        Assert::AreEqual<Byte>     (live.x,  replayed.x,  L"the live run's X");
        Assert::AreEqual<Byte>     (live.y,  replayed.y,  L"the live run's Y");
        Assert::AreEqual<Byte>     (live.sp, replayed.sp, L"the live run's stack pointer");
        Assert::AreEqual<Byte>     (live.p,  replayed.p,  L"the live run's flags");
        Assert::AreEqual<uint64_t> (liveChecksum, ReverseSessionRig::Checksum (machine), L"the whole machine as the live run left it");
    }


    //  A file changed outside the emulator and taken up while the machine
    //  runs replaces the disk in the drive. Seeking back across the reload
    //  puts the disk that went out back in the drive, and seeking forward
    //  again takes the reloaded one.
    TEST_METHOD (SeekingBackAcrossAnExternalReloadPutsTheOutgoingDiskBack)
    {
        TestMachine         machine      ("Apple2e");
        ReverseController   controller   (machine);
        DiskImageStore    & store        = machine.GetDiskStore();
        FlushLog            log;
        ReverseResult       result;
        HRESULT             hr           = S_OK;
        int64_t             now          = 0;
        bool                isRewritten  = false;
        uint64_t            before       = 0;
        uint64_t            checksum     = 0;
        uint64_t            outgoing     = 0;
        uint64_t            liveEnd      = 0;
        uint64_t            liveChecksum = 0;



        PrepareRigLoop (machine, log);

        store.SetClock       ([&now] () { return now; });
        store.SetImageReader ([&isRewritten] (const std::string & path, std::vector<Byte> & bytes)
        {
            return isRewritten ? ReadRewrittenImage (path, bytes) : ReadPatternImage (path, bytes);
        });

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kDiskWarmupCycles);

        before   = machine.GetPosition();
        checksum = ReverseSessionRig::Checksum (machine);
        outgoing = store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);

        machine.StepOne();

        //  Saved first, so taking up the file is a reload rather than a
        //  conflict with the guest's writes.
        hr = store.FlushAll();
        AssertSucceeded (hr, L"FlushAll");

        isRewritten = true;

        store.NoteExternalChange ("history.nib", ExternalChangeIntent::ReloadInPlace);

        now += MountedImageState::kQuietPeriodMs;

        store.ApplyPendingReload();

        Assert::AreNotEqual<uint64_t> (outgoing, store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"the changed file was taken up");

        machine.RunCycles (s_kDiskAfterCycles);

        liveEnd      = machine.GetPosition();
        liveChecksum = ReverseSessionRig::Checksum (machine);

        hr = controller.SeekToPosition (before, result);
        AssertSucceeded (hr, L"SeekToPosition back before the reload");

        Assert::AreEqual<uint64_t> (outgoing, store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"the disk that went out is back in the drive");
        Assert::IsTrue (machine.GetRefs().diskController->GetDisk (0) == store.GetImage (6, 0), L"and the drive reads it");
        Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"the whole machine as it was before the reload");

        hr = controller.SeekToPosition (liveEnd, result);
        AssertSucceeded (hr, L"SeekToPosition forward across the reload");

        Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, L"the replay matched every keyframe");
        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"back at the live end");
        Assert::AreEqual<uint64_t> (liveChecksum, ReverseSessionRig::Checksum (machine), L"the whole machine as it was at the live end");
    }


    //  The drive's quiet-moment callback takes up a changed file inside an
    //  instruction, after the keyframe at that instruction's start was taken
    //  with the outgoing disk in the drive. While that keyframe is the oldest
    //  one held, seeking back to it must still put the outgoing disk back.
    TEST_METHOD (SeekingBackToTheKeyframeBeforeAReloadInsideAnInstructionPutsTheOutgoingDiskBack)
    {
        static constexpr uint64_t  kLongestInstruction = 7;
        TestMachine                machine     ("Apple2e");
        ReverseController          controller  (machine);
        DiskImageStore           & store       = machine.GetDiskStore();
        FlushLog                   log;
        ReverseResult              result;
        HRESULT                    hr          = S_OK;
        int64_t                    now         = 0;
        bool                       isRewritten = false;
        bool                       hasIdled    = false;
        uint64_t                   sinceIdle   = 0;
        uint64_t                   start       = 0;
        uint64_t                   outgoing    = 0;



        PrepareMotorProgram (machine, log);

        store.SetClock       ([&now] () { return now; });
        store.SetImageReader ([&isRewritten] (const std::string & path, std::vector<Byte> & bytes)
        {
            return isRewritten ? ReadRewrittenImage (path, bytes) : ReadPatternImage (path, bytes);
        });

        //  As the machine builder installs it, noting each time it runs.
        machine.GetRefs().diskController->SetIdleCallback ([&store, &hasIdled] ()
        {
            hasIdled = true;
            store.ApplyPendingReload();
        });

        machine.RunCycles (s_kDiskWarmupCycles);

        //  On from one run of the callback to the last instructions before
        //  the next.
        while (!hasIdled)
        {
            machine.StepOne();
        }

        while (sinceIdle + kLongestInstruction < Disk2Controller::kIdleCallbackCycles)
        {
            sinceIdle += machine.StepOne();
        }

        //  Saved first, so taking up the file is a reload rather than a
        //  conflict with the guest's writes.
        hr = store.FlushAll();
        AssertSucceeded (hr, L"FlushAll");

        isRewritten = true;

        store.NoteExternalChange ("history.nib", ExternalChangeIntent::ReloadInPlace);

        now += MountedImageState::kQuietPeriodMs;

        outgoing = store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);
        hasIdled = false;

        //  Recording starts again at each instruction that may be the one
        //  whose tick runs the callback, so the first keyframe is taken at the
        //  start of the instruction that takes up the file.
        while (!hasIdled)
        {
            controller.Stop();

            hr = controller.Start (MakeSettings());
            AssertSucceeded (hr, L"Start");

            start = machine.GetPosition();

            machine.StepOne();
        }

        Assert::AreNotEqual<uint64_t> (outgoing, store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"the instruction's tick took up the changed file");

        //  The next instruction start takes the keyframe after the reload.
        machine.StepOne();

        Assert::AreEqual<uint64_t> (start, controller.GetOldestPosition(), L"the keyframe from before the reload is the oldest");

        hr = controller.SeekToPosition (start, result);
        AssertSucceeded (hr, L"SeekToPosition back to the keyframe before the reload");

        Assert::AreEqual<uint64_t> (outgoing, store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"the disk that went out is back in the drive");
        Assert::IsTrue (machine.GetRefs().diskController->GetDisk (0) == store.GetImage (6, 0), L"and the drive reads it");
    }


private:

    static ReverseSettings MakeSettings()
    {
        return ReverseSettings();
    }


    //  The rig's machine and guest loop, with a nibble image in slot 6 drive
    //  1. Reads of an image file give a patterned nibble image, every write
    //  to the host is counted, and a bay change points the drive at the bay's
    //  disk, as the shell does.
    static void PrepareRigLoop (TestMachine & machine, FlushLog & log)
    {
        DiskImageStore  & store = machine.GetDiskStore();



        ReverseSessionRig::Prepare (machine);

        store.SetImageReader    (ReadPatternImage);
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });

        store.SetBayChangeSink ([&machine] (int slot, int drive, BayChange)
        {
            if (slot == ReverseSessionRig::kDiskSlot)
            {
                machine.GetRefs().diskController->SetExternalDisk (drive, machine.GetDiskStore().GetImage (slot, drive));
            }
        });

        Mount (machine, "history.nib");

        store.SetFlushSink ([&log] (const std::string &, const std::vector<Byte> & bytes)
        {
            log.count++;
            log.last = bytes;
            return S_OK;
        });

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
    }


    //  The rig's machine with a guest that turns the motor on, waits for it to
    //  spin up, writes sixteen bytes, turns the motor off and counts forever.
    static void PrepareMotorProgram (TestMachine & machine, FlushLog & log)
    {
        static constexpr Byte  kProgram[] =
        {
            0xAD, 0xE9, 0xC0,       // 0800  LDA $C0E9    motor on
            0xAD, 0xEA, 0xC0,       // 0803  LDA $C0EA    drive 1
            0xA2, 0x00,             // 0806  LDX #$00
            0xCA,                   // 0808  DEX
            0xD0, 0xFD,             // 0809  BNE $0808    spin up
            0x20, 0x00, 0x09,       // 080B  JSR $0900    write
            0xAD, 0xE8, 0xC0,       // 080E  LDA $C0E8    motor off
            0xEE, 0x04, 0x03,       // 0811  INC $0304
            0x4C, 0x11, 0x08,       // 0814  JMP $0811
        };
        size_t  i = 0;



        PrepareRigLoop (machine, log);

        for (i = 0; i < sizeof (kProgram); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (ReverseSessionRig::kProgramStart + i), kProgram[i]);
        }
    }


    static HRESULT ReadPatternImage (const std::string &, std::vector<Byte> & bytes)
    {
        size_t  i = 0;



        bytes.assign (NibbleImageCodec::kNibImageSize, 0);

        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * s_kNibPattern) | s_kHighBit);
        }

        return S_OK;
    }


    //  The image file after another program has rewritten it.
    static HRESULT ReadRewrittenImage (
        const std::string  & path,
        std::vector<Byte>  & bytes)
    {
        HRESULT  hr = ReadPatternImage (path, bytes);



        std::reverse (bytes.begin(), bytes.end());

        return hr;
    }


    //  Mounts an image in slot 6 drive 1 as the CPU thread does: journaled
    //  first, then mounted.
    static void Mount (TestMachine & machine, const std::string & path)
    {
        HRESULT  hr = S_OK;



        machine.RecordInput (InputKind::DiskMount, 0, 0, path);

        hr = machine.GetDiskStore().Mount (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive, path);
        AssertSucceeded (hr, L"Mount");
    }


    //  Ejects slot 6 drive 1 as the CPU thread does.
    static void Eject (TestMachine & machine)
    {
        machine.RecordInput (InputKind::DiskEject, 0, 0, {});
        machine.GetDiskStore().Eject (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);
    }


    //  Resets the machine as the CPU thread does: journaled first, then the
    //  slot 6 disk read back from its file, then the reset itself.
    static void Reset (TestMachine & machine)
    {
        HRESULT  hr = S_OK;



        machine.RecordInput (InputKind::Reset, 0, 0, {});

        hr = machine.GetDiskStore().Mount (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive, "history.nib");
        AssertSucceeded (hr, L"Mount, reading the disk back");

        machine.SoftReset();
    }


    //  Runs the guest until it has written to the disk and on past that, then
    //  seeks back to where the writes were made and runs one instruction there,
    //  as the debugger's step or the emulator running on from the past does.
    static void StepFromThePast (
        TestMachine        & machine,
        ReverseController  & controller)
    {
        HRESULT        hr     = S_OK;
        uint64_t       middle = 0;
        ReverseResult  result;



        machine.RunCycles (s_kDiskWarmupCycles);

        middle = machine.GetPosition();

        machine.RunCycles (s_kDiskAfterCycles);

        hr = controller.SeekToPosition (middle, result);
        AssertSucceeded (hr, L"SeekToPosition back into history");

        machine.StepOne();

        Assert::IsTrue (controller.IsInHistory(), L"the step left the machine behind live");
        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the disk holds the guest's writes");
    }


    static void RunUntilPc (TestMachine & machine, Word pc)
    {
        static constexpr size_t  kMaxSteps = 200000;
        size_t                   steps     = 0;



        machine.StepOne();

        while (machine.GetCpu()->GetPC() != pc && steps < kMaxSteps)
        {
            machine.StepOne();
            steps++;
        }

        Assert::AreEqual<Word> (pc, machine.GetCpu()->GetPC(), L"the guest reached the address");
    }


    static std::vector<std::vector<Byte>> CopyTracks (TestMachine & machine)
    {
        DiskImage                       * image  = machine.GetDiskStore().GetImage (6, 0);
        std::vector<std::vector<Byte>>    tracks;
        int                               track  = 0;



        Assert::IsNotNull (image, L"a disk is mounted");

        for (track = 0; track < image->GetTrackCount(); track++)
        {
            tracks.push_back (image->GetTrackBits (track));
        }

        return tracks;
    }
};
