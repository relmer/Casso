#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/DebugMemoryView.h"
#include "Debugger/Reply.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kTraceWarmupCycles = 60000;
static constexpr int       s_kTraceBudgetMb     = 16;
static constexpr size_t    s_kTraceRows         = 50;
static constexpr uint64_t  s_kTraceSeekBack     = 100;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTraceTests
//
//  Behind live, the instructions that led to the current position, rebuilt
//  by replaying the current stretch, for the trace pane; and the disks
//  holding writes their files do not, for the history band.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryTraceTests)
{
public:

    //  The newest entry is the instruction just before where the machine
    //  stands: stepping back once more lands on it, with the registers and
    //  cycle count it recorded.
    TEST_METHOD (TheNewestEntryIsTheInstructionStepBackLandsOn)
    {
        TestMachine               machine ("Apple2e");
        ReverseHost               host    (machine);
        ReverseResult             result;
        HRESULT                   hr      = S_OK;
        std::vector<TraceRecord>  entries;
        TraceRecord               newest;
        Cpu6502Registers          now     = {};
        DebugMemoryView           memory  (machine);
        Byte                      opcode  = 0;



        PrepareRecording (machine, host);

        for (int i = 0; i < 3; i++)
        {
            hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
            AssertSucceeded (hr, L"StepBack");
        }

        host.GetRecentTrace (s_kTraceRows, entries);

        Assert::IsFalse (entries.empty(), L"the instructions behind the position are listed");
        Assert::IsTrue  (entries.size() <= s_kTraceRows, L"no more than asked for");

        newest = entries.back();

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        now = machine.GetCpu()->GetCpu6502()->GetRegisters();

        Assert::AreEqual<int>      (now.pc, newest.pc, L"the PC it began with");
        Assert::AreEqual<int>      (now.a,  newest.a,  L"A");
        Assert::AreEqual<int>      (now.x,  newest.x,  L"X");
        Assert::AreEqual<int>      (now.sp, newest.sp, L"S");
        Assert::AreEqual<int>      (now.p,  newest.p,  L"P");
        Assert::AreEqual<uint64_t> (machine.GetCpu()->GetTotalCycles(), newest.cycles, L"the cycle count");
        Assert::IsTrue (memory.TryPeek (now.pc, opcode), L"the opcode is readable");
        Assert::AreEqual<int> (opcode, newest.opcode, L"the opcode, as the machine's banking shows it");
    }


    //  A seek builds no step table of its own; the host replays the
    //  stretch once so the trace has the instructions, and puts the machine
    //  back where the seek landed.
    TEST_METHOD (ASeekStillListsTheInstructionsBehindIt)
    {
        TestMachine               machine ("Apple2e");
        ReverseHost               host    (machine);
        ReverseResult             result;
        HRESULT                   hr      = S_OK;
        uint64_t                  target  = 0;
        std::vector<TraceRecord>  entries;



        PrepareRecording (machine, host);

        target = machine.GetPosition() - s_kTraceSeekBack;

        hr = host.Execute (ReverseCommand::Seek, target, nullptr, result);
        AssertSucceeded (hr, L"Seek");

        host.GetRecentTrace (s_kTraceRows, entries);

        Assert::AreEqual<uint64_t> (target, machine.GetPosition(), L"the machine is where the seek landed");
        Assert::IsTrue  (result.outcome == ReverseOutcome::Moved, L"and the command reports the move");
        Assert::IsFalse (entries.empty(), L"the instructions behind the position are listed");
        Assert::IsTrue  (entries.back().cycles < machine.GetCpu()->GetTotalCycles(), L"all of them before the position");
    }


    TEST_METHOD (LiveListsNothing)
    {
        TestMachine               machine ("Apple2e");
        ReverseHost               host    (machine);
        ReverseResult             result;
        HRESULT                   hr      = S_OK;
        std::vector<TraceRecord>  entries;



        PrepareRecording (machine, host);

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        hr = host.Execute (ReverseCommand::GoLive, 0, nullptr, result);
        AssertSucceeded (hr, L"GoLive");

        host.GetRecentTrace (s_kTraceRows, entries);

        Assert::IsTrue (entries.empty(), L"live, the live trace is the trace");
    }


    //  Behind live the band counts the disks holding writes their files do
    //  not; live, the automatic flushes save them, so nothing is counted.
    TEST_METHOD (TheStatusCountsUnsavedDisksBehindLive)
    {
        TestMachine      machine ("Apple2e");
        ReverseHost      host    (machine);
        ReverseResult    result;
        HRESULT          hr      = S_OK;
        DiskImageStore & store   = machine.GetDiskStore();



        PrepareRecording (machine, host);

        store.GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->SetLoadedForTest (true, true);

        Assert::AreEqual (1, store.CountUnsavedDisks(), L"the store counts the dirty disk");
        Assert::AreEqual (0, host.GetStatus().unsavedDisks, L"live: not reported");

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        store.GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->SetLoadedForTest (true, true);

        Assert::AreEqual (1, host.GetStatus().unsavedDisks, L"behind live: reported");

        store.GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->ClearDirty();

        Assert::AreEqual (0, host.GetStatus().unsavedDisks, L"saved: nothing reported");
    }


private:

    static void PrepareRecording (TestMachine & machine, ReverseHost & host)
    {
        HRESULT  hr = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kTraceBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kTraceWarmupCycles);
    }
};
