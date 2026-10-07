#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatUnwrittenTests
//
//  The heat map's reads before written and its writes that changed their
//  byte, on a real //e: which RAM counts as written, when the map knows it,
//  the break on such a read, the ranges it leaves out, and the counts as a
//  reset, a power cycle and the bank views see them.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatUnwrittenTests)
    {
    public:

        //  The breakpoint commands, with the run and memory commands beside
        //  them, on a //e power-cycled a moment ago, at cycle zero.
        class Rig : public MachineHandlerRig<BreakpointHandlers>
        {
        public:
            static constexpr uint64_t  kBudget = 20000;

            ExecutionHandlers  execution;
            MemoryHandlers     memory;

            Rig()
            {
                session.AddHandler (&execution);
                session.AddHandler (&memory);
                session.SetBudget  (kBudget);
            }

            const AccessHeatMap & Fold()
            {
                const AccessHeatMap  * map = target.FoldHeatMap();



                Assert::IsNotNull (map, L"the map is on");
                return *map;
            }
        };

        static constexpr Word  kFresh   = 0x2000;      // nothing writes it
        static constexpr Word  kStored  = 0x2001;      // written, then read
        static constexpr Word  kProgram = 0x0300;



        //  $0300: LDA $2000 / STA $2001 / LDA $2001 / JMP $0309 (a spin).
        static void LoadReadThenStore (Rig & rig)
        {
            rig.Load (kProgram, { 0xAD, 0x00, 0x20, 0x8D, 0x01, 0x20, 0xAD, 0x01, 0x20, 0x4C, 0x09, 0x03 }, kProgram);
        }



        //  A byte nothing has written since power-on counts as a read before
        //  written each time it is read, in the CPU's space and in main RAM's;
        //  a byte the program stored first does not, and neither does the
        //  program the debugger put there.
        TEST_METHOD (AReadOfRamNothingWroteIsAReadBeforeWritten)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);
            LoadReadThenStore (rig);
            rig.RunOk ("G");

            const AccessHeatMap & map = rig.Fold();

            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Cpu,  HeatKind::UnwrittenRead, kFresh));
            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Main, HeatKind::UnwrittenRead, kFresh), L"main RAM's view counts it too");
            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Cpu,  HeatKind::Read,          kFresh), L"and it is a read as well");
            Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatSpace::Cpu,  HeatKind::UnwrittenRead, kStored), L"stored before it was read");
            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Cpu,  HeatKind::Read,          kStored));

            for (Word address = kProgram; address < kProgram + 12; address++)
            {
                Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, address), L"code the debugger loaded is written");
            }

            Assert::IsTrue  (map.IsWritten (HeatSpace::Main, kStored));
            Assert::IsTrue  (map.IsWritten (HeatSpace::Main, kProgram), L"a byte the debugger wrote counts as written");
            Assert::IsFalse (map.IsWritten (HeatSpace::Main, kFresh));
        }



        //  An opcode is fetched, not read, so code nobody wrote runs without
        //  a read before written, and the break does not stop on it.
        TEST_METHOD (CodeNothingWroteRunsWithoutAReadBeforeWritten)
        {
            Rig   rig;
            Word  at  = kProgram;



            rig.RunOk ("BRKUNINIT ON");

            //  $0300: LDA #$00 / JMP $0300, stored by the bus, not the CPU or
            //  the debugger.
            for (Byte b : { 0xA9, 0x00, 0x4C, 0x00, 0x03 })
            {
                rig.machine.GetMemoryBus().WriteByte (at++, b);
            }

            rig.Load (kProgram, {}, kProgram);
            rig.RunOk ("G");

            const AccessHeatMap & map = rig.Fold();

            Assert::IsTrue  (rig.LastStop().reason == StopReason::Budget, L"no read before written stopped it");
            Assert::IsFalse (map.IsWritten (HeatSpace::Main, kProgram), L"the code was not written by the CPU");

            for (Word address = kProgram; address < kProgram + 5; address++)
            {
                Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, address), L"code is fetched, not read");
            }
        }



        //  A power cycle makes every byte unwritten again; a reset keeps them,
        //  as RAM keeps its bytes across a reset.
        TEST_METHOD (APowerCycleClearsWhatWasWrittenAndAResetKeepsIt)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);
            LoadReadThenStore (rig);
            rig.RunOk ("G");

            Assert::IsTrue (rig.Fold().IsWritten (HeatSpace::Main, kStored));

            rig.machine.SoftReset();
            Assert::IsTrue (rig.Fold().IsWritten (HeatSpace::Main, kStored), L"a reset keeps it");

            rig.machine.PowerCycle();
            Assert::IsFalse (rig.Fold().IsWritten (HeatSpace::Main, kStored), L"a power cycle clears it");
            Assert::IsFalse (rig.Fold().IsWritten (HeatSpace::Main, kProgram), L"the debugger's bytes as well");
        }



        //  A map turned on after the machine has run cannot know what was
        //  written before, so it counts every byte as written and nothing as
        //  a read before written, and says so, until a power cycle.
        TEST_METHOD (AMapStartedAfterPowerOnTracksNothingUntilAPowerCycle)
        {
            Rig                  rig;
            HeatUnwrittenStatus  status;



            LoadReadThenStore (rig);
            rig.RunOk ("T 1");
            rig.target.SetHeatMapOn (true);

            status = rig.target.GetUnwrittenStatus();
            Assert::IsTrue  (status.isOn);
            Assert::IsFalse (status.isTracking, L"started after power-on");
            Assert::IsTrue  (rig.Fold().IsWritten (HeatSpace::Main, kFresh));

            rig.Load (kProgram, {}, kProgram);
            rig.RunOk ("G");
            Assert::AreEqual ((uint64_t) 0, rig.Fold().GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, kFresh), L"nothing is known, so nothing counts");

            rig.machine.PowerCycle();
            Assert::IsTrue (rig.target.GetUnwrittenStatus().isTracking, L"a power cycle starts it");
        }



        //  BRKUNINIT stops after the read, as a watchpoint does, giving the
        //  address, the value and the instruction that read it.
        TEST_METHOD (BRKUNINIT_StopsAfterAReadBeforeWritten)
        {
            Rig    rig;
            Reply  reply;
            Byte   value = 0;



            rig.RunOk ("BRKUNINIT ON");
            LoadReadThenStore (rig);
            rig.RunOk ("G");

            Assert::IsTrue   (rig.LastStop().reason == StopReason::Watchpoint);
            Assert::IsTrue   (rig.LastStop().watch.has_value());
            Assert::IsTrue   (rig.LastStop().watch->isUnwritten);
            Assert::AreEqual (kFresh,           rig.LastStop().watch->address);
            Assert::AreEqual (kProgram,         rig.LastStop().watch->accessPc, L"the LDA read it");
            Assert::AreEqual ((Word) 0x0303,    rig.LastStop().pc, L"after the read, as a watchpoint stops");

            Assert::IsTrue   (rig.target.TryPeek (kFresh, value));
            Assert::AreEqual (std::format ("Read before written: ${:02X} from $2000 by $0300", value), AppleWinFormatter::FormatStop (rig.LastStop()));

            //  On from there, the other reads were all written first.
            rig.RunOk ("G");
            Assert::IsTrue (rig.LastStop().reason == StopReason::Budget);

            reply = rig.RunOk ("BRKUNINIT OFF");
            Assert::AreEqual (std::string ("Break on read before written: off"), reply.text.at (0));
        }



        //  The ranges left out neither stop the machine nor count in what
        //  BRKUNINIT reports, though the map still counts them.
        TEST_METHOD (BRKUNINIT_LeavesOutTheRangesGiven)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("BRKUNINIT ON");
            rig.target.SetUnwrittenIgnore ({ { 0x2000, 0x20FF } });
            LoadReadThenStore (rig);
            rig.RunOk ("G");

            Assert::IsTrue   (rig.LastStop().reason == StopReason::Budget, L"the read was in a range left out");
            Assert::AreEqual ((uint64_t) 1, rig.Fold().GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, kFresh), L"the map still counts it");

            reply = rig.RunOk ("BRKUNINIT");
            Assert::AreEqual (std::string ("Break on read before written: on"),                                    reply.text.at (0));
            Assert::AreEqual (std::string ("Reads before written counted: 0 at 0 addresses, outside 1 range left out."), reply.text.at (1));

            rig.target.SetUnwrittenIgnore ({});
            reply = rig.RunOk ("BRKUNINIT");
            Assert::AreEqual (std::string ("Reads before written counted: 1 at 1 address."), reply.text.at (1));
        }



        //  Alone, BRKUNINIT says whether the map knows what was written, and
        //  takes nothing but ON or OFF.
        TEST_METHOD (BRKUNINIT_ReportsWhatTheMapKnows)
        {
            Rig    rig;
            Reply  reply;



            reply = rig.RunOk ("BRKUNINIT");
            Assert::AreEqual (std::string ("Break on read before written: off"),          reply.text.at (0));
            Assert::AreEqual (std::string ("The heat map is off, so no reads are seen."), reply.text.at (1));

            LoadReadThenStore (rig);
            rig.RunOk ("T 1");

            reply = rig.RunOk ("BRKUNINIT ON");
            Assert::AreEqual (std::string ("Not tracking yet: the heat map started after power-on. Power-cycle the machine to start."), reply.text.at (1));

            rig.RunFails ("BRKUNINIT MAYBE", "invalid arguments");

            rig.RunOk ("BPC *");
            Assert::IsFalse (rig.session.HasUnwrittenBreak(), L"BPC * clears it with the rest");
        }



        //  A write that stores the value already there is a write but no
        //  change; one that stores another is both.
        TEST_METHOD (AWriteOfTheValueAlreadyThereIsNoChange)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);
            rig.target.TryPoke (kStored, 0xFF);

            // $0300: LDA #$5A / STA $2001 / STA $2001 / LDA #$A5 / STA $2001 / JMP $030C
            rig.Load (kProgram, { 0xA9, 0x5A, 0x8D, 0x01, 0x20, 0x8D, 0x01, 0x20, 0xA9, 0xA5, 0x8D, 0x01, 0x20, 0x4C, 0x0D, 0x03 }, kProgram);
            rig.RunOk ("G");

            const AccessHeatMap & map = rig.Fold();

            Assert::AreEqual ((uint64_t) 3, map.GetTotal (HeatSpace::Cpu,  HeatKind::Write,        kStored));
            Assert::AreEqual ((uint64_t) 2, map.GetTotal (HeatSpace::Cpu,  HeatKind::ChangedWrite, kStored), L"the second store changed nothing");
            Assert::AreEqual ((uint64_t) 2, map.GetTotal (HeatSpace::Main, HeatKind::ChangedWrite, kStored), L"main RAM's view the same");
        }



        //  A write into the language card's window, which the card stores
        //  itself, is a change only when it changes the card's byte; and a
        //  read of the card's RAM nobody wrote is a read before written in
        //  the bank it is in.
        TEST_METHOD (TheLanguageCardsBanksTrackChangesAndWrites)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);

            // $0300: LDA $C08B / LDA $C08B   bank 1, RAM, write enabled
            //        LDA $D010               bank 1 $D010, never written
            //        LDA #$77 / STA $D000 / LDA #$88 / STA $D000 / STA $D000
            //        JMP $0316
            rig.Load (kProgram, { 0xAD, 0x8B, 0xC0, 0xAD, 0x8B, 0xC0, 0xAD, 0x10, 0xD0, 0xA9, 0x77, 0x8D, 0x00, 0xD0,
                                  0xA9, 0x88, 0x8D, 0x00, 0xD0, 0x8D, 0x00, 0xD0, 0x4C, 0x16, 0x03 }, kProgram);
            rig.RunOk ("G");

            const AccessHeatMap & map = rig.Fold();

            Assert::AreEqual ((uint64_t) 3, map.GetTotal (HeatSpace::Main, HeatKind::Write,         0xC000), L"bank 1 of $D000 stands at $C000");
            Assert::AreEqual ((uint64_t) 2, map.GetTotal (HeatSpace::Main, HeatKind::ChangedWrite,  0xC000), L"the last store changed nothing");
            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Main, HeatKind::UnwrittenRead, 0xC010), L"a read before written of bank 1");
            Assert::AreEqual ((uint64_t) 1, map.GetTotal (HeatSpace::Cpu,  HeatKind::UnwrittenRead, 0xD010));
            Assert::IsTrue   (map.IsWritten (HeatSpace::Main, 0xC000));
            Assert::IsFalse  (map.IsWritten (HeatSpace::Main, 0xD000), L"bank 2 was not written");
        }



        //  Reset counts zeroes the new counts with the rest and leaves what
        //  was written alone, since that is what the RAM holds.
        TEST_METHOD (ResetCountsZeroesTheCountsAndKeepsWhatWasWritten)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);
            LoadReadThenStore (rig);
            rig.RunOk ("G");

            Assert::AreEqual ((uint64_t) 1, rig.Fold().GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, kFresh));

            rig.target.ResetHeatMap();

            const AccessHeatMap & map = rig.Fold();

            Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatSpace::Cpu, HeatKind::UnwrittenRead, kFresh));
            Assert::AreEqual ((uint64_t) 0, map.GetTotal (HeatSpace::Cpu, HeatKind::ChangedWrite,  kStored));
            Assert::IsTrue   (map.IsWritten (HeatSpace::Main, kStored), L"the RAM is still written");
            Assert::IsTrue   (rig.target.GetUnwrittenStatus().isTracking);
        }



        //  A machine state loaded, which the debugger hears as a machine
        //  replaced, forgets the map: its counts start over, and as the map
        //  comes on again away from power-on, every byte counts as written.
        TEST_METHOD (ALoadedStateStartsTheMapOverUntracked)
        {
            Rig                  rig;
            HeatUnwrittenStatus  status;



            rig.RunOk ("BRKUNINIT ON");
            LoadReadThenStore (rig);
            rig.RunOk ("G");
            rig.RunOk ("G");

            rig.session.OnMachineChanged ("Apple2e");
            rig.target.SetHeatMapOn (true);

            status = rig.target.GetUnwrittenStatus();
            Assert::IsFalse  (rig.session.HasUnwrittenBreak(), L"the break goes with the machine's other breakpoints");
            Assert::IsFalse  (status.isTracking);
            Assert::AreEqual ((uint64_t) 0, status.reads);
            Assert::AreEqual ((uint64_t) 0, rig.Fold().GetTotal (HeatSpace::Cpu, HeatKind::Write, kStored), L"the counts started over");
        }
    };
}
