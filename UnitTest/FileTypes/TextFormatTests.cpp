#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "FileTypes/BusinessBasic.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/GutenbergDocument.h"
#include "FileTypes/LisaSource.h"
#include "FileTypes/MagicWindowDocument.h"
#include "FileTypes/MerlinSource.h"
#include "FileTypes/PascalText.h"
#include "FileTypes/RandomText.h"
#include "FileTypes/ScAssemblerSource.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalTextTests, RandomTextTests, MerlinSourceTests,
//  ScAssemblerSourceTests, FileFormatTableTests, MagicWindowDocumentTests,
//  GutenbergDocumentTests, BusinessBasicTests and LisaSourceTests
//
//  Files built in each test, identified by the table, decoded, and for the
//  import rows encoded back.
//
////////////////////////////////////////////////////////////////////////////////

static FileEntry MakeEntry (const char * name, Byte type, Word aux = 0, bool hasAux = false)
{
    FileEntry  entry;



    entry.name       = name;
    entry.type       = type;
    entry.auxType    = aux;
    entry.hasAuxType = hasAux;

    return entry;
}



TEST_CLASS (PascalTextTests)
{
public:

    TEST_METHOD (Encode_AndDecode_RoundTripIndentAndPages)
    {
        std::vector<std::string>  lines = { "PROGRAM X;", "    BEGIN", std::string (1000, 'A'), "END." };
        std::vector<Byte>         bytes;
        DecodedFile               file;
        std::wstring              error;



        AssertSucceeded (PascalText::Encode (lines, bytes, error));

        Assert::AreEqual ((size_t) 3 * 1024, bytes.size(), L"The header, then a page for the long line's move");
        Assert::AreEqual ((Byte) 0x10, bytes[1024 + 11], L"The indent is a DLE code");

        AssertSucceeded (PascalText::Decode (bytes, FileFacts(), DecodeOptions { 0, Charset::AppleText }, file, error));
        Assert::AreEqual ((size_t) 4, file.lines.size());
        Assert::AreEqual (std::wstring (L"    BEGIN"), file.lines[1]);
        Assert::AreEqual ((size_t) 1000, file.lines[2].size());
    }


    TEST_METHOD (Decode_FailsAFileOfPartPages)
    {
        DecodedFile   file;
        std::wstring  error;
        HRESULT       hr = PascalText::Decode (std::vector<Byte> (1500, 0), FileFacts(), DecodeOptions(), file, error);



        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (error.empty());
    }
};



TEST_CLASS (RandomTextTests)
{
public:

    TEST_METHOD (Decode_SplitsRecordsAndFields)
    {
        std::vector<Byte>  bytes (64, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        bytes[0]  = 0xC1;
        bytes[1]  = 0x8D;
        bytes[2]  = 0xC2;
        bytes[3]  = 0x8D;
        bytes[32] = 0xC3;
        bytes[33] = 0x8D;

        Assert::AreEqual ((size_t) 32, RandomText::FindRecordLength (bytes));

        facts.volume = VolumeKind::Dos33;
        AssertSucceeded (RandomText::Decode (bytes, facts, DecodeOptions { 0, Charset::AppleText }, file, error));
        Assert::AreEqual ((size_t) 2, file.grid.rows.size());
        Assert::IsTrue   (file.grid.rows[0] == std::vector<std::wstring> ({ L"0", L"A", L"B" }));
        Assert::IsTrue   (file.grid.rows[1] == std::vector<std::wstring> ({ L"1", L"C" }));
        Assert::AreEqual (std::wstring (L"Field 2"), file.grid.columns[2]);

        Assert::AreEqual ((size_t) 0, RandomText::FindRecordLength (std::vector<Byte> { 0xC1, 0x8D, 0 }), L"A sequential file");
    }
};



TEST_CLASS (MerlinSourceTests)
{
public:

    TEST_METHOD (Encode_AndDecode_RoundTripMerlinsBytes)
    {
        std::vector<std::string>  lines = { "* A COMMENT", "START    LDA   #$00     ;CLEAR IT", "         ASC   \"HI THERE\"", "LOOP     JMP   LOOP" };
        std::vector<Byte>         bytes;
        std::vector<Byte>         again;
        std::vector<std::string>  decoded;
        DecodedFile               file;
        std::wstring              error;



        AssertSucceeded (MerlinSource::Encode (lines, bytes, error));
        Assert::IsTrue (MerlinSource::Check (bytes));

        AssertSucceeded (MerlinSource::Decode (bytes, FileFacts(), DecodeOptions(), file, error));
        Assert::AreEqual (std::wstring (L"START    LDA   #$00       ;CLEAR IT"), file.lines[1]);
        Assert::AreEqual (std::wstring (L"         ASC   \"HI THERE\""), file.lines[2]);

        for (const std::wstring & line : file.lines)
        {
            decoded.push_back (TextEncoding::WideToNarrow (line));
        }

        AssertSucceeded (MerlinSource::Encode (decoded, again, error));
        Assert::IsTrue (again == bytes, L"The listing encodes back to the same bytes");
    }


    TEST_METHOD (Encode_FailsALineTooLongWithItsNumber)
    {
        std::vector<Byte>  bytes;
        std::wstring       error;
        HRESULT            hr = MerlinSource::Encode ({ "OK", std::string (300, 'X') }, bytes, error);



        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (error.find (L"Line 2") != std::wstring::npos);
    }
};



TEST_CLASS (ScAssemblerSourceTests)
{
public:

    TEST_METHOD (Decode_ExpandsSpacesAndRuns)
    {
        std::vector<Byte>  bytes = { 10, 0x0A, 0x00, 'L', 0x83, 'A', 0xC0, 3, '-', 0 };
        DecodedFile        file;
        std::wstring       error;



        Assert::IsTrue  (ScAssemblerSource::Check (bytes));
        AssertSucceeded (ScAssemblerSource::Decode (bytes, FileFacts(), DecodeOptions(), file, error));
        Assert::AreEqual (std::wstring (L"0010 L   A---"), file.lines[0]);

        Assert::IsFalse (ScAssemblerSource::Check (std::vector<Byte> { 5, 0, 0, 0x01, 0 }), L"Integer BASIC's tokens fail the check");
    }
};



TEST_CLASS (FileFormatTableTests)
{
public:

    TEST_METHOD (Identify_MatchesEachRowAndMissesTheNearOnes)
    {
        std::vector<Byte>  merlin  = { 0xC1, 0xA0, 0xCE, 0xCF, 0xD0, 0x8D };
        std::vector<Byte>  textfile (2048, 0);



        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("SRC.S", 0x04), VolumeKind::ProDos, merlin) == FileFormat::Merlin);
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("T.SRC", 0x04), VolumeKind::Dos33, merlin) == FileFormat::Merlin);
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("SRC", 0x04), VolumeKind::ProDos, merlin) == FileFormat::None, L"No .S");
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("X.TEXT", 3), VolumeKind::Pascal, textfile) == FileFormat::PascalText);
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("X.TEXT", 3), VolumeKind::Pascal, std::vector<Byte> (100, 0)) == FileFormat::None, L"Too short");
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("DATA", 0x04, 64, true), VolumeKind::ProDos, std::vector<Byte> (128, 0xC1)) == FileFormat::RandomText);
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("DATA", 0x04), VolumeKind::ProDos, std::vector<Byte> (128, 0xC1)) == FileFormat::None, L"Sequential text");

        Assert::IsTrue (FileFormatTable::GetImportBySuffix (L"HELLO.Merlin.txt") == FileFormat::Merlin);
        Assert::IsTrue (FileFormatTable::GetImportBySuffix (L"HELLO.Random text.csv") == FileFormat::None, L"Not an import row");
        Assert::AreEqual (L".Pascal text.txt", FileFormatTable::GetSuffix (FileFormat::PascalText));
    }
};


TEST_CLASS (MagicWindowDocumentTests)
{
public:

    TEST_METHOD (Decode_SkipsTheHeaderAndDropsCommands)
    {
        std::vector<Byte>  bytes (256, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        bytes[0] = 0x8D;
        bytes[2] = 0xA0;
        bytes.insert (bytes.end(), { 0xC8, 0x89, 0xC9, 0x8D, 'l', 'o', 0x0D });

        facts.volume = VolumeKind::Dos33;
        facts.type   = 0x04;
        facts.name   = "LETTER.MW";

        Assert::IsTrue (MagicWindowDocument::Matches (facts, bytes));
        AssertSucceeded (MagicWindowDocument::Decode (bytes, facts, DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"HI", L"lo" });

        facts.name = "LETTER";
        Assert::IsFalse (MagicWindowDocument::Matches (facts, bytes));
    }
};


TEST_CLASS (GutenbergDocumentTests)
{
public:

    TEST_METHOD (Decode_SplitsLinesAndEndsAtTheFirstZero)
    {
        std::vector<Byte>  bytes = { 0xC8, 0xC9, 0x8D, 0x81, 0xEC, 0x9F, 0xEF, 0xFF, 0x8D, 0xD8, 0x00, 0xD9, 0x8D };
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        facts.volume = VolumeKind::Gutenberg;
        facts.type   = ' ';

        Assert::IsTrue (GutenbergDocument::Matches (facts, bytes));
        AssertSucceeded (GutenbergDocument::Decode (bytes, facts, DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"HI", L"lo", L"X" });
        Assert::IsTrue (file.notes.empty(), L"No alternate character, so no note");
    }


    TEST_METHOD (Decode_DropsAlternateCharactersWithANote)
    {
        std::vector<Byte>  bytes = { 0xC1, 0x41, 0xC2, 0x8D };
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (GutenbergDocument::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"AB" });
        Assert::AreEqual ((size_t) 1, file.notes.size());
    }


    TEST_METHOD (Matches_DocumentsOnAGutenbergVolumeOnly)
    {
        std::vector<Byte>  bytes = { 0xC8, 0xC9, 0x8D, 0x00 };
        FileFacts          facts;



        facts.volume = VolumeKind::Gutenberg;
        facts.type   = 'L';
        Assert::IsTrue (GutenbergDocument::Matches (facts, bytes), L"A locked document");

        facts.type = 'P';
        Assert::IsFalse (GutenbergDocument::Matches (facts, bytes), L"A program");

        facts.type = ' ';
        Assert::IsFalse (GutenbergDocument::Matches (facts, std::vector<Byte>()), L"An empty file");

        facts.volume = VolumeKind::Dos33;
        Assert::IsFalse (GutenbergDocument::Matches (facts, bytes), L"Another file system");

        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("TEXT", ' '), VolumeKind::Gutenberg, bytes) == FileFormat::Gutenberg);
        Assert::IsTrue (FileFormatTable::Identify (MakeEntry ("BOOT", 'P'), VolumeKind::Gutenberg, bytes) == FileFormat::None);
        Assert::AreEqual (L".Gutenberg.txt", FileFormatTable::GetSuffix (FileFormat::Gutenberg));
    }
};


TEST_CLASS (BusinessBasicTests)
{
public:

    TEST_METHOD (ListLine_KeywordsStringsAndRemarks)
    {
        Assert::AreEqual (std::wstring (L"PRINT \"HI\":REM X"),  BusinessBasic::ListLine (std::vector<Byte> { 0xD9, '"', 'H', 'I', '"', ':', 0xC0, ' ', 'X' }));
        Assert::AreEqual (std::wstring (L"LET A=LEN(B$)"),       BusinessBasic::ListLine (std::vector<Byte> { 0xB9, 'A', '=', 0xFF, 0xC6, 'B', '$', ')' }));
        Assert::AreEqual (std::wstring (L"GOTO 10"),             BusinessBasic::ListLine (std::vector<Byte> { 0xBA, '1', '0' }));
        Assert::AreEqual (std::wstring (L"{$8A}"),               BusinessBasic::ListLine (std::vector<Byte> { 0x8A }));
    }


    TEST_METHOD (Decode_LinesToTheZeroLength)
    {
        std::vector<Byte>  bytes = { 0, 0, 7, 10, 0, 0xBA, '1', '0', 0, 4, 20, 0, 0, 0 };
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (BusinessBasic::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"10 GOTO 10", L"20" });
        Assert::IsTrue (file.notes.empty());
    }
};


TEST_CLASS (LisaSourceTests)
{
public:

    TEST_METHOD (Decode_Version2LabelMnemonicAndOperand)
    {
        std::vector<Byte>  bytes = { 0x00, 0x18, 17, 0, 11, 'L', 'O', 'O', 'P', 0xCA, 0x01, 'L', 'O', 'O', 'P', 0x0D, 0xFF };
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        facts.volume = VolumeKind::Dos33;
        facts.type   = 0x40;

        Assert::IsTrue (LisaSource::Matches (facts, bytes));
        AssertSucceeded (LisaSource::Decode (bytes, facts, DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"LOOP     JMP LOOP" });
    }


    TEST_METHOD (Decode_Version4SymbolsAndOperands)
    {
        std::vector<Byte>  bytes (16, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        bytes[4] = 1;
        bytes[6] = 9;
        bytes[7] = 18;
        bytes[8] = 40;
        bytes.insert (bytes.end(), { 7, 'S', 'T', 'A', 'R', 'T', 0 });
        bytes[2] = (Byte) bytes.size();
        bytes.insert (bytes.end(), { 0xFA, 0x00, 0x00, 5, 0x06, 0x00, 0x1C, 0x10, 0xBD, 0x00 });

        facts.volume     = VolumeKind::ProDos;
        facts.type       = 0xFA;
        facts.auxType    = 0x50E1;
        facts.hasAuxType = true;

        Assert::IsTrue (LisaSource::Matches (facts, bytes));
        AssertSucceeded (LisaSource::Decode (bytes, facts, DecodeOptions(), file, error));

        Assert::IsTrue (file.lines == std::vector<std::wstring> { L"start", L"         lda      $10", L"         rts" });
        Assert::IsTrue (file.notes.empty());
    }
};
