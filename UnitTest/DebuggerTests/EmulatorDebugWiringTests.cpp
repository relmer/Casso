#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Shell/CpuManager.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/FrameCycleBudget.h"
#include "EmuTests/TestMachine.h"
#include "resource.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
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
    //  PassRunner
    //
    //  One pass of the CPU thread's frame loop as ExecuteCpuSlices runs it: to
    //  the next frame boundary, or to a pending pause's landing point, in the
    //  loop's slices, reporting each slice to the driver and the landing point
    //  when it is reached.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PassRunner
    {
    public:
        static uint32_t RunPass (TestMachine & machine, CpuManagerRunDriver & driver)
        {
            uint32_t  nominal   = VideoTiming::kCyclesPerFrame;
            uint64_t  total     = machine.GetCpu()->GetTotalCycles();
            uint32_t  target    = FrameCycleBudget::GetTarget (nominal, total);
            bool      isLanding = driver.IsPausePending();
            bool      hasEnded  = false;
            uint32_t  executed  = 0;
            uint32_t  slice     = 0;
            uint32_t  actual    = 0;



            if (isLanding)
            {
                target = FrameCycleBudget::GetPauseTarget (nominal, total, driver.GetPauseFraction());
            }

            while (executed < target)
            {
                slice     = std::min (target - executed, FrameCycleBudget::kSliceCycles);
                actual    = (uint32_t) machine.RunCycles (slice);
                executed += actual;

                if (driver.OnSliceExecuted (actual))
                {
                    hasEnded = true;
                    break;
                }

                if (actual == 0)
                {
                    break;
                }
            }

            if (isLanding && !hasEnded)
            {
                driver.OnPausePointReached (executed);
            }

            return executed;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FixedTickClock
    //
    //  A host clock a test sets by hand, for a CpuManager whose tick fraction
    //  the test chooses.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FixedTickClock
    {
    public:
        static constexpr double  kTickNs = 1e9 * kAppleCyclesPerFrame / kAppleCpuClock;

        explicit FixedTickClock (CpuManager & cpuManager) :
            m_cpuManager (cpuManager)
        {
            m_cpuManager.SetClock ([this] { return m_now; });
            m_cpuManager.MarkTickStart();
        }

        //  Starts a tick and moves the clock `fraction` of the way through it.
        void SetFraction (double fraction)
        {
            m_now = {};
            m_cpuManager.MarkTickStart();
            m_now += std::chrono::nanoseconds ((int64_t) std::ceil (fraction * kTickNs));
        }

    private:
        CpuManager                & m_cpuManager;
        CpuManager::TimePoint       m_now = {};
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
            FixedTickClock       clock;



            Rig() :
                machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, cpuManager, target.GetRunHook()),
                clock   (cpuManager)
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
        //  APauseDuringARunWaitsForItsLandingPoint
        //
        //  Not the next slice: the pass runs on to the point in the frame that
        //  matches when the pause was asked for, and the run ends there.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (APauseDuringARunWaitsForItsLandingPoint)
        {
            Rig  rig;



            rig.StartOk (Rig::Go());

            Assert::IsFalse (rig.driver.OnSliceExecuted (1023), L"the run continues");

            rig.driver.Pause();

            Assert::IsTrue  (rig.driver.IsPausePending(),        L"the pause waits");
            Assert::IsFalse (rig.driver.OnSliceExecuted (1023),  L"a slice short of the landing point does not end the run");

            rig.driver.OnPausePointReached (1023);

            Assert::IsTrue  (rig.cpuManager.IsPaused(),         L"the landing point stops the machine");
            Assert::IsFalse (rig.driver.IsRunning());
            Assert::IsFalse (rig.driver.IsPausePending());

            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Pause);
            Assert::AreEqual ((uint64_t) 2046, rig.observer.stops[0].cycles, L"the run's cycles across its slices");
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



            rig.cpuManager.SetSpeedMode (SpeedMode::Double, SpeedChooser::User);
            request.fullSpeed = true;

            rig.StartOk (request);

            Assert::IsTrue (rig.cpuManager.GetSpeedMode() == SpeedMode::Maximum,
                            L"the run takes full speed");
            Assert::IsTrue (rig.cpuManager.GetUserSpeedMode() == SpeedMode::Double,
                            L"as an automatic change, which leaves the speed the user chose");
            Assert::IsFalse (rig.cpuManager.IsUserMaximumSpeed(), L"so history keeps recording");

            rig.driver.Pause();
            rig.driver.OnPausePointReached (0);
            Assert::IsFalse (rig.driver.IsRunning(), L"the pause ends the run");

            Assert::IsTrue (rig.cpuManager.GetSpeedMode() == SpeedMode::Double,
                            L"and gives back the speed it found");

            rig.cpuManager.SetSpeedMode (SpeedMode::Maximum, SpeedChooser::User);
            Assert::IsTrue (rig.cpuManager.IsUserMaximumSpeed(), L"Maximum chosen by the user is the user's speed");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  AnOrdinaryRunLeavesTheSpeedAlone
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AnOrdinaryRunLeavesTheSpeedAlone)
        {
            Rig  rig;



            rig.cpuManager.SetSpeedMode (SpeedMode::Double, SpeedChooser::User);

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
            PassRunner::RunPass (rig.machine, rig.driver);

            Assert::IsTrue   (rig.cpuManager.IsPaused(), L"the machine stops");
            Assert::AreEqual ((size_t) 1, rig.observer.stops.size(), L"and the stop is announced");
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Pause);
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  APauseFromFreeRunningLandsAtItsShareOfTheFrame
        //
        //  A pause asked for 40% of the way through the host tick stops the
        //  next pass 40% of the way through the frame, on an instruction
        //  boundary, rather than at the frame's top.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (APauseFromFreeRunningLandsAtItsShareOfTheFrame)
        {
            Rig  rig;



            PassRunner::RunPass (rig.machine, rig.driver);
            AssertLandedAt (rig, 0.0, L"the first pass ends at the top of a frame");

            rig.clock.SetFraction (kFortyPercent);
            rig.driver.Pause();

            Assert::IsFalse (rig.cpuManager.IsPaused(), L"the pause waits for the next pass");

            PassRunner::RunPass (rig.machine, rig.driver);

            Assert::IsTrue   (rig.cpuManager.IsPaused(), L"the pass stops the machine");
            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Pause);
            AssertLandedAt   (rig, kFortyPercent, L"from free running");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  APauseDuringARunLandsAtItsShareOfTheFrame
        //
        //  The same point during a debugger run, which used to stop after the
        //  pass's first slice whenever the pause was asked for.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (APauseDuringARunLandsAtItsShareOfTheFrame)
        {
            Rig  rig;



            PassRunner::RunPass (rig.machine, rig.driver);
            rig.StartOk (Rig::Go());
            PassRunner::RunPass (rig.machine, rig.driver);

            rig.clock.SetFraction (kFortyPercent);
            rig.driver.Pause();
            PassRunner::RunPass (rig.machine, rig.driver);

            Assert::IsFalse  (rig.driver.IsRunning(),    L"the run ended");
            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::AreEqual ((size_t) 1, rig.observer.stops.size());
            Assert::IsTrue   (rig.observer.stops[0].reason == StopReason::Pause);
            AssertLandedAt   (rig, kFortyPercent, L"during a debugger run");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  PausesAtDifferentMomentsLandAtDifferentBeams
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PausesAtDifferentMomentsLandAtDifferentBeams)
        {
            static constexpr double  kFractions[] = { 0.1, 0.55, 0.9, 0.3 };

            Rig                    rig;
            std::vector<uint32_t>  beams;



            PassRunner::RunPass (rig.machine, rig.driver);

            for (double fraction : kFractions)
            {
                rig.clock.SetFraction (fraction);
                rig.driver.Pause();
                PassRunner::RunPass (rig.machine, rig.driver);

                AssertLandedAt (rig, fraction, std::format (L"pause at {}", fraction));
                beams.push_back (rig.machine.GetVideoTiming()->GetCycleInFrame());

                rig.cpuManager.SetPaused (false);
                PassRunner::RunPass (rig.machine, rig.driver);
                AssertLandedAt (rig, 0.0, std::format (L"the resumed pass after {} finishes the frame", fraction));
            }

            std::sort (beams.begin(), beams.end());
            Assert::AreEqual (std::size (kFractions), beams.size());
            Assert::IsTrue   (std::adjacent_find (beams.begin(), beams.end()) == beams.end(), L"every pause landed somewhere different");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  ABreakpointBeforeTheLandingPointStillStopsExactly
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ABreakpointBeforeTheLandingPointStillStopsExactly)
        {
            Rig         rig;
            RunRequest  request = Rig::Go();



            PassRunner::RunPass (rig.machine, rig.driver);

            rig.target.TryPoke (0x0300, 0xEA);
            rig.target.TryPoke (0x0301, 0xEA);
            rig.target.TryPoke (0x0302, 0xEA);
            rig.machine.GetCpu()->SetPC (0x0300);

            request.kind       = RunKind::RunTo;
            request.hasUntilPc = true;
            request.untilPc    = 0x0302;
            rig.StartOk (request);

            rig.clock.SetFraction (0.9);
            rig.driver.Pause();
            PassRunner::RunPass (rig.machine, rig.driver);

            Assert::AreEqual ((size_t) 1, rig.observer.stops.size(), L"one stop");
            Assert::IsTrue   (rig.observer.stops[0].reason != StopReason::Pause, L"the breakpoint's, not the pause's");
            Assert::AreEqual ((Word) 0x0302, rig.observer.stops[0].pc,          L"exactly at its instruction");
            Assert::IsFalse  (rig.driver.IsPausePending(),                      L"and the pause went with it");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  PauseAndResumeRoundsStayOnTheFrameGrid
        //
        //  However often the machine is paused and resumed, the pass after
        //  each resume finishes the frame, and the cycle count over all of
        //  them is a whole number of frames.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PauseAndResumeRoundsStayOnTheFrameGrid)
        {
            static constexpr uint32_t  kRounds = 60;

            Rig       rig;
            uint64_t  start   = 0;
            uint64_t  spent   = 0;
            uint64_t  nominal = (uint64_t) VideoTiming::kCyclesPerFrame * kRounds;



            PassRunner::RunPass (rig.machine, rig.driver);
            start = rig.machine.GetCpu()->GetTotalCycles();

            for (uint32_t round = 0; round < kRounds; round++)
            {
                rig.clock.SetFraction ((double) ((round * 37) % 100) / 100.0);
                rig.driver.Pause();
                PassRunner::RunPass (rig.machine, rig.driver);
                Assert::IsTrue (rig.cpuManager.IsPaused(), std::format (L"round {} paused", round).c_str());

                rig.cpuManager.SetPaused (false);
                PassRunner::RunPass (rig.machine, rig.driver);
                AssertLandedAt (rig, 0.0, std::format (L"round {} resumed to the boundary", round));
            }

            spent = rig.machine.GetCpu()->GetTotalCycles() - start;

            Assert::IsTrue (spent + kLongestInstruction > nominal, std::format (L"{} cycles over {} rounds, short of {}", spent, kRounds, nominal).c_str());
            Assert::IsTrue (spent < nominal + kLongestInstruction, std::format (L"{} cycles over {} rounds, past {}", spent, kRounds, nominal).c_str());
        }



    private:

        static constexpr double    kFortyPercent       = 0.4;
        static constexpr uint32_t  kLongestInstruction = 7;

        //  The beam sits `fraction` of the way into the frame, or at most one
        //  instruction past it.
        static void AssertLandedAt (Rig & rig, double fraction, const std::wstring & when)
        {
            uint32_t  expected  = (uint32_t) (fraction * VideoTiming::kCyclesPerFrame);
            uint32_t  intoFrame = rig.machine.GetVideoTiming()->GetCycleInFrame();



            Assert::IsTrue (intoFrame >= expected && intoFrame < expected + kLongestInstruction,
                            std::format (L"{}: {} cycles into the frame, expected {}", when, intoFrame, expected).c_str());
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



        TEST_METHOD (AModeSurvivesTheTrip)
        {
            DebugCommandPayload  decoded;



            Assert::IsTrue   (DebugCommandPayload::TryDecode (DebugCommandPayload::Encode (5, "G 0302", CommandMode::Casso), decoded));
            Assert::AreEqual ((uint32_t) 5, decoded.clientId);
            Assert::AreEqual (std::string ("G 0302"), decoded.line);
            Assert::IsTrue   (decoded.mode == CommandMode::Casso);

            Assert::IsTrue   (DebugCommandPayload::TryDecode (DebugCommandPayload::Encode (5, "G 0302"), decoded));
            Assert::IsFalse  (decoded.mode.has_value(), L"no mode given");

            Assert::IsFalse  (DebugCommandPayload::TryDecode ("5 bogus\nR", decoded), L"not a mode");
            Assert::IsFalse  (DebugCommandPayload::TryDecode (" casso\nR",  decoded), L"no id before the mode");
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
            MemoryHandlers             memory;
            FixedTickClock             clock;



            //  $0300: INX / JMP $0300, with the PC on the INX. The clock stays
            //  at the top of the tick, so a pause lands where the machine is.
            Rig() :
                machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, cpuManager, target.GetRunHook()),
                session (target, sink, RunState::FreeRunning),
                clock   (cpuManager)
            {
                Cpu6502Registers  r = {};



                target.SetRunDriver (&driver);
                session.AddHandler  (&breakpoints);
                session.AddHandler  (&execution);
                session.AddHandler  (&memory);

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


            //  PAUSE, and the pass that lands it if the machine was running.
            Reply Pause()
            {
                Reply  reply = session.ExecuteLine ("PAUSE");



                if (driver.IsPausePending())
                {
                    PassRunner::RunPass (machine, driver);
                }

                return reply;
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

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Pause().status);
            Assert::IsTrue   (rig.cpuManager.IsPaused(),                      L"the machine stops");
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused, L"the session knows");
            Assert::AreEqual ((size_t) 1, rig.sink.stops.size(),              L"clients hear one stop");
            Assert::IsTrue   (rig.sink.stops[0].reason == StopReason::Pause);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("= 300").status, L"setting the PC is not refused as running");
            Assert::AreEqual ((Word) 0x0300, rig.target.GetRegisters().pc);
        }



        //  A video breakpoint armed while the machine runs freely stops it
        //  within a frame, paused and announced rather than frozen.
        TEST_METHOD (AVideoBreakpointHitWhileFreeRunningStopsAndIsAnnounced)
        {
            Rig  rig;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPV 0,106").status);

            rig.RunFrames (50);

            Assert::IsTrue   (rig.cpuManager.IsPaused(),                      L"the machine is paused, not frozen running");
            Assert::AreEqual ((size_t) 1, rig.sink.stops.size(),              L"clients hear the stop");
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused, L"and the session knows it stopped");
        }



        //  KEY feeds its queue while the machine runs freely, one key each
        //  time the guest clears the strobe, not only during debugger runs.
        TEST_METHOD (KeysQueuedWhileFreeRunningAreAllDelivered)
        {
            Rig    rig;
            Reply  reply;



            // $0300: LDA $C010 / JMP $0300, clearing the strobe each pass.
            (void) rig.target.TryPoke (0x0300, 0xAD);
            (void) rig.target.TryPoke (0x0301, 0x10);
            (void) rig.target.TryPoke (0x0302, 0xC0);
            (void) rig.target.TryPoke (0x0303, 0x4C);
            (void) rig.target.TryPoke (0x0304, 0x00);
            (void) rig.target.TryPoke (0x0305, 0x03);
            rig.session.SetInstructionObserver (&rig.execution);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("KEY 41 42 43").status);
            rig.RunFrames (10);

            //  With the queue drained, a new key goes to the keyboard at once.
            reply = rig.session.ExecuteLine ("KEY 44");
            Assert::IsFalse  (rig.cpuManager.IsPaused());
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 0."), std::get<MessageData> (reply.data).lines.at (0));
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

            (void) rig.Pause();
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



        //  A power cycle between two slices of a profiled run restarts the
        //  cycle count under the instruction still waiting to be billed. The
        //  profile holds no more cycles than the run took.
        TEST_METHOD (APowerCycleDuringAProfiledRunBillsNoWrappedCount)
        {
            Rig                  rig;
            Reply                reply;
            const ProfileData  * data    = nullptr;
            uint64_t             start   = 0;
            uint64_t             elapsed = 0;



            rig.session.SetInstructionObserver (&rig.execution);
            (void) rig.Pause();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("PROFILE ON").status);

            start = rig.target.GetCycleCount();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("G").status);
            rig.RunFrames (1);
            elapsed = rig.target.GetCycleCount() - start;

            rig.machine.PowerCycle();
            rig.RunFrames (1);
            elapsed += rig.target.GetCycleCount();
            (void) rig.Pause();

            reply = rig.session.ExecuteLine ("PROFILE");
            data  = std::get_if<ProfileData> (&reply.data);
            Assert::IsNotNull (data);
            Assert::IsTrue    (data->instructions > 0, L"the run was profiled");
            Assert::IsTrue    (data->cycles <= elapsed, L"no instruction was billed a wrapped count");
        }



        //  A run from an address sets the PC first, so a running machine is
        //  left alone, as it is for JSR.
        TEST_METHOD (RunFromAddressWhileFreeRunningChangesNothing)
        {
            Rig               rig;
            Cpu6502Registers  before = rig.target.GetRegisters();
            Reply             reply  = rig.session.ExecuteLine ("310G");



            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::AreEqual (before.pc, rig.target.GetRegisters().pc);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::FreeRunning);
        }



        //  A step in the emulator runs on the CPU thread after the command
        //  returns. Until it stops, the machine may not be changed under it.
        TEST_METHOD (MachineWritesWaitForAStepToStop)
        {
            Rig  rig;



            rig.RunFrames (5);
            (void) rig.Pause();

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("T 100").status);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Stepping, L"the step is still on foot");
            Assert::AreEqual (std::string ("machine running"), rig.session.ExecuteLine ("= 300").error.label);
            Assert::AreEqual (std::string ("machine running"), rig.session.ExecuteLine ("JSR 310").error.label);

            rig.RunFrames (50);
            Assert::IsTrue   (rig.session.GetRunState() == RunState::Paused, L"the step ended");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("= 300").status);
        }



        //  IN is a bus read with side effects, so it waits for the machine to
        //  stop, as OUT does.
        TEST_METHOD (InWaitsForTheMachineToStop)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("machine running"), rig.session.ExecuteLine ("IN C030").error.label);

            (void) rig.Pause();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("IN C030").status);
        }



        //  IN's read is the debugger's, not the program's, so a watchpoint on
        //  its address does not stop the next step.
        TEST_METHOD (InDoesNotHitAWatchpoint)
        {
            Rig  rig;



            rig.RunFrames (5);
            (void) rig.Pause();
            rig.target.SetRegisters ([&] { Cpu6502Registers r = rig.target.GetRegisters(); r.pc = 0x0300; return r; } ());

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPMR C030").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("IN C030").status);
            Assert::IsFalse  (rig.session.HasPendingStop(), L"nothing pending from IN");

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("T").status);
            rig.RunFrames (50);

            Assert::AreEqual ((Word) 0x0301, rig.target.GetRegisters().pc, L"the INX ran");
            Assert::IsTrue   (rig.sink.stops.back().reason == StopReason::Step);
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



        //  $0300: INX / STX $0400 / JMP $0300, a write watchpoint on $0400 and
        //  a breakpoint on the JMP, with the machine paused.
        static void SetUpWatchThenBreakpoint (Rig & rig)
        {
            (void) rig.target.TryPoke (0x0301, 0x8E);
            (void) rig.target.TryPoke (0x0302, 0x00);
            (void) rig.target.TryPoke (0x0303, 0x04);
            (void) rig.target.TryPoke (0x0304, 0x4C);
            (void) rig.target.TryPoke (0x0305, 0x00);
            (void) rig.target.TryPoke (0x0306, 0x03);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Pause().status);
            rig.sink.stops.clear();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPMW 400").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 304").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("G").status);

            rig.RunFrames (50);

            Assert::AreEqual ((int) StopReason::Watchpoint, (int) rig.sink.stops.at (0).reason, L"the watchpoint stops the run");
            Assert::AreEqual ((Word) 0x0304, rig.target.GetRegisters().pc, L"after the STX, before the JMP");
        }



        //  A watchpoint hit ends a run before the next instruction is asked
        //  about, so the next run asks: the breakpoint there stops it at once.
        TEST_METHOD (ARunAfterAWatchpointHitStopsOnTheNextInstructionsBreakpoint)
        {
            Rig   rig;
            Byte  x = 0;



            SetUpWatchThenBreakpoint (rig);
            x = rig.target.GetRegisters().x;

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("G").status);
            rig.RunFrames (50);

            Assert::AreEqual ((int) StopReason::Breakpoint, (int) rig.sink.stops.at (1).reason, L"the breakpoint is reported");
            Assert::AreEqual ((Word) 0x0304, rig.target.GetRegisters().pc);
            Assert::AreEqual (x, rig.target.GetRegisters().x,                         L"before the loop ran again");
        }



        //  The same, resumed from the main window rather than by a run.
        TEST_METHOD (AResumeAfterAWatchpointHitStopsOnTheNextInstructionsBreakpoint)
        {
            Rig   rig;
            Byte  x = 0;



            SetUpWatchThenBreakpoint (rig);
            x = rig.target.GetRegisters().x;

            rig.cpuManager.SetPaused (false);
            rig.session.OnUserResumed();
            rig.RunFrames (50);

            Assert::AreEqual ((int) StopReason::Breakpoint, (int) rig.sink.stops.at (1).reason, L"the breakpoint is reported");
            Assert::AreEqual ((Word) 0x0304, rig.target.GetRegisters().pc);
            Assert::AreEqual (x, rig.target.GetRegisters().x,                         L"before the loop ran again");
        }



        //  A breakpoint hit while running freely leaves nothing latched: with
        //  every entry cleared and a new one set that is never reached, the
        //  machine runs on.
        TEST_METHOD (ABreakpointSetAfterAFreeRunHitDoesNotFreezeTheMachine)
        {
            Rig  rig;



            (void) rig.session.ExecuteLine ("BP 301");
            rig.RunFrames (50);
            Assert::AreEqual ((size_t) 1, rig.sink.stops.size());

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BPC *").status);
            rig.cpuManager.SetPaused (false);
            rig.session.OnUserResumed();
            rig.RunFrames (5);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.session.ExecuteLine ("BP 310").status);
            rig.RunFrames (5);

            Assert::IsFalse  (rig.cpuManager.IsPaused(), L"still running");
            Assert::AreEqual ((size_t) 1, rig.sink.stops.size(), L"no second stop");
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
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Pause().status);
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



        //  A second run on a Monitor line while the first is still on foot
        //  points at PAUSE as Monitor mode reaches it, through `/`.
        TEST_METHOD (ASecondMonitorRunWhileSteppingPointsAtSlashPause)
        {
            Rig    rig;
            Reply  reply;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Pause().status);

            reply = rig.session.ExecuteLine ("300S S", CommandMode::Monitor);

            Assert::AreEqual (std::string ("already running"), reply.error.label);
            Assert::AreEqual (std::string ("A run is in progress. Use /PAUSE to stop it."), reply.error.detail);
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




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ControllerRunToCursorTests
    //
    //  The window's run to cursor, through its console, in the emulator.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ControllerRunToCursorTests)
    {
    public:

        //  GSSquared's g takes no address, so run to cursor runs Casso's own G
        //  whatever dialect the console is in.
        TEST_METHOD (GSSquaredMode_RunToCursor_ReachesTheAddress)
        {
            ControllerRig               rig;
            IDebugTarget              & target = rig.controller.GetSession().GetTarget();
            std::vector<std::string>    lines;
            HRESULT                     hr     = rig.controller.Open();



            Assert::IsTrue (SUCCEEDED (hr));
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("MODE GSSQUARED").status);

            lines = rig.view.ExecuteConsoleLine (rig.controller.GetSession(),
                                                 DebuggerViewState::GetRunToCursorLine (0x0302),
                                                 DebuggerViewState::kRunToCursorMode);
            ControllerStepOutTests::RunFrames (rig, 50);

            Assert::AreEqual ((Word) 0x0302, target.GetRegisters().pc, L"stopped at the cursor");
            Assert::IsTrue   (rig.controller.GetSession().GetMode() == CommandMode::GSSquared, L"the console stays in GSSquared");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MainWindowStepTests
    //
    //  The main window's Step with the debugger attached. The debugger is put
    //  together as OpenDebugger puts it together, over the shell's own CPU
    //  manager, and the machine runs slice by slice as the CPU thread runs it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MainWindowStepTests)
    {
    public:

        class Rig
        {
        public:
            TestMachine                     machine;
            std::unique_ptr<EmulatorShell>  shell;
            InMemoryPipeTransport           transport;
            InMemoryFileSystem              files;
            DebuggerController              controller;



            //  $0300: LDA #$41 / STA $0400 / RTS, with the PC on the LDA.
            Rig() :
                machine    (std::string ("Apple2e"), TestMachine::Slots::Empty),
                shell      (std::make_unique<EmulatorShell>()),
                controller (machine, ControllerRig::Paused (shell->GetCpuManager()), transport, files, nullptr, 1)
            {
                Cpu6502Registers  r = {};



                (void) GetTarget().TryPoke (0x0300, 0xA9);
                (void) GetTarget().TryPoke (0x0301, 0x41);
                (void) GetTarget().TryPoke (0x0302, 0x8D);
                (void) GetTarget().TryPoke (0x0303, 0x00);
                (void) GetTarget().TryPoke (0x0304, 0x04);
                (void) GetTarget().TryPoke (0x0305, 0x60);

                r    = GetTarget().GetRegisters();
                r.pc = 0x0300;
                r.a  = 0x00;
                GetTarget().SetRegisters (r);

                shell->GetDebugger().SetDebugRunDriver (&controller.GetRunDriver());
                shell->GetDebugger().SetDebugSession   (&controller.GetSession());
            }



            ~Rig()
            {
                shell->GetDebugger().SetDebugRunDriver (nullptr);
                shell->GetDebugger().SetDebugSession   (nullptr);
            }



            IDebugTarget & GetTarget() { return controller.GetSession().GetTarget(); }



            //  The CPU thread's frame: slices while the machine is not paused,
            //  until a slice comes back short.
            void RunFrames (int slices)
            {
                for (int i = 0; i < slices && !shell->GetCpuManager().IsPaused(); i++)
                {
                    uint32_t  actual = (uint32_t) machine.RunCycles (1000);



                    if (controller.GetRunDriver().OnSliceExecuted (actual) || actual == 0)
                    {
                        break;
                    }
                }
            }
        };



        //  The UI thread runs nothing: the step goes to the CPU thread's queue.
        TEST_METHOD (StepIsPostedToTheCpuThread)
        {
            Rig  rig;



            rig.shell->HandleCommand (IDM_MACHINE_STEP);

            Assert::IsTrue   (rig.shell->GetCpuManager().HasPendingCommands(),              L"the step waits for the CPU thread");
            Assert::AreEqual ((Word) 0x0300, rig.GetTarget().GetRegisters().pc,              L"nothing ran on the UI thread");
            Assert::IsTrue   (rig.controller.GetSession().GetRunState() == RunState::Paused, L"and the session saw nothing");
        }



        //  On the CPU thread the step is the debugger's own step into, so the
        //  session starts it, the frame loop runs it, and it stops after one
        //  instruction.
        TEST_METHOD (StepWithTheDebuggerAttachedIsTheSessionsStepInto)
        {
            Rig               rig;
            EmulatorCommand   step;



            step.id = IDM_MACHINE_STEP;
            rig.shell->DispatchCpuCommand (step);

            Assert::IsTrue (rig.controller.GetSession().GetRunState() == RunState::Stepping, L"the session started the step");

            rig.RunFrames (50);

            Assert::AreEqual ((Word) 0x0302, rig.GetTarget().GetRegisters().pc,              L"one instruction ran");
            Assert::AreEqual ((Byte) 0x41,   rig.GetTarget().GetRegisters().a,               L"and it was the LDA");
            Assert::IsTrue   (rig.shell->GetCpuManager().IsPaused(),                        L"the machine is paused again");
            Assert::IsTrue   (rig.controller.GetSession().GetRunState() == RunState::Paused, L"and the session knows the step ended");
        }
    };
}
