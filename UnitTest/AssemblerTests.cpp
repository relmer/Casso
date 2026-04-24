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
    // T017: Label Resolution Tests
    // =========================================================================
    TEST_CLASS (LabelResolutionTests)
    {
    public:

        TEST_METHOD (ForwardReference_BEQ)
        {
            // BEQ target (2 bytes) + NOP (1 byte) + target: NOP
            // Branch offset = +1 (skip NOP, relative to PC after BEQ instruction)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("BEQ target\nNOP\ntarget: NOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xF0, result.bytes[0]); // BEQ opcode
            Assert::AreEqual ((Byte) 0x01, result.bytes[1]); // offset +1 (skip 1-byte NOP)
            Assert::AreEqual ((Byte) 0xEA, result.bytes[2]); // NOP
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP (target)
        }

        TEST_METHOD (BackwardReference_BNE)
        {
            // loop: INX (1 byte) + BNE loop (2 bytes)
            // Branch offset = -3 (back to INX, relative to PC after BNE instruction)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("loop: INX\nBNE loop");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xE8, result.bytes[0]); // INX opcode
            Assert::AreEqual ((Byte) 0xD0, result.bytes[1]); // BNE opcode
            Assert::AreEqual ((Byte) 0xFD, result.bytes[2]); // offset -3 (0xFD signed)
        }

        TEST_METHOD (JMP_Label_Absolute)
        {
            // JMP label (3 bytes) + label: NOP
            // label address = 0x8003
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("JMP label\nlabel: NOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x4C, result.bytes[0]); // JMP opcode
            Assert::AreEqual ((Byte) 0x03, result.bytes[1]); // lo byte of 0x8003
            Assert::AreEqual ((Byte) 0x80, result.bytes[2]); // hi byte of 0x8003
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP
        }

        TEST_METHOD (JSR_Label_Absolute)
        {
            // JSR sub (3 bytes) + NOP (1 byte) + sub: RTS (1 byte)
            // sub address = 0x8004
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("JSR sub\nNOP\nsub: RTS");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 5, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x20, result.bytes[0]); // JSR opcode
            Assert::AreEqual ((Byte) 0x04, result.bytes[1]); // lo byte of 0x8004
            Assert::AreEqual ((Byte) 0x80, result.bytes[2]); // hi byte of 0x8004
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP
            Assert::AreEqual ((Byte) 0x60, result.bytes[4]); // RTS
        }

        TEST_METHOD (Label_AppearsInSymbols)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("start: NOP\nend: NOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.symbols.size ());
            Assert::AreEqual ((Word) 0x8000, result.symbols["start"]);
            Assert::AreEqual ((Word) 0x8001, result.symbols["end"]);
        }
    };



    // =========================================================================
    // T018: Label Error Tests
    // =========================================================================
    TEST_CLASS (LabelErrorTests)
    {
    public:

        TEST_METHOD (DuplicateLabel_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("dup: NOP\ndup: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
            Assert::AreEqual (2, result.errors[0].lineNumber);
        }

        TEST_METHOD (UndefinedLabel_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("BEQ nowhere");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (LabelCollisionWithMnemonic_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (LabelCollisionWithRegisterA_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("A: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (LabelCollisionWithRegisterX_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("X: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (LabelCollisionWithRegisterY_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("Y: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }
    };



    // =========================================================================
    // T027: .org Directive Tests
    // =========================================================================
    TEST_CLASS (OrgDirectiveTests)
    {
    public:

        TEST_METHOD (Org_SetsStartAddress)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".org $C000\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0xC000, result.startAddress);
            Assert::AreEqual ((size_t) 1, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]);
        }

        TEST_METHOD (Org_BackwardFromCurrentPC_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".org $C000\nNOP\n.org $BFFF");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }
    };



    // =========================================================================
    // T028: .byte Directive Tests
    // =========================================================================
    TEST_CLASS (ByteDirectiveTests)
    {
    public:

        TEST_METHOD (Byte_EmitsMultipleBytes)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".byte $FF,$00,$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xFF, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[2]);
        }
    };



    // =========================================================================
    // T029: .word Directive Tests
    // =========================================================================
    TEST_CLASS (WordDirectiveTests)
    {
    public:

        TEST_METHOD (Word_EmitsLittleEndian)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".word $1234,$ABCD");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x34, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xCD, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xAB, result.bytes[3]);
        }
    };



    // =========================================================================
    // T030: .text Directive Tests
    // =========================================================================
    TEST_CLASS (TextDirectiveTests)
    {
    public:

        TEST_METHOD (Text_EmitsAsciiBytes)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".text \"Hello\"");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 5, result.bytes.size ());
            Assert::AreEqual ((Byte) 0x48, result.bytes[0]); // H
            Assert::AreEqual ((Byte) 0x65, result.bytes[1]); // e
            Assert::AreEqual ((Byte) 0x6C, result.bytes[2]); // l
            Assert::AreEqual ((Byte) 0x6C, result.bytes[3]); // l
            Assert::AreEqual ((Byte) 0x6F, result.bytes[4]); // o
        }

        TEST_METHOD (Text_EmptyString_EmitsZeroBytes)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".text \"\"");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size ());
        }
    };



    // =========================================================================
    // T031: Label Before Data Tests
    // =========================================================================
    TEST_CLASS (LabelBeforeDataTests)
    {
    public:

        TEST_METHOD (LabelBeforeByte_ResolvesToDataAddress)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("data: .byte $01,$02,$03");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.symbols.size ());
            Assert::AreEqual ((Word) 0x8000, result.symbols["data"]);
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
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
