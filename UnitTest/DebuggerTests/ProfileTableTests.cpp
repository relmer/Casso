#include "Pch.h"

#include "Cpu.h"
#include "Debugger/ProfileTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTableTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ProfileTableTests)
    {
    public:

        static constexpr Byte  kLdaAbsoluteX = 0xBD;
        static constexpr Byte  kBne          = 0xD0;



        TEST_METHOD (Off_CountsNothing)
        {
            ProfileTable  table;



            table.Record (0x0300, kLdaAbsoluteX, 4, 0);

            Assert::IsFalse  (table.IsOn());
            Assert::AreEqual ((uint64_t) 0, table.GetInstructionCount());
            Assert::AreEqual ((uint64_t) 0, table.GetTotalCycles());
            Assert::IsTrue   (table.GetByAddress().empty());
        }



        TEST_METHOD (On_SplitsBaseCyclesFromPenalties)
        {
            ProfileTable  table;



            table.SetOn  (true);
            table.Record (0x0302, kLdaAbsoluteX, 4, 0);
            table.Record (0x0302, kLdaAbsoluteX, 5, Cpu::kPenaltyPageCross);
            table.Record (0x0308, kBne,          4, Cpu::kPenaltyBranchTaken | Cpu::kPenaltyBranchCross);
            table.Record (0x0308, kBne,          2, 0);

            Assert::AreEqual ((uint64_t) 2,  table.GetOpcode (kLdaAbsoluteX).count);
            Assert::AreEqual ((uint64_t) 8,  table.GetOpcode (kLdaAbsoluteX).cycles, L"the crossing cycle is not base cost");
            Assert::AreEqual ((uint64_t) 4,  table.GetOpcode (kBne).cycles);
            Assert::AreEqual ((uint64_t) 1,  table.GetPenalties().pageCross);
            Assert::AreEqual ((uint64_t) 1,  table.GetPenalties().branchTaken);
            Assert::AreEqual ((uint64_t) 1,  table.GetPenalties().branchCross);
            Assert::AreEqual ((uint64_t) 9,  table.GetByAddress().at (0x0302), L"an address carries the whole cost");
            Assert::AreEqual ((uint64_t) 6,  table.GetByAddress().at (0x0308));
            Assert::AreEqual ((uint64_t) 15, table.GetTotalCycles());
            Assert::AreEqual ((uint64_t) 4,  table.GetInstructionCount());
        }



        TEST_METHOD (Reset_ClearsCountsAndKeepsOn)
        {
            ProfileTable  table;



            table.SetOn  (true);
            table.Record (0x0300, kLdaAbsoluteX, 5, Cpu::kPenaltyPageCross);
            table.Reset();

            Assert::IsTrue   (table.IsOn());
            Assert::AreEqual ((uint64_t) 0, table.GetOpcode (kLdaAbsoluteX).count);
            Assert::AreEqual ((uint64_t) 0, table.GetPenalties().pageCross);
            Assert::AreEqual ((uint64_t) 0, table.GetTotalCycles());
            Assert::IsTrue   (table.GetByAddress().empty());
        }
    };
}
