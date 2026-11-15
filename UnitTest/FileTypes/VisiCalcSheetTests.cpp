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


    //  A DOS 3.3 sheet's name is whatever the user typed, so a text file
    //  whose first line is an entry matches by its contents.
    TEST_METHOD (Matches_ADos33SheetByItsFirstEntryWhateverItsName)
    {
        FileFacts  facts;

        facts.volume = VolumeKind::Dos33;
        facts.type   = 0x00;
        facts.name   = "BREAK-EVEN POINT";

        Assert::IsTrue  (VisiCalcSheet::Matches (facts, ToApple (">B45:/F$(B29-B20)\n>A45:\"PROFIT =\n")));
        Assert::IsFalse (VisiCalcSheet::Matches (facts, ToApple ("> A QUOTED LINE\n")));
        Assert::IsFalse (VisiCalcSheet::Matches (facts, ToApple ("/X>A1:>A1:\n")), L"a setting alone needs the .VC name");

        facts.name = "SHEET.VC";
        Assert::IsTrue  (VisiCalcSheet::Matches (facts, ToApple ("/X>A1:>A1:\n")));
    }


    TEST_METHOD (Decode_EntriesAsTyped)
    {
        std::vector<Byte>  bytes = ToApple (">B3:/FR@SUM(A1...A2)\n>A3:/--\n>A2:2.5\n>A1:\"PRICE\n/W1\n/GOR\n/GC5\n/X>A1:>A1:\n");
        DecodeOptions      options;
        DecodedFile        file;
        std::wstring       error;



        options.formulas = true;

        AssertSucceeded (VisiCalcSheet::Decode (bytes, FileFacts(), options, file, error));

        Assert::IsTrue (file.grid.columns == std::vector<std::wstring> { L"Row", L"A", L"B" });
        Assert::AreEqual ((size_t) 3, file.grid.rows.size());
        Assert::IsTrue (file.grid.rows[0] == std::vector<std::wstring> { L"1", L"PRICE", L"" });
        Assert::IsTrue (file.grid.rows[1] == std::vector<std::wstring> { L"2", L"2.5", L"" });
        Assert::IsTrue (file.grid.rows[2] == std::vector<std::wstring> { L"3", L"-----", L"@SUM(A1...A2)" });
    }


    TEST_METHOD (Decode_ValuesAsDrawn)
    {
        std::vector<Byte>  bytes = ToApple (">B3:/F$@SUM(A1...A2)*2\n>A3:/--\n>A2:2.5\n>A1:\"PRICE LIST\n>B1:1/3\n>B2:/FI7/2\n/W1\n/GOC\n/GC7\n");
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (VisiCalcSheet::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual ((size_t) 3, file.grid.rows.size());
        Assert::IsTrue (file.grid.rows[0] == std::vector<std::wstring> { L"1", L"PRICE L", L".33333" });
        Assert::IsTrue (file.grid.rows[1] == std::vector<std::wstring> { L"2", L"2.5", L"4" });
        Assert::IsTrue (file.grid.rows[2] == std::vector<std::wstring> { L"3", L"-------", L"5.00" });
    }


    //  VisiCalc's 80-column version saves each column's width as a command
    //  at the column's first cell, after the cell's label. CiderPress II's
    //  sheets on the 1984 VisiCalc disk have it.
    TEST_METHOD (Decode_ACommandAtACellIsntItsContents)
    {
        std::vector<Byte>  bytes = ToApple (">B1:\"  WK1\n>A1:\" ITEM\n>A1:/GCC5\n>B1:/GCC5\n");
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (VisiCalcSheet::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.grid.rows[0][1].find (L"ITEM") != std::wstring::npos, file.grid.rows[0][1].c_str());
        Assert::IsTrue (file.grid.rows[0][2].find (L"WK1")  != std::wstring::npos, file.grid.rows[0][2].c_str());
    }


    TEST_METHOD (Draw_PadsToTheColumnByTheFormat)
    {
        VisiCalcSheet::Sheet  sheet;
        VisiCalcSheet::Cell   cell;
        VisiCalcSheet::Value  value;



        sheet.columnWidth = 6;
        value.kind        = VisiCalcSheet::Value::Kind::Number;
        value.number      = 12.5;

        Assert::AreEqual (std::wstring (L"  12.5"), VisiCalcSheet::Draw (sheet, cell, value));

        cell.format = 'L';
        Assert::AreEqual (std::wstring (L" 12.5 "), VisiCalcSheet::Draw (sheet, cell, value), L"a number leaves the column's first character blank");

        cell.format = '*';
        Assert::AreEqual (std::wstring (L"***** "), VisiCalcSheet::Draw (sheet, cell, value));

        cell.format = 'D';
        value.kind  = VisiCalcSheet::Value::Kind::Error;
        Assert::AreEqual (std::wstring (L" ERROR"), VisiCalcSheet::Draw (sheet, cell, value));

        value.kind = VisiCalcSheet::Value::Kind::NotAvailable;
        Assert::AreEqual (std::wstring (L"    NA"), VisiCalcSheet::Draw (sheet, cell, value));

        value.kind = VisiCalcSheet::Value::Kind::Boolean;
        Assert::AreEqual (std::wstring (L"  TRUE"), VisiCalcSheet::Draw (sheet, cell, value));

        cell.label    = true;
        cell.contents = "TOTAL SALES";
        cell.format   = 0;
        value.kind    = VisiCalcSheet::Value::Kind::Label;
        Assert::AreEqual (std::wstring (L"TOTAL "), VisiCalcSheet::Draw (sheet, cell, value));

        cell.contents = "TAX";
        cell.format   = 'R';
        Assert::AreEqual (std::wstring (L"   TAX"), VisiCalcSheet::Draw (sheet, cell, value));
    }


    //  Each as VisiCalc 2.08 draws it in the Home and Office Companion's
    //  sheets.
    TEST_METHOD (FormatNumber_MatchesVisiCalc)
    {
        Assert::AreEqual (std::string ("0.27"),     VisiCalcSheet::FormatNumber (0.27, '$', 8));
        Assert::AreEqual (std::string ("20.63"),    VisiCalcSheet::FormatNumber (20.625, '$', 8));
        Assert::AreEqual (std::string (".9291020"), VisiCalcSheet::FormatNumber (0.92910199, 'G', 8));
        Assert::AreEqual (std::string ("76.39917"), VisiCalcSheet::FormatNumber (76.399166666, 'G', 8));
        Assert::AreEqual (std::string ("44"),       VisiCalcSheet::FormatNumber (44.444, 'G', 2));
        Assert::AreEqual (std::string (".5"),       VisiCalcSheet::FormatNumber (0.5, 'G', 8));
    }


    TEST_METHOD (FormatNumber_FitsTheLimit)
    {
        Assert::AreEqual (std::string ("0"),        VisiCalcSheet::FormatNumber (0, 'G', 8));
        Assert::AreEqual (std::string ("123"),      VisiCalcSheet::FormatNumber (123, 'G', 8));
        Assert::AreEqual (std::string ("-.5"),      VisiCalcSheet::FormatNumber (-0.5, 'G', 8));
        Assert::AreEqual (std::string (".3333333"), VisiCalcSheet::FormatNumber (1.0 / 3, 'G', 8));
        Assert::AreEqual (std::string ("3.141593"), VisiCalcSheet::FormatNumber (3.14159265358979, 'G', 8));
        Assert::AreEqual (std::string ("12345679"), VisiCalcSheet::FormatNumber (12345678.9, 'G', 8));
        Assert::AreEqual (std::string ("1.2346E9"), VisiCalcSheet::FormatNumber (1234567890, 'G', 8));
        Assert::AreEqual (std::string ("1E-10"),    VisiCalcSheet::FormatNumber (1e-10, 'G', 8));
        Assert::AreEqual (std::string ("1E10"),     VisiCalcSheet::FormatNumber (9999999999.0, 'G', 4));
        Assert::AreEqual (std::string (">>"),       VisiCalcSheet::FormatNumber (1234567890, 'G', 2));
        Assert::AreEqual (std::string ("3"),        VisiCalcSheet::FormatNumber (2.5, 'I', 8));
        Assert::AreEqual (std::string ("-3"),       VisiCalcSheet::FormatNumber (-2.5, 'I', 8));
        Assert::AreEqual (std::string ("1234.50"),  VisiCalcSheet::FormatNumber (1234.5, '$', 8));
        Assert::AreEqual (std::string ("0.05"),     VisiCalcSheet::FormatNumber (0.049999, '$', 8));
        Assert::AreEqual (std::string (">>>>"),     VisiCalcSheet::FormatNumber (1234.5, '$', 4));
        Assert::AreEqual (std::string ("***"),      VisiCalcSheet::FormatNumber (3.9, '*', 8));
        Assert::AreEqual (std::string (""),         VisiCalcSheet::FormatNumber (-2, '*', 8));
        Assert::AreEqual (std::string ("********"), VisiCalcSheet::FormatNumber (40, '*', 8));
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
