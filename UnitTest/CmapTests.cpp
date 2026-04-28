#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CmapTests
{
    TEST_CLASS (CmapDirectiveTests)
    {
    public:

        TEST_METHOD (CmapResetToIdentity)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    cmap 'A'=$C1\n"
                "    cmap 0\n"
                "    .byte \"A\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x41, result.bytes[0], L"Should be identity after reset");
        }





        TEST_METHOD (CmapSingleCharMapping)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    cmap 'A'=$C1\n"
                "    .byte \"A\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0xC1, result.bytes[0], L"Should be mapped to $C1");
        }





        TEST_METHOD (CmapRangeMapping)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    cmap 'A'-'Z'=$C1\n"
                "    .byte \"ABC\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0xC1, result.bytes[0], L"A -> $C1");
            Assert::AreEqual ((Byte) 0xC2, result.bytes[1], L"B -> $C2");
            Assert::AreEqual ((Byte) 0xC3, result.bytes[2], L"C -> $C3");
        }





        TEST_METHOD (CmapAppliesToText)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    cmap 'H'=$01\n"
                "    .text \"Hi\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x01, result.bytes[0], L"H should be mapped");
            Assert::AreEqual ((Byte) 'i',  result.bytes[1], L"i should be unmapped");
        }
    };





    TEST_CLASS (InstructionSynonymTests)
    {
    public:

        TEST_METHOD (DisableEmitsSEI)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    disable\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x78, result.bytes[0], L"Should emit SEI opcode");
        }





        TEST_METHOD (EnableEmitsCLI)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    enable\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x58, result.bytes[0], L"Should emit CLI opcode");
        }





        TEST_METHOD (StcEmitsSEC)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    stc\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x38, result.bytes[0], L"Should emit SEC opcode");
        }
    };





    TEST_CLASS (MultiNopTests)
    {
    public:

        TEST_METHOD (NopWithCount)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    nop 5\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 5, result.bytes.size (), L"Should emit 5 bytes");

            for (size_t i = 0; i < 5; i++)
            {
                Assert::AreEqual ((Byte) 0xEA, result.bytes[i], L"Should be NOP");
            }
        }





        TEST_METHOD (NopWithoutCount)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    nop\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.bytes.size (), L"Should emit 1 byte");
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0], L"Should be NOP");
        }
    };





    TEST_CLASS (SetKeywordTests)
    {
    public:

        TEST_METHOD (SetDefinesReassignableConstant)
        {
            TestCpu cpu;

            // Set constants use the value at the time of use (temporal ordering)
            auto result = cpu.Assemble (
                "    .org $1000\n"
                "val set $10\n"
                "    lda #val\n"
                "val set $20\n"
                "    ldx #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // lda #val uses $10 (first set), ldx #val uses $20 (second set)
            Assert::AreEqual ((Byte) 0x10, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x20, result.bytes[3]);
        }
    };
}
