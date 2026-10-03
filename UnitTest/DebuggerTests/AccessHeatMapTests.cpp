#include "Pch.h"

#include "Core/Cpu65C02Table.h"
#include "Debugger/AccessHeatMap.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AccessHeatMapTests
//
//  The counts and the fading heat behind the heat map pane, fed the way the
//  bus and the CPU feed it: the opcode as a read, then the fetch, then the
//  operand bytes as reads.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AccessHeatMapTests)
    {
    public:

        static constexpr Byte  kLdaAbsolute = 0xAD;
        static constexpr Byte  kStaAbsolute = 0x8D;
        static constexpr Byte  kNop         = 0xEA;

        static void Read  (AccessHeatMap & map, Word address) { map.OnWatchedAccess (address, 0, BusAccess::Read,  std::nullopt); }
        static void Write (AccessHeatMap & map, Word address) { map.OnWatchedAccess (address, 0, BusAccess::Write, std::nullopt); }

        //  An instruction as the CPU runs it: the opcode read, its fetch,
        //  then its operand bytes.
        static void Run (AccessHeatMap & map, Word pc, Byte opcode, int operands)
        {
            Read (map, pc);
            map.OnFetch (pc, opcode);

            for (int i = 1; i <= operands; i++)
            {
                Read (map, (Word) (pc + i));
            }
        }



        TEST_METHOD (AnInstructionsBytesAreExecutedAndItsOperandIsRead)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);

            // $0300: LDA $2000
            Run  (map, 0x0300, kLdaAbsolute, 2);
            Read (map, 0x2000);
            map.Fold (0);

            for (Word address = 0x0300; address <= 0x0302; address++)
            {
                Assert::AreEqual (1.0f, map.GetHeat (HeatKind::Execute, address), L"each byte of the instruction ran once");
                Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read,    address), L"an instruction byte is not data");
            }

            Assert::AreEqual (1.0f, map.GetHeat (HeatKind::Read,    0x2000));
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Execute, 0x2000));
        }



        TEST_METHOD (AWriteIsCountedAsAWrite)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);

            // $0300: STA $2000
            Run   (map, 0x0300, kStaAbsolute, 2);
            Write (map, 0x2000);
            Write (map, 0x2000);
            map.Fold (0);

            Assert::AreEqual (2.0f, map.GetHeat (HeatKind::Write, 0x2000));
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read,  0x2000));
        }



        TEST_METHOD (AReadOfTheNextAddressAfterAOneByteInstructionIsData)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);

            //  A NOP takes no operand, so a read of the byte after it is not
            //  part of it.
            Run  (map, 0x0300, kNop, 0);
            Read (map, 0x0301);
            map.Fold (0);

            Assert::AreEqual (1.0f, map.GetHeat (HeatKind::Execute, 0x0300));
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Execute, 0x0301));
            Assert::AreEqual (1.0f, map.GetHeat (HeatKind::Read,    0x0301));
        }



        TEST_METHOD (HeatFadesByTheFramesThatWentBy)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 1000);
            Read (map, 0x2000);
            map.Fold (1000);
            map.Fold (1000 + AccessHeatMap::kCyclesPerFrame);

            Assert::AreEqual (AccessHeatMap::kFadePerFrame, map.GetHeat (HeatKind::Read, 0x2000), 1e-5f);

            map.Fold (1000 + AccessHeatMap::kCyclesPerFrame * 100);
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read, 0x2000), L"a long-untouched address is cold again");
        }



        TEST_METHOD (ACycleCountThatWentBackFadesNothing)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 50000);
            Write (map, 0x2000);
            map.Fold (50000);
            map.Fold (10);

            Assert::AreEqual (1.0f, map.GetHeat (HeatKind::Write, 0x2000));
        }



        TEST_METHOD (AMapThatIsOffHoldsNothingAndCountsNothing)
        {
            AccessHeatMap      map;
            std::vector<Byte>  levels;



            Read (map, 0x2000);
            map.Fold (0);
            map.GetLevels (HeatKind::Read, levels);

            Assert::IsFalse (map.IsOn());
            Assert::IsTrue  (levels.empty());

            map.Start (GetCpu65C02InstructionSet(), 0);
            Read (map, 0x2000);
            map.Stop();

            Assert::IsFalse (map.IsOn());
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read, 0x2000));
        }



        TEST_METHOD (LevelsRiseWithHeatFromOneToTheTop)
        {
            Assert::AreEqual ((Byte) 0,   AccessHeatMap::HeatToLevel (0.0f));
            Assert::AreEqual ((Byte) 1,   AccessHeatMap::HeatToLevel (0.02f), L"any warm address shows");
            Assert::AreEqual ((Byte) 255, AccessHeatMap::HeatToLevel (AccessHeatMap::kHottestHeat));
            Assert::AreEqual ((Byte) 255, AccessHeatMap::HeatToLevel (AccessHeatMap::kHottestHeat * 10));
            Assert::IsTrue   (AccessHeatMap::HeatToLevel (5.0f) < AccessHeatMap::HeatToLevel (500.0f));
        }



        TEST_METHOD (LevelsHoldOneEntryPerAddress)
        {
            AccessHeatMap      map;
            std::vector<Byte>  levels;



            map.Start (GetCpu65C02InstructionSet(), 0);
            Write (map, 0xFFFF);
            map.Fold (0);
            map.GetLevels (HeatKind::Write, levels);

            Assert::AreEqual (AccessHeatMap::kAddressCount, levels.size());
            Assert::AreEqual (AccessHeatMap::HeatToLevel (1.0f), levels[0xFFFF]);
            Assert::AreEqual ((Byte) 0, levels[0xFFFE]);
        }
    };
}
