#include "Pch.h"

#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StepWhileRunningTests
//
//  A step over or step out asked for while a run is in progress pauses the
//  run and starts from where it stops (FR-139).
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (StepWhileRunningTests)
    {
    public:

        class Sink : public IDebugNotificationSink
        {
        public:
            std::vector<StopEvent>  stops;

            void OnStopped        (const StopEvent & stop) override  { stops.push_back (stop); }
            void OnResumed() override                                {}
            void OnReset          (bool) override                    {}
            void OnMachineChanged (const std::string &) override     {}
            void OnModeChanged    (CommandMode) override             {}
        };

        static DebugCommand MakeCommand (DebugVerb verb, const std::string & name)
        {
            DebugCommand  command;



            command.verb       = verb;
            command.sourceName = name;
            return command;
        }

        static StopEvent MakeStop (StopReason reason, Word pc)
        {
            StopEvent  stop;



            stop.reason = reason;
            stop.pc     = pc;
            return stop;
        }



        TEST_METHOD (AStepOutWhileRunningPausesAndStepsOutFromThere)
        {
            MockDebugTarget  target;
            Sink             sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            session.Execute (MakeCommand (DebugVerb::Go, "G"));
            reply = session.Execute (MakeCommand (DebugVerb::StepOut, "PO"));

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, L"no 'already running' error");
            Assert::AreEqual (1,                       target.pauseRequests, L"the run is paused");
            Assert::AreEqual ((size_t) 1,              target.runs.size(),   L"the step waits for the pause");

            target.Stop (MakeStop (StopReason::Pause, 0x0302));

            Assert::AreEqual ((size_t) 2,                  target.runs.size(), L"the step starts from the pause");
            Assert::AreEqual ((int) RunKind::StepOut,      (int) target.runs[1].kind);
            Assert::IsTrue   (sink.stops.empty(),          L"the pause is not reported");
            Assert::AreEqual ((int) RunState::Stepping,    (int) session.GetRunState());
        }



        TEST_METHOD (AStepOverWhileRunningPausesAndStepsOverFromThere)
        {
            MockDebugTarget  target;
            Sink             sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.Execute (MakeCommand (DebugVerb::Go, "G"));
            session.Execute (MakeCommand (DebugVerb::StepOver, "P"));
            target.Stop     (MakeStop (StopReason::Pause, 0x0300));

            Assert::AreEqual ((size_t) 2,                 target.runs.size());
            Assert::AreEqual ((int) RunKind::StepOver,    (int) target.runs[1].kind);
        }



        //  A breakpoint reached before the pause takes effect is where the
        //  machine stopped; it is reported and the step is dropped.
        TEST_METHOD (ABreakpointBeforeThePauseIsReportedAndTheStepDropped)
        {
            MockDebugTarget  target;
            Sink             sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.Execute (MakeCommand (DebugVerb::Go, "G"));
            session.Execute (MakeCommand (DebugVerb::StepOver, "P"));
            target.Stop     (MakeStop (StopReason::Breakpoint, 0x0300));

            Assert::AreEqual ((size_t) 1,              target.runs.size());
            Assert::AreEqual ((size_t) 1,              sink.stops.size());
            Assert::AreEqual ((int) RunState::Paused,  (int) session.GetRunState());
        }
    };
}