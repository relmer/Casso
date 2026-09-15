#include "Pch.h"

#include "Debugger/WatchTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchTableTests
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



        TEST_METHOD (ClearAll_Empties)
        {
            WatchTable  table;



            table.Add (0x0300);
            table.Add (0x0301);
            table.ClearAll();

            Assert::IsTrue (table.GetAll().empty());
        }
    };
}
