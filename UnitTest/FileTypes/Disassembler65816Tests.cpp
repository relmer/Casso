#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "FileTypes/Disassembler65816.h"
#include "FileTypes/FileFormatTable.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler65816Tests and CodeListingTests
//
//  Code built in each test, disassembled for each processor a copy's
//  options choose.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disassembler65816Tests)
{
public:

    TEST_METHOD (RepAndSep_SetTheWidthOfAnImmediate)
    {
        std::vector<Byte>              code = { 0xC2, 0x30, 0xA9, 0x34, 0x12, 0xE2, 0x20, 0xA9, 0x12, 0xA2, 0x00, 0x10 };
        std::vector<DisassembledLine>  lines;



        Disassembler65816::Disassemble (code, 0x2000, false, true, lines);

        Assert::AreEqual ((size_t) 5, lines.size());
        Assert::AreEqual (std::string ("REP"),    lines[0].mnemonic);
        Assert::AreEqual (std::string ("#$30"),   lines[0].operand);
        Assert::AreEqual (std::string ("#$1234"), lines[1].operand, L"after REP #$30, the accumulator is 16 bits");
        Assert::AreEqual (std::string ("#$12"),   lines[3].operand, L"after SEP #$20, it is 8 bits");
        Assert::AreEqual (std::string ("#$1000"), lines[4].operand, L"the index registers are 16 bits from the REP");
    }


    TEST_METHOD (WithoutFollowing_TheStartingWidthsAreKept)
    {
        std::vector<Byte>              code = { 0xE2, 0x30, 0xA9, 0x34, 0x12 };
        std::vector<DisassembledLine>  lines;



        Disassembler65816::Disassemble (code, 0x2000, true, false, lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("#$1234"), lines[1].operand);
    }


    TEST_METHOD (LongAndBlockAndBranchOperands)
    {
        std::vector<Byte>              code = { 0x22, 0x00, 0x10, 0xE1, 0x54, 0x01, 0x02, 0x82, 0xFD, 0xFF, 0xA7, 0x10, 0x83, 0x03 };
        std::vector<DisassembledLine>  lines;



        Disassembler65816::Disassemble (code, 0x2000, true, true, lines);

        Assert::AreEqual (std::string ("JSL"),       lines[0].mnemonic);
        Assert::AreEqual (std::string ("$E11000"),   lines[0].operand);
        Assert::AreEqual (std::string ("MVN"),       lines[1].mnemonic);
        Assert::AreEqual (std::string ("$02,$01"),   lines[1].operand, L"the source bank first");
        Assert::AreEqual (std::string ("BRL"),       lines[2].mnemonic);
        Assert::AreEqual (std::string ("$2007"),     lines[2].operand, L"a branch back to itself");
        Assert::AreEqual (std::string ("[$10]"),     lines[3].operand);
        Assert::AreEqual (std::string ("$03,S"),     lines[4].operand);
    }


    TEST_METHOD (ACutOffInstruction_IsListedAsBytes)
    {
        std::vector<Byte>              code = { 0xEA, 0xAD, 0x00 };
        std::vector<DisassembledLine>  lines;



        Disassembler65816::Disassemble (code, 0x2000, true, true, lines);

        Assert::AreEqual ((size_t) 3, lines.size());
        Assert::AreEqual (std::string ("NOP"), lines[0].mnemonic);
        Assert::AreEqual (std::string ("DFB"), lines[1].mnemonic);
        Assert::AreEqual (std::string ("$AD"), lines[1].operand);
    }
};





TEST_CLASS (CodeListingTests)
{
public:

    TEST_METHOD (EachProcessor_ReadsItsOwnOpcodes)
    {
        std::vector<Byte>          code = { 0x80, 0x00, 0xEA };
        std::vector<std::wstring>  nmos;
        std::vector<std::wstring>  cmos;



        PreviewDecoder::RenderDisassembly (code, 0x2000, VolumeKind::ProDos, false, nmos, DisassemblyCpu::Mos6502);
        PreviewDecoder::RenderDisassembly (code, 0x2000, VolumeKind::ProDos, false, cmos, DisassemblyCpu::Wdc65C02);

        Assert::IsTrue (nmos[0].find (L"BRA") == std::wstring::npos, L"$80 isn't a branch on the 6502");
        Assert::IsTrue (cmos[0].find (L"BRA") != std::wstring::npos, L"$80 is BRA on the 65C02");
    }


    TEST_METHOD (ACodeFile_IsDisassembledFromWhereItRuns)
    {
        std::vector<Byte>  code = { 0x4C, 0x00, 0x20 };
        FileEntry          entry;
        FileFacts          facts;
        DecodeOptions      options;
        DecodedFile        decoded;
        std::wstring       error;



        entry.name = "PROG.SYSTEM";
        entry.type = 0xFF;
        facts      = FileFacts::From (entry, VolumeKind::ProDos);

        options.disassembly = DisassemblyCpu::Wdc65C02;

        AssertSucceeded (FileFormatTable::Decode (FileFormat::Disassembly, code, facts, options, decoded, error));

        Assert::IsTrue (decoded.kind == DecodedFile::Kind::Lines);
        Assert::IsTrue (decoded.lines[0].starts_with (L"2000"), L"ProDOS loads a SYS file at $2000");
        Assert::IsTrue (std::wstring (FileFormatTable::GetSuffix (FileFormat::Disassembly)) == L".Disassembly.txt");
    }
};
