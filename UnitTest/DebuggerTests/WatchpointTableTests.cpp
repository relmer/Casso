#include "Pch.h"

#include "Debugger/WatchpointTable.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointTableTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (WatchpointTableTests)
    {
    public:

        static size_t CountPages (const WatchedPages & pages)
        {
            return (size_t) std::count (pages.begin(), pages.end(), true);
        }



        TEST_METHOD (Mask_CoversExactlyEnabledPages)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);
            MockDebugTarget  target;
            int              ranged = 0;



            table.SetTarget (&target);
            table.Add (WatchAccess::Read, 0xC019, 0xC019);
            ranged = table.Add (WatchAccess::Write, 0x03F0, 0x0410);

            Assert::IsTrue   (target.watchedPages[0xC0]);
            Assert::IsTrue   (target.watchedPages[0x03]);
            Assert::IsTrue   (target.watchedPages[0x04]);
            Assert::AreEqual ((size_t) 3, CountPages (target.watchedPages));

            table.TrySetEnabled (ranged, false);
            Assert::AreEqual ((size_t) 1, CountPages (target.watchedPages));

            table.TrySetEnabled (ranged, true);
            Assert::IsTrue   (table.TryClear (ranged));
            Assert::AreEqual ((size_t) 1, CountPages (target.watchedPages));
            Assert::IsTrue   (target.watchedPages[0xC0]);
        }



        TEST_METHOD (Ids_SharedWithBreakpointCounter)
        {
            int              nextId = 3;
            WatchpointTable  table (nextId);



            Assert::AreEqual (3, table.Add (WatchAccess::Read,  0x0300, 0x0300));
            Assert::AreEqual (4, table.Add (WatchAccess::Write, 0x0300, 0x0300));
            Assert::AreEqual (5, nextId);
        }



        TEST_METHOD (MatchingAccess_RecordsHitAndPendingStop)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);
            int              id     = table.Add (WatchAccess::Read, 0xC019, 0xC019);



            table.SetAccessPc (0x0303);
            table.OnWatchedAccess (0xC019, 0x80, BusAccess::Read, std::nullopt);

            Assert::IsTrue   (table.HasPendingStop());
            Assert::AreEqual (id,              table.GetPendingHit()->id);
            Assert::AreEqual ((Word) 0xC019,   table.GetPendingHit()->address);
            Assert::AreEqual ((Byte) 0x80,     table.GetPendingHit()->value);
            Assert::AreEqual ((Word) 0x0303,   table.GetPendingHit()->accessPc);
            Assert::IsTrue   (table.GetPendingHit()->access == WatchAccess::Read);
            Assert::AreEqual ((uint32_t) 1,    table.GetAll()[0].hits);

            table.ClearPending();
            Assert::IsFalse  (table.HasPendingStop());
        }



        TEST_METHOD (OtherAccessOrOutsideRange_Ignored)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);
            int              id     = table.Add (WatchAccess::Read, 0x0300, 0x030F);



            table.OnWatchedAccess (0x0305, 0x01, BusAccess::Write, std::nullopt);
            table.OnWatchedAccess (0x0310, 0x01, BusAccess::Read,  std::nullopt);
            Assert::IsFalse (table.HasPendingStop());

            table.TrySetEnabled (id, false);
            table.OnWatchedAccess (0x0305, 0x01, BusAccess::Read, std::nullopt);
            Assert::IsFalse (table.HasPendingStop());
        }



        TEST_METHOD (ReadWrite_FirstHitKept_UnlessAWriteFollowsToTheSameAddress)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);



            table.Add (WatchAccess::ReadWrite, 0x0300, 0x030F);

            // A write, then a read elsewhere: the write stays.
            table.OnWatchedAccess (0x0300, 0x11, BusAccess::Write, (Byte) 0x00);
            table.OnWatchedAccess (0x0301, 0x22, BusAccess::Read,  std::nullopt);

            Assert::IsTrue   (table.GetPendingHit()->access == WatchAccess::Write);
            Assert::AreEqual ((Byte) 0x11,  table.GetPendingHit()->value);
            Assert::AreEqual ((uint32_t) 2, table.GetAll()[0].hits);

            // An indexed store: the pre-read of the target, then the write of
            // it. The stop must report the write and the replaced byte.
            table.ClearPending();
            table.OnWatchedAccess (0x0305, 0xA0, BusAccess::Read,  std::nullopt);
            table.OnWatchedAccess (0x0305, 0x41, BusAccess::Write, (Byte) 0xA0);

            Assert::IsTrue   (table.GetPendingHit()->access == WatchAccess::Write);
            Assert::AreEqual ((Byte) 0x41,  table.GetPendingHit()->value);
            Assert::AreEqual ((Byte) 0xA0,  table.GetPendingHit()->previous.value_or (0xFF));

            // A write to a different address does not replace a pending hit.
            table.ClearPending();
            table.OnWatchedAccess (0x0306, 0x01, BusAccess::Write, (Byte) 0x00);
            table.OnWatchedAccess (0x0307, 0x02, BusAccess::Write, (Byte) 0x00);

            Assert::AreEqual ((Word) 0x0306, table.GetPendingHit()->address);
        }



        TEST_METHOD (BeforeMode_MatchesPredictedTouches)
        {
            int               nextId = 0;
            WatchpointTable   table (nextId);
            AccessPrediction  store;
            AccessPrediction  load;
            AccessPrediction  rmw;
            WatchHit          hit;
            int               writeOnly = table.Add (WatchAccess::Write, 0x0400, 0x04FF, WatchMode::Before);
            int               after     = table.Add (WatchAccess::ReadWrite, 0x0400, 0x04FF);



            store.touches = { { 0x0410, PredictedAccess::Write } };
            load.touches  = { { 0x0410, PredictedAccess::Read } };
            rmw.touches   = { { 0x0410, PredictedAccess::ReadWrite } };

            Assert::IsTrue   (table.HasEnabledBefore());
            Assert::IsTrue   (table.TryMatchBefore (0x0300, store, hit));
            Assert::AreEqual (writeOnly,        hit.id);
            Assert::AreEqual ((Word) 0x0410,    hit.address);
            Assert::AreEqual ((Word) 0x0300,    hit.accessPc);
            Assert::IsTrue   (hit.mode == WatchMode::Before);
            Assert::IsTrue   (hit.access == WatchAccess::Write);
            Assert::IsFalse  (hit.previous.has_value());

            Assert::IsFalse  (table.TryMatchBefore (0x0300, load, hit), L"a write-only watch ignores a predicted read");
            Assert::IsTrue   (table.TryMatchBefore (0x0300, rmw, hit),  L"a read-modify-write matches a write watch");
            Assert::AreEqual ((uint32_t) 2, table.GetAll()[0].hits);
            Assert::AreEqual ((uint32_t) 0, table.GetAll()[1].hits, L"an after-mode entry is not matched by prediction");
            Assert::AreEqual (1, after);
        }



        TEST_METHOD (OneInstructionOneStop_SuppressesTheResumedAccess)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);



            table.Add (WatchAccess::ReadWrite, 0x0400, 0x04FF);

            // A before-stop happened for the instruction at $0300 on this
            // range; when it executes, its accesses to the range are ignored.
            table.SetAccessPc (0x0300);
            table.SuppressAfterStopFor (0x0300, 0x0400, 0x04FF);
            table.OnWatchedAccess (0x0410, 0x41, BusAccess::Write, (Byte) 0x00);
            Assert::IsFalse  (table.HasPendingStop());
            Assert::AreEqual ((uint32_t) 0, table.GetAll()[0].hits);

            // An access outside the range from the same instruction still counts.
            table.OnWatchedAccess (0x0500, 0x41, BusAccess::Write, (Byte) 0x00);
            Assert::IsFalse  (table.HasPendingStop(), L"outside the watch range, so no hit either way");

            // The next instruction is not suppressed.
            table.SetAccessPc (0x0303);
            table.OnWatchedAccess (0x0410, 0x42, BusAccess::Write, (Byte) 0x41);
            Assert::IsTrue   (table.HasPendingStop());
            Assert::AreEqual ((Word) 0x0303, table.GetPendingHit()->accessPc);
        }



        TEST_METHOD (BeforeMode_NotInMask_NotReportedByBus)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);
            MockDebugTarget  target;
            int              before = 0;



            table.SetTarget (&target);
            before = table.Add (WatchAccess::Write, 0x0400, 0x0400, WatchMode::Before);
            table.Add (WatchAccess::Read, 0xC019, 0xC019);

            Assert::IsFalse  (target.watchedPages[0x04]);
            Assert::IsTrue   (target.watchedPages[0xC0]);
            Assert::IsTrue   (table.GetAll()[0].mode == WatchMode::Before);

            table.OnWatchedAccess (0x0400, 0x01, BusAccess::Write, (Byte) 0x00);
            Assert::IsFalse  (table.HasPendingStop());
            Assert::AreEqual ((uint32_t) 0, table.GetAll()[0].hits);
            Assert::AreEqual (0, before);
        }
    };
}
