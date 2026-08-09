#include "Pch.h"

#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;







namespace CpuInitializationTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstructionSetTests
    //
    //  The instruction table after construction: which opcodes are legal, which
    //  are hidden from the assembler, and which are neither.
    //
    //  THREE classifications, not two, and that is the point. An opcode can be
    //  legal and assemblable, legal but assembler-hidden -- a reserved NOP that
    //  executes and disassembles but must not be reachable by mnemonic -- or
    //  genuinely illegal. Collapsing the middle case lets a filler NOP shadow
    //  the real $EA.
    //
    //  Representative opcodes from each encoding group are checked rather than
    //  a random sample, since the table is generated from bit patterns and a
    //  bug affects a group.
    //
    //  This runs at CONSTRUCTION, before any execution, so a table built wrong
    //  fails here rather than as a mysterious wrong instruction later.
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
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0xA9);

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
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0x85);

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
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0x4C);

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
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0x6C);

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
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0x02);

            Assert::IsFalse (mc.isLegal);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Opcode89_IsHiddenUndocumentedNop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Opcode89_IsHiddenUndocumentedNop)
        {
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0x89);

            // There is no STA #imm; on the NMOS 6502 $89 is the undocumented
            // 2-byte NOP -- legal to execute but assembler-hidden, so the
            // assembler still cannot encode "STA #$xx".
            Assert::IsTrue (mc.isLegal);
            Assert::IsTrue (mc.assemblerHidden);
            Assert::AreEqual ((int) Microcode::NoOperation, (int) mc.operation);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DEX_IsLegal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DEX_IsLegal)
        {
            TestCpu            cpu;
            const Microcode  & mc  = cpu.GetMicrocode (0xCA);

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
            TestCpu  cpu;
            Byte     branchOpcodes[] = { 0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0 };

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
            TestCpu  cpu;
            Byte     opcodes[] = { 0x09, 0x29, 0x49, 0x69, 0xA9, 0xC9, 0xE9 };

            for (Byte opcode : opcodes)
            {
                const Microcode & mc = cpu.GetMicrocode (opcode);

                Assert::IsTrue (mc.isLegal);
                Assert::AreEqual ((int) GlobalAddressingMode::Immediate, (int) mc.globalAddressingMode);
            }
        }
    };
}
