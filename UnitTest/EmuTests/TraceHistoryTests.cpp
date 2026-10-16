#include "Pch.h"

#include "HResultAssert.h"
#include "TestMachine.h"
#include "Debugger/Reply.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Debugger/TraceController.h"
#include "Shell/DebugEventLogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace TraceHistoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceRig
    //
    //  An enhanced //e recording history from its start, with the live trace
    //  ring on, run long enough for history to have many stretches; the
    //  machine counts as stopped, so the trace's history rebuilds at once.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TraceRig
    {
    public:
        static constexpr uint64_t  kRunCycles = 3'000'000;

        TestMachine        machine    { "Apple2eEnhanced" };
        ReverseController  controller { machine };
        DebugEventLogs     logs       { machine };
        TraceController    trace      { machine };



        TraceRig()
        {
            HRESULT  hr = S_OK;



            hr = controller.Start (ReverseSettings());
            AssertSucceeded (hr, L"Start");

            logs.Start (&controller);
            logs.GetTraceHistory().SetIsStopped ([] { return true; });

            trace.On();
            machine.RunCycles (kRunCycles);

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");
        }



        ~TraceRig()
        {
            trace.SetHistory (nullptr);
            trace.Off();
            logs.Stop();
            controller.Stop();
        }



        Cpu6502 * GetCpu()
        {
            return machine.GetCpu()->GetCpu6502();
        }



        static bool  IsSame (const Cpu::TraceEntry & a, const Cpu::TraceEntry & b)
        {
            return a.cycles == b.cycles && a.pc == b.pc && a.opcode == b.opcode && a.op1 == b.op1 && a.op2 == b.op2 &&
                   a.a == b.a && a.x == b.x && a.y == b.y && a.sp == b.sp && a.p == b.p && a.intr == b.intr &&
                   a.hasAccess == b.hasAccess && (!a.hasAccess || (a.accessIsWrite == b.accessIsWrite && a.accessData == b.accessData && a.accessAddress == b.accessAddress));
        }
    };





    TEST_CLASS (TraceHistoryTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RebuiltInstructionsMatchTheLiveRing
        //
        //  Every instruction the ring has, rebuilt from history at the same
        //  position, has the same registers, bytes, cycle and bus access.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RebuiltInstructionsMatchTheLiveRing)
        {
            TraceRig                         rig;
            std::vector<TraceHistoryEntry>   rebuilt;
            Cpu::TraceEntry                  live      = {};
            uint64_t                         position  = rig.machine.GetPosition();
            size_t                           size      = rig.GetCpu()->GetTraceSize();
            size_t                           i         = 0;



            Assert::AreEqual ((uint64_t) TraceController::kCapacity, (uint64_t) size, L"the run filled the ring");
            Assert::IsTrue   (rig.logs.GetTraceHistory().TryGetEntries (position - size, position, rebuilt), L"history rebuilds the ring's span");
            Assert::AreEqual (size, rebuilt.size());

            for (i = 0; i < size; i++)
            {
                Assert::IsTrue (rig.GetCpu()->TryGetTraceEntry (i, live));
                Assert::IsTrue (rebuilt[i].isAvailable, std::format (L"entry {} available", i).c_str());
                Assert::IsTrue (TraceRig::IsSame (live, rebuilt[i].entry), std::format (L"entry {} at position {} matches", i, position - size + i).c_str());
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  TheTraceReachesAsFarBackAsHistory
        //
        //  With history, the trace has every instruction since history began,
        //  far more than the ring's capacity, and the oldest are rebuilt.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TheTraceReachesAsFarBackAsHistory)
        {
            TraceRig                   rig;
            std::vector<TraceRecord>   window;
            uint64_t                   oldest   = rig.controller.GetKeyframes().GetInfo (0).position;



            rig.trace.SetHistory (&rig.logs.GetTraceHistory());

            Assert::AreEqual ((uint64_t) (rig.machine.GetPosition() - oldest), (uint64_t) rig.trace.GetSize(), L"every instruction since history began");
            Assert::IsTrue   (rig.trace.GetSize() > TraceController::kCapacity, L"more than the ring's capacity");

            rig.trace.GetWindow (0, 3, window);

            Assert::AreEqual ((size_t) 3, window.size());
            Assert::IsTrue   (window[0].isAvailable, L"the oldest is rebuilt");
            Assert::IsTrue   (window[0].cycles < window[1].cycles && window[1].cycles < window[2].cycles, L"in order");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ACycleFindsItsInstruction
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ACycleFindsItsInstruction)
        {
            TraceRig                   rig;
            std::vector<TraceRecord>   window;
            std::vector<TraceRecord>   found;
            size_t                     index  = 0;



            rig.trace.SetHistory (&rig.logs.GetTraceHistory());

            rig.trace.GetWindow (1000, 2, window);
            Assert::AreEqual ((size_t) 2, window.size());

            Assert::IsTrue   (rig.trace.TryFindCycle (window[1].cycles, index));
            Assert::AreEqual ((size_t) 1001, index, L"the cycle of an entry finds that entry");

            Assert::IsTrue   (rig.trace.TryFindCycle (window[0].cycles + 1, index));
            Assert::AreEqual ((size_t) 1001, index, L"a cycle within an instruction finds the next");

            rig.trace.GetWindow (rig.trace.GetSize() - 1, 1, window);
            Assert::IsTrue   (rig.trace.TryFindCycle (window[0].cycles, index), L"the newest, from the ring");
            Assert::AreEqual (rig.trace.GetSize() - 1, index);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AfterTheRingStopsHistoryHasTheNewest
        //
        //  Off, the ring stops while the machine runs on, so it no longer has
        //  the newest instructions; history does.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AfterTheRingStopsHistoryHasTheNewest)
        {
            TraceRig                   rig;
            std::vector<TraceRecord>   window;
            Cpu::TraceEntry            ringLast = {};
            HRESULT                    hr       = S_OK;



            rig.trace.SetHistory (&rig.logs.GetTraceHistory());
            rig.trace.Off();
            rig.machine.RunCycles (200'000);

            hr = rig.controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            rig.trace.GetWindow (rig.trace.GetSize() - 3, 3, window);

            Assert::AreEqual ((size_t) 3, window.size());

            for (const TraceRecord & record : window)
            {
                Assert::IsTrue (record.isAvailable, L"rebuilt from history");
            }

            Assert::IsTrue (rig.GetCpu()->TryGetTraceEntry (rig.GetCpu()->GetTraceSize() - 1, ringLast));
            Assert::IsTrue (window[2].cycles > ringLast.cycles, L"newer than the ring's last");
        }
    };
}
