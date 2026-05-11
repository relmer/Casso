#include "Pch.h"

#include "CppUnitTest.h"
#include "Cpu6502.h"
#include "ICpu.h"
#include "I6502DebugInfo.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Cpu6502InterfaceTests
//
//  Confirms Cpu6502 satisfies both ICpu (CPU-family-agnostic strategy)
//  and I6502DebugInfo (6502-family register inspection) and that the
//  two interfaces compose correctly on a single instance.
//
////////////////////////////////////////////////////////////////////////////////

namespace Apple2eFidelity
{
    TEST_CLASS (Cpu6502InterfaceTests)
    {
    public:
        TEST_METHOD (ImplementsICpu)
        {
            Cpu6502     cpu;
            ICpu      * iface = &cpu;



            Assert::IsNotNull (iface, L"Cpu6502 must implement ICpu");
        }


        TEST_METHOD (ImplementsI6502DebugInfo)
        {
            Cpu6502             cpu;
            I6502DebugInfo    * iface = &cpu;



            Assert::IsNotNull (iface, L"Cpu6502 must implement I6502DebugInfo");
        }


        TEST_METHOD (RegistersRoundTripViaI6502DebugInfo)
        {
            TestCpu             cpu;
            Cpu6502Registers    expected = {};
            Cpu6502Registers    actual   = {};



            cpu.InitForTest ();

            expected.pc = 0x1234;
            expected.a  = 0x55;
            expected.x  = 0x66;
            expected.y  = 0x77;
            expected.sp = 0x88;
            expected.p  = 0x21;

            static_cast<I6502DebugInfo &> (cpu).SetRegisters (expected);

            actual = static_cast<I6502DebugInfo &> (cpu).GetRegisters ();

            Assert::AreEqual (expected.pc, actual.pc, L"PC round-trip");
            Assert::AreEqual (expected.a,  actual.a,  L"A round-trip");
            Assert::AreEqual (expected.x,  actual.x,  L"X round-trip");
            Assert::AreEqual (expected.y,  actual.y,  L"Y round-trip");
            Assert::AreEqual (expected.sp, actual.sp, L"SP round-trip");
            Assert::AreEqual (expected.p,  actual.p,  L"P round-trip");
        }


        TEST_METHOD (ResetClearsCycleCount)
        {
            TestCpu     cpu;
            ICpu      & iface = cpu;
            HRESULT     hr    = S_OK;



            cpu.InitForTest ();

            // Drive a couple of NOPs through ICpu::Step to accumulate cycles.
            cpu.Poke (0x8000, 0xEA);
            cpu.Poke (0x8001, 0xEA);

            uint32_t    cycles = 0;
            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);
            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            Assert::IsTrue (iface.GetCycleCount () > 0,
                            L"Step should accumulate cycles");

            hr = iface.Reset ();
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (static_cast<uint64_t> (0), iface.GetCycleCount (),
                              L"Reset should zero the cycle counter");
        }


        TEST_METHOD (SetInterruptLineLatchesIrqLevel)
        {
            Cpu6502     cpu;
            ICpu      & iface = cpu;



            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);
            Assert::IsTrue (cpu.IsIrqLineAsserted ());

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, false);
            Assert::IsFalse (cpu.IsIrqLineAsserted ());
        }


        TEST_METHOD (SetInterruptLineLatchesNmiEdge)
        {
            Cpu6502     cpu;
            ICpu      & iface = cpu;



            // Rising edge latches a pending dispatch.
            iface.SetInterruptLine (CpuInterruptKind::kNonMaskable, true);
            Assert::IsTrue (cpu.IsNmiLineAsserted ());
            Assert::IsTrue (cpu.IsNmiPending ());

            // Falling edge does not clear the pending latch.
            iface.SetInterruptLine (CpuInterruptKind::kNonMaskable, false);
            Assert::IsFalse (cpu.IsNmiLineAsserted ());
            Assert::IsTrue (cpu.IsNmiPending ());
        }
    };
}
