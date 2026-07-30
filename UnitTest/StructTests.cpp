#include "Pch.h"

#include "Assembler.h"
#include "TestCpu65C02.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace StructTests
{
    TEST_CLASS (StructDefinitionTests)
    {
    public:

        TEST_METHOD (StructDefinesMemberOffsets)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct PlayerState\n"
                "xpos ds 1\n"
                "ypos ds 1\n"
                "health ds 2\n"
                "    end struct\n"
                "    lda #PlayerState.xpos\n"
                "    ldx #PlayerState.ypos\n"
                "    ldy #PlayerState.health\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #0 (xpos offset), LDX #1 (ypos), LDY #2 (health)
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xA2, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x01, result.bytes[3]);
            Assert::AreEqual ((Byte) 0xA0, result.bytes[4]);
            Assert::AreEqual ((Byte) 0x02, result.bytes[5]);
        }





        TEST_METHOD (StructSizeAsSymbol)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Obj\n"
                "x ds 1\n"
                "y ds 1\n"
                "flags ds 1\n"
                "    end struct\n"
                "    lda #Obj\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #3 (struct size)
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x03, result.bytes[1]);
        }





        TEST_METHOD (StructWithStartOffset)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Regs, 4\n"
                "regA ds 1\n"
                "regB ds 1\n"
                "    end struct\n"
                "    lda #Regs.regA\n"
                "    ldx #Regs.regB\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #4 (offset 4), LDX #5 (offset 5)
            Assert::AreEqual ((Byte) 0x04, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x05, result.bytes[3]);
        }





        TEST_METHOD (StructWithDbDwMembers)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Pkt\n"
                "id db\n"
                "len dw\n"
                "    end struct\n"
                "    lda #Pkt.id\n"
                "    ldx #Pkt.len\n"
                "    ldy #Pkt\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);  // id = offset 0
            Assert::AreEqual ((Byte) 0x01, result.bytes[3]);  // len = offset 1
            Assert::AreEqual ((Byte) 0x03, result.bytes[5]);  // size = 3
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StructMemberSpellingTests
    //
    //  Member declarations accept every as65 storage spelling, but only ds / db
    //  / dw were covered, so a synonym could stop working unnoticed -- the same
    //  gap that let `rmb <count>` reserve zero bytes for a while.
    //
    //  These sweep the whole set. The widths now come from a token-keyed table
    //  while the spellings come from DirectiveTable, so the risk this guards is
    //  a synonym resolving to the wrong token or to none at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StructMemberSpellingTests)
    {
    public:

        //  Assembles a one-member struct and returns the size the assembler
        //  published for it under the struct's own name.
        static int MeasureMember (const char * declaration)
        {
            TestCpu  cpu;

            std::string  source = std::string ("    .org $1000\n    struct S\n")
                                + "m " + declaration + "\n"
                                + "    end struct\n    lda #S\n";

            AssemblyResult  result = cpu.Assemble (source.c_str());

            return result.success ? (int) result.bytes[1] : -1;
        }


        TEST_METHOD (ReserveSpellings_AllTakeCountFromOperand)
        {
            // ds / dsb / rmb all mean "reserve <count> bytes".
            Assert::AreEqual (4, MeasureMember ("ds 4"),  L"ds <count>");
            Assert::AreEqual (4, MeasureMember ("dsb 4"), L"dsb <count>");
            Assert::AreEqual (4, MeasureMember ("rmb 4"), L"rmb <count> is the .DS synonym");
        }


        TEST_METHOD (ByteSpellings_AreOneByteEach)
        {
            Assert::AreEqual (1, MeasureMember ("db"),   L"db");
            Assert::AreEqual (1, MeasureMember ("byt"),  L"byt");
            Assert::AreEqual (1, MeasureMember ("byte"), L"byte");
            Assert::AreEqual (1, MeasureMember ("fcb"),  L"fcb");
        }


        TEST_METHOD (WordSpellings_AreTwoBytesEach)
        {
            Assert::AreEqual (2, MeasureMember ("dw"),   L"dw");
            Assert::AreEqual (2, MeasureMember ("word"), L"word");
            Assert::AreEqual (2, MeasureMember ("fcw"),  L"fcw");
            Assert::AreEqual (2, MeasureMember ("fdb"),  L"fdb");
        }


        TEST_METHOD (DoubleWordSpelling_IsFourBytes)
        {
            Assert::AreEqual (4, MeasureMember ("dd"), L"dd");
        }


        //  Spellings are case-insensitive, as everywhere else in the dialect.
        TEST_METHOD (Spellings_AreCaseInsensitive)
        {
            Assert::AreEqual (2, MeasureMember ("DW"), L"upper case");
            Assert::AreEqual (2, MeasureMember ("Dw"), L"mixed case");
        }


        //  A member whose type word is not a storage directive contributes
        //  nothing, leaving the struct empty rather than guessing a width.
        TEST_METHOD (UnknownSpelling_ReservesNothing)
        {
            Assert::AreEqual (0, MeasureMember ("wat 4"), L"unknown type word");
        }


        //  The dual-purpose spelling must NOT leak the other way: outside a
        //  struct, `rmb <bit>,<zp>` is still the Rockwell instruction, and
        //  routing struct members through FromStorageSpelling must not change
        //  that. Two bytes of opcode plus zero page, not reserved storage.
        TEST_METHOD (RmbBitForm_OutsideStruct_StaysAnInstruction)
        {
            TestCpu65C02  cpu;
            cpu.InitForTest();

            Assembler       asm65C02 (cpu.GetInstructionSet());
            AssemblyResult  result = asm65C02.Assemble ("    .org $1000\n    rmb 3,$20\n");

            Assert::IsTrue (result.success, L"rmb <bit>,<zp> is a 65C02 instruction");
            Assert::AreEqual ((size_t) 2, result.bytes.size(), L"opcode + zero page operand");
            Assert::AreEqual ((Byte) 0x37, result.bytes[0], L"RMB3 = $07 + 3*$10");
            Assert::AreEqual ((Byte) 0x20, result.bytes[1]);
        }
    };
}
