#include "Pch.h"

#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/IInstructionObserver.h"
#include "MockDebugTarget.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSessionTests
//
//  The session's state machine, hook management and table lifetime, against
//  the mock target.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugSessionTests)
    {
    public:

        class RecordingSink : public IDebugNotificationSink
        {
        public:
            std::vector<StopEvent>    stops;
            int                       resumed  = 0;
            int                       resets   = 0;
            std::vector<std::string>  machines;
            std::vector<CommandMode>  modes;

            void OnStopped        (const StopEvent & stop) override          { stops.push_back (stop); }
            void OnResumed() override                                        { ++resumed; }
            void OnReset          (bool) override                            { ++resets; }
            void OnMachineChanged (const std::string & name) override        { machines.push_back (name); }
            void OnModeChanged    (CommandMode mode) override                { modes.push_back (mode); }
        };

        static DebugCommand MakeCommand (DebugVerb verb, const std::string & name)
        {
            DebugCommand command;



            command.verb       = verb;
            command.sourceName = name;
            return command;
        }

        static StopEvent MakeStop (StopReason reason, Word pc)
        {
            StopEvent stop;



            stop.reason = reason;
            stop.pc     = pc;
            return stop;
        }



        TEST_METHOD (Go_FromPaused_StartsDebugRun)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            reply = session.Execute (MakeCommand (DebugVerb::Go, "G"));

            Assert::AreEqual ((int) CommandStatus::Ok,     (int) reply.status);
            Assert::AreEqual ((int) RunState::DebugRun,    (int) session.GetRunState());
            Assert::AreEqual ((size_t) 1,                  target.runs.size());
            Assert::IsTrue   (target.hookInstalled);
        }



        TEST_METHOD (GoWithStopAddress_RunsTo)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            DebugCommand     go = MakeCommand (DebugVerb::Go, "G");



            go.a1    = 0xC600;
            go.hasA1 = true;
            session.Execute (go);

            Assert::AreEqual ((int) RunKind::RunTo, (int) target.runs.at (0).kind);
            Assert::IsTrue   (target.runs[0].hasUntilPc);
            Assert::AreEqual ((Word) 0xC600,        target.runs[0].untilPc);
            Assert::AreEqual ((int) RunState::DebugRun, (int) session.GetRunState());
        }



        TEST_METHOD (Run_WhileRunning_IsError)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            session.Execute (MakeCommand (DebugVerb::Go, "G"));
            reply = session.Execute (MakeCommand (DebugVerb::StepInto, "T"));

            Assert::AreEqual ((int) CommandStatus::Error,  (int) reply.status);
            Assert::AreEqual (std::string ("already running"), reply.error.label);
            Assert::AreEqual ((size_t) 1,                  target.runs.size());
            Assert::AreEqual ((int) RunState::DebugRun,    (int) session.GetRunState());
        }



        TEST_METHOD (Go_WhileFreeRunning_AdoptsMachine)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::FreeRunning);
            Reply            reply;



            reply = session.Execute (MakeCommand (DebugVerb::Go, "g"));

            Assert::AreEqual ((int) CommandStatus::Ok,     (int) reply.status);
            Assert::AreEqual ((int) RunState::DebugRun,    (int) session.GetRunState());
            Assert::AreEqual ((size_t) 1,                  target.runs.size());
        }



        TEST_METHOD (Step_SetsStepping_StopReturnsToPaused)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.Execute (MakeCommand (DebugVerb::StepOver, "P"));
            Assert::AreEqual ((int) RunState::Stepping, (int) session.GetRunState());
            Assert::AreEqual ((int) RunKind::StepOver,  (int) target.runs[0].kind);

            target.Stop (MakeStop (StopReason::Step, 0x0303));

            Assert::AreEqual ((int) RunState::Paused, (int) session.GetRunState());
            Assert::AreEqual ((size_t) 1,             sink.stops.size());
            Assert::IsFalse  (target.hookInstalled);
        }



        TEST_METHOD (StopWhileFreeRunning_PausesAndNotifies)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::FreeRunning);



            session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            session.OnStopConditionsChanged();

            Assert::IsTrue   (session.ShouldStopBefore (0x0300));
            target.Stop (MakeStop (StopReason::Breakpoint, 0x0300));

            Assert::AreEqual ((int) RunState::Paused, (int) session.GetRunState());
            Assert::AreEqual ((size_t) 1,             sink.stops.size());
            Assert::AreEqual (0,                      sink.stops[0].breakpointId.value_or (-1));
            Assert::IsTrue   (target.hookInstalled);
        }



        TEST_METHOD (Hook_InstalledForEveryKindOfStopCondition)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::FreeRunning);
            Expression       condition;
            std::string      error;
            size_t           checked = 0;

            std::vector<std::function<int (DebugSession &)>> adders =
            {
                [] (DebugSession & s) { return s.GetBreakpoints().AddAddress (0x0300, 0x0300); },
                [] (DebugSession & s) { return s.GetBreakpoints().AddOpcode (0xA9); },
                [] (DebugSession & s) { return s.GetBreakpoints().AddIo (0xC000, 0xC000); },
                [] (DebugSession & s) { return s.GetBreakpoints().AddBrk(); },
                [] (DebugSession & s) { return s.GetBreakpoints().AddInterrupt(); },
                [] (DebugSession & s) { Expression e; std::string err; DebugExpressionEvaluator::Parse ("A=0", e, err); return s.GetBreakpoints().AddCondition (e); },
                [] (DebugSession & s) { return s.GetWatchpoints().Add (WatchAccess::Read, 0xC019, 0xC019); },
            };



            for (auto & add : adders)
            {
                int id = add (session);



                session.OnStopConditionsChanged();
                Assert::IsTrue (target.hookInstalled);

                if (!session.GetBreakpoints().TryClear (id))
                {
                    session.GetWatchpoints().TryClear (id);
                }

                session.OnStopConditionsChanged();
                Assert::IsFalse (target.hookInstalled);
                ++checked;
            }

            Assert::AreEqual (adders.size(), checked);
        }



        TEST_METHOD (MachineSwitch_ClearsTables_ResetKeepsThem)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            session.GetWatchpoints().Add (WatchAccess::Write, 0x0400, 0x0400);
            session.GetWatches().Add (0x0036);
            session.OnStopConditionsChanged();

            session.OnReset (false);
            Assert::AreEqual ((size_t) 1, session.GetBreakpoints().GetAll().size());
            Assert::AreEqual ((size_t) 1, session.GetWatchpoints().GetAll().size());
            Assert::AreEqual (1,          sink.resets);

            session.OnMachineChanged ("Apple2c");
            Assert::IsTrue   (session.GetBreakpoints().GetAll().empty());
            Assert::IsTrue   (session.GetWatchpoints().GetAll().empty());
            Assert::AreEqual ((size_t) 1, session.GetWatches().GetAll().size());
            Assert::IsFalse  (target.hookInstalled);
            Assert::IsFalse  (target.watchedPages[0x04]);
            Assert::AreEqual (std::string ("Apple2c"), sink.machines.back());
        }



        TEST_METHOD (ModeSwitch_KeepsTables)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            DebugCommand     command = MakeCommand (DebugVerb::SetMode, "MODE MONITOR");



            session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            command.mode = CommandMode::Monitor;
            session.Execute (command);

            Assert::AreEqual ((int) CommandMode::Monitor, (int) session.GetMode());
            Assert::AreEqual ((size_t) 1,                 session.GetBreakpoints().GetAll().size());
            Assert::AreEqual ((size_t) 1,                 sink.modes.size());
        }



        TEST_METHOD (UnknownCommand_ChangesNothing)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            reply = session.Execute (MakeCommand (DebugVerb::None, "FROB"));

            Assert::AreEqual ((int) CommandStatus::Unknown, (int) reply.status);
            Assert::AreEqual ((int) RunState::Paused,       (int) session.GetRunState());
            Assert::IsTrue   (target.runs.empty());
            Assert::AreEqual (0, target.hookChanges);
            Assert::IsTrue   (sink.stops.empty());
        }



        TEST_METHOD (Budget_SessionDefault_PerRunOverride_ZeroUnbounds)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            DebugCommand     budget = MakeCommand (DebugVerb::SetBudget, "BUDGET");
            DebugCommand     go     = MakeCommand (DebugVerb::Go, "G");



            session.Execute (go);
            Assert::IsFalse (target.runs.back().budget.has_value());
            target.Stop (MakeStop (StopReason::Pause, 0));

            budget.count = 5000;
            session.Execute (budget);
            session.Execute (go);
            Assert::AreEqual ((uint64_t) 5000, target.runs.back().budget.value_or (0));
            target.Stop (MakeStop (StopReason::Pause, 0));

            go.budget = 42;
            session.Execute (go);
            Assert::AreEqual ((uint64_t) 42, target.runs.back().budget.value_or (0));
            target.Stop (MakeStop (StopReason::Pause, 0));

            budget.count = 0;
            session.Execute (budget);
            go.budget.reset();
            session.Execute (go);
            Assert::IsFalse (target.runs.back().budget.has_value());
        }



        TEST_METHOD (WatchpointHit_AttachedToStop)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.GetWatchpoints().Add (WatchAccess::Read, 0xC019, 0xC019);
            session.ShouldStopBefore (0x0303);
            session.GetWatchpoints().OnWatchedAccess (0xC019, 0x80, BusAccess::Read, std::nullopt);

            Assert::IsTrue (session.HasPendingStop());
            target.Stop (MakeStop (StopReason::Watchpoint, 0x0306));

            Assert::IsTrue   (sink.stops[0].watch.has_value());
            Assert::AreEqual ((Word) 0x0303, sink.stops[0].watch->accessPc);
            Assert::IsFalse  (session.HasPendingStop());
        }



        TEST_METHOD (BeforeWatchpoint_StopsBeforeTheStore_OnceOnly)
        {
            TestCpu          cpu;
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            int              before = 0;



            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();

            // $0300: STA $0410, and a read-write after-mode watch on the same
            // page as well as the before-mode write watch.
            target.memory[0x0300] = 0x8D;
            target.memory[0x0301] = 0x10;
            target.memory[0x0302] = 0x04;
            target.memory[0x0410] = 0xA0;
            target.registers.pc   = 0x0300;
            target.registers.a    = 0x41;

            before = session.GetWatchpoints().Add (WatchAccess::Write, 0x0400, 0x04FF, WatchMode::Before);
            session.GetWatchpoints().Add (WatchAccess::ReadWrite, 0x0400, 0x04FF);
            session.OnStopConditionsChanged();

            Assert::IsTrue   (target.hookInstalled);
            Assert::IsTrue   (target.watchedPages[0x04], L"the after-mode watch keeps its page in the mask");
            Assert::IsTrue   (session.ShouldStopBefore (0x0300));
            Assert::AreEqual ((Byte) 0xA0, target.memory[0x0410], L"memory is untouched at a before-stop");

            target.Stop (MakeStop (StopReason::Breakpoint, 0x0300));

            Assert::AreEqual ((int) StopReason::Watchpoint, (int) sink.stops.at (0).reason);
            Assert::IsTrue   (sink.stops[0].watch.has_value());
            Assert::AreEqual (before,          sink.stops[0].watch->id);
            Assert::IsTrue   (sink.stops[0].watch->mode == WatchMode::Before);
            Assert::AreEqual ((Word) 0x0300,   sink.stops[0].watch->accessPc);
            Assert::IsFalse  (sink.stops[0].breakpointId.has_value());

            // Resume: the store executes and the bus reports it, but the
            // after-mode watch on the same range does not stop again.
            session.GetWatchpoints().SetAccessPc (0x0300);
            session.GetWatchpoints().OnWatchedAccess (0x0410, 0x41, BusAccess::Write, (Byte) 0xA0);
            Assert::IsFalse  (session.HasPendingStop(), L"one instruction, one stop");

            // The next instruction's access is reported as usual.
            Assert::IsFalse  (session.ShouldStopBefore (0x0303), L"nothing at $0303 touches the range");
            session.GetWatchpoints().OnWatchedAccess (0x0411, 0x42, BusAccess::Write, (Byte) 0x00);
            Assert::IsTrue   (session.HasPendingStop());
        }



        TEST_METHOD (ExpressionContext_ReadsTarget)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Word             value = 0;
            Byte             byte  = 0;



            target.registers.a  = 0x41;
            target.memory[0x0300] = 0xA9;

            Assert::IsTrue   (session.TryGetRegister ("A", value));
            Assert::AreEqual ((Word) 0x41, value);
            Assert::IsTrue   (session.TryPeek (0x0300, byte));
            Assert::AreEqual ((Byte) 0xA9, byte);
            Assert::IsFalse  (session.TryGetRegister ("Q", value));
        }



        TEST_METHOD (ExecuteLine_ParsesInSessionMode_EchoesLine)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            reply = session.ExecuteLine ("  g  ");
            Assert::AreEqual ((int) CommandStatus::Ok,    (int) reply.status);
            Assert::AreEqual (std::string ("  g  "),      reply.command);
            Assert::AreEqual ((int) RunState::DebugRun,   (int) session.GetRunState());
            target.Stop (MakeStop (StopReason::Pause, 0));

            reply = session.ExecuteLine ("frob");
            Assert::AreEqual ((int) CommandStatus::Unknown, (int) reply.status);
            Assert::AreEqual (std::string ("unknown command"), reply.error.label);

            reply = session.ExecuteLine ("bp");
            Assert::AreEqual ((int) CommandStatus::Error,   (int) reply.status);
            Assert::AreEqual (std::string ("invalid arguments"), reply.error.label);

            reply = session.ExecuteLine ("hgr");
            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status);

            reply = session.ExecuteLine ("");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);

            session.ExecuteLine ("mode monitor");
            reply = session.ExecuteLine ("300L");
            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status);
            Assert::AreEqual (std::string ("Monitor mode is not available yet."), reply.error.detail);

            reply = session.ExecuteLine ("/mode applewin");
            Assert::AreEqual ((int) CommandStatus::Ok,        (int) reply.status);
            Assert::AreEqual ((int) CommandMode::AppleWin,    (int) session.GetMode());
        }



        TEST_METHOD (ResolvePath_RelativeFromCurrentDirectory_QuotesDropped)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.SetCurrentDirectory (L"C:\\Work");

            Assert::AreEqual (std::wstring (L"C:\\Work\\out.bin"), session.ResolvePath ("out.bin"));
            Assert::AreEqual (std::wstring (L"C:\\Work\\Test.txt"), session.ResolvePath ("\"Test.txt\""));
            Assert::AreEqual (std::wstring (L"D:\\x\\y.bin"),       session.ResolvePath ("D:\\x\\y.bin"));
            Assert::AreEqual (std::wstring (L"\\\\server\\s\\f"),   session.ResolvePath ("\\\\server\\s\\f"));
            Assert::IsTrue   (session.ResolvePath ("").empty());
        }



        TEST_METHOD (SearchResults_ResolveAsAtN)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Word             address = 0;



            session.SetSearchResults ({ 0x0300, 0x0410 });

            Assert::IsTrue   (session.TryResolveSymbol ("@1", address));
            Assert::AreEqual ((Word) 0x0300, address);
            Assert::IsTrue   (session.TryResolveSymbol ("@2", address));
            Assert::AreEqual ((Word) 0x0410, address);
            Assert::IsFalse  (session.TryResolveSymbol ("@3", address));
            Assert::IsFalse  (session.TryResolveSymbol ("@0", address));
            Assert::IsFalse  (session.TryResolveSymbol ("@",  address));
            Assert::IsFalse  (session.TryResolveSymbol ("NOSUCH", address));
            Assert::IsTrue   (session.TryResolveSymbol ("home", address), L"the machine's ROM symbols are loaded");
            Assert::AreEqual ((Word) 0xFC58, address);
        }



        TEST_METHOD (Assembly_LinesAssembleAtAdvancingAddress_BlankEnds)
        {
            TestCpu          cpu;
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            Reply            reply;



            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();

            session.BeginAssembly (0x0300);
            Assert::IsTrue (session.IsAssembling());

            reply = session.ExecuteLine ("LDA #$41");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::IsTrue   (std::holds_alternative<DisassemblyData> (reply.data));
            Assert::AreEqual ((Byte) 0xA9, target.memory[0x0300]);
            Assert::AreEqual ((Byte) 0x41, target.memory[0x0301]);

            reply = session.ExecuteLine ("FROB");
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::IsTrue   (session.IsAssembling());

            reply = session.ExecuteLine ("RTS");
            Assert::AreEqual ((Byte) 0x60, target.memory[0x0302]);

            reply = session.ExecuteLine ("");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::IsFalse  (session.IsAssembling());

            reply = session.ExecuteLine ("RTS");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, L"RTS is a command again");
            Assert::AreEqual ((int) RunState::Stepping, (int) session.GetRunState());
        }



        TEST_METHOD (TemporaryBreakpoint_ClearedByItsStop_CountingOneNeverStops)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);
            int              temporary = session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            int              counting  = session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            Breakpoint       entry;



            session.GetBreakpoints().TrySetFlags (temporary, true, true);
            session.GetBreakpoints().TrySetFlags (counting, false, false);
            session.OnStopConditionsChanged();

            Assert::IsFalse  (session.ShouldStopBefore (0x0303), L"a counting breakpoint does not stop");
            Assert::IsTrue   (session.GetBreakpoints().TryFind (counting, entry));
            Assert::AreEqual ((uint32_t) 1, entry.hits);

            Assert::IsTrue   (session.ShouldStopBefore (0x0300));
            target.Stop (MakeStop (StopReason::Breakpoint, 0x0300));

            Assert::AreEqual (temporary, sink.stops.at (0).breakpointId.value_or (-1));
            Assert::IsFalse  (session.GetBreakpoints().TryFind (temporary, entry), L"the temporary entry is gone");
            Assert::IsTrue   (session.GetBreakpoints().TryFind (counting, entry));
        }



        TEST_METHOD (ClearAllBreakpoints_RestartsNumbering)
        {
            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::Paused);



            session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            session.GetWatchpoints().Add (WatchAccess::Read, 0xC000, 0xC000);
            session.ClearAllBreakpoints();

            Assert::AreEqual (0, session.GetBreakpoints().AddAddress (0x0300, 0x0300));
            Assert::AreEqual (1, session.GetWatchpoints().Add (WatchAccess::Read, 0xC000, 0xC000));
        }



        TEST_METHOD (InstructionObserver_HearsOnlyDebuggerRuns)
        {
            class Counting : public IInstructionObserver
            {
            public:
                std::vector<Word> pcs;

                void OnInstruction (DebugSession &, Word pc) override { pcs.push_back (pc); }
            };

            MockDebugTarget  target;
            RecordingSink    sink;
            DebugSession     session (target, sink, RunState::FreeRunning);
            Counting         observer;



            session.SetInstructionObserver (&observer);
            session.GetBreakpoints().AddAddress (0x0400, 0x0400);
            session.OnStopConditionsChanged();

            session.OnInstruction (0x0300);
            Assert::IsTrue (observer.pcs.empty(), L"a free-running machine is not observed");

            session.Execute (MakeCommand (DebugVerb::Go, "G"));
            session.OnInstruction (0x0300);
            session.OnInstruction (0x0301);

            Assert::AreEqual ((size_t) 2,    observer.pcs.size());
            Assert::AreEqual ((Word) 0x0301, observer.pcs[1]);
        }
    };
}
