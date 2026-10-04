#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StepWhileRunningTests
//
//  Every step, forward and back, works only on a paused machine: the console
//  refuses it with a message while the machine runs, and its key takes no
//  action. Run, Pause and breakpoints stay usable.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (StepWhileRunningTests)
    {
    public:

        using Action = DebuggerKeySchemes::Action;


        class QuietSink : public IDebugNotificationSink
        {
        public:
            void OnStopped        (const StopEvent &) override   {}
            void OnResumed() override                            {}
            void OnReset          (bool) override                {}
            void OnMachineChanged (const std::string &) override {}
            void OnModeChanged    (CommandMode) override         {}
        };


        static DebugCommand MakeCommand (DebugVerb verb, const std::string & name)
        {
            DebugCommand  command;



            command.verb       = verb;
            command.sourceName = name;
            command.count      = 1;
            return command;
        }



        TEST_METHOD (TheConsoleRefusesEveryStepWhileTheMachineRuns)
        {
            struct Case
            {
                DebugVerb     verb;
                const char  * name;
            };

            static constexpr Case  kCases[] =
            {
                { DebugVerb::StepInto,     "T"     },
                { DebugVerb::StepOver,     "P"     },
                { DebugVerb::StepOut,      "RTS"   },
                { DebugVerb::RunFrame,     "FRAME" },
                { DebugVerb::StepBack,     "T-"    },
                { DebugVerb::StepBackOver, "P-"    },
                { DebugVerb::StepBackOut,  "GU-"   },
                { DebugVerb::ReverseGo,    "G-"    },
            };



            for (const Case & each : kCases)
            {
                MockDebugTarget  target;
                QuietSink        sink;
                DebugSession     session (target, sink, RunState::FreeRunning);
                DebugHandlerSet  handlers;
                Reply            reply;
                std::wstring     label   (each.name, each.name + strlen (each.name));



                handlers.Attach (session);

                reply = session.Execute (MakeCommand (each.verb, each.name));

                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, label.c_str());
                Assert::AreEqual (std::string ("machine running"), reply.error.label, label.c_str());
                Assert::IsTrue   (reply.error.detail.find ("Pause the machine first.") != std::string::npos, label.c_str());
                Assert::AreEqual ((size_t) 0, target.runs.size(), label.c_str());
                Assert::AreEqual ((int) RunState::FreeRunning, (int) session.GetRunState(), label.c_str());
            }
        }



        TEST_METHOD (TheConsoleRefusesATypedStepInEveryDialect)
        {
            struct Case
            {
                CommandMode   mode;
                const char  * line;
            };

            static constexpr Case  kCases[] =
            {
                { CommandMode::AppleWin,  "T"     },
                { CommandMode::Casso,     "T"     },
                { CommandMode::WinDbg,    "p"     },
                { CommandMode::GSSquared, "s"     },
                { CommandMode::Monitor,   "S"     },
                { CommandMode::AppleWin,  "FRAME" },
            };



            for (const Case & each : kCases)
            {
                MockDebugTarget  target;
                QuietSink        sink;
                DebugSession     session (target, sink, RunState::FreeRunning);
                Reply            reply   = session.ExecuteLine (each.line, each.mode);
                std::wstring     label   (each.line, each.line + strlen (each.line));



                Assert::AreEqual (std::string ("machine running"), reply.error.label, label.c_str());
                Assert::AreEqual ((size_t) 0, target.runs.size(), label.c_str());
            }
        }



        TEST_METHOD (APausedMachineStepsAndARunningOneStillRunsAndPauses)
        {
            MockDebugTarget  target;
            QuietSink        sink;
            DebugSession     paused  (target, sink, RunState::Paused);
            Reply            stepped = paused.Execute (MakeCommand (DebugVerb::StepInto, "T"));



            Assert::AreEqual ((int) CommandStatus::Ok, (int) stepped.status, L"paused, a step runs");
            Assert::AreEqual ((size_t) 1, target.runs.size());

            {
                MockDebugTarget  other;
                DebugSession     running (other, sink, RunState::FreeRunning);
                Reply            go      = running.Execute (MakeCommand (DebugVerb::Go, "G"));



                Assert::AreEqual ((int) CommandStatus::Ok, (int) go.status, L"Run stays usable while the machine runs");
                Assert::AreNotEqual (std::string ("machine running"), running.ExecuteLine ("BP 300").error.label, L"so do breakpoints");
            }
        }



        TEST_METHOD (AStepKeyTakesNothingWhileTheMachineRuns)
        {
            DebuggerViewSnapshot  running;
            DebuggerViewSnapshot  paused;



            running.isPaused = false;
            paused.isPaused  = true;

            running.code.push_back ({});
            paused.code.push_back ({});

            for (Action action : { Action::StepInto, Action::StepOver, Action::StepOut, Action::RunToCursor, Action::RunFrame,
                                   Action::StepBackInto, Action::StepBackOver, Action::StepBackOut })
            {
                Assert::IsTrue  (DebuggerActions::IsStepAction (action));
                Assert::IsFalse (DebuggerActions::GetForKey (action, &running, 0, CommandMode::AppleWin).has_value(), L"running: no action");
                Assert::IsTrue  (DebuggerActions::GetForKey (action, &paused,  0, CommandMode::AppleWin).has_value(), L"paused: the step");
            }

            Assert::IsTrue (DebuggerActions::GetForKey (Action::Run,              &running, 0, CommandMode::AppleWin).has_value(), L"Run stays live");
            Assert::IsTrue (DebuggerActions::GetForKey (Action::ToggleBreakpoint, &running, 0, CommandMode::AppleWin).has_value(), L"so does a breakpoint");
        }
    };
}
