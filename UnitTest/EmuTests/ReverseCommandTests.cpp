#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kCommandWarmupCycles = 60000;
static constexpr size_t    s_kCommandSteps        = 3000;
static constexpr size_t    s_kCommandStepBacks    = 400;
static constexpr size_t    s_kCommandHookSteps    = 50;
static constexpr size_t    s_kCommandGapSteps     = 400;
static constexpr uint64_t  s_kCommandGapCycles    = 40000;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseCommandTests
//
//  The reverse commands against a recorded run of the rig's guest loop. The
//  live run is stepped one instruction at a time and remembers where each
//  instruction began and what the machine held there, so every command can
//  be checked against what running forward actually did.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseCommandTests)
{
public:

    struct Step
    {
        uint64_t  position = 0;
        Word      pc       = 0;
        uint64_t  cycle    = 0;
        uint64_t  checksum = 0;
    };


    //  Stepping back one instruction at a time, hundreds of times, must give
    //  the state that was live at each of those positions.
    TEST_METHOD (StepBackMatchesTheLiveRunAtEveryPosition)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             i          = 0;
        size_t             index      = 0;



        Record (machine, controller, steps, nullptr);

        for (i = 1; i <= s_kCommandStepBacks; i++)
        {
            index = steps.size() - 1 - i;

            hr = controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            Assert::AreEqual<uint64_t> (steps[index].position, machine.GetPosition(), L"one instruction back");
            Assert::AreEqual<Word>     (steps[index].pc,       machine.GetCpu()->GetPC(), L"PC");
            Assert::AreEqual<uint64_t> (steps[index].checksum, ReverseSessionRig::Checksum (machine),
                                        std::format (L"the whole machine at position {}", steps[index].position).c_str());
        }
    }


    TEST_METHOD (StepBackOverAReturnLandsOnTheJsr)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             call       = 0;
        size_t             back       = 0;



        Record (machine, controller, steps, nullptr);

        call = FindLast (steps, ReverseSessionRig::kCallSite, steps.size());
        back = call;

        while (steps[back].pc != ReverseSessionRig::kReturnSite)
        {
            back++;
        }

        hr = controller.SeekToPosition (steps[back].position, result);
        AssertSucceeded (hr, L"SeekToPosition after the return");

        hr = controller.StepBackOver (result);
        AssertSucceeded (hr, L"StepBackOver");

        Assert::AreEqual<uint64_t> (steps[call].position, result.position, L"back over the whole call");
        Assert::AreEqual<Word>     (ReverseSessionRig::kCallSite, machine.GetCpu()->GetPC());
        Assert::AreEqual<uint64_t> (steps[call].checksum, ReverseSessionRig::Checksum (machine), L"the state the JSR began with");

        hr = controller.StepBackOver (result);
        AssertSucceeded (hr, L"StepBackOver an ordinary instruction");

        Assert::AreEqual<uint64_t> (steps[call - 1].position, result.position, L"an ordinary instruction is one step");
        Assert::AreEqual<uint64_t> (steps[call - 1].checksum, ReverseSessionRig::Checksum (machine));
    }


    TEST_METHOD (StepBackOutLandsOnTheJsrThatEnteredTheRoutine)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             call       = 0;
        size_t             inside     = 0;



        Record (machine, controller, steps, nullptr);

        call   = FindLast (steps, ReverseSessionRig::kCallSite, steps.size());
        inside = call;

        // The PLA, with a byte pushed on top of the return address.
        while (steps[inside].pc != ReverseSessionRig::kRoutinePla)
        {
            inside++;
        }

        hr = controller.SeekToPosition (steps[inside].position, result);
        AssertSucceeded (hr, L"SeekToPosition inside the routine");

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
        Assert::AreEqual<uint64_t> (steps[call].position, result.position, L"out to the JSR");
        Assert::AreEqual<uint64_t> (steps[call].checksum, ReverseSessionRig::Checksum (machine));

        // The guest loop was entered by setting the PC, not by a call.
        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut of the top level");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"no call entered the top level");
        Assert::AreEqual<uint64_t> (controller.GetOldestPosition(), result.position);
    }


    TEST_METHOD (ReverseContinueStopsAtTheLatestBreakpointHit)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseProbe       probe;
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             hit        = 0;



        Record (machine, controller, steps, nullptr);

        probe.breakPc = ReverseSessionRig::kWriteRoutine;
        hit           = FindLast (steps, ReverseSessionRig::kWriteRoutine, steps.size());

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
        Assert::AreEqual<uint64_t> (steps[hit].position, result.position, L"the latest time the routine was entered");
        Assert::AreEqual<uint64_t> (steps[hit].checksum, ReverseSessionRig::Checksum (machine));

        hit = FindLast (steps, ReverseSessionRig::kWriteRoutine, hit);

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue again");

        Assert::AreEqual<uint64_t> (steps[hit].position, result.position, L"the time before that");
    }


    //  A breakpoint whose condition reads memory, evaluated against the
    //  machine as it was at each position.
    TEST_METHOD (ReverseContinueEvaluatesAMemoryConditionInThePast)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseProbe       probe;
        std::vector<Step>  steps;
        std::vector<Byte>  counters;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             hit        = 0;
        size_t             i          = 0;



        Record (machine, controller, steps, &counters);

        // A counter value from well inside the recording, at the loop top.
        hit = FindLast (steps, ReverseSessionRig::kLoopTop, steps.size() / 2);

        probe.breakPc       = ReverseSessionRig::kLoopTop;
        probe.conditionAddr = ReverseSessionRig::kCounter;
        probe.conditionByte = counters[hit];

        for (i = steps.size() - 1; i > hit; i--)
        {
            if (steps[i].pc == ReverseSessionRig::kLoopTop && counters[i] == probe.conditionByte)
            {
                hit = i;
                break;
            }
        }

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::AreEqual<uint64_t> (steps[hit].position, result.position, L"the latest loop top where the counter held the value");
        Assert::AreEqual<uint64_t> (steps[hit].checksum, ReverseSessionRig::Checksum (machine));
    }


    TEST_METHOD (ReverseContinueStopsAtTheLatestWatchpointHit)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseProbe       probe;
        std::vector<Step>  steps;
        std::vector<Step>  writes;
        ReverseResult      result;
        HRESULT            hr         = S_OK;



        probe.Watch (machine, ReverseSessionRig::kKeyStore);

        Record (machine, controller, steps, nullptr, &probe, &writes);

        Assert::IsTrue (writes.size() >= 2, L"the guest must have stored at least two keys");

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::AreEqual<uint64_t> (writes.back().position, result.position, L"just after the latest store to the watched byte");
        Assert::AreEqual<uint64_t> (writes.back().checksum, ReverseSessionRig::Checksum (machine));

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue again");

        Assert::AreEqual<uint64_t> (writes[writes.size() - 2].position, result.position, L"the store before that");
    }


    //  Targets in cycles: a scanline or a frame back lands on the first
    //  instruction boundary at or after the target cycle.
    TEST_METHOD (StepBackByScanlineAndFrameLandsOnTheFirstBoundaryAtOrAfterTheTarget)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        ReverseResult      before;
        HRESULT            hr         = S_OK;
        uint64_t           target     = 0;



        Record (machine, controller, steps, nullptr);

        for (uint64_t span : { ReverseController::kScanlineCycles, ReverseController::kFrameCycles })
        {
            target = machine.GetCpu()->GetTotalCycles() - span;

            hr = controller.StepBackCycles (span, result);
            AssertSucceeded (hr, L"StepBackCycles");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            Assert::IsTrue (result.cycle >= target, L"at or after the target");

            hr = controller.StepBack (before);
            AssertSucceeded (hr, L"StepBack");

            Assert::IsTrue (before.cycle < target, L"and the boundary before it is short of the target");
        }
    }


    //  Running the machine from the past, without a seek, replays the
    //  recorded future: every instruction gives the state the live run had
    //  there, recorded keys included, the history is kept the whole way, and
    //  the machine is live again once it reaches the end of it.
    TEST_METHOD (RunningFromThePastReplaysTheRecordedFuture)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           liveEnd    = 0;
        size_t             start      = 0;
        size_t             index      = 0;



        Record (machine, controller, steps, nullptr);

        liveEnd = machine.GetPosition();
        start   = steps.size() / 4;

        hr = controller.SeekToPosition (steps[start].position, result);
        AssertSucceeded (hr, L"SeekToPosition");

        Assert::IsTrue (controller.IsInHistory());

        for (index = start + 1; index < steps.size(); index++)
        {
            machine.StepOne();

            //  A key the host pressed is recorded when the guest reads it, so
            //  between the press and the read the live machine held a latch
            //  the replay sets only at the read. The path is compared at
            //  every position, and the whole machine at the end.
            Assert::AreEqual<uint64_t> (steps[index].position, machine.GetPosition(), L"one instruction on");
            Assert::AreEqual<Word>     (steps[index].pc,       machine.GetCpu()->GetPC(),
                                        std::format (L"the PC at position {}", steps[index].position).c_str());
            Assert::AreEqual<uint64_t> (steps[index].cycle,    machine.GetCpu()->GetTotalCycles(), L"the cycle count");

            if (index + 1 < steps.size())
            {
                Assert::IsTrue (controller.IsInHistory(), L"still replaying");
                Assert::AreEqual<uint64_t> (liveEnd, controller.GetLiveEndPosition(), L"the recorded future is kept");
            }
        }

        Assert::AreEqual<uint64_t> (steps.back().checksum, ReverseSessionRig::Checksum (machine), L"the whole machine at the end of history");

        machine.StepOne();

        Assert::IsFalse (controller.IsInHistory(), L"live once the end of history is reached");
        Assert::AreEqual<uint64_t> (liveEnd + 1, machine.GetPosition());
    }


    //  A change made in the past still drops the recorded future and makes
    //  the machine live where it stands.
    TEST_METHOD (AChangeInThePastDropsTheFuture)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           liveEnd    = 0;



        Record (machine, controller, steps, nullptr);

        liveEnd = machine.GetPosition();

        hr = controller.SeekToPosition (steps[steps.size() / 2].position, result);
        AssertSucceeded (hr, L"SeekToPosition");

        hr = controller.OnMachineChanged();
        AssertSucceeded (hr, L"OnMachineChanged");

        Assert::IsFalse (controller.IsInHistory(), L"live where the change was made");
        Assert::IsTrue (controller.GetLiveEndPosition() < liveEnd, L"and the future is gone");
    }


    //  One instruction at a time back over the whole recording, across at
    //  least one keyframe, then forward again to the live end: the whole
    //  machine must match the live run at every position, and each stretch's
    //  table is built once on the way back.
    TEST_METHOD (StepBackAndForwardOneAtATimeAcrossStretchesMatchesTheLiveRun)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             index      = 0;
        size_t             crossings  = 0;
        size_t             builds     = 0;



        // A key the host presses is in the live machine before any guest read
        // sees it, but a replay applies it only at that read, so the steps
        // between the two do not replay exactly; this run presses none.
        Record (machine, controller, steps, nullptr, nullptr, nullptr, false);

        crossings = CountKeyframesWithin (controller, steps.front().position, steps.back().position);
        Assert::IsTrue (crossings >= 1, L"the recording must cross a keyframe");

        for (index = steps.size() - 1; index > 0; index--)
        {
            hr = controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            CheckStep (machine, steps[index - 1], L"back");
        }

        builds = controller.GetTableBuildCount();
        Assert::IsTrue (builds >= crossings + 1 && builds <= crossings + 2, std::format (L"one table per stretch stepped into, not one per step: {} for {} keyframes crossed", builds, crossings).c_str());

        for (index = 1; index < steps.size(); index++)
        {
            hr = controller.StepForward (result);
            AssertSucceeded (hr, L"StepForward");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            CheckStep (machine, steps[index], L"forward");
        }

        Assert::IsFalse (controller.IsInHistory(), L"forward to the end of history makes the machine live");
    }


    //  The debugger's own breakpoints, a counting breakpoint and a write
    //  watchpoint are installed as they are when a debugger is attached; no
    //  reverse command may let a replayed instruction reach them. The stop
    //  test still sees the replayed write through its own sink.
    TEST_METHOD (AReplayNeverReachesTheDebuggersBreakpointsOrWatchpoints)
    {
        TestMachine                machine    ("Apple2e");
        ReverseController          controller (machine);
        MachineDebugTarget         target     (machine);
        RecordingNotificationSink  sink;
        DebugSession               session    (target, sink, RunState::Paused);
        BreakpointHandlers         handlers;
        ReverseProbe               probe;
        std::vector<Step>          steps;
        ReverseResult              result;
        HRESULT                    hr         = S_OK;
        int                        stopping   = 0;
        int                        counting   = 0;
        size_t                     i          = 0;



        session.AddHandler (&handlers);

        Record (machine, controller, steps, nullptr);

        stopping = AddBreakpoint (session, "BP 900");
        counting = AddBreakpoint (session, "BP 806");
        session.GetBreakpoints().TrySetFlags (counting, false, false);
        session.ExecuteLine ("BPMW 300");

        machine.SetDebugHook (&session);

        probe.watchAddr = ReverseSessionRig::kKeyStore;

        for (i = 0; i < s_kCommandHookSteps; i++)
        {
            hr = controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");
        }

        hr = controller.StepBackOver (result);
        AssertSucceeded (hr, L"StepBackOver");

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        hr = controller.SeekToPosition (steps.back().position, result);
        AssertSucceeded (hr, L"SeekToPosition");

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the stop test saw a replayed write to the watched byte");
        Assert::AreEqual<uint32_t> (0, GetBreakpointHits (session, stopping), L"the stopping breakpoint counted nothing");
        Assert::AreEqual<uint32_t> (0, GetBreakpointHits (session, counting), L"the counting breakpoint counted nothing");
        Assert::AreEqual<uint32_t> (0, session.GetWatchpoints().GetAll().front().hits, L"the debugger's watchpoint saw nothing");
        Assert::IsTrue (sink.stops.empty(), L"and nothing was reported to the debugger");
        Assert::IsTrue (machine.GetMemoryBus().GetWatchSink() == &session.GetWatchpoints(), L"the debugger's watch sink is back on the bus");

        // Live, the same hook counts: the wiring itself works. The stopping
        // breakpoint goes first, or the machine would stop on it.
        session.GetBreakpoints().TryClear (stopping);

        hr = controller.SeekToPosition (steps.back().position, result);
        AssertSucceeded (hr, L"SeekToPosition live");

        for (i = 0; i < s_kCommandSteps && GetBreakpointHits (session, counting) == 0; i++)
        {
            machine.StepOne();
        }

        Assert::IsTrue (GetBreakpointHits (session, counting) > 0, L"running live, the counting breakpoint counts");
    }


    //  At a Maximum speed the user chose, recording pauses; stepping back from
    //  where it resumed stops at the edge of the gap, and the history on
    //  either side of it is exact.
    TEST_METHOD (SteppingBackIntoASpeedGapStopsAtItsEdge)
    {
        TestMachine          machine    ("Apple2e");
        ReverseController    controller (machine);
        std::vector<Step>    before;
        std::vector<Step>    after;
        ReverseResult        result;
        HRESULT              hr         = S_OK;
        size_t               keyframes  = 0;
        size_t               i          = 0;
        Step                 gapStart;
        KeyframeInfo         resumed;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kCommandWarmupCycles);

        StepLive (machine, s_kCommandGapSteps, before);
        gapStart = before.back();

        hr = controller.SetUserMaximumSpeed (true);
        AssertSucceeded (hr, L"SetUserMaximumSpeed on");

        keyframes = controller.GetKeyframes().GetCount();
        machine.RunCycles (s_kCommandGapCycles);

        Assert::AreEqual (keyframes, controller.GetKeyframes().GetCount(), L"no keyframe is taken while paused");

        hr = controller.SetUserMaximumSpeed (false);
        AssertSucceeded (hr, L"SetUserMaximumSpeed off");

        resumed = controller.GetKeyframes().GetInfo (controller.GetKeyframes().GetCount() - 1);

        Assert::IsTrue (resumed.hasGapBefore, L"recording resumed with a keyframe after a gap");
        Assert::AreEqual<uint64_t> (gapStart.position, resumed.gapStart, L"the gap starts where recording paused");
        Assert::AreEqual<uint64_t> (machine.GetPosition(), resumed.position, L"and ends where it resumed");

        StepLive (machine, s_kCommandGapSteps, after);

        for (i = after.size() - 1; i > 0; i--)
        {
            hr = controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack after the gap");

            CheckStep (machine, after[i - 1], L"after the gap");
        }

        hr = controller.StepBack (result);
        AssertSucceeded (hr, L"StepBack into the gap");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryGap, L"a step into the gap reports it");
        CheckStep (machine, gapStart, L"at the edge of the gap");

        hr = controller.StepBack (result);
        AssertSucceeded (hr, L"StepBack before the gap");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
        CheckStep (machine, before[before.size() - 2], L"before the gap");

        hr = controller.StepForward (result);
        AssertSucceeded (hr, L"StepForward to the edge");

        CheckStep (machine, gapStart, L"forward to the edge");

        hr = controller.StepForward (result);
        AssertSucceeded (hr, L"StepForward across the gap");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryGap, L"a step across the gap reports it");
        CheckStep (machine, after.front(), L"where recording resumed");
    }


private:

    //  Warms the guest loop up, then steps it one instruction at a time,
    //  keeping each position, PC and checksum, and feeding a key at a few
    //  points. With a probe, each position just after the instruction that
    //  tripped its watchpoint goes into writes.
    static void Record (
        TestMachine        & machine,
        ReverseController  & controller,
        std::vector<Step>  & steps,
        std::vector<Byte>  * counters,
        ReverseProbe       * probe       = nullptr,
        std::vector<Step>  * writes      = nullptr,
        bool                 isPressing  = true)
    {
        static constexpr size_t  kKeyEvery = 1000;
        HRESULT                  hr        = S_OK;
        Step                     step;
        size_t                   i         = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        // Paddle 0 at zero keeps each pass of the loop short, so the
        // recording holds several calls to the write routine.
        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kCommandWarmupCycles);

        if (probe != nullptr)
        {
            probe->TakePendingStop();
        }

        for (i = 0; i <= s_kCommandSteps; i++)
        {
            if (isPressing && i % kKeyEvery == kKeyEvery / 2)
            {
                machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('a' + i / kKeyEvery));
            }

            step.position = machine.GetPosition();
            step.pc       = machine.GetCpu()->GetPC();
            step.cycle    = machine.GetCpu()->GetTotalCycles();
            step.checksum = ReverseSessionRig::Checksum (machine);
            steps.push_back (step);

            if (counters != nullptr)
            {
                counters->push_back (machine.GetMemoryBus().GetShadowReadPage (ReverseSessionRig::kCounter)[ReverseSessionRig::kCounter & 0xFF]);
            }

            if (probe != nullptr && writes != nullptr && probe->TakePendingStop())
            {
                writes->push_back (step);
            }

            if (i < s_kCommandSteps)
            {
                machine.StepOne();
            }
        }
    }


    //  Steps the machine live count times, keeping each position, PC, cycle
    //  and checksum, the last one being where it stops.
    static void StepLive (TestMachine & machine, size_t count, std::vector<Step> & steps)
    {
        Step    step;
        size_t  i = 0;



        for (i = 0; i <= count; i++)
        {
            step.position = machine.GetPosition();
            step.pc       = machine.GetCpu()->GetPC();
            step.cycle    = machine.GetCpu()->GetTotalCycles();
            step.checksum = ReverseSessionRig::Checksum (machine);
            steps.push_back (step);

            if (i < count)
            {
                machine.StepOne();
            }
        }
    }


    static void CheckStep (TestMachine & machine, const Step & expected, const wchar_t * when)
    {
        Assert::AreEqual<uint64_t> (expected.position, machine.GetPosition(),     std::format (L"{}: position", when).c_str());
        Assert::AreEqual<Word>     (expected.pc,       machine.GetCpu()->GetPC(), std::format (L"{}: PC at position {}", when, expected.position).c_str());
        Assert::AreEqual<uint64_t> (expected.checksum, ReverseSessionRig::Checksum (machine),
                                    std::format (L"{}: the whole machine at position {}", when, expected.position).c_str());
    }


    //  The keyframes taken after first and at or before last.
    static size_t CountKeyframesWithin (const ReverseController & controller, uint64_t first, uint64_t last)
    {
        const KeyframeStore  & store = controller.GetKeyframes();
        size_t                 count = 0;
        size_t                 i     = 0;



        for (i = 0; i < store.GetCount(); i++)
        {
            count += (store.GetInfo (i).position > first && store.GetInfo (i).position <= last) ? 1 : 0;
        }

        return count;
    }


    static int AddBreakpoint (DebugSession & session, const std::string & line)
    {
        size_t  before = session.GetBreakpoints().GetAll().size();



        session.ExecuteLine (line);

        Assert::AreEqual (before + 1, session.GetBreakpoints().GetAll().size(), L"the breakpoint was set");

        return session.GetBreakpoints().GetAll().back().id;
    }


    static uint32_t GetBreakpointHits (DebugSession & session, int id)
    {
        Breakpoint  entry;



        Assert::IsTrue (session.GetBreakpoints().TryFind (id, entry), L"no such breakpoint");

        return entry.hits;
    }


    //  The last step before end whose PC is pc.
    static size_t FindLast (const std::vector<Step> & steps, Word pc, size_t end)
    {
        size_t  i = end;



        while (i > 0)
        {
            i--;

            if (steps[i].pc == pc)
            {
                return i;
            }
        }

        Assert::Fail (std::format (L"the recording never reached ${:04X}", pc).c_str());
        return 0;
    }
};
