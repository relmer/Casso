#include "Pch.h"

#include "WinDbgRigs.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbg mode's run commands, each on the window's path (the controller's
//  run driver, ended at slice boundaries) and in batch (the synchronous
//  driver, with the stop printed in WinDbg's layout). WinDbgProgram gives
//  the code each one runs.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgStepIntoTests
    //
    //  t, t N, t =addr.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgStepIntoTests)
    {
    public:

        //  Sixty-four NOPs at $2000, for counting steps where hex and decimal
        //  counts end in different places.
        static void LoadNops (WinDbgWindowRig & rig)
        {
            for (Word at = 0x2000; at < 0x2040; at++)
            {
                rig.Load (at, { 0xEA });
            }
        }



        TEST_METHOD (T_StepsIntoTheCall_OnTheWindowPath)
        {
            WinDbgWindowRig          rig;
            Reply                    reply = rig.TypeOk ("t");
            WinDbgWindowRig::Stop    stop;



            Assert::IsTrue (reply.text.empty(), L"a step replies with no text; the stop reports it");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::Stepping, L"the step is a run until its slice ends");
            Assert::IsTrue (rig.FinishRun());

            stop = rig.LastStop();
            Assert::AreEqual (std::string ("step"), stop.reason);
            Assert::AreEqual ((Word) 0x0310,        stop.pc);
            Assert::AreEqual ((Word) 0x0310,        rig.Registers().pc);
            Assert::AreEqual ((Byte) 0xFD,          rig.Registers().sp, L"the JSR pushed its return address");
            Assert::AreEqual ((Byte) 0x03,          rig.Peek (0x01FF));
            Assert::AreEqual ((Byte) 0x02,          rig.Peek (0x01FE));
            Assert::AreEqual ((Byte) 0x00,          rig.Registers().a, L"only the JSR ran");
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        TEST_METHOD (T_StepsIntoTheCall_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("t\nt\n"));
            Assert::AreEqual (std::string ("Step at $0310\na=00 x=00 y=00 sp=fd pc=0310 nv-bdIzc\n"), rig.Section ("t", 0));
            Assert::AreEqual (std::string ("Step at $0312\na=41 x=00 y=00 sp=fd pc=0312 nv-bdIzc\n"), rig.Section ("t", 1));
            Assert::AreEqual ((Word) 0x0312, rig.Registers().pc);
        }



        //  The count is hex unless it carries 0n; 0 steps once. Each case
        //  starts at $2000, among NOPs, so the PC is the count.
        TEST_METHOD (T_Count_IsHex_UnlessDecimal)
        {
            static constexpr std::pair<const char *, Word>  kCases[] =
            {
                { "t 10",    0x2010 },
                { "t 0x10",  0x2010 },
                { "t 0n10",  0x200A },
                { "t 0",     0x2001 },
                { "t 1",     0x2001 },
                { "T 3",     0x2003 },
                { "t 3f",    0x203F },
            };
            size_t  checked = 0;



            for (const auto & [line, pc] : kCases)
            {
                WinDbgWindowRig          rig;
                WinDbgWindowRig::Stop    stop;



                LoadNops (rig);
                rig.SetPc (0x2000);

                stop = rig.RunToStop (line);
                Assert::AreEqual (std::string ("step"), stop.reason, std::wstring (line, line + strlen (line)).c_str());
                Assert::AreEqual (pc, stop.pc,                        std::wstring (line, line + strlen (line)).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (kCases), checked);
        }



        //  A count runs through the call the program makes: JSR, LDA, STA.
        TEST_METHOD (T_Count_RunsThatManyInstructions_ThroughACall)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop = rig.RunToStop ("t 3");



            Assert::AreEqual ((Word) 0x0315, stop.pc);
            Assert::AreEqual ((Byte) 0x41,   rig.Registers().a);
            Assert::AreEqual ((Byte) 0x41,   rig.Peek (0x0400), L"the STA ran");
            Assert::AreEqual ((Byte) 0xFD,   rig.Registers().sp);
        }



        //  The largest count a step takes, 65535 instructions, spans many
        //  slices; one more is outside the range a count is read in.
        TEST_METHOD (T_Count_LargestIsFfff)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("t ffff");
            Assert::IsFalse (rig.FinishRun (1), L"one slice does not finish 65535 instructions");
            Assert::IsTrue  (rig.Session().GetRunState() == RunState::Stepping);
            Assert::IsTrue  (rig.FinishRun (1000));

            stop = rig.LastStop();
            Assert::AreEqual (std::string ("step"), stop.reason);
            Assert::AreEqual ((Word) 0x0305,        stop.pc, L"the program ends in its JMP loop");

            rig.TypeFails ("t 10000", CommandStatus::Error, "invalid arguments", "10000 is outside $0000-$FFFF.");
            rig.TypeFails ("t 0n65536", CommandStatus::Error, "invalid arguments", "#65536 is outside $0000-$FFFF.");
        }



        //  WinDbg's =addr starts the step there.
        TEST_METHOD (T_FromAnAddress)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.SetPc (0x0312, 0xFD);

            stop = rig.RunToStop ("t =310");
            Assert::AreEqual ((Word) 0x0312, stop.pc, L"one instruction from $0310");
            Assert::AreEqual ((Byte) 0x41,   rig.Registers().a);

            stop = rig.RunToStop ("t = 310 2");
            Assert::AreEqual ((Word) 0x0315, stop.pc, L"two instructions from $0310");
            Assert::AreEqual ((Byte) 0x41,   rig.Peek (0x0400));
        }



        TEST_METHOD (T_Errors)
        {
            WinDbgWindowRig  rig;



            rig.TypeFails ("t zzz",    CommandStatus::Error, "invalid arguments", "zzz is not a symbol, register, or hex number.");
            rig.TypeFails ("t 1 2",    CommandStatus::Error, "invalid arguments", "t takes a count, and =addr to start from: t =300 3.");
            rig.TypeFails ("t =",      CommandStatus::Error, "invalid arguments", "= takes the address to start from, as =300.");
            rig.TypeFails ("t =zzz",   CommandStatus::Error, "invalid arguments", "zzz is not a symbol, register, or hex number.");
            rig.TypeFails ("t =10000", CommandStatus::Error, "invalid arguments", "10000 is outside $0000-$FFFF.");

            Assert::AreEqual ((Word) 0x0300, rig.Registers().pc, L"no failed step ran anything");
            Assert::IsTrue   (rig.GetStops().empty());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgStepOverTests
    //
    //  p and p =addr. p N is reported with the defects: the count is ignored.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgStepOverTests)
    {
    public:

        TEST_METHOD (P_OverAJsr_RunsTheWholeCall_OnTheWindowPath)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop = rig.RunToStop ("p");



            Assert::AreEqual (std::string ("step"), stop.reason);
            Assert::AreEqual ((Word) 0x0303,        stop.pc, L"the instruction after the JSR");
            Assert::AreEqual ((Byte) 0xFF,          rig.Registers().sp, L"the stack back at its level");
            Assert::AreEqual ((Byte) 0x42,          rig.Registers().a,  L"the routine ran to its end");
            Assert::AreEqual ((Byte) 0x41,          rig.Peek (0x0400));
        }



        TEST_METHOD (P_OverAJsr_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("p\np\n"));
            Assert::AreEqual (std::string ("Step at $0303\na=42 x=00 y=00 sp=ff pc=0303 nv-bdIzc\n"), rig.Section ("p", 0));
            Assert::AreEqual (std::string ("Step at $0304\na=42 x=00 y=00 sp=ff pc=0304 nv-bdIzc\n"), rig.Section ("p", 1),
                              L"on a NOP, one step");
            Assert::AreEqual ((Byte) 0x41, rig.Peek (0x0400));
        }



        TEST_METHOD (P_OnAnythingButAJsr_StepsOne)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.SetPc (0x0310, 0xFD);

            stop = rig.RunToStop ("p");
            Assert::AreEqual ((Word) 0x0312, stop.pc);
            Assert::AreEqual ((Byte) 0x41,   rig.Registers().a);
            Assert::AreEqual ((Byte) 0x00,   rig.Peek (0x0400), L"the STA has not run");
        }



        //  A breakpoint inside the routine stops the step there.
        TEST_METHOD (P_StopsAtABreakpointInsideTheCall)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("bp 315");

            stop = rig.RunToStop ("p");
            Assert::AreEqual (std::string ("breakpoint"), stop.reason);
            Assert::AreEqual ((Word) 0x0315,              stop.pc);
            Assert::IsTrue   (stop.breakpointId == 0);
            Assert::AreEqual ((Byte) 0xFD,                rig.Registers().sp, L"still inside the call");
        }



        TEST_METHOD (P_FromAnAddress)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.SetPc (0x0304);

            stop = rig.RunToStop ("p =300");
            Assert::AreEqual ((Word) 0x0303, stop.pc, L"over the JSR at $0300");
            Assert::AreEqual ((Byte) 0xFF,   rig.Registers().sp);
            Assert::AreEqual ((Byte) 0x42,   rig.Registers().a);
        }



        TEST_METHOD (P_Errors)
        {
            WinDbgWindowRig  rig;



            rig.TypeFails ("p 1 2",  CommandStatus::Error, "invalid arguments", "p takes a count, and =addr to start from: p =300 3.");
            rig.TypeFails ("p zzz",  CommandStatus::Error, "invalid arguments", "zzz is not a symbol, register, or hex number.");
            Assert::AreEqual ((Word) 0x0300, rig.Registers().pc);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgStepOutTests
    //
    //  gu. With the debugger attached the call is recorded, and gu ends when
    //  the innermost recorded frame is left; with no frame recorded it ends
    //  when the stack rises past where it started after a return.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgStepOutTests)
    {
    public:

        //  Stepped into the routine at its first instruction, so three more
        //  run before its return: LDA, STA, LDA.
        TEST_METHOD (Gu_LeavesTheRecordedCall_OnTheWindowPath)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.RunToStop ("t");

            stop = rig.RunToStop ("gu");
            Assert::AreEqual (std::string ("step"), stop.reason);
            Assert::AreEqual ((Word) 0x0303,        stop.pc);
            Assert::AreEqual ((Byte) 0xFF,          rig.Registers().sp);
            Assert::AreEqual ((Byte) 0x42,          rig.Registers().a, L"the rest of the routine ran");
            Assert::AreEqual ((Byte) 0x41,          rig.Peek (0x0400));
        }



        TEST_METHOD (Gu_LeavesTheRecordedCall_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("t\ngu\n"));
            Assert::AreEqual (std::string ("Step at $0303\na=42 x=00 y=00 sp=ff pc=0303 nv-bdIzc\n"), rig.Section ("gu"));
        }



        //  A routine the record never saw entered -- the PC put inside it by
        //  hand, its return address on the stack -- ends on the stack alone.
        TEST_METHOD (Gu_WithNoRecordedFrame_EndsOnTheStack)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.Load (0x01FE, { 0x02, 0x03 });
            rig.SetPc (0x0312, 0xFD);

            stop = rig.RunToStop ("gu");
            Assert::AreEqual ((Word) 0x0303, stop.pc);
            Assert::AreEqual ((Byte) 0xFF,   rig.Registers().sp);
            Assert::AreEqual ((Byte) 0x42,   rig.Registers().a);
        }



        TEST_METHOD (Gu_TakesNoArguments)
        {
            WinDbgWindowRig  rig;



            rig.RunToStop ("t");
            rig.TypeFails ("gu 5", CommandStatus::Error, "invalid arguments", "gu takes no arguments.");
            Assert::AreEqual ((Word) 0x0310, rig.Registers().pc, L"nothing ran");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgGoTests
    //
    //  g, g addr, g =addr: a run until a breakpoint, a watchpoint, the
    //  address, a pause or the budget.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgGoTests)
    {
    public:

        TEST_METHOD (G_RunsToABreakpoint_OnTheWindowPath)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("bp 312");

            stop = rig.RunToStop ("g");
            Assert::AreEqual (std::string ("breakpoint"), stop.reason);
            Assert::AreEqual ((Word) 0x0312,              stop.pc);
            Assert::IsTrue   (stop.breakpointId == 0);
            Assert::AreEqual ((Byte) 0x41,                rig.Registers().a);
            Assert::AreEqual ((Byte) 0x00,                rig.Peek (0x0400), L"stopped before the STA");
        }



        TEST_METHOD (G_RunsToABreakpoint_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("bp 312\ng\n"));
            Assert::AreEqual (std::string ("Breakpoint #0 at $0312\na=41 x=00 y=00 sp=fd pc=0312 nv-bdIzc\n"), rig.Section ("g"));
        }



        //  With nothing to stop it the run goes on until the user pauses it,
        //  which `!pause` asks for.
        TEST_METHOD (G_WithNothingToStopIt_RunsUntilPaused)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("g");
            Assert::IsFalse (rig.FinishRun (4), L"the program spins at $0305");
            Assert::IsTrue  (rig.Session().GetRunState() == RunState::DebugRun);

            rig.TypeOk ("!pause");
            Assert::IsTrue (rig.FinishRun (1));

            stop = rig.LastStop();
            Assert::AreEqual (std::string ("pause"), stop.reason);
            Assert::AreEqual ((Word) 0x0305,         stop.pc);
        }



        TEST_METHOD (G_EndsOnTheBudget)
        {
            WinDbgWindowRig          rig;
            WinDbgBatchRig           batch;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("!budget 100");

            stop = rig.RunToStop ("g");
            Assert::AreEqual (std::string ("budget"), stop.reason);
            Assert::AreEqual ((Word) 0x0305,          stop.pc);

            batch.options.maxCycles = 100;
            Assert::AreEqual (3, batch.Run ("g\n"), L"batch exits 3 on a budget stop");
            Assert::IsTrue   (batch.Section ("g").starts_with ("Budget at $0305\na=42 x=00 y=00 sp=ff pc=0305 "), std::wstring (batch.result.output.begin(), batch.result.output.end()).c_str());
        }



        TEST_METHOD (G_ToAnAddress_StopsBeforeIt)
        {
            static constexpr const char * kForms[] = { "g 304", "g 0x304", "g $304", "G 304", "g 0n772" };
            size_t                        checked  = 0;



            for (const char * line : kForms)
            {
                WinDbgWindowRig          rig;
                WinDbgWindowRig::Stop    stop = rig.RunToStop (line);
                std::wstring             where (line, line + strlen (line));



                Assert::AreEqual (std::string ("runTo"), stop.reason, where.c_str());
                Assert::AreEqual ((Word) 0x0304,         stop.pc,     where.c_str());
                Assert::AreEqual ((Byte) 0x42,           rig.Registers().a, where.c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (kForms), checked);
        }



        TEST_METHOD (G_ToAnAddress_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("g 304\n"));
            Assert::AreEqual (std::string ("Run to at $0304\na=42 x=00 y=00 sp=ff pc=0304 nv-bdIzc\n"), rig.Section ("g 304"));
        }



        TEST_METHOD (G_ToAnAddress_StopsEarlierAtABreakpoint)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("bp 312");

            stop = rig.RunToStop ("g 304");
            Assert::AreEqual (std::string ("breakpoint"), stop.reason);
            Assert::AreEqual ((Word) 0x0312,              stop.pc);
        }



        //  WinDbg's g stops at any of several addresses; a Casso run cannot,
        //  so a second address is refused rather than read as a skip range.
        TEST_METHOD (G_WithTwoAddresses_IsAnErrorAndRunsNothing)
        {
            WinDbgWindowRig  rig;



            rig.TypeFails ("g 310 320", CommandStatus::Error, "invalid arguments", "g takes one address to stop at.");
            Assert::IsFalse  (rig.rig.controller.GetRunDriver().IsRunning());
            Assert::AreEqual ((Word) 0x0300, rig.Registers().pc);
        }



        //  g =addr puts the PC there and runs.
        TEST_METHOD (G_FromAnAddress)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.TypeOk ("bp 318");
            rig.SetPc (0x0304, 0xFD);

            stop = rig.RunToStop ("g =315");
            Assert::AreEqual (std::string ("breakpoint"), stop.reason);
            Assert::AreEqual ((Word) 0x0318,              stop.pc);
            Assert::AreEqual ((Byte) 0x42,                rig.Registers().a, L"the LDA at $0315 ran");
            Assert::AreEqual ((Byte) 0x00,                rig.Peek (0x0400), L"the STA before it did not");

            rig.TypeFails ("g =300 310", CommandStatus::NotAvailable, "command not available",
                           "g =addr with an address to stop at is not available yet. Use g =addr, or r pc=addr and then g addr.");
        }



        //  A run command while a run is in progress changes nothing, and a
        //  change to memory or the registers is refused under a run.
        TEST_METHOD (G_WhileRunning_OtherRunsAndWritesAreRefused)
        {
            static constexpr const char * kRuns[]   = { "g", "g 304", "t", "p", "gu", "pa 304", "ta 304", "!gg" };
            static constexpr const char * kWrites[] = { "eb 400 99", "ew 400 1234", "ea 400 \"X\"", "f 400 l4 99", "m 300 l4 400", "r a=99" };
            WinDbgWindowRig  rig;



            rig.TypeOk ("g");

            for (const char * line : kRuns)
            {
                rig.TypeFails (line, CommandStatus::Error, "already running", "A run is in progress. Use PAUSE to stop it.");
            }

            for (const char * line : kWrites)
            {
                std::string  name (line, strchr (line, ' '));



                rig.TypeFails (line, CommandStatus::Error, "machine running", name + " changes registers or memory. Pause the machine first.");
            }

            Assert::AreNotEqual ((Byte) 0x99, rig.Peek (0x0400), L"no write landed");
            Assert::AreNotEqual ((Byte) 0x99, rig.Registers().a);
        }



        //  A run on a machine the user resumed adopts it.
        TEST_METHOD (G_OnAFreeRunningMachine_AdoptsIt)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.rig.cpuManager.SetPaused (false);
            rig.Session().OnUserResumed();
            Assert::IsTrue (rig.Session().GetRunState() == RunState::FreeRunning);

            stop = rig.RunToStop ("g 304");
            Assert::AreEqual (std::string ("runTo"), stop.reason);
            Assert::AreEqual ((Word) 0x0304,         stop.pc);
            Assert::IsTrue   (rig.rig.cpuManager.IsPaused(), L"the stop pauses the machine");
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        //  !gg is g at full speed; the speed chosen comes back at the stop.
        TEST_METHOD (Gg_RunsAtFullSpeed_AndPutsTheSpeedBack)
        {
            WinDbgWindowRig          rig;
            WinDbgWindowRig::Stop    stop;



            rig.rig.cpuManager.SetSpeedMode (SpeedMode::Double);

            rig.TypeOk ("!gg 304");
            Assert::IsTrue (rig.rig.cpuManager.GetSpeedMode() == SpeedMode::Maximum, L"the run takes full speed");
            Assert::IsTrue (rig.FinishRun());

            stop = rig.LastStop();
            Assert::AreEqual (std::string ("runTo"), stop.reason);
            Assert::AreEqual ((Word) 0x0304,         stop.pc);
            Assert::IsTrue   (rig.rig.cpuManager.GetSpeedMode() == SpeedMode::Double, L"and gives it back");
        }



        //  The branch record follows an ordinary run, with no stop condition
        //  set, and a run on the free-running machine.
        TEST_METHOD (Lbr_ReportsTheLastBranchOfAnOrdinaryRun)
        {
            WinDbgWindowRig  rig;



            rig.RunToStop ("g 304");
            Assert::AreEqual (std::string ("Last branch at $0318"), rig.TypeOk ("!lbr").text.at (0), L"the RTS");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgRunToTests
    //
    //  pa addr and ta addr: both run to the address, even inside a call.
    //  WinDbg's distinction between stepping over and tracing calls on the
    //  way is not modeled.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgRunToTests)
    {
    public:

        TEST_METHOD (PaAndTa_RunToTheAddress_InsideTheCall)
        {
            for (const char * line : { "pa 315", "ta 315" })
            {
                WinDbgWindowRig          rig;
                WinDbgWindowRig::Stop    stop = rig.RunToStop (line);
                std::wstring             where (line, line + strlen (line));



                Assert::AreEqual (std::string ("runTo"), stop.reason, where.c_str());
                Assert::AreEqual ((Word) 0x0315,         stop.pc,     where.c_str());
                Assert::AreEqual ((Byte) 0xFD,           rig.Registers().sp, where.c_str());
                Assert::AreEqual ((Byte) 0x41,           rig.Peek (0x0400),  where.c_str());
            }
        }



        TEST_METHOD (PaAndTa_InBatch)
        {
            WinDbgBatchRig  rig;



            Assert::AreEqual (0, rig.Run ("pa 312\nta 318\n"));
            Assert::AreEqual (std::string ("Run to at $0312\na=41 x=00 y=00 sp=fd pc=0312 nv-bdIzc\n"), rig.Section ("pa 312"));
            Assert::AreEqual (std::string ("Run to at $0318\na=42 x=00 y=00 sp=fd pc=0318 nv-bdIzc\n"), rig.Section ("ta 318"));
        }



        TEST_METHOD (PaAndTa_TakeOneAddress)
        {
            WinDbgWindowRig  rig;



            for (const char * name : { "pa", "ta" })
            {
                std::string  word (name);
                std::string  detail = word + " takes the address to stop at.";



                rig.TypeFails (word,              CommandStatus::Error, "invalid arguments", detail);
                rig.TypeFails (word + " 300 310", CommandStatus::Error, "invalid arguments", detail);
                rig.TypeFails (word + " =300",    CommandStatus::Error, "invalid arguments", detail);
            }

            rig.TypeFails ("pa zzz", CommandStatus::Error, "invalid arguments", "zzz is not a symbol, register, or hex number.");
            Assert::IsFalse (rig.rig.controller.GetRunDriver().IsRunning());
        }
    };
}
