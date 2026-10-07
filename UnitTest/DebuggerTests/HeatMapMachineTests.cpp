#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapMachineTests
//
//  The heat map over a real machine: on, the bus and the CPU report to it and
//  every page takes the watched path; off, the pages are served directly
//  again and nothing reports to it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapMachineTests)
    {
    public:

        class QuietObserver : public IRunObserver
        {
        public:
            void OnStopped (const StopEvent &) override {}
        };

        struct Rig
        {
            TestMachine           machine;
            MachineDebugTarget    target;
            SynchronousRunDriver  driver;
            QuietObserver         observer;

            Rig() :
                machine ("Apple2e", TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, target.GetRunHook())
            {
                target.SetRunDriver   (&driver);
                target.SetRunObserver (&observer);
                machine.PowerCycle();
            }
        };

        static constexpr uint64_t  kBudget = 2000;

        //  $0300: LDA $2000 / STA $2001 / JMP $0300, run for kBudget cycles.
        static void RunLoop (Rig & rig)
        {
            static constexpr Byte  kProgram[] = { 0xAD, 0x00, 0x20, 0x8D, 0x01, 0x20, 0x4C, 0x00, 0x03 };
            Cpu6502Registers       registers  = rig.target.GetRegisters();
            RunRequest             run;
            Word                   at         = 0x0300;



            for (Byte b : kProgram)
            {
                rig.target.TryPoke (at++, b);
            }

            registers.pc = 0x0300;
            registers.p  = 0x34;
            rig.target.SetRegisters (registers);

            run.kind   = RunKind::Go;
            run.budget = kBudget;
            Assert::AreEqual (S_OK, rig.target.StartRun (run));
        }



        TEST_METHOD (TheMapIsOffAndTheBusUntouchedUntilTurnedOn)
        {
            Rig  rig;



            Assert::IsNull    (rig.target.FoldHeatMap());
            Assert::IsFalse   (rig.machine.GetMemoryBus().AreAllPagesWatched());
            Assert::IsNotNull (rig.machine.GetMemoryBus().GetReadPage (0x0300), L"RAM is served directly");
        }



        TEST_METHOD (OnTheMapSeesCodeReadsAndWrites)
        {
            Rig                    rig;
            const AccessHeatMap  * map = nullptr;



            rig.target.SetHeatMapOn (true);
            RunLoop (rig);
            map = rig.target.FoldHeatMap();

            Assert::IsNotNull (map);
            Assert::IsTrue    (map->GetHeat (HeatKind::Execute, 0x0300) > 0.0f, L"the loop's first byte ran");
            Assert::IsTrue    (map->GetHeat (HeatKind::Execute, 0x0308) > 0.0f, L"the JMP's last operand byte ran");
            Assert::AreEqual  (0.0f, map->GetHeat (HeatKind::Read, 0x0300), L"code is not data");
            Assert::AreEqual  (0.0f, map->GetHeat (HeatKind::Read, 0x0301), L"an operand is not data");
            Assert::IsTrue    (map->GetHeat (HeatKind::Read,  0x2000) > 0.0f);
            Assert::IsTrue    (map->GetHeat (HeatKind::Write, 0x2001) > 0.0f);
            Assert::AreEqual  (0.0f, map->GetHeat (HeatKind::Read, 0x2001), L"a store does not read its target");
        }



        TEST_METHOD (OffGivesTheBusItsPagesBack)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);

            Assert::IsNull (rig.machine.GetMemoryBus().GetReadPage (0x0300), L"on, every page takes the watched path");

            rig.target.SetHeatMapOn (false);

            Assert::IsNull    (rig.target.FoldHeatMap());
            Assert::IsFalse   (rig.machine.GetMemoryBus().AreAllPagesWatched());
            Assert::IsNotNull (rig.machine.GetMemoryBus().GetReadPage (0x0300));
        }



        TEST_METHOD (OffTheMachineRunsWithoutReportingToTheMap)
        {
            Rig                    rig;
            const AccessHeatMap  * map = nullptr;



            rig.target.SetHeatMapOn (true);
            rig.target.SetHeatMapOn (false);
            RunLoop (rig);
            rig.target.SetHeatMapOn (true);
            map = rig.target.FoldHeatMap();

            Assert::IsNotNull (map);
            Assert::AreEqual  (0.0f, map->GetHeat (HeatKind::Execute, 0x0300), L"nothing ran while it was off");
        }



        TEST_METHOD (AReadTheBusMakesForItselfIsNotCounted)
        {
            Rig                    rig;
            const AccessHeatMap  * map = nullptr;



            //  The video modes read the screen through the bus, not the CPU.
            rig.target.SetHeatMapOn (true);
            rig.machine.GetMemoryBus().ReadByte (0x0400);
            map = rig.target.FoldHeatMap();

            Assert::IsNotNull (map);
            Assert::AreEqual  (0.0f, map->GetHeat (HeatKind::Read, 0x0400));
        }



        //  A //e program that writes aux RAM through RAMWRT, both language
        //  card banks and the high RAM, main and aux, and reads ROM, then
        //  spins. Run for kBudget cycles from $0300.
        static void RunBanking (Rig & rig)
        {
            static constexpr Byte  kProgram[] =
            {
                0x8D, 0x05, 0xC0,       // STA $C005    RAMWRT on
                0x8D, 0x00, 0x20,       // STA $2000    aux $2000
                0x8D, 0x04, 0xC0,       // STA $C004    RAMWRT off
                0xAD, 0x8B, 0xC0,       // LDA $C08B    bank 1, RAM, write armed
                0xAD, 0x8B, 0xC0,       // LDA $C08B    write enabled
                0x8D, 0x00, 0xD0,       // STA $D000    main bank 1
                0xAD, 0x83, 0xC0,       // LDA $C083    bank 2
                0xAD, 0x83, 0xC0,       // LDA $C083
                0x8D, 0x00, 0xD0,       // STA $D000    main bank 2
                0x8D, 0x00, 0xE0,       // STA $E000    main high RAM
                0xAD, 0x82, 0xC0,       // LDA $C082    ROM, write protected
                0xAD, 0x00, 0xD0,       // LDA $D000    ROM
                0x8D, 0x09, 0xC0,       // STA $C009    ALTZP on
                0xAD, 0x8B, 0xC0,       // LDA $C08B    bank 1, RAM, write armed
                0xAD, 0x8B, 0xC0,       // LDA $C08B
                0x8D, 0x01, 0xD0,       // STA $D001    aux bank 1
                0x8D, 0x08, 0xC0,       // STA $C008    ALTZP off
                0xAD, 0x82, 0xC0,       // LDA $C082    ROM
                0x4C, 0x36, 0x03,       // JMP $0336
            };
            Cpu6502Registers       registers  = rig.target.GetRegisters();
            RunRequest             run;
            Word                   at         = 0x0300;



            for (Byte b : kProgram)
            {
                rig.target.TryPoke (at++, b);
            }

            registers.pc = 0x0300;
            registers.p  = 0x34;
            rig.target.SetRegisters (registers);

            run.kind   = RunKind::Go;
            run.budget = kBudget;
            Assert::AreEqual (S_OK, rig.target.StartRun (run));
        }



        TEST_METHOD (EachAccessCountsWhereItLandedAsWellAsWhereTheCpuAddressedIt)
        {
            Rig                    rig;
            const AccessHeatMap  * map = nullptr;



            rig.target.SetHeatMapOn (true);
            RunBanking (rig);
            map = rig.target.FoldHeatMap();

            Assert::IsNotNull (map);
            Assert::AreEqual  ((uint64_t) 2, map->GetTotal (HeatSpace::Cpu,  HeatKind::Write,   0xD000), L"the CPU wrote $D000 twice");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Aux,  HeatKind::Write,   0x2000), L"RAMWRT sent $2000 to aux");
            Assert::AreEqual  ((uint64_t) 0, map->GetTotal (HeatSpace::Main, HeatKind::Write,   0x2000), L"and not to main");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Main, HeatKind::Write,   0xC000), L"bank 1 of $D000 stands at $C000");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Main, HeatKind::Write,   0xD000), L"bank 2 at $D000");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Main, HeatKind::Write,   0xE000), L"the high RAM at $E000");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Aux,  HeatKind::Write,   0xC001), L"ALTZP sent bank 1 to aux");
            Assert::AreEqual  ((uint64_t) 1, map->GetTotal (HeatSpace::Rom,  HeatKind::Read,    0xD000), L"a read with ROM banked in reads ROM");
            Assert::AreEqual  ((uint64_t) 0, map->GetTotal (HeatSpace::Main, HeatKind::Read,    0xD000));
            Assert::IsTrue    (map->GetTotal (HeatSpace::Main, HeatKind::Execute, 0x0300) > 0, L"the code ran from main RAM");
            Assert::AreEqual  ((uint64_t) 0, map->GetTotal (HeatSpace::Main, HeatKind::Read, 0xC005), L"I/O is the CPU's alone");
            Assert::AreEqual  ((uint64_t) 0, map->GetTotal (HeatSpace::Rom,  HeatKind::Write, 0xC005));
        }



        TEST_METHOD (ClearForgetsTheMap)
        {
            Rig  rig;



            rig.target.SetHeatMapOn (true);
            rig.target.ClearHeatMap();

            Assert::IsNull    (rig.target.FoldHeatMap());
            Assert::IsNotNull (rig.machine.GetMemoryBus().GetReadPage (0x0300));
        }
    };
}
