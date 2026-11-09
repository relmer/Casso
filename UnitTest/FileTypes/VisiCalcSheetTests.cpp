#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/VisiCalcSheet.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheetTests
//
//  Sheets written as VisiCalc saves them, last entry first, in high ASCII,
//  then parsed and decoded with each formula as typed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VisiCalcSheetTests)
{
public:

    static std::vector<Byte> ToApple (const std::string & text)
    {
        std::vector<Byte>  bytes;



        for (char c : text)
        {
            bytes.push_back ((Byte) ((c == '\n') ? 0x8D : (c | 0x80)));
        }

        return bytes;
    }


    TEST_METHOD (Decode_EntriesAsTyped)
    {
        std::vector<Byte>  bytes = ToApple (">B3:/FR@SUM(A1...A2)\n>A3:/--\n>A2:2.5\n>A1:\"PRICE\n/W1\n/GOR\n/GC5\n/X>A1:>A1:\n");
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (VisiCalcSheet::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.grid.columns == std::vector<std::wstring> { L"Row", L"A", L"B" });
        Assert::AreEqual ((size_t) 3, file.grid.rows.size());
        Assert::IsTrue (file.grid.rows[0] == std::vector<std::wstring> { L"1", L"PRICE", L"" });
        Assert::IsTrue (file.grid.rows[1] == std::vector<std::wstring> { L"2", L"2.5", L"" });
        Assert::IsTrue (file.grid.rows[2] == std::vector<std::wstring> { L"3", L"-----", L"@SUM(A1...A2)" });
    }


    TEST_METHOD (Parse_GlobalsAndFormats)
    {
        VisiCalcSheet::Sheet  sheet;
        std::wstring          error;



        AssertSucceeded (VisiCalcSheet::Parse (ToApple (">BK254:/F$1\n/GOR\n/GF$\n/GC12\n"), sheet, error));

        Assert::IsFalse  (sheet.byColumns);
        Assert::AreEqual ('$', sheet.format);
        Assert::AreEqual ((size_t) 12, sheet.columnWidth);
        Assert::AreEqual ((size_t) 62, sheet.cells[0].column);
        Assert::AreEqual ((size_t) 254, sheet.cells[0].row);
        Assert::AreEqual ('$', sheet.cells[0].format);
    }


    TEST_METHOD (Parse_FailsALineThatIsNotAnEntry)
    {
        VisiCalcSheet::Sheet  sheet;
        std::wstring          error;
        HRESULT               hr = VisiCalcSheet::Parse (ToApple (">A1:5\nHELLO\n"), sheet, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::wstring (L"Line 2 isn't a VisiCalc entry."), error);
    }


    TEST_METHOD (ParseCoordinate_RangeOfColumnsAndRows)
    {
        size_t  column = 0;
        size_t  row    = 0;
        size_t  length = 0;



        Assert::IsTrue   (VisiCalcSheet::ParseCoordinate ("AB12:", column, row, length));
        Assert::AreEqual ((size_t) 27, column);
        Assert::AreEqual ((size_t) 12, row);
        Assert::AreEqual ((size_t) 4, length);

        Assert::IsFalse (VisiCalcSheet::ParseCoordinate ("BL1", column, row, length), L"Past column BK");
        Assert::IsFalse (VisiCalcSheet::ParseCoordinate ("A255", column, row, length), L"Past row 254");
        Assert::IsFalse (VisiCalcSheet::ParseCoordinate ("A0", column, row, length));
    }
};
