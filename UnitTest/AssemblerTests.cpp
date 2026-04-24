#include "Pch.h"

#include "TestHelpers.h"
#include "Assembler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace AssemblerTests
{
    // Helper to build an Assembler from a TestCpu's instruction set
    static Assembler BuildAssembler ()
    {
        TestCpu cpu;
        cpu.InitForTest ();
        return Assembler (cpu.GetInstructionSet ());
    }



    // =========================================================================
    // T012: Instruction Encoding Tests
    // =========================================================================
    TEST_CLASS (InstructionEncodingTests)
    {
    public:

        TEST_METHOD (LDA_Immediate)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }

        TEST_METHOD (STA_ZeroPage)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("STA $10");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x85, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (JMP_Absolute)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("JMP $1234");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x4C, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }

        TEST_METHOD (ROL_Accumulator)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("ROL A");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x2A, result.bytes[0]);
        }

        TEST_METHOD (NOP_Implied)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("NOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]);
        }

        TEST_METHOD (LDA_ZeroPageX)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA $10,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xB5, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (LDA_AbsoluteX)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA $1234,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xBD, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }

        TEST_METHOD (LDA_AbsoluteY)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA $1234,Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xB9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }

        TEST_METHOD (STA_ZeroPageX)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("STA $10,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x95, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (STX_ZeroPageY)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("STX $10,Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x96, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (LDA_ZeroPageXIndirect)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA ($10,X)");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA1, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (LDA_ZeroPageIndirectY)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA ($10),Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xB1, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }

        TEST_METHOD (JMP_Indirect)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("JMP ($1234)");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x6C, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }
    };



    // =========================================================================
    // T013: Comment and Whitespace Handling Tests
    // =========================================================================
    TEST_CLASS (CommentAndWhitespaceTests)
    {
    public:

        TEST_METHOD (FullLineComment_ProducesZeroBytes)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("; this is a comment");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size ());
        }

        TEST_METHOD (InlineComment_AssemblesCorrectly)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #$42 ; load value");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }

        TEST_METHOD (BlankLines_ProduceSameOutput)
        {
            Assembler asm6502 = BuildAssembler ();
            auto withBlanks    = asm6502.Assemble ("LDA #$42\n\n\nSTA $10");
            auto withoutBlanks = asm6502.Assemble ("LDA #$42\nSTA $10");

            Assert::IsTrue (withBlanks.success);
            Assert::IsTrue (withoutBlanks.success);
            Assert::AreEqual (withoutBlanks.bytes.size (), withBlanks.bytes.size ());

            for (size_t i = 0; i < withBlanks.bytes.size (); i++)
            {
                Assert::AreEqual (withoutBlanks.bytes[i], withBlanks.bytes[i]);
            }
        }

        TEST_METHOD (VariedIndentation_AssemblesCorrectly)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("  LDA   #$42  ");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }
    };
}
