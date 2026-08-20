#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AssemblerTests
{
    // Helper to build an Assembler from a TestCpu's instruction set
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler()
    {
        TestCpu cpu;
        cpu.InitForTest();
        return Assembler (cpu.GetInstructionSet());
    }


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler65C02 — assembler over the CMOS 65C02 instruction table
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler65C02()
    {
        TestCpu65C02 cpu;
        cpu.InitForTest();
        return Assembler (cpu.GetInstructionSet());
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstructionEncodingTests
    //
    //  One instruction in, the exact bytes out -- the assembler's most basic
    //  contract.
    //
    //  Coverage is by ADDRESSING MODE rather than by mnemonic. The 6502's
    //  opcodes are largely a cross product of operations and modes, and the
    //  assembler is table-driven along the same axis, so a bug lives in a mode
    //  far more often than in a particular instruction. Testing every mnemonic
    //  in one mode would be a hundred tests exercising one code path.
    //
    //  Each test asserts the opcode AND the operand bytes, not just the size.
    //  A wrong opcode of the right length is exactly the failure that gets
    //  through a size-only check and produces a program that runs and does
    //  something else.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InstructionEncodingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Immediate
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Immediate)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA #$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }



        //  AN OPCODE'S CASE NEVER MATTERS, and no flag is involved in that.
        //
        //  THE HELP MAKES THIS CLAIM ON as65's BEHALF. `-i` is as65's request
        //  for case-insensitive opcodes, and it is accepted here as a no-op
        //  BECAUSE the behavior it asks for is already unconditional, not
        //  because it is unimplemented -- which is what the help used to say.
        //  The difference matters to anyone reading the flag list to decide
        //  whether they can rely on lowercase source: they can, and always
        //  could.
        //
        //  Assembled with no options at all, so nothing but the default is
        //  being measured.
        TEST_METHOD (OpcodeCaseIsIgnoredWithNoFlagAskingForIt)
        {
            Assembler  asm6502 = BuildAssembler();
            auto       lower   = asm6502.Assemble ("lda #$42");
            auto       upper   = asm6502.Assemble ("LDA #$42");
            auto       mixed   = asm6502.Assemble ("Lda #$42");

            Assert::IsTrue (lower.success, L"lowercase assembles with no flag asked for");
            Assert::IsTrue (mixed.success, L"and so does mixed case");
            Assert::IsTrue (lower.bytes == upper.bytes, L"to the same bytes as uppercase");
            Assert::IsTrue (mixed.bytes == upper.bytes);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_ZeroPage
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_ZeroPage)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("STA $10");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0x85, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Absolute
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Absolute)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("JMP $1234");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0x4C, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ROL_Accumulator
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ROL_Accumulator)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("ROL A");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.bytes.size());
            Assert::AreEqual ((Byte) 0x2A, result.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NOP_Implied
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NOP_Implied)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("NOP");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPageX
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPageX)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA $10,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xB5, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_AbsoluteX
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_AbsoluteX)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA $1234,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xBD, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_AbsoluteY
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_AbsoluteY)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA $1234,Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xB9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_ZeroPageX
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_ZeroPageX)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("STA $10,X");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0x95, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STX_ZeroPageY
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STX_ZeroPageY)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("STX $10,Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0x96, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPageXIndirect
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPageXIndirect)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA ($10,X)");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA1, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPageIndirectY
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPageIndirectY)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA ($10),Y");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xB1, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Indirect
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Indirect)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("JMP ($1234)");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0x6C, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LabelResolutionTests
    //
    //  Labels resolving to the right addresses -- which is what the whole
    //  two-pass design exists for.
    //
    //  FORWARD references are the point of most of these. A backward reference
    //  is trivially resolvable during a single walk; a forward one is not, and
    //  it is the reason pass 1 sizes before pass 2 emits. A bug in that split
    //  shows up here and almost nowhere else.
    //
    //  Branch offsets get particular attention because they are relative to
    //  the PC AFTER the instruction, not to the instruction. That off-by-two
    //  is the classic 6502 assembler bug, and it produces code that jumps two
    //  bytes wrong -- often into the middle of an instruction, where the
    //  failure looks nothing like a branch problem.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LabelResolutionTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ForwardReference_BEQ
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ForwardReference_BEQ)
        {
            // BEQ target (2 bytes) + NOP (1 byte) + target: NOP
            // Branch offset = +1 (skip NOP, relative to PC after BEQ instruction)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 BEQ target
                                    NOP
                            target: NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0xF0, result.bytes[0]); // BEQ opcode
            Assert::AreEqual ((Byte) 0x01, result.bytes[1]); // offset +1 (skip 1-byte NOP)
            Assert::AreEqual ((Byte) 0xEA, result.bytes[2]); // NOP
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP (target)
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BackwardReference_BNE
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BackwardReference_BNE)
        {
            // loop: INX (1 byte) + BNE loop (2 bytes)
            // Branch offset = -3 (back to INX, relative to PC after BNE instruction)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(         loop:   INX
                                    BNE loop
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xE8, result.bytes[0]); // INX opcode
            Assert::AreEqual ((Byte) 0xD0, result.bytes[1]); // BNE opcode
            Assert::AreEqual ((Byte) 0xFD, result.bytes[2]); // offset -3 (0xFD signed)
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Label_Absolute
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Label_Absolute)
        {
            // JMP label (3 bytes) + label: NOP
            // label address = 0x0003
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 JMP label
                            label:  NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0x4C, result.bytes[0]); // JMP opcode
            Assert::AreEqual ((Byte) 0x03, result.bytes[1]); // lo byte of 0x0003
            Assert::AreEqual ((Byte) 0x00, result.bytes[2]); // hi byte of 0x0003
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JSR_Label_Absolute
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JSR_Label_Absolute)
        {
            // JSR sub (3 bytes) + NOP (1 byte) + sub: RTS (1 byte)
            // sub address = 0x0004
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 JSR sub
                                    NOP
                            sub:    RTS
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 5, result.bytes.size());
            Assert::AreEqual ((Byte) 0x20, result.bytes[0]); // JSR opcode
            Assert::AreEqual ((Byte) 0x04, result.bytes[1]); // lo byte of 0x0004
            Assert::AreEqual ((Byte) 0x00, result.bytes[2]); // hi byte of 0x0004
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP
            Assert::AreEqual ((Byte) 0x60, result.bytes[4]); // RTS
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Label_AppearsInSymbols
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Label_AppearsInSymbols)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(         start:  NOP
                            end:    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 5, result.symbols.size());  // 2 labels + 3 built-ins
            Assert::AreEqual ((Word) 0x0000, result.symbols["start"]);
            Assert::AreEqual ((Word) 0x0001, result.symbols["end"]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LabelErrorTests
    //
    //  What happens when a label is WRONG: undefined, duplicated, or out of
    //  branch range.
    //
    //  Every case asserts that assembly FAILS as well as what it reports. An
    //  assembler that emits a diagnostic and still returns success produces a
    //  binary nobody trusts and a build that does not stop -- the failure flag
    //  is as much a contract as the message.
    //
    //  Branch range is checked here rather than with the encoding tests because
    //  it is a label question: the offset is only knowable once the label
    //  resolves, so a too-distant target is caught in pass 2 and must be
    //  reported rather than silently truncated to eight bits.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LabelErrorTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DuplicateLabel_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DuplicateLabel_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(         dup:    NOP
                            dup:    NOP
                )");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (2, result.errors[0].lineNumber);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UndefinedLabel_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UndefinedLabel_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("BEQ nowhere");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelCollisionWithMnemonic_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelCollisionWithMnemonic_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelCollisionWithRegisterA_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelCollisionWithRegisterA_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("A: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelCollisionWithRegisterX_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelCollisionWithRegisterX_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("X: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelCollisionWithRegisterY_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelCollisionWithRegisterY_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("Y: NOP");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OrgDirectiveTests
    //
    //  .org setting the program counter, and everything downstream following
    //  it.
    //
    //  These matter more than they look. .org moves the PC, so it changes every
    //  label defined after it and every absolute address that references one --
    //  a bug here does not corrupt one instruction, it shifts the whole program
    //  and produces code that assembles cleanly and jumps into nothing.
    //
    //  The tests assert the reported start address as well as the emitted
    //  bytes, since the output image's placement is what a loader uses and it
    //  can be wrong independently of the code.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OrgDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Org_SetsStartAddress
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Org_SetsStartAddress)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 .org $C000
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0xC000, result.startAddress);
            Assert::AreEqual ((size_t) 1, result.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Org_BackwardFromCurrentPC_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Org_BackwardFromCurrentPC_ReportsError)
        {
            // Backward .org is allowed for multi-segment assembly (flat binary output)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 .org $C000
                                    NOP
                                    .org $BFFF
                )");

            Assert::IsTrue (result.success);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ByteDirectiveTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ByteDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Byte_EmitsMultipleBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Byte_EmitsMultipleBytes)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (".byte $FF,$00,$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xFF, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[2]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WordDirectiveTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WordDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Word_EmitsLittleEndian
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Word_EmitsLittleEndian)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (".word $1234,$ABCD");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0x34, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xCD, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xAB, result.bytes[3]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TextDirectiveTests
    //
    //  String directives emitting the right bytes, including the Apple II's
    //  high-bit conventions.
    //
    //  The high bit is the substance here. Apple II text sets bit 7 for normal
    //  display, and the several string directives differ precisely in whether
    //  they set it -- so a test that only checked the character values would
    //  pass on output the machine renders as inverse or flashing text.
    //
    //  Terminator handling is likewise asserted rather than assumed: a
    //  directive that adds a NUL and one that does not are different tools, and
    //  the difference is invisible until a routine runs off the end of a string.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TextDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Text_EmitsAsciiBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Text_EmitsAsciiBytes)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (".text \"Hello\"");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 5, result.bytes.size());
            Assert::AreEqual ((Byte) 0x48, result.bytes[0]); // H
            Assert::AreEqual ((Byte) 0x65, result.bytes[1]); // e
            Assert::AreEqual ((Byte) 0x6C, result.bytes[2]); // l
            Assert::AreEqual ((Byte) 0x6C, result.bytes[3]); // l
            Assert::AreEqual ((Byte) 0x6F, result.bytes[4]); // o
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Text_EmptyString_EmitsZeroBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Text_EmptyString_EmitsZeroBytes)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (".text \"\"");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LabelBeforeDataTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LabelBeforeDataTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelBeforeByte_ResolvesToDataAddress
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelBeforeByte_ResolvesToDataAddress)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("data: .byte $01,$02,$03");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 4, result.symbols.size());  // 1 label + 3 built-ins
            Assert::AreEqual ((Word) 0x0000, result.symbols["data"]);
            Assert::AreEqual ((size_t) 3, result.bytes.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CommentAndWhitespaceTests
    //
    //  Text the assembler must IGNORE: comments, blank lines, and leading or
    //  trailing whitespace.
    //
    //  Unglamorous and load-bearing. Real period sources are full of comment
    //  columns, tab-aligned operands, and blank separator lines, so a lexer
    //  that mishandles any of them fails on almost every genuine file while
    //  passing every hand-written test case.
    //
    //  Column 0 gets specific attention because whitespace there is
    //  SIGNIFICANT: a word starting in column 0 may be a label, while the same
    //  word indented is a mnemonic. That is the one place the assembler cannot
    //  simply trim and move on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CommentAndWhitespaceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  FullLineComment_ProducesZeroBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (FullLineComment_ProducesZeroBytes)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("; this is a comment");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  InlineComment_AssemblesCorrectly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (InlineComment_AssemblesCorrectly)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA #$42 ; load value");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BlankLines_ProduceSameOutput
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BlankLines_ProduceSameOutput)
        {
            Assembler asm6502 = BuildAssembler();
            auto withBlanks    = asm6502.Assemble (
                R"(                 LDA #$42

                                    STA $10
                )");

            auto withoutBlanks = asm6502.Assemble (
                R"(                 LDA #$42
                                    STA $10
                )");

            Assert::IsTrue (withBlanks.success);
            Assert::IsTrue (withoutBlanks.success);
            Assert::AreEqual (withoutBlanks.bytes.size(), withBlanks.bytes.size());

            for (size_t i = 0; i < withBlanks.bytes.size(); i++)
            {
                Assert::AreEqual (withoutBlanks.bytes[i], withBlanks.bytes[i]);
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  VariedIndentation_AssemblesCorrectly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (VariedIndentation_AssemblesCorrectly)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("  LDA   #$42  ");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExpressionTests
    //
    //  Operand expressions: arithmetic, precedence, the low and high byte
    //  selectors, and the program counter.
    //
    //  PRECEDENCE is the reason most of these exist. The evaluator's binding
    //  levels are a table now, but the correct answer for a mixed expression is
    //  a fact about the language rather than about the table -- so the tests
    //  pin the results and would catch a row edited to the wrong level.
    //
    //  The `<` and `>` selectors are covered specifically because they are
    //  context-dependent: the same characters are comparisons after a value and
    //  byte selectors otherwise, so these tests are what keep the tokenizer's
    //  last-was-value rule honest.
    //
    //  `*` as the program counter is here for the same reason -- it is
    //  multiplication in every other position.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExpressionTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LowByte_OfLabel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LowByte_OfLabel)
        {
            // data at $1234, LDA #<data → operand $34 (low byte)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 .org $1234
                            data:   .byte $FF
                                    LDA #<data
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x1234, result.symbols["data"]);
            // bytes: $FF (data), $A9 (LDA), $34 (lo byte of $1234)
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x34, result.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  HighByte_OfLabel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (HighByte_OfLabel)
        {
            // data at $1234, LDA #>data → operand $12 (high byte)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 .org $1234
                            data:   .byte $FF
                                    LDA #>data
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x1234, result.symbols["data"]);
            // bytes: $FF (data), $A9 (LDA), $12 (hi byte of $1234)
            Assert::AreEqual ((size_t) 3, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x12, result.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelPlusOffset
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelPlusOffset)
        {
            // table at $2000, LDA table+3 → address $2003
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 .org $2000
                            table:  .byte $01,$02,$03,$04
                                    LDA table+3
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x2000, result.symbols["table"]);
            // bytes: {$01,$02,$03,$04} (data), $AD (LDA abs), $03, $20 (addr $2003 LE)
            Assert::AreEqual ((size_t) 7, result.bytes.size());
            Assert::AreEqual ((Byte) 0xAD, result.bytes[4]); // LDA absolute opcode
            Assert::AreEqual ((Byte) 0x03, result.bytes[5]); // lo byte of $2003
            Assert::AreEqual ((Byte) 0x20, result.bytes[6]); // hi byte of $2003
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Immediate_Binary
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Immediate_Binary)
        {
            // LDA #%10101010 → $A9, $AA
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA #%10101010");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xAA, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Immediate_Decimal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Immediate_Decimal)
        {
            // LDA #255 → $A9, $FF
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA #255");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, result.bytes[1]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ErrorReportingTests
    //
    //  Diagnostics naming the right LINE and saying something useful.
    //
    //  The line number is asserted, not just the presence of an error, because
    //  a diagnostic pointing at the wrong line is worse than a vague one: it
    //  sends the reader to correct code. Several of these guard specific
    //  regressions where an error was attributed to the end of the file or to
    //  a macro's expansion site rather than to its definition.
    //
    //  Message text is checked loosely -- by substring -- so wording can be
    //  improved without breaking tests, while the fact being reported stays
    //  pinned.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ErrorReportingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  InvalidMnemonic_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (InvalidMnemonic_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("    XYZ");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (1, result.errors[0].lineNumber);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MissingOperand_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MissingOperand_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (1, result.errors[0].lineNumber);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ValueOutOfRange_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ValueOutOfRange_ReportsError)
        {
            // Immediate mode now truncates to 8 bits silently (AS65 behavior)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("LDA #$1FF");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BranchOutOfRange_ReportsError
        //
        //  A branch whose target is one byte past the reachable range must be
        //  an error, not a truncated offset.
        //
        //  The source is generated rather than written out because the boundary
        //  is 128 bytes away -- and it is built to land EXACTLY one past the
        //  limit, since a test at some comfortably-distant target would pass
        //  against an off-by-one range check.
        //
        //  Silently truncating the offset is the failure this guards: the
        //  program would assemble cleanly and branch to an address unrelated to
        //  the label, which is nearly impossible to diagnose from the symptom.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BranchOutOfRange_ReportsError)
        {
            bool            hasRangeError = false;
            AssemblyResult  result;



            // BEQ (2 bytes) + 128 NOPs → offset = 128, out of range
            Assembler asm6502 = BuildAssembler();
            std::string source = "BEQ target\n";

            for (int i = 0; i < 128; i++)
            {
                source += "NOP\n";
            }

            source += "target: NOP";

            result = asm6502.Assemble (source);

            Assert::IsFalse (result.success);


            for (const auto & e : result.errors)
            {
                if (e.message.find ("range") != std::string::npos)
                {
                    hasRangeError = true;
                }
            }

            Assert::IsTrue (hasRangeError);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MultipleErrors_AllCollected
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MultipleErrors_AllCollected)
        {
            // Line 1: invalid mnemonic XYZ
            // Line 2: NOP (valid)
            // Line 3: LDA (missing operand)
            // Line 4: BEQ nowhere (undefined label)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 XYZ
                                    NOP
                                    LDA
                                    BEQ nowhere
                )");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 3, result.errors.size());
            Assert::AreEqual (1, result.errors[0].lineNumber);
            Assert::AreEqual (3, result.errors[1].lineNumber);
            Assert::AreEqual (4, result.errors[2].lineNumber);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Pass1ErrorRecoveryTests
    //
    //  A pass-1 error must not stop the walk: the assembler keeps going and
    //  reports everything wrong with the file.
    //
    //  Stopping at the first error turns fixing a source into one edit-build
    //  cycle per mistake. Recovery is what lets a build report all of them at
    //  once, and it is easy to lose -- a bail added for one error class quietly
    //  truncates the diagnostics for every later line.
    //
    //  So these assert the COUNT and the later lines' diagnostics, not merely
    //  that the assembly failed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Pass1ErrorRecoveryTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Pass1ErrorRecovery_LabelAddressCloseToCorrect
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Pass1ErrorRecovery_LabelAddressCloseToCorrect)
        {
            // Line 1: NOP (1 byte)
            // Line 2: XYZ (error, estimated 1 byte)
            // Line 3: NOP (1 byte)
            // Line 4: target: NOP (1 byte)
            // target should be at startAddr + 3 (best-effort PC estimation)
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 NOP
                                    XYZ
                                    NOP
                            target: NOP
                )");

            Assert::IsFalse (result.success);

            auto it = result.symbols.find ("target");
            Assert::IsTrue (it != result.symbols.end());
            Assert::AreEqual ((Word) 0x0003, it->second);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ListingOutputTests
    //
    //  The assembly listing's content and its exact column layout.
    //
    //  Listings are read POSITIONALLY, by people and by tools, so the column
    //  widths are a specification rather than a preference -- which is why
    //  these tests assert character offsets and not merely that the right
    //  values appear somewhere on the line.
    //
    //  The AS65 layout is matched deliberately, so listings from this assembler
    //  can be diffed against reference output from the tool it is compatible
    //  with. That comparison is only meaningful if the columns line up.
    //
    //  Optional elements -- cycle counts, macro-expansion markers, page breaks
    //  -- are covered because each SHIFTS the line, and a shift is exactly what
    //  breaks a positional reader.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ListingOutputTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_InstructionLine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_InstructionLine)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble ("LDA #$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((Word) 0x0000, line.address);
            Assert::AreEqual ((size_t) 2, line.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, line.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, line.bytes[1]);
            Assert::AreEqual (std::string ("LDA #$42"), line.sourceText);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_CommentOnlyLine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_CommentOnlyLine)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble ("; comment");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size());

            const auto & line = result.listing[0];
            Assert::IsFalse (line.hasAddress);
            Assert::AreEqual (std::string ("; comment"), line.sourceText);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_ByteDirective
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_ByteDirective)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble (".byte $FF,$00,$42");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((size_t) 3, line.bytes.size());
            Assert::AreEqual ((Byte) 0xFF, line.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_LabelOnlyLine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_LabelOnlyLine)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble (
                R"(         start:
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.listing.size());

            const auto & line = result.listing[0];
            Assert::IsTrue (line.hasAddress);
            Assert::AreEqual ((Word) 0x0000, line.address);
            Assert::AreEqual ((size_t) 0, line.bytes.size());
            Assert::AreEqual (std::string ("         start:"), line.sourceText);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_OrgDirective
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_OrgDirective)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble (
                R"(                 .org $C000
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 3, result.listing.size());

            const auto & orgLine = result.listing[0];
            Assert::IsTrue (orgLine.hasAddress);
            Assert::AreEqual ((Word) 0xC000, orgLine.address);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_DisabledByDefault
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_DisabledByDefault)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                R"(                 LDA #$42
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.listing.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_FormatHelper
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_FormatHelper)
        {
            AssemblerOptions  options   = {};
            TestCpu           cpu;
            std::string       formatted;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble ("LDA #$42");
            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size());

            formatted = Assembler::FormatListingLine (result.listing[0]);
            Assert::AreEqual (std::string ("    1 0000   A9 42     LDA #$42"), formatted);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_FormatHelper_NoAddress
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_FormatHelper_NoAddress)
        {
            AssemblerOptions  options   = {};
            TestCpu           cpu;
            std::string       formatted;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble ("; comment");
            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.listing.size());

            formatted = Assembler::FormatListingLine (result.listing[0]);
            Assert::AreEqual (std::string ("    1                  ; comment"), formatted);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_ColumnLayout_MatchesAS65
        //
        //  Pins the listing's field positions against the AS65 layout, column
        //  by column.
        //
        //  This is the test that makes the layout a contract. Everything else
        //  checks that the right VALUES appear; this checks they appear at the
        //  right OFFSETS, which is what a positional reader -- or a diff
        //  against reference output from the original tool -- actually depends
        //  on.
        //
        //  It is deliberately brittle. A change to any field width is supposed
        //  to fail here, because that change breaks every consumer of the
        //  format whether or not it looks fine on screen.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_ColumnLayout_MatchesAS65)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            std::string       line1;
            std::string       line2;
            std::string       line3;
            std::string       line4;
            std::string       line5;
            options.generateListing = true;

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble (
                ".org $1000\n"
                "LDA #$05\n"
                "CLC\n"
                "ADC #$03\n"
                "STA $2000");

            Assert::IsTrue (result.success);

            // Verify key listing lines match AS65 column layout
            // Line 1: .org $1000 — address 1000, no bytes
            line1 = Assembler::FormatListingLine (result.listing[0]);
            Assert::AreEqual (std::string ("    1 1000             .org $1000"), line1);

            // Line 2: LDA #$05 — address 1000, bytes A9 05
            line2 = Assembler::FormatListingLine (result.listing[1]);
            Assert::AreEqual (std::string ("    2 1000   A9 05     LDA #$05"), line2);

            // Line 3: CLC — address 1002, byte 18
            line3 = Assembler::FormatListingLine (result.listing[2]);
            Assert::AreEqual (std::string ("    3 1002   18        CLC"), line3);

            // Line 4: ADC #$03 — address 1003, bytes 69 03
            line4 = Assembler::FormatListingLine (result.listing[3]);
            Assert::AreEqual (std::string ("    4 1003   69 03     ADC #$03"), line4);

            // Line 5: STA $2000 — address 1005, bytes 8D 00 20
            line5 = Assembler::FormatListingLine (result.listing[4]);
            Assert::AreEqual (std::string ("    5 1005   8D 00 20  STA $2000"), line5);

            // Verify source text starts at column 24 (index 23)
            Assert::AreEqual ('.', line1[23]);
            Assert::AreEqual ('L', line2[23]);
            Assert::AreEqual ('S', line5[23]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_Pagination
        //
        //  What -h asks for, checked where it happens.
        //
        //  The page height reached the assembler options and was read by
        //  NOTHING: no code anywhere in the tool broke a listing into pages, so
        //  a listing produced with -h 10 and one produced with no flag at all
        //  were byte-for-byte identical. Pagination had to be built before the
        //  flag could be fixed -- parsing the number correctly would have
        //  changed nothing on its own.
        //
        ////////////////////////////////////////////////////////////////////////////////

        static AssemblyResult ListingOfNops (int count)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            std::string       source;

            options.generateListing = true;
            cpu.InitForTest();

            //  No trailing newline: it would make an empty final line that the
            //  listing renders, and the count these tests reason about has to
            //  be the one asked for.
            for (int i = 0; i < count; i++)
            {
                source += (i == 0) ? "NOP" : "\nNOP";
            }

            Assembler  asm6502 (cpu.GetInstructionSet(), options);

            return asm6502.Assemble (source);
        }


        static size_t CountPageBreaks (const std::string & listing)
        {
            size_t  breaks = 0;

            for (char c : listing)
            {
                if (c == '\f')
                {
                    breaks++;
                }
            }

            return breaks;
        }


        TEST_METHOD (Listing_NoPageHeight_IsOneContinuousPage)
        {
            AssemblyResult  result = ListingOfNops (42);

            Assert::AreEqual ((size_t) 42, result.listing.size());
            Assert::AreEqual ((size_t) 0, CountPageBreaks (Assembler::FormatListing (result)),
                              L"the default must not start breaking pages");
        }


        //  42 lines at 10 to a page is four breaks: after lines 10, 20, 30 and
        //  40. Not five -- the last two lines do not fill a page, and a form
        //  feed with nothing after it is a blank page on the printer.
        TEST_METHOD (Listing_PageHeight_BreaksEveryThatManyLines)
        {
            AssemblyResult  result = ListingOfNops (42);
            std::string     paged  = Assembler::FormatListing (result, 10);

            Assert::AreEqual ((size_t) 4, CountPageBreaks (paged));
        }


        //  The flag has to reach the page it names. A height of 1 breaks
        //  between every pair of lines, which no off-by-one can also produce.
        TEST_METHOD (Listing_PageHeightOfOne_BreaksBetweenEveryLine)
        {
            AssemblyResult  result = ListingOfNops (5);
            std::string     paged  = Assembler::FormatListing (result, 1);

            Assert::AreEqual ((size_t) 4, CountPageBreaks (paged));
        }


        //  A listing that fits has no break in it at all, which is what stops
        //  pagination from putting a form feed on the front of a short one.
        TEST_METHOD (Listing_ShorterThanThePage_HasNoBreakAtAll)
        {
            AssemblyResult  result = ListingOfNops (5);

            Assert::AreEqual ((size_t) 0, CountPageBreaks (Assembler::FormatListing (result, 60)));
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_MacroExpansion_HasPrefix
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_MacroExpansion_HasPrefix)
        {
            std::string  formatted;



            // Build a line manually with isMacroExpansion = true
            AssemblyLine macroLine = {};
            macroLine.lineNumber       = 7;
            macroLine.hasAddress       = true;
            macroLine.address          = 0x2000;
            macroLine.bytes            = { 0xA9, 0xFF };
            macroLine.sourceText       = "LDA #$FF";
            macroLine.isMacroExpansion = true;

            formatted = Assembler::FormatListingLine (macroLine);

            // Column 23 (index 22) should be '>' for macro expansion
            Assert::AreEqual ('>', formatted[22]);

            // Full expected format: linenum(5) space addr(4) spaces(3) bytes(9) > source
            Assert::AreEqual (std::string ("    7 2000   A9 FF    >LDA #$FF"), formatted);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Listing_SymbolTable_Format
        //
        //  The symbol table's rendering: sorted, aligned, and annotated by kind.
        //
        //  All three symbol KINDS are present in the fixture on purpose -- a
        //  label, an equ, and a set. They are formatted differently because
        //  they mean different things, and a table that rendered them
        //  identically would hide that a supposed constant is actually
        //  reassignable.
        //
        //  Two symbols share an address, which is the case that catches an
        //  ordering implemented as a sort on value alone: without a stable
        //  tiebreak the output would vary between runs and the test would flake
        //  rather than fail.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Listing_SymbolTable_Format)
        {
            std::unordered_map<std::string, Word>        symbols;
            std::unordered_map<std::string, SymbolKind>  symbolKinds;
            std::string                                  table;
            symbols["START"]   = 0x1000;
            symbols["COUNTER"] = 0x0005;
            symbols["LABEL"]   = 0x1000;

            symbolKinds["START"]   = SymbolKind::Label;
            symbolKinds["COUNTER"] = SymbolKind::Set;
            symbolKinds["LABEL"]   = SymbolKind::Equ;

            table = Assembler::FormatSymbolTable (symbols, symbolKinds);

            // Should be alphabetically sorted
            size_t posCounter = table.find ("*COUNTER");
            size_t posLabel   = table.find ("LABEL");
            size_t posStart   = table.find ("START");

            Assert::IsTrue (posCounter != std::string::npos, L"COUNTER not found");
            Assert::IsTrue (posLabel != std::string::npos, L"LABEL not found");
            Assert::IsTrue (posStart != std::string::npos, L"START not found");
            Assert::IsTrue (posCounter < posLabel, L"COUNTER should sort before LABEL");
            Assert::IsTrue (posLabel < posStart, L"LABEL should sort before START");

            // Verify * prefix on Set symbol
            Assert::IsTrue (table.find ("*COUNTER") != std::string::npos, L"Set symbol should have * prefix");

            // Verify $ prefix on values
            Assert::IsTrue (table.find ("$1000") != std::string::npos, L"Values should have $ prefix");
            Assert::IsTrue (table.find ("$0005") != std::string::npos, L"Values should have $ prefix");

            // Verify no = separator (AS65 format uses spacing, not =)
            Assert::IsTrue (table.find ("=") == std::string::npos, L"AS65 format has no = separator");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WarningModeTests
    //
    //  The three warning modes, and that each diagnostic honors all three.
    //
    //  Coverage is a MATRIX -- every warning kind against Warn, NoWarn, and
    //  FatalWarnings -- because the modes are applied at a single choke point
    //  and a diagnostic that bypasses it looks correct in the default mode
    //  while ignoring the other two entirely. That is the exact bug this shape
    //  of test catches and a per-warning test would not.
    //
    //  FatalWarnings asserts the failure FLAG as well as the diagnostic
    //  landing on the error list, since the point of the mode is a non-zero
    //  exit for a build script.
    //
    //  NoWarn asserts the warning list is empty rather than that it is
    //  filtered later -- suppressed means never recorded.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WarningModeTests)
    {
    public:

        // Helper to build assembler with specific warning mode
        static Assembler BuildWithWarningMode (WarningMode mode)
        {
            TestCpu           cpu;
            AssemblerOptions  options;
            cpu.InitForTest();

            options = {};
            options.warningMode = mode;

            return Assembler (cpu.GetInstructionSet(), options);
        }

        // Unused label warning
        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnusedLabel_WarnMode_RecordedAsWarning
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnusedLabel_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble (
                R"(         unused: NOP
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnusedLabel_FatalWarnings_PromotedToError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnusedLabel_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble (
                R"(         unused: NOP
                                    NOP
                )");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual ((size_t) 0, result.warnings.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnusedLabel_NoWarn_Suppressed
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnusedLabel_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble (
                R"(         unused: NOP
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size());
        }

        // Redundant .org warning
        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RedundantOrg_WarnMode_RecordedAsWarning
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RedundantOrg_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble (
                R"(                 .org $C000
                                    .org $C000
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RedundantOrg_FatalWarnings_PromotedToError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RedundantOrg_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble (
                R"(                 .org $C000
                                    .org $C000
                                    NOP
                )");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RedundantOrg_NoWarn_Suppressed
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RedundantOrg_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble (
                R"(                 .org $8000
                                    NOP
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size());
        }

        // Label differing from mnemonic only by case (FR-033a)
        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelSimilarToMnemonic_WarnMode_RecordedAsWarning
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelSimilarToMnemonic_WarnMode_RecordedAsWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble (
                R"(         lda:    NOP
                                    BEQ lda
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 1, result.warnings.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelSimilarToMnemonic_FatalWarnings_PromotedToError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelSimilarToMnemonic_FatalWarnings_PromotedToError)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::FatalWarnings);
            auto result = asm6502.Assemble (
                R"(         lda:    NOP
                                    BEQ lda
                )");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((size_t) 1, result.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelSimilarToMnemonic_NoWarn_Suppressed
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelSimilarToMnemonic_NoWarn_Suppressed)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::NoWarn);
            auto result = asm6502.Assemble (
                R"(         lda:    NOP
                                    BEQ lda
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size());
        }

        // Used label should not warn
        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UsedLabel_NoWarning
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UsedLabel_NoWarning)
        {
            Assembler asm6502 = BuildWithWarningMode (WarningMode::Warn);
            auto result = asm6502.Assemble (
                R"(         loop:   NOP
                                    BEQ loop
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.warnings.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstanceReuseTests
    //
    //  One Assembler assembling twice must produce two independent results.
    //
    //  This is what the per-run AssemblySession buys, asserted from the
    //  outside. Symbols, macros, the conditional stack, and the output image
    //  are all per-run state, and holding any of it on the long-lived
    //  Assembler would let the first assembly contaminate the second -- a
    //  symbol resolving because a previous file defined it is the kind of bug
    //  that only appears in a multi-file build.
    //
    //  It is asserted rather than assumed because the failure is invisible in
    //  single-shot use: every other test in this file assembles once.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InstanceReuseTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AssembleTwice_ResultsAreIndependent
        //
        //  Assembles two different programs through the same Assembler and
        //  asserts each result describes only its own source.
        //
        //  The two programs use DIFFERENT instructions, so leaked bytes from
        //  the first would be visible in the second's output rather than
        //  coincidentally matching.
        //
        //  Both results are checked, not just the second: the first must also
        //  remain valid after the second run, since results are returned by
        //  value and a shared buffer would corrupt the earlier one too.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AssembleTwice_ResultsAreIndependent)
        {
            Assembler asm6502 = BuildAssembler();

            auto result1 = asm6502.Assemble (
                R"(                 LDA #$42
                                    STA $10
                )");

            auto result2 = asm6502.Assemble (
                R"(                 LDX #$FF
                                    STX $20
                )");

            Assert::IsTrue (result1.success);
            Assert::IsTrue (result2.success);

            // First result: LDA #$42, STA $10
            Assert::AreEqual ((size_t) 4, result1.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result1.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result1.bytes[1]);
            Assert::AreEqual ((Byte) 0x85, result1.bytes[2]);
            Assert::AreEqual ((Byte) 0x10, result1.bytes[3]);

            // Second result: LDX #$FF, STX $20
            Assert::AreEqual ((size_t) 4, result2.bytes.size());
            Assert::AreEqual ((Byte) 0xA2, result2.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, result2.bytes[1]);
            Assert::AreEqual ((Byte) 0x86, result2.bytes[2]);
            Assert::AreEqual ((Byte) 0x20, result2.bytes[3]);

            // Symbol tables are independent (3 built-in symbols each)
            Assert::AreEqual ((size_t) 3, result1.symbols.size());
            Assert::AreEqual ((size_t) 3, result2.symbols.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AssembleTwice_LabelsDoNotLeak
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AssembleTwice_LabelsDoNotLeak)
        {
            Assembler asm6502 = BuildAssembler();

            auto result1 = asm6502.Assemble (
                R"(         start:  NOP
                            end:    BRK
                )");

            auto result2 = asm6502.Assemble (
                R"(         begin:  NOP
                            finish: BRK
                )");

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





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CaseSensitiveLabelTests
    //
    //  Labels are case-SENSITIVE, while mnemonics and directives are not.
    //
    //  That asymmetry is deliberate and is what these pin. Period sources write
    //  instructions in either case interchangeably, so the parser uppercases
    //  mnemonics -- but doing the same to labels would silently merge `foo` and
    //  `FOO` into one symbol, which is both a duplicate-definition error the
    //  author never made and a resolution to the wrong address.
    //
    //  Easy to break precisely because the uppercasing is already there for
    //  mnemonics: extending it one step too far looks like a simplification.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CaseSensitiveLabelTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  FooAndFOO_ResolveToDifferentAddresses
        //
        //  Two labels differing only in case must be two symbols at two
        //  addresses.
        //
        //  Asserting the ADDRESSES rather than merely that assembly succeeded
        //  is the point: a case-folding assembler would report a duplicate
        //  definition, but a subtler one might accept both and resolve every
        //  reference to whichever was defined last -- which succeeds and is
        //  wrong.
        //
        //  Warnings are suppressed so the unused-label diagnostics do not
        //  obscure what is being asserted.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (FooAndFOO_ResolveToDifferentAddresses)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            options.warningMode = WarningMode::NoWarn; // suppress unused label warnings

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            auto result = asm6502.Assemble (
                "foo: NOP\n"
                "FOO: NOP\n"
                "JMP foo\n"
                "JMP FOO\n"
            );

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x0000, result.symbols["foo"]);
            Assert::AreEqual ((Word) 0x0001, result.symbols["FOO"]);

            // JMP foo → bytes at offset 2: 0x4C, 0x00, 0x00
            Assert::AreEqual ((Byte) 0x4C, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[3]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[4]);

            // JMP FOO → bytes at offset 5: 0x4C, 0x01, 0x00
            Assert::AreEqual ((Byte) 0x4C, result.bytes[5]);
            Assert::AreEqual ((Byte) 0x01, result.bytes[6]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[7]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StressTests
    //
    //  Sources large enough to shake out anything that only works at small
    //  scale.
    //
    //  Everything else in this file assembles a handful of lines, where a
    //  quadratic symbol lookup, a fixed-size buffer, or an index that wraps at
    //  256 all behave perfectly. These exist to reach past those thresholds.
    //
    //  Sources are GENERATED rather than written out, so the scale is a number
    //  to raise rather than a file to edit.
    //
    //  Every label's address is verified, not just the final count -- a
    //  resolution bug at scale typically corrupts some symbols rather than
    //  failing outright.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StressTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  HundredLabels_AllResolveCorrectly
        //
        //  A hundred labels, each on a one-byte instruction, every address
        //  checked.
        //
        //  One NOP per label makes each expected address trivially predictable
        //  -- label N sits at the origin plus N -- so the assertion is exact
        //  rather than approximate, and an off-by-one anywhere in the sequence
        //  is caught at the label where it starts.
        //
        //  A hundred is past a few plausible internal limits while staying fast
        //  enough to run in every build.
        //
        //  Warnings are suppressed because every one of these labels is
        //  unused, and the diagnostics would swamp the result.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (HundredLabels_AllResolveCorrectly)
        {
            AssemblerOptions  options = {};
            TestCpu           cpu;
            std::string       source;
            AssemblyResult    result;
            options.warningMode = WarningMode::NoWarn; // lots of "unused" labels otherwise

            cpu.InitForTest();
            Assembler asm6502 (cpu.GetInstructionSet(), options);

            // Generate a program with 100 labels, each with a NOP

            for (int i = 0; i < 100; i++)
            {
                source += "label" + std::to_string (i) + ": NOP\n";
            }

            // Add cross-references: jump to every 10th label
            for (int i = 0; i < 100; i += 10)
            {
                source += "JMP label" + std::to_string (i) + "\n";
            }

            result = asm6502.Assemble (source);

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 103, result.symbols.size());  // 100 labels + 3 built-ins

            // Verify labels are at expected addresses
            for (int i = 0; i < 100; i++)
            {
                Word  expectedAddr = 0;

                std::string name = "label" + std::to_string (i);
                expectedAddr = 0x0000 + (Word) i;

                Assert::AreEqual (expectedAddr, result.symbols[name],
                    (std::wstring (L"Label: ") + std::wstring (name.begin(), name.end())).c_str());
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EmptySourceTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EmptySourceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EmptySource_ReturnsSuccessWithZeroBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EmptySource_ReturnsSuccessWithZeroBytes)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble ("");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((size_t) 0, result.bytes.size());
            Assert::AreEqual ((size_t) 0, result.errors.size());
            Assert::AreEqual ((size_t) 3, result.symbols.size());  // 3 built-in symbols
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Cpu65C02EncodingTests
    //
    //  The as65 `--cpu 65c02` tier: the CMOS-only opcodes the assembler must emit,
    //  and the strictness guarantee that they are rejected under the default 6502
    //  table. Focus is on the Rockwell bit instructions (RMBn/SMBn zero-page and
    //  BBRn/BBSn zero-page-relative) whose bit index rides in the mnemonic.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Cpu65C02EncodingTests)
    {
    public:

        //  RMBn / SMBn — zero-page read-modify-write; opcode is $x7, bit in x.

        TEST_METHOD (RMB_SMB_ZeroPage_EncodePerBit)
        {
            Assembler a = BuildAssembler65C02();

            auto rmb0 = a.Assemble ("RMB0 $30");
            Assert::IsTrue (rmb0.success);
            Assert::AreEqual ((size_t) 2, rmb0.bytes.size());
            Assert::AreEqual ((Byte) 0x07, rmb0.bytes[0]);
            Assert::AreEqual ((Byte) 0x30, rmb0.bytes[1]);

            auto rmb3 = a.Assemble ("RMB3 $30");
            Assert::IsTrue (rmb3.success);
            Assert::AreEqual ((Byte) 0x37, rmb3.bytes[0]);   // $07 + 3*$10

            auto smb7 = a.Assemble ("SMB7 $30");
            Assert::IsTrue (smb7.success);
            Assert::AreEqual ((size_t) 2, smb7.bytes.size());
            Assert::AreEqual ((Byte) 0xF7, smb7.bytes[0]);   // $87 + 7*$10
            Assert::AreEqual ((Byte) 0x30, smb7.bytes[1]);
        }


        //  BBRn / BBSn — zero-page, then a relative offset to the branch target.

        TEST_METHOD (BBR_BBS_ZeroPageRelative_ForwardAndBackward)
        {
            Assembler a = BuildAssembler65C02();

            // Forward branch: BBR0 at $0200 (3 bytes) → target $0205 is +2 past the
            // instruction end ($0203).
            auto fwd = a.Assemble (".org $0200\nBBR0 $30,$0205");
            Assert::IsTrue (fwd.success);
            Assert::AreEqual ((size_t) 3, fwd.bytes.size());
            Assert::AreEqual ((Byte) 0x0F, fwd.bytes[0]);    // BBR0
            Assert::AreEqual ((Byte) 0x30, fwd.bytes[1]);    // zero-page address
            Assert::AreEqual ((Byte) 0x02, fwd.bytes[2]);    // $0205 - $0203

            // Backward branch: BBS7 at $0210 → target $0205 is -14 (0xF2).
            auto back = a.Assemble (".org $0210\nBBS7 $30,$0205");
            Assert::IsTrue (back.success);
            Assert::AreEqual ((size_t) 3, back.bytes.size());
            Assert::AreEqual ((Byte) 0xFF, back.bytes[0]);   // BBS7 = $8F + 7*$10
            Assert::AreEqual ((Byte) 0x30, back.bytes[1]);
            Assert::AreEqual ((Byte) 0xF2, back.bytes[2]);   // $0205 - $0213 = -14
        }


        //  The branch target may be a symbol resolved on pass 2.

        TEST_METHOD (BBR_ZeroPageRelative_LabelTarget)
        {
            Assembler a = BuildAssembler65C02();

            // Two single-byte instructions separate the branch from its target so
            // `skip` lands at $0204; the offset byte (skip - $0203) is what proves
            // the second operand resolved against the pass-2 symbol table. (INX/DEX
            // rather than NOP: on the CMOS table "NOP" aliases a reserved single-
            // byte opcode, which would muddy an exact-byte filler assertion.)
            auto r = a.Assemble (".org $0200\nBBR3 $20,skip\nINX\nskip: DEX");
            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size());
            Assert::AreEqual ((Byte) 0x3F, r.bytes[0]);      // BBR3 = $0F + 3*$10
            Assert::AreEqual ((Byte) 0x20, r.bytes[1]);      // zero-page address
            Assert::AreEqual ((Byte) 0x01, r.bytes[2]);      // skip ($0204) - $0203
            Assert::AreEqual ((Byte) 0xE8, r.bytes[3]);      // INX
            Assert::AreEqual ((Byte) 0xCA, r.bytes[4]);      // skip: DEX
        }


        //  An out-of-range bit-branch target is a hard error.

        TEST_METHOD (BBR_ZeroPageRelative_OutOfRangeIsError)
        {
            Assembler a = BuildAssembler65C02();

            auto r = a.Assemble (".org $0200\nBBR0 $30,$0400");
            Assert::IsFalse (r.success);
        }


        //  NOP must assemble to the canonical $EA, not one of the 65C02's reserved
        //  NOP-fill slots. Those slots execute/disassemble as NOPs but are hidden
        //  from the opcode table, so they can't shadow $EA.

        TEST_METHOD (NOP_EncodesToCanonicalEA)
        {
            Assembler a = BuildAssembler65C02();

            auto r = a.Assemble ("NOP");
            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }


        //  A representative non-bit CMOS opcode also assembles on the 65C02 table.

        TEST_METHOD (STZ_ZeroPage_Encodes)
        {
            Assembler a = BuildAssembler65C02();

            auto r = a.Assemble ("STZ $30");
            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0x64, r.bytes[0]);      // STZ zp
            Assert::AreEqual ((Byte) 0x30, r.bytes[1]);
        }


        //  Strictness: CMOS-only mnemonics must NOT assemble under the 6502 table,
        //  so `casso as65` (default 6502) never silently accepts 65C02 code.

        TEST_METHOD (Cmos_Opcodes_RejectedOn6502)
        {
            Assembler a = BuildAssembler();

            Assert::IsFalse (a.Assemble (".org $0200\nBBR0 $30,$0205").success, L"BBR0 must fail on 6502");
            Assert::IsFalse (a.Assemble ("RMB0 $30").success,               L"RMB0 must fail on 6502");
            Assert::IsFalse (a.Assemble ("SMB7 $30").success,               L"SMB7 must fail on 6502");
            Assert::IsFalse (a.Assemble ("STZ $30").success,                L"STZ must fail on 6502");
            Assert::IsFalse (a.Assemble ("RMB 0,$30").success,              L"RMB bit,zp must fail on 6502");
            Assert::IsFalse (a.Assemble (".org $0200\nBBR 0,$30,$0205").success, L"BBR bit,zp,tgt must fail on 6502");
        }


        //  as65 operand form: RMB/SMB take `<bit>,<zp>` with a bare mnemonic.

        TEST_METHOD (RMB_SMB_OperandForm_EncodePerBit)
        {
            Assembler a = BuildAssembler65C02();

            auto rmb3 = a.Assemble ("RMB 3,$30");
            Assert::IsTrue (rmb3.success);
            Assert::AreEqual ((size_t) 2, rmb3.bytes.size());
            Assert::AreEqual ((Byte) 0x37, rmb3.bytes[0]);   // same opcode as RMB3 $30
            Assert::AreEqual ((Byte) 0x30, rmb3.bytes[1]);

            auto smb7 = a.Assemble ("SMB 7,$30");
            Assert::IsTrue (smb7.success);
            Assert::AreEqual ((Byte) 0xF7, smb7.bytes[0]);

            // A constant expression for the bit is fine (as65 allows it).
            auto rmbExpr = a.Assemble ("RMB (1<<2),$30");
            Assert::IsTrue (rmbExpr.success);
            Assert::AreEqual ((Byte) 0x47, rmbExpr.bytes[0]);   // bit 4 -> $07 + 4*$10
        }


        //  as65 operand form: BBR/BBS take `<bit>,<zp>,<target>`.

        TEST_METHOD (BBR_BBS_OperandForm_WithLabel)
        {
            Assembler a = BuildAssembler65C02();

            auto fwd = a.Assemble (".org $0200\nBBR 0,$30,$0205");
            Assert::IsTrue (fwd.success);
            Assert::AreEqual ((size_t) 3, fwd.bytes.size());
            Assert::AreEqual ((Byte) 0x0F, fwd.bytes[0]);    // BBR0
            Assert::AreEqual ((Byte) 0x30, fwd.bytes[1]);
            Assert::AreEqual ((Byte) 0x02, fwd.bytes[2]);    // $0205 - $0203

            auto lbl = a.Assemble (".org $0200\nBBS 3,$20,skip\nINX\nskip: DEX");
            Assert::IsTrue (lbl.success);
            Assert::AreEqual ((size_t) 5, lbl.bytes.size());
            Assert::AreEqual ((Byte) 0xBF, lbl.bytes[0]);    // BBS3 = $8F + 3*$10
            Assert::AreEqual ((Byte) 0x20, lbl.bytes[1]);
            Assert::AreEqual ((Byte) 0x01, lbl.bytes[2]);    // skip ($0204) - $0203
        }


        //  An out-of-range bit index is a hard error under either form.

        TEST_METHOD (BitOp_BadBitNumberIsError)
        {
            Assembler a = BuildAssembler65C02();

            Assert::IsFalse (a.Assemble ("RMB 8,$30").success, L"bit 8 is out of range");
            Assert::IsFalse (a.Assemble ("SMB -1,$30").success, L"negative bit is out of range");
        }


        //  RMB stays a reserve-storage directive in its `<count>` form: as65 uses
        //  `rmb <bit>,<zp>` for the instruction and `ds`/`rmb <count>` for storage,
        //  so the comma is what selects the instruction.

        TEST_METHOD (RMB_CountForm_StillReservesStorage)
        {
            Assembler a = BuildAssembler65C02();

            auto r = a.Assemble ("RMB 5");
            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size());   // reserved 5 bytes, not an opcode
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IndirectErrorRecoverySizingTests
    //
    //  EstimateErrorRecoverySize runs on exactly one path: after RecordError,
    //  when ResolveAddressingMode named a mode the opcode table does not carry.
    //  The assembly has already failed, so no output depends on the width -- it
    //  only decides where the labels on the following lines land, and therefore
    //  whether the rest of the diagnostics stay useful or cascade into noise.
    //
    //  Nothing else calls it. The success path is covered by the ordinary
    //  encoding tests, not by these.
    //
    //  What these pin is that the recovery width is the width the *mnemonic*
    //  actually has: a jump is 3 bytes in every form it possesses, everything
    //  else parenthesized is 2. The tempting alternative -- size whatever mode
    //  the resolver returned -- is wrong, because when nothing matches it
    //  returns a default the mnemonic does not have, which is how a JMP ends up
    //  sized at 2 despite having no 2-byte encoding on any 6502.
    //
    //  A failed assembly still publishes its symbol table, which is what makes
    //  the advance observable at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IndirectErrorRecoverySizingTests)
    {
    public:

        //  `JMP (abs,X)` is 65C02-only, so this fails on NMOS -- but JMP is a
        //  3-byte instruction there too (abs and (abs) both), so 3 is the right
        //  skip. Sizing the resolver's ZeroPageXIndirect fallback would say 2.

        TEST_METHOD (JmpIndirectX_UnencodableOnNmos_AdvancesJumpWidth)
        {
            Assembler a = BuildAssembler();

            AssemblyResult  r = a.Assemble (".org $0800\nJMP (target,X)\nhere: .word here\n");

            Assert::IsFalse (r.success, L"JMP (abs,X) is a 65C02-only mode");
            Assert::AreEqual ((Word) 0x0803, r.symbols["here"],
                L"JMP has no 2-byte form on any 6502, so recovery skips 3");
        }


        //  The mirror case: LDA has no plain `(...)` form on NMOS, and every
        //  indirect LDA that does exist -- (zp,X), (zp),Y -- is 2 bytes.

        TEST_METHOD (LdaIndirect_UnencodableOnNmos_AdvancesTwoBytes)
        {
            Assembler a = BuildAssembler();

            AssemblyResult  r = a.Assemble (".org $0800\nLDA (target)\nhere: .word here\n");

            Assert::IsFalse (r.success, L"LDA (zp) is a 65C02-only mode");
            Assert::AreEqual ((Word) 0x0802, r.symbols["here"],
                L"every indirect LDA is 2 bytes, so recovery skips 2");
        }


        //  Unchanged by instruction set: a forward `(zp)` operand cannot be
        //  proven zero-page-sized on pass 1, so this fails on the CMOS table too,
        //  and LDA is still 2 bytes wide.

        TEST_METHOD (LdaIndirect_ForwardRefOn65C02_AdvancesTwoBytes)
        {
            Assembler a = BuildAssembler65C02();

            AssemblyResult  r = a.Assemble (".org $0800\nLDA (target)\nhere: .word here\n");

            Assert::IsFalse (r.success, L"a forward (zp) operand is not zp-provable on pass 1");
            Assert::AreEqual ((Word) 0x0802, r.symbols["here"], L"LDA is 2 bytes here as well");
        }


        //  JSR is the case the old `mnemonic == "JMP"` compare got wrong: it is
        //  every bit as much a 3-byte instruction, but the compare sized it at 2.
        //  Asking for JumpAbsolute covers both mnemonics that carry it.

        TEST_METHOD (JsrIndirect_Unencodable_AdvancesJumpWidth)
        {
            Assembler a = BuildAssembler();

            AssemblyResult  r = a.Assemble (".org $0800\nJSR (target)\nhere: .word here\n");

            Assert::IsFalse (r.success, L"JSR has no indirect form");
            Assert::AreEqual ((Word) 0x0803, r.symbols["here"],
                L"JSR is 3 bytes, so recovery skips 3 -- the old JMP-only compare said 2");
        }
    };
}
