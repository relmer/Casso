#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "Debugger/DebugHook.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHookTests
//
//  The per-instruction hook in MachineHost: absent, it changes nothing; present,
//  it stops before an instruction, in StepOne and in the middle of a RunCycles
//  slice, and a stop raised during an instruction takes effect at the next
//  boundary.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugHookTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  ScriptedHook
        //
        //  Stops before any address in stopAddresses. Raises a pending stop
        //  while pendingAt is about to execute, which is what a watchpoint hit
        //  during that instruction looks like to the host.
        //
        ////////////////////////////////////////////////////////////////////////

        class ScriptedHook : public DebugHook
        {
        public:
            std::set<Word>  stopAddresses;
            int             pendingAt = -1;
            bool            pending   = false;
            int             queries   = 0;

            bool ShouldStopBefore (Word pc) override
            {
                ++queries;

                if (pc == pendingAt)
                {
                    pending = true;
                }

                return stopAddresses.contains (pc);
            }

            bool HasPendingStop() const override { return pending; }
        };

        static constexpr Word      kProgramStart = 0x0300;
        static constexpr uint64_t  kLongRun      = 1000000;
        static constexpr uint64_t  kShortRun     = 10000;
        static constexpr uint64_t  kShortSpent   = 100;



        ////////////////////////////////////////////////////////////////////////
        //
        //  LoadLoop
        //
        //  $0300: INX INX INX JMP $0300, with interrupts masked.
        //
        ////////////////////////////////////////////////////////////////////////

        static void LoadLoop (TestMachine & machine)
        {
            static constexpr Byte  kInx      = 0xE8;
            static constexpr Byte  kJmp      = 0x4C;
            static constexpr Byte  kIrqMask  = 0x34;

            const Byte        program[] = { kInx, kInx, kInx, kJmp, 0x00, 0x03 };
            Cpu6502Registers  registers = {};



            machine.PowerCycle();

            for (size_t i = 0; i < std::size (program); ++i)
            {
                machine.GetMemoryBus().WriteByte ((Word) (kProgramStart + i), program[i]);
            }

            registers    = machine.GetCpu()->GetCpu6502()->GetRegisters();
            registers.pc = kProgramStart;
            registers.x  = 0;
            registers.p  = kIrqMask;
            machine.GetCpu()->GetCpu6502()->SetRegisters (registers);
        }



        static Cpu6502Registers GetRegisters (TestMachine & machine)
        {
            return machine.GetCpu()->GetCpu6502()->GetRegisters();
        }



        TEST_METHOD (NoStopHook_BehavesAsNoHook)
        {
            TestMachine   plain  ("Apple2e", TestMachine::Slots::Empty);
            TestMachine   hooked ("Apple2e", TestMachine::Slots::Empty);
            ScriptedHook  hook;
            uint64_t      plainSpent  = 0;
            uint64_t      hookedSpent = 0;



            LoadLoop (plain);
            LoadLoop (hooked);
            hooked.SetDebugHook (&hook);

            plainSpent  = plain.RunCycles  (kShortRun);
            hookedSpent = hooked.RunCycles (kShortRun);

            Assert::AreEqual (plainSpent,              hookedSpent);
            Assert::AreEqual (GetRegisters (plain).x,  GetRegisters (hooked).x);
            Assert::AreEqual (GetRegisters (plain).pc, GetRegisters (hooked).pc);
            Assert::IsTrue   (hook.queries > 0);
        }



        TEST_METHOD (StepOne_StopsBeforeAddress)
        {
            TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
            ScriptedHook  hook;



            LoadLoop (machine);
            hook.stopAddresses.insert (kProgramStart + 1);
            machine.SetDebugHook (&hook);

            Assert::IsTrue   (machine.StepOne() > 0);
            Assert::AreEqual ((Byte) 0,                    machine.StepOne());
            Assert::AreEqual ((uint16_t) (kProgramStart + 1), GetRegisters (machine).pc);
            Assert::AreEqual ((uint8_t) 1,                 GetRegisters (machine).x);
        }



        TEST_METHOD (RunCycles_StopsMidSlice)
        {
            TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
            ScriptedHook  hook;
            uint64_t      spent = 0;



            LoadLoop (machine);
            hook.stopAddresses.insert (kProgramStart + 2);
            machine.SetDebugHook (&hook);

            spent = machine.RunCycles (kLongRun);

            Assert::IsTrue   (spent < kShortSpent);
            Assert::AreEqual ((uint16_t) (kProgramStart + 2), GetRegisters (machine).pc);
            Assert::AreEqual ((uint8_t) 2,                 GetRegisters (machine).x);
        }



        TEST_METHOD (PendingStop_TakesEffectAtNextBoundary)
        {
            TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
            ScriptedHook  hook;
            uint64_t      spent = 0;



            LoadLoop (machine);
            hook.pendingAt = kProgramStart + 1;
            machine.SetDebugHook (&hook);

            spent = machine.RunCycles (kLongRun);

            Assert::IsTrue   (spent < kShortSpent);
            Assert::AreEqual ((uint16_t) (kProgramStart + 2), GetRegisters (machine).pc);
            Assert::AreEqual ((uint8_t) 2,                 GetRegisters (machine).x);
        }
    };
}
