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



        //  Each address keeps the instruction that last wrote it and the one
        //  that last read it as data, with where it stood and its cycle; an
        //  opcode's read is the fetch's, not the instruction before's.
        TEST_METHOD (TheLastWriterAndReaderOfAnAddressAreKept)
        {
            AccessHeatMap   map;
            uint64_t        position = 10;
            uint64_t        cycles   = 500;
            HeatLastAccess  access;



            map.SetPositionSource (&position);
            map.SetCycleSource    (&cycles);
            map.Start (GetCpu65C02InstructionSet(), 0);

            // $0300: STA $2000
            Run   (map, 0x0300, kStaAbsolute, 2);
            Write (map, 0x2000);

            position = 11;
            cycles   = 504;

            // $0303: LDA $2000, then $0306: NOP, whose opcode comes as a read
            // before its fetch takes the read back.
            Run  (map, 0x0303, kLdaAbsolute, 2);
            Read (map, 0x2000);

            position = 12;
            cycles   = 508;

            Run (map, 0x0306, kNop, 0);

            position = 13;

            Assert::IsTrue   (map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access) == HeatAccessState::Found, L"written");
            Assert::AreEqual ((Word) 0x0300,    access.GetPc());
            Assert::AreEqual ((uint64_t) 10,    access.GetPosition());
            Assert::AreEqual ((uint64_t) 500,   access.cycle);

            Assert::IsTrue   (map.GetLastAccess (HeatSpace::Cpu, false, 0x2000, access) == HeatAccessState::Found, L"read");
            Assert::AreEqual ((Word) 0x0303,    access.GetPc());
            Assert::AreEqual ((uint64_t) 11,    access.GetPosition());
            Assert::AreEqual ((uint64_t) 504,   access.cycle);

            Assert::IsFalse  (map.GetLastAccess (HeatSpace::Cpu, false, 0x0306, access) == HeatAccessState::Found, L"an opcode is executed, not read");
            Assert::IsFalse  (map.GetLastAccess (HeatSpace::Cpu, true,  0x2001, access) == HeatAccessState::Found, L"untouched");

            //  As of a position before the write, it has not happened.
            position = 10;
            Assert::AreEqual ((int) HeatAccessState::Unknown, (int) map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access), L"as of its own position, the access before it is not in the map");

            //  A reset shows nothing from before it.
            position = 13;
            map.Reset();
            Assert::IsFalse  (map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access) == HeatAccessState::Found, L"from before the reset");
        }



        //  A replay of the past makes accesses at earlier positions, which
        //  leave a later record in place: the record is the last up to the
        //  furthest the machine ran, and unknown before it. History cut at a
        //  position forgets the records from there on.
        TEST_METHOD (ARecordOnlyMovesLaterAndACutForgetsTheFuture)
        {
            AccessHeatMap   map;
            uint64_t        position = 10;
            HeatLastAccess  access;



            map.SetPositionSource (&position);
            map.Start (GetCpu65C02InstructionSet(), 0);

            position = 50;
            Run   (map, 0x0300, kStaAbsolute, 2);
            Write (map, 0x2000);

            position = 20;
            Run   (map, 0x0400, kStaAbsolute, 2);
            Write (map, 0x2000);

            position = 60;
            Assert::AreEqual ((int) HeatAccessState::Found, (int) map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access));
            Assert::AreEqual ((Word) 0x0300, access.GetPc(), L"the replay's earlier write left the later one");

            position = 30;
            Assert::AreEqual ((int) HeatAccessState::Unknown, (int) map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access), L"before the later record, unknown");

            map.ForgetAccessesFrom (40);
            position = 60;
            Assert::AreEqual ((int) HeatAccessState::Unknown, (int) map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access), L"a record from a future dropped is forgotten");

            Run   (map, 0x0500, kStaAbsolute, 2);
            Write (map, 0x2000);
            position = 61;
            Assert::AreEqual ((int) HeatAccessState::Found, (int) map.GetLastAccess (HeatSpace::Cpu, true, 0x2000, access), L"and set again by the next write");
            Assert::AreEqual ((Word) 0x0500, access.GetPc());
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

            Assert::AreEqual ((float) map.GetFadePerFrame(), map.GetHeat (HeatKind::Read, 0x2000), 1e-5f);

            map.Fold (1000 + AccessHeatMap::kCyclesPerFrame * 10000);
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
            constexpr double  kTop = 4096.0;



            Assert::AreEqual ((Byte) 0,   AccessHeatMap::ToLevel (0.0,   kTop));
            Assert::AreEqual ((Byte) 1,   AccessHeatMap::ToLevel (0.02,  kTop), L"any warm address shows");
            Assert::AreEqual ((Byte) 255, AccessHeatMap::ToLevel (kTop,  kTop));
            Assert::AreEqual ((Byte) 255, AccessHeatMap::ToLevel (kTop * 10, kTop));
            Assert::IsTrue   (AccessHeatMap::ToLevel (5.0, kTop) < AccessHeatMap::ToLevel (500.0, kTop));
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
            Assert::AreEqual (AccessHeatMap::ToLevel (map.GetRate (HeatKind::Write, 0xFFFF), AccessHeatMap::kHottestPerSecond), levels[0xFFFF]);
            Assert::AreEqual ((Byte) 0, levels[0xFFFE]);
        }



        //  About the top rate, in accesses a frame.
        static constexpr int  kHottestPerFrame = (int) (AccessHeatMap::kHottestPerSecond / AccessHeatMap::kFramesPerSecond) + 1;

        //  The cycles in a stretch of machine time.
        static uint64_t GetCycles (double seconds)
        {
            return (uint64_t) (seconds * AccessHeatMap::kCyclesPerSecond);
        }

        //  One address read `perFrame` times a frame for `frames` frames,
        //  folded each frame, starting at cycle 0.
        static void Feed (AccessHeatMap & map, Word address, int perFrame, int frames)
        {
            for (int frame = 1; frame <= frames; frame++)
            {
                for (int i = 0; i < perFrame; i++)
                {
                    Read (map, address);
                }

                map.Fold ((uint64_t) frame * AccessHeatMap::kCyclesPerFrame);
            }
        }



        TEST_METHOD (TheDefaultFadeKeepsASingleAccessForTenSeconds)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);
            Read (map, 0x2000);
            map.Fold (0);

            map.Fold (GetCycles (9.5));
            Assert::IsTrue (map.GetHeat (HeatKind::Read, 0x2000) > 0.0f, L"still on the map after nine and a half seconds");

            map.Fold (GetCycles (10.5));
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read, 0x2000), L"gone after ten");
        }



        TEST_METHOD (TheFadeTimeIsHowLongASingleAccessStays)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);
            map.SetFadeSeconds (30.0);
            Read (map, 0x2000);
            map.Fold (0);

            map.Fold (GetCycles (29.0));
            Assert::IsTrue (map.GetHeat (HeatKind::Read, 0x2000) > 0.0f);

            map.Fold (GetCycles (31.0));
            Assert::AreEqual (0.0f, map.GetHeat (HeatKind::Read, 0x2000));
        }



        TEST_METHOD (ABusyAddressDimsStepByStepOnceLeft)
        {
            AccessHeatMap      map;
            std::vector<Byte>  levels;
            Byte               busy    = 0;
            Byte               later   = 0;
            Byte               latest  = 0;
            uint64_t           stopped = 0;



            //  At the top rate long enough to settle at a two-second fade.
            map.Start (GetCpu65C02InstructionSet(), 0);
            map.SetFadeSeconds (2.0);
            Feed (map, 0x2000, kHottestPerFrame, 200);
            stopped = 200 * AccessHeatMap::kCyclesPerFrame;

            map.GetLevels (HeatKind::Read, levels);
            busy = levels[0x2000];

            map.Fold (stopped + GetCycles (0.5));
            map.GetLevels (HeatKind::Read, levels);
            later = levels[0x2000];

            map.Fold (stopped + GetCycles (1.5));
            map.GetLevels (HeatKind::Read, levels);
            latest = levels[0x2000];

            Assert::AreEqual ((Byte) 255, busy, L"a busy address is at the top");
            Assert::IsTrue   (later < busy && later > 0, L"left half a second, it has dimmed but still shows");
            Assert::IsTrue   (latest < later && latest > 0, L"left a second and a half, dimmer still, and still shows");
        }



        TEST_METHOD (ASteadyRateShowsAtTheSameLevelWhateverTheFade)
        {
            AccessHeatMap      shortFade;
            AccessHeatMap      longFade;
            std::vector<Byte>  shortLevels;
            std::vector<Byte>  longLevels;



            shortFade.Start (GetCpu65C02InstructionSet(), 0);
            longFade.Start  (GetCpu65C02InstructionSet(), 0);
            shortFade.SetFadeSeconds (1.0);
            longFade.SetFadeSeconds  (4.0);

            //  Long enough for the longer fade to settle.
            Feed (shortFade, 0x2000, 20, 400);
            Feed (longFade,  0x2000, 20, 400);

            shortFade.GetLevels (HeatKind::Read, shortLevels);
            longFade.GetLevels  (HeatKind::Read, longLevels);

            Assert::IsTrue (std::abs ((int) shortLevels[0x2000] - (int) longLevels[0x2000]) <= 1, L"the level is the rate's, not the fade's");
            Assert::IsTrue (shortLevels[0x2000] > 1 && shortLevels[0x2000] < 255);
        }



        TEST_METHOD (TotalsNeverFade)
        {
            AccessHeatMap  map;



            map.Start (GetCpu65C02InstructionSet(), 0);
            Read  (map, 0x2000);
            Write (map, 0x2001);
            Write (map, 0x2001);
            map.Fold (0);
            map.Fold (GetCycles (1000.0));

            Assert::AreEqual (0.0f,             map.GetHeat  (HeatKind::Read,  0x2000), L"the heat has faded");
            Assert::AreEqual ((uint64_t) 1,     map.GetTotal (HeatKind::Read,  0x2000), L"the total has not");
            Assert::AreEqual ((uint64_t) 2,     map.GetTotal (HeatKind::Write, 0x2001));
        }



        TEST_METHOD (TotalLevelsRiseWithTheLogarithmOfTheCountToTheBusiestAddress)
        {
            AccessHeatMap      map;
            std::vector<Byte>  reads;
            std::vector<Byte>  writes;



            map.Start (GetCpu65C02InstructionSet(), 0);
            Feed (map, 0x2000, 100000, 10);     // a million reads
            Feed (map, 0x3000, 1000, 1);        // a thousand
            Read  (map, 0x4000);                // one
            Write (map, 0x5000);                // one, of another kind
            map.Fold (GetCycles (2000.0));

            map.GetTotalLevels (HeatKind::Read,  reads);
            map.GetTotalLevels (HeatKind::Write, writes);

            Assert::AreEqual ((Byte) 255, reads[0x2000], L"the busiest address is the top");
            Assert::IsTrue   (std::abs ((int) reads[0x3000] - 128) <= 2, L"a thousand is half way to a million on a log scale");
            Assert::IsTrue   (reads[0x4000] >= 1 && reads[0x4000] < reads[0x3000], L"a single access still shows");
            Assert::AreEqual (reads[0x4000], writes[0x5000], L"every kind is measured against the same busiest address");
            Assert::AreEqual ((Byte) 0, reads[0x6000]);
        }



        TEST_METHOD (ResetZeroesTheHeatAndTheTotalsAndKeepsTheMapOn)
        {
            AccessHeatMap      map;
            std::vector<Byte>  levels;



            map.Start (GetCpu65C02InstructionSet(), 0);
            Read (map, 0x2000);
            map.Fold (0);
            Read (map, 0x2001);
            map.Reset();
            map.Fold (0);

            Assert::IsTrue   (map.IsOn());
            Assert::AreEqual (0.0f,         map.GetHeat  (HeatKind::Read, 0x2000));
            Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatKind::Read, 0x2000));
            Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatKind::Read, 0x2001), L"a count not yet folded goes too");

            Read (map, 0x2002);
            map.Fold (0);
            map.GetTotalLevels (HeatKind::Read, levels);
            Assert::AreEqual ((Byte) 255, levels[0x2002], L"counting starts over");
        }
    };
}
