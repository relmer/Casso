#include "Pch.h"

#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;







namespace CpuInitializationTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstructionSetTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InstructionSetTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Immediate_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Immediate_IsLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0xA9);

            Assert::IsTrue  (mc.isLegal);
            Assert::AreEqual ((int) Microcode::Load, (int) mc.operation);
            Assert::AreEqual ((int) GlobalAddressingMode::Immediate, (int) mc.globalAddressingMode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_ZeroPage_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_ZeroPage_IsLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0x85);

            Assert::IsTrue  (mc.isLegal);
            Assert::AreEqual ((int) Microcode::Store, (int) mc.operation);
            Assert::AreEqual ((int) GlobalAddressingMode::ZeroPage, (int) mc.globalAddressingMode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Absolute_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Absolute_IsLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0x4C);

            Assert::IsTrue  (mc.isLegal);
            Assert::AreEqual ((int) Microcode::Jump, (int) mc.operation);
            Assert::AreEqual ((int) GlobalAddressingMode::JumpAbsolute, (int) mc.globalAddressingMode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Indirect_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Indirect_IsLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0x6C);

            Assert::IsTrue  (mc.isLegal);
            Assert::AreEqual ((int) Microcode::Jump, (int) mc.operation);
            Assert::AreEqual ((int) GlobalAddressingMode::JumpIndirect, (int) mc.globalAddressingMode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IllegalOpcode_IsNotLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IllegalOpcode_IsNotLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0x02);

            Assert::IsFalse (mc.isLegal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_Immediate_IsNotLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_Immediate_IsNotLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0x89);

            Assert::IsFalse (mc.isLegal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DEX_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DEX_IsLegal)
        {
            TestCpu cpu;
            const Microcode & mc = cpu.GetMicrocode (0xCA);

            Assert::IsTrue  (mc.isLegal);
            Assert::AreEqual ((int) Microcode::Decrement, (int) mc.operation);
            Assert::AreEqual ((int) GlobalAddressingMode::SingleByteNoOperand, (int) mc.globalAddressingMode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AllBranches_AreLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AllBranches_AreLegal)
        {
            TestCpu cpu;
            Byte branchOpcodes[] = { 0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0 };

            for (Byte opcode : branchOpcodes)
            {
                const Microcode & mc = cpu.GetMicrocode (opcode);

                Assert::IsTrue (mc.isLegal);
                Assert::AreEqual ((int) Microcode::Branch, (int) mc.operation);
                Assert::AreEqual ((int) GlobalAddressingMode::Relative, (int) mc.globalAddressingMode);
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Group01_ImmediateOpcodes_AreLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Group01_ImmediateOpcodes_AreLegal)
        {
            // ORA=09, AND=29, EOR=49, ADC=69, LDA=A9, CMP=C9, SBC=E9
            TestCpu cpu;
            Byte opcodes[] = { 0x09, 0x29, 0x49, 0x69, 0xA9, 0xC9, 0xE9 };

            for (Byte opcode : opcodes)
            {
                const Microcode & mc = cpu.GetMicrocode (opcode);

                Assert::IsTrue (mc.isLegal);
                Assert::AreEqual ((int) GlobalAddressingMode::Immediate, (int) mc.globalAddressingMode);
            }
        }
    };
}
