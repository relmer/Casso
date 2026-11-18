#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpLayoutTests
//
//  The help dialog's two columns. Measurement is supplied by each test: a
//  description's height is its line count at the column width times a line
//  height, with a fixed number of characters per line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandLineHelpLayoutTests)
{
public:

    static constexpr int  kWidthPx      = 600;
    static constexpr int  kGapPx        = 24;
    static constexpr int  kPadPx        = 4;
    static constexpr int  kLinePx       = 20;
    static constexpr int  kCharPx       = 10;


    //  Lines of `chars` characters wrapped at `widthPx`, times the line height.
    static int MeasureWrapped (int chars, int widthPx)
    {
        int  perLine = std::max (1, widthPx / kCharPx);



        return ((chars + perLine - 1) / perLine) * kLinePx;
    }


    static CommandLineHelpLayout::Input MakeInput (std::vector<int> optionWidths, std::vector<int> descriptionChars)
    {
        CommandLineHelpLayout::Input  input;



        input.widthPx        = kWidthPx;
        input.columnGapPx    = kGapPx;
        input.rowPadPx       = kPadPx;
        input.lineHeightPx   = kLinePx;
        input.optionWidthsPx = optionWidths;

        input.measureOption = [] (size_t row, int widthPx)
        {
            UNREFERENCED_PARAMETER (row);
            UNREFERENCED_PARAMETER (widthPx);

            return kLinePx;
        };

        input.measureDescription = [descriptionChars] (size_t row, int widthPx)
        {
            return MeasureWrapped (descriptionChars[row], widthPx);
        };

        return input;
    }


    //  THE SWITCH COLUMN IS AS WIDE AS ITS WIDEST SWITCH, and the descriptions
    //  start one gap to its right.
    TEST_METHOD (OptionColumnFitsTheWidestSwitch)
    {
        CommandLineHelpTable  table = CommandLineHelpLayout::Compute (MakeInput ({ 90, 150, 120 }, { 10, 10, 10 }));



        Assert::AreEqual (150,                   table.optionWidthPx);
        Assert::AreEqual (150 + kGapPx,          table.descriptionLeftPx);
        Assert::AreEqual (kWidthPx - 150 - kGapPx, table.descriptionWidthPx);
    }


    //  A SWITCH TOO WIDE FOR ITS SHARE does not squeeze the descriptions to
    //  nothing: the column stops at its share of the table.
    TEST_METHOD (OptionColumnStopsAtItsShare)
    {
        CommandLineHelpTable  table = CommandLineHelpLayout::Compute (MakeInput ({ 500 }, { 10 }));
        int                   cap   = kWidthPx * CommandLineHelpLayout::kMaxOptionPercent / 100;



        Assert::AreEqual (cap, table.optionWidthPx);
        Assert::IsTrue   (table.descriptionWidthPx >= kWidthPx / 2, L"the description column is at least half the table");
    }


    //  EACH ROW IS AS TALL AS ITS WRAPPED DESCRIPTION, plus the padding, and
    //  the rows stack without overlap.
    TEST_METHOD (RowsTakeTheirWrappedHeightAndStack)
    {
        CommandLineHelpTable  table   = CommandLineHelpLayout::Compute (MakeInput ({ 100, 100 }, { 10, 120 }));
        int                   perLine = (kWidthPx - 100 - kGapPx) / kCharPx;
        int                   lines   = (120 + perLine - 1) / perLine;



        Assert::AreEqual ((size_t) 2, table.rows.size());
        Assert::AreEqual (0,                                table.rows[0].topPx);
        Assert::AreEqual (kLinePx + 2 * kPadPx,             table.rows[0].heightPx);
        Assert::AreEqual (table.rows[0].heightPx,           table.rows[1].topPx);
        Assert::IsTrue   (lines > 1, L"the second description wraps, or the case tests nothing");
        Assert::AreEqual (lines * kLinePx + 2 * kPadPx,     table.rows[1].heightPx);
        Assert::AreEqual (table.rows[1].topPx + table.rows[1].heightPx, table.heightPx);
    }


    //  A ROW IS NEVER SHORTER THAN ONE LINE, even when a measurement returns
    //  less, so an empty description cannot collapse its switch.
    TEST_METHOD (RowIsAtLeastOneLine)
    {
        CommandLineHelpLayout::Input  input = MakeInput ({ 100 }, { 0 });
        CommandLineHelpTable          table;



        input.measureOption = nullptr;

        table = CommandLineHelpLayout::Compute (input);

        Assert::AreEqual (kLinePx + 2 * kPadPx, table.rows[0].heightPx);
    }
};
