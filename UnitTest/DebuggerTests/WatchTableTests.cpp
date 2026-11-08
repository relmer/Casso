#include "Pch.h"

#include "Debugger/WatchTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTableTests
//
//  The numbered list behind watches, zero-page pointers and bookmarks.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (WatchTableTests)
    {
    public:

        TEST_METHOD (Ids_CountUp_NotReused)
        {
            WatchTable  table;
            int         first  = table.Add (0x0300);
            int         second = table.Add (0x0400);
            int         third  = 0;



            Assert::IsTrue  (table.TryClear (first));
            Assert::IsFalse (table.TryClear (first));

            third = table.Add (0x0500);

            Assert::AreEqual (0, first);
            Assert::AreEqual (1, second);
            Assert::AreEqual (2, third);
            Assert::AreEqual ((size_t) 2, table.GetAll().size());
        }



        TEST_METHOD (EnableDisable_AndFind)
        {
            WatchTable  table;
            WatchItem   item;
            int         id = table.Add (0x0036);



            Assert::IsTrue   (table.TrySetEnabled (id, false));
            Assert::IsTrue   (table.TryFind (id, item));
            Assert::IsFalse  (item.enabled);
            Assert::AreEqual ((Word) 0x0036, item.address);
            Assert::IsFalse  (table.TrySetEnabled (42, true));
            Assert::IsFalse  (table.TryFind (42, item));
        }



        //  Clearing everything also starts the numbering over, which is what
        //  lets a saved script replay into the same ids.
        TEST_METHOD (ClearAll_Empties_AndRestartsNumbering)
        {
            WatchTable  table;



            table.Add (0x0300);
            table.Add (0x0301);
            table.ClearAll();

            Assert::IsTrue   (table.GetAll().empty());
            Assert::AreEqual (0, table.Add (0x0302), L"the first id after a clear is 0");
            Assert::AreEqual (1, table.Add (0x0303));
        }



        //  AddAt fills the slot it is given, keeps the list in id order, and
        //  moves the next plain Add past it.
        TEST_METHOD (AddAt_FillsTheSlot_InIdOrder)
        {
            WatchTable  table;



            Assert::AreEqual (3, table.AddAt (3, 0x0036));
            Assert::AreEqual (4, table.Add (0x0037), L"the next id follows the slot");
            Assert::AreEqual (1, table.AddAt (1, 0x0038));
            Assert::AreEqual (5, table.Add (0x0039), L"a lower slot does not pull the next id back");

            Assert::AreEqual ((size_t) 4, table.GetAll().size());
            Assert::AreEqual (1, table.GetAll()[0].id);
            Assert::AreEqual (3, table.GetAll()[1].id);
            Assert::AreEqual (4, table.GetAll()[2].id);
            Assert::AreEqual (5, table.GetAll()[3].id);
        }



        //  A slot already filled is replaced in place, and enabled again.
        TEST_METHOD (AddAt_AnOccupiedSlot_ReplacesAndEnables)
        {
            WatchTable  table;
            WatchItem   item;



            table.AddAt (3, 0x0036);
            table.TrySetEnabled (3, false);

            Assert::AreEqual (3, table.AddAt (3, 0x003C));
            Assert::AreEqual ((size_t) 1, table.GetAll().size(), L"replaced, not added");
            Assert::IsTrue   (table.TryFind (3, item));
            Assert::AreEqual ((Word) 0x003C, item.address);
            Assert::IsTrue   (item.enabled, L"setting a slot again enables it");
        }
    };
}
