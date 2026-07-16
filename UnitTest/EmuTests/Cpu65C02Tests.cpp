#include "Pch.h"

#include "Core/Cpu65C02.h"
#include "Core/CpuFactory.h"
#include "Core/MemoryBus.h"
#include "ICpu.h"

// Each Harness embeds a MemoryBus (64K RAM array) + a 65C02; several are
// stack-allocated per TEST_METHOD (one per scoped sub-case), which sums past
// the C6262 /analyze stack-frame budget. Matches the per-file suppression the
// sibling device/CPU test files already use (DeviceTests, Disk2Tests, ...).
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Cpu65C02TestNs
{
    static constexpr Byte    kFlagCarry    = 0x01;
    static constexpr Byte    kFlagZero     = 0x02;
    static constexpr Byte    kFlagDecimal  = 0x08;
    static constexpr Byte    kFlagNegative = 0x80;

    static constexpr Word    kProgram = 0x0200;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Harness
    //
    //  A Cpu65C02 bound to a bare MemoryBus. Programs and data live below
    //  $C000, which MemoryBusCpu backs with the CPU's own RAM, so PokeByte and
    //  the bus-routed fetch see the same bytes. Deterministic; no I/O.
    //
    ////////////////////////////////////////////////////////////////////////////

    class Harness
    {
    public:
        Harness() : m_cpu (m_bus)
        {
            SetRegs (0, 0, 0, 0);
            m_cpu.SetPC (kProgram);
        }

        void    Load (std::initializer_list<Byte> bytes)
        {
            Word addr = kProgram;

            for (Byte b : bytes)
            {
                m_cpu.PokeByte (addr++, b);
            }
        }

        void    Poke (Word addr, Byte value) { m_cpu.PokeByte (addr, value); }
        Byte    Peek (Word addr)             { return m_cpu.PeekByte (addr); }

        void    SetRegs (Byte a, Byte x, Byte y, Byte p)
        {
            Cpu6502Registers    regs = m_cpu.GetRegisters();

            regs.a = a;
            regs.x = x;
            regs.y = y;
            regs.p = p;

            m_cpu.SetRegisters (regs);
        }

        Cpu6502Registers    Regs()   { return m_cpu.GetRegisters(); }
        Byte                Cycles() { return m_cpu.GetLastInstructionCycles(); }
        Word                PC()     { return m_cpu.GetPC(); }
        void                Step()   { m_cpu.StepOne(); }

    private:
        MemoryBus    m_bus;
        Cpu65C02     m_cpu;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Cpu65C02Tests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Cpu65C02Tests)
    {
    public:
        TEST_METHOD (IncrementAccumulator)
        {
            Harness h;

            h.SetRegs (0x7F, 0, 0, 0);
            h.Load ({ 0x1A });          // INC A
            h.Step();

            Assert::AreEqual<Byte> (0x80, h.Regs().a);
            Assert::IsTrue ((h.Regs().p & kFlagNegative) != 0);
            Assert::AreEqual<Byte> (2, h.Cycles());
        }


        TEST_METHOD (DecrementAccumulator)
        {
            Harness h;

            h.SetRegs (0x01, 0, 0, 0);
            h.Load ({ 0x3A });          // DEC A
            h.Step();

            Assert::AreEqual<Byte> (0x00, h.Regs().a);
            Assert::IsTrue ((h.Regs().p & kFlagZero) != 0);
        }


        TEST_METHOD (StoreZeroZeroPage)
        {
            Harness h;

            h.Poke (0x0010, 0xAB);
            h.Load ({ 0x64, 0x10 });    // STZ $10
            h.Step();

            Assert::AreEqual<Byte> (0x00, h.Peek (0x0010));
            Assert::AreEqual<Byte> (3, h.Cycles());
        }


        TEST_METHOD (BranchAlwaysTaken)
        {
            Harness h;

            h.Load ({ 0x80, 0x04 });    // BRA +4
            h.Step();

            // PC after the 2-byte instruction is $0202; +4 = $0206.
            Assert::AreEqual<Word> (0x0206, h.PC());
            Assert::AreEqual<Byte> (3, h.Cycles());   // 2 base + 1 taken
        }


        TEST_METHOD (PushAndPullX)
        {
            Harness h;

            h.SetRegs (0, 0x5C, 0, 0);
            h.Load ({ 0xDA, 0xA2, 0x00, 0xFA });   // PHX ; LDX #$00 ; PLX
            h.Step();                              // PHX
            h.Step();                              // LDX #$00
            Assert::AreEqual<Byte> (0x00, h.Regs().x);
            h.Step();                              // PLX
            Assert::AreEqual<Byte> (0x5C, h.Regs().x);
        }


        TEST_METHOD (TestAndSetBits)
        {
            Harness h;

            h.SetRegs (0x0F, 0, 0, 0);
            h.Poke (0x0020, 0xF0);
            h.Load ({ 0x04, 0x20 });    // TSB $20
            h.Step();

            Assert::IsTrue ((h.Regs().p & kFlagZero) != 0);   // 0x0F & 0xF0 == 0
            Assert::AreEqual<Byte> (0xFF, h.Peek (0x0020));    // bits set
        }


        TEST_METHOD (TestAndResetBits)
        {
            Harness h;

            h.SetRegs (0x0F, 0, 0, 0);
            h.Poke (0x0020, 0xFF);
            h.Load ({ 0x14, 0x20 });    // TRB $20
            h.Step();

            Assert::IsTrue ((h.Regs().p & kFlagZero) == 0);   // 0x0F & 0xFF != 0
            Assert::AreEqual<Byte> (0xF0, h.Peek (0x0020));    // bits cleared
        }


        TEST_METHOD (LoadAccumulatorZeroPageIndirect)
        {
            Harness h;

            h.Poke (0x0040, 0x00);      // pointer low
            h.Poke (0x0041, 0x03);      // pointer high -> $0300
            h.Poke (0x0300, 0x99);      // target value
            h.Load ({ 0xB2, 0x40 });    // LDA ($40)
            h.Step();

            Assert::AreEqual<Byte> (0x99, h.Regs().a);
            Assert::AreEqual<Byte> (5, h.Cycles());
        }


        TEST_METHOD (JumpIndirectPageBoundaryFixed)
        {
            Harness h;

            // Pointer straddles a page boundary at $02FF/$0300. The NMOS bug
            // would read the high byte from $0200; the 65C02 reads $0300.
            h.Poke (0x02FF, 0x34);      // target low
            h.Poke (0x0300, 0x12);      // target high (correct)
            h.Poke (0x0200, 0xAA);      // NMOS would read high from here
            h.SetRegs (0, 0, 0, 0);
            h.Load ({ });               // (program bytes placed explicitly below)
            h.Poke (0x0200, 0x6C);      // JMP ($02FF)  -- note $0200 overwritten
            h.Poke (0x0201, 0xFF);
            h.Poke (0x0202, 0x02);
            h.Step();

            Assert::AreEqual<Word> (0x1234, h.PC());
            Assert::AreEqual<Byte> (6, h.Cycles());
        }


        TEST_METHOD (RockwellBitOpsExecute)
        {
            // Casso models the Rockwell R65C02: RMB/SMB/BBR/BBS are real
            // instructions (Apple's //c ROM 4 and Enhanced //e firmware use
            // them). Dormann's rkwl_op suite exercises them exhaustively; here
            // we assert the core behaviors and encodings.

            // RMB0 ($07 zp): clear bit 0 of the zp byte (RMW). 2 bytes, 5 cycles.
            {
                Harness h;
                h.Poke (0x0030, 0xFF);
                h.Load ({ 0x07, 0x30 });
                h.Step();
                Assert::AreEqual<Word> (0x0202, h.PC());
                Assert::AreEqual<Byte> (5,      h.Cycles());
                Assert::AreEqual<Byte> (0xFE,   h.Peek (0x0030));
            }

            // SMB7 ($F7 zp): set bit 7 of the zp byte.
            {
                Harness h;
                h.Poke (0x0030, 0x00);
                h.Load ({ 0xF7, 0x30 });
                h.Step();
                Assert::AreEqual<Word> (0x0202, h.PC());
                Assert::AreEqual<Byte> (0x80,   h.Peek (0x0030));
            }

            // BBR0 ($0F zp,rel): branch if bit 0 clear. 3 bytes.
            {
                Harness h;                                  // taken: bit 0 clear
                h.Poke (0x0030, 0xFE);
                h.Load ({ 0x0F, 0x30, 0x10 });
                h.Step();
                Assert::AreEqual<Word> (0x0213, h.PC());   // $0203 + $10
            }
            {
                Harness h;                                  // not taken: bit 0 set
                h.Poke (0x0030, 0x01);
                h.Load ({ 0x0F, 0x30, 0x10 });
                h.Step();
                Assert::AreEqual<Word> (0x0203, h.PC());
            }

            // BBS0 ($8F zp,rel): branch if bit 0 set.
            {
                Harness h;                                  // taken: bit 0 set
                h.Poke (0x0030, 0x01);
                h.Load ({ 0x8F, 0x30, 0x10 });
                h.Step();
                Assert::AreEqual<Word> (0x0213, h.PC());
            }
        }


        TEST_METHOD (WdcWaiStpDecodeAsNop)
        {
            // WDC's WAI/STP ($CB/$DB) are NOT on the Rockwell parts Apple
            // shipped, so they remain single-byte, single-cycle NOPs.
            for (Byte opcode : { 0xCB, 0xDB })
            {
                Harness h;

                h.SetRegs (0x11, 0x22, 0x33, 0);
                h.Load ({ opcode, 0xEA });
                h.Step();

                Assert::AreEqual<Word> (0x0201, h.PC());          // consumed 1 byte
                Assert::AreEqual<Byte> (1,      h.Cycles());      // 1 cycle
                Assert::AreEqual<Byte> (0x11,   h.Regs().a);      // A untouched
            }
        }


        TEST_METHOD (DecimalAddSetsFlagsAndExtraCycle)
        {
            Harness h;

            h.SetRegs (0x09, 0, 0, kFlagDecimal);
            h.Load ({ 0x69, 0x01 });    // ADC #$01  (decimal)
            h.Step();

            Assert::AreEqual<Byte> (0x10, h.Regs().a);        // BCD 9 + 1 = 10
            Assert::IsTrue ((h.Regs().p & kFlagZero) == 0);
            Assert::IsTrue ((h.Regs().p & kFlagNegative) == 0);
            Assert::AreEqual<Byte> (3, h.Cycles());           // 2 base + 1 decimal
        }


        TEST_METHOD (ReservedOpcodeIsNop)
        {
            Harness h;

            h.SetRegs (0x11, 0x22, 0x33, 0);
            h.Load ({ 0x03 });          // reserved -> 1-byte NOP
            h.Step();

            Assert::AreEqual<Word> (0x0201, h.PC());
            Assert::AreEqual<Byte> (0x11, h.Regs().a);
            Assert::AreEqual<Byte> (1, h.Cycles());
        }


        TEST_METHOD (FactoryBuilds65C02AndRejectsUnknown)
        {
            MemoryBus                busA;
            MemoryBus                busB;
            std::unique_ptr<ICpu>    cpu = nullptr;
            HRESULT                  hr  = S_OK;

            hr = CpuFactory::Create ("65C02", busA, cpu);
            Assert::AreEqual (S_OK, hr);
            Assert::IsNotNull (cpu.get());

            hr = CpuFactory::Create ("z80", busB, cpu);
            Assert::IsTrue (FAILED (hr));
        }
    };
}
