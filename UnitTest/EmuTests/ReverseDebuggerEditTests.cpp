#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kEditWarmupCycles = 60000;
static constexpr int       s_kEditBudgetMb     = 16;
static constexpr uint64_t  s_kEditStepsBack    = 50;
static constexpr Word      s_kEditAddress      = 0x0310;
static constexpr Byte      s_kEditValue        = 0xA5;
static constexpr Byte      s_kEditA            = 0x3C;
static constexpr Word      s_kEditPokes        = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseDebuggerEditTests
//
//  A memory, register or I/O edit from the debugger is not a recorded input,
//  so the recorded future no longer follows from the machine. Made behind
//  live it drops that future at once and the machine is live where it
//  stands; made live it is kept as a boundary, so stepping back past it and
//  forward again comes back to the edited machine.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseDebuggerEditTests)
{
public:

    TEST_METHOD (APokeBehindLiveDropsTheRecordedFuture)
    {
        TestMachine         machine ("Apple2e");
        ReverseHost         host    (machine);
        MachineDebugTarget  target  (machine);
        ReverseResult       result;
        HRESULT             hr      = S_OK;
        uint64_t            here    = 0;
        bool                isPoked = false;



        PrepareRecording (machine, host);
        StepBack (machine, host);

        here = machine.GetPosition();

        Assert::IsTrue (host.IsBehindLive(), L"behind live before the edit");

        isPoked = target.TryPoke (s_kEditAddress, s_kEditValue);
        Assert::IsTrue (isPoked, L"poked");

        CheckLiveAfterEdit (machine, host, here);

        hr = host.Execute (ReverseCommand::StepForward, 0, nullptr, result);
        AssertSucceeded (hr, L"StepForward");

        Assert::AreEqual<uint64_t> (here, machine.GetPosition(), L"nothing recorded ahead to step into");
    }


    TEST_METHOD (ARegisterEditBehindLiveDropsTheRecordedFuture)
    {
        TestMachine         machine   ("Apple2e");
        ReverseHost         host      (machine);
        MachineDebugTarget  target    (machine);
        Cpu6502Registers    registers = {};
        uint64_t            here      = 0;



        PrepareRecording (machine, host);
        StepBack (machine, host);

        here      = machine.GetPosition();
        registers = target.GetRegisters();
        registers.a = s_kEditA;

        target.SetRegisters (registers);

        CheckLiveAfterEdit (machine, host, here);
    }


    TEST_METHOD (AnIoWriteBehindLiveDropsTheRecordedFuture)
    {
        constexpr Word      kTextOff = 0xC051;
        TestMachine         machine  ("Apple2e");
        ReverseHost         host     (machine);
        MachineDebugTarget  target   (machine);
        uint64_t            here     = 0;



        PrepareRecording (machine, host);
        StepBack (machine, host);

        here = machine.GetPosition();

        target.WriteIo (kTextOff, 0);

        CheckLiveAfterEdit (machine, host, here);
    }


    //  Live, a run of pokes is one boundary keyframe, taken before the next
    //  reverse command; going back past it and returning to the live end
    //  finds the poked byte, not what the recording held there.
    TEST_METHOD (APokeWhileLiveSurvivesAStepBackAndReturn)
    {
        TestMachine         machine   ("Apple2e");
        ReverseHost         host      (machine);
        MachineDebugTarget  target    (machine);
        ReverseResult       result;
        HRESULT             hr        = S_OK;
        size_t              keyframes = 0;
        Byte                value     = 0;
        Word                i         = 0;



        PrepareRecording (machine, host);

        keyframes = host.GetController().GetKeyframes().GetCount();

        for (i = 0; i < s_kEditPokes; i++)
        {
            target.TryPoke (static_cast<Word> (s_kEditAddress + i), s_kEditValue);
        }

        Assert::AreEqual (keyframes, host.GetController().GetKeyframes().GetCount(), L"no keyframe per poke");

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        Assert::AreEqual (keyframes + 1, host.GetController().GetKeyframes().GetCount(), L"the pokes became one boundary");

        hr = host.Execute (ReverseCommand::GoLive, 0, nullptr, result);
        AssertSucceeded (hr, L"GoLive");

        target.TryPeek (s_kEditAddress, value);

        Assert::AreEqual<int> (s_kEditValue, value, L"the poked byte is back");
        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"and the replay matched");
    }


private:

    static void PrepareRecording (TestMachine & machine, ReverseHost & host)
    {
        HRESULT  hr = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kEditBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kEditWarmupCycles);
    }


    static void StepBack (TestMachine & machine, ReverseHost & host)
    {
        ReverseResult  result;
        HRESULT        hr     = S_OK;



        hr = host.Execute (ReverseCommand::Seek, machine.GetPosition() - s_kEditStepsBack, nullptr, result);
        AssertSucceeded (hr, L"Seek back");
    }


    //  After an edit behind live: the machine is live where the edit was
    //  made, history ends there, and the host's input flows again once the
    //  gate is synced, as the shell does on its next tick.
    static void CheckLiveAfterEdit (TestMachine & machine, ReverseHost & host, uint64_t here)
    {
        Assert::IsFalse (host.IsBehindLive(), L"the edit made the machine live");
        Assert::AreEqual<uint64_t> (here, machine.GetPosition(), L"where it stood");
        Assert::AreEqual<uint64_t> (here, host.GetController().GetLiveEndPosition(), L"history now ends here");

        host.SyncInputGate();

        Assert::IsFalse (machine.GetHostInputGate().IsHeld(), L"host input flows again");
    }
};
