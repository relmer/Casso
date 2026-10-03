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


    //  The motor stopping is a flush nobody asked for; while history is
    //  recording, the guest's writes stay in memory instead.
    TEST_METHOD (MotorStoppingWhileRecordingLeavesTheFileAlone)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        FlushLog           log;
        HRESULT            hr         = S_OK;



        PrepareMotorProgram (machine, log);

        hr = controller.Start (MakeSettings());
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPastSpindown);

        Assert::AreEqual<size_t> (0, log.count, L"the motor stopped and nothing reached the file");
        Assert::IsTrue (machine.GetDiskStore().GetImage (6, 0)->IsDirty(), L"the guest's writes are still held");
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
        Assert::AreEqual<size_t>   (0, liveCount, L"nor did the live power cycle while recording");
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


private:

    static ReverseSettings MakeSettings()
    {
        ReverseSettings  settings;



        settings.ring.budgetBytes = 0;

        return settings;
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
