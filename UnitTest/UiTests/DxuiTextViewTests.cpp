#include "Pch.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewTests
//
//  Columns, the hanging indent of a wrapped last cell, character selection
//  across cells and rows, and scrolling. Cells are 8 by 16 pixels at 96 DPI,
//  inside a 6-pixel pad.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewTests)
{
public:

    static DxuiTextView::Row  MakeRow (std::vector<std::wstring> cells, bool warning = false)
    {
        DxuiTextView::Row  row;

        row.cells   = std::move (cells);
        row.warning = warning;

        return row;
    }


    static void  LayOut (DxuiTextView & view, LONG width, LONG height)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSize (8, 16);
        view.Layout (RECT { 0, 0, width, height }, scaler);
    }


    static void  Mouse (DxuiTextView & view, DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        view.OnMouse (ev);
    }


    TEST_METHOD (LastCell_WrapsAtASpaceUnderItsOwnFirstCharacter)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetRows ({ MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) });

        Assert::AreEqual (2, view.GetLineCount(), L"Sixteen columns less the number and its gap leave twelve, so the statement wraps once");

        //  The second line's fifth column is the first character after the
        //  number's column and gap, which is where the wrapped run begins.
        Assert::AreEqual (14, view.HitTest (POINT { 6 + 4 * 8, 6 + 16 + 4 }).offset,
            L"and the wrapped run starts with BBBB, under the P of PRINT");
    }


    //  A row's wrap indent starts each wrapped line that many characters past
    //  the last cell's first, as source code wraps under its operand.
    TEST_METHOD (WrapIndent_StartsAWrappedLinePastTheCellsFirstCharacter)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row = MakeRow ({ L"10", L"X LDA AAAA BBBB" });

        row.wrapIndent = 6;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetRows ({ row });

        Assert::AreEqual (2, view.GetLineCount());
        Assert::AreEqual (14, view.HitTest (POINT { 6 + 10 * 8, 6 + 16 + 4 }).offset, L"BBBB starts under the first A");
        Assert::AreEqual (15, view.HitTest (POINT { 6 + 11 * 8, 6 + 16 + 4 }).offset);
    }


    //  A span draws its characters in its own color, and the rest of the cell
    //  in the text color, each stretch of one color as one string.
    TEST_METHOD (Span_DrawsItsCharactersInItsColor)
    {
        DxuiTextView           view;
        DxuiTextView::Row      row     = MakeRow ({ L"10", L"LDA #1" });
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        bool                   colored = false;
        bool                   plain   = false;

        row.spans.push_back ({ 1, 0, 3, 0xFF112233 });

        LayOut (view, 400, 400);
        view.SetRows ({ row });
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            colored = colored || (call.text == L"LDA" && call.argb == 0xFF112233);
            plain   = plain   || (call.text == L" #1" && call.argb != 0xFF112233);
        }

        Assert::IsTrue (colored, L"The span's characters are in its color");
        Assert::IsTrue (plain,   L"and the rest of the cell in the text color");
    }


    TEST_METHOD (SelectionText_SpansCellsAndRows)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        view.Select (DxuiTextView::Position { 0, 2 }, DxuiTextView::Position { 1, 3 });

        Assert::AreEqual (std::wstring (L"one\r\nB\tt"), view.GetSelectionText(),
            L"A tab separates cells and a line break separates rows");
    }


    TEST_METHOD (Drag_SelectsFromTheCharacterPressedToTheOneReleased)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        Mouse (view, DxuiMouseEventKind::Down, 6 + 3 * 8, 6 + 4);
        Mouse (view, DxuiMouseEventKind::Move, 6 + 5 * 8, 6 + 16 + 4);
        Mouse (view, DxuiMouseEventKind::Up,   6 + 5 * 8, 6 + 16 + 4);

        Assert::AreEqual (std::wstring (L"one\r\nB\ttw"), view.GetSelectionText());
        Assert::IsFalse  (view.IsInteracting());
    }


    //  Proportional text is laid out once a paint has measured its
    //  characters, and a point is hit-tested by those measured places.
    TEST_METHOD (Proportional_LaysOutAndHitTestsByMeasuredPlaces)
    {
        DxuiTextView          view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        LayOut (view, 400, 400);
        view.SetProportional (true);
        view.SetRows ({ MakeRow ({ L"Format", L"WOZ 2" }), MakeRow ({ L"", L"second line" }) });

        Assert::AreEqual (0, view.GetLineCount(), L"Nothing is laid out until a paint measures the characters");

        view.Paint (painter, text, theme);

        Assert::AreEqual (2, view.GetLineCount());
        Assert::AreEqual (0,  view.HitTest (POINT { 0, 6 + 4 }).offset, L"The row's left edge is its first character");
        Assert::AreEqual (12, view.HitTest (POINT { 399, 6 + 4 }).offset, L"and past its end is the end of its text");

        view.SelectAll();
        Assert::AreEqual (std::wstring (L"Format\tWOZ 2\r\n\tsecond line"), view.GetSelectionText());
    }


    //  A bold run is measured bold, so the text after it starts where the
    //  bold run ends, not where the same characters end in the regular face.
    TEST_METHOD (Proportional_MeasuresABoldRunBold)
    {
        DxuiTextView          view;
        DxuiTextView::Row     row = MakeRow ({ L"XXXYY" });
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        row.spans.push_back ({ 0, 0, 3, 0, true });
        text.SetBoldGlyphWidth (9.0f);

        LayOut (view, 400, 400);
        view.SetProportional (true);
        view.SetRows ({ row });
        view.Paint (painter, text, theme);

        Assert::AreEqual (2, view.HitTest (POINT { 6 + 18, 6 + 4 }).offset, L"Two bold characters are 18 wide, not 14");
        Assert::AreEqual (4, view.HitTest (POINT { 6 + 27 + 7, 6 + 4 }).offset, L"and a regular one after them starts at 27");
    }


    //  A double-click selects a word, and a third click on the same spot
    //  selects everything.
    TEST_METHOD (TripleClick_SelectsEverything)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one two" }), MakeRow ({ L"B", L"three" }) });

        for (int click = 0; click < 2; click++)
        {
            Mouse (view, DxuiMouseEventKind::Down, 6 + 3 * 8, 6 + 4);
            Mouse (view, DxuiMouseEventKind::Up,   6 + 3 * 8, 6 + 4);
        }

        Assert::AreEqual (std::wstring (L"one"), view.GetSelectionText(), L"Two clicks select the word");

        Mouse (view, DxuiMouseEventKind::Down, 6 + 3 * 8, 6 + 4);
        Mouse (view, DxuiMouseEventKind::Up,   6 + 3 * 8, 6 + 4);

        Assert::AreEqual (std::wstring (L"A\tone two\r\nB\tthree"), view.GetSelectionText(), L"and a third selects everything");
    }


    TEST_METHOD (Commands_CopyNeedsASelection_SelectAllTakesEverything)
    {
        DxuiTextView  view;
        bool          enabled = true;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        Assert::IsTrue   (view.QueryCommand (DxuiStandardCommand::Copy, enabled));
        Assert::IsFalse  (enabled, L"Nothing selected, nothing to copy");

        Assert::IsTrue   (view.InvokeCommand (DxuiStandardCommand::SelectAll));
        Assert::AreEqual (std::wstring (L"A\tone\r\nB\ttwo"), view.GetSelectionText());
    }


    TEST_METHOD (Wheel_ScrollsWhenTheLinesOutnumberTheView)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;
        DxuiMouseEvent                  ev;

        for (int i = 0; i < 100; i++)
        {
            rows.push_back (MakeRow ({ std::to_wstring (i) }));
        }

        LayOut (view, 400, 12 + 16 * 10);
        view.SetRows (std::move (rows));

        Assert::IsTrue   (view.IsScrollbarVisible());

        ev.kind       = DxuiMouseEventKind::Wheel;
        ev.wheelDelta = -1.0f;
        view.OnMouse (ev);

        Assert::AreEqual (DxuiTextView::kWheelLines, view.GetTopLine());
    }


    TEST_METHOD (WarningRow_StartsItsTextAfterTheMark)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"Line 10: the file ends" }, true) });

        Assert::AreEqual (0, view.HitTest (POINT { 6 + DxuiTextView::kWarningCells * 8, 6 + 4 }).offset,
            L"The first character is past the warning mark");
    }


    TEST_METHOD (NoWrap_KeepsARowOnOneLine_AndScrollsSideways)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"SYSTEM.APPLE 32 3-Sep-85 Datafile" }) });

        Assert::AreEqual (1, view.GetLineCount());
        Assert::IsTrue   (view.IsHorzScrollbarVisible());

        view.ScrollColumns (7);

        Assert::AreEqual (7, view.GetLeftColumn());
        Assert::AreEqual (7, view.HitTest (POINT { 6, 6 + 4 }).offset, L"The character at the left edge is the one scrolled to");

        view.ScrollColumns (1000);

        Assert::AreEqual (33 - 16, view.GetLeftColumn(), L"The row's last character stops at the right edge");
    }


    TEST_METHOD (NoWrap_HasNoHorizontalBar_WhenRowsFit)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"SHORT" }) });

        view.ScrollColumns (3);

        Assert::IsFalse  (view.IsHorzScrollbarVisible());
        Assert::AreEqual (0, view.GetLeftColumn());
    }


    //  The horizontal bar takes a line's height, so a view whose rows just fit
    //  needs the vertical bar as well.
    TEST_METHOD (NoWrap_HorizontalBar_CanBringOnTheVerticalBar)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows (10, MakeRow ({ std::wstring (60, L'X') }));

        LayOut (view, 12 + 16 * 8, 12 + 16 * 10);
        view.SetNoWrap (true);
        view.SetRows (std::move (rows));

        Assert::IsTrue (view.IsHorzScrollbarVisible());
        Assert::IsTrue (view.IsScrollbarVisible());
    }


    TEST_METHOD (NoWrap_Off_WrapsTheRow_AndResetsTheScroll)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"PRINT AAAA BBBB CCCC DDDD" }) });
        view.ScrollColumns (5);
        view.SetNoWrap (false);

        Assert::AreEqual (2, view.GetLineCount());
        Assert::AreEqual (0, view.GetLeftColumn());
        Assert::IsFalse  (view.IsHorzScrollbarVisible());
    }


    TEST_METHOD (Wheel_WithShift_ScrollsSideways)
    {
        DxuiTextView    view;
        DxuiMouseEvent  ev;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ std::wstring (60, L'X') }) });

        ev.kind       = DxuiMouseEventKind::Wheel;
        ev.wheelDelta = -1.0f;
        ev.shift      = true;
        view.OnMouse (ev);

        Assert::AreEqual (DxuiTextView::kWheelColumns, view.GetLeftColumn());
    }
};
