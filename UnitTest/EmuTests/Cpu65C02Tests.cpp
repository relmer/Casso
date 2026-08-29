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
    //  The CMOS core: the instructions it ADDS, and the NMOS behaviors it
    //  CHANGES.
    //
    //  The changes matter more than the additions, and they are what this suite
    //  exists for. The 65C02 is not a superset -- it fixes the JMP indirect
    //  page-boundary bug and computes decimal-mode flags after the adjustment
    //  rather than before -- so a shared implementation is necessarily wrong for
    //  one core or the other.
    //
    //  So these assert the divergence in both directions, and the NMOS
    //  regression suite asserts it from the other side. Either alone would let
    //  one core's behavior leak into the other.
    //
    //  The Rockwell bit instructions (RMB/SMB/BBR/BBS) are covered here, since
    //  they exist only on this core and the assembler accepts two operand
    //  syntaxes for them.
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


        ////////////////////////////////////////////////////////////////////////////
        //
        //  IndexedShiftsPayTheCrossingCycleOnlyWhenTheyCross
        //
        //  The NMOS part spends seven cycles on ASL/LSR/ROL/ROR in abs,X however
        //  the address lands; the 65C02 spends six and pays the seventh only on a
        //  real crossing. Both halves are asserted, because a base lowered
        //  without the conditional crossing under-counts every crossing access
        //  and reads exactly like a correct non-crossing one.
        //
        //  INC and DEC in abs,X share the mode and the read-modify-write shape
        //  and are NOT part of the change, so they are asserted alongside: they
        //  are what a fix applied by addressing mode rather than by opcode would
        //  break, and nothing else here would notice.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IndexedShiftsPayTheCrossingCycleOnlyWhenTheyCross)
        {
            static constexpr Byte    kIndexedShifts[4] = { 0x1E, 0x3E, 0x5E, 0x7E };   // ASL, ROL, LSR, ROR
            static constexpr Byte    kIndexedIncDec[2] = { 0xDE, 0xFE };               // DEC, INC

            for (Byte opcode : kIndexedShifts)
            {
                // $0380 + $10 = $0390, same page.
                Harness h;

                h.SetRegs (0, 0x10, 0, 0);
                h.Load ({ opcode, 0x80, 0x03 });
                h.Step();

                Assert::AreEqual<Byte> (6, h.Cycles());
            }

            for (Byte opcode : kIndexedShifts)
            {
                // $03F8 + $10 = $0408, over the page.
                Harness h;

                h.SetRegs (0, 0x10, 0, 0);
                h.Load ({ opcode, 0xF8, 0x03 });
                h.Step();

                Assert::AreEqual<Byte> (7, h.Cycles());
            }

            for (Byte opcode : kIndexedIncDec)
            {
                // Seven either way, on both cores.
                Harness  same;
                Harness  crossing;

                same.SetRegs (0, 0x10, 0, 0);
                same.Load ({ opcode, 0x80, 0x03 });
                same.Step();

                crossing.SetRegs (0, 0x10, 0, 0);
                crossing.Load ({ opcode, 0xF8, 0x03 });
                crossing.Step();

                Assert::AreEqual<Byte> (7, same.Cycles());
                Assert::AreEqual<Byte> (7, crossing.Cycles());
            }
        }


        ////////////////////////////////////////////////////////////////////////////
        //
        //  BitBranchesAreTimedLikeBranches
        //
        //  BBRn/BBSn were billed a flat five however they resolved. They are
        //  branches: five when the bit test fails, six when the branch is taken,
        //  seven when it is taken to a different page. Sources are cited at
        //  CpuOperations::BitBranchReset.
        //
        //  All three outcomes are asserted, because five is the right answer for
        //  the not-taken case and six for a taken branch inside the page -- a
        //  test that checked only one of them would be satisfied by a core that
        //  charges the same amount for everything, which is the bug.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitBranchesAreTimedLikeBranches)
        {
            Harness    notTaken;
            Harness    takenInPage;
            Harness    takenAcrossPage;

            notTaken.Poke (0x0030, 0x01);                   // bit 0 set: BBR0 does not branch
            notTaken.Load ({ 0x0F, 0x30, 0x10 });
            notTaken.Step();

            takenInPage.Poke (0x0030, 0xFE);                // bit 0 clear: BBR0 branches
            takenInPage.Load ({ 0x0F, 0x30, 0x10 });        // $0203 + $10 = $0213
            takenInPage.Step();

            takenAcrossPage.Poke (0x0030, 0xFE);
            takenAcrossPage.Load ({ 0x0F, 0x30, 0x80 });    // $0203 - $80 = $0183, over the page
            takenAcrossPage.Step();

            Assert::AreEqual<Byte> (5, notTaken.Cycles());
            Assert::AreEqual<Byte> (6, takenInPage.Cycles());
            Assert::AreEqual<Byte> (7, takenAcrossPage.Cycles());

            Assert::AreEqual<Word> (0x0203, notTaken.PC());
            Assert::AreEqual<Word> (0x0213, takenInPage.PC());
            Assert::AreEqual<Word> (0x0183, takenAcrossPage.PC());
        }


        ////////////////////////////////////////////////////////////////////////////
        //
        //  BranchAlwaysToTheFollowingInstructionStillCostsThree
        //
        //  BRA with a displacement of zero targets the instruction after it, so
        //  PC lands where it would have landed anyway. It is still a taken
        //  branch and still costs three; the core used to answer two, having
        //  decided takenness by watching PC.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BranchAlwaysToTheFollowingInstructionStillCostsThree)
        {
            Harness    h;

            h.Load ({ 0x80, 0x00 });   // BRA to $0202, the next instruction

            h.Step();

            Assert::AreEqual<Byte> (3,      h.Cycles());
            Assert::AreEqual<Word> (0x0202, h.PC());
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
            AssertFailed (hr);
        }
    };
}
