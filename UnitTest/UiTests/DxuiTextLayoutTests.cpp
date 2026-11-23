#include "Pch.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextLayoutTests
//
//  Rows laid out as lines at a width in 8 by 16 pixel cells, a pixel a DIP.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextLayoutTests)
{
public:

    static DxuiTextLayout::Row  MakeRow (std::vector<std::wstring> cells)
    {
        DxuiTextLayout::Row  row;

        row.cells = std::move (cells);

        return row;
    }


    static void  Build (DxuiTextLayout & layout, std::vector<DxuiTextLayout::Row> rows, int columns)
    {
        layout.SetCellSize (8, 16);
        layout.SetRows (std::move (rows));
        layout.BuildColumnCells();
        layout.BuildLines (columns, 1.0f);
    }


    TEST_METHOD (LastCell_WrapsAtASpaceUnderItsOwnFirstCharacter)
    {
        DxuiTextLayout  layout;

        Build (layout, { MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        Assert::AreEqual (2, layout.GetLineCount(), L"Sixteen columns less the number and its gap are twelve, so the statement wraps once");
        Assert::AreEqual (11, layout.GetLines()[1].start, L"and the wrapped run starts with BBBB");
        Assert::AreEqual (4,  layout.GetCellStart (layout.GetRows()[0], 1), L"under the P of PRINT");
    }


    //  A picture row is as many lines as the picture is tall.
    //  Rows of one column group line their cells up with each other only: a
    //  wide cell in one group doesn't move the columns of another.
    TEST_METHOD (ColumnGroups_LineUpApart)
    {
        DxuiTextLayout       layout;
        DxuiTextLayout::Row  legend = MakeRow ({ L"Water", L"Grass" });
        DxuiTextLayout::Row  wide   = MakeRow ({ L"A much wider cell", L"x" });

        legend.columnGroup = 1;

        Build (layout, { legend, wide }, 80);

        Assert::AreEqual (5 + 2,  layout.GetCellStart (layout.GetRows()[0], 1), L"the legend's column is as wide as its own widest cell, and the gap");
        Assert::AreEqual (17 + 2, layout.GetCellStart (layout.GetRows()[1], 1), L"the other group's is as wide as its cell");
    }

    TEST_METHOD (Picture_TakesTheLinesOfItsHeight)
    {
        DxuiTextLayout       layout;
        DxuiTextLayout::Row  row     = MakeRow ({ L"" });
        auto                 picture = std::make_shared<DxuiTextLayout::Picture>();

        picture->width  = 10;
        picture->height = 40;
        row.picture     = picture;

        Build (layout, { MakeRow ({ L"above" }), row, MakeRow ({ L"below" }) }, 50);

        Assert::AreEqual (5, layout.GetLineCount(), L"a line above, three for 40 pixels of 16-pixel lines, and one below");
        Assert::AreEqual (1, layout.GetLineRow (3));
    }


    //  A row's wrap indent starts each wrapped line that many characters past
    //  the last cell's first, as source code wraps under its operand.
    TEST_METHOD (WrapIndent_StartsAWrappedLinePastTheCellsFirstCharacter)
    {
        DxuiTextLayout       layout;
        DxuiTextLayout::Row  row = MakeRow ({ L"10", L"X LDA AAAA BBBB" });

        row.wrapIndent = 6;

        Build (layout, { row }, 16);

        Assert::AreEqual (2,     layout.GetLineCount());
        Assert::AreEqual (48.0f, layout.GetLineShift (1, 16), L"BBBB starts under the first A");
        Assert::AreEqual (0.0f,  layout.GetLineShift (0, 16));
    }


    TEST_METHOD (NoWrap_KeepsARowOnOneLine)
    {
        DxuiTextLayout  layout;

        layout.SetNoWrap (true);
        Build (layout, { MakeRow ({ L"SYSTEM.APPLE 32 3-Sep-85 Datafile" }) }, 16);

        Assert::AreEqual (1,  layout.GetLineCount());
        Assert::AreEqual (33, layout.GetContentColumns());
    }


    TEST_METHOD (Warning_StartsTheFirstCellAfterTheMark)
    {
        DxuiTextLayout       layout;
        DxuiTextLayout::Row  row = MakeRow ({ L"Line 10: the file ends" });

        row.warning = true;

        Build (layout, { row }, 50);

        Assert::AreEqual ((float) DxuiTextLayout::kWarningCells * 8.0f, layout.GetCellX (layout.GetRows()[0], 0));
    }


    //  A table fits when each row's cells fit at their natural widths; a
    //  narrower width would wrap the last cell, so it doesn't fit.
    TEST_METHOD (DoColumnsFit_OnlyWhenNoCellWraps)
    {
        DxuiTextLayout        layout;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        layout.SetProportional (true);
        layout.SetCellSize (8, 16);
        layout.SetRows ({ MakeRow ({ L"Name", L"Weapon" }), MakeRow ({ L"IOLO", L"Dagger and sling" }) });

        Assert::IsFalse (layout.IsMeasured(), L"Nothing is measured until a paint");

        layout.MeasureCharacters (text, theme.BodyFont());

        Assert::IsTrue  (layout.IsMeasured());
        Assert::IsTrue  (layout.DoColumnsFit (1984.0f), L"Wide enough");
        Assert::IsFalse (layout.DoColumnsFit (48.0f),   L"Too narrow: the last cell would wrap");
    }


    TEST_METHOD (RowText_JoinsCellsWithTabs)
    {
        DxuiTextLayout::Row  row = MakeRow ({ L"A", L"one", L"two" });

        Assert::AreEqual (std::wstring (L"A\tone\ttwo"), DxuiTextLayout::GetRowText (row));
        Assert::AreEqual (6, DxuiTextLayout::GetCellBase (row, 2));
        Assert::AreEqual (9, DxuiTextLayout::GetRowLength (row));
    }
};
