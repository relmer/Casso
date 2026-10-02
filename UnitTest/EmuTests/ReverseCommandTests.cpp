#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kCommandWarmupCycles = 60000;
static constexpr size_t    s_kCommandSteps        = 3000;
static constexpr size_t    s_kCommandStepBacks    = 400;





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
        HRESULT            hr         = S_OK;
        uint64_t           target     = 0;
        UndoRecord         before;



        Record (machine, controller, steps, nullptr);

        for (uint64_t span : { ReverseController::kScanlineCycles, ReverseController::kFrameCycles })
        {
            target = machine.GetCpu()->GetTotalCycles() - span;

            hr = controller.StepBackCycles (span, result);
            AssertSucceeded (hr, L"StepBackCycles");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            Assert::IsTrue (result.cycle >= target, L"at or after the target");
            Assert::IsTrue (controller.GetRing().TryGetRecord (result.position - 1, before), L"the ring holds the instruction before");
            Assert::IsTrue (before.cycle < target, L"and the boundary before it is short of the target");
        }
    }


    //  Running the machine from the past, without a seek, drops the recorded
    //  future, and a change reported in the past does the same.
    TEST_METHOD (RunningFromThePastDropsTheFuture)
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

        Assert::IsTrue (controller.IsInHistory());
        Assert::AreEqual<uint64_t> (liveEnd, controller.GetLiveEndPosition(), L"the recorded future is kept");

        machine.StepOne();

        Assert::IsFalse (controller.IsInHistory(), L"running from the past made the machine live there");
        Assert::IsTrue (controller.GetLiveEndPosition() < liveEnd, L"and the future is gone");
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
        ReverseProbe       * probe  = nullptr,
        std::vector<Step>  * writes = nullptr)
    {
        static constexpr size_t  kKeyEvery = 1000;
        HRESULT                  hr        = S_OK;
        Step                     step;
        size_t                   i         = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (true));
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
            if (i % kKeyEvery == kKeyEvery / 2)
            {
                machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('a' + i / kKeyEvery));
            }

            step.position = machine.GetPosition();
            step.pc       = machine.GetCpu()->GetPC();
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
