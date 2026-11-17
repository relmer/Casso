#include "Pch.h"
#include "../EhmTestHelper.h"
#include "PCodeDisassembler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassemblerTests
//
//  The procedure of LINEFEED.CODE from the Apple Pascal 1.3 disks, and one
//  instruction for each kind of operand.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PCodeDisassemblerTests)
{
public:

    struct Listed
    {
        const char *  mnemonic;
        const char *  operand;
        size_t        length;
    };



    static void AssertLines (const std::vector<PCodeLine> & lines, const std::vector<Listed> & listed)
    {
        Assert::AreEqual (listed.size(), lines.size(), L"one line per instruction");

        for (size_t i = 0; i < listed.size(); i++)
        {
            Assert::AreEqual (std::string (listed[i].mnemonic), lines[i].mnemonic);
            Assert::AreEqual (std::string (listed[i].operand),  lines[i].operand);
            Assert::AreEqual (listed[i].length, lines[i].bytes.size());
            Assert::IsTrue   (lines[i].isDefined);
        }
    }



    static std::vector<PCodeLine> Disassemble (const std::vector<Byte> & bytes, size_t attributes = 0)
    {
        std::vector<PCodeLine>  lines;

        PCodeDisassembler::Disassemble (bytes, 0, bytes.size(), attributes, lines);

        return lines;
    }



    TEST_METHOD (Disassemble_LinefeedProcedure)
    {
        std::vector<Byte>       segment =
        {
            0xD7, 0xD7, 0xC7, 0xF1, 0x40, 0x91, 0xAB, 0x03, 0xEA, 0x00, 0x00, 0x01, 0x88, 0xC7, 0xFF, 0x00,
            0x00, 0xC7, 0xFF, 0x00, 0x88, 0xBF, 0xC1, 0x00, 0x02, 0x00, 0x04, 0x00, 0x06, 0x00, 0x1E, 0x00,
            0x01, 0x00, 0x02, 0x00, 0x01, 0x01,
        };
        std::vector<PCodeLine>  lines;



        PCodeDisassembler::Disassemble (segment, 0, 24, 32, lines);

        AssertLines (lines,
        {
            { "NOP",  "",      1 }, { "NOP",  "",    1 }, { "LDCI", "16625", 3 }, { "NGI",  "",  1 },
            { "SRO",  "3",     2 }, { "SLDO", "3",   1 }, { "SLDC", "0",     1 }, { "SLDC", "0", 1 },
            { "SLDC", "1",     1 }, { "CHK",  "",    1 }, { "LDCI", "255",   3 }, { "SLDC", "0", 1 },
            { "LDCI", "255",   3 }, { "CHK",  "",    1 }, { "STB",  "",      1 }, { "RBP",  "0", 2 },
        });

        Assert::AreEqual (std::string ("0002  C7 F1 40     LDCI  16625"), PCodeDisassembler::FormatLine (lines[2]));
        Assert::AreEqual (std::string ("0000  D7           NOP"),         PCodeDisassembler::FormatLine (lines[0]));
        Assert::IsTrue   (PCodeDisassembler::IsReturn (lines.back()));
        Assert::IsFalse  (PCodeDisassembler::IsReturn (lines[0]));
    }


    TEST_METHOD (Disassemble_EachOperandKind)
    {
        AssertLines (Disassemble ({ 0x9E, 0x04 }),                   { { "CSP",  "EXIT",   2 } });
        AssertLines (Disassemble ({ 0x9E, 0x0D }),                   { { "CSP",  "13",     2 } });
        AssertLines (Disassemble ({ 0xA6, 0x02, 'H', 'I' }),         { { "LSA",  "'HI'",   4 } });
        AssertLines (Disassemble ({ 0xB3, 0x01, 0x02, 0x01 }),       { { "LDC",  "258",    4 } });
        AssertLines (Disassemble ({ 0xAF, 0x0A, 0x05 }),             { { "EQU",  "BYTE,5", 3 } });
        AssertLines (Disassemble ({ 0xB7, 0x02 }),                   { { "NEQ",  "REAL",   2 } });
        AssertLines (Disassemble ({ 0xCA, 0x81, 0x00 }),             { { "LDL",  "256",    3 } });
        AssertLines (Disassemble ({ 0xB6, 0x01, 0x05 }),             { { "LOD",  "1,5",    3 } });
        AssertLines (Disassemble ({ 0xCD, 0x02, 0x03 }),             { { "CXP",  "2,3",    3 } });
        AssertLines (Disassemble ({ 0xC7, 0xFF, 0xFF }),             { { "LDCI", "-1",     3 } });
        AssertLines (Disassemble ({ 0x7F, 0xD8, 0xE8, 0xF8, 0xFF }), { { "SLDC", "127", 1 }, { "SLDL", "1", 1 }, { "SLDO", "1", 1 }, { "SIND", "0", 1 }, { "SIND", "7", 1 } });
    }


    TEST_METHOD (Disassemble_JumpForwardAndThroughTheJumpTable)
    {
        //  The word at offset 6 is the jump table's first, ten bytes below
        //  the attribute table at 16, and its target is offset 0.
        std::vector<Byte>       segment = { 0xD7, 0xA1, 0xF6, 0xB9, 0x01, 0x00, 0x06, 0x00 };
        std::vector<PCodeLine>  lines;



        PCodeDisassembler::Disassemble (segment, 0, 5, 16, lines);

        AssertLines (lines, { { "NOP", "", 1 }, { "FJP", "$0000", 2 }, { "UJP", "$0006", 2 } });
    }


    TEST_METHOD (Disassemble_CaseJumpListsItsTable)
    {
        std::vector<Byte>  segment =
        {
            0xD7,                      // NOP
            0xAC, 0x01, 0x00, 0x02, 0x00,   // XJP 1,2
            0xB9, 0x04,                // UJP past the table
            0x08, 0x00, 0x09, 0x00,    // the cases, to offsets 0 and 1
            0xD7,
        };



        AssertLines (Disassemble (segment),
        {
            { "NOP", "",      1 }, { "XJP", "1,2",   5 }, { "UJP", "$000C", 2 },
            { "DW",  "$0000", 2 }, { "DW",  "$0001", 2 }, { "NOP", "",      1 },
        });
    }


    TEST_METHOD (Disassemble_UndefinedAndCutOffInstructions)
    {
        std::vector<PCodeLine>  undefined = Disassemble ({ 0xD2 });
        std::vector<PCodeLine>  cut       = Disassemble ({ 0xD7, 0xC7, 0x01 });



        Assert::AreEqual ((size_t) 1, undefined.size());
        Assert::AreEqual (std::string (PCodeDisassembler::kUndefinedMnemonic), undefined[0].mnemonic);
        Assert::IsFalse  (undefined[0].isDefined);

        Assert::AreEqual ((size_t) 2, cut.size());
        Assert::AreEqual (std::string (PCodeDisassembler::kUndefinedMnemonic), cut[1].mnemonic);
        Assert::AreEqual ((size_t) 2, cut[1].bytes.size());
        Assert::IsFalse  (cut[1].isDefined);
    }
};
