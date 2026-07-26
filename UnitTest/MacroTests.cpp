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
