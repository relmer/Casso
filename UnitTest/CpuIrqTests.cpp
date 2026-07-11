#include "Pch.h"

#include "Cpu6502.h"
#include "ICpu.h"
#include "TestHelpers.h"
#include "Core/InterruptController.h"
#include "Devices/Mockingboard/Via6522.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuIrqTests
//
//  Drives Cpu6502's IRQ/NMI vectored-dispatch logic exclusively through the
//  CPU-family-agnostic ICpu::SetInterruptLine surface plus ICpu::Step.
//  All scenarios honor constitution §II Test Isolation by setting up CPU
//  state via TestCpu::InitForTest — no host filesystem, registry, or
//  network access.
//
//  Vectors:
//    $FFFE/$FFFF — IRQ
//    $FFFA/$FFFB — NMI
//
////////////////////////////////////////////////////////////////////////////////

namespace Apple2eFidelity
{
    static constexpr Word   kIrqHandler = 0x9000;
    static constexpr Word   kNmiHandler = 0xA000;
    static constexpr Word   kStartPc    = 0x8000;


    static void SeedVectors (TestCpu & cpu)
    {
        cpu.PokeWord (0xFFFA, kNmiHandler);
        cpu.PokeWord (0xFFFC, kStartPc);
        cpu.PokeWord (0xFFFE, kIrqHandler);
    }


    TEST_CLASS (CpuIrqTests)
    {
    public:
        TEST_METHOD (AssertedIrqWithIClearDispatchesViaFFFE)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            uint32_t    cycles = 0;
            HRESULT     hr     = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Status ().flags.interruptDisable = 0;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (kIrqHandler, cpu.RegPC (),
                              L"PC should be loaded from $FFFE/$FFFF");
            Assert::AreEqual (static_cast<uint32_t> (7), cycles,
                              L"IRQ dispatch should consume 7 cycles");
        }


        TEST_METHOD (AssertedIrqWithISetIsIgnored)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            uint32_t    cycles = 0;
            HRESULT     hr     = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Poke (kStartPc, 0xEA);  // NOP
            cpu.Status ().flags.interruptDisable = 1;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (static_cast<Word> (kStartPc + 1), cpu.RegPC (),
                              L"With I=1, IRQ must be ignored and NOP must run");
        }


        TEST_METHOD (IrqPushesCorrectPCAndStatus)
        {
            TestCpu     cpu;
            ICpu      & iface     = cpu;
            uint32_t    cycles    = 0;
            Byte        spBefore  = 0;
            Byte        pushedHi  = 0;
            Byte        pushedLo  = 0;
            Byte        pushedP   = 0;
            HRESULT     hr        = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);

            cpu.RegSP () = 0xFF;
            cpu.Status ().status               = 0;
            cpu.Status ().flags.alwaysOne      = 1;
            cpu.Status ().flags.carry          = 1;
            cpu.Status ().flags.zero           = 1;
            cpu.Status ().flags.interruptDisable = 0;

            spBefore = cpu.RegSP ();

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            // Stack frame: PCH at $01FF, PCL at $01FE, P at $01FD; SP=$FC.
            pushedHi = cpu.Peek (0x0100 + spBefore);
            pushedLo = cpu.Peek (0x0100 + spBefore - 1);
            pushedP  = cpu.Peek (0x0100 + spBefore - 2);

            Assert::AreEqual (static_cast<Byte> (kStartPc >> 8), pushedHi,
                              L"PCH must be pushed first");
            Assert::AreEqual (static_cast<Byte> (kStartPc & 0xFF), pushedLo,
                              L"PCL must be pushed second");
            Assert::AreEqual (static_cast<Byte> (spBefore - 3), cpu.RegSP (),
                              L"SP should drop by 3 (PCH + PCL + P)");

            // B (0x10) cleared on hardware IRQ; U (0x20) always set on push;
            // C (0x01) and Z (0x02) preserved from before.
            Assert::AreEqual (static_cast<Byte> (0x00), static_cast<Byte> (pushedP & 0x10),
                              L"B flag must be CLEAR on hardware IRQ");
            Assert::AreEqual (static_cast<Byte> (0x20), static_cast<Byte> (pushedP & 0x20),
                              L"U flag must be SET on pushed status");
            Assert::AreEqual (static_cast<Byte> (0x01), static_cast<Byte> (pushedP & 0x01),
                              L"Carry preserved on pushed status");
            Assert::AreEqual (static_cast<Byte> (0x02), static_cast<Byte> (pushedP & 0x02),
                              L"Zero preserved on pushed status");
        }


        TEST_METHOD (IrqSetsIFlagOnEntry)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            uint32_t    cycles = 0;
            HRESULT     hr     = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Status ().flags.interruptDisable = 0;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (static_cast<Byte> (1),
                              static_cast<Byte> (cpu.Status ().flags.interruptDisable),
                              L"I flag must be set after IRQ entry");
        }


        TEST_METHOD (ClearIrqBeforeDispatchIsNoop)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            uint32_t    cycles = 0;
            HRESULT     hr     = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Poke (kStartPc, 0xEA);  // NOP
            cpu.Status ().flags.interruptDisable = 0;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);
            iface.SetInterruptLine (CpuInterruptKind::kMaskable, false);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (static_cast<Word> (kStartPc + 1), cpu.RegPC (),
                              L"Cleared IRQ must not dispatch — NOP must execute");
        }


        TEST_METHOD (NmiEdgeDispatchesViaFFFAEvenWithISet)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            uint32_t    cycles = 0;
            HRESULT     hr     = S_OK;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Status ().flags.interruptDisable = 1;

            iface.SetInterruptLine (CpuInterruptKind::kNonMaskable, true);

            hr = iface.Step (cycles);
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (kNmiHandler, cpu.RegPC (),
                              L"NMI must dispatch via $FFFA regardless of I flag");
            Assert::AreEqual (static_cast<uint32_t> (7), cycles,
                              L"NMI dispatch should consume 7 cycles");
        }


        // -------------------------------------------------------------------
        // Host-loop dispatch (TryStepInterrupt + StepOne).
        //
        // The shell run loop drives the CPU with StepOne -- which does NOT
        // check interrupts -- gated on TryStepInterrupt. These tests exercise
        // that gating directly. Without it the loop silently dropped every
        // maskable IRQ, the regression that hid until the Mockingboard became
        // the first device in Casso to assert one.
        // -------------------------------------------------------------------

        TEST_METHOD (HostStepLoopServicesAssertedIrq)
        {
            TestCpu     cpu;
            ICpu      & iface   = cpu;
            bool        tookIrq = false;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Poke (kStartPc, 0xEA);   // NOP -- must NOT run; IRQ pre-empts it
            cpu.Status ().flags.interruptDisable = 0;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            // One iteration of the shell's exact stepping pattern.
            tookIrq = cpu.TryStepInterrupt ();
            if (!tookIrq)
            {
                cpu.StepOne ();
            }

            Assert::IsTrue (tookIrq,
                            L"Host step loop must dispatch the asserted IRQ");
            Assert::AreEqual (kIrqHandler, cpu.RegPC (),
                              L"PC must vector to the IRQ handler via the StepOne loop");
            Assert::AreEqual (static_cast<Byte> (7), cpu.GetLastInstructionCycles (),
                              L"Interrupt step must report 7 cycles for host cycle accounting");
        }


        TEST_METHOD (HostStepLoopHonorsIFlag)
        {
            TestCpu     cpu;
            ICpu      & iface   = cpu;
            bool        tookIrq = false;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Poke (kStartPc, 0xEA);   // NOP -- must run because I=1 masks the IRQ
            cpu.Status ().flags.interruptDisable = 1;

            iface.SetInterruptLine (CpuInterruptKind::kMaskable, true);

            tookIrq = cpu.TryStepInterrupt ();
            if (!tookIrq)
            {
                cpu.StepOne ();
            }

            Assert::IsFalse (tookIrq, L"IRQ must not dispatch while I=1");
            Assert::AreEqual (static_cast<Word> (kStartPc + 1), cpu.RegPC (),
                              L"With I=1 the NOP must run instead of vectoring");
        }


        TEST_METHOD (HostStepLoopServicesNmiEdge)
        {
            TestCpu     cpu;
            ICpu      & iface  = cpu;
            bool        took   = false;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            cpu.Status ().flags.interruptDisable = 1;   // NMI ignores I

            iface.SetInterruptLine (CpuInterruptKind::kNonMaskable, true);

            took = cpu.TryStepInterrupt ();
            if (!took)
            {
                cpu.StepOne ();
            }

            Assert::IsTrue (took, L"Host step loop must dispatch the NMI edge");
            Assert::AreEqual (kNmiHandler, cpu.RegPC (),
                              L"NMI must vector via $FFFA through the StepOne loop");
        }


        TEST_METHOD (Via6522Timer1IrqReachesCpuThroughHostLoop)
        {
            // End-to-end reproduction of the Rescue Raiders "won't start" hang:
            // a Mockingboard VIA Timer1 IRQ must actually vector the CPU when
            // the shell drives it with the StepOne loop. Wires
            // Via6522 -> InterruptController -> 6502 exactly as MachineManager
            // does, then runs the host loop until (or fails to reach) the
            // handler.
            TestCpu               cpu;
            InterruptController   ic  (&cpu);
            Via6522               via;
            bool                  reachedHandler = false;
            Word                  a              = 0;
            int                   i              = 0;



            cpu.InitForTest (kStartPc);
            SeedVectors (cpu);
            for (a = kStartPc; a < kStartPc + 64; ++a)
            {
                cpu.Poke (a, 0xEA);      // NOP field so StepOne makes progress
            }
            cpu.Status ().flags.interruptDisable = 0;

            Assert::AreEqual (S_OK, via.AttachInterruptController (&ic),
                              L"VIA must bind to the interrupt controller");

            // Timer1 continuous mode, T1 IRQ enabled, short latch so it
            // underflows within a handful of NOPs.
            via.WriteRegister (Via6522::kRegAcr,  Via6522::kAcrT1Continuous);
            via.WriteRegister (Via6522::kRegIer,  Via6522::kIerSetClear | Via6522::kIrqTimer1);
            via.WriteRegister (Via6522::kRegT1LL, 0x10);
            via.WriteRegister (Via6522::kRegT1CH, 0x00);   // load+arm, count = 0x0010

            for (i = 0; i < 200 && !reachedHandler; ++i)
            {
                if (!cpu.TryStepInterrupt ())
                {
                    cpu.StepOne ();
                }

                via.Tick (cpu.GetLastInstructionCycles ());

                if (cpu.RegPC () == kIrqHandler)
                {
                    reachedHandler = true;
                }
            }

            Assert::IsTrue (reachedHandler,
                            L"Timer1 IRQ must vector the CPU to its handler via the host StepOne loop");
        }
    };
}
