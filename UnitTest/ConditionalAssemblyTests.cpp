#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ConditionalAssemblyTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler (AssemblerOptions opts = {})
    {
        TestCpu cpu;
        cpu.InitForTest();
        return Assembler (cpu.GetInstructionSet(), opts);
    }





    TEST_CLASS (ConditionalAssemblyTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ifdef_DefinedSymbol_Assembles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ifdef_DefinedSymbol_Assembles)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "FOO = 1\n"
                "    ifdef FOO\n"
                "    LDA #$42\n"
                "    endif\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size(), L"Should emit LDA #$42");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ifdef_UndefinedSymbol_Skips
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ifdef_UndefinedSymbol_Skips)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    ifdef MISSING\n"
                "    LDA #$42\n"
                "    endif\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 0, result.bytes.size(), L"Should emit nothing");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ifndef_UndefinedSymbol_Assembles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ifndef_UndefinedSymbol_Assembles)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    ifndef MISSING\n"
                "    LDA #$42\n"
                "    endif\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size(), L"Should emit LDA #$42");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ifndef_DefinedSymbol_Skips
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ifndef_DefinedSymbol_Skips)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "FOO = 1\n"
                "    ifndef FOO\n"
                "    LDA #$42\n"
                "    endif\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 0, result.bytes.size(), L"Should emit nothing");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ifdef_WithElse
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ifdef_WithElse)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    ifdef MISSING\n"
                "    LDA #$01\n"
                "    else\n"
                "    LDA #$02\n"
                "    endif\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size(), L"Should emit else branch");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x02, result.bytes[1]);
        }
    };
}
