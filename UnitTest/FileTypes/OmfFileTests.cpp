#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/OmfFile.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFileTests
//
//  A version 2.1 segment built with a CONST, a cRELOC, a GLOBAL and an EXPR,
//  then listed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (OmfFileTests)
{
public:

    static std::vector<Byte> MakeSegment()
    {
        std::vector<Byte>  bytes (0x30, 0);



        bytes[14] = 4;
        bytes[15] = 2;
        bytes[17] = 0x01;
        bytes[21] = 0x80;
        bytes[34] = 1;
        bytes[40] = 0x30;
        bytes[42] = 0x30 + 10 + 5;

        bytes.insert (bytes.end(), 10, ' ');
        bytes.insert (bytes.end(), { 4, 'M', 'A', 'I', 'N' });
        bytes.insert (bytes.end(), { 0x03, 0xA9, 0x00, 0x60 });
        bytes.insert (bytes.end(), { 0xF5, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00 });
        bytes.insert (bytes.end(), { 0xE6, 0x01, 'X', 0x01, 0x00, 'N', 0x00 });
        bytes.insert (bytes.end(), { 0xEB, 0x02, 0x83, 0x01, 'X', 0x81, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00 });
        bytes.insert (bytes.end(), { 0x00 });

        bytes[0] = (Byte) bytes.size();
        bytes[8] = 3;

        return bytes;
    }


    TEST_METHOD (Decode_ListsTheHeaderAndEachRecord)
    {
        std::vector<Byte>  bytes = MakeSegment();
        DecodedFile        file;
        std::wstring       error;



        Assert::IsTrue (OmfFile::Check (bytes));
        AssertSucceeded (OmfFile::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual (std::wstring (L"Segment 1: \"MAIN\", load name \"\", code, dynamic, version 2.1"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"  CONST 3 bytes"), file.lines[2]);
        Assert::AreEqual (std::wstring (L"    0000: A9 00 60"), file.lines[3]);
        Assert::AreEqual (std::wstring (L"  cRELOC 2 bytes, shift 0, at $1, value $2"), file.lines[4]);
        Assert::AreEqual (std::wstring (L"  GLOBAL X, length 1, type 'N'"), file.lines[5]);
        Assert::AreEqual (std::wstring (L"  EXPR 2 bytes: X $4 +"), file.lines[6]);
        Assert::AreEqual (std::wstring (L"  END"), file.lines[7]);
        Assert::IsTrue   (file.notes.empty());
    }


    TEST_METHOD (Check_FailsAHeaderThatDoesNotRead)
    {
        std::vector<Byte>  bytes = MakeSegment();



        bytes[14] = 2;
        Assert::IsFalse (OmfFile::Check (bytes), L"A number length other than 4");

        bytes     = MakeSegment();
        bytes[0]  = 0xFF;
        Assert::IsFalse (OmfFile::Check (bytes), L"A segment longer than the file");
    }


    TEST_METHOD (DescribeKind_TypeAndAttributes)
    {
        Assert::AreEqual (std::wstring (L"direct page and stack, static"),      OmfFile::DescribeKind (0x0012));
        Assert::AreEqual (std::wstring (L"data, dynamic, private, skip"),       OmfFile::DescribeKind (0xD001));
    }
};
