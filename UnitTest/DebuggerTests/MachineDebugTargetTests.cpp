#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/SynchronousRunDriver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTargetTests
//
//  IDebugTarget over a real machine, running synchronously.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MachineDebugTargetTests)
    {
    public:

        class RecordingObserver : public IRunObserver
        {
        public:
            std::vector<StopEvent> stops;

            void OnStopped (const StopEvent & stop) override { stops.push_back (stop); }
        };

        class AddressConditions : public DebugHook
        {
        public:
            Word  stopAt = 0;

            bool ShouldStopBefore (Word pc) override { return pc == stopAt; }
            bool HasPendingStop() const override     { return false; }
        };

        ////////////////////////////////////////////////////////////////////////
        //
        //  Rig
        //
        //  A machine, a target over it, and a synchronous driver, wired the
        //  way batch mode wires them.
        //
        ////////////////////////////////////////////////////////////////////////

        struct Rig
        {
            TestMachine           machine;
            MachineDebugTarget    target;
            SynchronousRunDriver  driver;
            RecordingObserver     observer;

            explicit Rig (const std::string & id) :
                machine (id, TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, target.GetRunHook())
            {
                target.SetRunDriver   (&driver);
                target.SetRunObserver (&observer);
                machine.PowerCycle();
            }
        };

        static constexpr Byte      kIrqMask              = 0x34;
        static constexpr Byte      kStackTop             = 0xFF;
        static constexpr uint64_t  kBudget               = 1000;
        static constexpr uint64_t  kMaxInstructionCycles = 7;



        ////////////////////////////////////////////////////////////////////////
        //
        //  Load
        //
        //  Writes bytes at address and points the CPU at start with X set and
        //  interrupts masked.
        //
        ////////////////////////////////////////////////////////////////////////

        static void Load (Rig & rig, Word address, std::initializer_list<Byte> bytes, Word start, Byte x)
        {
            Cpu6502Registers  registers = {};
            Word              at        = address;



            for (Byte b : bytes)
            {
                rig.target.TryPoke (at++, b);
            }

            registers    = rig.target.GetRegisters();
            registers.pc = start;
            registers.x  = x;
            registers.sp = kStackTop;
            registers.p  = kIrqMask;
            rig.target.SetRegisters (registers);
        }

        static RunRequest MakeRun (RunKind kind)
        {
            RunRequest request;



            request.kind = kind;
            return request;
        }

        static const StopEvent & LastStop (Rig & rig)
        {
            Assert::IsFalse (rig.observer.stops.empty(), L"no stop was delivered");
            return rig.observer.stops.back();
        }



        TEST_METHOD (Registers_RoundTrip)
        {
            Rig               rig ("Apple2e");
            Cpu6502Registers  set = { 0x1234, 0x11, 0x22, 0x33, 0xE0, 0x30 };
            Cpu6502Registers  got = {};



            rig.target.SetRegisters (set);
            got = rig.target.GetRegisters();

            Assert::AreEqual (set.pc, got.pc);
            Assert::AreEqual (set.a,  got.a);
            Assert::AreEqual (set.x,  got.x);
            Assert::AreEqual (set.y,  got.y);
            Assert::AreEqual (set.sp, got.sp);
        }



        TEST_METHOD (RunTo_StopsAtAddress)
        {
            Rig         rig ("Apple2e");
            RunRequest  run = MakeRun (RunKind::RunTo);



            // $0300: INX INX INX JMP $0300
            Load (rig, 0x0300, { 0xE8, 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300, 0);
            run.hasUntilPc = true;
            run.untilPc    = 0x0303;

            Assert::AreEqual (S_OK, rig.target.StartRun (run));

            Assert::AreEqual ((int) StopReason::RunTo, (int) LastStop (rig).reason);
            Assert::AreEqual ((Word) 0x0303,           LastStop (rig).pc);
            Assert::AreEqual ((uint8_t) 3,             LastStop (rig).registers.x);
        }



        TEST_METHOD (Go_BudgetStop)
        {
            Rig         rig ("Apple2e");
            RunRequest  run = MakeRun (RunKind::Go);



            Load (rig, 0x0300, { 0xE8, 0x4C, 0x00, 0x03 }, 0x0300, 0);
            run.budget = kBudget;

            Assert::AreEqual (S_OK, rig.target.StartRun (run));

            Assert::AreEqual ((int) StopReason::Budget, (int) LastStop (rig).reason);
            Assert::IsTrue   (LastStop (rig).cycles >= kBudget);
            Assert::IsTrue   (LastStop (rig).cycles <  kBudget + kMaxInstructionCycles);
        }



        TEST_METHOD (StepInto_Count)
        {
            Rig         rig ("Apple2e");
            RunRequest  run = MakeRun (RunKind::StepInto);



            Load (rig, 0x0300, { 0xE8, 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300, 0);
            run.count = 2;

            Assert::AreEqual (S_OK, rig.target.StartRun (run));

            Assert::AreEqual ((int) StopReason::Step, (int) LastStop (rig).reason);
            Assert::AreEqual ((Word) 0x0302,          LastStop (rig).pc);
        }



        TEST_METHOD (StepOver_RecursiveCall_IsOneStep)
        {
            Rig         rig ("Apple2e");
            RunRequest  run = MakeRun (RunKind::StepOver);



            // $0300: DEX / BEQ $0306 / JSR $0300 / RTS. Stepping over the JSR
            // at $0303 recurses three deep; every inner call returns to $0306
            // with a deeper stack, and only the outermost one completes the step.
            Load (rig, 0x0300, { 0xCA, 0xF0, 0x03, 0x20, 0x00, 0x03, 0x60 }, 0x0303, 3);

            Assert::AreEqual (S_OK, rig.target.StartRun (run));

            Assert::AreEqual ((int) StopReason::Step, (int) LastStop (rig).reason);
            Assert::AreEqual ((Word) 0x0306,          LastStop (rig).pc);
            Assert::AreEqual ((uint8_t) kStackTop,    LastStop (rig).registers.sp);
            Assert::AreEqual ((uint8_t) 0,            LastStop (rig).registers.x);
        }



        TEST_METHOD (StepOut_StopsAfterReturn)
        {
            Rig         rig ("Apple2e");



            // $0300: JSR $0310 / NOP ... $0310: INX / RTS
            Load (rig, 0x0300, { 0x20, 0x10, 0x03, 0xEA }, 0x0300, 0);
            Load (rig, 0x0310, { 0xE8, 0x60 },             0x0300, 0);

            Assert::AreEqual (S_OK, rig.target.StartRun (MakeRun (RunKind::StepInto)));
            Assert::AreEqual ((Word) 0x0310, LastStop (rig).pc);

            Assert::AreEqual (S_OK, rig.target.StartRun (MakeRun (RunKind::StepOut)));
            Assert::AreEqual ((int) StopReason::Step, (int) LastStop (rig).reason);
            Assert::AreEqual ((Word) 0x0303,          LastStop (rig).pc);
            Assert::AreEqual ((uint8_t) 1,            LastStop (rig).registers.x);
        }



        TEST_METHOD (Breakpoint_ThroughConditions)
        {
            Rig                rig ("Apple2e");
            AddressConditions  conditions;



            Load (rig, 0x0300, { 0xE8, 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300, 0);
            conditions.stopAt = 0x0302;
            rig.target.SetStopConditions (&conditions);

            Assert::AreEqual (S_OK, rig.target.StartRun (MakeRun (RunKind::Go)));

            Assert::AreEqual ((int) StopReason::Breakpoint, (int) LastStop (rig).reason);
            Assert::AreEqual ((Word) 0x0302,                LastStop (rig).pc);
        }



        TEST_METHOD (HookAndMask_Forwarded)
        {
            Rig           rig ("Apple2e");
            WatchedPages  pages = {};



            rig.target.SetHookInstalled (true);
            Assert::IsNotNull (rig.machine.GetDebugHook());

            rig.target.SetHookInstalled (false);
            Assert::IsNull (rig.machine.GetDebugHook());

            pages[0x03] = true;
            rig.target.SetWatchedPages (pages);
            Assert::IsTrue  (rig.machine.GetMemoryBus().IsPageWatched (0x03));
            Assert::IsFalse (rig.machine.GetMemoryBus().IsPageWatched (0x04));
        }



        TEST_METHOD (VideoPosition_FollowsTiming)
        {
            Rig            rig ("Apple2e");
            VideoPosition  position;



            rig.machine.RunCycles (kBudget);
            position = rig.target.GetVideoPosition();

            Assert::AreEqual (rig.machine.GetVideoTiming()->GetCurrentScanline(), position.scanline);
            Assert::AreEqual (rig.machine.GetVideoTiming()->GetHorizontalPos(),   position.cycleInLine);
        }



        TEST_METHOD (SoftSwitches_AndCpuKind_EachMachine)
        {
            static constexpr const char * kMachines[] = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" };

            size_t checked = 0;



            for (const char * id : kMachines)
            {
                Rig                      rig (id);
                std::vector<SoftSwitch>  switches;
                std::string              narrow (id);
                bool                     hasText  = false;
                bool                     hasRamRd = false;
                bool                     isIie    = rig.machine.GetMmu() != nullptr;
                bool                     isCmos   = narrow == "Apple2eEnhanced" || narrow == "Apple2c";



                rig.target.GetSoftSwitches (switches);

                for (const SoftSwitch & entry : switches)
                {
                    hasText  |= entry.name == "TEXT";
                    hasRamRd |= entry.name == "RAMRD";
                }

                Assert::IsTrue   (hasText);
                Assert::AreEqual (isIie, hasRamRd);
                Assert::AreEqual ((int) (isCmos ? DebugCpuKind::M65C02 : DebugCpuKind::M6502), (int) rig.target.GetCpuKind());
                ++checked;
            }

            Assert::AreEqual (std::size (kMachines), checked);
        }
    };
}
