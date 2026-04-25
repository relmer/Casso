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



    // =========================================================================
    // T036: Expression Tests (low byte, high byte, label+offset)
    // =========================================================================
    TEST_CLASS (ExpressionTests)
    {
    public:

        TEST_METHOD (LowByte_OfLabel)
        {
            // data at $1234, LDA #<data → operand $34 (low byte)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".org $1234\ndata: .byte $FF\nLDA #<data");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x1234, result.symbols["data"]);
            // bytes: $FF (data), $A9 (LDA), $34 (lo byte of $1234)
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[2]);
        }

        TEST_METHOD (HighByte_OfLabel)
        {
            // data at $1234, LDA #>data → operand $12 (high byte)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".org $1234\ndata: .byte $FF\nLDA #>data");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x1234, result.symbols["data"]);
            // bytes: $FF (data), $A9 (LDA), $12 (hi byte of $1234)
            Assert::AreEqual ((size_t) 3, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }

        TEST_METHOD (LabelPlusOffset)
        {
            // table at $2000, LDA table+3 → address $2003
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble (".org $2000\ntable: .byte $01,$02,$03,$04\nLDA table+3");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x2000, result.symbols["table"]);
            // bytes: {$01,$02,$03,$04} (data), $AD (LDA abs), $03, $20 (addr $2003 LE)
            Assert::AreEqual ((size_t) 7, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xAD, result.bytes[4]); // LDA absolute opcode
            Assert::AreEqual ((Byte) 0x03, result.bytes[5]); // lo byte of $2003
            Assert::AreEqual ((Byte) 0x20, result.bytes[6]); // hi byte of $2003
        }

        TEST_METHOD (LDA_Immediate_Binary)
        {
            // LDA #%10101010 → $A9, $AA
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #%10101010");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xAA, result.bytes[1]);
        }

        TEST_METHOD (LDA_Immediate_Decimal)
        {
            // LDA #255 → $A9, $FF
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #255");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, result.bytes[1]);
        }
    };



    // =========================================================================
    // T039: Comprehensive Error Tests
    // =========================================================================
    TEST_CLASS (ErrorReportingTests)
    {
    public:

        TEST_METHOD (InvalidMnemonic_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("XYZ");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
            Assert::AreEqual (1, result.errors[0].lineNumber);
        }

        TEST_METHOD (MissingOperand_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
            Assert::AreEqual (1, result.errors[0].lineNumber);
        }

        TEST_METHOD (ValueOutOfRange_ReportsError)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #$1FF");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
            Assert::AreEqual (1, result.errors[0].lineNumber);
        }

        TEST_METHOD (BranchOutOfRange_ReportsError)
        {
            // BEQ (2 bytes) + 128 NOPs → offset = 128, out of range
            Assembler asm6502 = BuildAssembler ();
            std::string source = "BEQ target\n";

            for (int i = 0; i < 128; i++)
            {
                source += "NOP\n";
            }

            source += "target: NOP";

            auto result = asm6502.Assemble (source);

            Assert::IsFalse (result.success);

            bool hasRangeError = false;

            for (const auto & e : result.errors)
            {
                if (e.message.find ("range") != std::string::npos)
                {
                    hasRangeError = true;
                }
            }

            Assert::IsTrue (hasRangeError);
        }

        TEST_METHOD (MultipleErrors_AllCollected)
        {
            // Line 1: invalid mnemonic XYZ
            // Line 2: NOP (valid)
            // Line 3: LDA (missing operand)
            // Line 4: BEQ nowhere (undefined label)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("XYZ\nNOP\nLDA\nBEQ nowhere");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 3, result.errors.size ());
            Assert::AreEqual (1, result.errors[0].lineNumber);
            Assert::AreEqual (3, result.errors[1].lineNumber);
            Assert::AreEqual (4, result.errors[2].lineNumber);
        }
    };



    // =========================================================================
    // T040: Pass 1 Error Recovery Tests
    // =========================================================================
    TEST_CLASS (Pass1ErrorRecoveryTests)
    {
    public:

        TEST_METHOD (Pass1ErrorRecovery_LabelAddressCloseToCorrect)
        {
            // Line 1: NOP (1 byte)
            // Line 2: XYZ (error, estimated 1 byte)
            // Line 3: NOP (1 byte)
            // Line 4: target: NOP (1 byte)
            // target should be at startAddr + 3 (best-effort PC estimation)
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("NOP\nXYZ\nNOP\ntarget: NOP");

            Assert::IsFalse (result.success);

            auto it = result.symbols.find ("target");
            Assert::IsTrue (it != result.symbols.end ());
            Assert::AreEqual ((Word) 0x8003, it->second);
        }
    };



    // =========================================================================
    // T043: Listing Output Tests
    // =========================================================================
    TEST_CLASS (ListingOutputTests)
    {
    public:

        TEST_METHOD (Listing_InstructionLine)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble ("LDA #$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size ());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((Word) 0x8000, line.address);
            Assert::AreEqual ((size_t) 2, line.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, line.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, line.bytes[1]);
            Assert::AreEqual (std::string ("LDA #$42"), line.sourceText);
        }

        TEST_METHOD (Listing_CommentOnlyLine)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble ("; comment");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size ());

            const auto & line = result.listing[0];
            Assert::IsFalse (line.hasAddress);
            Assert::AreEqual (std::string ("; comment"), line.sourceText);
        }

        TEST_METHOD (Listing_ByteDirective)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble (".byte $FF,$00,$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size ());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((size_t) 3, line.bytes.size ());
            Assert::AreEqual ((Byte) 0xFF, line.bytes[0]);
        }

        TEST_METHOD (Listing_LabelOnlyLine)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble ("start:\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.listing.size ());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((Word) 0x8000, line.address);
            Assert::AreEqual ((size_t) 0, line.bytes.size ());
            Assert::AreEqual (std::string ("start:"), line.sourceText);
        }

        TEST_METHOD (Listing_OrgDirective)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble (".org $C000\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.listing.size ());

            const auto & orgLine = result.listing[0];
            Assert::IsTrue (orgLine.hasAddress);
            Assert::AreEqual ((Word) 0xC000, orgLine.address);
        }

        TEST_METHOD (Listing_DisabledByDefault)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("LDA #$42\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.listing.size ());
        }

        TEST_METHOD (Listing_FormatHelper)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble ("LDA #$42");
            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size ());

            std::string formatted = Assembler::FormatListingLine (result.listing[0]);
            Assert::AreEqual (std::string ("$8000  A9 42     LDA #$42"), formatted);
        }

        TEST_METHOD (Listing_FormatHelper_NoAddress)
        {
            AssemblerOptions options = {};
            options.generateListing = true;

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble ("; comment");
            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size ());

            std::string formatted = Assembler::FormatListingLine (result.listing[0]);
            Assert::AreEqual (std::string ("                 ; comment"), formatted);
        }
    };



    // =========================================================================
    // T052: Warning Mode Tests
    // =========================================================================
    TEST_CLASS (WarningModeTests)
    {
    public:

        // Helper to build assembler with specific warning mode
        static Assembler BuildWithWarningMode (WarningMode mode)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            AssemblerOptions options = {};
            options.warningMode = mode;

            return Assembler (cpu.GetInstructionSet (), options);
        }

        // --- Unused label warning ---

        TEST_METHOD (UnusedLabel_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble ("unused: NOP\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size ());
        }

        TEST_METHOD (UnusedLabel_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble ("unused: NOP\nNOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
            Assert::AreEqual ((size_t) 0, result.warnings.size ());
        }

        TEST_METHOD (UnusedLabel_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble ("unused: NOP\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size ());
        }

        // --- Redundant .org warning ---

        TEST_METHOD (RedundantOrg_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble (".org $8000\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size ());
        }

        TEST_METHOD (RedundantOrg_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble (".org $8000\nNOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (RedundantOrg_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble (".org $8000\nNOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size ());
        }

        // --- Label differing from mnemonic only by case (FR-033a) ---

        TEST_METHOD (LabelSimilarToMnemonic_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble ("lda: NOP\nBEQ lda");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size ());
        }

        TEST_METHOD (LabelSimilarToMnemonic_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble ("lda: NOP\nBEQ lda");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size ());
        }

        TEST_METHOD (LabelSimilarToMnemonic_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble ("lda: NOP\nBEQ lda");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size ());
        }

        // --- Used label should not warn ---

        TEST_METHOD (UsedLabel_NoWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble ("loop: NOP\nBEQ loop");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size ());
        }
    };



    // =========================================================================
    // T057a: Assembler Instance Reuse Test
    // =========================================================================
    TEST_CLASS (InstanceReuseTests)
    {
    public:

        TEST_METHOD (AssembleTwice_ResultsAreIndependent)
        {
            Assembler asm6502 = BuildAssembler ();

            auto result1 = asm6502.Assemble ("LDA #$42\nSTA $10");
            auto result2 = asm6502.Assemble ("LDX #$FF\nSTX $20");

            Assert::IsTrue (result1.success);
            Assert::IsTrue (result2.success);

            // First result: LDA #$42, STA $10
            Assert::AreEqual ((size_t) 4, result1.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result1.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result1.bytes[1]);
            Assert::AreEqual ((Byte) 0x85, result1.bytes[2]);
            Assert::AreEqual ((Byte) 0x10, result1.bytes[3]);

            // Second result: LDX #$FF, STX $20
            Assert::AreEqual ((size_t) 4, result2.bytes.size ());
            Assert::AreEqual ((Byte) 0xA2, result2.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, result2.bytes[1]);
            Assert::AreEqual ((Byte) 0x86, result2.bytes[2]);
            Assert::AreEqual ((Byte) 0x20, result2.bytes[3]);

            // Symbol tables are independent
            Assert::AreEqual ((size_t) 0, result1.symbols.size ());
            Assert::AreEqual ((size_t) 0, result2.symbols.size ());
        }

        TEST_METHOD (AssembleTwice_LabelsDoNotLeak)
        {
            Assembler asm6502 = BuildAssembler ();

            auto result1 = asm6502.Assemble ("start: NOP\nend: BRK");
            auto result2 = asm6502.Assemble ("begin: NOP\nfinish: BRK");

            Assert::IsTrue (result1.success);
            Assert::IsTrue (result2.success);

            // result1 has start/end, result2 has begin/finish
            Assert::IsTrue (result1.symbols.count ("start")  > 0);
            Assert::IsTrue (result1.symbols.count ("end")    > 0);
            Assert::IsTrue (result1.symbols.count ("begin")  == 0);
            Assert::IsTrue (result2.symbols.count ("begin")  > 0);
            Assert::IsTrue (result2.symbols.count ("finish") > 0);
            Assert::IsTrue (result2.symbols.count ("start")  == 0);
        }
    };



    // =========================================================================
    // T057b: Case-Sensitive Labels Test
    // =========================================================================
    TEST_CLASS (CaseSensitiveLabelTests)
    {
    public:

        TEST_METHOD (FooAndFOO_ResolveToDifferentAddresses)
        {
            AssemblerOptions options = {};
            options.warningMode = WarningMode::NoWarn; // suppress unused label warnings

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            auto result = asm6502.Assemble (
                "foo: NOP\n"
                "FOO: NOP\n"
                "JMP foo\n"
                "JMP FOO\n"
            );

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x8000, result.symbols["foo"]);
            Assert::AreEqual ((Word) 0x8001, result.symbols["FOO"]);

            // JMP foo → bytes at offset 2: 0x4C, 0x00, 0x80
            Assert::AreEqual ((Byte) 0x4C, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[3]);
            Assert::AreEqual ((Byte) 0x80, result.bytes[4]);

            // JMP FOO → bytes at offset 5: 0x4C, 0x01, 0x80
            Assert::AreEqual ((Byte) 0x4C, result.bytes[5]);
            Assert::AreEqual ((Byte) 0x01, result.bytes[6]);
            Assert::AreEqual ((Byte) 0x80, result.bytes[7]);
        }
    };



    // =========================================================================
    // T057c: 100-Label Stress Test
    // =========================================================================
    TEST_CLASS (StressTests)
    {
    public:

        TEST_METHOD (HundredLabels_AllResolveCorrectly)
        {
            AssemblerOptions options = {};
            options.warningMode = WarningMode::NoWarn; // lots of "unused" labels otherwise

            TestCpu cpu;
            cpu.InitForTest ();
            Assembler asm6502 (cpu.GetInstructionSet (), options);

            // Generate a program with 100 labels, each with a NOP
            std::string source;

            for (int i = 0; i < 100; i++)
            {
                source += "label" + std::to_string (i) + ": NOP\n";
            }

            // Add cross-references: jump to every 10th label
            for (int i = 0; i < 100; i += 10)
            {
                source += "JMP label" + std::to_string (i) + "\n";
            }

            auto result = asm6502.Assemble (source);

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 100, result.symbols.size ());

            // Verify labels are at expected addresses
            for (int i = 0; i < 100; i++)
            {
                std::string name = "label" + std::to_string (i);
                Word expectedAddr = 0x8000 + (Word) i;

                Assert::AreEqual (expectedAddr, result.symbols[name],
                    (std::wstring (L"Label: ") + std::wstring (name.begin (), name.end ())).c_str ());
            }
        }
    };



    // =========================================================================
    // T057d: Empty Source Text Test
    // =========================================================================
    TEST_CLASS (EmptySourceTests)
    {
    public:

        TEST_METHOD (EmptySource_ReturnsSuccessWithZeroBytes)
        {
            Assembler asm6502 = BuildAssembler ();
            auto result = asm6502.Assemble ("");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size ());
            Assert::AreEqual ((size_t) 0, result.errors.size ());
            Assert::AreEqual ((size_t) 0, result.symbols.size ());
        }
    };
}
