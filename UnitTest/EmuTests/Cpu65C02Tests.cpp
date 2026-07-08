#include "Pch.h"

#include "Core/Cpu65C02.h"
#include "Core/CpuFactory.h"
#include "Core/MemoryBus.h"
#include "ICpu.h"

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
        Harness () : m_cpu (m_bus)
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
            Cpu6502Registers    regs = m_cpu.GetRegisters ();

            regs.a = a;
            regs.x = x;
            regs.y = y;
            regs.p = p;

            m_cpu.SetRegisters (regs);
        }

        Cpu6502Registers    Regs ()   { return m_cpu.GetRegisters (); }
        Byte                Cycles () { return m_cpu.GetLastInstructionCycles (); }
        Word                PC ()     { return m_cpu.GetPC (); }
        void                Step ()   { m_cpu.StepOne (); }

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
            h.Step ();

            Assert::AreEqual<Byte> (0x80, h.Regs ().a);
            Assert::IsTrue ((h.Regs ().p & kFlagNegative) != 0);
            Assert::AreEqual<Byte> (2, h.Cycles ());
        }


        TEST_METHOD (DecrementAccumulator)
        {
            Harness h;

            h.SetRegs (0x01, 0, 0, 0);
            h.Load ({ 0x3A });          // DEC A
            h.Step ();

            Assert::AreEqual<Byte> (0x00, h.Regs ().a);
            Assert::IsTrue ((h.Regs ().p & kFlagZero) != 0);
        }


        TEST_METHOD (StoreZeroZeroPage)
        {
            Harness h;

            h.Poke (0x0010, 0xAB);
            h.Load ({ 0x64, 0x10 });    // STZ $10
            h.Step ();

            Assert::AreEqual<Byte> (0x00, h.Peek (0x0010));
            Assert::AreEqual<Byte> (3, h.Cycles ());
        }


        TEST_METHOD (BranchAlwaysTaken)
        {
            Harness h;

            h.Load ({ 0x80, 0x04 });    // BRA +4
            h.Step ();

            // PC after the 2-byte instruction is $0202; +4 = $0206.
            Assert::AreEqual<Word> (0x0206, h.PC ());
            Assert::AreEqual<Byte> (3, h.Cycles ());   // 2 base + 1 taken
        }


        TEST_METHOD (PushAndPullX)
        {
            Harness h;

            h.SetRegs (0, 0x5C, 0, 0);
            h.Load ({ 0xDA, 0xA2, 0x00, 0xFA });   // PHX ; LDX #$00 ; PLX
            h.Step ();                              // PHX
            h.Step ();                              // LDX #$00
            Assert::AreEqual<Byte> (0x00, h.Regs ().x);
            h.Step ();                              // PLX
            Assert::AreEqual<Byte> (0x5C, h.Regs ().x);
        }


        TEST_METHOD (TestAndSetBits)
        {
            Harness h;

            h.SetRegs (0x0F, 0, 0, 0);
            h.Poke (0x0020, 0xF0);
            h.Load ({ 0x04, 0x20 });    // TSB $20
            h.Step ();

            Assert::IsTrue ((h.Regs ().p & kFlagZero) != 0);   // 0x0F & 0xF0 == 0
            Assert::AreEqual<Byte> (0xFF, h.Peek (0x0020));    // bits set
        }


        TEST_METHOD (TestAndResetBits)
        {
            Harness h;

            h.SetRegs (0x0F, 0, 0, 0);
            h.Poke (0x0020, 0xFF);
            h.Load ({ 0x14, 0x20 });    // TRB $20
            h.Step ();

            Assert::IsTrue ((h.Regs ().p & kFlagZero) == 0);   // 0x0F & 0xFF != 0
            Assert::AreEqual<Byte> (0xF0, h.Peek (0x0020));    // bits cleared
        }


        TEST_METHOD (LoadAccumulatorZeroPageIndirect)
        {
            Harness h;

            h.Poke (0x0040, 0x00);      // pointer low
            h.Poke (0x0041, 0x03);      // pointer high -> $0300
            h.Poke (0x0300, 0x99);      // target value
            h.Load ({ 0xB2, 0x40 });    // LDA ($40)
            h.Step ();

            Assert::AreEqual<Byte> (0x99, h.Regs ().a);
            Assert::AreEqual<Byte> (5, h.Cycles ());
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
            h.Step ();

            Assert::AreEqual<Word> (0x1234, h.PC ());
            Assert::AreEqual<Byte> (6, h.Cycles ());
        }


        TEST_METHOD (RockwellBitOpcodesAreNopOnBaseTier)
        {
            // The Apple 65C02 is the base CMOS tier -- the Rockwell bit ops
            // (RMB/SMB/BBR/BBS) are not present and decode as single-byte NOPs,
            // matching Apple's shipped parts and AppleWin.
            Byte    bitOpcodes[] = { 0x07, 0x87, 0x0F, 0x8F };   // RMB0, SMB0, BBR0, BBS0

            for (Byte opcode : bitOpcodes)
            {
                Harness h;

                h.SetRegs (0x11, 0x22, 0x33, 0);
                h.Poke (0x0030, 0xFF);
                h.Load ({ opcode, 0x30, 0x10 });
                h.Step ();

                Assert::AreEqual<Word> (0x0201, h.PC ());          // consumed 1 byte
                Assert::AreEqual<Byte> (1, h.Cycles ());
                Assert::AreEqual<Byte> (0xFF, h.Peek (0x0030));    // memory untouched
                Assert::AreEqual<Byte> (0x11, h.Regs ().a);
            }
        }


        TEST_METHOD (DecimalAddSetsFlagsAndExtraCycle)
        {
            Harness h;

            h.SetRegs (0x09, 0, 0, kFlagDecimal);
            h.Load ({ 0x69, 0x01 });    // ADC #$01  (decimal)
            h.Step ();

            Assert::AreEqual<Byte> (0x10, h.Regs ().a);        // BCD 9 + 1 = 10
            Assert::IsTrue ((h.Regs ().p & kFlagZero) == 0);
            Assert::IsTrue ((h.Regs ().p & kFlagNegative) == 0);
            Assert::AreEqual<Byte> (3, h.Cycles ());           // 2 base + 1 decimal
        }


        TEST_METHOD (ReservedOpcodeIsNop)
        {
            Harness h;

            h.SetRegs (0x11, 0x22, 0x33, 0);
            h.Load ({ 0x03 });          // reserved -> 1-byte NOP
            h.Step ();

            Assert::AreEqual<Word> (0x0201, h.PC ());
            Assert::AreEqual<Byte> (0x11, h.Regs ().a);
            Assert::AreEqual<Byte> (1, h.Cycles ());
        }


        TEST_METHOD (FactoryBuilds65C02AndRejectsUnknown)
        {
            MemoryBus                busA;
            MemoryBus                busB;
            std::unique_ptr<ICpu>    cpu = nullptr;
            HRESULT                  hr  = S_OK;

            hr = CpuFactory::Create ("65C02", busA, cpu);
            Assert::AreEqual (S_OK, hr);
            Assert::IsNotNull (cpu.get ());

            hr = CpuFactory::Create ("z80", busB, cpu);
            Assert::IsTrue (FAILED (hr));
        }
    };
}
