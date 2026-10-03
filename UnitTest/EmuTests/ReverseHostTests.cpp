#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Shell/CpuManager.h"
#include "Shell/MachineGamePortSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kHostWarmupCycles = 60000;
static constexpr uint64_t  s_kHostGapCycles    = KeyframeSettings::kFrameCycles * 3;
static constexpr int       s_kHostBudgetMb     = 16;
static constexpr Byte      s_kHostPaddle       = 0x5A;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseHostTests
//
//  Reverse execution as the shell hosts it: recording from when a machine is
//  built, paused at a Maximum speed the user chose and not at one raised
//  automatically, moved by commands, and holding the host's input back while
//  the machine is behind live.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseHostTests)
{
public:

    TEST_METHOD (StartingRecordsTheMachineAndStoppingDetaches)
    {
        TestMachine  machine ("Apple2e");
        ReverseHost  host    (machine);
        HRESULT      hr      = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kHostBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        Assert::IsTrue (host.IsRecording(), L"recording");
        Assert::IsTrue (machine.GetHistoryRecorder() == &host.GetController(), L"the machine reports to the controller");
        Assert::IsTrue (machine.GetInputJournal().IsOn(), L"the journal is on");

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kHostBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording again, as a machine switch does");

        Assert::AreEqual<size_t> (1, host.GetController().GetKeyframes().GetCount(), L"a restart begins history again");

        host.StopRecording();

        Assert::IsFalse (host.IsRecording(), L"stopped");
        Assert::IsTrue  (machine.GetHistoryRecorder() == nullptr, L"detached");
    }


    TEST_METHOD (TheSettingsBecomeTheKeyframeBudgetAndInterval)
    {
        constexpr size_t  kBytesPerMb = 1024 * 1024;
        constexpr int     kFrames     = 7;
        ReverseSettings   settings    = ReverseHost::MakeSettings (s_kHostBudgetMb, kFrames);



        Assert::AreEqual<size_t>   (s_kHostBudgetMb * kBytesPerMb,          settings.keyframes.budgetBytes);
        Assert::AreEqual<uint64_t> (KeyframeSettings::kFrameCycles * kFrames, settings.keyframes.intervalCycles);
    }


    //  The user's Maximum speed pauses recording, so no keyframe is taken
    //  while the machine runs there, and leaving it resumes with a keyframe
    //  marked as following a gap. The debugger's own full-speed run is an
    //  automatic raise and records on.
    TEST_METHOD (OnlyAMaximumSpeedTheUserChosePausesRecording)
    {
        TestMachine   machine ("Apple2e");
        ReverseHost   host    (machine);
        CpuManager    cpu;
        HRESULT       hr      = S_OK;
        size_t        count   = 0;
        KeyframeInfo  resumed;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kHostBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        machine.RunCycles (s_kHostWarmupCycles);

        cpu.SetSpeedMode (SpeedMode::Maximum, SpeedChooser::Automatic);

        hr = host.OnFrame (cpu.IsUserMaximumSpeed());
        AssertSucceeded (hr, L"OnFrame at an automatic Maximum");

        Assert::IsFalse (host.GetController().IsPaused(), L"an automatic raise keeps recording");

        count = host.GetController().GetKeyframes().GetCount();
        machine.RunCycles (s_kHostGapCycles);

        Assert::IsTrue (host.GetController().GetKeyframes().GetCount() > count, L"keyframes are taken at an automatic Maximum");

        cpu.SetSpeedMode (SpeedMode::Maximum, SpeedChooser::User);

        hr = host.OnFrame (cpu.IsUserMaximumSpeed());
        AssertSucceeded (hr, L"OnFrame at the user's Maximum");

        Assert::IsTrue (host.GetController().IsPaused(), L"the user's Maximum pauses recording");

        count = host.GetController().GetKeyframes().GetCount();
        machine.RunCycles (s_kHostGapCycles);

        Assert::AreEqual (count, host.GetController().GetKeyframes().GetCount(), L"no keyframe while paused");

        cpu.SetSpeedMode (SpeedMode::Authentic, SpeedChooser::User);

        hr = host.OnFrame (cpu.IsUserMaximumSpeed());
        AssertSucceeded (hr, L"OnFrame back at 1x");

        resumed = host.GetController().GetKeyframes().GetInfo (host.GetController().GetKeyframes().GetCount() - 1);

        Assert::IsFalse (host.GetController().IsPaused(), L"recording resumed");
        Assert::IsTrue  (resumed.hasGapBefore, L"the keyframe where it resumed follows a gap");
    }


    //  The gate is held from the moment a command moves the machine into
    //  history until it is live again, and the live callback is told once.
    TEST_METHOD (TheHostInputGateIsHeldWhileBehindLive)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;
        uint64_t       liveEnd = 0;
        int            lives   = 0;



        PrepareRecording (machine, host);

        host.SetLiveCallback ([&lives] () { lives++; });

        liveEnd = machine.GetPosition();

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"moved");
        Assert::AreEqual<uint64_t> (liveEnd - 1, machine.GetPosition(), L"one instruction back");
        Assert::IsTrue (host.IsBehindLive(), L"behind live");
        Assert::IsTrue (machine.GetHostInputGate().IsHeld(), L"host input is held back");

        hr = host.Execute (ReverseCommand::StepBackOver, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBackOver");

        Assert::IsTrue (machine.GetHostInputGate().IsHeld(), L"still held");
        Assert::AreEqual (0, lives, L"not live yet");

        hr = host.Execute (ReverseCommand::GoLive, 0, nullptr, result);
        AssertSucceeded (hr, L"GoLive");

        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"at the live end");
        Assert::IsFalse (host.IsBehindLive(), L"live");
        Assert::IsFalse (machine.GetHostInputGate().IsHeld(), L"host input flows again");
        Assert::AreEqual (1, lives, L"the live callback ran once");
    }


    TEST_METHOD (SeekAndStepForwardMoveThroughTheRecordedFuture)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;
        uint64_t       liveEnd = 0;
        uint64_t       target  = 0;



        PrepareRecording (machine, host);

        liveEnd = machine.GetPosition();
        target  = liveEnd - 100;

        hr = host.Execute (ReverseCommand::Seek, target, nullptr, result);
        AssertSucceeded (hr, L"Seek");

        Assert::AreEqual<uint64_t> (target, machine.GetPosition(), L"at the target");
        Assert::AreEqual<uint64_t> (target, result.position, L"the result says so");

        hr = host.Execute (ReverseCommand::StepForward, 0, nullptr, result);
        AssertSucceeded (hr, L"StepForward");

        Assert::AreEqual<uint64_t> (target + 1, machine.GetPosition(), L"one instruction on");
        Assert::IsTrue (host.IsBehindLive(), L"still behind live");
    }


    TEST_METHOD (StepBackAtTheOldestPositionReportsTheStartOfHistory)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;



        PrepareRecording (machine, host);

        hr = host.Execute (ReverseCommand::Seek, host.GetController().GetOldestPosition(), nullptr, result);
        AssertSucceeded (hr, L"Seek to the oldest position");

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"at the start of history");
    }


    //  Reverse continue needs a stop test; without one nothing moves and the
    //  command fails rather than claiming a landing.
    TEST_METHOD (ReverseContinueWithoutAStopTestFailsInPlace)
    {
        TestMachine    machine  ("Apple2e");
        ReverseHost    host     (machine);
        ReverseResult  result;
        HRESULT        hr       = S_OK;
        uint64_t       position = 0;



        PrepareRecording (machine, host);

        position = machine.GetPosition();

        hr = host.Execute (ReverseCommand::ReverseContinue, 0, nullptr, result);

        Assert::IsTrue (FAILED (hr), L"no stop test, no reverse continue");
        Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the machine did not move");
        Assert::IsFalse (machine.GetHostInputGate().IsHeld(), L"and the host's input still flows");
    }


    TEST_METHOD (ACommandWithoutRecordingFails)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);

        Assert::IsTrue (FAILED (hr), L"nothing recorded, nothing to step back into");
        Assert::IsFalse (machine.GetHostInputGate().IsHeld(), L"the gate is not left held");
    }


    //  A game-port write from the host while the machine is behind live is
    //  not written; the mixer keeps it pending, and it lands once the machine
    //  is live again.
    TEST_METHOD (AGamePortWriteWaitsUntilTheMachineIsLive)
    {
        TestMachine          machine   ("Apple2e");
        ReverseHost          host      (machine);
        MachineGamePortSink  sink      (machine.GetLifetimeLock(), [&machine] () { return GetIieTargets (machine); });
        ReverseResult        result;
        HRESULT              hr        = S_OK;
        GamePortState        state;
        bool                 isApplied = false;



        sink.SetInputGate (&machine.GetHostInputGate());

        PrepareRecording (machine, host);

        state.paddle[0] = s_kHostPaddle;

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        isApplied = sink.TryApply (state, nullptr);

        Assert::IsFalse (isApplied, L"behind live the write is refused and stays pending");
        Assert::AreNotEqual<int> (s_kHostPaddle, machine.GetRefs().iieSoftSwitches->GetPaddle (0), L"the paddle kept its recorded value");

        hr = host.Execute (ReverseCommand::GoLive, 0, nullptr, result);
        AssertSucceeded (hr, L"GoLive");

        isApplied = sink.TryApply (state, nullptr);

        Assert::IsTrue (isApplied, L"live, the write goes through");
        Assert::AreEqual<int> (s_kHostPaddle, machine.GetRefs().iieSoftSwitches->GetPaddle (0), L"the paddle moved");
    }


    TEST_METHOD (AHeldGateTurnsAWriterAway)
    {
        HostInputGate                        gate;
        std::shared_lock<std::shared_mutex>  lock;
        bool                                 isOpen = false;



        isOpen = gate.TryEnter (lock);
        Assert::IsTrue (isOpen, L"open at first");
        lock.unlock();

        gate.Hold();

        isOpen = gate.TryEnter (lock);
        Assert::IsFalse (isOpen, L"held");
        lock.unlock();

        gate.Release();

        isOpen = gate.TryEnter (lock);
        Assert::IsTrue (isOpen, L"open again");
    }


private:

    //  The //e's paddles, the targets the game-port test writes to.
    static GamePortTargets GetIieTargets (TestMachine & machine)
    {
        constexpr size_t  kAxes   = 2;
        GamePortTargets   targets;



        targets.iieSwitches = machine.GetRefs().iieSoftSwitches;
        targets.axisCount   = kAxes;

        return targets;
    }


    static void PrepareRecording (TestMachine & machine, ReverseHost & host)
    {
        HRESULT  hr = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kHostBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kHostWarmupCycles);
    }
};
