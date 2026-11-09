#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/AppleWorks.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksWpTests, AppleWorksDbTests and AppleWorksSsTests
//
//  Files built in each test from the layouts in File Type Notes $19, $1A
//  and $1B, then decoded.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendText (std::vector<Byte> & bytes, std::initializer_list<Byte> text, bool carriage)
{
    bytes.push_back ((Byte) (text.size() + 2));
    bytes.push_back (0);
    bytes.push_back (0);
    bytes.push_back ((Byte) (text.size() | (carriage ? 0x80 : 0)));
    bytes.insert (bytes.end(), text);
}



TEST_CLASS (AppleWorksWpTests)
{
public:

    TEST_METHOD (Decode_JoinsLinesAndStylesRuns)
    {
        std::vector<Byte>  bytes (300, 0);
        DecodedFile        file;
        std::wstring       error;



        AppendText (bytes, { 'O', 'n', 'e', ' ' }, false);
        AppendText (bytes, { 't', 'w', 'o', 0x01, 'B', 0x02 }, true);
        bytes.insert (bytes.end(), { 0x00, 0xE1 });
        AppendText (bytes, { 0x07, 'U', 0x08, 0x16, 'x' }, true);
        bytes.insert (bytes.end(), { 0x00, 0xD0, 0xFF, 0xFF });

        AssertSucceeded (AppleWorksWp::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual ((size_t) 3, file.paragraphs.size());
        Assert::AreEqual ((size_t) 2, file.paragraphs[0].runs.size());
        Assert::AreEqual (std::wstring (L"One two"), file.paragraphs[0].runs[0].text);
        Assert::IsTrue   (file.paragraphs[0].runs[1].bold);
        Assert::IsTrue   (file.paragraphs[1].alignment == Alignment::Center);
        Assert::IsTrue   (file.paragraphs[1].runs[0].underline);
        Assert::AreEqual (std::wstring (L"\tx"), file.paragraphs[1].runs[1].text);
        Assert::IsTrue   (file.paragraphs[2].runs.empty());
        Assert::IsTrue   (file.notes.empty());
    }


    TEST_METHOD (Decode_SkipsTheVersion30Bytes)
    {
        std::vector<Byte>  bytes (300, 0);
        DecodedFile        file;
        std::wstring       error;



        bytes[183] = 30;
        bytes.insert (bytes.end(), { 0x12, 0x34 });
        AppendText (bytes, { 'H', 'i' }, true);
        bytes.insert (bytes.end(), { 0xFF, 0xFF });

        AssertSucceeded (AppleWorksWp::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual ((size_t) 1, file.paragraphs.size());
        Assert::AreEqual (std::wstring (L"Hi"), file.paragraphs[0].runs[0].text);
    }
};



TEST_CLASS (AppleWorksDbTests)
{
public:

    TEST_METHOD (Decode_ListsCategoriesAndEntries)
    {
        std::vector<Byte>  bytes (357 + 2 * 22, 0);
        DecodedFile        file;
        std::wstring       error;



        bytes[0]   = (Byte) ((bytes.size() - 2) & 0xFF);
        bytes[1]   = (Byte) ((bytes.size() - 2) >> 8);
        bytes[35]  = 2;
        bytes[357] = 4;
        bytes[379] = 4;

        memcpy (&bytes[358], "Name", 4);
        memcpy (&bytes[380], "Born", 4);

        bytes.insert (bytes.end(), { 0x01, 0x00, 0xFF });
        bytes.insert (bytes.end(), { 0x0B, 0x00, 0x02, 'A', 'L', 0x06, 0xC0, '8', '4', 'B', '0', '9', 0xFF });
        bytes.insert (bytes.end(), { 0x06, 0x00, 0x81, 0x04, 0xD4, 'N', '0', '5' });
        bytes.insert (bytes.end(), { 0xFF, 0xFF });

        AssertSucceeded (AppleWorksDb::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue   (file.grid.columns == std::vector<std::wstring> { L"Name", L"Born" });
        Assert::AreEqual ((size_t) 2, file.grid.rows.size(), L"The standard values aren't an entry");
        Assert::AreEqual (std::wstring (L"AL"), file.grid.rows[0][0]);
        Assert::AreEqual (std::wstring (L"Feb 09 84"), file.grid.rows[0][1]);
        Assert::AreEqual (std::wstring (), file.grid.rows[1][0]);
        Assert::AreEqual (std::wstring (L"1:05 PM"), file.grid.rows[1][1]);
    }
};



TEST_CLASS (AppleWorksSsTests)
{
public:

    TEST_METHOD (Decode_LabelsValuesAndFormulaResults)
    {
        std::vector<Byte>  bytes (300, 0);
        std::vector<Byte>  row;
        double             value  = 1234.5;
        Byte               number[8];
        DecodedFile        file;
        std::wstring       error;



        memcpy (number, &value, sizeof (number));
        bytes[4]   = 5;
        bytes[139] = 6;

        row = { 0x00, 0x00, 0x03, 0x00, 0x04, 0x01, 'T', 'o', 'p' };
        row.push_back (0x0A);
        row.insert (row.end(), { 0xA3, 0x02 });
        row.insert (row.end(), number, number + 8);
        row.insert (row.end(), { 0x02, 0x21, '-', 0x0A, 0x80, 0xC0 });
        row.insert (row.end(), number, number + 8);
        row.push_back (0xFF);
        row[0] = (Byte) (row.size() - 2);
        bytes.insert (bytes.end(), row.begin(), row.end());
        bytes.insert (bytes.end(), { 0xFF, 0xFF });

        AssertSucceeded (AppleWorksSs::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual ((size_t) 5, file.grid.columns.size());
        Assert::AreEqual (std::wstring (L"D"), file.grid.columns[4]);
        Assert::IsTrue   (file.grid.rows[0] == std::vector<std::wstring> { L"3", L"Top", L"$1,234.50", L"-", L"NA" });
    }


    TEST_METHOD (Helpers_LettersAndFormats)
    {
        Assert::AreEqual (std::wstring (L"A"),  AppleWorksSs::GetColumnLetters (0));
        Assert::AreEqual (std::wstring (L"Z"),  AppleWorksSs::GetColumnLetters (25));
        Assert::AreEqual (std::wstring (L"AA"), AppleWorksSs::GetColumnLetters (26));
        Assert::AreEqual (std::wstring (L"DW"), AppleWorksSs::GetColumnLetters (126));

        Assert::AreEqual (std::wstring (L"-1,234,567.0"), AppleWorksSs::FormatNumber (-1234567.0, 4, 1));
        Assert::AreEqual (std::wstring (L"12.50%"),       AppleWorksSs::FormatNumber (0.125, 5, 2));
        Assert::AreEqual (std::wstring (L"0.1"),          AppleWorksSs::FormatNumber (0.1, 6, 0));
    }
};
