#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Shell/CpuManager.h"
#include "EmuTests/TestMachine.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "ControllerRig.h"
#include "HandlerTestRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace EmulatorDebugWiringTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingObserver
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RecordingObserver : public IRunObserver
    {
    public:
        void OnStopped (const StopEvent & stop) override { stops.push_back (stop); }

        std::vector<StopEvent>  stops;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CpuManagerRunDriverTests
    //
    //  The driver that runs a debugger run inside the emulator, against a real
    //  machine, a real CpuManager and a real stop hook.
    //
    //  THE SLICE LOOP IS STOOD IN FOR, and that is a limit worth stating rather
    //  than hiding. `EmulatorShell` cannot run in a test: it needs Initialize,
    //  an HWND and a message pump before a frame is ever executed, so the frame
    //  loop that would call OnSliceExecuted in production cannot be started
    //  here. These tests call it directly, with the cycle counts a slice would
    //  report, which covers every decision the driver makes and none of the
    //  wiring that reaches it. The one line in `ExecuteCpuSlices` that makes the
    //  call is covered by running the emulator, not by this file.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CpuManagerRunDriverTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////
        //
        //  Rig
        //
        ////////////////////////////////////////////////////////////////////////////

        class Rig
        {
        public:
            TestMachine          machine;
            MachineDebugTarget   target;
            CpuManager           cpuManager;
            CpuManagerRunDriver  driver;
            RecordingObserver    observer;



            Rig() :
                machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, cpuManager, target.GetRunHook())
            {
                driver.SetRunObserver (&observer);
            }



            //  Starts a run and checks it started, so no call sits inside a
            //  SUCCEEDED and no test repeats the hoist.
            void StartOk (const RunRequest & request)
            {
                HRESULT  hr = driver.Start (request);



                Assert::IsTrue (SUCCEEDED (hr), L"the run started");
            }



            static RunRequest Go (std::optional<uint64_t> budget = {})
            {
                RunRequest  request;

                request.kind   = RunKind::Go;
                request.budget = budget;

                return request;
            }
        };





        ////////////////////////////////////////////////////////////////////////////
        //
        //  StartDoesNotBlockAndUnpausesTheMachine
        //
        //  The property the whole design turns on: Start returns with the run
        //  still to come. A driver that ran it here would hold the CPU thread,
        //  and the command queue a `pause` arrives on is drained by that thread.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (StartDoesNotBlockAndUnpausesTheMachine)
        {
            Rig  rig;



            rig.cpuManager.SetPaused (true);

            rig.StartOk (Rig::Go());
            Assert::IsFalse (rig.cpuManager.IsPaused(), L"the machine was let go");
            Assert::IsTrue  (rig.driver.IsRunning(),    L"and the run is on foot");
            Assert::AreEqual ((size_t) 0, rig.observer.stops.size(),
                              L"nothing has stopped yet, because nothing has run yet");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  APauseDuringARunIsHonoredAtTheNextSlice
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (APauseDuringARunIsHonoredAtTheNextSlice)
        {
            Rig  rig;



            rig.StartOk (Rig::Go());

            Assert::IsFalse (rig.driver.OnSliceExecuted (1023), L"the run continues");

            rig.driver.Pause();

            Assert::IsTrue  (rig.driver.OnSliceExecuted (1023), L"the pause ends it");
            Assert::IsTrue  (rig.cpuManager.IsPaused(),         L"and stops the machine");
            Assert::IsFalse (rig.driver.IsRunning());

            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Pause);
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  TheBudgetIsCountedAcrossSlices
        //
        //  Not within one. The frame loop picks its own slice length, so a budget
        //  compared against a single slice would be a budget rounded up to
        //  whatever that frame asked for.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TheBudgetIsCountedAcrossSlices)
        {
            Rig  rig;



            rig.StartOk (Rig::Go (2500));

            Assert::IsFalse (rig.driver.OnSliceExecuted (1000), L"1000 of 2500");
            Assert::IsFalse (rig.driver.OnSliceExecuted (1000), L"2000 of 2500");
            Assert::IsTrue  (rig.driver.OnSliceExecuted (1000), L"3000 passes 2500");

            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Budget);
            Assert::AreEqual ((uint64_t) 3000, rig.observer.stops[0].cycles,
                              L"the stop reports what actually ran");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  AnUnboundedRunNeverStopsOnABudget
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AnUnboundedRunNeverStopsOnABudget)
        {
            Rig  rig;



            rig.StartOk (Rig::Go());

            for (int slice = 0; slice < 200; slice++)
            {
                Assert::IsFalse (rig.driver.OnSliceExecuted (100000), L"a run with no budget runs on");
            }

            Assert::IsTrue (rig.observer.stops.empty());
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  FullSpeedIsRestoredWhenTheRunStops
        //
        //  `GG` borrows the speed; it must give it back. Otherwise a single
        //  debugger run silently redefines the speed the operator chose for the
        //  rest of the session.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (FullSpeedIsRestoredWhenTheRunStops)
        {
            Rig         rig;
            RunRequest  request = Rig::Go();



            rig.cpuManager.SetSpeedMode (SpeedMode::Double);
            request.fullSpeed = true;

            rig.StartOk (request);

            Assert::IsTrue (rig.cpuManager.GetSpeedMode() == SpeedMode::Maximum,
                            L"the run takes full speed");

            rig.driver.Pause();
            Assert::IsTrue (rig.driver.OnSliceExecuted (1023), L"the pause ends the run");

            Assert::IsTrue (rig.cpuManager.GetSpeedMode() == SpeedMode::Double,
                            L"and gives back the speed it found");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  AnOrdinaryRunLeavesTheSpeedAlone
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AnOrdinaryRunLeavesTheSpeedAlone)
        {
            Rig  rig;



            rig.cpuManager.SetSpeedMode (SpeedMode::Double);

            rig.StartOk (Rig::Go());

            Assert::IsTrue (rig.cpuManager.GetSpeedMode() == SpeedMode::Double,
                            L"a run that did not ask for full speed does not take it");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  AStopBeforeASlicesFirstInstructionEndsTheRun
        //
        //  A slice can end exactly ahead of the instruction the run stops on. The
        //  next slice then runs nothing, and its zero is the report the run ends
        //  on; the slice loop has to pass it on rather than treat it as a machine
        //  with no CPU. Missing it left the emulator running at a breakpoint on
        //  an interrupt handler, deaf to pause.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AStopBeforeASlicesFirstInstructionEndsTheRun)
        {
            Rig         rig;
            RunRequest  request = Rig::Go();
            uint64_t    ran     = 0;



            rig.target.TryPoke (0x0300, 0xEA);
            rig.target.TryPoke (0x0301, 0xEA);
            rig.target.TryPoke (0x0302, 0xEA);
            rig.machine.GetCpu()->SetPC (0x0300);

            request.kind       = RunKind::RunTo;
            request.hasUntilPc = true;
            request.untilPc    = 0x0302;
            rig.StartOk (request);

            ran = rig.machine.RunCycles (4);
            Assert::AreEqual ((uint64_t) 4, ran, L"two NOPs fill the first slice and stop short of $0302");
            Assert::IsFalse (rig.driver.OnSliceExecuted ((uint32_t) ran));

            ran = rig.machine.RunCycles (1023);
            Assert::AreEqual ((uint64_t) 0, ran, L"the next slice stops before its first instruction");
            Assert::IsTrue  (rig.driver.OnSliceExecuted ((uint32_t) ran), L"and its zero ends the run");
            Assert::IsFalse (rig.driver.IsRunning());
            Assert::IsTrue  (rig.cpuManager.IsPaused());

            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::AreEqual ((Word) 0x0302, rig.observer.stops[0].pc);
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  ASliceWithNoRunInProgressIsIgnored
        //
        //  The cost the slice loop pays on a machine nobody is debugging, and the
        //  reason the loop may call this unconditionally.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ASliceWithNoRunInProgressIsIgnored)
        {
            Rig  rig;



            Assert::IsFalse (rig.driver.OnSliceExecuted (1023));
            Assert::IsTrue  (rig.observer.stops.empty());
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  PauseWithNoRunStopsTheMachineAnyway
        //
        //  An emulator session is free-running whenever no debugger run is
        //  active, so a client asking it to stop means the machine rather than
        //  the bookkeeping.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PauseWithNoRunStopsTheMachineAnyway)
        {
            Rig  rig;



            rig.cpuManager.SetPaused (false);
            rig.driver.Pause();

            Assert::IsTrue (rig.cpuManager.IsPaused(), L"the machine stops");
        }
    };
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UserPauseDuringARunTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UserPauseDuringARunTests)
    {
    public:

        //  The user paused a machine that was in the middle of a debugger run.
        //  No slice will run to deliver the stop, so it is delivered at once, and
        //  the client hears one stop rather than a run that never ends.
        TEST_METHOD (APauseDuringARunEndsItAtOnce)
        {
            TestMachine          machine (std::string ("Apple2e"), TestMachine::Slots::Empty);
            MachineDebugTarget   target  (machine);
            CpuManager           cpuManager;
            CpuManagerRunDriver  driver  (machine, cpuManager, target.GetRunHook());
            RecordingObserver    observer;
            RunRequest           request;
            HRESULT              hr      = S_OK;



            driver.SetRunObserver (&observer);

            hr = driver.Start (request);
            Assert::IsTrue (SUCCEEDED (hr));

            driver.EndForUserPause();

            Assert::IsFalse  (driver.IsRunning());
            Assert::AreEqual ((size_t) 1, observer.stops.size());
            Assert::IsTrue   (observer.stops[0].reason == StopReason::Pause);
        }



        TEST_METHOD (APauseWithNoRunAnnouncesNothingFromTheDriver)
        {
            TestMachine          machine (std::string ("Apple2e"), TestMachine::Slots::Empty);
            MachineDebugTarget   target  (machine);
            CpuManager           cpuManager;
            CpuManagerRunDriver  driver  (machine, cpuManager, target.GetRunHook());
            RecordingObserver    observer;



            driver.SetRunObserver (&observer);
            driver.EndForUserPause();

            Assert::IsTrue (observer.stops.empty(), L"the session reports a plain user pause, not the driver");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SessionUserPauseTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SessionUserPauseTests)
    {
    public:

        TEST_METHOD (PausingAFreeRunningMachineAnnouncesAStop)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::FreeRunning);



            target.registers.pc = 0xC600;
            session.OnUserPaused();

            Assert::AreEqual ((size_t) 1, sink.stops.size());
            Assert::IsTrue   (sink.stops[0].reason == StopReason::Pause);
            Assert::AreEqual ((Word) 0xC600, sink.stops[0].pc, L"where the machine stopped");
            Assert::IsTrue   (session.GetRunState() == RunState::Paused);
        }



        //  The stop a client last heard is still the true one.
        TEST_METHOD (PausingAPausedMachineAnnouncesNothing)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);



            session.OnUserPaused();

            Assert::IsTrue (sink.stops.empty());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugCommandPayloadTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugCommandPayloadTests)
    {
    public:

        TEST_METHOD (ALineSurvivesTheTripExactly)
        {
            std::string          payload = DebugCommandPayload::Encode (42, "  bp c000 , Stop Here  ");
            DebugCommandPayload  decoded;



            Assert::IsTrue   (DebugCommandPayload::TryDecode (payload, decoded));
            Assert::AreEqual ((uint32_t) 42, decoded.clientId);
            Assert::AreEqual (std::string ("  bp c000 , Stop Here  "), decoded.line);
        }



        TEST_METHOD (AnEmptyLineIsStillACommand)
        {
            DebugCommandPayload  decoded;



            Assert::IsTrue   (DebugCommandPayload::TryDecode (DebugCommandPayload::Encode (3, ""), decoded));
            Assert::AreEqual (std::string(), decoded.line);
        }



        TEST_METHOD (AClientIdThatIsNotAWholeNumberIsRefused)
        {
            DebugCommandPayload  decoded;



            Assert::IsFalse (DebugCommandPayload::TryDecode ("R",            decoded), L"no separator");
            Assert::IsFalse (DebugCommandPayload::TryDecode ("\nR",          decoded), L"no id");
            Assert::IsFalse (DebugCommandPayload::TryDecode ("-1\nR",        decoded), L"negative");
            Assert::IsFalse (DebugCommandPayload::TryDecode ("4 2\nR",       decoded), L"not one number");
            Assert::IsFalse (DebugCommandPayload::TryDecode ("4294967296\nR", decoded), L"past 32 bits");
            Assert::IsTrue  (DebugCommandPayload::TryDecode ("4294967295\nR", decoded), L"the largest id fits");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FreeRunStopTests
    //
    //  A breakpoint hit while the emulator runs with no debugger run on foot,
    //  as when the user sets a breakpoint and resumes from the main window.
    //  The machine runs slice by slice exactly as the CPU thread runs it:
    //  RunCycles, then OnSliceExecuted, then stop on a short slice.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (FreeRunStopTests)
    {
    public:

        class Rig
        {
        public:
            TestMachine                machine;
            MachineDebugTarget         target;
            CpuManager                 cpuManager;
            CpuManagerRunDriver        driver;
            RecordingNotificationSink  sink;
            DebugSession               session;
            BreakpointHandlers         breakpoints;
            ExecutionHandlers          execution;



            //  $0300: INX / JMP $0300, with the PC on the INX.
            Rig() :
                machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, cpuManager, target.GetRunHook()),
                session (target, sink, RunState::FreeRunning)
            {
                Cpu6502Registers  r = {};



                target.SetRunDriver (&driver);
                session.AddHandler  (&breakpoints);
                session.AddHandler  (&execution);

                (void) target.TryPoke (0x0300, 0xE8);
                (void) target.TryPoke (0x0301, 0x4C);
                (void) target.TryPoke (0x0302, 0x00);
                (void) target.TryPoke (0x0303, 0x03);

                r    = target.GetRegisters();
                r.pc = 0x0300;
                r.x  = 0x00;
                r.sp = 0xFF;
                r.p  = 0x34;
                target.SetRegisters (r);
                cpuManager.SetPaused (false);
            }



            //  The CPU thread's frame: slices while the machine is not paused,
            //  until a slice comes back short.
            void RunFrames (int slices)
            {
                for (int i = 0; i < slices && !cpuManager.IsPaused(); i++)
                {
                    uint32_t  actual = (uint32_t) machine.RunCycles (1000);



                    if (driver.OnSliceExecuted (actual) || actual == 0)
                    {
                        break;
                    }
                }
            }
        };



        TEST_METHOD (ABreakpointHitWhileFreeRunningStopsAndIsAnnounced)
        {
            Rig  rig;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 301").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused(),                               L"the machine is paused, not frozen running");
            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc,             L"at the breakpoint");
            Assert::AreEqual ((size_t) 1,    rig.sink.stops.size(),                    L"clients hear the stop");
            Assert::IsTrue   (rig.sink.stops[0].reason == StopReason::Breakpoint);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused,          L"and the session knows it stopped");
        }



        //  Resuming with the PC on the breakpoint runs past it: the loop comes
        //  round and stops there again with one more INX done.
        TEST_METHOD (ResumingFromABreakpointRunsPastIt)
        {
            Rig   rig;
            Byte  x = 0;



            (void) rig.session.ExecuteLine ("BP 301");
            rig.RunFrames (50);
            x = rig.target.GetRegisters().x;

            rig.cpuManager.SetPaused (false);
            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::AreEqual ((Word) 0x0301,   rig.target.GetRegisters().pc);
            Assert::AreEqual ((Byte) (x + 1),  rig.target.GetRegisters().x, L"the loop ran once more");
            Assert::AreEqual ((size_t) 2,      rig.sink.stops.size());
        }



        //  PAUSE typed while the machine runs freely stops the machine and the
        //  session both, so commands that need a stopped machine then work.
        TEST_METHOD (PauseTypedWhileFreeRunningPausesTheSession)
        {
            Rig  rig;



            rig.RunFrames (5);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("PAUSE").status);
            Assert::IsTrue   (rig.cpuManager.IsPaused(),                      L"the machine stops");
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused, L"the session knows");
            Assert::AreEqual ((size_t) 1, rig.sink.stops.size(),              L"clients hear one stop");
            Assert::IsTrue   (rig.sink.stops[0].reason == StopReason::Pause);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("= 300").status, L"setting the PC is not refused as running");
            Assert::AreEqual ((Word) 0x0300, rig.target.GetRegisters().pc);
        }



        //  An opcode breakpoint armed beside an address breakpoint does not
        //  hide the address one: both stop a free run.
        TEST_METHOD (AnAddressBreakpointStopsWhileBrkIsArmed)
        {
            Rig  rig;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BRK ON").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 301").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused(), L"the address breakpoint stopped the machine");
            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc);
        }



        //  BRK ON alone still stops on a BRK, and runs everything else freely.
        TEST_METHOD (BrkOnAloneStopsOnABrk)
        {
            Rig  rig;



            (void) rig.target.TryPoke (0x0302, 0x00);
            (void) rig.target.TryPoke (0x0301, 0xEA);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BRK ON").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::AreEqual ((Word) 0x0302, rig.target.GetRegisters().pc, L"before the BRK");
            Assert::IsTrue   (rig.sink.stops.at (0).reason == StopReason::Brk, L"reported as a BRK stop");
            Assert::IsTrue   (rig.sink.stops.at (0).breakpointId.has_value(),  L"with its breakpoint's id");
        }



        //  BRKOP stops before its opcode and says so.
        TEST_METHOD (BrkopStopsBeforeItsOpcode)
        {
            Rig  rig;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BRKOP 4C").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc, L"before the JMP");
            Assert::IsTrue   (rig.sink.stops.at (0).reason == StopReason::InvalidOpcode);
        }



        //  A resume is announced once, when the machine actually starts again.
        TEST_METHOD (AResumeIsAnnouncedOnlyFromAStop)
        {
            Rig  rig;



            rig.session.OnUserResumed();
            Assert::AreEqual (0, rig.sink.resumed, L"already running: nothing to announce");

            (void) rig.session.ExecuteLine ("PAUSE");
            rig.session.OnUserResumed();
            rig.session.OnUserResumed();
            Assert::AreEqual (1, rig.sink.resumed);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::FreeRunning);
        }



        //  JSR changes the stack and the PC, so a running machine is left alone.
        TEST_METHOD (JsrWhileFreeRunningChangesNothing)
        {
            Rig               rig;
            Cpu6502Registers  before = rig.target.GetRegisters();
            Reply             reply  = rig.session.ExecuteLine ("JSR 310");



            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::AreEqual (before.pc, rig.target.GetRegisters().pc);
            Assert::AreEqual (before.sp, rig.target.GetRegisters().sp, L"nothing pushed");
        }



        //  A step in the emulator runs on the CPU thread after the command
        //  returns. Until it stops, the machine may not be changed under it.
        TEST_METHOD (MachineWritesWaitForAStepToStop)
        {
            Rig  rig;



            rig.RunFrames (5);
            (void) rig.session.ExecuteLine ("PAUSE");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("T 100").status);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Stepping, L"the step is still on foot");
            Assert::AreEqual (std::string ("machine running"), rig.session.ExecuteLine ("= 300").error.label);
            Assert::AreEqual (std::string ("machine running"), rig.session.ExecuteLine ("JSR 310").error.label);

            rig.RunFrames (50);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused, L"the step ended");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("= 300").status);
        }



        //  An after-mode watchpoint hit while running freely is delivered and
        //  then forgotten: the next step runs its full count.
        TEST_METHOD (AFreeRunWatchpointHitLeavesNothingPending)
        {
            Rig  rig;



            // $0300: INX / STX $0400 / JMP $0300
            (void) rig.target.TryPoke (0x0301, 0x8E);
            (void) rig.target.TryPoke (0x0302, 0x00);
            (void) rig.target.TryPoke (0x0303, 0x04);
            (void) rig.target.TryPoke (0x0304, 0x4C);
            (void) rig.target.TryPoke (0x0305, 0x00);
            (void) rig.target.TryPoke (0x0306, 0x03);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPMW 400").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::IsTrue   (rig.sink.stops.at (0).reason == StopReason::Watchpoint);
            Assert::IsFalse  (rig.session.HasPendingStop(), L"the hit was delivered");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("T 2").status);
            rig.RunFrames (50);

            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc, L"the JMP and the INX both ran");
            Assert::IsTrue   (rig.sink.stops.back().reason == StopReason::Step);
        }



        //  A before-mode watchpoint hit while running freely, then a pause and
        //  a breakpoint: the breakpoint's stop is a breakpoint's, not the old
        //  watchpoint hit.
        TEST_METHOD (ABreakpointAfterAFreeRunBeforeWatchpointHitIsABreakpoint)
        {
            Rig  rig;



            // $0300: INX / STX $0400 / JMP $0300
            (void) rig.target.TryPoke (0x0301, 0x8E);
            (void) rig.target.TryPoke (0x0302, 0x00);
            (void) rig.target.TryPoke (0x0303, 0x04);
            (void) rig.target.TryPoke (0x0304, 0x4C);
            (void) rig.target.TryPoke (0x0305, 0x00);
            (void) rig.target.TryPoke (0x0306, 0x03);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPMW 400 BEFORE").status);

            rig.RunFrames (50);
            Assert::IsTrue   (rig.sink.stops.at (0).reason == StopReason::Watchpoint);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPC *").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 304").status);
            rig.cpuManager.SetPaused (false);
            rig.session.OnUserResumed();
            rig.session.OnUserPaused();
            rig.cpuManager.SetPaused (false);
            rig.session.OnUserResumed();
            rig.RunFrames (50);

            Assert::AreEqual ((Word) 0x0304, rig.target.GetRegisters().pc);
            Assert::IsTrue   (rig.sink.stops.back().reason == StopReason::Breakpoint, L"a breakpoint stop");
            Assert::IsFalse  (rig.sink.stops.back().watch.has_value(),                 L"with no watchpoint on it");
        }



        //  A Monitor G stopped at a breakpoint, the breakpoint cleared, and the
        //  machine resumed from the main window: nothing else is armed, and the
        //  final RTS still stops at the Monitor's return.
        TEST_METHOD (AFreeRunResumedAfterAMonitorGoStopsAtTheMonitorsReturn)
        {
            Rig  rig;



            // $0300: INX / INX / RTS
            (void) rig.target.TryPoke (0x0301, 0xE8);
            (void) rig.target.TryPoke (0x0302, 0x60);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("PAUSE").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 301").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("300G", CommandMode::Monitor).status);

            rig.RunFrames (50);
            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc, L"at the breakpoint");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPC *").status);
            rig.cpuManager.SetPaused (false);
            rig.session.OnUserResumed();
            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused(),                             L"stopped, not running on in the Monitor");
            Assert::AreEqual ((Word) 0xFF69, rig.target.GetRegisters().pc,           L"at the Monitor's return");
            Assert::IsTrue   (rig.sink.stops.back().reason == StopReason::RunTo,     L"as a run to");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ControllerStepOutTests
    //
    //  A step out through the debugger in the emulator, run slice by slice as
    //  the CPU thread runs it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ControllerStepOutTests)
    {
    public:

        static void RunFrames (ControllerRig & rig, int slices)
        {
            for (int i = 0; i < slices && !rig.cpuManager.IsPaused(); i++)
            {
                uint32_t  actual = (uint32_t) rig.machine.RunCycles (1000);



                if (rig.controller.GetRunDriver().OnSliceExecuted (actual) || actual == 0)
                {
                    break;
                }
            }
        }



        //  Closing the channel mid step out ends the call record, but the step
        //  out still runs until the routine returns.
        TEST_METHOD (ClosingTheChannelDuringAStepOutLetsItFinish)
        {
            ControllerRig    rig;
            IDebugTarget   & target    = rig.controller.GetSession().GetTarget();
            const Byte       code[]    = { 0x20, 0x10, 0x03, 0xEA };
            const Byte       routine[] = { 0xA2, 0x00, 0xE8, 0xD0, 0xFD, 0x60 };
            HRESULT          hr        = S_OK;



            // $0300: JSR $0310 / NOP    $0310: LDX #0 / INX / BNE $0312 / RTS
            for (Word i = 0; i < sizeof (code); i++)
            {
                (void) target.TryPoke ((Word) (0x0300 + i), code[i]);
            }

            for (Word i = 0; i < sizeof (routine); i++)
            {
                (void) target.TryPoke ((Word) (0x0310 + i), routine[i]);
            }

            hr = rig.controller.Open();
            Assert::IsTrue (SUCCEEDED (hr));

            rig.Run ("T");
            RunFrames (rig, 50);
            Assert::AreEqual ((Word) 0x0310, target.GetRegisters().pc, L"inside the call");

            rig.Run ("RTS");
            RunFrames (rig, 1);
            Assert::IsFalse (rig.cpuManager.IsPaused(), L"the loop is still running");

            rig.controller.Close();
            RunFrames (rig, 50);

            Assert::AreEqual ((Word) 0x0303, target.GetRegisters().pc, L"after the routine returned");
        }
    };
}
