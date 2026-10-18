#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/DivergenceGate.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Shell/CpuManager.h"
#include "Shell/MachineGamePortSink.h"
#include "HResultAssert.h"
#include "Debugger/Reverse/HistoryStatus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kHostWarmupCycles = 60000;
static constexpr uint64_t  s_kHostGapCycles    = KeyframeSettings::kFrameCycles * 3;
static constexpr int       s_kHostBudgetMb     = 16;
static constexpr Byte      s_kHostPaddle       = 0x5A;
static uint64_t            s_wallClock         = 0;





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

    //  The keyframe store's wall clock, held where a test puts it.
    static uint64_t  ReadWallClock() { return s_wallClock; }


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


    //  A minute spent in history and then going live leaves no hole in the
    //  timeline: the machine carries on from the old live end, keyframes
    //  keep their cycle spacing, and the cells are placed by machine time.
    //  Only the wall time stamped on the next keyframe jumps, which a label
    //  shows and nothing lays out by.
    TEST_METHOD (TimeSpentInHistoryLeavesNoHoleInTheTimeline)
    {
        constexpr uint64_t                 kTicksPerSecond = 10'000'000;
        constexpr uint64_t                 kMinuteTicks    = 60 * kTicksPerSecond;
        constexpr int                      kCellCount      = 8;
        TestMachine                        machine ("Apple2e");
        ReverseHost                        host    (machine);
        ReverseResult                      result;
        HRESULT                            hr      = S_OK;
        uint64_t                           liveEnd = 0;
        uint64_t                           step    = 0;
        uint64_t                           span    = 0;
        uint64_t                           widest  = 0;
        uint64_t                           mean    = 0;
        uint64_t                           wallGap = 0;
        size_t                             count   = 0;
        KeyframeStore                    & store   = host.GetController().GetKeyframes();
        std::vector<HistoryThumbnailCell>  cells;
        KeyframeInfo                       before;
        KeyframeInfo                       after;



        s_wallClock = kMinuteTicks;
        store.SetWallClock (&ReadWallClock);

        PrepareRecording (machine, host);

        liveEnd = machine.GetPosition();
        count   = store.GetCount();

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        s_wallClock += kMinuteTicks;

        hr = host.Execute (ReverseCommand::GoLive, 0, nullptr, result);
        AssertSucceeded (hr, L"GoLive");

        Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"live again from the old live end");
        Assert::AreEqual (count, store.GetCount(), L"no history was dropped");

        machine.RunCycles (s_kHostGapCycles);

        Assert::IsTrue (store.GetCount() > count, L"keyframes taken after going live");

        before  = store.GetInfo (count - 1);
        after   = store.GetInfo (count);
        wallGap = after.wallTime - before.wallTime;

        Assert::IsTrue (wallGap >= kMinuteTicks, L"the wall time stamped after going live jumps by the minute");
        Assert::IsTrue (after.cycle - before.cycle <= KeyframeSettings::kFrameCycles * 2, L"but the machine time between them is one interval");

        HistoryThumbnails::PlanCells (store, kCellCount, step, cells);

        Assert::IsTrue (cells.size() > 2, L"cells were planned");

        for (size_t i = 1; i < cells.size(); i++)
        {
            span   = cells[i].cycle - cells[i - 1].cycle;
            widest = (std::max) (widest, span);
        }

        mean = (cells.back().cycle - cells.front().cycle) / (cells.size() - 1);

        Assert::IsTrue (widest <= mean * 2, L"no cell spans the minute: they are spaced by machine time");
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


    //  The timeline's drag seeks by cycle: the machine lands on the first
    //  instruction boundary at or after it, still behind live.
    TEST_METHOD (SeekCycleLandsAtOrAfterTheCycle)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;
        uint64_t       target  = 0;



        PrepareRecording (machine, host);

        target = (host.GetController().GetKeyframes().GetInfo (0).cycle + machine.GetCpu()->GetTotalCycles()) / 2;

        hr = host.Execute (ReverseCommand::SeekCycle, target, nullptr, result);
        AssertSucceeded (hr, L"SeekCycle");

        Assert::IsTrue (machine.GetCpu()->GetTotalCycles() >= target, L"at or after the cycle");
        Assert::IsTrue (machine.GetCpu()->GetTotalCycles() < target + ReverseController::kScanlineCycles, L"within an instruction of it");
        Assert::IsTrue (host.IsBehindLive(), L"behind live");
    }


    //  Diverging is the yes to the question asked before a change behind
    //  live: the recorded future is dropped where the machine stands, which
    //  is live from then on, with the gate released and the callback told.
    //  Live, there is nothing to drop.
    TEST_METHOD (DivergingBehindLiveDropsTheFutureAndGoesLive)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HRESULT        hr      = S_OK;
        uint64_t       liveEnd = 0;
        uint64_t       target  = 0;
        size_t         count   = 0;
        int            lives   = 0;



        PrepareRecording (machine, host);

        host.SetLiveCallback ([&lives] () { lives++; });

        count = host.GetController().GetKeyframes().GetCount();

        hr = host.Diverge();
        AssertSucceeded (hr, L"Diverge while live");

        Assert::AreEqual (count, host.GetController().GetKeyframes().GetCount(), L"live, nothing is dropped or kept");

        liveEnd = machine.GetPosition();
        target  = liveEnd - 100;

        hr = host.Execute (ReverseCommand::Seek, target, nullptr, result);
        AssertSucceeded (hr, L"Seek");

        hr = host.Diverge();
        AssertSucceeded (hr, L"Diverge behind live");

        Assert::IsFalse (host.IsBehindLive(), L"live where it stood");
        Assert::AreEqual<uint64_t> (target, machine.GetPosition(), L"the machine did not move");
        Assert::AreEqual<uint64_t> (target, host.GetController().GetLiveEndPosition(), L"the future is gone");
        Assert::IsFalse (machine.GetHostInputGate().IsHeld(), L"host input flows again");
        Assert::AreEqual (1, lives, L"the live callback ran once");
    }


    //  Behind live the status holds where the machine stands, in cycles and
    //  by the host's clock, for the timeline's line and the caption.
    TEST_METHOD (TheStatusHoldsThePlayheadCycleAndClock)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReverseResult  result;
        HistoryStatus  status;
        HRESULT        hr      = S_OK;



        PrepareRecording (machine, host);

        hr = host.Execute (ReverseCommand::Seek, machine.GetPosition() - 100, nullptr, result);
        AssertSucceeded (hr, L"Seek");

        status = host.GetStatus();

        Assert::AreEqual<uint64_t> (machine.GetCpu()->GetTotalCycles(), status.cycle, L"the machine's cycle count");
        Assert::IsTrue (status.wallTime >= host.GetController().GetKeyframes().GetInfo (0).wallTime, L"no earlier than history begins");
        Assert::IsTrue (status.wallTime != 0, L"a host time");
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


    //  Owner report 2026-10-05: rewound in a game and played on, a joystick
    //  moved behind live asked nothing. Behind live nothing is written: a
    //  deflection or a button press is held, line by line, for the guest's
    //  reads to judge, and a position the host already held is not.
    TEST_METHOD (AStickOrButtonBehindLiveIsHeldNotWritten)
    {
        constexpr Byte       kRecorded = 0;
        constexpr Byte       kFarEnd   = 255;
        TestMachine          machine   ("Apple2e");
        ReverseHost          host      (machine);
        MachineGamePortSink  sink      (machine.GetLifetimeLock(), [&machine] () { return GetIieTargetsWithKeys (machine); });
        DivergenceGate       gate;
        ReverseResult        result;
        HRESULT              hr        = S_OK;
        GamePortState        rest;
        GamePortState        deflected;
        GamePortState        pressed;
        int                  helds     = 0;
        bool                 isApplied = false;



        sink.SetInputGate      (&machine.GetHostInputGate());
        sink.SetDivergenceGate (&gate, [&helds] () { helds++; });

        PrepareRecording (machine, host);

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        Assert::AreEqual<int> (kRecorded, machine.GetRefs().iieSoftSwitches->GetPaddle (0), L"the recording has paddle 0 at its near end");

        deflected.paddle[0] = kFarEnd;
        pressed.buttons.set (0);

        isApplied = sink.TryApply (rest, &rest);

        Assert::IsFalse (isApplied, L"behind live nothing is written");
        Assert::IsFalse (gate.IsHolding(), L"the state the host held holds nothing");

        isApplied = sink.TryApply (deflected, &rest);

        Assert::IsFalse (isApplied);
        Assert::IsTrue  (gate.IsHolding(), L"a deflection is held");
        Assert::IsFalse (gate.IsAsking(),  L"with no question yet");
        Assert::IsTrue  (gate.GetHeldLines().paddles[0] == std::optional<Byte> (kFarEnd));
        Assert::AreEqual<int> (kRecorded, machine.GetRefs().iieSoftSwitches->GetPaddle (0), L"the paddle kept its recorded value");

        isApplied = sink.TryApply (pressed, &rest);

        Assert::IsFalse (isApplied);
        Assert::IsTrue  (gate.GetHeldLines().buttons[0] == std::optional<bool> (true), L"Open Apple pressed is held");
        Assert::IsFalse (gate.GetHeldLines().paddles[0].has_value(), L"and the stick let go of is not");
        Assert::IsFalse (machine.GetRefs().iieKeyboard->IsOpenApplePressed(), L"Open Apple kept its recorded state");
        Assert::AreEqual (3, helds, L"the shell hears of each refused write");
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


    //  The //e's paddles and its Apple keys, which are its buttons.
    static GamePortTargets GetIieTargetsWithKeys (TestMachine & machine)
    {
        GamePortTargets  targets = GetIieTargets (machine);



        targets.iieKeyboard = machine.GetRefs().iieKeyboard;

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
