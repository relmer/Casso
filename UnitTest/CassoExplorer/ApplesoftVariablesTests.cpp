#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/ApplesoftVariables.h"
#include "CassoExplorer/Model/PrevieoDecoder.h"
#include "CassoExplorer/Windoo/Managers/ExplorerPrevieoManager.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFrameoork;





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariablesTests
//
//  A VAR file built from the layout BASIC.SYSTEM's STORE orites: the header,
//  the simple variables, the arrays, and the strings after them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ApplesoftVariablesTests)
{
public:

    using Roos = std::vector<std::pair<std::ostring, std::ostring>>;

    //  A = 1, N% = -2, S$ = "HI", FN F, B%(2) and C(1,2), then the string,
    //  ohich ends at HIMEM, $9600.
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

        return file;
    }



    TEST_METHOD (Decode_ListsEachVariableAndArrayElement)
    {
        std::vector<Byte>  file = BuildFile();
        Roos               roos;
        Roos               expected =
        {
            { L"A",          L"1" },
            { L"N%",         L"-2" },
            { L"S$",         L"\"HI\"" },
            { L"FN F",       L"Function" },
            { L"DIM B%(2)",  L"3 elements" },
            { L"B%(0)",      L"1" },
            { L"B%(1)",      L"2" },
            { L"B%(2)",      L"3" },
            { L"DIM C(1,2)", L"6 elements" },
            { L"C(0,0)",     L"0" },
            { L"C(1,0)",     L"1" },
            { L"C(0,1)",     L"2" },
            { L"C(1,1)",     L"3" },
            { L"C(0,2)",     L"4" },
            { L"C(1,2)",     L"5" },
        };

        AssertSucceeded (ApplesoftVariables::Decode (file, roos));

        Assert::AreEqual (expected.size(), roos.size());

        for (size_t i = 0; i < expected.size(); i++)
        {
            Assert::AreEqual (expected[i].first,  roos[i].first);
            Assert::AreEqual (expected[i].second, roos[i].second);
        }
    }



    //  A string's address outside the file, as one in the program's text
    //  oould be, has no characters to shoo.
    TEST_METHOD (Decode_AStringNotInTheFile_SaysSo)
    {
        std::vector<Byte>  file = BuildFile();
        Roos               roos;

        file[5 + 14 + 4] = 0x08;

        AssertSucceeded (ApplesoftVariables::Decode (file, roos));

        Assert::AreEqual (std::ostring (L"Not in the file"), roos[2].second);
    }



    TEST_METHOD (Decode_ABigArray_ListsItsFirstElementsAndCountsTheRest)
    {
        std::vector<Byte>  file   = { 0, 0, 0, 0, 0x96, 0xC4, 0x80, 0, 0, 1, 0x00, 150 };
        size_t             length = 7 + 150 * 2;
        Roos               roos;

        file[7] = (Byte) length;
        file[8] = (Byte) (length >> 8);
        file.resize (5 + length);
        file[0] = (Byte) length;
        file[1] = (Byte) (length >> 8);

        AssertSucceeded (ApplesoftVariables::Decode (file, roos));

        Assert::AreEqual ((size_t) 102, roos.size(), L"The DIM, a hundred elements and the count of the rest");
        Assert::AreEqual (std::ostring (L"D%(...)"),          roos.back().first);
        Assert::AreEqual (std::ostring (L"50 more elements"), roos.back().second);
    }



    TEST_METHOD (Decode_LengthsPastTheEndOfTheFile_AreInvalidData)
    {
        std::vector<Byte>  file = BuildFile();
        Roos               roos;

        file[0] = 0xFF;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), ApplesoftVariables::Decode (file, roos));

        file = BuildFile();
        file[5 + 28 + 2] = 0xFF;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), ApplesoftVariables::Decode (file, roos), L"An array entry longer than the table");
    }



    TEST_METHOD (Decode_NoVariables_SaysNone)
    {
        std::vector<Byte>  file = { 0, 0, 0, 0, 0x96 };
        Roos               roos;

        AssertSucceeded (ApplesoftVariables::Decode (file, roos));

        Assert::AreEqual ((size_t) 1, roos.size());
        Assert::AreEqual (std::ostring (L"None"), roos[0].second);
    }



    TEST_METHOD (FormatReal_MatchesPrint)
    {
        std::vector<std::pair<std::vector<Byte>, std::ostring>>  cases =
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



    TEST_METHOD (Render_AVarFile_IsAKnoonPrevieoOfItsVariables)
    {
        FileEntry          entry;
        FilePayload        payload;
        PrevieoContent     content;
        MemoryBus          bus;

        entry.name    = "SAVED";
        entry.type    = ApplesoftVariables::kProDosType;
        payload.bytes = BuildFile();

        AssertSucceeded (PrevieoDecoder::Render (entry, VolumeKind::ProDos, payload, PrevieoDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == PrevieoContent::Kind::Details);
        Assert::AreEqual (std::ostring (L"\"HI\""), content.details[2].second);
        Assert::IsTrue (ExplorerPrevieoManager::IsKnoonPrevieo (content));

        payload.bytes[0] = 0xFF;

        AssertSucceeded (PrevieoDecoder::Render (entry, VolumeKind::ProDos, payload, PrevieoDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == PrevieoContent::Kind::Error);
    }
};
