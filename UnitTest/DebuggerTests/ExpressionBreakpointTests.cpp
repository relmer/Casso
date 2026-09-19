#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"
#include "Debugger/BreakpointTable.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "HandlerTestRig.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionBreakpointTests
//
//  IF expressions on BP, BPX, BPM, BPMR and BPMW; the value breakpoint BPMV;
//  and BPR's spacing forms. An expression is evaluated only when its address
//  or access hits, a false one neither stops nor counts, and a true one's
//  stop carries the expression and its value.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ExpressionBreakpointTests)
    {
    public:

        using Rig = HandlerRig<BreakpointHandlers>;

        static constexpr Byte  kNop = 0xEA;



        //  Counts every register read, which every expression below makes
        //  first, so the count is the number of evaluations.
        class CountingContext : public MockExpressionContext
        {
        public:
            mutable int  reads = 0;

            bool TryGetRegister (const std::string & name, Word & value) const override
            {
                ++reads;
                return MockExpressionContext::TryGetRegister (name, value);
            }
        };

        static Expression Parse (const std::string & text)
        {
            Expression   expression;
            std::string  error;



            Assert::AreEqual (S_OK, DebugExpressionEvaluator::Parse (text, expression, error));
            return expression;
        }

        static StopEvent MakeStop (StopReason reason, Word pc)
        {
            StopEvent  stop;



            stop.reason = reason;
            stop.pc     = pc;
            return stop;
        }

        static uint32_t GetHits (Rig & rig, int id)
        {
            Breakpoint  breakpoint;
            Watchpoint  watchpoint;



            if (rig.session.GetBreakpoints().TryFind (id, breakpoint))
            {
                return breakpoint.hits;
            }

            Assert::IsTrue (rig.session.GetWatchpoints().TryFind (id, watchpoint), L"no such entry");
            return watchpoint.hits;
        }



        TEST_METHOD (If_IsEvaluatedOnlyWhenTheAddressHits)
        {
            int              nextId  = 0;
            BreakpointTable  table (nextId);
            CountingContext  context;
            int              hitId   = -1;



            table.AddAddress (0x0300, 0x0300, Parse ("A == 42"));

            for (Word pc = 0x0301; pc < 0x0310; ++pc)
            {
                Assert::IsFalse (table.TryMatchBeforeInstruction (pc, kNop, context, hitId));
            }

            Assert::AreEqual (0, context.reads, L"no evaluation away from the address");

            Assert::IsFalse  (table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId), L"A is $41");
            Assert::AreEqual (1, context.reads);
            Assert::AreEqual (0u, table.GetAll().at (0).hits, L"a false expression does not count");

            context.registers["A"] = 0x42;
            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId));
            Assert::AreEqual (0, hitId);
            Assert::AreEqual (1u, table.GetAll().at (0).hits);
            Assert::IsTrue   (table.GetLastConditionValue() == std::optional<int32_t> (1));
        }



        TEST_METHOD (BP_If_False_KeepsRunning_AndLeavesTheHitCount)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300 if A == 41"), rig.RunOk ("BP 300 IF A == 41").text.at (0));

            rig.target.registers.a = 0x40;
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300));
            Assert::AreEqual (0u, GetHits (rig, 0));
        }



        TEST_METHOD (BP_If_True_StopsAndReportsTheExpressionAndItsValue)
        {
            Rig  rig;



            rig.RunOk ("BP 300 IF A == 41");
            rig.target.registers.a = 0x41;

            Assert::IsTrue (rig.session.ShouldStopBefore (0x0300));
            rig.target.Stop (MakeStop (StopReason::Breakpoint, 0x0300));

            Assert::AreEqual (1u, GetHits (rig, 0));
            Assert::AreEqual (std::string ("A == 41"), rig.sink.stops.at (0).condition);
            Assert::IsTrue   (rig.sink.stops[0].conditionValue == std::optional<int32_t> (1));
            Assert::AreEqual (std::string ("Breakpoint #0 at $0300, IF A == 41 is $1"), AppleWinFormatter::FormatStop (rig.sink.stops[0]));
        }



        TEST_METHOD (BPX_If_ReadsMemoryAndSymbols)
        {
            Rig  rig;



            rig.session.GetSymbols().Add (SymbolTableId::User, "PTR", 0x0006);
            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300 if X == 3 & *PTR != 0"), rig.RunOk ("BPX 300 IF X == 3 & *PTR != 0").text.at (0));

            rig.target.registers.x = 3;
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"$06 holds zero");

            rig.target.memory[0x06] = 0x10;
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0300));
        }



        TEST_METHOD (UnknownSymbol_IsAnError_AndCreatesNothing)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunFails ("BP 300 IF NOSUCH = 1", "invalid condition");
            Assert::IsTrue (reply.error.detail.find ("NOSUCH") != std::string::npos);

            rig.RunFails ("BPMW 400 IF NOSUCH = 1", "invalid condition");
            rig.RunFails ("BP 300 IF ACCESS = 1",   "invalid condition");
            Assert::IsTrue (rig.session.GetBreakpoints().GetAll().empty());
            Assert::IsTrue (rig.session.GetWatchpoints().GetAll().empty());
        }



        TEST_METHOD (IoRead_IsRefusedWhenSet)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunFails ("BP 300 IF *C000 = 80", "invalid condition");
            Assert::IsTrue (reply.error.detail.find ("$C000") != std::string::npos, L"the detail gives the address");

            rig.RunFails ("BPMR 400 IF *C010 = 0", "invalid condition");
            Assert::IsTrue (rig.session.GetBreakpoints().GetAll().empty());
            Assert::IsTrue (rig.session.GetWatchpoints().GetAll().empty());
        }



        TEST_METHOD (IfWithoutAnExpression_IsAnError)
        {
            Rig  rig;



            rig.RunFails ("BP 300 IF", "invalid arguments");
        }



        TEST_METHOD (BPMW_If_Value_StopsOnlyOnAMatchingWrite)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint #0 set on write of $0400 if VALUE = 7"), rig.RunOk ("BPMW 400 IF VALUE = 7").text.at (0));

            rig.session.OnInstruction (0x0310);
            rig.session.GetWatchpoints().OnWatchedAccess (0x0400, 0x06, BusAccess::Write, (Byte) 0x05);
            Assert::IsFalse  (rig.session.HasPendingStop());
            Assert::AreEqual (0u, GetHits (rig, 0));

            rig.session.GetWatchpoints().OnWatchedAccess (0x0400, 0x07, BusAccess::Write, (Byte) 0x06);
            Assert::IsTrue   (rig.session.HasPendingStop());
            Assert::AreEqual (1u, GetHits (rig, 0));

            rig.target.Stop (MakeStop (StopReason::Watchpoint, 0x0313));
            Assert::AreEqual (std::string ("VALUE = 7"), rig.sink.stops.at (0).condition);
            Assert::AreEqual (std::string ("Watchpoint #0: Write $07 to $0400 by $0310 (was $06), IF VALUE = 7 is $1"),
                              AppleWinFormatter::FormatStop (rig.sink.stops[0]));
        }



        TEST_METHOD (BPMR_If_Access_GivesTheAccessedAddress)
        {
            Rig  rig;



            rig.RunOk ("BPMR 400:40F IF ACCESS = 405");

            rig.session.GetWatchpoints().OnWatchedAccess (0x0404, 0x00, BusAccess::Read, std::nullopt);
            Assert::IsFalse (rig.session.HasPendingStop());

            rig.session.GetWatchpoints().OnWatchedAccess (0x0405, 0x00, BusAccess::Read, std::nullopt);
            Assert::IsTrue  (rig.session.HasPendingStop());
            Assert::AreEqual ((Word) 0x0405, rig.session.GetWatchpoints().GetPendingHit()->address);
        }



        TEST_METHOD (BPM_If_ReadsRegistersOnAnAccess)
        {
            Rig  rig;



            rig.RunOk ("BPM 400 IF Y = 2 & VALUE > 80");

            rig.target.registers.y = 2;
            rig.session.GetWatchpoints().OnWatchedAccess (0x0400, 0x80, BusAccess::Read, std::nullopt);
            Assert::IsFalse (rig.session.HasPendingStop());

            rig.session.GetWatchpoints().OnWatchedAccess (0x0400, 0x81, BusAccess::Read, std::nullopt);
            Assert::IsTrue  (rig.session.HasPendingStop());
        }



        TEST_METHOD (BPMV_StopsOnTheWriteThatLeavesTheValue_AndNoOther)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint #0 set when $0006 becomes $07"), rig.RunOk ("BPMV 6 7").text.at (0));
            Assert::IsTrue   (rig.target.watchedPages[0x00], L"the value breakpoint's page is watched");
            Assert::IsTrue   (rig.target.hookInstalled);

            rig.session.OnInstruction (0x0300);
            rig.session.GetWatchpoints().OnWatchedAccess (0x0006, 0x06, BusAccess::Write, (Byte) 0x05);
            rig.session.GetWatchpoints().OnWatchedAccess (0x0007, 0x07, BusAccess::Write, (Byte) 0x00);
            rig.session.GetWatchpoints().OnWatchedAccess (0x0006, 0x07, BusAccess::Read,  std::nullopt);
            Assert::IsFalse  (rig.session.HasPendingStop(), L"another value, another address, a read");
            Assert::AreEqual (0u, GetHits (rig, 0));

            rig.session.GetWatchpoints().OnWatchedAccess (0x0006, 0x07, BusAccess::Write, (Byte) 0x06);
            Assert::IsTrue   (rig.session.HasPendingStop());
            Assert::AreEqual (1u, GetHits (rig, 0));

            rig.target.Stop (MakeStop (StopReason::Watchpoint, 0x0302));
            Assert::AreEqual (0, rig.sink.stops.at (0).watch->id);
            Assert::AreEqual ((Byte) 0x07, rig.sink.stops[0].watch->value);
        }



        TEST_METHOD (BPMV_If_AndItsDefinition)
        {
            Rig                       rig;
            std::vector<std::string>  list;



            rig.RunOk ("BPMV 6 7 IF X = 1");
            rig.RunOk ("BP 300 IF A = 41");

            Assert::AreEqual (std::string ("BPMV 0006 07 IF X = 1"),                BreakpointHandlers::MakeDefinition (BreakpointHandlers::MakeInfo (rig.session.GetBreakpoints().GetAll().at (0))));
            Assert::AreEqual (std::string ("BP 0300 IF A = 41"),                    BreakpointHandlers::MakeDefinition (BreakpointHandlers::MakeInfo (rig.session.GetBreakpoints().GetAll().at (1))));

            rig.target.registers.x = 0;
            rig.session.GetWatchpoints().OnWatchedAccess (0x0006, 0x07, BusAccess::Write, (Byte) 0x06);
            Assert::IsFalse (rig.session.HasPendingStop(), L"X is not 1");

            rig.target.registers.x = 1;
            rig.session.GetWatchpoints().OnWatchedAccess (0x0006, 0x07, BusAccess::Write, (Byte) 0x07);
            Assert::IsTrue  (rig.session.HasPendingStop());

            rig.RunFails ("BPMV 6 100", "invalid arguments");
            rig.RunFails ("BPMV 6",     "invalid arguments");
        }



        TEST_METHOD (BPR_SpacingForms_SetTheSameBreakpoint)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint #0 set when A=0"), rig.RunOk ("BPR A=0").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #1 set when A=0"), rig.RunOk ("BPR A = 0").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #2 set when A=0"), rig.RunOk ("BPR A 0").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #3 set when X<10"), rig.RunOk ("BPR X<10").text.at (0));

            rig.RunFails ("BPR A=",  "invalid arguments");
            rig.RunFails ("BPR A =", "invalid arguments");
            rig.RunFails ("BPR A",   "invalid arguments");
        }
    };
}
