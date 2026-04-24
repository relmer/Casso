#include "Pch.h"

#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace CpuOperationTests
{
    // =========================================================================
    // Load / Store
    // =========================================================================
    TEST_CLASS (LoadStoreTests)
    {
    public:

        TEST_METHOD (Load_SetsRegisterValue)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x42);

            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
        }

        TEST_METHOD (Load_Zero_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x00);

            Assert::IsTrue  ((bool) cpu.Status ().flags.zero);
            Assert::IsFalse ((bool) cpu.Status ().flags.negative);
        }

        TEST_METHOD (Load_NegativeValue_SetsNegativeFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Load (cpu, cpu.RegA (), 0x80);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.negative);
        }

        TEST_METHOD (Store_WritesToMemory)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xBB;

            CpuOperations::Store (cpu, cpu.RegA (), 0x1234);

            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0x1234));
        }
    };



    // =========================================================================
    // ADC (Add with Carry)
    // =========================================================================
    TEST_CLASS (AddWithCarryTests)
    {
    public:

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

        TEST_METHOD (ADC_WithCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x10;
            cpu.Status ().flags.carry = 1;

            CpuOperations::AddWithCarry (cpu, 0x20);

            Assert::AreEqual ((Byte) 0x31, cpu.RegA ());
        }

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
    };



    // =========================================================================
    // SBC (Subtract with Carry)
    // =========================================================================
    TEST_CLASS (SubtractWithCarryTests)
    {
    public:

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
    };



    // =========================================================================
    // Logic: AND, OR, XOR
    // =========================================================================
    TEST_CLASS (LogicTests)
    {
    public:

        TEST_METHOD (And_MasksAccumulator)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xFF;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegA ());
        }

        TEST_METHOD (And_ZeroResult_SetsZeroFlag)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xF0;

            CpuOperations::And (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }

        TEST_METHOD (Or_CombinesBits)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0xF0;

            CpuOperations::Or (cpu, 0x0F);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
        }

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



    // =========================================================================
    // Compare
    // =========================================================================
    TEST_CLASS (CompareTests)
    {
    public:

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

        TEST_METHOD (Compare_GreaterThan_SetsCarryClearsZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x42;

            CpuOperations::Compare (cpu, cpu.RegA (), 0x30);

            Assert::IsFalse ((bool) cpu.Status ().flags.zero);
            Assert::IsTrue  ((bool) cpu.Status ().flags.carry);
        }

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



    // =========================================================================
    // Increment / Decrement
    // =========================================================================
    TEST_CLASS (IncrementDecrementTests)
    {
    public:

        TEST_METHOD (Increment_Register)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x10;

            CpuOperations::Increment (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x11, cpu.RegX ());
        }

        TEST_METHOD (Increment_WrapsToZero)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0xFF;

            CpuOperations::Increment (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegX ());
            Assert::IsTrue ((bool) cpu.Status ().flags.zero);
        }

        TEST_METHOD (Increment_Memory)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Poke (0x50, 0x10);

            CpuOperations::Increment (cpu, nullptr, 0x50);

            Assert::AreEqual ((Byte) 0x11, cpu.Peek (0x50));
        }

        TEST_METHOD (Decrement_Register)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x10;

            CpuOperations::Decrement (cpu, &cpu.RegX (), 0);

            Assert::AreEqual ((Byte) 0x0F, cpu.RegX ());
        }

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



    // =========================================================================
    // Shift / Rotate
    // =========================================================================
    TEST_CLASS (ShiftRotateTests)
    {
    public:

        TEST_METHOD (ShiftLeft_Basic)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x01;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x02, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }

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

        TEST_METHOD (ShiftLeft_DoesNotRotateCarryIn)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x02;
            cpu.Status ().flags.carry = 1;

            CpuOperations::ShiftLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x04, cpu.RegA ());
        }

        TEST_METHOD (ShiftRight_Basic)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x02;

            CpuOperations::ShiftRight (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());
            Assert::IsFalse ((bool) cpu.Status ().flags.carry);
        }

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

        TEST_METHOD (RotateLeft_Bit7IntoCarry)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegA () = 0x80;

            CpuOperations::RotateLeft (cpu, &cpu.RegA (), 0);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA ());
            Assert::IsTrue ((bool) cpu.Status ().flags.carry);
        }

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



    // =========================================================================
    // Branch (direct CpuOperations calls)
    // =========================================================================
    TEST_CLASS (BranchOperationTests)
    {
    public:

        TEST_METHOD (BPL_Taken_WhenPositive)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 0;

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }

        TEST_METHOD (BPL_NotTaken_WhenNegative)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 1;
            Word originalPC = cpu.RegPC ();

            CpuOperations::Branch (cpu, Instruction (0x10), 0x9000);

            Assert::AreEqual (originalPC, cpu.RegPC ());
        }

        TEST_METHOD (BMI_Taken_WhenNegative)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.negative = 1;

            CpuOperations::Branch (cpu, Instruction (0x30), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }

        TEST_METHOD (BCS_Taken_WhenCarrySet)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.carry = 1;

            CpuOperations::Branch (cpu, Instruction (0xB0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }

        TEST_METHOD (BEQ_Taken_WhenZeroSet)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.zero = 1;

            CpuOperations::Branch (cpu, Instruction (0xF0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }

        TEST_METHOD (BNE_Taken_WhenZeroClear)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Status ().flags.zero = 0;

            CpuOperations::Branch (cpu, Instruction (0xD0), 0x9000);

            Assert::AreEqual ((Word) 0x9000, cpu.RegPC ());
        }
    };



    // =========================================================================
    // Jump
    // =========================================================================
    TEST_CLASS (JumpOperationTests)
    {
    public:

        TEST_METHOD (Jump_SetsPC)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            CpuOperations::Jump (cpu, Instruction (0x4C), 0x1234);

            Assert::AreEqual ((Word) 0x1234, cpu.RegPC ());
        }
    };
}
