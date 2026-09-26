#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlersTests
//
//  The run commands and the commands fed by a run, mostly against a real
//  machine driven synchronously; the queue, cycles, video and benchmark
//  commands against the mock.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ExecutionHandlersTests)
    {
    public:

        using Rig        = HandlerRig<ExecutionHandlers>;
        using MachineRig = MachineHandlerRig<ExecutionHandlers>;

        static std::vector<std::string> SplitLines (const std::string & text)
        {
            std::vector<std::string>  lines;



            for (size_t start = 0, end = text.find ('\n'); end != std::string::npos; start = end + 1, end = text.find ('\n', start))
            {
                lines.push_back (text.substr (start, end - start));
            }

            return lines;
        }



        //  P n and RTS n take n steps, as T n does, and announce one stop.
        TEST_METHOD (CountedStepOverAndStepOut_TakeEveryStep)
        {
            MachineRig  rig;
            size_t      stops = 0;



            // $0300: JSR $0310 / NOP / JMP $0300    $0310: INX / RTS
            rig.Load (0x0300, { 0x20, 0x10, 0x03, 0xEA, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0x60 }, 0x0300);

            stops = rig.sink.stops.size();
            rig.RunOk ("P 3");
            Assert::AreEqual ((Word) 0x0300,   rig.LastStop().pc, L"over the call, the NOP and the JMP");
            Assert::AreEqual ((uint8_t) 1,     rig.LastStop().registers.x, L"the call ran once");
            Assert::AreEqual (stops + 1,       rig.sink.stops.size(), L"one stop announced");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step);

            // $0300: JSR $0320 / NOP    $0320: JSR $0330 / RTS    $0330: NOP / RTS
            rig.Load (0x0300, { 0x20, 0x20, 0x03, 0xEA }, 0x0300);
            rig.Load (0x0320, { 0x20, 0x30, 0x03, 0x60 }, 0x0300);
            rig.Load (0x0330, { 0xEA, 0x60 }, 0x0300);
            rig.RunOk ("T 2");
            Assert::AreEqual ((Word) 0x0330, rig.LastStop().pc, L"two calls deep");

            stops = rig.sink.stops.size();
            rig.RunOk ("RTS 2");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"out of both routines");
            Assert::AreEqual (stops + 1,     rig.sink.stops.size());
        }



        //  $0300: JSR $0310 / NOP    $0310: INX / RTS
        //  $03FB (the NMI's user vector): JMP $0320    $0320: INC $10 / RTI
        static void LoadCallAndNmiHandler (MachineRig & rig)
        {
            rig.Load (0x0010, { 0x00 }, 0x0300);
            rig.Load (0x0300, { 0x20, 0x10, 0x03, 0xEA }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0x60 }, 0x0300);
            rig.Load (s_kNmiUserVector, { 0x4C, 0x20, 0x03 }, 0x0300);
            rig.Load (0x0320, { 0xE6, 0x10, 0x40 }, 0x0300);
        }



        //  The same, with an NMI pending, so it is taken in place of the JSR.
        static void LoadCallWithAnNmiPending (MachineRig & rig)
        {
            LoadCallAndNmiHandler (rig);
            RaiseNmi (rig);
        }



        static void RaiseNmi (MachineRig & rig)
        {
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kNonMaskable, true);
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kNonMaskable, false);
        }



        static Byte PeekHandlerCount (MachineRig & rig)
        {
            Byte  count = 0;



            rig.target.TryPeek (0x0010, count);
            return count;
        }



        static constexpr Word  s_kNmiUserVector = 0x03FB;



        //  P runs an interrupt taken in place of the JSR through its RTI, then
        //  steps over the JSR: it stops after the call returns, not in the
        //  handler and not back on the JSR.
        TEST_METHOD (StepOverAJsrPreemptedByAnNmi_RunsTheHandlerThenTheCall)
        {
            MachineRig  rig;



            LoadCallWithAnNmiPending (rig);

            rig.RunOk ("P");

            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"after the call returns");
            Assert::AreEqual ((Byte) 1,      PeekHandlerCount (rig), L"the handler ran once");
            Assert::AreEqual ((uint8_t) 1,   rig.LastStop().registers.x, L"the call ran once");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step);
        }



        //  P over an instruction other than a call runs the interrupt through
        //  as well, then that one instruction.
        TEST_METHOD (StepOverANopPreemptedByAnNmi_RunsTheHandlerThenTheNop)
        {
            MachineRig  rig;



            LoadCallWithAnNmiPending (rig);
            rig.Load (0x0300, { 0xEA, 0xEA }, 0x0300);

            rig.RunOk ("P");

            Assert::AreEqual ((Word) 0x0301, rig.LastStop().pc);
            Assert::AreEqual ((Byte) 1,      PeekHandlerCount (rig), L"the handler ran once");
        }



        //  A breakpoint in the handler still stops a step over that runs
        //  through the interrupt, as one in a called routine does.
        TEST_METHOD (StepOverThroughAnNmi_StopsAtABreakpointInTheHandler)
        {
            MachineRig  rig;



            LoadCallWithAnNmiPending (rig);
            rig.session.GetBreakpoints().AddAddress (0x0320, 0x0320);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("P");

            Assert::AreEqual ((Word) 0x0320, rig.LastStop().pc);
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Breakpoint);
        }



        //  T lands in the handler of an interrupt taken in place of the JSR.
        TEST_METHOD (StepIntoAJsrPreemptedByAnNmi_LandsInTheHandler)
        {
            MachineRig  rig;



            LoadCallWithAnNmiPending (rig);

            rig.RunOk ("T");

            Assert::AreEqual ((Word) s_kNmiUserVector, rig.LastStop().pc, L"in the handler");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step);
        }



        //  RTS runs an interrupt taken inside the routine through its RTI and
        //  goes on out of the routine.
        TEST_METHOD (StepOutWithAnNmiPending_RunsTheHandlerAndReturns)
        {
            MachineRig  rig;



            LoadCallAndNmiHandler (rig);

            rig.RunOk ("T");
            Assert::AreEqual ((Word) 0x0310, rig.LastStop().pc, L"in the routine");

            RaiseNmi (rig);
            rig.RunOk ("RTS");

            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"out of the routine");
            Assert::AreEqual ((Byte) 1,      PeekHandlerCount (rig), L"the handler ran once");
        }



        //  RTS in a routine entered before the step began, so no call is on
        //  record, ends on the stack pointer. An IRQ taken after the routine
        //  pulls a byte is not its return: the handler's RTI leaves the stack
        //  above where the step began, but the step goes on to the RTS.
        TEST_METHOD (StepOutWithNoFrameOnRecord_RunsAnIrqThrough)
        {
            static constexpr Word  kIrqUserVector = 0x03FE;
            MachineRig             rig;
            Cpu6502Registers       registers;



            // $0310: PLA / CLI / PHA / RTS
            // $0320: PLA / ORA #$04 / PHA / INC $10 / LDA $45 / RTI   (masks the IRQ it returns to)
            rig.Load (0x0010, { 0x00 }, 0x0310);
            rig.Load (0x0310, { 0x68, 0x58, 0x48, 0x60 }, 0x0310);
            rig.Load (0x0320, { 0x68, 0x09, 0x04, 0x48, 0xE6, 0x10, 0xA5, 0x45, 0x40 }, 0x0310);
            rig.Load (kIrqUserVector, { 0x20, 0x03 }, 0x0310);

            //  A return to $0303 on the stack, as a JSR at $0300 left it.
            rig.Load (0x01FE, { 0x02, 0x03 }, 0x0310);
            registers    = rig.target.GetRegisters();
            registers.sp = 0xFD;
            rig.target.SetRegisters (registers);
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);

            rig.RunOk ("RTS");

            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"out of the routine, not back in it after the RTI");
            Assert::AreEqual ((Byte) 1,      PeekHandlerCount (rig), L"the handler ran once");
        }



        //  A step that is complete on the cycle the budget runs out is a step:
        //  the reason a client acts on wins over the budget.
        TEST_METHOD (AStepEndingAsTheBudgetRunsOut_IsAStep)
        {
            MachineRig  rig;



            // $0300: LDA #$41 (2 cycles) / NOP
            rig.Load (0x0300, { 0xA9, 0x41, 0xEA }, 0x0300);

            rig.RunOk ("BUDGET 2");
            rig.RunOk ("T");
            Assert::AreEqual ((Word) 0x0302, rig.LastStop().pc);
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Step, L"not Budget");

            rig.RunOk ("BUDGET 1");
            rig.RunOk ("T 2");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Budget, L"a budget that runs out first is still the reason");
        }



        //  A Monitor G leaves the Monitor's return on the stack; a program
        //  that returns there stops as a run to it, not as a breakpoint.
        TEST_METHOD (MonitorGo_ReturningToTheMonitorIsARunTo)
        {
            MachineRig  rig;



            // $0300: INX / RTS
            rig.Load (0x0300, { 0xE8, 0x60 }, 0x0400);

            (void) rig.session.ExecuteLine ("300G", CommandMode::Monitor);
            Assert::IsTrue   (rig.LastStop().reason == StopReason::RunTo, L"not a breakpoint");
            Assert::IsFalse  (rig.LastStop().breakpointId.has_value());
            Assert::AreEqual ((uint8_t) 1, rig.LastStop().registers.x, L"the program ran");
        }



        //  A breakpoint partway through a program started by a Monitor G
        //  leaves the return armed: resuming, the final RTS stops at the
        //  Monitor's return rather than running on into the ROM.
        TEST_METHOD (MonitorGo_ABreakpointPartwayLeavesTheReturnArmed)
        {
            MachineRig          rig;
            BreakpointHandlers  breakpoints;



            // $0300: INX / INX / RTS
            rig.Load (0x0300, { 0xE8, 0xE8, 0x60 }, 0x0400);
            rig.session.AddHandler (&breakpoints);
            rig.RunOk ("BUDGET 100000");
            rig.RunOk ("BP 301");

            (void) rig.session.ExecuteLine ("300G", CommandMode::Monitor);
            Assert::AreEqual ((Word) 0x0301, rig.LastStop().pc, L"at the breakpoint");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Breakpoint);

            rig.RunOk ("G");
            Assert::IsTrue   (rig.LastStop().reason == StopReason::RunTo, L"not the budget, in the Monitor ROM");
            Assert::AreEqual ((Word) 0xFF69, rig.LastStop().pc, L"at the Monitor's return");
            Assert::AreEqual ((uint8_t) 2,   rig.LastStop().registers.x, L"the program ran to its end");
        }



        //  A reset abandons the stack the Monitor's return was pushed onto, so
        //  it no longer stops a run that later passes through that address.
        TEST_METHOD (MonitorGo_AResetDisarmsTheReturn)
        {
            MachineRig          rig;
            BreakpointHandlers  breakpoints;



            // $0300: INX / INX / RTS
            rig.Load (0x0300, { 0xE8, 0xE8, 0x60 }, 0x0400);
            rig.session.AddHandler (&breakpoints);
            rig.RunOk ("BUDGET 100000");
            rig.RunOk ("BP 301");

            (void) rig.session.ExecuteLine ("300G", CommandMode::Monitor);
            Assert::AreEqual ((Word) 0x0301, rig.LastStop().pc, L"at the breakpoint");

            rig.machine.SoftReset();
            rig.session.OnReset (false);

            // $0300: JMP $FF69
            rig.Load (0x0300, { 0x4C, 0x69, 0xFF }, 0x0300);
            rig.RunOk ("BPC *");
            rig.RunOk ("G");
            Assert::IsTrue (rig.LastStop().reason == StopReason::Budget, L"no stop at the Monitor's return after a reset");
        }



        //  A long counted step runs its steps one after another, never one
        //  inside another: a count in the thousands does not grow the stack.
        TEST_METHOD (ALongCountedStepOver_RunsEveryStep)
        {
            MachineRig  rig;



            // $0300: INX / JMP $0300
            rig.Load (0x0300, { 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);

            rig.RunOk ("P 1388");   // counts are hex: 5,000 steps
            Assert::AreEqual ((Word) 0x0300,           rig.LastStop().pc);
            Assert::AreEqual ((uint8_t) (2500 & 0xFF), rig.LastStop().registers.x, L"2,500 INX in 5,000 steps");
        }



        //  The Monitor's T traces until something stops it.
        TEST_METHOD (MonitorTrace_RunsUntilAStop)
        {
            MachineRig  rig;



            // $0300: INX / INX / INX / NOP
            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xEA }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            (void) rig.session.ExecuteLine ("T", CommandMode::Monitor);
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"three instructions, to the breakpoint");
            Assert::AreEqual ((uint8_t) 3,   rig.LastStop().registers.x);
        }



        //  A breakpoint partway through a counted step ends it there.
        TEST_METHOD (CountedStep_EndsAtABreakpoint)
        {
            MachineRig  rig;



            // $0300: JSR $0310 / NOP / JMP $0300    $0310: INX / RTS
            rig.Load (0x0300, { 0x20, 0x10, 0x03, 0xEA, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0x60 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0304, 0x0304);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("P 5");
            Assert::AreEqual ((Word) 0x0304, rig.LastStop().pc);
            Assert::IsTrue   (rig.LastStop().reason == StopReason::Breakpoint);

            rig.RunOk ("P");
            Assert::AreEqual ((Word) 0x0300, rig.LastStop().pc, L"the next P is a single step again");
        }



        TEST_METHOD (Steps_Go_RunTo_AndSetPc)
        {
            MachineRig  rig;



            // $0300: JSR $0310 / NOP / JMP $0300    $0310: INX / RTS
            rig.Load (0x0300, { 0x20, 0x10, 0x03, 0xEA, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0x60 }, 0x0300);

            rig.RunOk ("P");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"P steps over the call");
            Assert::AreEqual ((uint8_t) 1,   rig.LastStop().registers.x);

            rig.RunOk ("T");
            Assert::AreEqual ((Word) 0x0304, rig.LastStop().pc);

            rig.RunOk ("= 300");
            Assert::AreEqual ((Word) 0x0300, rig.target.GetRegisters().pc);

            rig.RunOk ("TL");
            Assert::AreEqual ((Word) 0x0310, rig.LastStop().pc, L"TL is T, and T steps into the call");

            rig.RunOk ("RTS");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"RTS steps out");

            rig.RunOk ("G 310");
            Assert::AreEqual ((int) StopReason::RunTo, (int) rig.LastStop().reason);
            Assert::AreEqual ((Word) 0x0310, rig.LastStop().pc);

            rig.session.GetBreakpoints().AddAddress (0x0304, 0x0304);
            rig.session.OnStopConditionsChanged();
            rig.RunOk ("300G");
            Assert::AreEqual ((int) StopReason::Breakpoint, (int) rig.LastStop().reason);
            Assert::AreEqual ((Word) 0x0304, rig.LastStop().pc, L"addrG sets PC and runs");
        }



        TEST_METHOD (G_SkipRange_StopsWhenPcLeavesIt)
        {
            MachineRig  rig;



            // $0300: INX / INX / JMP $0400    $0400: INX / JMP $0400
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x04 }, 0x0300);
            rig.Load (0x0400, { 0xE8, 0x4C, 0x00, 0x04 }, 0x0300);

            rig.RunOk ("BUDGET 100000");
            rig.RunOk ("G FFFF 300,10");
            Assert::AreEqual ((int) StopReason::RunTo, (int) rig.LastStop().reason);
            Assert::AreEqual ((Word) 0x0400, rig.LastStop().pc, L"the run ends when PC leaves $0300-$030F");
            Assert::AreEqual ((uint8_t) 2,   rig.LastStop().registers.x);
        }



        TEST_METHOD (JSR_CallsAndReturnsToPc)
        {
            MachineRig  rig;



            // $0300: NOP    $0310: INX / RTS
            rig.Load (0x0300, { 0xEA }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0x60 }, 0x0300);

            rig.RunOk ("BUDGET 100000");
            rig.RunOk ("JSR 310");
            Assert::AreEqual ((int) StopReason::RunTo, (int) rig.LastStop().reason);
            Assert::AreEqual ((Word) 0x0300,   rig.LastStop().pc, L"the subroutine returned to the program counter");
            Assert::AreEqual ((uint8_t) 1,     rig.LastStop().registers.x);
            Assert::AreEqual ((uint8_t) 0xFF,  rig.LastStop().registers.sp, L"the stack is back where it was");
        }



        TEST_METHOD (NOP_ZAP_OverwriteTheWholeInstruction)
        {
            MachineRig  rig;
            Reply       reply;



            rig.Load (0x0300, { 0xA9, 0x41, 0x60 }, 0x0300);

            reply = rig.RunOk ("NOP");
            Assert::AreEqual ((size_t) 2, reply.text.size(), L"a two-byte instruction becomes two NOPs");
            Assert::AreEqual (std::string ("0300: EA       NOP"), reply.text[0]);
            Assert::AreEqual (std::string ("0301: EA       NOP"), reply.text[1]);

            rig.RunOk ("= 302");
            rig.RunOk ("zap");
            Assert::IsTrue (rig.RunOk ("T").status == CommandStatus::Ok);
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc, L"the RTS is gone");

            //  An instruction running into memory that cannot be written is
            //  left whole: none of it becomes a NOP.
            rig.Load (0xBFFF, { 0x4C }, 0xBFFF);
            rig.RunFails ("NOP", "memory not writable");

            Byte  first = 0;

            Assert::IsTrue   (rig.target.TryPeek (0xBFFF, first));
            Assert::AreEqual ((Byte) 0x4C, first, L"the byte that could be written was put back");
        }



        TEST_METHOD (KEY_QueuesUntilTheStrobeClears)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Queued 2 keys. Keys waiting: 1."), rig.RunOk ("KEY 41 42").text.at (0));
            Assert::AreEqual (std::string ("Queued 1 key. Keys waiting: 2."),  rig.RunOk ("KEY 43").text.at (0));
            Assert::AreEqual ((size_t) 1, rig.target.injectedKeys.size(), L"the first key goes at once");
            Assert::AreEqual ((Byte) 0x41, rig.target.injectedKeys[0]);

            rig.session.SetInstructionObserver (&rig.handlers);
            rig.RunOk ("G");
            rig.session.OnInstruction (0x0300);
            Assert::AreEqual ((size_t) 1, rig.target.injectedKeys.size(), L"the strobe is still set");

            rig.target.keyPending = false;
            rig.session.OnInstruction (0x0301);
            Assert::AreEqual ((size_t) 2, rig.target.injectedKeys.size());
            Assert::AreEqual ((Byte) 0x42, rig.target.injectedKeys[1]);
            rig.RunFails ("KEY", "invalid arguments");
        }



        //  The CPU keeps the record, so it holds with no observer installed:
        //  a run with nothing to stop on never reports an instruction.
        TEST_METHOD (LBR_RecordsTheLastControlTransfer)
        {
            MachineRig  rig;



            // $0300: INX / JMP $0305 / NOP    $0305: INX / INX / BNE $0305
            rig.Load (0x0300, { 0xE8, 0x4C, 0x05, 0x03, 0xEA, 0xE8, 0xE8, 0xD0, 0xFC }, 0x0300);
            Assert::AreEqual (std::string ("No branch recorded."), rig.RunOk ("LBR").text.at (0));

            rig.session.GetBreakpoints().AddAddress (0x0307, 0x0307);
            rig.session.OnStopConditionsChanged();
            rig.RunOk ("G");
            Assert::AreEqual ((Word) 0x0307, rig.LastStop().pc);
            Assert::AreEqual (std::string ("Last branch at $0301"), rig.RunOk ("LBR").text.at (0), L"the JMP, not its destination");

            rig.RunOk ("T 2");
            Assert::AreEqual ((Word) 0x0306, rig.LastStop().pc);
            Assert::AreEqual (std::string ("Last branch at $0307"), rig.RunOk ("LBR").text.at (0), L"the taken BNE");
        }



        //  A reset sends the CPU through the vector, not through a transfer the
        //  program made, so a record from before it would point at code that
        //  did not lead to where the CPU now is.
        TEST_METHOD (LBR_IsClearedByAResetAndAPowerCycle)
        {
            MachineRig  rig;



            // $0300: JMP $0310
            rig.Load (0x0300, { 0x4C, 0x10, 0x03 }, 0x0300);
            rig.RunOk ("T");
            Assert::AreEqual (std::string ("Last branch at $0300"), rig.RunOk ("LBR").text.at (0));

            rig.machine.SoftReset();
            Assert::AreEqual (std::string ("No branch recorded."), rig.RunOk ("LBR").text.at (0), L"after Ctrl+Reset");

            rig.Load (0x0300, { 0x4C, 0x10, 0x03 }, 0x0300);
            rig.RunOk ("T");
            rig.machine.PowerCycle();
            Assert::AreEqual (std::string ("No branch recorded."), rig.RunOk ("LBR").text.at (0), L"after a power cycle");
        }



        //  A taken branch whose displacement is zero, and a jump to the next
        //  instruction, leave PC where it would have been anyway.
        TEST_METHOD (LBR_RecordsATransferToTheNextInstruction)
        {
            MachineRig  rig;



            // $0300: INX / BNE $0303 / JMP $0306 / NOP
            rig.Load (0x0300, { 0xE8, 0xD0, 0x00, 0x4C, 0x06, 0x03, 0xEA }, 0x0300);
            rig.RunOk ("T 2");
            Assert::AreEqual (std::string ("Last branch at $0301"), rig.RunOk ("LBR").text.at (0), L"the taken BNE");

            rig.RunOk ("T");
            Assert::AreEqual (std::string ("Last branch at $0303"), rig.RunOk ("LBR").text.at (0), L"the JMP");
        }


        TEST_METHOD (PROFILE_CountsWhileOnDuringDebuggerRuns_ResetAndSave)
        {
            MachineRig                rig;
            std::vector<std::string>  lines;
            std::string               saved;



            rig.session.SetInstructionObserver (&rig.handlers);

            // $0300: INX / INX / INX / LDA #$41 / JMP $0300
            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xA9, 0x41, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0305, 0x0305);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("G");
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), rig.RunOk ("PROFILE").text.at (0), L"nothing is counted while profiling is off");

            Assert::AreEqual (std::string ("Profiling on."), rig.RunOk ("PROFILE ON").text.at (0));
            rig.machine.RunCycles (20);
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), rig.RunOk ("PROFILE").text.at (0), L"a machine running on its own is not profiled");

            rig.RunOk ("= 300");
            rig.RunOk ("G");
            lines = rig.RunOk ("PROFILE LIST").text;
            Assert::AreEqual (std::string ("Instructions: 4, cycles: 8"),                                  lines.at (0), L"three INX and one LDA");
            Assert::AreEqual (std::string ("INX    [No Operand]                  3          6   75.0%"), lines.at (2));
            Assert::AreEqual (std::string ("LDA    #Immediate                    1          2   25.0%"), lines.at (3));

            Assert::AreEqual (std::string ("Saved the profile to Profile.txt."), rig.RunOk ("PROFILE SAVE").text.at (0));
            saved = rig.files.PeekContent (L"C:\\Work\\Profile.txt");
            Assert::IsTrue   (saved.find (lines.at (2) + "\n") != std::string::npos, L"the saved file holds the listed rows");
            Assert::IsTrue   (saved.find ("Address Symbol") != std::string::npos,   L"and the per-address rows");
            Assert::AreEqual (std::string ("Saved the profile to hot.txt."), rig.RunOk ("PROFILE SAVE hot.txt").text.at (0));
            Assert::AreEqual (saved, rig.files.PeekContent (L"C:\\Work\\hot.txt"));

            Assert::AreEqual (std::string ("Profile reset."), rig.RunOk ("PROFILE RESET").text.at (0));
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), rig.RunOk ("PROFILE").text.at (0));
            Assert::AreEqual (std::string ("Profiling off."), rig.RunOk ("PROFILE OFF").text.at (0));
            Assert::IsTrue   (rig.Run ("PROFILE SIDEWAYS").status == CommandStatus::Error);
            Assert::IsTrue   (rig.Run ("PROFILE LIST SIDEWAYS").status == CommandStatus::Error);
        }



        //  A quoted file name with a space in it is one name, as HISTORY SAVE
        //  takes it.
        TEST_METHOD (PROFILE_SAVE_TakesAQuotedNameWithSpaces)
        {
            MachineRig  rig;



            Assert::AreEqual (std::string ("Saved the profile to \"my profile.txt\"."), rig.RunOk ("PROFILE SAVE \"my profile.txt\"").text.at (0));
            Assert::IsFalse  (rig.files.PeekContent (L"C:\\Work\\my profile.txt").empty());
        }



        //  The counts were gathered on the old machine's CPU, whose opcodes the
        //  new one may decode differently, so a machine switch clears them.
        TEST_METHOD (PROFILE_MachineSwitch_ClearsTheCounts)
        {
            MachineRig  rig;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0302, 0x0302);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("PROFILE ON");
            rig.RunOk ("G");
            Assert::AreEqual (std::string ("Instructions: 2, cycles: 4"), rig.RunOk ("PROFILE").text.at (0));

            rig.session.OnMachineChanged ("Apple //e Enhanced");
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), rig.RunOk ("PROFILE").text.at (0));
        }



        TEST_METHOD (PROFILE_SeparatesPenaltiesFromBaseCycles)
        {
            MachineRig                rig;
            std::vector<std::string>  lines;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.session.GetSymbols().Add (SymbolTableId::User, "LOOP", 0x0302);

            // $0300: LDX #$00 / LOOP: LDA $10FF,X / INX / CPX #$04 / BNE LOOP / NOP
            rig.Load (0x0300, { 0xA2, 0x00, 0xBD, 0xFF, 0x10, 0xE8, 0xE0, 0x04, 0xD0, 0xF8, 0xEA }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x030A, 0x030A);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("PROFILE ON");
            rig.RunOk ("G");
            lines = rig.RunOk ("PROFILE LIST").text;
            Assert::AreEqual (std::string ("Instructions: 17, cycles: 48"),                                lines.at (0));
            Assert::AreEqual (std::string ("LDA    Absolute, X                   4         16   33.3%"), lines.at (2), L"the base cycles only");
            Assert::AreEqual (std::string ("BNE    Relative                      4          8   16.7%"), lines.at (3));
            Assert::AreEqual (std::string ("Page crossing                                   3    6.2%"), lines.at (8));
            Assert::AreEqual (std::string ("Taken branches                                  3    6.2%"), lines.at (9));
            Assert::AreEqual (std::string ("Branches crossing a page                        0    0.0%"), lines.at (10));

            lines = rig.RunOk ("PROFILE LIST ADDR").text;
            Assert::AreEqual (std::string ("$0302   LOOP                         19   39.6%"), lines.at (2), L"the hottest address, with its symbol");
            Assert::AreEqual (std::string ("$0308                                11   22.9%"), lines.at (3));
        }



        TEST_METHOD (PROFILE_InstallsNoHook)
        {
            Rig  rig;
            int  changes = rig.target.hookChanges;



            rig.RunOk ("PROFILE ON");
            rig.RunOk ("PROFILE OFF");
            Assert::AreEqual (changes, rig.target.hookChanges, L"profiling rides the debugger's own runs");
            Assert::IsFalse  (rig.target.hookInstalled);
        }


        TEST_METHOD (TF_WritesOneLinePerInstruction)
        {
            MachineRig                rig;
            std::vector<std::string>  lines;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0xE8, 0xA9, 0x41, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            Assert::AreEqual (std::string ("Trace on: trace.txt"), rig.RunOk ("TF trace.txt").text.at (0));
            rig.RunOk ("G");
            lines = SplitLines (rig.files.PeekContent (L"C:\\Work\\trace.txt"));
            Assert::AreEqual ((size_t) 2, lines.size(), L"the file is written at the stop");
            Assert::IsTrue   (lines[0].ends_with ("  0300: INX "),      (L"line 0: " + std::wstring (lines[0].begin(), lines[0].end())).c_str());
            Assert::IsTrue   (lines[1].find ("X=01") != std::string::npos, L"the registers as they were before the instruction");
            Assert::IsTrue   (lines[1].ends_with ("  0301: LDA #$41"));

            Assert::AreEqual (std::string ("Trace off: trace.txt"), rig.RunOk ("TF").text.at (0));
            rig.RunOk ("= 300");
            rig.RunOk ("G");
            Assert::AreEqual ((size_t) 2, SplitLines (rig.files.PeekContent (L"C:\\Work\\trace.txt")).size(), L"nothing recorded while off");

            rig.RunOk ("TF v");
            rig.RunOk ("= 300");
            rig.RunOk ("G");
            lines = SplitLines (rig.files.PeekContent (L"C:\\Work\\Trace.txt"));
            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::IsTrue   (lines[0].starts_with ("V"), L"the video position leads with v");
        }



        TEST_METHOD (TF_OffWithNothingRecorded_KeepsTheExistingFile)
        {
            MachineRig  rig;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0xE8, 0xA9, 0x41, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("TF trace.txt");
            rig.RunOk ("G");
            rig.RunOk ("TF");
            rig.RunOk ("TF trace.txt");
            rig.RunOk ("TF");
            Assert::AreEqual ((size_t) 2, SplitLines (rig.files.PeekContent (L"C:\\Work\\trace.txt")).size());
        }



        //  A quoted file name with a space in it is one name, with v before
        //  or after it.
        TEST_METHOD (TF_TakesAQuotedNameWithSpaces)
        {
            MachineRig  rig;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0xE8, 0xA9, 0x41, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            Assert::AreEqual (std::string ("Trace on: \"my trace.txt\""), rig.RunOk ("TF \"my trace.txt\"").text.at (0));
            rig.RunOk ("G");
            rig.RunOk ("TF");
            Assert::AreEqual ((size_t) 2, SplitLines (rig.files.PeekContent (L"C:\\Work\\my trace.txt")).size());

            rig.RunOk ("TF \"video trace.txt\" v");
            rig.RunOk ("= 300");
            rig.RunOk ("G");
            Assert::IsTrue (rig.files.PeekContent (L"C:\\Work\\video trace.txt").starts_with ("V"), L"v after the name");
        }



        TEST_METHOD (CYCLES_PART_AfterTheCountRestarts_CountsFromTheRestart)
        {
            Rig  rig;



            rig.target.cycleCount = 1000;
            rig.RunOk ("RCC");
            rig.target.cycleCount = 300;
            Assert::AreEqual (std::string ("Cycles: 300"), rig.RunOk ("CYCLES PART").text.at (0));
        }



        TEST_METHOD (CYCLES_RCC_VIDEOINFO_BPV_BENCHMARK)
        {
            Rig        rig;
            StopEvent  stop;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.target.cycleCount = 1000;
            Assert::AreEqual (std::string ("Cycles: 1000"), rig.RunOk ("CYCLES").text.at (0));
            Assert::AreEqual (std::string ("Cycles: 1000"), rig.RunOk ("CYCLES abs").text.at (0));

            rig.RunOk ("RCC");
            rig.target.cycleCount = 1500;
            Assert::AreEqual (std::string ("Cycles: 500"), rig.RunOk ("CYCLES PART").text.at (0));

            rig.RunOk ("G");
            stop.reason = StopReason::Pause;
            stop.cycles = 42;
            rig.target.Stop (stop);
            Assert::AreEqual (std::string ("Cycles: 42"), rig.RunOk ("CYCLES rel").text.at (0), L"the last run's cycles");
            rig.RunFails ("CYCLES sideways", "invalid arguments");

            rig.target.videoPosition = { 42, 17 };
            Assert::AreEqual (std::string ("Scanline $2A, cycle $11"), rig.RunOk ("VIDEOINFO").text.at (0));

            Assert::AreEqual (std::string ("Breakpoint set on video scanlines $A0-$A0. It clears after it fires."), rig.RunOk ("BPV A0").text.at (0));
            Assert::IsTrue   (rig.target.hookInstalled);
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300));
            rig.target.videoPosition.scanline = 160;
            Assert::IsTrue   (rig.session.ShouldStopBefore (0x0300));
            rig.target.Stop (StopEvent { StopReason::Breakpoint, 0x0300 });
            Assert::IsFalse  (rig.session.HasVideoBreak(), L"a video break clears when it fires");
            Assert::IsFalse  (rig.target.hookInstalled);

            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) rig.Run ("BENCHMARK").status);
            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) rig.Run ("EXITBENCH").status);
        }



        //  A video break is a stop condition like any breakpoint, so BPC *,
        //  which clears every breakpoint, and a machine change remove it.
        TEST_METHOD (BPV_ClearedByClearAllAndMachineChange)
        {
            Rig  rig;



            rig.RunOk ("BPV A0");
            Assert::IsTrue  (rig.session.HasVideoBreak());
            rig.session.ClearAllBreakpoints();
            Assert::IsFalse (rig.session.HasVideoBreak(), L"clearing every breakpoint clears the video break");
            Assert::IsFalse (rig.target.hookInstalled);

            rig.RunOk ("BPV A0");
            rig.session.OnMachineChanged ("Apple //e", true);
            Assert::IsFalse (rig.session.HasVideoBreak(), L"a machine change clears the video break");
            Assert::IsFalse (rig.target.hookInstalled);
        }



        //  A scanline past the frame's last can never be reached, so BPV
        //  there is an error.
        TEST_METHOD (BPV_UnreachableIsAnError)
        {
            Rig  rig;



            rig.RunFails ("BPV 200",   "invalid arguments");
            rig.RunFails ("BPV FFFF",  "invalid arguments");
            rig.RunFails ("BPV A0:10", "invalid arguments");
            Assert::IsFalse (rig.session.HasVideoBreak());

            rig.RunOk ("BPV 105");
            Assert::IsTrue  (rig.session.HasVideoBreak(), L"scanline 261 is the frame's last");
        }



        //  A reversed range holds no scanline, so it could never stop.
        TEST_METHOD (BPV_ReversedRangeIsAnError)
        {
            Rig  rig;



            rig.RunFails ("BPV 100:50", "invalid arguments");
            Assert::IsFalse (rig.session.HasVideoBreak());
        }



        //  The break stops when the beam enters the range, not while it is
        //  inside: one armed with the beam already there waits for the next
        //  pass.
        TEST_METHOD (BPV_StopsOnEnteringTheRange)
        {
            Rig  rig;



            rig.target.videoPosition.scanline = 0xA0;
            rig.RunOk ("BPV A0,10");
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"already inside when armed");
            rig.target.videoPosition.scanline = 0xA1;
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0301), L"still inside");

            rig.target.videoPosition.scanline = 0;
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0302), L"outside");
            rig.target.videoPosition.scanline = 0xA0;
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0303), L"entered");
        }



        //  An interrupt taken in place of an instruction is not that
        //  instruction: it gets no trace line and no profile count, and the
        //  dispatch's cycles are not billed to it. It is traced and counted
        //  once, when it runs after the return.
        TEST_METHOD (TF_PROFILE_AnInterruptIsNotThePreemptedInstruction)
        {
            MachineRig                rig;
            std::vector<std::string>  lines;
            size_t                    jsrLines = 0;



            rig.session.SetInstructionObserver (&rig.handlers);
            LoadCallWithAnNmiPending (rig);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            rig.RunOk ("PROFILE ON");
            rig.RunOk ("TF trace.txt");
            rig.RunOk ("G");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc);

            lines = SplitLines (rig.files.PeekContent (L"C:\\Work\\trace.txt"));

            for (const std::string & line : lines)
            {
                jsrLines += line.find ("0300: JSR") != std::string::npos ? 1 : 0;
            }

            Assert::AreEqual ((size_t) 1, jsrLines, L"the JSR is traced once, when it runs");
            Assert::AreEqual ((size_t) 6, lines.size(), L"JMP, INC, RTI, JSR, INX, RTS");
            Assert::AreEqual (std::string ("Instructions: 6, cycles: 28"), rig.RunOk ("PROFILE").text.at (0), L"the dispatch is billed to nothing");
        }



        //  A trace file that cannot be written is reported, when tracing is
        //  turned on and when a write at a stop failed.
        TEST_METHOD (TF_AFileThatCannotBeWrittenIsAnError)
        {
            MachineRig  rig;
            HRESULT     hr = S_OK;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0xE8, 0xA9, 0x41, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.session.GetBreakpoints().AddAddress (0x0303, 0x0303);
            rig.session.OnStopConditionsChanged();

            hr = rig.files.WriteAllText (L"C:\\Work\\locked.txt", "kept");
            Assert::AreEqual (S_OK, hr);
            hr = rig.files.SetReadOnlyAttribute (L"C:\\Work\\locked.txt", true);
            Assert::AreEqual (S_OK, hr);
            rig.RunFails ("TF locked.txt", "file not written");
            Assert::AreEqual (std::string ("kept"), rig.files.PeekContent (L"C:\\Work\\locked.txt"));

            rig.RunOk ("TF trace.txt");
            Assert::IsFalse (rig.files.Exists (L"C:\\Work\\trace.txt"), L"checking the path leaves no file behind");
            hr = rig.files.WriteAllText (L"C:\\Work\\trace.txt", "");
            Assert::AreEqual (S_OK, hr);
            hr = rig.files.SetReadOnlyAttribute (L"C:\\Work\\trace.txt", true);
            Assert::AreEqual (S_OK, hr);
            rig.RunOk ("G");
            rig.RunFails ("TF", "file not written");
        }
    };
}
