#include "Pch.h"

#include "FileTypes/FileFormatTable.h"
#include "FileTypes/HexDump.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HexDumpTests
//
//  A file's bytes as hex dump lines: the offset, the bytes in two halves,
//  and the characters in each character set.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HexDumpTests)
{
public:

    TEST_METHOD (Format_AFullLineAndAShortOne_LineUp)
    {
        std::vector<Byte>          bytes;
        std::vector<std::wstring>  lines;



        for (int i = 0; i < 20; i++)
        {
            bytes.push_back ((Byte) (0xC1 + i));
        }

        lines = HexDump::Format (bytes, Charset::AppleText);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::wstring (L"0000: C1 C2 C3 C4 C5 C6 C7 C8  C9 CA CB CC CD CE CF D0  ABCDEFGHIJKLMNOP"), lines[0]);
        Assert::AreEqual (std::wstring (L"0010: D1 D2 D3 D4                                       QRST"),             lines[1]);
    }


    TEST_METHOD (Format_ControlCharacters_ArePeriods)
    {
        std::vector<Byte>  bytes = { 0x8D, 0x09, 0xA0, 0x7F, 0x41 };

        Assert::AreEqual (std::wstring (L".. .A"), HexDump::Format (bytes, Charset::AppleText)[0].substr (56));
    }


    TEST_METHOD (Format_ReadsTheCharacterSet)
    {
        std::vector<Byte>  bytes = { 0x8E, 0x83, 0xE9 };

        Assert::AreEqual (std::wstring (L"\x00E9\x00C9\x00C8"), HexDump::Format (bytes, Charset::MacOsRoman)[0].substr (56), L"$8E, $83 and $E9 in Mac OS Roman");
        Assert::AreEqual (std::wstring (L"..\x00E9"),           HexDump::Format (bytes, Charset::Iso8859_1)[0].substr (56),  L"$80 to $9F are controls in ISO 8859-1");
        Assert::AreEqual (std::wstring (L"..i"),                HexDump::Format (bytes, Charset::AppleText)[0].substr (56),  L"Apple text drops the high bit");
    }


    TEST_METHOD (Format_AFileOver64K_HasSixDigitOffsets)
    {
        std::vector<Byte>  bytes (0x10001, 0);

        Assert::IsTrue (HexDump::Format (bytes, Charset::AppleText).back().starts_with (L"010000: 00 "));
        Assert::IsTrue (HexDump::Format (std::vector<Byte> (0x10000, 0), Charset::AppleText).back().starts_with (L"FFF0: "));
        Assert::IsTrue (HexDump::Format ({}, Charset::AppleText).empty());
    }


    TEST_METHOD (Decode_ThroughTheTable_IsTextUnderItsSuffix)
    {
        std::vector<Byte>  bytes = { 0xC1 };
        DecodedFile        file;
        std::wstring       error;
        std::vector<Byte>  exported;

        Assert::AreEqual (S_OK, FileFormatTable::Decode (FileFormat::HexDump, bytes, FileFacts(), DecodeOptions(), file, error));
        Assert::AreEqual ((size_t) 1, file.lines.size());
        Assert::AreEqual (std::wstring (L".hex dump.txt"), std::wstring (FileFormatTable::GetSuffix (FileFormat::HexDump)));
        Assert::AreEqual (S_OK, FileFormatTable::Export (file, exported));
        Assert::IsFalse  (exported.empty());
    }
};
