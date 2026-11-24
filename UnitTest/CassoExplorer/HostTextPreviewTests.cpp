#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/HostTextPreview.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreviewTests
//
//  The character set a host file is text in, the files that preview as
//  text, and a file's bytes as lines of text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostTextPreviewTests)
{
public:

    static std::vector<Byte> MakeBytes (const std::string & text)
    {
        return std::vector<Byte> (text.begin(), text.end());
    }


    TEST_METHOD (HostTextCharsets)
    {
        const Byte  ascii[]   = { 'R', 'E', 'A', 'D', '\r', '\n', '\t', 'x', 0x0C, 0x1A };
        const Byte  dos[]     = { ' ', 0xC4, 0xC4, 0xCD, 0xB3, 'Z', 0x82, '\r', '\n' };
        const Byte  windows[] = { 'c', 'a', 'f', 0xE9, ' ', 0xFC, 'r' };
        const Byte  utf8[]    = { 'c', 'a', 'f', 0xC3, 0xA9 };
        const Byte  cutOff[]  = { 'a', 0xE2, 0x80 };
        const Byte  binary[]  = { 'M', 'Z', 0x90, 0x00, 0x03 };

        Assert::IsTrue (HostTextPreview::GetHostCharset (ascii)   == HostCharset::Ascii,   L"Tabs, line breaks, form feeds and DOS's end mark are text");
        Assert::IsTrue (HostTextPreview::GetHostCharset (dos)     == HostCharset::Dos,     L"Mostly box drawing is DOS text");
        Assert::IsTrue (HostTextPreview::GetHostCharset (windows) == HostCharset::Windows, L"Accented letters are Windows text");
        Assert::IsTrue (HostTextPreview::GetHostCharset (utf8)    == HostCharset::Utf8,    L"Well-formed UTF-8 is UTF-8");
        Assert::IsTrue (HostTextPreview::GetHostCharset (cutOff)  == HostCharset::Utf8,    L"A sequence cut off at the end of the sample is UTF-8");
        Assert::IsTrue (HostTextPreview::GetHostCharset (binary)  == HostCharset::None,    L"A control code other than those is binary");
        Assert::IsTrue (HostTextPreview::GetHostCharset ({})      == HostCharset::None,    L"An empty file is no text");

        //  DOS text draws a control code now and then as a symbol, such as a
        //  menu's pointer; more than one byte in a hundred is no text.
        std::vector<Byte>  pointer (200, 'a');

        pointer[50] = 0x10;
        Assert::IsTrue (HostTextPreview::GetHostCharset (pointer) == HostCharset::Dos,     L"A DOS symbol among text is DOS text");

        pointer[51] = 0x10;
        pointer[52] = 0x11;
        Assert::IsTrue (HostTextPreview::GetHostCharset (pointer) == HostCharset::None,    L"but too many are binary");
    }


    TEST_METHOD (SourceNames)
    {
        Assert::IsTrue  (HostTextPreview::IsSourceName (L"STAGE2.S"));
        Assert::IsTrue  (HostTextPreview::IsSourceName (L"loader.a65"));
        Assert::IsTrue  (HostTextPreview::IsSourceName (L"Game.Asm"), L"The extension is matched in any case");
        Assert::IsFalse (HostTextPreview::IsSourceName (L"readme.txt"));
        Assert::IsFalse (HostTextPreview::IsSourceName (L"asm"), L"A name with no dot has no extension");
    }


    TEST_METHOD (AppleExportNames)
    {
        Assert::IsTrue  (HostTextPreview::IsAppleExportName (L"GAME.Integer BASIC.txt"));
        Assert::IsTrue  (HostTextPreview::IsAppleExportName (L"NOTES.Text.txt"));
        Assert::IsFalse (HostTextPreview::IsAppleExportName (L"readme.txt"));
    }


    TEST_METHOD (TryPreviewText_Utf16WithMark_BothByteOrders)
    {
        PreviewContent  preview;
        const Byte      littleEndian[] = { 0xFF, 0xFE, 'h', 0x00, 'i', 0x00 };
        const Byte      bigEndian[]    = { 0xFE, 0xFF, 0x00, 'h', 0x00, 'i' };

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"wide.bin", littleEndian, preview));
        Assert::IsTrue (preview.lines == std::vector<std::wstring> ({ L"hi" }), L"Little-endian, with the mark dropped");

        preview = PreviewContent();
        Assert::IsTrue (HostTextPreview::TryPreviewText (L"wide.bin", bigEndian, preview));
        Assert::IsTrue (preview.lines == std::vector<std::wstring> ({ L"hi" }), L"Big-endian, with the mark dropped");
    }


    TEST_METHOD (TryPreviewText_Utf8_DropsTheMarkAndExpandsTabs)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("\xEF\xBB\xBF" "a\tb\r\n\tc");

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"notes.txt", bytes, preview));
        Assert::IsTrue (preview.kind == PreviewContent::Kind::Text);
        Assert::IsFalse (preview.source);
        Assert::IsTrue (preview.lines == std::vector<std::wstring> ({ L"a       b", L"        c" }), L"Tabs go to the next stop of eight");
    }


    TEST_METHOD (TryPreviewText_LineBreaks_CrLfAndCrAndLf)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("one\r\ntwo\rthree\nfour\xC3\xA9");

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"notes.txt", bytes, preview));
        Assert::AreEqual ((size_t) 4, preview.lines.size());
        Assert::AreEqual (std::wstring (L"one"),        preview.lines[0]);
        Assert::AreEqual (std::wstring (L"two"),        preview.lines[1]);
        Assert::AreEqual (std::wstring (L"three"),      preview.lines[2]);
        Assert::AreEqual (std::wstring (L"four\u00E9"), preview.lines[3]);
    }


    TEST_METHOD (TryPreviewText_FinalBreak_AddsNoLine)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("a\xC3\xA9\r\n");

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"notes.txt", bytes, preview));
        Assert::AreEqual ((size_t) 1, preview.lines.size(), L"A last line break ends the last line");
    }


    TEST_METHOD (TryPreviewText_PlainAsciiThatIsNeitherSourceNorExport_IsRejected)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("just ascii\r\n");

        Assert::IsFalse (HostTextPreview::TryPreviewText (L"readme.txt", bytes, preview));
        Assert::IsTrue  (preview.lines.empty(), L"The preview is as it was");
    }


    TEST_METHOD (TryPreviewText_SourceFile_IsAcceptedWithItsLineNumbers)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("        LDA #1\r\n");

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"boot.s", bytes, preview));
        Assert::IsTrue (preview.source);
        Assert::IsTrue (preview.kind == PreviewContent::Kind::Text);
    }


    TEST_METHOD (TryPreviewText_DosTextExport_IsAccepted)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("10  PRINT \"\xC4\xC4\"\r\n");

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"HELLO.Applesoft BASIC.txt", bytes, preview));
        Assert::IsFalse (preview.source);
        Assert::AreEqual ((size_t) 1, preview.lines.size());
        Assert::AreEqual (std::wstring (L"10  PRINT \"\u2500\u2500\""), preview.lines[0], L"DOS code page 437's line-drawing characters");
    }


    TEST_METHOD (TryPreviewText_Success_ClearsTheBytesAndTheHostPath)
    {
        PreviewContent      preview;
        std::vector<Byte>   bytes = MakeBytes ("\xEF\xBB\xBF" "x");

        preview.bytes    = { 1, 2, 3 };
        preview.hostPath = L"C:\\Disks\\notes.txt";

        Assert::IsTrue (HostTextPreview::TryPreviewText (L"notes.txt", bytes, preview));
        Assert::IsTrue (preview.bytes.empty());
        Assert::IsTrue (preview.hostPath.empty());
    }
};
