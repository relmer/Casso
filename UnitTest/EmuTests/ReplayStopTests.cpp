#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kStopWarmupCycles = KeyframeSettings::kFrameCycles * 8;
static constexpr int       s_kStopBudgetMb     = 16;
static constexpr int       s_kStopAskAfter     = 5000;





////////////////////////////////////////////////////////////////////////////////
//
//  StopAfterAskingTest
//
//  A stop test that never fires and, once it has been asked about enough
//  instructions, asks the replay to stop, as a press of Pause or Escape
//  does from the UI thread. It keeps the progress it saw on each ask.
//
////////////////////////////////////////////////////////////////////////////////

class StopAfterAskingTest : public IReverseStopTest
{
public:
    StopAfterAskingTest (ReplayControl & control, int askAfter) :
        m_control  (control),
        m_askAfter (askAfter)
    {
    }

    bool ShouldStopBefore (MachineHost &, Word) override
    {
        m_asked++;

        if (m_askAfter > 0 && m_asked == m_askAfter)
        {
            m_control.isStopRequested.store (true);
        }

        progress.push_back (m_control.progress.load());
        return false;
    }

    bool TakePendingStop() override { return false; }

    std::vector<float>  progress;

private:
    ReplayControl  & m_control;
    int              m_askAfter = 0;
    int              m_asked    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStopTests
//
//  A long search through history can be stopped from another thread, and
//  reports how far it has got: a reverse run stopped part way lands on the
//  oldest position it had searched, and a step back out asked to stop goes
//  no further.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReplayStopTests)
{
public:

    TEST_METHOD (AStoppedReverseRunLandsWhereItsSearchHadReached)
    {
        TestMachine          machine ("Apple2e");
        ReverseHost          host    (machine);
        ReplayControl        control;
        StopAfterAskingTest  test    (control, s_kStopAskAfter);
        ReverseResult        result;
        HRESULT              hr      = S_OK;
        uint64_t             from    = 0;
        uint64_t             oldest  = 0;



        PrepareRecording (machine, host, control);

        from   = machine.GetPosition();
        oldest = host.GetController().GetOldestPosition();

        hr = host.Execute (ReverseCommand::ReverseContinue, 0, &test, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::Stopped, L"the run reports it was stopped");
        Assert::IsTrue (machine.GetPosition() < from,   L"it moved back from where it began");
        Assert::IsTrue (machine.GetPosition() > oldest, L"and stopped short of the start of history");
        Assert::IsTrue (host.IsBehindLive(), L"behind live");
    }


    TEST_METHOD (AReverseRunReportsItsProgressThroughHistory)
    {
        TestMachine          machine ("Apple2e");
        ReverseHost          host    (machine);
        ReplayControl        control;
        StopAfterAskingTest  test    (control, 0);
        ReverseResult        result;
        HRESULT              hr      = S_OK;
        float                last    = 0.0f;



        PrepareRecording (machine, host, control);

        hr = host.Execute (ReverseCommand::ReverseContinue, 0, &test, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"with no stop, the run reaches the start");
        Assert::IsFalse (test.progress.empty());

        for (float each : test.progress)
        {
            Assert::IsTrue (each >= 0.0f && each <= 1.0f, L"progress is a fraction");
            Assert::IsTrue (each >= last, L"and never goes down");
            last = each;
        }

        Assert::IsTrue (last > 0.5f, L"the oldest stretch is searched with most of history covered");
    }


    TEST_METHOD (AStepBackOutAskedToStopGoesNoFurther)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReplayControl  control;
        ReverseResult  result;
        HRESULT        hr      = S_OK;
        uint64_t       from    = 0;



        PrepareRecording (machine, host, control);

        hr = host.Execute (ReverseCommand::StepBack, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBack");

        from = machine.GetPosition();
        control.isStopRequested.store (true);

        hr = host.Execute (ReverseCommand::StepBackOut, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBackOut");

        Assert::IsTrue (result.outcome == ReverseOutcome::Stopped, L"the step reports it was stopped");
        Assert::AreEqual<uint64_t> (from, machine.GetPosition(), L"and stayed where it was");
    }


    TEST_METHOD (AStepBackOutSearchReportsItsProgress)
    {
        TestMachine    machine ("Apple2e");
        ReverseHost    host    (machine);
        ReplayControl  control;
        ReverseResult  result;
        HRESULT        hr      = S_OK;



        PrepareRecording (machine, host, control);

        //  The rig's main loop is not inside any routine, so from there the
        //  search for a caller covers the whole of history. A first step out
        //  of the rig's routine, if the machine is in it, reaches the loop.
        hr = host.Execute (ReverseCommand::StepBackOut, 0, nullptr, result);
        AssertSucceeded (hr, L"StepBackOut");

        if (result.outcome != ReverseOutcome::AtHistoryStart)
        {
            hr = host.Execute (ReverseCommand::StepBackOut, 0, nullptr, result);
            AssertSucceeded (hr, L"StepBackOut from the loop");
        }

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"no caller before the loop");
        Assert::IsTrue (control.progress.load() > 0.9f, L"the search covered history");
    }


private:

    static void PrepareRecording (TestMachine & machine, ReverseHost & host, ReplayControl & control)
    {
        HRESULT  hr = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = host.StartRecording (ReverseHost::MakeSettings (s_kStopBudgetMb, 1));
        AssertSucceeded (hr, L"StartRecording");

        host.GetController().SetReplayControl (&control);

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kStopWarmupCycles);
    }
};
