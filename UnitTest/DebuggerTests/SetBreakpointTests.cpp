#include "Pch.h"

#include "BreakpointRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace BreakpointRigs;





////////////////////////////////////////////////////////////////////////////////
//
//  SetBreakpointTests
//
//  BP and BPX: every form in every dialect, the breakpoint each sets stopping
//  a real run before the instruction at its address, on the batch path and
//  on the window's and the pipe's, for G, GG and the steps; ranges, the
//  boundaries of the address space, IF clauses, the leading-operator form,
//  and every error, with the table left empty after one.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SetBreakpointTests)
    {
    public:

        struct Form
        {
            CommandMode   mode;
            const char  * line;
            const char  * reply;
            const char  * go;
        };

        static constexpr const char * kSet    = "Breakpoint #0 set at $0302";
        static constexpr const char * kSetGss = "Breakpoint id=0 set";



        static void AssertAt (BatchPathRig & rig, Word pc, Byte a, Byte x, Byte stored, const wchar_t * where)
        {
            Assert::AreEqual (pc,     rig.GetRegisters().pc, where);
            Assert::AreEqual (a,      rig.GetRegisters().a,  where);
            Assert::AreEqual (x,      rig.GetRegisters().x,  where);
            Assert::AreEqual (stored, rig.Peek (0x0400),     where);
        }



        static void AssertAt (WindowPathRig & rig, Word pc, Byte a, Byte x, Byte stored, const wchar_t * where)
        {
            Assert::AreEqual (pc,     rig.GetRegisters().pc, where);
            Assert::AreEqual (a,      rig.GetRegisters().a,  where);
            Assert::AreEqual (x,      rig.GetRegisters().x,  where);
            Assert::AreEqual (stored, rig.Peek (0x0400),     where);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Forms and what setting one does
        //
        ////////////////////////////////////////////////////////////////////////

        //  Every form sets one breakpoint at $0302, and a run in the same
        //  dialect stops before the store there: PC on it, A loaded, the byte
        //  it stores not yet written, one hit counted.
        TEST_METHOD (EveryForm_InEveryDialect_SetsABreakpointThatStopsARun)
        {
            static constexpr Form  kForms[] =
            {
                { CommandMode::AppleWin,  "BP 302",        kSet,    "G" },
                { CommandMode::AppleWin,  "BPX 302",       kSet,    "G" },
                { CommandMode::AppleWin,  "bp $302",       kSet,    "G" },
                { CommandMode::AppleWin,  "bpx $0302",     kSet,    "G" },
                { CommandMode::AppleWin,  "BP STORE",      kSet,    "G" },
                { CommandMode::AppleWin,  "BP PC+2",       kSet,    "G" },
                { CommandMode::AppleWin,  "BP #770",       kSet,    "G" },
                { CommandMode::Casso,     "BP 302",        kSet,    "G" },
                { CommandMode::Casso,     "BPX 302",       kSet,    "G" },
                { CommandMode::Monitor,   "/BP 302",       kSet,    "G" },
                { CommandMode::Monitor,   "/BPX 302",      kSet,    "G" },
                { CommandMode::GSSquared, "bp 302",        kSetGss, "g" },
                { CommandMode::GSSquared, "BP 302",        kSetGss, "g" },
                { CommandMode::GSSquared, "bp 00/302",     kSetGss, "g" },
                { CommandMode::GSSquared, "BPX 302",       kSetGss, "g" },
                { CommandMode::WinDbg,    "bp 302",        kSet,    "g" },
                { CommandMode::WinDbg,    "bp 0x302",      kSet,    "g" },
                { CommandMode::WinDbg,    "bp 0n770",      kSet,    "g" },
                { CommandMode::WinDbg,    "ba e1 302",     kSet,    "g" },
                { CommandMode::WinDbg,    "!BP 302",       kSet,    "g" },
                { CommandMode::WinDbg,    "!BPX 302",      kSet,    "g" },
            };



            for (const Form & form : kForms)
            {
                BatchPathRig  rig;
                std::string   where = std::string (CommandModeNames::GetName (form.mode)) + ": " + form.line;



                Programs::LoadLine (rig.target);
                rig.RunOk ("SYM STORE = 302");
                rig.EnterMode (form.mode);

                AssertLines ({ form.reply }, rig.RunOk (form.line), where);
                AssertLines (GetStopLines (form.mode, "Breakpoint #0 at $0302", { 0x0302, 0x41, 0x00, 0x00, 0xFF, 0x34 }), rig.RunOk (form.go), where + ", then " + form.go);

                AssertAt    (rig, 0x0302, 0x41, 0x00, 0xA0, Widen (where + ": stopped before the store").c_str());
                AssertLines ({ "#0 enabled  at $0302, hits 1" }, rig.List(), where);
            }
        }



        //  The command, not the test, installs the stop hook, and only the
        //  breakpoint's pages are asked about outside a run; clearing it takes
        //  them out of the filter, and the hook away once nothing is left.
        TEST_METHOD (Set_InstallsTheHookForTheBreakpointsPagesOnly_AndClearRemovesIt)
        {
            BatchPathRig  rig;



            Assert::IsNull (rig.machine.GetDebugHook(), L"no breakpoint, no hook");

            rig.RunOk ("BP 302");
            Assert::IsTrue  (rig.machine.GetDebugHook() == &rig.target.GetRunHook(), L"BP installed the stop hook");
            Assert::IsTrue  (rig.machine.GetDebugHook()->GetFilter().pages[0x03]);
            Assert::IsFalse (rig.machine.GetDebugHook()->GetFilter().pages[0x04]);
            Assert::IsFalse (rig.machine.GetDebugHook()->GetFilter().everyInstruction, L"an address needs no look at every instruction");
            Assert::IsFalse (rig.machine.GetDebugHook()->GetFilter().opcodesStop);

            rig.RunOk ("BP 1234,300");
            Assert::IsTrue  (rig.machine.GetDebugHook()->GetFilter().pages[0x12]);
            Assert::IsTrue  (rig.machine.GetDebugHook()->GetFilter().pages[0x15]);
            Assert::IsFalse (rig.machine.GetDebugHook()->GetFilter().pages[0x16]);

            rig.RunOk ("BPC 1");
            Assert::IsFalse (rig.machine.GetDebugHook()->GetFilter().pages[0x12], L"the cleared range leaves the filter");
            Assert::IsTrue  (rig.machine.GetDebugHook()->GetFilter().pages[0x03], L"the other breakpoint's page stays");

            rig.RunOk ("BPC 0");
            Assert::IsNull  (rig.machine.GetDebugHook(), L"nothing enabled, no hook");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Stopping a run: G, GG and the steps, on every path
        //
        ////////////////////////////////////////////////////////////////////////

        //  Batch G: the stop comes before the instruction, the session is
        //  paused, the hook stays for the next run, and every later run adds
        //  a hit as the loop comes round again.
        TEST_METHOD (G_OnTheBatchPath_StopsBeforeTheAddress_EveryPass)
        {
            BatchPathRig              rig;
            std::vector<std::string>  lines;
            JsonValue                 stop;



            Programs::LoadLoop (rig.target);
            rig.RunOk ("BP 302");

            rig.Expect ("G", { "Breakpoint #0 at $0302" });
            AssertAt (rig, 0x0302, 0x41, 0x00, 0xA0, L"the first pass");
            Assert::IsTrue    (rig.session.GetRunState() == RunState::Paused);
            Assert::IsNotNull (rig.machine.GetDebugHook(), L"the enabled breakpoint keeps the hook");

            rig.Expect ("G", { "Breakpoint #0 at $0302" });
            AssertAt (rig, 0x0302, 0x41, 0x01, 0x41, L"one pass of the loop between the stops, the store run on the way round");
            AssertLines ({ "#0 enabled  at $0302, hits 2" }, rig.List(), "hits accumulate across runs");

            //  As --json prints it.
            rig.sink.SetJson (true);
            lines = rig.RunOk ("G");

            Assert::AreEqual ((size_t) 1, lines.size());
            stop = ParseJson (lines[0]);
            Assert::AreEqual (std::string ("stopped"),    GetString (stop, "type"));
            Assert::AreEqual (std::string ("breakpoint"), GetString (stop, "reason"));
            Assert::AreEqual (0,                          GetInt    (stop, "breakpointId"));
            Assert::AreEqual (0x0302,                     GetInt    (stop, "pc"));
            Assert::AreEqual (0x0302,                     GetInt    (GetObject (stop, "registers"), "pc"));
            Assert::AreEqual (0x41,                       GetInt    (GetObject (stop, "registers"), "a"));
            Assert::AreEqual (0x02,                       GetInt    (GetObject (stop, "registers"), "x"));
            Assert::IsFalse  (HasMember (stop, "condition"), L"an unconditional breakpoint's stop has no condition");
        }



        //  The emulator's path: G returns with the run still to come, the
        //  frame loop runs it, and the slice the breakpoint stopped short ends
        //  it: the CPU manager paused, the session paused, the stop announced
        //  to the channel. From the pipe the stop gives the command that
        //  started the run.
        TEST_METHOD (G_OnTheWindowAndPipePaths_StopsBeforeTheAddress)
        {
            WindowPathRig  rig;
            JsonValue      stop;
            JsonValue      reply;
            size_t         stops = 0;



            Programs::LoadLoop (rig.GetTarget());
            rig.RunOk ("BP 302");

            rig.RunOk ("G");
            Assert::IsTrue  (rig.GetDriver().IsRunning(), L"the run is still to come");
            Assert::IsFalse (rig.rig.cpuManager.IsPaused());
            Assert::IsTrue  (rig.GetSession().GetRunState() == RunState::DebugRun);

            rig.RunSlices();
            Assert::IsFalse (rig.GetDriver().IsRunning());
            Assert::IsTrue  (rig.rig.cpuManager.IsPaused(),                     L"the stop paused the machine");
            Assert::IsTrue  (rig.GetSession().GetRunState() == RunState::Paused);
            Assert::IsNotNull (rig.rig.machine.GetDebugHook(),                  L"the stop hook is back for the enabled breakpoint");

            stop = rig.GetOnlyStop (0, "breakpoint", 0x0302);
            Assert::AreEqual (0,    GetInt (stop, "breakpointId"));
            Assert::AreEqual (0x41, GetInt (GetObject (stop, "registers"), "a"));
            Assert::IsFalse  (HasMember (stop, "causeId"), L"the window's own run has no channel command behind it");
            AssertAt (rig, 0x0302, 0x41, 0x00, 0xA0, L"window: before the store");
            AssertLines ({ "#0 enabled  at $0302, hits 1" }, rig.List(), "window");

            reply = rig.Send ("G");
            Assert::AreEqual (std::string ("ok"), GetString (reply, "status"));
            stops = rig.GetRecords ("stopped").size();

            rig.RunSlices();
            stop = rig.GetOnlyStop (stops, "breakpoint", 0x0302);
            Assert::AreEqual (GetInt (reply, "id"), GetInt (stop, "causeId"), L"the pipe's stop gives the command that caused it");
            Assert::AreEqual (0,    GetInt (stop, "breakpointId"));
            Assert::AreEqual (0x01, GetInt (GetObject (stop, "registers"), "x"));
            AssertAt (rig, 0x0302, 0x41, 0x01, 0x41, L"pipe: one pass later");
            AssertLines ({ "#0 enabled  at $0302, hits 2" }, rig.List(), "pipe");
        }



        //  A breakpoint set from the pipe stops a run the window starts, and
        //  one set from the window stops a run the pipe starts: one session.
        TEST_METHOD (BreakpointFromOneWayIn_StopsARunFromTheOther)
        {
            WindowPathRig  rig;
            JsonValue      reply;



            Programs::LoadLine (rig.GetTarget());

            reply = rig.Send ("BP 305");
            Assert::AreEqual (std::string ("ok"), GetString (reply, "status"));
            AssertLines ({ "Breakpoint #0 set at $0305" }, GetTextLines (reply), "the pipe's reply");
            Assert::AreEqual (std::string ("breakpointSet"), GetString (GetObject (reply, "data"), "kind"));
            Assert::AreEqual (0x0305, GetInt (GetObject (GetObject (reply, "data"), "breakpoint"), "address"));

            rig.Go ("G");
            rig.GetOnlyStop (0, "breakpoint", 0x0305);

            rig.RunOk ("BPC 0");
            rig.RunOk ("BP 306");
            reply = rig.Send ("G");
            rig.RunSlices();
            Assert::AreEqual (GetInt (reply, "id"), GetInt (rig.GetOnlyStop (1, "breakpoint", 0x0306), "causeId"));
        }



        //  GG stops at a breakpoint as G does. On the emulator's path it takes
        //  full speed for the run and gives the operator's speed back when the
        //  breakpoint ends it; the batch path has no speed to change.
        TEST_METHOD (GG_StopsAtABreakpoint_AndGivesTheSpeedBack)
        {
            BatchPathRig   batch;
            WindowPathRig  window;



            Programs::LoadLine (batch.target);
            batch.RunOk ("BP 302");
            batch.Expect ("GG", { "Breakpoint #0 at $0302" });
            AssertAt (batch, 0x0302, 0x41, 0x00, 0xA0, L"batch GG");

            Programs::LoadLine (window.GetTarget());
            window.RunOk ("BP 302");
            window.rig.cpuManager.SetSpeedMode (SpeedMode::Double);

            window.RunOk ("GG");
            Assert::IsTrue (window.rig.cpuManager.GetSpeedMode() == SpeedMode::Maximum, L"the run takes full speed");

            window.RunSlices();
            window.GetOnlyStop (0, "breakpoint", 0x0302);
            AssertAt (window, 0x0302, 0x41, 0x00, 0xA0, L"window GG");
            Assert::IsTrue (window.rig.cpuManager.GetSpeedMode() == SpeedMode::Double, L"the breakpoint's stop gave the speed back");
        }



        //  A breakpoint met part way through a step ends it as a breakpoint:
        //  T n before the count is done, P inside the call it steps over, RTS
        //  before the routine returns. Without the breakpoint each step ends
        //  where it would.
        TEST_METHOD (Steps_OnTheBatchPath_EndAtABreakpointPartWay)
        {
            BatchPathRig  count;
            BatchPathRig  over;
            BatchPathRig  out;



            Programs::LoadLine (count.target);
            count.RunOk ("BP 305");
            count.Expect ("T 5", { "Breakpoint #0 at $0305" });
            AssertAt (count, 0x0305, 0x41, 0x00, 0x41, L"T 5 stopped after two instructions");
            count.Expect ("T 5", { "Step at $0307" });

            Programs::LoadCall (over.target);
            over.RunOk ("BP 312");
            over.Expect ("P", { "Breakpoint #0 at $0312" });
            AssertAt (over, 0x0312, 0x41, 0x00, 0xA0, L"P stopped inside the call");
            Assert::AreEqual ((Byte) 0xFD, over.GetRegisters().sp, L"with the return address on the stack");

            Programs::LoadCall (out.target);
            out.Expect ("T", { "Step at $0310" });
            out.RunOk  ("BP 315");
            out.Expect ("RTS", { "Breakpoint #0 at $0315" });
            AssertAt (out, 0x0315, 0x41, 0x00, 0x41, L"RTS stopped before the return");
            out.RunOk  ("BPC 0");
            out.Expect ("RTS", { "Step at $0303" });
        }



        //  The same on the emulator's path, and in each dialect's words for the
        //  steps: GSSquared's o and r, WinDbg's t, p and gu.
        TEST_METHOD (Steps_OnTheWindowPath_AndInEachDialect_EndAtABreakpointPartWay)
        {
            struct Step
            {
                CommandMode   mode;
                bool          isCall;
                const char  * first;
                const char  * breakpoint;
                const char  * step;
                Word          pc;
            };

            static constexpr Step  kSteps[] =
            {
                { CommandMode::AppleWin,  false, nullptr, "BP 305",  "T 5",  0x0305 },
                { CommandMode::AppleWin,  true,  nullptr, "BP 312",  "P",    0x0312 },
                { CommandMode::AppleWin,  true,  "T",     "BP 315",  "RTS",  0x0315 },
                { CommandMode::GSSquared, true,  nullptr, "bp 312",  "o",    0x0312 },
                { CommandMode::GSSquared, true,  "s",     "bp 315",  "r",    0x0315 },
                { CommandMode::WinDbg,    false, nullptr, "bp 305",  "t 5",  0x0305 },
                { CommandMode::WinDbg,    true,  nullptr, "bp 312",  "p",    0x0312 },
                { CommandMode::WinDbg,    true,  "t",     "bp 315",  "gu",   0x0315 },
            };



            for (const Step & step : kSteps)
            {
                WindowPathRig  rig;
                std::string    where = std::string (CommandModeNames::GetName (step.mode)) + ": " + step.step;



                if (step.isCall)
                {
                    Programs::LoadCall (rig.GetTarget());
                }
                else
                {
                    Programs::LoadLine (rig.GetTarget());
                }

                rig.EnterMode (step.mode);

                if (step.first != nullptr)
                {
                    rig.Go (step.first);
                }

                rig.RunOk (step.breakpoint);
                rig.Go    (step.step);

                Assert::AreEqual (0, GetInt (rig.GetOnlyStop (step.first != nullptr ? 1 : 0, "breakpoint", step.pc), "breakpointId"), Widen (where).c_str());
                Assert::AreEqual (step.pc, rig.GetRegisters().pc, Widen (where).c_str());
            }
        }



        //  A step that would end on a breakpoint's address anyway ends as the
        //  step: the run is complete before the breakpoint is asked. It is
        //  not counted, and a temporary one is not used up. G addr likewise.
        TEST_METHOD (AStepEndingOnABreakpoint_IsReportedAsTheStep_AndDoesNotCount)
        {
            BatchPathRig   batch;
            WindowPathRig  window;



            Programs::LoadLine (batch.target);
            batch.RunOk ("BP 302");
            batch.RunOk ("BPCHANGE 0 T");
            batch.RunOk ("BP 305");

            batch.Expect ("T",     { "Step at $0302" });
            batch.Expect ("G 305", { "Run to at $0305" });
            AssertLines ({ "#0 enabled  at $0302, temporary, hits 0", "#1 enabled  at $0305, hits 0" }, batch.List(), "batch");

            Programs::LoadLine (window.GetTarget());
            window.RunOk ("BP 302");
            window.RunOk ("BPCHANGE 0 T");
            window.RunOk ("BP 305");

            window.Go ("T");
            window.GetOnlyStop (0, "step", 0x0302);
            window.Go ("G 305");
            window.GetOnlyStop (1, "runTo", 0x0305);
            AssertLines ({ "#0 enabled  at $0302, temporary, hits 0", "#1 enabled  at $0305, hits 0" }, window.List(), "window");
        }



        //  The first instruction of a run is never asked about, so G from a
        //  breakpoint's address runs it, and the loop coming round is what
        //  stops.
        TEST_METHOD (G_FromABreakpointsAddress_RunsItAndStopsTheNextTime)
        {
            BatchPathRig  rig;



            Programs::LoadLoop (rig.target);
            rig.RunOk ("BP 300");

            rig.Expect ("G", { "Breakpoint #0 at $0300" });
            AssertAt (rig, 0x0300, 0x41, 0x01, 0x41, L"one pass, then the stop");
            AssertLines ({ "#0 enabled  at $0300, hits 1" }, rig.List(), "the start is not a hit");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Ranges and boundaries
        //
        ////////////////////////////////////////////////////////////////////////

        //  addr,len and addr:last cover every PC between: a run stops at the
        //  first instruction that enters the range, and again at each later
        //  one inside it, and the lists show the range in each format.
        TEST_METHOD (Range_StopsAtEveryInstructionInside)
        {
            static constexpr Form  kForms[] =
            {
                { CommandMode::AppleWin,  "BP 302,4",   "Breakpoint #0 set at $0302-$0305", "G" },
                { CommandMode::AppleWin,  "BP 302:305", "Breakpoint #0 set at $0302-$0305", "G" },
                { CommandMode::AppleWin,  "BPX 302,4",  "Breakpoint #0 set at $0302-$0305", "G" },
                { CommandMode::GSSquared, "bp 302.305", kSetGss,                            "g" },
                { CommandMode::WinDbg,    "ba e4 302",  "Breakpoint #0 set at $0302-$0305", "g" },
            };



            for (const Form & form : kForms)
            {
                BatchPathRig  rig;
                std::string   where = std::string (CommandModeNames::GetName (form.mode)) + ": " + form.line;



                Programs::LoadLine (rig.target);
                rig.EnterMode (form.mode);
                AssertLines ({ form.reply }, rig.RunOk (form.line), where);

                rig.RunOk (form.go);
                Assert::AreEqual ((Word) 0x0302, rig.GetRegisters().pc, Widen (where + ": the first PC in the range").c_str());
                rig.RunOk (form.go);
                Assert::AreEqual ((Word) 0x0305, rig.GetRegisters().pc, Widen (where + ": the next PC in the range").c_str());
                rig.RunOk (form.go);
                Assert::AreEqual ((Word) 0x0307, rig.GetRegisters().pc, Widen (where + ": past the range, to the budget").c_str());
                Assert::IsTrue   (rig.sink.GetLastStopReason() == StopReason::Budget, Widen (where).c_str());

                AssertLines ({ "#0 enabled  at $0302-$0305, hits 2" }, rig.List(), where);
            }

            {
                BatchPathRig  rig;



                rig.RunOk ("BP 302:305");
                rig.EnterMode (CommandMode::GSSquared);
                rig.Expect ("bp", { "Current breakpoints:", "[0] exec 00/0302.0305" });
                rig.EnterMode (CommandMode::AppleWin);
            }

            {
                BatchPathRig  rig;



                rig.RunOk ("BP 302:305");
                rig.EnterMode (CommandMode::WinDbg);
                rig.Expect ("bl", { "0 e 0302-0305 0001 (0001)" });
            }
        }



        //  The ends of the address space are ordinary addresses: a run stops
        //  at $0000 and at $FFFF, and 0:FFFF stops at the next instruction
        //  wherever it is, with every page in the filter.
        TEST_METHOD (Boundaries_StopAtTheEndsOfTheAddressSpace)
        {
            BatchPathRig  zero;
            BatchPathRig  top;
            BatchPathRig  all;
            BatchPathRig  one;



            Load (zero.target, 0x0300, { 0x4C, 0x00, 0x00 }, 0x0300);
            zero.Expect ("BP 0", { "Breakpoint #0 set at $0000" });
            zero.Expect ("G",    { "Breakpoint #0 at $0000" });

            Load (top.target, 0x0300, { 0x4C, 0xFF, 0xFF }, 0x0300);
            top.Expect ("BP FFFF", { "Breakpoint #0 set at $FFFF" });
            top.Expect ("G",       { "Breakpoint #0 at $FFFF" });

            Programs::LoadLine (all.target);
            all.Expect ("BP 0:FFFF", { "Breakpoint #0 set at $0000-$FFFF" });
            Assert::AreEqual ((size_t) DebugHookFilter::kCount, (size_t) std::count (all.machine.GetDebugHook()->GetFilter().pages.begin(),
                                                                                    all.machine.GetDebugHook()->GetFilter().pages.end(), true));
            all.Expect ("G", { "Breakpoint #0 at $0302" });

            Load (one.target, 0x0300, { 0x4C, 0xFF, 0xFF }, 0x0300);
            one.Expect ("BP FFFF,1", { "Breakpoint #0 set at $FFFF" });
            one.Expect ("G",         { "Breakpoint #0 at $FFFF" });
        }



        //  A value past either end of the address space is an error, and
        //  nothing is set.
        TEST_METHOD (Boundaries_OutOfRange_AreErrors)
        {
            BatchPathRig  rig;



            rig.RunFails ("BP 10000", "invalid arguments", "10000 is outside $0000-$FFFF.");
            rig.RunFails ("BP -1",    "invalid arguments", "-1 is outside $0000-$FFFF.");
            rig.RunFails ("BPX 1FFFF", "invalid arguments", "1FFFF is outside $0000-$FFFF.");
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");
            Assert::IsNull (rig.machine.GetDebugHook());
        }



        //  A range that ends before it starts, written that way or wrapping
        //  past $FFFF, could never match: it is an error, and nothing is set.
        TEST_METHOD (ReversedOrWrappingRange_IsAnError)
        {
            BatchPathRig  rig;



            rig.RunFails ("BP 400:300",  "invalid arguments", "$0400-$0300 ends before it starts.");
            rig.RunFails ("BPX 400:300", "invalid arguments", "$0400-$0300 ends before it starts.");
            rig.RunFails ("BP FFF0,20",  "invalid arguments", "$FFF0-$000F ends before it starts.");
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");

            rig.EnterMode (CommandMode::GSSquared);
            rig.RunFails ("bp 400.300", "invalid arguments", "400.300 ends before it starts.");
            rig.EnterMode (CommandMode::AppleWin);
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Errors
        //
        ////////////////////////////////////////////////////////////////////////

        //  Every way to get BP wrong, in every dialect, with the whole message,
        //  and nothing set by any of them.
        TEST_METHOD (Errors_GiveTheWholeMessage_AndSetNothing)
        {
            BatchPathRig  rig;



            rig.RunFails ("BP",        "invalid arguments", "BP needs an address.");
            rig.RunFails ("BPX",       "invalid arguments", "BPX needs an address.");
            rig.RunFails ("bp",        "invalid arguments", "BP needs an address.");
            rig.RunFails ("BP 300,0",  "invalid arguments", "A range length must be at least 1.");
            rig.RunFails ("BP NOSUCH", "invalid arguments", "NOSUCH is not a symbol, register, or hex number.");
            rig.RunFails ("BP 3G",     "invalid arguments", "3G is not a hex number.");
            rig.RunFails ("BP #12A",   "invalid arguments", "#12A is not a decimal number.");
            rig.RunFails ("BP 3+",     "invalid arguments", "The expression is incomplete.");

            //  A left side that is no address and a right side that is a line
            //  number is a source line, which needs a debug file.
            rig.RunFails ("BP NOSUCH:310", "no debug file", "A source breakpoint needs a debug file. Use SYM LOAD to load one.");

            rig.EnterMode (CommandMode::GSSquared);
            rig.RunFails ("bp zz",      "invalid arguments",     "zz is not an address. An address is one to four hex digits.");
            rig.RunFails ("bp 300 400", "invalid arguments",     "BP takes an address or a range, and optionally IF and an expression.");
            rig.RunFails ("bp 12345",   "invalid arguments",     "12345 is not an address. An address is one to four hex digits.");
            rig.RunFails ("bp E1/300",  "command not available", "E1/0300 is in bank E1. Only bank 00 exists on this machine.", CommandStatus::NotAvailable);
            rig.RunFails ("bp 300.zz",  "invalid arguments",     "300.zz is not a range. A range is first.last in hex.");

            rig.EnterMode (CommandMode::WinDbg);
            rig.RunFails ("bp",         "invalid arguments",     "bp takes an address or a source line.");
            rig.RunFails ("ba e0 300",  "invalid arguments",     "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");
            rig.RunFails ("ba e1",      "invalid arguments",     "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");

            rig.EnterMode (CommandMode::Monitor);
            rig.RunFails ("/BP",        "invalid arguments",     "BP needs an address.");

            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");
            Assert::IsNull (rig.machine.GetDebugHook(), L"and no hook");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  IF
        //
        ////////////////////////////////////////////////////////////////////////

        //  An IF is evaluated only at the address: false runs on without a
        //  hit, true stops and says so with the expression's value. X counts
        //  the loop's passes, so the stop is on the third.
        TEST_METHOD (If_StopsOnlyWhenTrue_InEveryDialect)
        {
            struct IfForm
            {
                CommandMode   mode;
                const char  * line;
                const char  * reply;
                const char  * go;
                const char  * condition;
            };

            static constexpr IfForm  kForms[] =
            {
                { CommandMode::AppleWin,  "BP 302 IF X=2",       "Breakpoint #0 set at $0302 if X=2",  "G", "X=2"  },
                { CommandMode::AppleWin,  "bpx 302 if x==2",     "Breakpoint #0 set at $0302 if x==2", "G", "x==2" },
                { CommandMode::Casso,     "BP 302 IF X=2",       "Breakpoint #0 set at $0302 if X=2",  "G", "X=2"  },
                { CommandMode::Monitor,   "/BP 302 IF X=2",      "Breakpoint #0 set at $0302 if X=2",  "G", "X=2"  },
                { CommandMode::GSSquared, "bp 302 if X=2",       kSetGss,                              "g", "X=2"  },
                { CommandMode::WinDbg,    "bp 302 IF X=2",       "Breakpoint #0 set at $0302 if X=2",  "g", "X=2"  },
                { CommandMode::WinDbg,    "ba e1 302 IF X=2",    "Breakpoint #0 set at $0302 if X=2",  "g", "X=2"  },
            };



            for (const IfForm & form : kForms)
            {
                BatchPathRig  rig;
                std::string   where = std::string (CommandModeNames::GetName (form.mode)) + ": " + form.line;



                Programs::LoadLoop (rig.target);
                rig.EnterMode (form.mode);

                AssertLines ({ form.reply }, rig.RunOk (form.line), where);
                AssertLines (GetStopLines (form.mode, std::format ("Breakpoint #0 at $0302, IF {} is $1", form.condition), { 0x0302, 0x41, 0x02, 0x00, 0xFF, 0x34 }),
                             rig.RunOk (form.go), where);
                AssertAt    (rig, 0x0302, 0x41, 0x02, 0x41, Widen (where).c_str());
                AssertLines ({ std::format ("#0 enabled  at $0302 if {}, hits 1", form.condition) }, rig.List(), where + ": the false passes did not count");
            }
        }



        TEST_METHOD (If_OnTheWindowPath_AndWhileStepping)
        {
            WindowPathRig  window;
            BatchPathRig   stepping;
            JsonValue      stop;



            Programs::LoadLoop (window.GetTarget());
            window.RunOk ("BP 302 IF X=2");
            window.Go ("G");

            stop = window.GetOnlyStop (0, "breakpoint", 0x0302);
            Assert::AreEqual (std::string ("X=2"), GetString (stop, "condition"));
            Assert::AreEqual (1,                   GetInt    (stop, "conditionValue"));
            Assert::AreEqual (0x02,                GetInt    (GetObject (stop, "registers"), "x"));

            Programs::LoadLoop (stepping.target);
            stepping.RunOk ("BP 302 IF X=1");
            stepping.Expect ("T 20", { "Breakpoint #0 at $0302, IF X=1 is $1" });
            AssertAt (stepping, 0x0302, 0x41, 0x01, 0x41, L"T 20 ended on the second pass");
        }



        //  An IF is checked as it is set: a name nothing resolves, a read of an
        //  I/O address, ACCESS or VALUE on an execution breakpoint, which has
        //  no access, and a malformed expression are errors, with the whole
        //  message, and set nothing.
        TEST_METHOD (If_IsCheckedWhenSet)
        {
            BatchPathRig  rig;



            rig.RunFails ("BP 302 IF NOSUCH = 1", "invalid condition", "NOSUCH is not a symbol, register, or hex number.");
            rig.RunFails ("BP 302 IF *C000=80",   "invalid condition", "The condition reads $C000, an I/O address. Reading it would change the machine.");
            rig.RunFails ("BP 302 IF *C0FF=80",   "invalid condition", "The condition reads $C0FF, an I/O address. Reading it would change the machine.");
            rig.RunFails ("BP 302 IF ACCESS = 1", "invalid condition", "ACCESS is not a symbol, register, or hex number.");
            rig.RunFails ("BP 302 IF VALUE = 1",  "invalid condition", "VALUE is not a symbol, register, or hex number.");
            rig.RunFails ("BP 302 IF",            "invalid arguments", "IF needs an expression.");
            rig.RunFails ("BP 302 IF (A",         "invalid arguments", "The parentheses do not balance.");
            rig.RunFails ("BP 302 IF A 1",        "invalid arguments", "An operator was expected at column 3.");
            rig.RunFails ("BP 302 IF A=",         "invalid arguments", "The expression is incomplete.");
            rig.RunFails ("BP 302 IF A=1 && X=2", "invalid arguments", "A value was expected at column 6.");

            rig.EnterMode (CommandMode::GSSquared);
            rig.RunFails ("bp 302 if",             "invalid arguments", "IF needs an expression.");
            rig.RunFails ("bp 302 if NOSUCH = 1",  "invalid condition", "NOSUCH is not a symbol, register, or hex number.");

            rig.EnterMode (CommandMode::WinDbg);
            rig.RunFails ("bp 302 IF *C000=80",    "invalid condition", "The condition reads $C000, an I/O address. Reading it would change the machine.");

            rig.EnterMode (CommandMode::AppleWin);
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");

            //  $BFFF is below the I/O page, so reading it is allowed.
            rig.Expect ("BP 302 IF *BFFF=80", { "Breakpoint #0 set at $0302 if *BFFF=80" });
        }



        //  The check at setting time runs the expression against the machine
        //  as it is now, so it looks for names and I/O reads only: a division
        //  by a register that happens to be zero is not an error then, and at
        //  a hit it is only false.
        TEST_METHOD (If_DividingByARegisterThatIsZeroNow_IsAccepted)
        {
            BatchPathRig  rig;



            Programs::LoadLoop (rig.target);
            rig.Expect ("BP 302 IF 10/X > 1", { "Breakpoint #0 set at $0302 if 10/X > 1" });
            rig.Expect ("G",                  { "Breakpoint #0 at $0302, IF 10/X > 1 is $1" });
            AssertAt (rig, 0x0302, 0x41, 0x01, 0x41, L"X=0 divided by zero, which is false; X=1 stopped");
        }



        //  An IF that cannot be evaluated at a hit -- its symbol removed since
        //  it was set -- is false: no stop, no hit, and the run goes on.
        TEST_METHOD (If_ThatCannotBeEvaluatedAtAHit_IsFalse)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk ("SYM PTR = 6");
            rig.Expect ("BP 302 IF *PTR = 0", { "Breakpoint #0 set at $0302 if *PTR = 0" });
            rig.RunOk ("SYM ! PTR");
            rig.RunOk ("BP 307");

            rig.Expect ("G", { "Breakpoint #1 at $0307" });
            AssertLines ({ "#0 enabled  at $0302 if *PTR = 0, hits 0", "#1 enabled  at $0307, hits 1" }, rig.List(), "no hit for #0");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The leading-operator form
        //
        ////////////////////////////////////////////////////////////////////////

        //  BP with an operator first is a condition on PC, asked before every
        //  instruction: each operator stops the run at the first PC it holds
        //  for, and the stop has no IF part.
        TEST_METHOD (LeadingOperator_IsAConditionOnPc_ThatStopsARun)
        {
            struct Condition
            {
                const char  * line;
                const char  * when;
                Word          pc;
            };

            static constexpr Condition  kConditions[] =
            {
                { "BP < FA62",  "PC<FA62", 0x0302 },
                { "BPX < FA62", "PC<FA62", 0x0302 },
                { "BP =305",    "PC=305",  0x0305 },
                { "BP !300",    "PC!=300", 0x0302 },
                { "BP != 300",  "PC!=300", 0x0302 },
                { "BP >= 305",  "PC>=305", 0x0305 },
                { "BP <= 3FF",  "PC<=3FF", 0x0302 },
                { "BP > 304",   "PC>304",  0x0305 },
            };



            for (const Condition & condition : kConditions)
            {
                BatchPathRig  rig;



                Programs::LoadLine (rig.target);
                rig.Expect (condition.line, { std::format ("Breakpoint #0 set when {}", condition.when) });
                Assert::IsTrue (rig.machine.GetDebugHook()->GetFilter().everyInstruction, Widen (condition.line).c_str());

                rig.Expect ("G", { std::format ("Breakpoint #0 at ${:04X}", condition.pc) });
                Assert::AreEqual (condition.pc, rig.GetRegisters().pc, Widen (condition.line).c_str());
                AssertLines ({ std::format ("#0 enabled  when {}, hits 1", condition.when) }, rig.List(), condition.line);
            }
        }



        TEST_METHOD (LeadingOperator_OnTheWindowPath)
        {
            WindowPathRig  rig;
            JsonValue      stop;



            Programs::LoadLine (rig.GetTarget());
            rig.RunOk ("BP >= 305");
            rig.Go ("G");

            stop = rig.GetOnlyStop (0, "breakpoint", 0x0305);
            Assert::AreEqual (0, GetInt (stop, "breakpointId"));
            Assert::IsFalse  (HasMember (stop, "condition"), L"the condition is the breakpoint, not an IF");
        }
    };
}
