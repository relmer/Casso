#include "Pch.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "FakeTextGeometry.h"
#include "FakeTextViewport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelectionTests
//
//  Selection by character over lines laid out in 8 by 16 pixel cells, in a
//  view whose text is 6 pixels in from its edges.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextSelectionTests)
{
public:

    using Position = DxuiTextSelection::Position;


    static DxuiTextLayout::Row  MakeRow (std::vector<std::wstring> cells, bool warning = false)
    {
        DxuiTextLayout::Row  row;

        row.cells   = std::move (cells);
        row.warning = warning;

        return row;
    }


    static void  Build (DxuiTextLayout & layout, std::vector<DxuiTextLayout::Row> rows, int columns)
    {
        layout.SetCellSize (8, 16);
        layout.SetRows (std::move (rows));
        layout.BuildColumnCells();
        layout.BuildLines (columns, 1.0f);
    }


    TEST_METHOD (HitTest_FindsAWrappedRunUnderItsCellsFirstCharacter)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (12 + 16 * 8, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) }, 16);

        //  The second line's fifth column is the first character after the
        //  number's column and gap, which is where the wrapped run begins.
        Assert::AreEqual (14, selection.HitTest (POINT { 6 + 4 * 8, 6 + 16 + 4 }).offset,
            L"the wrapped run starts with BBBB, under the P of PRINT");
    }


    TEST_METHOD (HitTest_FollowsTheWrapIndent)
    {
        DxuiTextLayout       layout;
        FakeTextGeometry     geometry (12 + 16 * 8, 400);
        FakeTextViewport     viewport;
        DxuiTextSelection    selection (layout, geometry, viewport);
        DxuiTextLayout::Row  row = MakeRow ({ L"10", L"X LDA AAAA BBBB" });

        row.wrapIndent = 6;

        Build (layout, { row }, 16);

        Assert::AreEqual (14, selection.HitTest (POINT { 6 + 10 * 8, 6 + 16 + 4 }).offset, L"BBBB starts under the first A");
        Assert::AreEqual (15, selection.HitTest (POINT { 6 + 11 * 8, 6 + 16 + 4 }).offset);
    }


    TEST_METHOD (HitTest_StartsAWarningsTextAfterTheMark)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"Line 10: the file ends" }, true) }, 48);

        Assert::AreEqual (0, selection.HitTest (POINT { 6 + DxuiTextLayout::kWarningCells * 8, 6 + 4 }).offset,
            L"The first character is past the warning mark");
    }


    TEST_METHOD (HitTest_CountsTheColumnsScrolledPast)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (12 + 16 * 8, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        layout.SetNoWrap (true);
        Build (layout, { MakeRow ({ L"SYSTEM.APPLE 32 3-Sep-85 Datafile" }) }, 16);
        viewport.leftColumn = 7;

        Assert::AreEqual (7, selection.HitTest (POINT { 6, 6 + 4 }).offset, L"The character at the left edge is the one scrolled to");
    }


    //  Proportional text is hit-tested by its measured places.
    TEST_METHOD (HitTest_Proportional_ByMeasuredPlaces)
    {
        DxuiTextLayout        layout;
        FakeTextGeometry      geometry (400, 400);
        FakeTextViewport      viewport;
        DxuiTextSelection     selection (layout, geometry, viewport);
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        layout.SetProportional (true);
        layout.SetCellSize (8, 16);
        layout.SetRows ({ MakeRow ({ L"Format", L"WOZ 2" }), MakeRow ({ L"", L"second line" }) });
        layout.MeasureCharacters (text, theme.BodyFont());
        layout.BuildProportionalLines (384.0f, 1.0f);

        Assert::AreEqual (0,  selection.HitTest (POINT { 0, 6 + 4 }).offset, L"The row's left edge is its first character");
        Assert::AreEqual (12, selection.HitTest (POINT { 399, 6 + 4 }).offset, L"and past its end is the end of its text");

        selection.SelectAll();
        Assert::AreEqual (std::wstring (L"Format\tWOZ 2\r\n\tsecond line"), selection.GetSelectionText());
    }


    //  A bold run is measured bold, so the text after it starts where the
    //  bold run ends, not where the same characters end in the regular face.
    TEST_METHOD (HitTest_Proportional_AfterABoldRun)
    {
        DxuiTextLayout        layout;
        FakeTextGeometry      geometry (400, 400);
        FakeTextViewport      viewport;
        DxuiTextSelection     selection (layout, geometry, viewport);
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiTextLayout::Row   row = MakeRow ({ L"XXXYY" });

        row.spans.push_back ({ 0, 0, 3, 0, true });
        text.SetBoldGlyphWidth (9.0f);

        layout.SetProportional (true);
        layout.SetCellSize (8, 16);
        layout.SetRows ({ row });
        layout.MeasureCharacters (text, theme.BodyFont());
        layout.BuildProportionalLines (384.0f, 1.0f);

        Assert::AreEqual (2, selection.HitTest (POINT { 6 + 18, 6 + 4 }).offset, L"Two bold characters are 18 wide, not 14");
        Assert::AreEqual (4, selection.HitTest (POINT { 6 + 27 + 7, 6 + 4 }).offset, L"and a regular one after them starts at 27");
    }


    TEST_METHOD (SelectionText_SpansCellsAndRows)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) }, 48);

        selection.Select (Position { 0, 2 }, Position { 1, 3 });

        Assert::AreEqual (std::wstring (L"one\r\nB\tt"), selection.GetSelectionText(),
            L"A tab separates cells and a line break separates rows");
    }


    TEST_METHOD (Drag_SelectsFromTheCharacterPressedToTheOneReleased)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) }, 48);

        selection.OnPress (POINT { 6 + 3 * 8, 6 + 4 }, false, 0);

        Assert::IsTrue  (selection.OnDrag (POINT { 6 + 5 * 8, 6 + 16 + 4 }));
        Assert::IsTrue  (selection.OnRelease());
        Assert::IsFalse (selection.IsDragging());
        Assert::AreEqual (std::wstring (L"one\r\nB\ttw"), selection.GetSelectionText());
    }


    //  A drag past the bottom of the view scrolls the text a line.
    TEST_METHOD (Drag_PastTheBottom_ScrollsALine)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) }, 48);

        selection.OnPress (POINT { 6, 6 + 4 }, false, 0);
        selection.OnDrag  (POINT { 6, 400 });

        Assert::AreEqual (1, viewport.linesScrolled);
        Assert::AreEqual (0, viewport.columnsScrolled);
    }


    //  A double-click selects a word, and a third click on the same spot
    //  selects everything.
    TEST_METHOD (TripleClick_SelectsEverything)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);
        POINT              word { 6 + 3 * 8, 6 + 4 };

        Build (layout, { MakeRow ({ L"A", L"one two" }), MakeRow ({ L"B", L"three" }) }, 48);

        for (int click = 0; click < 2; click++)
        {
            selection.OnPress (word, false, click);
            selection.OnRelease();
        }

        Assert::AreEqual (std::wstring (L"one"), selection.GetSelectionText(), L"Two clicks select the word");

        selection.OnPress (word, false, 2);
        selection.OnRelease();

        Assert::AreEqual (std::wstring (L"A\tone two\r\nB\tthree"), selection.GetSelectionText(), L"and a third selects everything");
    }


    TEST_METHOD (Reset_ClearsTheSelectionAndTheDrag)
    {
        DxuiTextLayout     layout;
        FakeTextGeometry   geometry (400, 400);
        FakeTextViewport   viewport;
        DxuiTextSelection  selection (layout, geometry, viewport);

        Build (layout, { MakeRow ({ L"A", L"one" }) }, 48);

        selection.OnPress (POINT { 6, 6 + 4 }, false, 0);
        selection.SelectAll();
        selection.Reset();

        Assert::IsFalse (selection.HasSelection());
        Assert::IsFalse (selection.IsDragging());
    }
};
