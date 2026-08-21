#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MacroTests
{
    TEST_CLASS (LocalLabelTests)
    {
    public:

        TEST_METHOD (LocalLabelsGetUniqueSuffix)
        {
            TestCpu cpu;

            // Two invocations of a macro with local labels should not collide
            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    local loop\n"
                "loop: nop\n"
                "    jmp loop\n"
                "    endm\n"
                "    test\n"
                "    test\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::IsTrue (result.bytes.size() > 0, L"Should emit bytes");
        }





        TEST_METHOD (LocalLabelsMultipleNames)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    local lbl1, lbl2\n"
                "lbl1: nop\n"
                "lbl2: nop\n"
                "    jmp lbl1\n"
                "    endm\n"
                "    test\n"
                "    test\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed with multi-local");
        }
    };





    TEST_CLASS (ExitmTests)
    {
    public:

        TEST_METHOD (ExitmStopsExpansion)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    nop\n"
                "    exitm\n"
                "    brk\n"
                "    endm\n"
                "    test\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // Should have only NOP ($EA), not BRK ($00)
            Assert::AreEqual ((size_t) 1, result.bytes.size(), L"Should emit only 1 byte (NOP)");
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0], L"Should be NOP");
        }


        //  Leaving a macro early from inside a conditional has to close the
        //  blocks it abandons, or the IF stays open past the expansion and the
        //  file ends with "Unclosed if block".
        //
        //  ExitmStopsExpansion above never reaches that code: with no IF open,
        //  the depth count is zero and the loop that does the work does not
        //  run. So this is the only cover for it -- and it is the loop where
        //  the accepted spellings used to be written out by hand, a third copy
        //  of the vocabulary DirectiveTable owns.

        TEST_METHOD (ExitmInsideIf_SynthesizesClosingEndif)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    if 1\n"
                "    nop\n"
                "    exitm\n"
                "    endif\n"
                "    brk\n"
                "    endm\n"
                "    test\n"
                "    lda #$42\n"
            );

            Assert::IsTrue (result.success, L"the synthesized ENDIF must balance the abandoned IF");
            Assert::AreEqual ((size_t) 3, result.bytes.size(), L"NOP, then the LDA after the macro");
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0], L"NOP from inside the conditional");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1], L"LDA immediate follows the expansion");
            Assert::AreEqual ((Byte) 0x42, result.bytes[2]);
        }


        //  One ENDIF per abandoned level, not just one overall.

        TEST_METHOD (ExitmInsideNestedIfs_ClosesEveryOpenLevel)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    if 1\n"
                "    if 1\n"
                "    nop\n"
                "    exitm\n"
                "    endif\n"
                "    endif\n"
                "    brk\n"
                "    endm\n"
                "    test\n"
                "    lda #$42\n"
            );

            Assert::IsTrue (result.success, L"both abandoned IF levels must be closed");
            Assert::AreEqual ((size_t) 3, result.bytes.size(), L"NOP, then the LDA after the macro");
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xA9, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[2]);
        }

        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnclosedMacro_ReportedAtTheOpeningLine
        //
        //  The third of ValidateAssemblyCompletion's end-of-pass checks, and
        //  the one with the best diagnostic: a MACRO with no ENDM is reported
        //  at the line the definition OPENED on, not at EOF where the pass
        //  finally noticed. That is where the fix goes, and it is what the
        //  unclosed-IF error cannot do -- ConditionalState keeps no opening
        //  line, so that one can only report a depth.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnclosedMacro_ReportedAtTheOpeningLine)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    nop\n"
                "test macro\n"
                "    nop\n"
            );

            Assert::IsFalse (result.success, L"a macro with no endm must fail the assembly");
            Assert::IsTrue  (result.errors.size() >= 1, L"and must say so");
            Assert::IsTrue  (result.errors[0].message.find ("Unclosed macro definition") != std::string::npos,
                             L"the message names the actual problem");
            // Stored as the parser normalized it -- mnemonics are uppercased --
            // so the message carries TEST, not the source's `test`.
            Assert::IsTrue  (result.errors[0].message.find ("TEST") != std::string::npos,
                             L"and names WHICH macro");
            Assert::AreEqual (3, result.errors[0].lineNumber,
                              L"reported where the definition opened, not at EOF");
        }
    };





    TEST_CLASS (ArgCountTests)
    {
    public:

        TEST_METHOD (BackslashZeroReplacedWithArgCount)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    .byte \\0\n"
                "    endm\n"
                "    test a, b, c\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.bytes.size(), L"Should emit 1 byte");
            Assert::AreEqual ((Byte) 3, result.bytes[0], L"Arg count should be 3");
        }





        TEST_METHOD (BackslashZeroWithNoArgs)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    .byte \\0\n"
                "    endm\n"
                "    test\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.bytes.size(), L"Should emit 1 byte");
            Assert::AreEqual ((Byte) 0, result.bytes[0], L"Arg count should be 0");
        }
    };





    TEST_CLASS (NamedParamTests)
    {
    public:

        TEST_METHOD (NamedParamsSubstituted)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "load macro addr, val\n"
                "    lda #val\n"
                "    sta addr\n"
                "    endm\n"
                "    load $80, $42\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #$42 = A9 42, STA $80 = 85 80
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x85, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x80, result.bytes[3]);
        }





        TEST_METHOD (NamedParamsWholeWordOnly)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro a\n"
                "    .byte a\n"
                "    endm\n"
                "    test $55\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.bytes.size());
            Assert::AreEqual ((Byte) 0x55, result.bytes[0]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OverlappingDefinitionTests
    //
    //  A definition opened while another is still collecting does not become
    //  body text of it -- it opens beside it, both receive every following line,
    //  and one terminator closes both.
    //
    //  These are written in as65's spelling deliberately. The behavior came from
    //  Merlin, whose own macro library depends on it, but it is the COLLECTOR's
    //  and no profile is consulted about it: nothing here selects a dialect, and
    //  the Merlin half in `MerlinDirectiveTests` asserts the same shape through
    //  `MAC` / `<<<`. A test in only one of the two dialects would leave a
    //  dialect-shaped special case indistinguishable from the mechanism.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OverlappingDefinitionTests)
    {
    public:

        TEST_METHOD (TwoDefinitionsShareOneTerminator)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "outer macro\n"
                "    txa\n"
                "inner macro\n"
                "    clc\n"
                "    endm\n"
                "    outer\n"
                "    inner\n"
            );

            Assert::IsTrue (result.success, L"one terminator must close both definitions");
            Assert::AreEqual ((size_t) 3, result.bytes.size(), L"TXA CLC from outer, then CLC from inner");
            Assert::AreEqual ((Byte) 0x8A, result.bytes[0], L"outer starts at its own first line");
            Assert::AreEqual ((Byte) 0x18, result.bytes[1], L"and runs on into inner's body");
            Assert::AreEqual ((Byte) 0x18, result.bytes[2], L"inner exists in its own right");
        }



        //  Each definition declares its own parameters, and the tail they share
        //  binds to whichever one is being expanded. A single shared parameter
        //  list would bind the second call's argument in both.
        TEST_METHOD (EachDefinitionBindsTheSharedTailToItsOwnArgument)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "outer macro val\n"
                "    txa\n"
                "inner macro val\n"
                "    .byte val\n"
                "    endm\n"
                "    outer $11\n"
                "    inner $22\n"
            );

            Assert::IsTrue (result.success, L"both definitions must expand");
            Assert::AreEqual ((size_t) 3, result.bytes.size(), L"TXA and a byte from outer, one byte from inner");
            Assert::AreEqual ((Byte) 0x8A, result.bytes[0], L"outer starts at its own first line");
            Assert::AreEqual ((Byte) 0x11, result.bytes[1], L"and binds the shared tail to its own argument");
            Assert::AreEqual ((Byte) 0x22, result.bytes[2], L"inner binds the same line to a different one");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LineContinuationTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LineContinuationTests)
    {
    public:

        TEST_METHOD (LineContinuation_InMacro)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "test macro\n"
                "    lda #\\\n"
                "    $42\n"
                "    endm\n"
                "    test\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size(), L"LDA imm = 2 bytes");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0], L"LDA opcode");
            Assert::AreEqual ((Byte) 0x42, result.bytes[1], L"Operand $42");
        }
    };
}
