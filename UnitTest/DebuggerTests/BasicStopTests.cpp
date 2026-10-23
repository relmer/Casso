#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/ReplyJson.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/KeystrokeInjector.h"
#include "EmuTests/MachineIdle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicStopTests
//
//  Applesoft's breakpoints, steps and error stop on an enhanced //e running
//  real programs. Each program is typed at the prompt with nothing armed,
//  RUN is typed without its Return, the stops are set, and then the Return
//  goes in and G runs the program under the debugger, with a budget so a
//  stop that never comes ends the run.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BasicStopTests)
    {
    public:

        static constexpr uint64_t  kBootCycles = 20'000'000;
        static constexpr Word      kCurLin     = 0x75;

        struct BasicRig
        {
            MachineHandlerRig<BasicHandlers>  rig { "Apple2eEnhanced", TestMachine::Slots::Empty };
            BreakpointHandlers                breakpoints;

            BasicRig (std::initializer_list<const char *> lines)
            {
                rig.session.AddHandler (&breakpoints);
                MachineIdle::RunUntilIdle (rig.machine, kBootCycles);

                for (const char * line : lines)
                {
                    GuestSession::TypeAndCollect (rig.machine, line);
                }

                Assert::AreEqual ((size_t) 3, KeystrokeInjector::InjectString (rig.machine, "RUN"), L"RUN typed");
                rig.RunOk ("BUDGET 5000000");
            }

            //  The Return after RUN, then the run.
            const StopEvent & Run()
            {
                rig.machine.GetRefs().keyboard->PressKey (KeystrokeInjector::kAppleReturn);
                return Go ("G");
            }

            const StopEvent & Go (const std::string & line)
            {
                size_t  before = rig.sink.stops.size();



                rig.RunOk (line);
                Assert::AreEqual (before + 1, rig.sink.stops.size(), TextEncoding::NarrowToWide (line + ": one stop").c_str());
                return rig.sink.stops.back();
            }

            Word CurrentLine()
            {
                Byte  low  = 0;
                Byte  high = 0;



                rig.target.TryPeek (kCurLin,               low);
                rig.target.TryPeek ((Word) (kCurLin + 1),  high);
                return (Word) (low | (high << 8));
            }

            uint32_t Hits (int id)
            {
                Breakpoint  entry;



                rig.session.GetBreakpoints().TryFind (id, entry);
                return entry.hits;
            }
        };

        static void AssertBasicStop (const StopEvent & stop, StopReason reason, Word line, const wchar_t * what)
        {
            Assert::IsTrue   (stop.reason == reason,                                         what);
            Assert::IsTrue   (stop.basic.has_value() && stop.basic->isBasicStop,             what);
            Assert::AreEqual (line, stop.basic->line.value_or (0xFFFF),                      what);
            Assert::AreEqual ((Word) BasicStopMonitor::kStatement, stop.pc,                  what);
        }


        //  FR-175: a line breakpoint stops as its line begins, once, and not
        //  again at the line's second statement.
        TEST_METHOD (ALineBreakpointStopsAsItsLineBegins)
        {
            BasicRig  basic ({ "10 A = 1", "20 B = 2: C = 3", "30 PRINT A" });



            basic.rig.RunOk ("BPBASIC 20");

            AssertBasicStop (basic.Run(), StopReason::Breakpoint, 20, L"line 20");
            Assert::AreEqual (0, basic.rig.LastStop().breakpointId.value_or (-1), L"the breakpoint's id");
            Assert::AreEqual ((Word) 20, basic.CurrentLine());

            Assert::IsTrue   (basic.Go ("G").reason == StopReason::Budget, L"no second stop in line 20");
            Assert::AreEqual (1u, basic.Hits (0));
        }

        //  FR-174: a statement step goes to each statement in turn, a line
        //  step to the next line.
        TEST_METHOD (StepsGoByStatementAndByLine)
        {
            BasicRig  basic ({ "10 A = 1", "20 B = 2: C = 3", "30 D = 4: E = 5", "40 F = 6" });



            basic.rig.RunOk ("BPBASIC 10");
            AssertBasicStop (basic.Run(),            StopReason::Breakpoint, 10, L"line 10");
            AssertBasicStop (basic.Go ("BASICSTEP"), StopReason::BasicStep,  20, L"B = 2");
            AssertBasicStop (basic.Go ("BASICLINE"), StopReason::BasicStep,  30, L"line 30, past C = 3");
            AssertBasicStop (basic.Go ("BASICSTEP"), StopReason::BasicStep,  30, L"E = 5");
            AssertBasicStop (basic.Go ("BASICSTEP"), StopReason::BasicStep,  40, L"F = 6");
        }

        //  FR-174: step out runs to the statement after the GOSUB's return,
        //  through the rest of the subroutine; with no GOSUB it fails.
        TEST_METHOD (StepOutRunsToTheStatementAfterTheReturn)
        {
            BasicRig  basic ({ "10 GOSUB 100", "20 X = 5", "30 END", "100 Y = 1", "110 Z = 2: RETURN" });



            basic.rig.RunOk ("BPBASIC 100");
            AssertBasicStop (basic.Run(),           StopReason::Breakpoint, 100, L"in the subroutine");
            AssertBasicStop (basic.Go ("BASICOUT"), StopReason::BasicStep,  20,  L"after the return");

            basic.rig.RunFails ("BASICOUT", "cannot step");
        }

        //  FR-175: a variable breakpoint stops at the boundary after the
        //  statement that changed the value, with the values and that
        //  statement's line; assigning the same value, RUN's clearing and
        //  another variable's change stop nothing.
        TEST_METHOD (AVariableBreakpointStopsAfterTheChange)
        {
            BasicRig  basic ({ "10 A = 1", "20 B = 2", "30 A = 1", "40 A = 7: B = 3", "50 END" });
            StopEvent  stop;



            basic.rig.RunOk ("BPBASICVAR A");

            stop = basic.Run();
            AssertBasicStop (stop, StopReason::Breakpoint, 20, L"after line 10");
            Assert::AreEqual (std::string ("A"), stop.basic->variable);
            Assert::AreEqual (std::string ("0"), stop.basic->previous);
            Assert::AreEqual (std::string ("1"), stop.basic->value);
            Assert::AreEqual ((Word) 10, stop.basic->changeLine.value_or (0));

            stop = basic.Go ("G");
            AssertBasicStop (stop, StopReason::Breakpoint, 40, L"after A = 7");
            Assert::AreEqual (std::string ("7"), stop.basic->value);
            Assert::AreEqual ((Word) 40, stop.basic->changeLine.value_or (0));

            Assert::IsTrue (basic.Go ("G").reason == StopReason::Budget, L"no other change");
        }

        //  A variable breakpoint enabled again compares with the value from
        //  then on, never with the one it had before it was disabled.
        TEST_METHOD (AReenabledVariableBreakpointStartsFromTheValueNow)
        {
            BasicRig   basic ({ "10 A = 1", "20 A = 2", "30 A = 3", "40 B = 1", "50 B = 2", "60 END" });
            StopEvent  stop;



            basic.rig.RunOk ("BPBASICVAR A");
            AssertBasicStop (basic.Run(), StopReason::Breakpoint, 20, L"after line 10");

            basic.rig.RunOk ("BPD 0");
            basic.rig.RunOk ("BPBASIC 40");
            AssertBasicStop (basic.Go ("G"), StopReason::Breakpoint, 40, L"A changed twice while disabled");

            basic.rig.RunOk ("BPE 0");
            stop = basic.Go ("G");
            Assert::IsTrue (stop.reason == StopReason::Budget, L"A does not change again");
        }

        //  Array elements and strings, by their first two characters.
        TEST_METHOD (ElementsAndStringsAreWatched)
        {
            BasicRig   basic ({ "10 DIM XY(5)", "20 XY(2) = 9", "30 NAME$ = \"HI\"", "40 END" });
            StopEvent  stop;



            basic.rig.RunOk ("BPBASICVAR XYZ(2)");
            basic.rig.RunOk ("BPBASICVAR NA$");

            stop = basic.Run();
            AssertBasicStop (stop, StopReason::Breakpoint, 30, L"after XY(2) = 9");
            Assert::AreEqual (std::string ("XYZ(2)"), stop.basic->variable);
            Assert::AreEqual (std::string ("9"),      stop.basic->value);

            stop = basic.Go ("G");
            AssertBasicStop (stop, StopReason::Breakpoint, 40, L"after NAME$ = \"HI\"");
            Assert::AreEqual (std::string ("HI"), stop.basic->value);

            basic.rig.RunFails ("BPBASICVAR 2X", "invalid arguments");
            basic.rig.RunFails ("BPBASIC 64000", "invalid arguments");
        }

        //  FR-177: an error stops before ERROR runs, with its code, message
        //  and line, whether ONERR takes it or not.
        TEST_METHOD (AnErrorStopsWithItsCodeAndLine)
        {
            BasicRig   basic ({ "10 ONERR GOTO 100", "20 A = 1 / 0", "30 END", "100 B = 1 / 0" });
            StopEvent  stop;



            basic.rig.RunOk ("BRKBASIC ON");

            stop = basic.Run();
            Assert::IsTrue   (stop.reason == StopReason::BasicError);
            Assert::AreEqual ((Word) BasicStopMonitor::kError, stop.pc);
            Assert::AreEqual ((Word) 20, stop.basic->line.value_or (0));
            Assert::AreEqual ((Byte) 0x85, stop.basic->errorCode.value_or (0));
            Assert::AreEqual (std::string ("DIVISION BY ZERO"), stop.basic->error);
            Assert::IsTrue   (stop.basic->isHandled, L"ONERR is on");
            Assert::AreEqual (std::string ("BASIC error: ?DIVISION BY ZERO ERROR IN 20 (code $85, ONERR on)"), AppleWinFormatter::FormatStop (stop));

            stop = basic.Go ("G");
            Assert::IsTrue   (stop.reason == StopReason::BasicError);
            Assert::AreEqual ((Word) 100, stop.basic->line.value_or (0));

            basic.rig.RunOk ("BRKBASIC OFF");
            Assert::IsTrue (basic.rig.session.GetBreakpoints().GetAll().empty());
        }

        //  The BASIC breakpoints take an IF clause, and their definitions have
        //  it; a line breakpoint whose condition is false does not stop.
        TEST_METHOD (TheBasicBreakpointsTakeIf)
        {
            BasicRig  basic ({ "10 A = 1", "20 GOSUB 100", "30 END", "100 FOR I = 1 TO 3: RETURN" });



            basic.rig.RunOk ("BPBASIC 100 IF A == 1");
            basic.rig.RunOk ("BPBASICVAR D(3,4) IF X == 2");
            basic.rig.RunOk ("BRKBASIC ON IF Y == 3");
            Assert::AreEqual (std::string ("BPC *\nBPBASIC 100 IF A == 1\nBPBASICVAR D(3,4) IF X == 2\nBRKBASIC ON IF Y == 3\n"), BreakpointHandlers::MakeScript (basic.rig.session));

            basic.rig.RunOk ("BPC *");
            basic.rig.RunOk ("BPBASIC 100 IF PC == 0");
            Assert::AreNotEqual ((int) StopReason::Breakpoint, (int) basic.Run().reason, L"a false condition stopped");
        }

        //  BPL's lines, BPSAVE's script, the channel's JSON and the stop text.
        TEST_METHOD (TheBreakpointsListSaveAndReport)
        {
            BasicRig     basic ({ "10 A = 1", "20 GOSUB 100", "30 END", "100 FOR I = 1 TO 3: RETURN" });
            Reply        reply;
            std::string  json;



            basic.rig.RunOk ("BPBASIC 100");
            basic.rig.RunOk ("BPBASICVAR A");
            basic.rig.RunOk ("BRKBASIC ON");

            reply = basic.rig.RunOk ("BPL");
            Assert::AreEqual (std::string ("#0 enabled  on BASIC line 100, hits 0"),           reply.text[0]);
            Assert::AreEqual (std::string ("#1 enabled  on BASIC variable A changing, hits 0"), reply.text[1]);
            Assert::AreEqual (std::string ("#2 enabled  on BASIC error, hits 0"),              reply.text[2]);

            Assert::AreEqual (std::string ("BPC *\nBPBASIC 100\nBPBASICVAR A\nBRKBASIC ON\n"), BreakpointHandlers::MakeScript (basic.rig.session));

            json = ReplyJson::WriteReply (reply, 1);
            Assert::IsTrue (json.find ("\"kind\":\"basicLine\"") != std::string::npos && json.find ("\"line\":100") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());
            Assert::IsTrue (json.find ("\"kind\":\"basicVariable\"") != std::string::npos && json.find ("\"variable\":\"A\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());
            Assert::IsTrue (json.find ("\"kind\":\"basicError\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());

            json = ReplyJson::WriteStopped (basic.Run(), std::nullopt);
            Assert::IsTrue (json.find ("\"reason\":\"breakpoint\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());
            Assert::IsTrue (json.find ("\"variable\":\"A\"") != std::string::npos && json.find ("\"changeLine\":10") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());
            Assert::AreEqual (std::string ("Breakpoint #1: A changed from 0 to 1 in line 10, stopped at line 20"), AppleWinFormatter::FormatStop (basic.rig.LastStop()));

            AssertBasicStop (basic.Go ("G"), StopReason::Breakpoint, 100, L"in the subroutine");
            Assert::AreEqual (std::string ("Breakpoint #0 at BASIC line 100"), AppleWinFormatter::FormatStop (basic.rig.LastStop()));

            reply = basic.rig.RunOk ("BASICVARS");
            Assert::AreEqual (std::string ("A = 1"), reply.text[0]);

            basic.rig.RunOk ("BPBASICVAR I");
            basic.Go ("BASICSTEP");
            reply = basic.rig.RunOk ("BASICSTACK");
            Assert::AreEqual (std::string ("FOR I TO 3 STEP 1 in line 100"), reply.text[0]);
            Assert::AreEqual (std::string ("GOSUB in line 20"),               reply.text[1]);
        }

        //  A machine whose $D000 bank is not Applesoft has nothing to step.
        TEST_METHOD (StepsNeedApplesoft)
        {
            MachineHandlerRig<BasicHandlers>  rig { "Apple2", TestMachine::Slots::Empty };



            rig.RunFails ("BASICSTEP",  "cannot step");
            rig.RunFails ("BASICVARS",  "no Applesoft");
            rig.RunFails ("BASICSTACK", "no Applesoft");
        }
    };
}
