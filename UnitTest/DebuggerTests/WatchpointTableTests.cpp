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
            table.OnWatchedAccess (0xC019, 0x80, BusAccess::Read);

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



            table.OnWatchedAccess (0x0305, 0x01, BusAccess::Write);
            table.OnWatchedAccess (0x0310, 0x01, BusAccess::Read);
            Assert::IsFalse (table.HasPendingStop());

            table.TrySetEnabled (id, false);
            table.OnWatchedAccess (0x0305, 0x01, BusAccess::Read);
            Assert::IsFalse (table.HasPendingStop());
        }



        TEST_METHOD (ReadWrite_MatchesBoth_FirstHitKept)
        {
            int              nextId = 0;
            WatchpointTable  table (nextId);



            table.Add (WatchAccess::ReadWrite, 0x0300, 0x0300);

            table.OnWatchedAccess (0x0300, 0x11, BusAccess::Write);
            table.OnWatchedAccess (0x0300, 0x22, BusAccess::Read);

            Assert::IsTrue   (table.GetPendingHit()->access == WatchAccess::Write);
            Assert::AreEqual ((Byte) 0x11,  table.GetPendingHit()->value);
            Assert::AreEqual ((uint32_t) 2, table.GetAll()[0].hits);
        }
    };
}
