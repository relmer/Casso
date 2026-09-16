#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Shell/CpuManager.h"
#include "EmuTests/TestMachine.h"





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
}
