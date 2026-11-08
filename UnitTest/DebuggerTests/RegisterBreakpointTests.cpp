#include "Pch.h"

#include "BreakpointRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace BreakpointRigs;





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterBreakpointTests
//
//  BPR: every spelling of the comparison and every register, in every
//  dialect, stopping a real run before the first instruction at which it
//  holds, on the batch path and the window's, while running and while
//  stepping; its errors with their whole messages; and the check at setting
//  time that the first operand is a register and the rest resolves.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (RegisterBreakpointTests)
    {
    public:

        //  Every spelling BPR takes, and every register, each stopping the
        //  straight-line program where it first holds: A is $41 from $0302,
        //  X is 1 from $0306, Y is 1 from $0307, S is $FD inside the call.
        TEST_METHOD (EverySpellingAndRegister_StopsWhereTheComparisonFirstHolds)
        {
            struct Spelling
            {
                const char  * line;
                const char  * when;
                Word          pc;
                bool          isCall;
            };

            static constexpr Spelling  kSpellings[] =
            {
                { "BPR A=41",     "A=41",   0x0302, false },
                { "BPR A = 41",   "A=41",   0x0302, false },
                { "BPR A 41",     "A=41",   0x0302, false },
                { "bpr a=41",     "A=41",   0x0302, false },
                { "BPR A==41",    "A==41",  0x0302, false },
                { "BPR X>0",      "X>0",    0x0306, false },
                { "BPR X >= 1",   "X>=1",   0x0306, false },
                { "BPR X ! 0",    "X!=0",   0x0306, false },
                { "BPR X != 0",   "X!=0",   0x0306, false },
                { "BPR X<=0",     "X<=0",   0x0302, false },
                { "BPR Y<1",      "Y<1",    0x0302, false },
                { "BPR Y = 1",    "Y=1",    0x0307, false },
                { "BPR P=34",     "P=34",   0x0302, false },
                { "BPR PC = 305", "PC=305", 0x0305, false },
                { "BPR S<FE",     "S<FE",   0x0310, true  },
                { "bpr s < fe",   "S<fe",   0x0310, true  },
            };



            for (const Spelling & spelling : kSpellings)
            {
                BatchPathRig  rig;



                if (spelling.isCall)
                {
                    Programs::LoadCall (rig.target);
                }
                else
                {
                    Programs::LoadLine (rig.target);
                }

                rig.Expect (spelling.line, { std::format ("Breakpoint #0 set when {}", spelling.when) });
                rig.Expect ("G",           { std::format ("Breakpoint #0 at ${:04X}", spelling.pc) });
                Assert::AreEqual (spelling.pc, rig.GetRegisters().pc, Widen (spelling.line).c_str());
                AssertLines ({ std::format ("#0 enabled  when {}, hits 1", spelling.when) }, rig.List(), spelling.line);
            }
        }



        //  The same breakpoint from every dialect's marker, stopping a run in
        //  that dialect.
        TEST_METHOD (EveryDialect_SetsTheSameRegisterCondition)
        {
            struct DialectForm
            {
                CommandMode   mode;
                const char  * line;
                const char  * reply;
                const char  * go;
            };

            static constexpr DialectForm  kForms[] =
            {
                { CommandMode::Casso,     "BPR X>0",   "Breakpoint #0 set when X>0", "G" },
                { CommandMode::Monitor,   "/BPR X>0",  "Breakpoint #0 set when X>0", "G" },
                { CommandMode::GSSquared, "BPR X>0",   "Breakpoint id=0 set",        "g" },
                { CommandMode::GSSquared, "bpr X>0",   "Breakpoint id=0 set",        "g" },
                { CommandMode::WinDbg,    "!BPR X>0",  "Breakpoint #0 set when X>0", "g" },
            };



            for (const DialectForm & form : kForms)
            {
                BatchPathRig  rig;
                std::string   where = std::string (CommandModeNames::GetName (form.mode)) + ": " + form.line;



                Programs::LoadLine (rig.target);
                rig.EnterMode (form.mode);

                AssertLines ({ form.reply }, rig.RunOk (form.line), where);
                rig.RunOk (form.go);
                Assert::AreEqual ((Word) 0x0306, rig.GetRegisters().pc, Widen (where).c_str());
                Assert::AreEqual ((Byte) 0x01,   rig.GetRegisters().x,  Widen (where).c_str());
                AssertLines ({ "#0 enabled  when X>0, hits 1" }, rig.List(), where);
            }
        }



        //  The command asks for every instruction to be looked at, even
        //  outside a run, since a register can change anywhere.
        TEST_METHOD (Set_AsksAboutEveryInstruction)
        {
            BatchPathRig  rig;



            rig.RunOk ("BPR A=41");
            Assert::IsNotNull (rig.machine.GetDebugHook());
            Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().everyInstruction);

            rig.RunOk ("BPD 0");
            Assert::IsNull    (rig.machine.GetDebugHook(), L"disabled, nothing is asked");
        }



        //  The window's and the pipe's path, GG, and steps: each stops where
        //  the comparison first holds, the stop naming the breakpoint and no IF.
        TEST_METHOD (StopsARun_OnTheWindowPath_AtFullSpeed_AndWhileStepping)
        {
            WindowPathRig  window;
            BatchPathRig   batch;
            JsonValue      stop;
            JsonValue      reply;



            Programs::LoadLine (window.GetTarget());
            window.RunOk ("BPR X>0");
            window.Go ("GG");

            stop = window.GetOnlyStop (0, "breakpoint", 0x0306);
            Assert::AreEqual (0, GetInt (stop, "breakpointId"));
            Assert::IsFalse  (HasMember (stop, "condition"), L"a register breakpoint's condition is not an IF");
            Assert::AreEqual (0x01, GetInt (GetObject (stop, "registers"), "x"));

            Programs::LoadLoop (window.GetTarget());
            window.RunOk ("BPC 0");
            reply = window.Send ("BPR X=2");
            Assert::AreEqual (std::string ("ok"), GetString (reply, "status"));
            reply = window.Send ("T 20");
            window.RunSlices();
            stop = window.GetOnlyStop (1, "breakpoint", 0x0306);
            Assert::AreEqual (GetInt (reply, "id"), GetInt (stop, "causeId"));
            Assert::AreEqual (0x02, GetInt (GetObject (stop, "registers"), "x"), L"T 20 ended when X reached 2");

            Programs::LoadLoop (batch.target);
            batch.RunOk  ("BPR X=1");
            batch.Expect ("T 20", { "Breakpoint #0 at $0306" });
            Assert::AreEqual ((Byte) 0x01, batch.GetRegisters().x);
        }



        //  A condition that stays true stops again one instruction after each
        //  resume: the resumed instruction is not asked, the next one is. Each
        //  stop is a hit.
        TEST_METHOD (AConditionThatStaysTrue_StopsAfterEveryInstruction)
        {
            BatchPathRig  rig;



            Programs::LoadLoop (rig.target);
            rig.RunOk ("BPR A=41");

            rig.Expect ("G", { "Breakpoint #0 at $0302" });
            rig.Expect ("G", { "Breakpoint #0 at $0305" });
            rig.Expect ("G", { "Breakpoint #0 at $0306" });
            rig.Expect ("G", { "Breakpoint #0 at $0300" });
            AssertLines ({ "#0 enabled  when A=41, hits 4" }, rig.List(), "one hit a stop");
        }



        //  A condition already true where a run starts is not reported there:
        //  the first instruction runs, and the condition is asked from the
        //  next. A single step asks nothing, so it ends as a step.
        TEST_METHOD (TheFirstInstruction_IsNeverAsked)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk ("R A=41");
            rig.RunOk ("BPR A=41");

            rig.Expect ("T", { "Step at $0302" });
            AssertLines ({ "#0 enabled  when A=41, hits 0" }, rig.List(), "the step asked nothing");

            rig.Expect ("G", { "Breakpoint #0 at $0305" });
            AssertLines ({ "#0 enabled  when A=41, hits 1" }, rig.List(), "the run's first instruction was not asked");
        }



        //  Every malformed BPR, with its whole message, sets nothing.
        TEST_METHOD (Errors_GiveTheWholeMessage_AndSetNothing)
        {
            BatchPathRig  rig;



            rig.RunFails ("BPR",         "invalid arguments", "BPR needs a register and a value.");
            rig.RunFails ("BPR A",       "invalid arguments", "A condition needs a value.");
            rig.RunFails ("BPR A=",      "invalid arguments", "A condition needs a value.");
            rig.RunFails ("BPR A =",     "invalid arguments", "A condition needs a value.");
            rig.RunFails ("BPR A B = 1", "invalid arguments", "BPR needs a register and a value.");
            rig.RunFails ("BPR A=(1",    "invalid arguments", "The parentheses do not balance.");
            rig.RunFails ("BPR A=1 IF X=2", "invalid arguments", "1IFX is not a hex number.");

            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");
            Assert::IsNull (rig.machine.GetDebugHook());
        }



        //  The first operand has to be a register: a symbol or a number there
        //  could never be what BPR compares. The value is checked as an IF
        //  is, so a name nothing resolves or a read of an I/O address is an
        //  error now rather than a breakpoint that never stops. An IF after
        //  the leading-operator form or BPR is joined into the value and
        //  caught the same way.
        TEST_METHOD (TheRegisterAndTheValue_AreCheckedWhenSet)
        {
            BatchPathRig  rig;



            rig.RunFails ("BPR Q=1",           "invalid arguments", "Q is not a register. BPR compares A, X, Y, P, S, or PC.");
            rig.RunFails ("BPR B=1",           "invalid arguments", "B is not a register. BPR compares A, X, Y, P, S, or PC.");
            rig.RunFails ("BPR AX=1",          "invalid arguments", "AX is not a register. BPR compares A, X, Y, P, S, or PC.");
            rig.RunFails ("BPR A=*C000",       "invalid condition", "The condition reads $C000, an I/O address. Reading it would change the machine.");
            rig.RunFails ("BPR A=NOSUCH",      "invalid condition", "NOSUCH is not a symbol, register, or hex number.");
            rig.RunFails ("BPR A=C0 IF X=2",   "invalid condition", "C0IFX is not a symbol, register, or hex number.");
            rig.RunFails ("BP < FA62 IF A=1",  "invalid condition", "FA62IFA is not a symbol, register, or hex number.");
            rig.RunFails ("BP < NOSUCH",       "invalid condition", "NOSUCH is not a symbol, register, or hex number.");
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");

            rig.RunOk    ("BPR A=41");
            rig.RunFails ("BPEDIT 0 BPR Q=1",    "invalid arguments", "Q is not a register. BPR compares A, X, Y, P, S, or PC.");
            rig.RunFails ("BPEDIT 0 BPR A=*C010", "invalid condition", "The condition reads $C010, an I/O address. Reading it would change the machine.");
            AssertLines ({ "#0 enabled  when A=41, hits 0" }, rig.List(), "a refused edit leaves the entry as it was");
        }
    };
}
