#include "Pch.h"
#include "Seams/ShellVerbs/Win32FileContents.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileContentsTextTests
//
//  Which byte sequences count as text for a file with no filter, and how
//  they decode. Bytes are built in memory; no file is opened.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileContentsTextTests)
{
public:

    static std::vector<Byte>  Bytes (const std::string & text)
    {
        return std::vector<Byte> (text.begin(), text.end());
    }


    static std::vector<Byte>  Utf16 (const std::wstring & text, bool isBigEndian, bool hasBom)
    {
        std::vector<Byte>  bytes;

        if (hasBom)
        {
            bytes.push_back (isBigEndian ? 0xFE : 0xFF);
            bytes.push_back (isBigEndian ? 0xFF : 0xFE);
        }

        for (wchar_t ch : text)
        {
            bytes.push_back ((Byte) (isBigEndian ? ch >> 8 : ch & 0xFF));
            bytes.push_back ((Byte) (isBigEndian ? ch & 0xFF : ch >> 8));
        }

        return bytes;
    }


    static bool  Looks (const std::vector<Byte> & bytes)
    {
        return Win32FileContents::LooksLikeText (bytes.data(), bytes.size());
    }


    static std::wstring  Decode (const std::vector<Byte> & bytes)
    {
        return Win32FileContents::DecodeText (bytes.data(), bytes.size());
    }


    TEST_METHOD (LooksLikeText_Ascii_IsText)
    {
        Assert::IsTrue (Looks (Bytes ("hello world\r\nsecond line\ttabbed\r\n")));
    }


    TEST_METHOD (LooksLikeText_NoBytes_IsNotText)
    {
        Assert::IsFalse (Looks (Bytes ("")));
    }


    TEST_METHOD (LooksLikeText_Utf8WithAndWithoutBom_IsText)
    {
        Assert::IsTrue (Looks (Bytes ("caf\xC3\xA9 au lait")));
        Assert::IsTrue (Looks (Bytes ("\xEF\xBB\xBF" "caf\xC3\xA9")));
    }


    TEST_METHOD (LooksLikeText_AnsiHighBytes_IsText)
    {
        Assert::IsTrue (Looks (Bytes ("caf\xE9 cr\xE8me")));
    }


    TEST_METHOD (LooksLikeText_Utf16WithBom_IsText)
    {
        Assert::IsTrue (Looks (Utf16 (L"log line\r\n", false, true)));
        Assert::IsTrue (Looks (Utf16 (L"log line\r\n", true,  true)));
    }


    TEST_METHOD (LooksLikeText_Utf16WithoutBom_IsText)
    {
        Assert::IsTrue (Looks (Utf16 (L"2026-11-05 started service\r\n", false, false)));
        Assert::IsTrue (Looks (Utf16 (L"2026-11-05 started service\r\n", true,  false)));
    }


    TEST_METHOD (LooksLikeText_ZeroBytes_IsNotText)
    {
        Assert::IsFalse (Looks (std::vector<Byte> (512, 0)));
    }


    TEST_METHOD (LooksLikeText_BinaryHeader_IsNotText)
    {
        //  The start of a PE file: "MZ", then zeros and control characters.
        std::vector<Byte>  pe = { 'M', 'Z', 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00 };

        Assert::IsFalse (Looks (pe));
    }


    TEST_METHOD (LooksLikeText_ControlCharacters_IsNotText)
    {
        Assert::IsFalse (Looks (Bytes ("text then \x01\x02\x03 control")));
    }


    TEST_METHOD (DecodeText_Ascii_ReturnsTheText)
    {
        Assert::AreEqual (std::wstring (L"hello"), Decode (Bytes ("hello")));
    }


    TEST_METHOD (DecodeText_Utf8WithBom_ResultHasNoBom)
    {
        Assert::AreEqual (std::wstring (L"caf\x00E9"), Decode (Bytes ("\xEF\xBB\xBF" "caf\xC3\xA9")));
    }


    TEST_METHOD (DecodeText_Utf8WithoutBom_DecodesMultiByteCharacters)
    {
        Assert::AreEqual (std::wstring (L"caf\x00E9 \x20AC"), Decode (Bytes ("caf\xC3\xA9 \xE2\x82\xAC")));
    }


    TEST_METHOD (DecodeText_Utf16LittleEndianWithBom_ResultHasNoBom)
    {
        Assert::AreEqual (std::wstring (L"caf\x00E9 \x20AC"), Decode (Utf16 (L"caf\x00E9 \x20AC", false, true)));
    }


    TEST_METHOD (DecodeText_Utf16BigEndianWithBom_ResultHasNoBom)
    {
        Assert::AreEqual (std::wstring (L"caf\x00E9 \x20AC"), Decode (Utf16 (L"caf\x00E9 \x20AC", true, true)));
    }


    TEST_METHOD (DecodeText_Utf16WithoutBom_DetectedFromZeroBytes)
    {
        Assert::AreEqual (std::wstring (L"service started"), Decode (Utf16 (L"service started", false, false)));
        Assert::AreEqual (std::wstring (L"service started"), Decode (Utf16 (L"service started", true,  false)));
    }


    TEST_METHOD (DecodeText_AnsiBytes_UseTheAnsiCodePage)
    {
        std::wstring  text = Decode (Bytes ("caf\xE9"));

        Assert::AreEqual ((size_t) 4, text.size());
        Assert::AreEqual (L"caf", text.substr (0, 3).c_str());
        Assert::AreNotEqual ((int) 0xFFFD, (int) text[3], L"A byte that is invalid UTF-8 is not replaced");
    }


    TEST_METHOD (DecodeText_NoBytes_ReturnsEmpty)
    {
        Assert::IsTrue (Decode (Bytes ("")).empty());
    }
};
