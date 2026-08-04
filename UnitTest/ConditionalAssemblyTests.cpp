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


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  StrayEndif_FailsAndKeepsGoing
        //
        //  An ENDIF with nothing open is a hard error, not a warning: the
        //  nesting is unbalanced, so no output can be trusted. It must also not
        //  pop -- popping an empty stack is undefined -- and assembly has to
        //  continue so the run reports everything rather than the first fault.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (StrayEndif_FailsAndKeepsGoing)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    LDA #$01\n"
                "    endif\n"
                "    LDA #$02\n"
            );

            Assert::IsFalse (result.success, L"an unmatched endif must fail the assembly");
            Assert::IsTrue  (result.errors.size() >= 1, L"and must say so");
            Assert::IsTrue  (result.errors[0].message.find ("endif without matching if") != std::string::npos,
                             L"the message names the actual problem");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"reported on the offending line");
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnclosedIf_ReportedAtEndOfPass
        //
        //  The mirror of the above, and the harder one: an IF that is never
        //  closed leaves nothing behind to notice it -- the source just ends --
        //  so only the leftover stack at end of pass 1 can catch it. It is
        //  still reported at the IF, not at the end of the file.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnclosedIf_ReportedAtTheOpeningLine)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    LDA #$01\n"
                "    LDA #$02\n"
                "    if 1\n"
                "    LDA #$03\n"
            );

            Assert::IsFalse (result.success, L"an unclosed if must fail the assembly");
            Assert::IsTrue  (result.errors.size() >= 1, L"and must say so");
            Assert::IsTrue  (result.errors[0].message.find ("Unclosed if block") != std::string::npos,
                             L"the message names the actual problem");
            Assert::AreEqual (3, result.errors[0].lineNumber,
                              L"reported at the if, not at the end of the file");
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnclosedNestedIfs_ReportOnePerOpenLevel
        //
        //  Every open level is separately missing an ENDIF, so every one is
        //  separately somewhere to go -- one error each, at its own opening
        //  line, innermost first (the order they need closing in). A single
        //  "3 level(s) open" summary blamed the end of the file and named no
        //  line worth visiting.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnclosedNestedIfs_ReportOnePerOpenLevel)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "    if 1\n"
                "    if 1\n"
                "    if 1\n"
                "    LDA #$01\n"
            );

            Assert::IsFalse  (result.success, L"three unclosed ifs must fail the assembly");
            Assert::AreEqual ((size_t) 3, result.errors.size(), L"one error per unclosed level");
            Assert::AreEqual (3, result.errors[0].lineNumber, L"innermost first");
            Assert::AreEqual (2, result.errors[1].lineNumber);
            Assert::AreEqual (1, result.errors[2].lineNumber, L"outermost last");
        }
    };
}
