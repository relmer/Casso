#include "Pch.h"

#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CpuOperationTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LoadStoreTests
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
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x42);

            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Load_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Load_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x00);

            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Load_NegativeValue_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Load_NegativeValue_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x80);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Store_WritesToMemory
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Store_WritesToMemory)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xBB;

            CpuOperations::Store (cpu, cpu.RegA (), 0x1234);

            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0x1234));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AddWithCarryTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0x10;

            CpuOperations::AddWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x30, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
            Assert::IsFalse ((bool) cpu.Status ().flags.overflow);
            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_WithCarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_WithCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x10;
            cpu.Status ().flags.carry = 1;

            CpuOperations::AddWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x31, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_ProducesCarryOut
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_ProducesCarryOut)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x40;

            CpuOperations::AddWithCarry (cpu, 0x40);

            Assert::AreEqual ((Byte) 0x80, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::AddWithCarry (cpu, 0x80);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x50;

            CpuOperations::AddWithCarry (cpu, 0xD0);

            Assert::AreEqual ((Byte) 0x20, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.overflow);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.RegA () = 0x25;

            CpuOperations::AddWithCarry (cpu, 0x48);

            Assert::AreEqual ((Byte) 0x73, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.RegA () = 0x09;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x10, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.RegA () = 0x99;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.Status ().flags.carry   = 1;
            cpu.RegA () = 0x25;

            CpuOperations::AddWithCarry (cpu, 0x48);

            Assert::AreEqual ((Byte) 0x74, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.RegA () = 0x50;

            CpuOperations::AddWithCarry (cpu, 0x50);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 0;
            cpu.RegA () = 0x09;

            CpuOperations::AddWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x0A, cpu.RegA ()); // binary, not 0x10
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SubtractWithCarryTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0x50;
            cpu.Status ().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x10);

            Assert::AreEqual ((Byte) 0x40, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x50;
            cpu.Status ().flags.carry = 0;

            CpuOperations::SubtractWithCarry (cpu, 0x10);

            Assert::AreEqual ((Byte) 0x3F, cpu.RegA ());
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
            cpu.InitForTest ();
            cpu.RegA () = 0x10;
            cpu.Status ().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0xF0, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_ZeroResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_ZeroResult)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x42;
            cpu.Status ().flags.carry = 1;

            CpuOperations::SubtractWithCarry (cpu, 0x42);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.Status ().flags.carry   = 1;
            cpu.RegA () = 0x46;

            CpuOperations::SubtractWithCarry (cpu, 0x12);

            Assert::AreEqual ((Byte) 0x34, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.Status ().flags.carry   = 1;
            cpu.RegA () = 0x40;

            CpuOperations::SubtractWithCarry (cpu, 0x13);

            Assert::AreEqual ((Byte) 0x27, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.Status ().flags.carry   = 0;
            cpu.RegA () = 0x50;

            CpuOperations::SubtractWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x29, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.Status ().flags.carry   = 1;
            cpu.RegA () = 0x00;

            CpuOperations::SubtractWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x99, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 0;
            cpu.Status ().flags.carry   = 1;
            cpu.RegA () = 0x10;

            CpuOperations::SubtractWithCarry (cpu, 0x01);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegA ()); // binary, not BCD-adjusted
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LogicTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  And_ZeroResult_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (And_ZeroResult_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xF0;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Or_CombinesBits
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Or_CombinesBits)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xF0;

            CpuOperations::Or (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Xor_IdenticalOperands_ProducesZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Xor_IdenticalOperands_ProducesZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::Xor (cpu, 0xFF);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CompareTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0x42;

            CpuOperations::Compare (cpu, cpu.RegA (), 0x42);

            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_GreaterThan_SetsCarryClearsZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_GreaterThan_SetsCarryClearsZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x42;

            CpuOperations::Compare (cpu, cpu.RegA (), 0x30);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Compare_LessThan_ClearsCarryAndZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Compare_LessThan_ClearsCarryAndZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x30;

            CpuOperations::Compare (cpu, cpu.RegA (), 0x42);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::Compare (cpu, cpu.RegA (), 0x00);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IncrementDecrementTests
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
            cpu.InitForTest ();
            cpu.RegX () = 0x10;

            CpuOperations::Increment (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x11, cpu.RegX ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Increment_WrapsToZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Increment_WrapsToZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0xFF;

            CpuOperations::Increment (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegX ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Increment_Memory
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Increment_Memory)
        {
            TestCpu cpu;
            cpu.InitForTest ();
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
            cpu.InitForTest ();
            cpu.RegX () = 0x10;

            CpuOperations::Decrement (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegX ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Decrement_WrapsToFF
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Decrement_WrapsToFF)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x00;

            CpuOperations::Decrement (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegX ());
            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShiftRotateTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0x01;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x02, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftLeft_Bit7IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftLeft_Bit7IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftLeft_DoesNotRotateCarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftLeft_DoesNotRotateCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x02;
            cpu.Status ().flags.carry = 1;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x04, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftRight_Basic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftRight_Basic)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x02;

            CpuOperations::ShiftRight (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ShiftRight_Bit0IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ShiftRight_Bit0IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x01;

            CpuOperations::ShiftRight (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x02;
            cpu.Status ().flags.carry = 1;
            cpu.WriteBytes (0x8000, { 0x0A });     // ASL A

            cpu.Step ();

            // Shift: 0x02 << 1 = 0x04 (carry is discarded, not rotated in).
            // If dispatch incorrectly called RotateLeft, result would be 0x05.
            Assert::AreEqual ((Byte) 0x04, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LSR_Opcode_WithCarrySet_ShiftsInZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LSR_Opcode_WithCarrySet_ShiftsInZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x02;
            cpu.Status ().flags.carry = 1;
            cpu.WriteBytes (0x8000, { 0x4A });     // LSR A

            cpu.Step ();

            // Shift: 0x02 >> 1 = 0x01 (carry is discarded, not rotated in).
            // If dispatch incorrectly called RotateRight, result would be 0x81.
            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateLeft_CarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateLeft_CarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x00;
            cpu.Status ().flags.carry = 1;

            CpuOperations::RotateLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateLeft_Bit7IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateLeft_Bit7IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::RotateLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateRight_CarryIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateRight_CarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x00;
            cpu.Status ().flags.carry = 1;

            CpuOperations::RotateRight (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x80, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RotateRight_Bit0IntoCarry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RotateRight_Bit0IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x01;

            CpuOperations::RotateRight (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchOperationTests
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
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 0;

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BPL_NotTaken_WhenNegative
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BPL_NotTaken_WhenNegative)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 1;
            Word originalPC = cpu.RegPC ();

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual (originalPC, cpu.RegPC ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BMI_Taken_WhenNegative
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BMI_Taken_WhenNegative)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 1;

            CpuOperations::Branch (cpu, Instruction (0x30), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BCS_Taken_WhenCarrySet
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BCS_Taken_WhenCarrySet)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.carry = 1;

            CpuOperations::Branch (cpu, Instruction (0xB0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BEQ_Taken_WhenZeroSet
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BEQ_Taken_WhenZeroSet)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.zero = 1;

            CpuOperations::Branch (cpu, Instruction (0xF0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BNE_Taken_WhenZeroClear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BNE_Taken_WhenZeroClear)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.zero = 0;

            CpuOperations::Branch (cpu, Instruction (0xD0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BitTestTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0xF0;

            CpuOperations::BitTest (cpu, 0x0F);

            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_ZeroFlag_ClearedWhenAndNonZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_ZeroFlag_ClearedWhenAndNonZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::BitTest (cpu, 0x01);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x00;

            CpuOperations::BitTest (cpu, 0x40);

            Assert::IsTrue ((bool) cpu.Status ().flags.overflow);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_OverflowFlag_ClearedWhenOperandBit6Clear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_OverflowFlag_ClearedWhenOperandBit6Clear)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::BitTest (cpu, 0x3F);

            Assert::IsFalse ((bool) cpu.Status ().flags.overflow);
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
            cpu.InitForTest ();
            cpu.RegA () = 0x00;

            CpuOperations::BitTest (cpu, 0x80);

            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BitTest_NegativeFlag_ClearedWhenOperandBit7Clear
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BitTest_NegativeFlag_ClearedWhenOperandBit7Clear)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::BitTest (cpu, 0x7F);

            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
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
            cpu.InitForTest ();

            CpuOperations::Jump (cpu, Instruction (0x4C), 0x1234);

            Assert::AreEqual ((Word) 0x1234, cpu.RegPC ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NoOperationTests
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
            cpu.InitForTest ();

            cpu.RegA  () = 0x12;
            cpu.RegX  () = 0x34;
            cpu.RegY  () = 0x56;
            cpu.RegSP () = 0x78;
            cpu.Status ().status = 0xA5;

            CpuOperations::NoOperation (cpu);

            Assert::AreEqual ((Byte) 0x12, cpu.RegA ());
            Assert::AreEqual ((Byte) 0x34, cpu.RegX ());
            Assert::AreEqual ((Byte) 0x56, cpu.RegY ());
            Assert::AreEqual ((Byte) 0x78, cpu.RegSP ());
            Assert::AreEqual ((Byte) 0xA5, cpu.Status ().status);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PushPullTests
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
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x42;

            Byte spBefore = cpu.RegSP ();
            CpuOperations::Push (cpu, &cpu.RegA ());

            Assert::AreEqual ((Byte) (spBefore - 1), cpu.RegSP ());
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
            cpu.InitForTest ();
            cpu.RegA () = 0x00;
            cpu.RegSP () = 0xFE;
            cpu.Poke (0x01FF, 0x77);

            CpuOperations::Pull (cpu, &cpu.RegA ());

            Assert::AreEqual ((Byte) 0x77, cpu.RegA ());
            Assert::AreEqual ((Byte) 0xFF, cpu.RegSP ());
            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_A_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_A_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xAA;
            cpu.RegSP () = 0xFE;
            cpu.Poke (0x01FF, 0x00);

            CpuOperations::Pull (cpu, &cpu.RegA ());

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pull_A_Negative_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pull_A_Negative_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegSP () = 0xFE;
            cpu.Poke (0x01FF, 0x80);

            CpuOperations::Pull (cpu, &cpu.RegA ());

            Assert::AreEqual ((Byte) 0x80, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Push_Status_SetsBreakAndAlwaysOneInPushedByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Push_Status_SetsBreakAndAlwaysOneInPushedByte)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().status = 0x00;
            cpu.Status ().flags.carry = 1;     // 0x01

            Byte spBefore = cpu.RegSP ();
            CpuOperations::Push (cpu, &cpu.Status ().status);

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
            cpu.InitForTest ();
            // Status currently has alwaysOne=1, brk=0
            cpu.Status ().status = 0x20;
            cpu.RegSP () = 0xFE;
            // Pulled byte has B=1 and U=1 set; PLP must not alter actual B/U bits.
            cpu.Poke (0x01FF, 0xFF);

            CpuOperations::Pull (cpu, &cpu.Status ().status);

            // B and AlwaysOne preserved from the pre-pull register state.
            Assert::AreEqual ((Byte) 0x20, (Byte) (cpu.Status ().status & 0x30));
            // Other bits come from the pulled value (0xFF & ~0x30 = 0xCF).
            Assert::AreEqual ((Byte) 0xCF, (Byte) (cpu.Status ().status & ~0x30));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Push_Then_Pull_RoundTripsAccumulator
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Push_Then_Pull_RoundTripsAccumulator)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x5A;

            CpuOperations::Push (cpu, &cpu.RegA ());
            cpu.RegA () = 0x00;
            CpuOperations::Pull (cpu, &cpu.RegA ());

            Assert::AreEqual ((Byte) 0x5A, cpu.RegA ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TransferTests
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
            cpu.InitForTest ();
            cpu.RegA () = 0x42;
            cpu.RegX () = 0x00;

            CpuOperations::Transfer (cpu, &cpu.RegA (), &cpu.RegX ());

            Assert::AreEqual ((Byte) 0x42, cpu.RegX ());
            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_Zero_SetsZeroFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x00;
            cpu.RegX () = 0xFF;

            CpuOperations::Transfer (cpu, &cpu.RegA (), &cpu.RegX ());

            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_Negative_SetsNegativeFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_Negative_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::Transfer (cpu, &cpu.RegA (), &cpu.RegY ());

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_X_To_SP_DoesNotAffectFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_X_To_SP_DoesNotAffectFlags)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX  () = 0x00;
            cpu.RegSP () = 0xFF;
            cpu.Status ().flags.zero     = 0;
            cpu.Status ().flags.negative = 1;

            CpuOperations::Transfer (cpu, &cpu.RegX (), &cpu.RegSP ());

            Assert::AreEqual ((Byte) 0x00, cpu.RegSP ());
            // TXS must not change Z or N even when transferring zero / negative values.
            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Transfer_SP_To_X_AffectsFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Transfer_SP_To_X_AffectsFlags)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegSP () = 0x00;
            cpu.RegX  () = 0x55;

            CpuOperations::Transfer (cpu, &cpu.RegSP (), &cpu.RegX ());

            Assert::AreEqual ((Byte) 0x00, cpu.RegX ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetFlagTests
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
            cpu.InitForTest ();
            cpu.Status ().flags.carry = 1;

            CpuOperations::SetFlag (cpu, Instruction (0x18));

            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SEC_SetsCarryFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SEC_SetsCarryFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::SetFlag (cpu, Instruction (0x38));

            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLI_ClearsInterruptDisableFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLI_ClearsInterruptDisableFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.interruptDisable = 1;

            CpuOperations::SetFlag (cpu, Instruction (0x58));

            Assert::IsFalse ((bool) cpu.Status ().flags.interruptDisable);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SEI_SetsInterruptDisableFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SEI_SetsInterruptDisableFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::SetFlag (cpu, Instruction (0x78));

            Assert::IsTrue ((bool) cpu.Status ().flags.interruptDisable);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLV_ClearsOverflowFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLV_ClearsOverflowFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.overflow = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xB8));

            Assert::IsFalse ((bool) cpu.Status ().flags.overflow);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CLD_ClearsDecimalFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CLD_ClearsDecimalFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xD8));

            Assert::IsFalse ((bool) cpu.Status ().flags.decimal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SED_SetsDecimalFlag
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SED_SetsDecimalFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::SetFlag (cpu, Instruction (0xF8));

            Assert::IsTrue ((bool) cpu.Status ().flags.decimal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SetFlag_DoesNotAffectUnrelatedFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SetFlag_DoesNotAffectUnrelatedFlags)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.zero     = 1;
            cpu.Status ().flags.negative = 1;
            cpu.Status ().flags.carry    = 1;

            CpuOperations::SetFlag (cpu, Instruction (0xF8)); // SED

            Assert::IsTrue ((bool) cpu.Status ().flags.decimal);
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
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
            cpu.InitForTest ();
            // JSR pushes (PC of last byte of JSR) = target-1; RTS pops and adds 1.
            cpu.DoPushWord (0x1233);

            CpuOperations::ReturnFromSubroutine (cpu);

            Assert::AreEqual ((Word) 0x1234, cpu.RegPC ());
            Assert::AreEqual ((Byte) 0xFF,   cpu.RegSP ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReturnFromInterruptTests
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
            cpu.InitForTest ();
            // Stack layout below SP (popped order): status, PCL, PCH.
            cpu.RegSP () = 0xFC;
            cpu.Poke (0x01FD, 0x33);  // status: carry+zero set, B+U set
            cpu.Poke (0x01FE, 0x21);  // PC low
            cpu.Poke (0x01FF, 0x43);  // PC high

            CpuOperations::ReturnFromInterrupt (cpu);

            Assert::AreEqual ((Word) 0x4321, cpu.RegPC ());
            Assert::AreEqual ((Byte) 0xFF,   cpu.RegSP ());
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            // B and AlwaysOne in actual register are preserved from the pre-RTI state
            // (InitForTest sets alwaysOne=1, brk=0), regardless of the pulled byte.
            Assert::AreEqual ((Byte) 0x20, (Byte) (cpu.Status ().status & 0x30));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RTI_DoesNotIncrementPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RTI_DoesNotIncrementPC)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegSP () = 0xFC;
            cpu.Poke (0x01FD, 0x00);  // status
            cpu.Poke (0x01FE, 0x00);  // PC low
            cpu.Poke (0x01FF, 0x80);  // PC high

            CpuOperations::ReturnFromInterrupt (cpu);

            // Unlike RTS, RTI does not add 1 to the pulled PC.
            Assert::AreEqual ((Word) 0x8000, cpu.RegPC ());
        }
    };
}
