#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Reverse/ReverseStopTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kSessionStopWarmupCycles = 60000;
static constexpr size_t    s_kSessionStopSteps        = 3000;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSessionStopTestTests
//
//  Reverse continue with a debug session's own breakpoints and watchpoints as
//  its stop test. Each landing is compared with the rig's probe, which tests
//  the same condition directly, and the session's tables must come out of
//  the replay exactly as they went in.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseSessionStopTestTests)
{
public:

    TEST_METHOD (ABreakpointStopsReverseContinueWhereTheProbeDoes)
    {
        TestMachine                machine    ("Apple2e");
        ReverseController          controller (machine);
        MachineDebugTarget         target     (machine);
        RecordingNotificationSink  sink;
        DebugSession               session    (target, sink, RunState::Paused);
        BreakpointHandlers         handlers;
        ReverseStopTest            stopTest   (session);
        ReverseProbe               probe;
        ReverseResult              expected;
        ReverseResult              result;
        uint64_t                   live       = 0;
        int                        stopping   = 0;
        int                        counting   = 0;
        HRESULT                    hr         = S_OK;



        session.AddHandler (&handlers);
        live = Record (machine, controller);

        stopping = AddBreakpoint (session, "BP 900");
        counting = AddBreakpoint (session, "BP 806");
        session.GetBreakpoints().TrySetFlags (counting, false, false);

        probe.breakPc = ReverseSessionRig::kWriteRoutine;

        hr = controller.ReverseContinue (probe, expected);
        AssertSucceeded (hr, L"ReverseContinue with the probe");

        hr = controller.SeekToPosition (live, result);
        AssertSucceeded (hr, L"SeekToPosition live");

        hr = controller.ReverseContinue (stopTest, result);
        AssertSucceeded (hr, L"ReverseContinue with the session");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the breakpoint stopped it");
        Assert::AreEqual<uint64_t> (expected.position, result.position, L"where the probe stopped");
        Assert::AreEqual<Word>     (ReverseSessionRig::kWriteRoutine, machine.GetCpu()->GetPC());
        Assert::AreEqual<uint32_t> (0, GetHits (session, stopping), L"the stopping breakpoint counted nothing");
        Assert::AreEqual<uint32_t> (0, GetHits (session, counting), L"the counting breakpoint counted nothing");
        Assert::IsTrue (sink.stops.empty(), L"nothing was reported to the debugger");
    }


    TEST_METHOD (AWriteWatchpointStopsReverseContinueWhereTheProbeDoes)
    {
        TestMachine                machine    ("Apple2e");
        ReverseController          controller (machine);
        MachineDebugTarget         target     (machine);
        RecordingNotificationSink  sink;
        DebugSession               session    (target, sink, RunState::Paused);
        BreakpointHandlers         handlers;
        ReverseStopTest            stopTest   (session);
        ReverseProbe               probe;
        ReverseResult              expected;
        ReverseResult              result;
        uint64_t                   live       = 0;
        HRESULT                    hr         = S_OK;



        session.AddHandler (&handlers);
        live = Record (machine, controller);

        probe.Watch (machine, ReverseSessionRig::kKeyStore);

        hr = controller.ReverseContinue (probe, expected);
        AssertSucceeded (hr, L"ReverseContinue with the probe");
        Assert::IsTrue (expected.outcome == ReverseOutcome::Moved, L"the recording wrote the watched byte");

        machine.GetMemoryBus().SetWatchSink (&session.GetWatchpoints());
        session.ExecuteLine ("BPMW 300");
        Assert::AreEqual<size_t> (1, session.GetWatchpoints().GetAll().size(), L"the watchpoint was set");

        hr = controller.SeekToPosition (live, result);
        AssertSucceeded (hr, L"SeekToPosition live");

        hr = controller.ReverseContinue (stopTest, result);
        AssertSucceeded (hr, L"ReverseContinue with the session");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the watchpoint stopped it");
        Assert::AreEqual<uint64_t> (expected.position, result.position, L"just after the latest write, as the probe found");
        Assert::AreEqual<uint32_t> (0, session.GetWatchpoints().GetAll().front().hits, L"the watchpoint counted nothing");
        Assert::IsFalse (session.GetWatchpoints().HasPendingStop(), L"and holds no stop");
        Assert::IsTrue (sink.stops.empty(), L"nothing was reported to the debugger");
    }


    //  A read watchpoint on a JSR's operand sees only the CPU fetching it,
    //  which is not the program reading memory, so it never fires. The
    //  opcode byte is left out, since the instruction before it reads it as
    //  a dummy cycle.
    TEST_METHOD (AReadWatchpointPassesOverInstructionFetches)
    {
        TestMachine                machine    ("Apple2e");
        ReverseController          controller (machine);
        MachineDebugTarget         target     (machine);
        RecordingNotificationSink  sink;
        DebugSession               session    (target, sink, RunState::Paused);
        BreakpointHandlers         handlers;
        ReverseStopTest            stopTest   (session);
        ReverseResult              result;
        HRESULT                    hr         = S_OK;



        session.AddHandler (&handlers);
        Record (machine, controller);

        session.ExecuteLine ("BPMR 835");
        Assert::AreEqual<size_t> (1, session.GetWatchpoints().GetAll().size(), L"the watchpoint was set");

        hr = controller.ReverseContinue (stopTest, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"no fetch of the JSR's operand counts");
    }


    //  A disabled breakpoint is not a stop condition backward any more than
    //  forward.
    TEST_METHOD (ADisabledBreakpointDoesNotStopReverseContinue)
    {
        TestMachine                machine    ("Apple2e");
        ReverseController          controller (machine);
        MachineDebugTarget         target     (machine);
        RecordingNotificationSink  sink;
        DebugSession               session    (target, sink, RunState::Paused);
        BreakpointHandlers         handlers;
        ReverseStopTest            stopTest   (session);
        ReverseResult              result;
        int                        id         = 0;
        HRESULT                    hr         = S_OK;



        session.AddHandler (&handlers);
        Record (machine, controller);

        id = AddBreakpoint (session, "BP 900");
        session.GetBreakpoints().TrySetEnabled (id, false);

        hr = controller.ReverseContinue (stopTest, result);
        AssertSucceeded (hr, L"ReverseContinue");

        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"ran back to the start of history");
    }


private:

    //  Records the rig's guest loop one instruction at a time, pressing a key
    //  now and then for the loop to store; returns the live position.
    static uint64_t Record (TestMachine & machine, ReverseController & controller)
    {
        static constexpr size_t  kKeyEvery = 1000;
        HRESULT                  hr        = S_OK;
        size_t                   i         = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kSessionStopWarmupCycles);

        for (i = 0; i < s_kSessionStopSteps; i++)
        {
            if (i % kKeyEvery == kKeyEvery / 2)
            {
                machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('a' + i / kKeyEvery));
            }

            machine.StepOne();
        }

        return machine.GetPosition();
    }


    static int AddBreakpoint (DebugSession & session, const std::string & line)
    {
        size_t  before = session.GetBreakpoints().GetAll().size();



        session.ExecuteLine (line);

        Assert::AreEqual (before + 1, session.GetBreakpoints().GetAll().size(), L"the breakpoint was set");

        return session.GetBreakpoints().GetAll().back().id;
    }


    static uint32_t GetHits (DebugSession & session, int id)
    {
        Breakpoint  entry;



        Assert::IsTrue (session.GetBreakpoints().TryFind (id, entry), L"no such breakpoint");

        return entry.hits;
    }
};
