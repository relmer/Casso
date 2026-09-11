#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Disassembler.h"
#include "Cpu.h"
#include "Microcode.h"
#include "Core/Cpu65C02Table.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblerTests
//
//  Every addressing mode on both tables, one opcode each, with the operand
//  text stated by hand; the table under test is the same one the emulator
//  executes from, so a mnemonic here is checked against the core, not
//  against a second copy of the opcode map.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DisassemblerTests)
{
public:

    static constexpr Word  kOrigin = 0x0300;



    struct Expected
    {
        const char *  mnemonic;
        const char *  operand;
        size_t        length;
    };



    static void AssertLines (const std::vector<DisassembledLine> & lines,
                             const Expected                      * expected,
                             size_t                                count)
    {
        size_t  i = 0;

        Assert::AreEqual (count, lines.size(), L"one line per instruction");

        for (i = 0; i < count; i++)
        {
            Assert::AreEqual (std::string (expected[i].mnemonic), lines[i].mnemonic);
            Assert::AreEqual (std::string (expected[i].operand),  lines[i].operand);
            Assert::AreEqual (expected[i].length, lines[i].bytes.size());
            Assert::IsTrue   (lines[i].isDefined);
        }
    }



    TEST_METHOD (Nmos_EveryAddressingMode)
    {
        Cpu                            cpu;
        std::vector<Byte>              bytes =
        {
            0xA9, 0x01,          // LDA #$01
            0xA5, 0x10,          // LDA $10
            0xB5, 0x10,          // LDA $10,X
            0xB6, 0x10,          // LDX $10,Y
            0xAD, 0x34, 0x12,    // LDA $1234
            0xBD, 0x34, 0x12,    // LDA $1234,X
            0xB9, 0x34, 0x12,    // LDA $1234,Y
            0xA1, 0x10,          // LDA ($10,X)
            0xB1, 0x10,          // LDA ($10),Y
            0x0A,                // ASL A
            0x4C, 0x34, 0x12,    // JMP $1234
            0x6C, 0x34, 0x12,    // JMP ($1234)
            0xD0, 0x02,          // BNE +2 -> $0320
            0xEA,                // NOP
        };
        const Expected                 expected[] =
        {
            { "LDA", "#$01",     2 },
            { "LDA", "$10",      2 },
            { "LDA", "$10,X",    2 },
            { "LDX", "$10,Y",    2 },
            { "LDA", "$1234",    3 },
            { "LDA", "$1234,X",  3 },
            { "LDA", "$1234,Y",  3 },
            { "LDA", "($10,X)",  2 },
            { "LDA", "($10),Y",  2 },
            { "ASL", "A",        1 },
            { "JMP", "$1234",    3 },
            { "JMP", "($1234)",  3 },
            { "BNE", "$0320",    2 },
            { "NOP", "",         1 },
        };
        std::vector<DisassembledLine>  lines;

        Disassembler::Disassemble (bytes, kOrigin, cpu.GetInstructionSet(), lines);

        AssertLines (lines, expected, std::size (expected));

        Assert::AreEqual ((int) kOrigin,      (int) lines[0].address);
        Assert::AreEqual ((int) kOrigin + 28, (int) lines[12].address);
    }



    TEST_METHOD (Cmos_TheAddedAddressingModes)
    {
        std::vector<Byte>              bytes =
        {
            0xB2, 0x10,          // LDA ($10)
            0x7C, 0x34, 0x12,    // JMP ($1234,X)
            0x0F, 0x10, 0x03,    // BBR0 $10,+3 -> $030B
            0x64, 0x10,          // STZ $10
            0x80, 0x05,          // BRA +5 -> $0311
            0x1A,                // INC A
        };
        std::vector<DisassembledLine>  lines;

        Disassembler::Disassemble (bytes, kOrigin, GetCpu65C02InstructionSet(), lines);

        Assert::AreEqual ((size_t) 6, lines.size());

        Assert::AreEqual (std::string ("LDA"),        lines[0].mnemonic);
        Assert::AreEqual (std::string ("($10)"),      lines[0].operand);
        Assert::AreEqual (std::string ("JMP"),        lines[1].mnemonic);
        Assert::AreEqual (std::string ("($1234,X)"),  lines[1].operand);
        Assert::IsTrue   (lines[2].mnemonic.rfind ("BBR", 0) == 0, L"the bit branch keeps its mnemonic");
        Assert::AreEqual (std::string ("$10,$030B"),  lines[2].operand);
        Assert::AreEqual (std::string ("STZ"),        lines[3].mnemonic);
        Assert::AreEqual (std::string ("$10"),        lines[3].operand);
        Assert::AreEqual (std::string ("BRA"),        lines[4].mnemonic);
        Assert::AreEqual (std::string ("$0311"),      lines[4].operand);
        Assert::AreEqual (std::string ("INC"),        lines[5].mnemonic);
        Assert::AreEqual (std::string ("A"),          lines[5].operand);
    }



    TEST_METHOD (Cmos_DefinesEveryOpcode)
    {
        const Microcode  * table  = GetCpu65C02InstructionSet();
        int                opcode = 0;

        for (opcode = 0; opcode < 256; opcode++)
        {
            std::vector<Byte>              bytes = { (Byte) opcode, 0x00, 0x00 };
            std::vector<DisassembledLine>  lines;

            Disassembler::Disassemble (bytes, kOrigin, table, lines);

            Assert::IsTrue (!lines.empty() && lines[0].isDefined,
                            L"the 65C02 assigns every opcode an instruction");
        }
    }



    TEST_METHOD (Nmos_AnUndefinedOpcodeRendersAsOneByte)
    {
        Cpu                            cpu;
        const Microcode              * table     = cpu.GetInstructionSet();
        int                            undefined = -1;
        int                            opcode    = 0;
        std::vector<Byte>              bytes;
        std::vector<DisassembledLine>  lines;

        for (opcode = 0; opcode < 256 && undefined < 0; opcode++)
        {
            if (!table[opcode].isLegal)
            {
                undefined = opcode;
            }
        }

        Assert::IsTrue (undefined >= 0, L"the NMOS table leaves at least one opcode undefined");

        bytes = { (Byte) undefined, 0xEA };

        Disassembler::Disassemble (bytes, kOrigin, table, lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string (Disassembler::kUndefinedMnemonic), lines[0].mnemonic);
        Assert::AreEqual ((size_t) 1, lines[0].bytes.size());
        Assert::IsFalse  (lines[0].isDefined);
        Assert::AreEqual (std::string ("NOP"), lines[1].mnemonic);
    }



    TEST_METHOD (AnOperandCutOffByTheBuffer_IsNotInvented)
    {
        Cpu                            cpu;
        std::vector<Byte>              bytes = { 0xEA, 0xAD, 0x34 };
        std::vector<DisassembledLine>  lines;

        Disassembler::Disassemble (bytes, kOrigin, cpu.GetInstructionSet(), lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string (Disassembler::kUndefinedMnemonic), lines[1].mnemonic);
        Assert::AreEqual ((size_t) 2, lines[1].bytes.size());
        Assert::IsFalse  (lines[1].isDefined);
    }



    TEST_METHOD (FormatLine_ColumnsAddressBytesAndText)
    {
        Cpu                            cpu;
        std::vector<Byte>              bytes = { 0xA9, 0x01 };
        std::vector<DisassembledLine>  lines;

        Disassembler::Disassemble (bytes, 0x2000, cpu.GetInstructionSet(), lines);

        Assert::AreEqual (std::string ("2000  A9 01     LDA #$01"), Disassembler::FormatLine (lines[0]));
    }
};
