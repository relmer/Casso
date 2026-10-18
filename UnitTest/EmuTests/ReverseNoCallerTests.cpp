#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/CallStack.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t  s_kNoCallerWarmupSteps = 20000;
static constexpr size_t  s_kNoCallerStepLimit   = 20000;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseNoCallerTests
//
//  Step back out asks the call record, kept as the machine runs, whether any
//  call in history entered the code now running. The rig's guest loop runs
//  at the top level, entered by setting the PC, and calls its write routine
//  every sixteenth pass; the record is fed every instruction, as the
//  debugger feeds it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseNoCallerTests)
{
public:

    //  At the top level nothing called the loop: the answer comes at once,
    //  without a replay, a search or a move.
    TEST_METHOD (NoCallerAtTheTopLevelAnswersWithoutReplaying)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        CallStackRecorder  recorder;
        ReplayControl      control;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           position   = 0;



        Begin (machine, recorder);
        Start (machine, controller, recorder);
        RunRecorded (machine, recorder, s_kNoCallerWarmupSteps);
        RunRecordedTo (machine, recorder, ReverseSessionRig::kCallSite);

        controller.SetReplayControl (&control);

        position = machine.GetPosition();

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::NoCaller, L"no call entered the top level");
        Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the machine stays where it is");
        Assert::AreEqual<uint64_t> (position, result.position);
        Assert::IsFalse (controller.IsInHistory(),                    L"still live");
        Assert::AreEqual<size_t> (0, controller.GetTableBuildCount(), L"nothing was replayed");
        Assert::AreEqual (ReplayControl::kNoProgress, control.progress.load(), L"no search reported progress");
    }


    //  Inside the routine, the record holds the JSR, made inside history, so
    //  the search runs and lands on it.
    TEST_METHOD (CallerInsideHistoryIsStillFound)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        CallStackRecorder  recorder;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           call       = 0;



        Begin (machine, recorder);
        Start (machine, controller, recorder);
        RunRecorded (machine, recorder, s_kNoCallerWarmupSteps);
        RunRecordedTo (machine, recorder, ReverseSessionRig::kCallSite);

        call = machine.GetPosition();

        RunRecordedTo (machine, recorder, ReverseSessionRig::kRoutinePla);

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the caller was found");
        Assert::AreEqual<uint64_t> (call, result.position, L"out to the JSR");
        Assert::AreEqual<Word>     (ReverseSessionRig::kCallSite, machine.GetCpu()->GetPC());
    }


    //  The routine was entered before history began, so no caller is in
    //  history: reported at once as no caller, not by searching back to the
    //  start of history.
    TEST_METHOD (RoutineEnteredBeforeHistoryHasNoCallerInHistory)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        CallStackRecorder  recorder;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           position   = 0;



        Begin (machine, recorder);
        RunRecorded (machine, recorder, s_kNoCallerWarmupSteps);
        RunRecordedTo (machine, recorder, ReverseSessionRig::kCallSite);
        RunRecorded (machine, recorder, 1);

        Assert::AreEqual<Word> (ReverseSessionRig::kWriteRoutine, machine.GetCpu()->GetPC(), L"inside the routine");

        Start (machine, controller, recorder);
        RunRecordedTo (machine, recorder, ReverseSessionRig::kRoutinePla);

        position = machine.GetPosition();

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::NoCaller, L"the JSR came before history");
        Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the machine stays where it is");
        Assert::AreEqual<size_t> (0, controller.GetTableBuildCount(), L"nothing was replayed");
    }


    //  A record that began after history did cannot speak for the part
    //  before it, so the search runs as it would without one.
    TEST_METHOD (RecordNewerThanHistoryLeavesTheSearchToRun)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        CallStackRecorder  recorder;
        ReverseResult      result;
        HRESULT            hr         = S_OK;



        ReverseSessionRig::Prepare (machine);
        Start (machine, controller, recorder);

        machine.RunCycles (KeyframeSettings::kFrameCycles * 2);

        Begin (machine, recorder, false);
        RunRecordedTo (machine, recorder, ReverseSessionRig::kCallSite);

        hr = controller.StepBackOut (result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"searched to the start of history");
        Assert::IsTrue (controller.GetReplayer().GetReplayedCount() > 0, L"the search replayed history");
    }


private:

    //  The guest loop in place and the record begun where the machine
    //  stands, dated by the machine's cycle count.
    static void Begin (TestMachine & machine, CallStackRecorder & recorder, bool isPreparing = true)
    {
        Word  pc = 0;



        if (isPreparing)
        {
            ReverseSessionRig::Prepare (machine);
        }

        recorder.SetPeek  ([&machine] (Word address) { return machine.GetMemoryBus().ReadByte (address); });
        recorder.SetClock ([&machine] { return machine.GetCpu()->GetTotalCycles(); });

        pc = machine.GetCpu()->GetPC();
        recorder.Begin (pc, machine.GetMemoryBus().ReadByte (pc));
    }


    static void Start (TestMachine & machine, ReverseController & controller, CallStackRecorder & recorder)
    {
        HRESULT  hr = S_OK;



        //  A call is dated when the record takes it in, at the next
        //  instruction; one made just before history begins is dated here.
        recorder.Settle (machine.GetCpu()->GetPC(), machine.GetCpu()->GetSP());

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        controller.SetCallerProbe ([&machine, &recorder] (uint64_t historyStartCycle)
        {
            recorder.Settle (machine.GetCpu()->GetPC(), machine.GetCpu()->GetSP());

            return recorder.HasNoCallSince (historyStartCycle);
        });
    }


    static void RunRecorded (TestMachine & machine, CallStackRecorder & recorder, size_t count)
    {
        size_t  i  = 0;
        Word    pc = 0;



        for (i = 0; i < count; i++)
        {
            pc = machine.GetCpu()->GetPC();

            recorder.OnInstruction (pc, machine.GetCpu()->GetSP(), machine.GetMemoryBus().ReadByte (pc));
            machine.StepOne();
        }
    }


    static void RunRecordedTo (TestMachine & machine, CallStackRecorder & recorder, Word pc)
    {
        size_t  i = 0;



        RunRecorded (machine, recorder, 1);

        for (i = 0; i < s_kNoCallerStepLimit && machine.GetCpu()->GetPC() != pc; i++)
        {
            RunRecorded (machine, recorder, 1);
        }

        Assert::AreEqual<Word> (pc, machine.GetCpu()->GetPC(), std::format (L"the run reached ${:04X}", pc).c_str());
    }
};
