#include "Pch.h"

#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayoutTests
//
//  Text laid out with no view: columns, wrapping, the places of cells and
//  characters, and line arithmetic. Cells are 8 by 16 pixels, a DIP a pixel.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextLayoutTests)
{
public:

    static DxuiTextRow  MakeRow (std::vector<std::wstring> cells, bool warning = false)
    {
        DxuiTextRow  row;

        row.cells   = std::move (cells);
        row.warning = warning;

        return row;
    }


    static void  WrapAt (DxuiTextLayout & layout, std::vector<DxuiTextRow> rows, int columns)
    {
        layout.SetCellSize (8, 8.0f, 16);
        layout.SetRows (std::move (rows));
        layout.ClearLines();
        layout.Wrap ((float) (columns * 8), columns, 1.0f);
    }


    TEST_METHOD (Wrap_BreaksTheLastCellAfterTheLastSpaceThatFits)
    {
        DxuiTextLayout  layout;



        WrapAt (layout, { MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        Assert::AreEqual (2,  layout.GetLineCount(), L"twelve columns are left past the number and its gap");
        Assert::AreEqual (11, layout.GetLines()[0].length, L"the first line ends after the space following AAAA");
        Assert::AreEqual (11, layout.GetLines()[1].start);
        Assert::AreEqual (9,  layout.GetLines()[1].length);
        Assert::IsFalse (layout.GetLines()[1].first);
    }


    TEST_METHOD (Wrap_BreaksMidWordWhenNoSpaceFits)
    {
        DxuiTextLayout  layout;



        WrapAt (layout, { MakeRow ({ L"ABCDEFGHIJ" }) }, 4);

        Assert::AreEqual (3, layout.GetLineCount());
        Assert::AreEqual (4, layout.GetLines()[1].length);
        Assert::AreEqual (2, layout.GetLines()[2].length);
    }


    //  The indent narrows each wrapped line, so the rest of a long word
    //  takes two lines where it would fit on one without it.
    TEST_METHOD (WrapIndent_NarrowsAndShiftsTheWrappedLines)
    {
        DxuiTextLayout  layout;
        DxuiTextRow     row = MakeRow ({ L"10", L"X LDA AAAA BBBBBBBB" });



        row.wrapIndent = 6;

        WrapAt (layout, { row }, 16);

        Assert::AreEqual (3,  layout.GetLineCount(), L"six columns are left past the indent");
        Assert::AreEqual (6,  layout.GetLines()[1].length);
        Assert::AreEqual (4,  layout.GetLineColumn (row, layout.GetLines()[0]));
        Assert::AreEqual (10, layout.GetLineColumn (row, layout.GetLines()[1]), L"the cell's column and the indent");
    }


    //  A cell longer than its column's cap pushes the next cell of that row
    //  along, and no other row's.
    TEST_METHOD (ColumnCap_LimitsTheColumnAndALongerCellPushesItsRow)
    {
        DxuiTextLayout  layout;



        layout.SetColumnCap (0, 4);

        WrapAt (layout, { MakeRow ({ L"AB", L"x" }), MakeRow ({ L"ABCDEFGH", L"y" }) }, 40);

        Assert::AreEqual (6,  layout.GetCellStart (layout.GetRows()[0], 1), L"the capped column and the gap");
        Assert::AreEqual (10, layout.GetCellStart (layout.GetRows()[1], 1), L"the long cell and the gap");
    }


    TEST_METHOD (ColumnGap_SetsTheSpaceAfterAColumnUntilCleared)
    {
        DxuiTextLayout  layout;



        layout.SetColumnGap (0, 5);
        WrapAt (layout, { MakeRow ({ L"AB", L"x" }) }, 40);

        Assert::AreEqual (7, layout.GetCellStart (layout.GetRows()[0], 1));

        layout.SetColumnGap (0, 0);

        Assert::AreEqual (2 + DxuiTextLayout::kColumnGapCells, layout.GetCellStart (layout.GetRows()[0], 1));
    }


    TEST_METHOD (CellBase_CountsATabBetweenCells)
    {
        DxuiTextRow  row = MakeRow ({ L"AB", L"CDE", L"F" });



        Assert::AreEqual (3, DxuiTextLayout::GetCellBase (row, 1));
        Assert::AreEqual (7, DxuiTextLayout::GetCellBase (row, 2));
        Assert::AreEqual (8, DxuiTextLayout::GetRowLength (row));
        Assert::AreEqual (std::wstring (L"AB\tCDE\tF"), DxuiTextLayout::GetRowText (row));
    }


    TEST_METHOD (Warning_StartsTheFirstCellPastTheMark)
    {
        DxuiTextLayout  layout;



        WrapAt (layout, { MakeRow ({ L"text" }, true) }, 40);

        Assert::AreEqual (DxuiTextLayout::kWarningCells, layout.GetCellStart (layout.GetRows()[0], 0));
        Assert::AreEqual ((float) DxuiTextLayout::kWarningCells * 8.0f, layout.GetCellX (layout.GetRows()[0], 0));
    }


    //  A position in the wrapped part of the last cell falls on the line
    //  that part is drawn on.
    TEST_METHOD (LineOfPosition_IsTheLineThePositionIsDrawnOn)
    {
        DxuiTextLayout  layout;



        WrapAt (layout, { MakeRow ({ L"first" }), MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        Assert::AreEqual (1,  layout.GetFirstLineOfRow (1));
        Assert::AreEqual (1,  layout.GetLineOfPosition (DxuiTextPosition { 1, 3 + 10 }));
        Assert::AreEqual (2,  layout.GetLineOfPosition (DxuiTextPosition { 1, 3 + 11 }), L"BBBB starts the wrapped line");
        Assert::AreEqual (-1, layout.GetLineOfPosition (DxuiTextPosition { 5, 0 }), L"a row not laid out");
    }


    TEST_METHOD (NoWrap_KeepsTheLastCellOnOneLineAndCountsItsExtent)
    {
        DxuiTextLayout  layout;



        layout.SetNoWrap (true);
        WrapAt (layout, { MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        Assert::AreEqual (1,  layout.GetLineCount());
        Assert::AreEqual (24, layout.GetWidestColumns(), L"the cell's column and its twenty characters");
        Assert::AreEqual (4 + 6, layout.GetColumnOfPosition (DxuiTextPosition { 0, 3 + 6 }));
    }


    TEST_METHOD (FirstClippedColumn_LeavesItsCellsOutOfTheExtent)
    {
        DxuiTextLayout  layout;



        layout.SetNoWrap (true);
        layout.SetFirstClippedColumn (1);
        WrapAt (layout, { MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        Assert::AreEqual (2, layout.GetWidestColumns());
    }


    TEST_METHOD (Picture_TakesTheLinesOfItsHeight)
    {
        DxuiTextLayout  layout;
        DxuiTextRow     row     = MakeRow ({ L"" });
        auto            picture = std::make_shared<DxuiTextPicture>();



        picture->width  = 10;
        picture->height = 40;
        row.picture     = picture;

        WrapAt (layout, { row }, 40);

        Assert::AreEqual (3, layout.GetLineCount(), L"40 pixels of 16-pixel lines");
        Assert::IsTrue  (layout.GetLines()[0].first);
        Assert::IsFalse (layout.GetLines()[2].first);
    }


    TEST_METHOD (LineArithmetic_CountsFromTheTopLine)
    {
        DxuiTextLayout  layout;



        WrapAt (layout, { MakeRow ({ L"a" }), MakeRow ({ L"b" }), MakeRow ({ L"c" }), MakeRow ({ L"d" }), MakeRow ({ L"e" }) }, 40);

        Assert::AreEqual (2,   layout.GetLineCap (0, 40), L"two 16-pixel lines fit in 40");
        Assert::AreEqual (3,   layout.GetTopForLast (4, 40));
        Assert::AreEqual (32,  layout.GetLineOffsetPx (1, 3));
        Assert::AreEqual (-32, layout.GetLineOffsetPx (3, 1));
        Assert::AreEqual (80,  layout.GetLinesHeightPx());
    }


    //  Proportional text has no lines until it is measured, then wraps by
    //  the measured places: the mock renderer draws each character 7 pixels
    //  wide.
    TEST_METHOD (Proportional_WrapsByTheMeasuredPlaces)
    {
        DxuiTextLayout        layout;
        MockDxuiTextRenderer  text;
        DxuiFontHandle        font;



        layout.SetProportional (true);
        WrapAt (layout, { MakeRow ({ L"ABCDEFGHIJ" }) }, 5);

        Assert::AreEqual (0, layout.GetLineCount());

        font.face = L"Segoe UI";
        layout.Measure (text, font);
        layout.Wrap (35.0f, 5, 1.0f);

        Assert::IsTrue (layout.IsMeasured());
        Assert::AreEqual (21.0f, layout.GetCharsPx (0, 0, 0, 3));
        Assert::AreEqual (2,     layout.GetLineCount(), L"five 7-pixel characters fit in 35 pixels");
        Assert::AreEqual (5,     layout.GetLines()[1].start);
    }


    //  Rows of one column group line their cells up with each other only: a
    //  wide cell in one group doesn't move the columns of another.
    TEST_METHOD (ColumnGroups_LineUpApart)
    {
        DxuiTextLayout  layout;
        DxuiTextRow     legend = MakeRow ({ L"Water", L"Grass" });
        DxuiTextRow     wide   = MakeRow ({ L"A much wider cell", L"x" });

        legend.columnGroup = 1;

        WrapAt (layout, { legend, wide }, 80);

        Assert::AreEqual (5 + 2,  layout.GetCellStart (layout.GetRows()[0], 1), L"the legend's column is as wide as its own widest cell, and the gap");
        Assert::AreEqual (17 + 2, layout.GetCellStart (layout.GetRows()[1], 1), L"the other group's is as wide as its cell");
    }


    //  A table fits when each row's cells fit at their natural widths; a
    //  narrower width would wrap the last cell, so it doesn't fit.
    TEST_METHOD (DoColumnsFit_OnlyWhenNoCellWraps)
    {
        DxuiTextLayout        layout;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        layout.SetProportional (true);
        layout.SetCellSize (8, 8.0f, 16);
        layout.SetRows ({ MakeRow ({ L"Name", L"Weapon" }), MakeRow ({ L"IOLO", L"Dagger and sling" }) });

        Assert::IsFalse (layout.IsMeasured(), L"Nothing is measured until a paint");

        layout.Measure (text, theme.BodyFont());

        Assert::IsTrue  (layout.IsMeasured());
        Assert::IsTrue  (layout.DoColumnsFit (1984.0f), L"Wide enough");
        Assert::IsFalse (layout.DoColumnsFit (48.0f),   L"Too narrow: the last cell would wrap");
    }
};
