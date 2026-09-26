#include "Pch.h"

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
        }



        TEST_METHOD (KEY_QueuesUntilTheStrobeClears)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Queued 2 keys. Keys waiting: 1."), rig.RunOk ("KEY 41 42").text.at (0));
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
            Assert::AreEqual (std::string ("Scanline 42, cycle 17"), rig.RunOk ("VIDEOINFO").text.at (0));

            rig.RunOk ("BPV A0");
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
    };
}
