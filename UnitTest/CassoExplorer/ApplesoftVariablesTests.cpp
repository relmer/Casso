#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/ApplesoftVariables.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariablesTests
//
//  A VAR file built from the layout BASIC.SYSTEM's STORE writes: the header,
//  the simple variables, the arrays, the strings after them, and the page
//  above HIMEM.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ApplesoftVariablesTests)
{
public:

    using Column = ApplesoftVariables::Column;

    //  Where the test file's copy of memory starts: its aux type.
    static constexpr Word  s_kOrigin = 0x95AE;



    //  A = 1, N% = -2, S$ = "HI", FN F, B%(2) and C(1,2), then the string,
    //  which ends at HIMEM, $9600, and the page above it.
    static std::vector<Byte> BuildFile()
    {
        std::vector<Byte>  simple =
        {
            0x41, 0x00,  0x81, 0x00, 0x00, 0x00, 0x00,
            0xCE, 0x80,  0xFF, 0xFE, 0x00, 0x00, 0x00,
            0x53, 0x80,  0x02, 0xFE, 0x95, 0x00, 0x00,
            0xC6, 0x00,  0x01, 0x08, 0x10, 0x90, 0xB0,
        };
        std::vector<Byte>  arrays =
        {
            0xC2, 0x80,  13, 0,  1,  0x00, 0x03,
            0x00, 0x01,  0x00, 0x02,  0x00, 0x03,

            0x43, 0x00,  39, 0,  2,  0x00, 0x03,  0x00, 0x02,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x81, 0x00, 0x00, 0x00, 0x00,
            0x82, 0x00, 0x00, 0x00, 0x00,
            0x82, 0x40, 0x00, 0x00, 0x00,
            0x83, 0x00, 0x00, 0x00, 0x00,
            0x83, 0x20, 0x00, 0x00, 0x00,
        };
        size_t             total = simple.size() + arrays.size();
        std::vector<Byte>  file  = { (Byte) total, (Byte) (total >> 8), (Byte) simple.size(), 0, 0x96 };

        file.insert (file.end(), simple.begin(), simple.end());
        file.insert (file.end(), arrays.begin(), arrays.end());
        file.insert (file.end(), { 0x48, 0x49 });
        file.resize (file.size() + 256);

        return file;
    }



    static std::vector<std::wstring> GetNames (const std::vector<VariableRow> & rows, const ApplesoftVariables::Section & section)
    {
        std::vector<std::wstring>  names;

        for (size_t index : section.rows)
        {
            names.push_back (rows[index].name);
        }

        return names;
    }



    TEST_METHOD (Decode_ListsEachVariableAndEveryArrayElement)
    {
        std::vector<Byte>           file = BuildFile();
        std::vector<VariableRow>    rows;
        std::vector<VariableArray>  arrays;
        std::vector<std::wstring>   names;
        std::vector<std::wstring>   expected = { L"A", L"N%", L"S$", L"FN F", L"B%(0)", L"B%(1)", L"B%(2)",
                                                 L"C(0,0)", L"C(1,0)", L"C(0,1)", L"C(1,1)", L"C(0,2)", L"C(1,2)" };

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        for (const VariableRow & row : rows)
        {
            names.push_back (row.name);
        }

        Assert::IsTrue (names == expected, L"The simple variables, then each array's elements, the first subscript fastest");

        Assert::AreEqual (std::wstring (L"Real"),     rows[0].type);
        Assert::AreEqual (std::wstring (L"1"),        rows[0].value);
        Assert::AreEqual (std::wstring (L"Integer"),  rows[1].type);
        Assert::AreEqual (std::wstring (L"-2"),       rows[1].value);
        Assert::AreEqual (std::wstring (L"String"),   rows[2].type);
        Assert::AreEqual (std::wstring (L"\"HI\""),   rows[2].value);
        Assert::AreEqual (std::wstring (L"2"),        rows[2].length, L"A string has its length");
        Assert::IsTrue   (rows[0].length.empty(),     L"and a number has none");
        Assert::AreEqual (std::wstring (L"Function"), rows[3].type);
        Assert::AreEqual (-1,                         rows[3].array);
        Assert::AreEqual (0,                          rows[4].array);
        Assert::AreEqual (std::wstring (L"5"),        rows[12].value);
        Assert::AreEqual (1,                          rows[12].array);

        Assert::AreEqual ((size_t) 2,                 arrays.size());
        Assert::AreEqual (std::wstring (L"B%"),       arrays[0].name);
        Assert::AreEqual (std::wstring (L"(2)"),      arrays[0].dimensions);
        Assert::AreEqual (std::wstring (L"(1,2)"),    arrays[1].dimensions);
        Assert::AreEqual ((size_t) 6,                 arrays[1].count);
    }



    //  A file copied without its aux type: the copy's start is worked out
    //  from HIMEM and the page past it.
    TEST_METHOD (Decode_WithoutAnAuxType_FindsItsStringsFromHimem)
    {
        std::vector<Byte>           file = BuildFile();
        std::vector<VariableRow>    rows;
        std::vector<VariableArray>  arrays;

        AssertSucceeded (ApplesoftVariables::Decode (file, 0, rows, arrays));

        Assert::AreEqual (std::wstring (L"\"HI\""), rows[2].value);
    }



    //  A string's address outside the file, as one in the program's text
    //  would be, has no characters in the file.
    TEST_METHOD (Decode_AStringNotInTheFile_SaysSo)
    {
        std::vector<Byte>           file = BuildFile();
        std::vector<VariableRow>    rows;
        std::vector<VariableArray>  arrays;

        file[5 + 14 + 4] = 0x08;

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        Assert::AreEqual (std::wstring (L"Not in the file"), rows[2].value);
        Assert::AreEqual (std::wstring (L"2"),               rows[2].length, L"Its length is in its entry");
    }



    TEST_METHOD (Decode_ABigArray_ListsEveryElement)
    {
        std::vector<Byte>           file   = { 0, 0, 0, 0, 0x96, 0xC4, 0x80, 0, 0, 1, 0x00, 150 };
        size_t                      length = 7 + 150 * 2;
        std::vector<VariableRow>    rows;
        std::vector<VariableArray>  arrays;

        file[7] = (Byte) length;
        file[8] = (Byte) (length >> 8);
        file.resize (5 + length);
        file[0] = (Byte) length;
        file[1] = (Byte) (length >> 8);

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        Assert::AreEqual ((size_t) 150, rows.size(), L"Every element");
        Assert::AreEqual (std::wstring (L"D%(149)"), rows.back().name);
    }



    TEST_METHOD (Decode_LengthsPastTheEndOfTheFile_AreInvalidData)
    {
        std::vector<Byte>           file = BuildFile();
        std::vector<VariableRow>    rows;
        std::vector<VariableArray>  arrays;

        file[1] = 0xFF;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        file = BuildFile();
        file[5 + 28 + 2] = 0xFF;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays), L"An array entry longer than the table");
    }



    //  Each section's name: a simple variable's, or an array's.
    static std::vector<std::wstring> GetSectionNames (const std::vector<VariableRow> & rows, const std::vector<VariableArray> & arrays, const std::vector<ApplesoftVariables::Section> & sections)
    {
        std::vector<std::wstring>  names;

        for (const ApplesoftVariables::Section & section : sections)
        {
            names.push_back ((section.array < 0) ? rows[section.rows.front()].name : arrays[(size_t) section.array].name);
        }

        return names;
    }



    TEST_METHOD (Arrange_SortsArraysAmongTheVariablesAndKeepsEachArraysElementsInOrder)
    {
        std::vector<Byte>                         file = BuildFile();
        std::vector<VariableRow>                  rows;
        std::vector<VariableArray>                arrays;
        std::vector<ApplesoftVariables::Section>  sections;

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        sections = ApplesoftVariables::Arrange (rows, arrays, L"", Column::Name, false);

        Assert::IsTrue (GetSectionNames (rows, arrays, sections) == std::vector<std::wstring> { L"A", L"B%", L"C", L"FN F", L"N%", L"S$" },
                        L"The arrays sort in among the simple variables");
        Assert::IsTrue (GetNames (rows, sections[1]) == std::vector<std::wstring> { L"B%(0)", L"B%(1)", L"B%(2)" }, L"An array's elements in subscript order");

        sections = ApplesoftVariables::Arrange (rows, arrays, L"", Column::Name, true);

        Assert::IsTrue (GetSectionNames (rows, arrays, sections) == std::vector<std::wstring> { L"S$", L"N%", L"FN F", L"C", L"B%", L"A" }, L"Descending");

        sections = ApplesoftVariables::Arrange (rows, arrays, L"", Column::Length, true);

        Assert::AreEqual (std::wstring (L"C"), GetSectionNames (rows, arrays, sections).front(), L"An array's length is its element count");

        sections = ApplesoftVariables::Arrange (rows, arrays, L"", Column::Value, true);

        for (const ApplesoftVariables::Section & section : sections)
        {
            if (section.array == 1)
            {
                Assert::AreEqual (std::wstring (L"C(1,2)"), rows[section.rows.front()].name, L"An array's elements sort on their values");
            }
        }
    }


    TEST_METHOD (Arrange_ByValue_PutsEmptyValuesFirstThenNumbersByValueThenStrings)
    {
        std::vector<Byte>                         file = BuildFile();
        std::vector<VariableRow>                  rows;
        std::vector<VariableArray>                arrays;
        std::vector<ApplesoftVariables::Section>  sections;

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        sections = ApplesoftVariables::Arrange (rows, arrays, L"", Column::Value, false);

        Assert::IsTrue (GetSectionNames (rows, arrays, sections) == std::vector<std::wstring> { L"B%", L"C", L"FN F", L"N%", L"A", L"S$" },
                        L"The arrays and the function have no value, -2 is less than 1, and the string is last");
    }



    TEST_METHOD (Arrange_FiltersOnNamesValuesAndArrays)
    {
        std::vector<Byte>                         file = BuildFile();
        std::vector<VariableRow>                  rows;
        std::vector<VariableArray>                arrays;
        std::vector<ApplesoftVariables::Section>  sections;

        AssertSucceeded (ApplesoftVariables::Decode (file, s_kOrigin, rows, arrays));

        sections = ApplesoftVariables::Arrange (rows, arrays, L"hi", Column::Name, false);

        Assert::AreEqual ((size_t) 1, sections.size());
        Assert::IsTrue   (GetNames (rows, sections[0]) == std::vector<std::wstring> { L"S$" }, L"A value matches, whatever its case");

        sections = ApplesoftVariables::Arrange (rows, arrays, L"C(1,2)", Column::Name, false);

        Assert::AreEqual ((size_t) 1,  sections.size());
        Assert::AreEqual ((size_t) 6, sections[0].rows.size(), L"An array's name matches all of its elements");

        sections = ApplesoftVariables::Arrange (rows, arrays, L"3", Column::Name, false);

        Assert::AreEqual ((size_t) 2, sections.size());
        Assert::IsTrue   (GetNames (rows, sections[0]) == std::vector<std::wstring> { L"B%(2)" }, L"and an element's value matches it alone");
        Assert::IsTrue   (GetNames (rows, sections[1]) == std::vector<std::wstring> { L"C(1,1)" });
    }



    TEST_METHOD (FormatReal_MatchesPrint)
    {
        std::vector<std::pair<std::vector<Byte>, std::wstring>>  cases =
        {
            { { 0x00, 0x00, 0x00, 0x00, 0x00 }, L"0" },
            { { 0x81, 0x00, 0x00, 0x00, 0x00 }, L"1" },
            { { 0x81, 0x80, 0x00, 0x00, 0x00 }, L"-1" },
            { { 0x80, 0x00, 0x00, 0x00, 0x00 }, L".5" },
            { { 0x82, 0xA0, 0x00, 0x00, 0x00 }, L"-2.5" },
            { { 0x82, 0x49, 0x0F, 0xDA, 0xA1 }, L"3.14159265" },
            { { 0x7A, 0x23, 0xD7, 0x0A, 0x3D }, L".01" },
            { { 0x77, 0x03, 0x12, 0x6E, 0x98 }, L"1E-03" },
            { { 0x9B, 0x6B, 0x79, 0xA2, 0xA0 }, L"123456789" },
            { { 0x9E, 0x6E, 0x6B, 0x28, 0x00 }, L"1E+09" },
        };

        for (const auto & [bytes, text] : cases)
        {
            Assert::AreEqual (text, ApplesoftVariables::FormatReal (bytes));
        }
    }



    TEST_METHOD (Render_AVarFile_IsAKnownPreviewOfItsVariables)
    {
        FileEntry          entry;
        FilePayload        payload;
        PreviewContent     content;
        MemoryBus          bus;

        entry.name    = "SAVED";
        entry.type    = ApplesoftVariables::kProDosType;
        entry.auxType = s_kOrigin;
        payload.bytes = BuildFile();

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue   (content.kind == PreviewContent::Kind::Variables);
        Assert::AreEqual (std::wstring (L"\"HI\""), content.variables[2].value);
        Assert::AreEqual ((size_t) 2, content.arrays.size());
        Assert::IsTrue   (ExplorerPreviewManager::IsKnownPreview (content));

        payload.bytes = { 0, 0, 0, 0, 0x96 };

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Text, L"A file with no variables says so");

        payload.bytes    = BuildFile();
        payload.bytes[1] = 0xFF;

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Error);
    }
};
