#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/CsvWriter.h"
#include "FileTypes/GlyphSheet.h"
#include "FileTypes/RtfWriter.h"
#include "FileTypes/TextCharset.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriterTests, CsvWriterTests, GlyphSheetTests and TextCharsetTests
//
//  The writers' output for documents, grids and glyphs built in each test,
//  and text in each character set.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RtfWriterTests)
{
public:

    TEST_METHOD (Write_StylesAlignmentsTabsAndUnicode)
    {
        std::vector<Paragraph>  paragraphs (2);
        std::vector<Byte>       bytes;
        std::string             rtf;



        paragraphs[0].alignment = Alignment::Center;
        paragraphs[0].runs      = { Run { L"Bold", true }, Run { L"\tplain {x}" } };
        paragraphs[1].runs      = { Run { L"\x00E9", false, true, true } };

        RtfWriter::Write (paragraphs, bytes);
        rtf.assign (bytes.begin(), bytes.end());

        Assert::IsTrue (rtf.starts_with ("{\\rtf1\\ansi"));
        Assert::IsTrue (rtf.find ("\\pard\\qc \\b \\i0 \\ulnone Bold") != std::string::npos);
        Assert::IsTrue (rtf.find ("\\tab plain \\{x\\}") != std::string::npos);
        Assert::IsTrue (rtf.find ("\\i \\ul \\u233?") != std::string::npos);
        Assert::IsTrue (rtf.ends_with ("}\r\n"));
    }
};



TEST_CLASS (CsvWriterTests)
{
public:

    TEST_METHOD (Write_QuotesFieldsWithCommasQuotesAndBreaks)
    {
        Grid               grid;
        std::vector<Byte>  bytes;
        std::string        csv;



        grid.columns = { L"A", L"B" };
        grid.rows    = { { L"1,2", L"say \"hi\"" }, { L"two\nlines", L"\x00E9" } };

        CsvWriter::Write (grid, bytes);
        csv.assign (bytes.begin(), bytes.end());

        Assert::AreEqual (std::string ("\xEF\xBB\xBF" "A,B\r\n\"1,2\",\"say \"\"hi\"\"\"\r\n\"two\nlines\",\xC3\xA9\r\n"), csv);
    }
};



TEST_CLASS (GlyphSheetTests)
{
public:

    TEST_METHOD (Draw_PutsSixteenGlyphsToARowAndLeavesAMissingOneBlank)
    {
        GlyphSheet   sheet;
        DecodedFile  file;
        Glyph        glyph;



        glyph.width  = 2;
        glyph.height = 2;
        glyph.pixels = { true, false, false, true };

        sheet.glyphs.assign (17, glyph);
        sheet.glyphs[1] = Glyph();
        sheet.name      = L"TEST";

        sheet.Draw (file);

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Picture);
        Assert::AreEqual (3 * 16, file.width);
        Assert::AreEqual (3 * 2, file.height);
        Assert::AreEqual (0xFF000000u, file.bgra[0], L"The first glyph's ink");
        Assert::AreEqual (0xFFFFFFFFu, file.bgra[3], L"The blank glyph's cell");
        Assert::AreEqual (0xFF000000u, file.bgra[(size_t) 3 * 48], L"The seventeenth glyph starts the second row");
    }
};



TEST_CLASS (TextCharsetTests)
{
public:

    TEST_METHOD (DecodeLines_ReadsEachCharacterSet)
    {
        std::vector<Byte>          apple = { 0xC8, 0xC9, 0x8D, 0x81, 0xC1 };
        std::vector<Byte>          roman = { 0x8E, 0x0D, 0x41 };
        std::vector<std::wstring>  lines;



        lines = TextCharset::DecodeLines (apple, Charset::AppleText);
        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::wstring (L"HI"), lines[0]);
        Assert::AreEqual (std::wstring (L" A"), lines[1], L"A control character is a space");

        lines = TextCharset::DecodeLines (roman, Charset::MacOsRoman);
        Assert::AreEqual (std::wstring (L"\x00E9"), lines[0], L"Mac OS Roman $8E is e-acute");

        lines = TextCharset::DecodeLines (roman, Charset::Iso8859_1);
        Assert::AreEqual ((wchar_t) 0x8E, lines[0][0]);

        Assert::AreEqual (std::wstring (L"\x2713"), TextCharset::FromMouseText (4));
        Assert::AreEqual ((size_t) 2, TextCharset::FromMouseText (0).size(), L"An apple is outside the BMP");
    }
};
