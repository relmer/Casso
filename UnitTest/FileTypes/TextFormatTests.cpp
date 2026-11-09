#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/MerlinSource.h"
#include "FileTypes/PascalText.h"
#include "FileTypes/RandomText.h"
#include "FileTypes/ScAssemblerSource.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalTextTests, RandomTextTests, MerlinSourceTests,
//  ScAssemblerSourceTests and FileFormatTableTests
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
            decoded.push_back (std::string (line.begin(), line.end()));
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
