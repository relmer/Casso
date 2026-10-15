#include "Pch.h"

#include "Debugger/BreakpointTable.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointTableTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BreakpointTableTests)
    {
    public:

        static constexpr Byte  kNop = 0xEA;
        static constexpr Byte  kLda = 0xA9;
        static constexpr Byte  kBrk = 0x00;



        static Expression Parse (const std::string & text)
        {
            Expression   expression;
            std::string  error;



            Assert::AreEqual (S_OK, DebugExpressionEvaluator::Parse (text, expression, error));
            return expression;
        }



        TEST_METHOD (AddressAndRange_Match)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId  = -1;



            table.AddAddress (0x0300, 0x0300);
            table.AddAddress (0x0400, 0x040F);

            Assert::IsTrue   (table.IsAddressHit (0x0300));
            Assert::IsTrue   (table.IsAddressHit (0x0405));
            Assert::IsFalse  (table.IsAddressHit (0x0410));
            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x0405, kNop, context, hitId));
            Assert::AreEqual (1, hitId);
            Assert::IsFalse  (table.TryMatchBeforeInstruction (0x0301, kNop, context, hitId));
        }



        TEST_METHOD (Ids_SharedCounter_NotReused)
        {
            int              nextId = 5;
            BreakpointTable  table (nextId);
            int              first  = 0;
            int              second = 0;
            int              third  = 0;



            first  = table.AddAddress (0x0300, 0x0300);
            second = table.AddOpcode  (kLda);

            Assert::IsTrue (table.TryClear (first));
            Assert::IsFalse (table.TryClear (first));

            third = table.AddBrk();

            Assert::AreEqual (5, first);
            Assert::AreEqual (6, second);
            Assert::AreEqual (7, third);
            Assert::AreEqual (8, nextId);
        }



        TEST_METHOD (EnableDisable_UpdatesBitmap)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    id     = table.AddAddress (0x0300, 0x0300);
            int                    hitId  = -1;



            Assert::IsTrue  (table.TrySetEnabled (id, false));
            Assert::IsFalse (table.IsAddressHit (0x0300));
            Assert::IsFalse (table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId));
            Assert::IsFalse (table.HasEnabledStopCondition());

            Assert::IsTrue  (table.TrySetEnabled (id, true));
            Assert::IsTrue  (table.IsAddressHit (0x0300));
            Assert::IsTrue  (table.HasEnabledStopCondition());
            Assert::IsFalse (table.TrySetEnabled (99, true));
        }



        TEST_METHOD (HitCount_Increments)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId  = -1;



            table.AddAddress (0x0300, 0x0300);
            table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId);
            table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId);

            Assert::AreEqual ((uint32_t) 2, table.GetAll()[0].hits);
        }



        TEST_METHOD (OpcodeBrkAndCondition_Match)
        {
            int                    nextId  = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    opcodeId = table.AddOpcode (kLda);
            int                    brkId    = table.AddBrk();
            int                    condId   = table.AddCondition (Parse ("A=0"));
            int                    hitId    = -1;



            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x1234, kLda, context, hitId));
            Assert::AreEqual (opcodeId, hitId);

            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x1234, kBrk, context, hitId));
            Assert::AreEqual (brkId, hitId);

            Assert::IsFalse  (table.TryMatchBeforeInstruction (0x1234, kNop, context, hitId));

            context.registers["A"] = 0;
            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x1234, kNop, context, hitId));
            Assert::AreEqual (condId, hitId);
        }



        TEST_METHOD (AddressChecked_BeforeOpcode)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId  = -1;



            table.AddOpcode (kLda);
            table.AddAddress (0x0300, 0x0300);

            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x0300, kLda, context, hitId));
            Assert::AreEqual (1, hitId);
        }



        TEST_METHOD (Interrupt_NeverMatchesBeforeInstruction)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId  = -1;



            table.AddInterrupt();

            Assert::IsTrue   (table.HasEnabledStopCondition());
            Assert::IsFalse  (table.TryMatchBeforeInstruction (0xC000, kNop, context, hitId));
            Assert::AreEqual ((size_t) 1, table.GetAll().size());
        }



        TEST_METHOD (CountsOnlyEntriesAfterAStop_StillCount)
        {
            int                    nextId  = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId   = -1;
            int                    stopper = table.AddAddress (0x0300, 0x0300);
            int                    counter = table.AddAddress (0x0300, 0x0300);
            int                    opcode  = table.AddOpcode (kNop);



            table.TrySetFlags (counter, false, false);
            table.TrySetFlags (opcode,  false, false);

            Assert::IsTrue   (table.TryMatchBeforeInstruction (0x0300, kNop, context, hitId));
            Assert::AreEqual (stopper, hitId);
            Assert::AreEqual ((uint32_t) 1, table.GetAll()[1].hits, L"the counts-only address entry");
            Assert::AreEqual ((uint32_t) 1, table.GetAll()[2].hits, L"the counts-only opcode entry");
        }



        TEST_METHOD (UnreadableOpcode_MatchesNoBrkEntry)
        {
            int                    nextId = 0;
            BreakpointTable        table (nextId);
            MockExpressionContext  context;
            int                    hitId  = -1;



            table.AddBrk();

            Assert::IsFalse  (table.TryMatchBeforeInstruction (0xC000, std::nullopt, context, hitId));
            Assert::AreEqual ((uint32_t) 0, table.GetAll()[0].hits);
        }



        TEST_METHOD (ClearAll_EmptiesBitmap)
        {
            int              nextId = 0;
            BreakpointTable  table (nextId);



            table.AddAddress (0x0000, 0xFFFF);
            table.ClearAll();

            Assert::IsFalse (table.IsAddressHit (0x8000));
            Assert::IsFalse (table.HasEnabledStopCondition());
        }



        static DiskBreakSpec MakeRead (Byte sector)
        {
            DiskBreakSpec  spec;



            spec.event  = DiskEvent::SectorRead;
            spec.track  = 0x11;
            spec.sector = sector;
            return spec;
        }



        //  Every disk entry the event matches counts the hit; the lowest id
        //  of the ones that stop is reported, and a count-only entry counts
        //  and does not stop.
        TEST_METHOD (DiskEntries_CountEveryMatch_AndReportTheLowestStoppingId)
        {
            int                     nextId  = 0;
            BreakpointTable         table (nextId);
            MockExpressionContext   context;
            int                     hitId   = -1;
            std::optional<int32_t>  value;
            int                     counts  = 0;
            int                     first   = 0;
            int                     second  = 0;
            int                     other   = 0;
            auto                    isZero  = [] (const Breakpoint & entry) { return entry.disk->sector == 0; };



            counts = table.AddDisk (MakeRead (0));
            first  = table.AddDisk (MakeRead (0));
            second = table.AddDisk (MakeRead (0));
            other  = table.AddDisk (MakeRead (1));
            Assert::IsTrue (table.TrySetFlags (counts, false, false));

            Assert::IsTrue   (table.TryMatchDisk (isZero, context, hitId, value));
            Assert::AreEqual (first, hitId, L"the lowest id that stops");

            for (const Breakpoint & entry : table.GetAll())
            {
                Assert::AreEqual (entry.id == other ? 0u : 1u, entry.hits, L"each match counted once");
            }

            Assert::IsTrue (table.IsDiskStop (isZero, context));
            Assert::AreEqual (1u, table.GetAll()[1].hits, L"a replay counts nothing");
            (void) second;
        }

        TEST_METHOD (DiskEntries_AFalseConditionNeitherStopsNorCounts)
        {
            int                     nextId = 0;
            BreakpointTable         table (nextId);
            MockExpressionContext   context;
            int                     hitId  = -1;
            std::optional<int32_t>  value;



            table.AddDisk (MakeRead (0), Parse ("1 == 2"));

            Assert::IsFalse  (table.TryMatchDisk ([] (const Breakpoint &) { return true; }, context, hitId, value));
            Assert::AreEqual (0u, table.GetAll()[0].hits);
        }

        //  A copy, an adoption and a find have the spec; an RWTS
        //  breakpoint takes the routine its vector leads to, and no other
        //  disk kind does.
        TEST_METHOD (DiskEntries_KeepTheirSpec_AndOnlyCallsResolve)
        {
            int              nextId = 0;
            BreakpointTable  table (nextId);
            DiskBreakSpec    rwts;
            Breakpoint       found;
            int              read   = 0;
            int              call   = 0;
            int              copy   = 0;



            rwts.event   = DiskEvent::RwtsCall;
            rwts.command = 1;

            read = table.AddDisk (MakeRead (5));
            call = table.AddDisk (rwts);

            Assert::IsTrue  (table.TrySetResolved (call, (Word) 0xBD00));
            Assert::IsFalse (table.TrySetResolved (read, (Word) 0xBD00), L"a sector read has no routine");

            Assert::IsTrue (table.TryFind (call, found));
            Assert::IsTrue (found.disk == rwts && found.resolved == (Word) 0xBD00);

            copy = table.AddCopy (found);
            Assert::IsTrue (table.TryFind (copy, found));
            Assert::IsTrue (found.disk == rwts, L"a copy has the spec");

            Assert::IsTrue  (table.TryClear (read));
            found.id = read;
            found.disk = MakeRead (5);
            Assert::IsTrue  (table.TryAdopt (found));
            Assert::IsTrue  (table.TryFind (read, found));
            Assert::IsTrue  (found.disk == MakeRead (5), L"an adopted entry has the spec");
        }
    };
}
