#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointRunTests
//
//  Breakpoints and watchpoints as a run on a real machine meets them: the
//  interrupt breakpoint, what a read watchpoint sees of the CPU's own bus
//  cycles, a before-mode watchpoint on the instruction a run starts on, and
//  a watch hit raised between runs.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BreakpointRunTests)
    {
    public:

        //  The breakpoint commands, with the run and I/O commands beside them.
        //  A run that misses its stop ends at the budget rather than never.
        class Rig : public MachineHandlerRig<BreakpointHandlers>
        {
        public:
            static constexpr uint64_t  kBudget = 100000;

            ExecutionHandlers  execution;
            MemoryHandlers     memory;

            Rig()
            {
                session.AddHandler (&execution);
                session.AddHandler (&memory);
                session.SetBudget  (kBudget);
            }
        };

        static constexpr Word  kNmiUserVector = 0x03FB;



        //  BRKINT ON stops in the handler, on the first instruction the
        //  interrupt dispatched to.
        TEST_METHOD (BRKINT_StopsOnTheHandlersFirstInstruction)
        {
            Rig  rig;



            // $0300: NOP / NOP / JMP $0300    $03FB: JMP $0320    $0320: RTI
            rig.Load (0x0300, { 0xEA, 0xEA, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.Load (kNmiUserVector, { 0x4C, 0x20, 0x03 }, 0x0300);
            rig.Load (0x0320, { 0x40 }, 0x0300);

            rig.RunOk ("BRKINT ON");
            rig.RunOk ("T 2");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step, L"no interrupt, no stop");

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kNonMaskable, true);
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kNonMaskable, false);
            rig.RunOk ("G");

            Assert::IsTrue   (rig.LastStop().reason == StopReason::Breakpoint);
            Assert::AreEqual ((Word) kNmiUserVector, rig.LastStop().pc);
            Assert::AreEqual (1u, rig.session.GetBreakpoints().GetAll().at (0).hits);
        }



        //  The CPU fetching the instruction bytes on a watched page, and an
        //  indexed store reading its target before writing it, are not the
        //  program reading memory.
        TEST_METHOD (BPMR_IgnoresInstructionFetchesAndAStoresOwnRead)
        {
            Rig  rig;



            // $0300: LDX #0 / STA $0310,X / LDA $0311 / NOP
            rig.Load (0x0300, { 0xA2, 0x00, 0x9D, 0x10, 0x03, 0xAD, 0x11, 0x03, 0xEA }, 0x0300);

            rig.RunOk ("BPMR 300:308");
            rig.RunOk ("T 3");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step, L"code fetches are not reads");
            Assert::AreEqual ((Word) 0x0308, rig.LastStop().pc);

            rig.RunOk ("BPC *");
            rig.RunOk ("BPMR 310:311");
            rig.Load  (0x0300, {}, 0x0300);
            rig.RunOk ("T 3");

            Assert::IsTrue   (rig.LastStop().reason == StopReason::Watchpoint);
            Assert::IsTrue   (rig.LastStop().watch.has_value());
            Assert::AreEqual ((Word) 0x0311, rig.LastStop().watch->address, L"the LDA's read, not the store's");
            Assert::AreEqual (1u, rig.session.GetWatchpoints().GetAll().at (0).hits);
        }



        //  A before-mode watchpoint on the instruction a run begins on stops
        //  before it; resuming from that stop runs it.
        TEST_METHOD (BeforeWatchpoint_OnTheRunsFirstInstruction_Stops)
        {
            Rig  rig;



            // $0300: STA $0400 / NOP
            rig.Load (0x0300, { 0x8D, 0x00, 0x04, 0xEA }, 0x0300);

            rig.RunOk ("BPMW 400 BEFORE");
            rig.RunOk ("T");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Watchpoint);
            Assert::AreEqual ((Word) 0x0300, rig.LastStop().pc);

            rig.RunOk ("T");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step, L"the stop is not repeated");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc);
        }



        //  OUT makes a real bus write while the machine is paused. A watch hit
        //  it raises belongs to no run, so the next run does not stop for it.
        TEST_METHOD (WatchHit_BetweenRuns_DoesNotStopTheNextRun)
        {
            Rig  rig;



            // $0300: NOP / NOP
            rig.Load (0x0300, { 0xEA, 0xEA }, 0x0300);

            rig.RunOk ("BPMW C030");
            rig.RunOk ("OUT C030 5");
            rig.RunOk ("T");

            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step);
            Assert::AreEqual ((Word) 0x0301, rig.LastStop().pc);
        }
    };
}
