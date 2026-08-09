#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ConstantTests
{
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





    TEST_CLASS (ConstantTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EquChain_ForwardReference_Resolves
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EquChain_ForwardReference_Resolves)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "a equ b\n"
                "b equ 42\n"
                "    LDA #a\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 42,   result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EquChain_ThreeLevel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EquChain_ThreeLevel)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "a equ b\n"
                "b equ c\n"
                "c equ 7\n"
                "    LDA #a\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 7,    result.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EquChain_Circular_ReportsError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EquChain_Circular_ReportsError)
        {
            Assembler asm6502 = BuildAssembler();
            auto result = asm6502.Assemble (
                "a equ b\n"
                "b equ a\n"
                "    LDA #a\n"
            );

            Assert::IsFalse (result.success, L"Assembly should fail on circular equ");
            Assert::IsTrue (result.errors.size() > 0, L"Should have errors");
        }
    };
}
