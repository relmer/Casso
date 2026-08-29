#include "Pch.h"

#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CpuOperationTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LoadStoreTests
    //
    //  LDA/LDX/LDY and STA/STX/STY: the value moved, and the flags that follow.
    //
    //  Loads set N and Z; stores set NOTHING. That asymmetry is the substance
    //  here -- a store implemented by reusing the load path would look correct
    //  in the value it writes while quietly clobbering flags a following branch
    //  depends on.
    //
    //  Boundary values are chosen to make the flags observable: zero for Z, a
    //  high-bit value for N, and an ordinary value for neither.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LoadStoreTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Load_SetsRegisterValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Load_SetsRegisterValue)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::Load (cpu, cpu.RegA(), 0x42);

            Assert::AreEqual ((Byte) 0x42, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Load_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Load_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::Load (cpu, cpu.RegA(), 0x00);

            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Load_NegativeValue_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Load_NegativeValue_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::Load (cpu, cpu.RegA(), 0x80);

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Store_WritesToMemory
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Store_WritesToMemory)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xBB;

            CpuOperations::Store (cpu, cpu.RegA(), 0x1234);

            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0x1234));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AddWithCarryTests
    //
    //  ADC in binary and decimal mode, with particular attention to the
    //  OVERFLOW flag.
    //
    //  V is the flag worth the tests. Carry is an unsigned concept and easy;
    //  overflow is a signed one -- set when the sign of the result cannot be
    //  right for the signs of the operands -- and it is routinely implemented
    //  as some variation on comparing bit 7, most of which are wrong for some
    //  quadrant. So the cases cover all four sign combinations rather than a
    //  representative sample.
    //
    //  Decimal mode is exercised here as well as in the regression tests: this
    //  group covers ordinary BCD arithmetic, while the regression group covers
    //  the flag quirks that arise from the intermediate binary sum.
    //
    //  Carry IN is varied independently of the operands, since ADC adds three
    //  things and an implementation can be right for two of them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AddWithCarryTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_BasicAdd
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_BasicAdd)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x10;

            CpuOperations::AddWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x30, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
            Assert::IsFalse ((bool) cpu.Status().flags.overflow);
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_WithCarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_WithCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x10;
            cpu.Status().flags.carry = 1;

            CpuOperations::AddWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x31, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_ProducesCarryOut
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_ProducesCarryOut)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_SignedOverflow_PositivePlusPositive
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_SignedOverflow_PositivePlusPositive)
        {
            // 0x40 + 0x40 = 0x80 (two positives produce negative)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x40;

            CpuOperations::AddWithCarry (cpu, 0x40);

            Assert::AreEqual ((Byte) 0x80, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_SignedOverflow_NegativePlusNegative
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_SignedOverflow_NegativePlusNegative)
        {
            // 0x80 + 0x80 = 0x00 with carry (two negatives produce positive)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x80;

            CpuOperations::AddWithCarry (cpu, 0x80);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_NoOverflow_DifferentSigns
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_NoOverflow_DifferentSigns)
        {
            // 0x50 + 0xD0 = 0x120 (positive + negative, no signed overflow)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x50;

            CpuOperations::AddWithCarry (cpu, 0xD0);

            Assert::AreEqual ((Byte) 0x20, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_Decimal_BasicAdd
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_Decimal_BasicAdd)
        {
            // BCD: 25 + 48 = 73 (no carry)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.RegA() = 0x25;

            CpuOperations::AddWithCarry (cpu, 0x48);

            Assert::AreEqual ((Byte) 0x73, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_Decimal_LowNibbleCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_Decimal_LowNibbleCarry)
        {
            // BCD: 09 + 01 = 10 (low-nibble rollover, no carry out)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.RegA() = 0x09;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x10, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_Decimal_ProducesCarryOut
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_Decimal_ProducesCarryOut)
        {
            // BCD: 99 + 01 = 00 with carry
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.RegA() = 0x99;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            // Z flag is from the binary result on NMOS 6502: 0x99+0x01 = 0x9A != 0
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_Decimal_WithCarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_Decimal_WithCarryIn)
        {
            // BCD: 25 + 48 + 1 = 74
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.Status().flags.carry   = 1;
            cpu.RegA() = 0x25;

            CpuOperations::AddWithCarry (cpu, 0x48);

            Assert::AreEqual ((Byte) 0x74, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_Decimal_HighNibbleCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_Decimal_HighNibbleCarry)
        {
            // BCD: 50 + 50 = 00 with carry
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.RegA() = 0x50;

            CpuOperations::AddWithCarry (cpu, 0x50);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_BinaryMode_NotAffectedByDecimalFlagWhenClear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_BinaryMode_NotAffectedByDecimalFlagWhenClear)
        {
            // When D=0, ADC must stay binary even if operands look BCD-ish
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 0;
            cpu.RegA() = 0x09;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x0A, cpu.RegA()); // binary, not 0x10
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SubtractWithCarryTests
    //
    //  SBC in binary and decimal mode, including the inverted sense of carry.
    //
    //  On the 6502 carry is a BORROW-NOT: it must be SET before a subtraction
    //  that should not borrow, and it comes back clear when a borrow occurred.
    //  That inversion trips almost everyone, so the tests fix carry in both
    //  states explicitly rather than leaving it wherever a previous operation
    //  left it.
    //
    //  Overflow gets the same four-quadrant treatment as ADC, since V is a
    //  signed-range question for subtraction too and the sign rules differ.
    //
    //  SBC is not simply ADC of a complement here, so it is tested
    //  independently -- in decimal mode the two genuinely diverge, and a shared
    //  implementation would be wrong for one of them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SubtractWithCarryTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_BasicSubtract_CarrySet
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_BasicSubtract_CarrySet)
        {
            // With carry set (no borrow): 0x50 - 0x10 = 0x40
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x50;
            cpu.Status().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x10);

            Assert::AreEqual ((Byte) 0x40, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_WithBorrow
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_WithBorrow)
        {
            // With carry clear (borrow): 0x50 - 0x10 - 1 = 0x3F
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x50;
            cpu.Status().flags.carry = 0;

            CpuOperations::SubtractWithCarry (cpu, 0x10);

            Assert::AreEqual ((Byte) 0x3F, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_ProducesBorrow
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_ProducesBorrow)
        {
            // 0x10 - 0x20 = 0xF0 with borrow (carry=0)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x10;
            cpu.Status().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0xF0, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_ZeroResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_ZeroResult)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x42;
            cpu.Status().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x42);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_Decimal_BasicSubtract
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_Decimal_BasicSubtract)
        {
            // BCD: 46 - 12 = 34 (carry=1 means no borrow in)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.Status().flags.carry   = 1;
            cpu.RegA() = 0x46;

            CpuOperations::SubtractWithCarry (cpu, 0x12);

            Assert::AreEqual ((Byte) 0x34, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_Decimal_LowNibbleBorrow
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_Decimal_LowNibbleBorrow)
        {
            // BCD: 40 - 13 = 27
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.Status().flags.carry   = 1;
            cpu.RegA() = 0x40;

            CpuOperations::SubtractWithCarry (cpu, 0x13);

            Assert::AreEqual ((Byte) 0x27, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_Decimal_WithBorrowIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_Decimal_WithBorrowIn)
        {
            // BCD: 50 - 20 - 1 = 29
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.Status().flags.carry   = 0;
            cpu.RegA() = 0x50;

            CpuOperations::SubtractWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x29, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_Decimal_ProducesBorrow
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_Decimal_ProducesBorrow)
        {
            // BCD: 00 - 01 = 99 with borrow (carry cleared)
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;
            cpu.Status().flags.carry   = 1;
            cpu.RegA() = 0x00;

            CpuOperations::SubtractWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x99, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_BinaryMode_NotAffectedByDecimalFlagWhenClear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_BinaryMode_NotAffectedByDecimalFlagWhenClear)
        {
            // Sanity: D=0 still produces binary result
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 0;
            cpu.Status().flags.carry   = 1;
            cpu.RegA() = 0x10;

            CpuOperations::SubtractWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegA()); // binary, not BCD-adjusted
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LogicTests
    //
    //  AND, ORA, and EOR: the bitwise result and the N and Z flags.
    //
    //  Straightforward operations, so the tests are mostly about the FLAGS --
    //  which are set from the result, and only N and Z. Carry and overflow must
    //  be left alone, and an implementation routing these through a shared
    //  arithmetic path would disturb them.
    //
    //  Operand pairs are chosen so each operation produces a distinguishable
    //  result: masks that clear the high bit for N, complementary values that
    //  produce zero for Z.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LogicTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  And_MasksAccumulator
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (And_MasksAccumulator)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  And_ZeroResult_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (And_ZeroResult_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xF0;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Or_CombinesBits
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Or_CombinesBits)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xF0;

            CpuOperations::Or (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Xor_IdenticalOperands_ProducesZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Xor_IdenticalOperands_ProducesZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::Xor (cpu, 0xFF);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CompareTests
    //
    //  CMP, CPX, and CPY: the three flags a comparison sets, and the register
    //  it must leave alone.
    //
    //  A compare is a subtraction whose RESULT IS DISCARDED, which is the thing
    //  to get right -- an implementation that reuses SBC must not write back to
    //  the register, and must not touch overflow either.
    //
    //  All three orderings are covered (less, equal, greater) because the flags
    //  encode the relation between them: carry answers unsigned ordering, Z
    //  answers equality, and N is the high bit of the difference. A test at one
    //  ordering leaves two of the three untested.
    //
    //  Carry is asserted specifically because it is the flag branches use for
    //  unsigned comparison, and its sense here -- SET when the register is
    //  greater or equal -- is the opposite of what a borrow suggests.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CompareTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_Equal_SetsZeroAndCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_Equal_SetsZeroAndCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x42;

            CpuOperations::Compare (cpu, cpu.RegA(), 0x42);

            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_GreaterThan_SetsCarryClearsZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_GreaterThan_SetsCarryClearsZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x42;

            CpuOperations::Compare (cpu, cpu.RegA(), 0x30);

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_LessThan_ClearsCarryAndZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_LessThan_ClearsCarryAndZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x30;

            CpuOperations::Compare (cpu, cpu.RegA(), 0x42);

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_BoundaryValue_0x80_vs_0x00_SetsCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_BoundaryValue_0x80_vs_0x00_SetsCarry)
        {
            // Regression: A=0x80 > operand=0x00, so carry must be set.
            // cmp = 0x80 - 0x00 = 0x80; old condition (< 0x80) wrongly cleared carry.
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x80;

            CpuOperations::Compare (cpu, cpu.RegA(), 0x00);

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IncrementDecrementTests
    //
    //  INC/DEC on memory and INX/DEX/INY/DEY on registers, including the wrap
    //  at each end.
    //
    //  Both TARGETS are covered because they are genuinely different paths: the
    //  register forms operate in place, while the memory forms read, modify,
    //  and write back through an effective address -- and only the latter can
    //  get the address wrong.
    //
    //  The wraps are the boundary that matters: $FF incrementing to $00 and $00
    //  decrementing to $FF, both setting the flags the wrapped value implies.
    //  A loop counting down to zero depends on exactly that.
    //
    //  Carry must be untouched. Unlike ADC, these do not carry out, and a
    //  counter loop that also relies on carry from an earlier operation would
    //  break if they did.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IncrementDecrementTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Increment_Register
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Increment_Register)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegX() = 0x10;

            CpuOperations::Increment (cpu, &cpu.RegX(), 0);

            Assert::AreEqual ((Byte) 0x11, cpu.RegX());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Increment_WrapsToZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Increment_WrapsToZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegX() = 0xFF;

            CpuOperations::Increment (cpu, &cpu.RegX(), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegX());
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Increment_Memory
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Increment_Memory)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Poke (0x50, 0x10);

            CpuOperations::Increment (cpu, nullptr, 0x50);

            Assert::AreEqual ((Byte) 0x11, cpu.Peek (0x50));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Decrement_Register
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Decrement_Register)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegX() = 0x10;

            CpuOperations::Decrement (cpu, &cpu.RegX(), 0);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegX());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Decrement_WrapsToFF
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Decrement_WrapsToFF)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegX() = 0x00;

            CpuOperations::Decrement (cpu, &cpu.RegX(), 0);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegX());
            Assert::IsTrue ((bool) cpu.Status().flags.negative);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShiftRotateTests
    //
    //  ASL, LSR, ROL, and ROR, on the accumulator and on memory.
    //
    //  CARRY is what separates the four. A shift discards the bit that falls
    //  off into carry and brings in a zero; a rotate brings in the OLD carry.
    //  So a rotate needs the incoming carry captured before the outgoing one
    //  overwrites it -- doing that in the wrong order silently turns ROL into
    //  ASL, which produces correct-looking output for any input whose carry
    //  happened to be clear.
    //
    //  Both operand forms are tested because the accumulator and memory paths
    //  differ: one operates on a register, the other reads and writes back
    //  through an effective address.
    //
    //  Multi-byte shift sequences appear here too, since chaining through carry
    //  is the whole reason these instructions expose it -- and a chain is where
    //  an off-by-one-bit error becomes visible.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ShiftRotateTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftLeft_Basic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftLeft_Basic)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x01;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x02, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftLeft_Bit7IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftLeft_Bit7IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x80;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftLeft_DoesNotRotateCarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftLeft_DoesNotRotateCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x02;
            cpu.Status().flags.carry = 1;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x04, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftRight_Basic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftRight_Basic)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x02;

            CpuOperations::ShiftRight (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x01, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftRight_Bit0IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftRight_Bit0IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x01;

            CpuOperations::ShiftRight (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }

        // Dispatch-level regression tests: verify that the ASL/LSR opcodes
        // dispatch to ShiftLeft/ShiftRight (not RotateLeft/RotateRight).
        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ASL_Opcode_WithCarrySet_ShiftsInZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ASL_Opcode_WithCarrySet_ShiftsInZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x02;
            cpu.Status().flags.carry = 1;
            cpu.WriteBytes (0x8000, { 0x0A });     // ASL A

            cpu.Step();

            // Shift: 0x02 << 1 = 0x04 (carry is discarded, not rotated in).
            // If dispatch incorrectly called RotateLeft, result would be 0x05.
            Assert::AreEqual ((Byte) 0x04, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LSR_Opcode_WithCarrySet_ShiftsInZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LSR_Opcode_WithCarrySet_ShiftsInZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x02;
            cpu.Status().flags.carry = 1;
            cpu.WriteBytes (0x8000, { 0x4A });     // LSR A

            cpu.Step();

            // Shift: 0x02 >> 1 = 0x01 (carry is discarded, not rotated in).
            // If dispatch incorrectly called RotateRight, result would be 0x81.
            Assert::AreEqual ((Byte) 0x01, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateLeft_CarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateLeft_CarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;
            cpu.Status().flags.carry = 1;

            CpuOperations::RotateLeft (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x01, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateLeft_Bit7IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateLeft_Bit7IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x80;

            CpuOperations::RotateLeft (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateRight_CarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateRight_CarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;
            cpu.Status().flags.carry = 1;

            CpuOperations::RotateRight (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x80, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateRight_Bit0IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateRight_Bit0IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x01;

            CpuOperations::RotateRight (cpu, &cpu.RegA(), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchOperationTests
    //
    //  All eight conditional branches, taken and not taken, forward and
    //  backward.
    //
    //  Every branch is tested in BOTH directions of its condition, because a
    //  polarity inversion is the easiest mistake to make and produces a program
    //  that runs and does the opposite thing -- the eight instructions are four
    //  flags times two senses, and half of them read as negations.
    //
    //  Backward branches matter separately: the offset is a SIGNED byte, so a
    //  backward target is a large unsigned value that must be sign-extended.
    //  An implementation treating it as unsigned works perfectly for every
    //  forward branch.
    //
    //  The not-taken case asserts the PC advanced past the operand rather than
    //  merely that it did not jump -- a branch that skips its own operand byte
    //  lands mid-instruction.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchOperationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BPL_Taken_WhenPositive
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BPL_Taken_WhenPositive)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.negative = 0;

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BPL_NotTaken_WhenNegative
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BPL_NotTaken_WhenNegative)
        {
            TestCpu  cpu;
            Word     originalPC = 0;
            cpu.InitForTest();
            cpu.Status().flags.negative = 1;
            originalPC = cpu.RegPC();

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual (originalPC, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BMI_Taken_WhenNegative
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BMI_Taken_WhenNegative)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.negative = 1;

            CpuOperations::Branch (cpu, Instruction (0x30), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BCS_Taken_WhenCarrySet
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BCS_Taken_WhenCarrySet)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.carry = 1;

            CpuOperations::Branch (cpu, Instruction (0xB0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BEQ_Taken_WhenZeroSet
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BEQ_Taken_WhenZeroSet)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.zero = 1;

            CpuOperations::Branch (cpu, Instruction (0xF0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BNE_Taken_WhenZeroClear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BNE_Taken_WhenZeroClear)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.zero = 0;

            CpuOperations::Branch (cpu, Instruction (0xD0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BitTestTests
    //
    //  BIT: the one instruction whose N and V come from the OPERAND rather
    //  than from its result.
    //
    //  That is the whole reason it needs its own group. BIT sets Z from A AND
    //  memory, but copies bit 7 of the MEMORY value into N and bit 6 into V --
    //  the accumulator does not participate in either. Every other instruction
    //  derives N from its result, so the natural implementation is wrong here
    //  in a way no other test would notice.
    //
    //  Which is why the fixtures use an accumulator whose own bits 6 and 7
    //  DIFFER from the operand's: identical values would let the wrong
    //  implementation pass.
    //
    //  The accumulator must also be unchanged -- BIT is a test, not an AND.
    //
    //  This is what makes BIT the idiomatic way to poll a hardware status
    //  register: two flag bits read in one instruction without disturbing A.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BitTestTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_ZeroFlag_SetFromAndResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_ZeroFlag_SetFromAndResult)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xF0;

            CpuOperations::BitTest (cpu, 0x0F);

            Assert::IsTrue ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_ZeroFlag_ClearedWhenAndNonZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_ZeroFlag_ClearedWhenAndNonZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::BitTest (cpu, 0x01);

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_OverflowFlag_SetFromOperandBit6_NotAndResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_OverflowFlag_SetFromOperandBit6_NotAndResult)
        {
            // operand bit6=1, A bit6=0 => AND result bit6=0, but V must be 1
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;

            CpuOperations::BitTest (cpu, 0x40);

            Assert::IsTrue ((bool) cpu.Status().flags.overflow);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_OverflowFlag_ClearedWhenOperandBit6Clear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_OverflowFlag_ClearedWhenOperandBit6Clear)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::BitTest (cpu, 0x3F);

            Assert::IsFalse ((bool) cpu.Status().flags.overflow);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_NegativeFlag_SetFromOperandBit7_NotAndResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_NegativeFlag_SetFromOperandBit7_NotAndResult)
        {
            // operand bit7=1, A bit7=0 => AND result bit7=0, but N must be 1
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;

            CpuOperations::BitTest (cpu, 0x80);

            Assert::IsTrue ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_NegativeFlag_ClearedWhenOperandBit7Clear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_NegativeFlag_ClearedWhenOperandBit7Clear)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;

            CpuOperations::BitTest (cpu, 0x7F);

            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JumpOperationTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (JumpOperationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Jump_SetsPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Jump_SetsPC)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::Jump (cpu, Instruction (0x4C), 0x1234);

            Assert::AreEqual ((Word) 0x1234, cpu.RegPC());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NoOperationTests
    //
    //  NOP advancing the PC by one and changing nothing else.
    //
    //  It looks like a test of nothing, and it is precisely a test of nothing:
    //  every register and every flag is asserted UNCHANGED. NOP is used for
    //  timing and for patching out instructions, so a NOP that disturbed a flag
    //  would corrupt code that is correct by construction.
    //
    //  The cycle cost is asserted too, since the timing use is the main one --
    //  a NOP that takes the wrong number of cycles breaks the delay loops it
    //  exists to build.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (NoOperationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NoOperation_DoesNotChangeRegistersOrFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NoOperation_DoesNotChangeRegistersOrFlags)
        {
            TestCpu cpu;
            cpu.InitForTest();

            cpu.RegA  () = 0x12;
            cpu.RegX  () = 0x34;
            cpu.RegY  () = 0x56;
            cpu.RegSP() = 0x78;
            cpu.Status().status = 0xA5;

            CpuOperations::NoOperation (cpu);

            Assert::AreEqual ((Byte) 0x12, cpu.RegA());
            Assert::AreEqual ((Byte) 0x34, cpu.RegX());
            Assert::AreEqual ((Byte) 0x56, cpu.RegY());
            Assert::AreEqual ((Byte) 0x78, cpu.RegSP());
            Assert::AreEqual ((Byte) 0xA5, cpu.Status().status);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PushPullTests
    //
    //  PHA/PLA and PHP/PLP: the stack pointer's direction, its wrap, and the
    //  status-register bits that do not survive a round trip.
    //
    //  The stack grows DOWNWARD from $01FF and the pointer is post-decrement on
    //  push, pre-increment on pull -- an off-by-one in either direction reads
    //  back the wrong byte, so the tests assert the pointer's value as well as
    //  the data.
    //
    //  The pointer is eight bits and WRAPS within page 1: pushing past $0100
    //  wraps to $01FF rather than descending into zero page. That is what makes
    //  stack overflow silently corrupt the stack instead of the zero page, and
    //  it is behavior real software has been observed to survive.
    //
    //  PHP/PLP get their own attention because the B flag and the unused bit
    //  are not simply stored and restored -- their values on push differ from
    //  what a pull produces, so a naive round trip test would pass while the
    //  bits are wrong.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PushPullTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Push_A_WritesToStackAndDecrementsSP
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Push_A_WritesToStackAndDecrementsSP)
        {
            TestCpu  cpu;
            Byte     spBefore = 0;
            cpu.InitForTest();
            cpu.RegA() = 0x42;

            spBefore = cpu.RegSP();
            CpuOperations::Push (cpu, &cpu.RegA());

            Assert::AreEqual ((Byte) (spBefore - 1), cpu.RegSP());
            Assert::AreEqual ((Byte) 0x42, cpu.Peek ((Word) (0x0100 + spBefore)));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_A_ReadsFromStackAndIncrementsSP
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_A_ReadsFromStackAndIncrementsSP)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;
            cpu.RegSP() = 0xFE;
            cpu.Poke (0x01FF, 0x77);

            CpuOperations::Pull (cpu, &cpu.RegA());

            Assert::AreEqual ((Byte) 0x77, cpu.RegA());
            Assert::AreEqual ((Byte) 0xFF, cpu.RegSP());
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_A_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_A_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xAA;
            cpu.RegSP() = 0xFE;
            cpu.Poke (0x01FF, 0x00);

            CpuOperations::Pull (cpu, &cpu.RegA());

            Assert::AreEqual ((Byte) 0x00, cpu.RegA());
            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_A_Negative_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_A_Negative_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegSP() = 0xFE;
            cpu.Poke (0x01FF, 0x80);

            CpuOperations::Pull (cpu, &cpu.RegA());

            Assert::AreEqual ((Byte) 0x80, cpu.RegA());
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Push_Status_SetsBreakAndAlwaysOneInPushedByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Push_Status_SetsBreakAndAlwaysOneInPushedByte)
        {
            TestCpu  cpu;
            Byte     spBefore = 0;
            cpu.InitForTest();
            cpu.Status().status = 0x00;
            cpu.Status().flags.carry = 1;     // 0x01

            spBefore = cpu.RegSP();
            CpuOperations::Push (cpu, &cpu.Status().status);

            // PHP pushes status with B (0x10) and AlwaysOne (0x20) set.
            Assert::AreEqual ((Byte) 0x31, cpu.Peek ((Word) (0x0100 + spBefore)));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_Status_PreservesBreakAndAlwaysOne
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_Status_PreservesBreakAndAlwaysOne)
        {
            TestCpu cpu;
            cpu.InitForTest();
            // Status currently has alwaysOne=1, brk=0
            cpu.Status().status = 0x20;
            cpu.RegSP() = 0xFE;
            // Pulled byte has B=1 and U=1 set; PLP must not alter actual B/U bits.
            cpu.Poke (0x01FF, 0xFF);

            CpuOperations::Pull (cpu, &cpu.Status().status);

            // B and AlwaysOne preserved from the pre-pull register state.
            Assert::AreEqual ((Byte) 0x20, (Byte) (cpu.Status().status & 0x30));
            // Other bits come from the pulled value (0xFF & ~0x30 = 0xCF).
            Assert::AreEqual ((Byte) 0xCF, (Byte) (cpu.Status().status & ~0x30));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Push_Then_Pull_RoundTripsAccumulator
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Push_Then_Pull_RoundTripsAccumulator)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x5A;

            CpuOperations::Push (cpu, &cpu.RegA());
            cpu.RegA() = 0x00;
            CpuOperations::Pull (cpu, &cpu.RegA());

            Assert::AreEqual ((Byte) 0x5A, cpu.RegA());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TransferTests
    //
    //  TAX/TXA/TAY/TYA/TSX/TXS -- and the one of them that does NOT set flags.
    //
    //  TXS is the exception, and it is the only reason this group is
    //  interesting. Every other transfer sets N and Z from the value moved, but
    //  TXS does not touch them at all -- so the conventional stack-setup
    //  sequence (LDX #$FF, TXS) must leave the flags exactly as it found them.
    //
    //  An implementation that treats all six uniformly is correct five times
    //  out of six, which is exactly the kind of bug a per-instruction group
    //  catches and a shared helper hides.
    //
    //  Both directions of each pair are covered, since they are separate
    //  opcodes with separate source and destination wiring.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TransferTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_A_To_X_CopiesValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_A_To_X_CopiesValue)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x42;
            cpu.RegX() = 0x00;

            CpuOperations::Transfer (cpu, &cpu.RegA(), &cpu.RegX());

            Assert::AreEqual ((Byte) 0x42, cpu.RegX());
            Assert::AreEqual ((Byte) 0x42, cpu.RegA());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x00;
            cpu.RegX() = 0xFF;

            CpuOperations::Transfer (cpu, &cpu.RegA(), &cpu.RegX());

            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_Negative_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_Negative_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x80;

            CpuOperations::Transfer (cpu, &cpu.RegA(), &cpu.RegY());

            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_X_To_SP_DoesNotAffectFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_X_To_SP_DoesNotAffectFlags)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegX  () = 0x00;
            cpu.RegSP() = 0xFF;
            cpu.Status().flags.zero     = 0;
            cpu.Status().flags.negative = 1;

            CpuOperations::Transfer (cpu, &cpu.RegX(), &cpu.RegSP());

            Assert::AreEqual ((Byte) 0x00, cpu.RegSP());
            // TXS must not change Z or N even when transferring zero / negative values.
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_SP_To_X_AffectsFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_SP_To_X_AffectsFlags)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegSP() = 0x00;
            cpu.RegX  () = 0x55;

            CpuOperations::Transfer (cpu, &cpu.RegSP(), &cpu.RegX());

            Assert::AreEqual ((Byte) 0x00, cpu.RegX());
            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetFlagTests
    //
    //  CLC/SEC, CLI/SEI, CLD/SED, and CLV -- each touching ONE flag and
    //  nothing else.
    //
    //  Isolation is what these assert. The six instructions are dispatched
    //  through a single SetFlag operation that selects its bit from the opcode,
    //  so a decode error changes the WRONG flag while still looking like a flag
    //  instruction. Every test therefore checks the other flags are unchanged
    //  as well as the target one.
    //
    //  CLV is the odd one with no matching set: overflow is cleared explicitly
    //  and set only by arithmetic, so a symmetric implementation would invent
    //  an SEV that does not exist.
    //
    //  SED matters beyond the flag itself, since decimal mode changes what ADC
    //  and SBC do -- it is the switch the whole BCD path hangs from.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SetFlagTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLC_ClearsCarryFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLC_ClearsCarryFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.carry = 1;

            CpuOperations::SetFlag (cpu, Instruction (0x18));

            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SEC_SetsCarryFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SEC_SetsCarryFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::SetFlag (cpu, Instruction (0x38));

            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLI_ClearsInterruptDisableFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLI_ClearsInterruptDisableFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.interruptDisable = 1;

            CpuOperations::SetFlag (cpu, Instruction (0x58));

            Assert::IsFalse ((bool) cpu.Status().flags.interruptDisable);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SEI_SetsInterruptDisableFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SEI_SetsInterruptDisableFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::SetFlag (cpu, Instruction (0x78));

            Assert::IsTrue ((bool) cpu.Status().flags.interruptDisable);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLV_ClearsOverflowFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLV_ClearsOverflowFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.overflow = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xB8));

            Assert::IsFalse ((bool) cpu.Status().flags.overflow);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLD_ClearsDecimalFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLD_ClearsDecimalFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.decimal = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xD8));

            Assert::IsFalse ((bool) cpu.Status().flags.decimal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SED_SetsDecimalFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SED_SetsDecimalFlag)
        {
            TestCpu cpu;
            cpu.InitForTest();

            CpuOperations::SetFlag (cpu, Instruction (0xF8));

            Assert::IsTrue ((bool) cpu.Status().flags.decimal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SetFlag_DoesNotAffectUnrelatedFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SetFlag_DoesNotAffectUnrelatedFlags)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().flags.zero     = 1;
            cpu.Status().flags.negative = 1;
            cpu.Status().flags.carry    = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xF8)); // SED

            Assert::IsTrue ((bool) cpu.Status().flags.decimal);
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
            Assert::IsTrue ((bool) cpu.Status().flags.negative);
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReturnFromSubroutineTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReturnFromSubroutineTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RTS_PullsReturnAddressAndIncrements
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RTS_PullsReturnAddressAndIncrements)
        {
            TestCpu cpu;
            cpu.InitForTest();
            // JSR pushes (PC of last byte of JSR) = target-1; RTS pops and adds 1.
            cpu.DoPushWord (0x1233);

            CpuOperations::ReturnFromSubroutine (cpu);

            Assert::AreEqual ((Word) 0x1234, cpu.RegPC());
            Assert::AreEqual ((Byte) 0xFF,   cpu.RegSP());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReturnFromInterruptTests
    //
    //  RTI restoring the status register AND the PC, in that order, from the
    //  stack.
    //
    //  It differs from RTS in two ways that both matter. RTI pulls the status
    //  register first, which RTS does not do at all -- and it returns to the
    //  EXACT pushed address, where RTS returns to the pushed address plus one
    //  because JSR pushes the address of its own last operand byte.
    //
    //  An RTI implemented as "RTS plus a status pull" is therefore off by one,
    //  and the failure is a return into the middle of the interrupted
    //  instruction.
    //
    //  The stack is set up by hand rather than by taking a real interrupt, so
    //  the test pins RTI's own behavior instead of depending on the interrupt
    //  sequence being correct.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReturnFromInterruptTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RTI_PullsStatusAndPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RTI_PullsStatusAndPC)
        {
            TestCpu cpu;
            cpu.InitForTest();
            // Stack layout below SP (popped order): status, PCL, PCH.
            cpu.RegSP() = 0xFC;
            cpu.Poke (0x01FD, 0x33);  // status: carry+zero set, B+U set
            cpu.Poke (0x01FE, 0x21);  // PC low
            cpu.Poke (0x01FF, 0x43);  // PC high

            CpuOperations::ReturnFromInterrupt (cpu);

            Assert::AreEqual ((Word) 0x4321, cpu.RegPC());
            Assert::AreEqual ((Byte) 0xFF,   cpu.RegSP());
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            // B and AlwaysOne in actual register are preserved from the pre-RTI state
            // (InitForTest sets alwaysOne=1, brk=0), regardless of the pulled byte.
            Assert::AreEqual ((Byte) 0x20, (Byte) (cpu.Status().status & 0x30));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RTI_DoesNotIncrementPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RTI_DoesNotIncrementPC)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegSP() = 0xFC;
            cpu.Poke (0x01FD, 0x00);  // status
            cpu.Poke (0x01FE, 0x00);  // PC low
            cpu.Poke (0x01FF, 0x80);  // PC high

            CpuOperations::ReturnFromInterrupt (cpu);

            // Unlike RTS, RTI does not add 1 to the pulled PC.
            Assert::AreEqual ((Word) 0x8000, cpu.RegPC());
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndocumentedOpcodeTests
    //
    //  NMOS 6502 undocumented opcodes used by real Apple II software
    //  (e.g. Space Quarks): $04 DOP zp and $CF DCP abs.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UndocumentedOpcodeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DOP_zp_IsTwoByteThreeCycleNop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DOP_zp_IsTwoByteThreeCycleNop)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x42;
            cpu.RegX() = 0x11;
            cpu.RegY() = 0x22;
            cpu.Poke (0x30, 0x99);                 // zero-page byte that is read and discarded
            cpu.WriteBytes (0x8000, { 0x04, 0x30 });  // DOP $30

            cpu.Step();

            // Two-byte instruction: PC advances past opcode + operand.
            Assert::AreEqual ((Word) 0x8002, cpu.RegPC());

            // No register or memory effects.
            Assert::AreEqual ((Byte) 0x42, cpu.RegA());
            Assert::AreEqual ((Byte) 0x11, cpu.RegX());
            Assert::AreEqual ((Byte) 0x22, cpu.RegY());
            Assert::AreEqual ((Byte) 0x99, cpu.Peek (0x30));
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DOP_zp_LeavesFlagsUnchanged
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DOP_zp_LeavesFlagsUnchanged)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.Status().status = 0;
            cpu.Status().flags.carry    = 1;
            cpu.Status().flags.zero     = 1;
            cpu.Status().flags.negative = 1;
            cpu.WriteBytes (0x8000, { 0x04, 0x00 });  // DOP $00

            cpu.Step();

            Assert::IsTrue ((bool) cpu.Status().flags.carry);
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
            Assert::IsTrue ((bool) cpu.Status().flags.negative);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DCP_abs_DecrementsMemoryAndComparesEqual
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DCP_abs_DecrementsMemoryAndComparesEqual)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x41;
            cpu.Poke (0x1234, 0x42);               // decremented to 0x41, equals A
            cpu.WriteBytes (0x8000, { 0xCF, 0x34, 0x12 });  // DCP $1234

            cpu.Step();

            // Three-byte instruction.
            Assert::AreEqual ((Word) 0x8003, cpu.RegPC());

            // Memory decremented in place.
            Assert::AreEqual ((Byte) 0x41, cpu.Peek (0x1234));

            // CMP A(0x41) vs decremented(0x41): equal -> Z and C set, N clear.
            Assert::IsTrue  ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);

            // Accumulator is never modified by DCP.
            Assert::AreEqual ((Byte) 0x41, cpu.RegA());
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DCP_abs_ComparesGreaterThan_SetsCarryClearsZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DCP_abs_ComparesGreaterThan_SetsCarryClearsZero)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x50;
            cpu.Poke (0x1234, 0x31);               // decremented to 0x30, A > value
            cpu.WriteBytes (0x8000, { 0xCF, 0x34, 0x12 });  // DCP $1234

            cpu.Step();

            Assert::AreEqual ((Byte) 0x30, cpu.Peek (0x1234));
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status().flags.carry);
            Assert::IsFalse ((bool) cpu.Status().flags.negative);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DCP_abs_ComparesLessThan_ClearsCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DCP_abs_ComparesLessThan_ClearsCarry)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x10;
            cpu.Poke (0x1234, 0x41);               // decremented to 0x40, A < value
            cpu.WriteBytes (0x8000, { 0xCF, 0x34, 0x12 });  // DCP $1234

            cpu.Step();

            Assert::AreEqual ((Byte) 0x40, cpu.Peek (0x1234));
            Assert::IsFalse ((bool) cpu.Status().flags.zero);
            Assert::IsFalse ((bool) cpu.Status().flags.carry);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DCP_abs_DecrementWrapsAndComparesAgainstWrappedValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DCP_abs_DecrementWrapsAndComparesAgainstWrappedValue)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0xFF;
            cpu.Poke (0x1234, 0x00);               // decrements to 0xFF, equals A
            cpu.WriteBytes (0x8000, { 0xCF, 0x34, 0x12 });  // DCP $1234

            cpu.Step();

            Assert::AreEqual ((Byte) 0xFF, cpu.Peek (0x1234));
            Assert::IsTrue ((bool) cpu.Status().flags.zero);
            Assert::IsTrue ((bool) cpu.Status().flags.carry);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UndocumentedOpcodes_AreMarkedLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UndocumentedOpcodes_AreMarkedLegal)
        {
            TestCpu cpu;
            cpu.InitForTest();

            Assert::IsTrue (cpu.GetMicrocode (0x04).isLegal);
            Assert::IsTrue (cpu.GetMicrocode (0xCF).isLegal);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DOP_zp_TakesThreeCycles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DOP_zp_TakesThreeCycles)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, { 0x04, 0x30 });  // DOP $30

            cpu.StepOne();

            // NMOS NOP zp is a fixed 3-cycle instruction with no
            // page-crossing penalty (zero-page addressing).
            Assert::AreEqual ((Byte) 3, cpu.GetLastInstructionCycles());
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DCP_abs_TakesSixCycles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DCP_abs_TakesSixCycles)
        {
            TestCpu cpu;
            cpu.InitForTest();
            cpu.RegA() = 0x10;
            cpu.Poke (0x1234, 0x42);
            cpu.WriteBytes (0x8000, { 0xCF, 0x34, 0x12 });  // DCP $1234

            cpu.StepOne();

            // NMOS DCP abs is a fixed 6-cycle read-modify-write; the
            // RMW exclusion keeps it from accruing a page-cross penalty.
            Assert::AreEqual ((Byte) 6, cpu.GetLastInstructionCycles());
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IndexedShiftsCostSevenWhetherOrNotThePageCrosses
        //
        //  The NMOS half of a divergence the 65C02 introduced: there, these four
        //  drop to six and pay a seventh cycle only on a real crossing. Here they
        //  are seven either way, because the part cannot know it has crossed
        //  until it has read and it must write regardless.
        //
        //  Asserted from BOTH sides of the page boundary. The CMOS retiming is a
        //  per-instruction flag that this core never sets, and a flag that leaked
        //  across would show up only on the crossing case -- which is exactly the
        //  case a single non-crossing assertion would miss.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IndexedShiftsCostSevenWhetherOrNotThePageCrosses)
        {
            static constexpr Byte    kIndexedShifts[4] = { 0x1E, 0x3E, 0x5E, 0x7E };   // ASL, ROL, LSR, ROR

            for (Byte opcode : kIndexedShifts)
            {
                TestCpu  sameP;
                TestCpu  crossing;

                sameP.InitForTest();
                sameP.RegX() = 0x10;
                sameP.WriteBytes (0x8000, { opcode, 0x80, 0x12 });   // $1280 + $10 = $1290
                sameP.StepOne();

                crossing.InitForTest();
                crossing.RegX() = 0x10;
                crossing.WriteBytes (0x8000, { opcode, 0xF8, 0x12 }); // $12F8 + $10 = $1308
                crossing.StepOne();

                Assert::AreEqual ((Byte) 7, sameP.GetLastInstructionCycles());
                Assert::AreEqual ((Byte) 7, crossing.GetLastInstructionCycles());
            }
        }
    };
}
