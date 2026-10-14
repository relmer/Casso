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


    //  A face of the host's own at a size rounded to whole multiples of its
    //  pixel grid, in the host's colors, with the selection in the host's.
    TEST_METHOD (Face_IsRoundedToItsGridAndDrawnInTheHostsColors)
    {
        DxuiTextView           view;
        DxuiDpiScaler          scaler;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        bool                   faced    = true;
        bool                   back     = false;
        bool                   plain    = false;
        bool                   inverse  = false;
        bool                   selFill  = false;

        scaler.SetDpi (120);
        view.SetCellSize (10, 16);
        view.Layout (RECT { 0, 0, 400, 200 }, scaler);
        view.SetRows ({ MakeRow ({ L"HELLO" }), MakeRow ({ L"WORLD" }) });
        view.SetFace (L"Casso Apple II", 14.0f, 8);
        view.SetColors (0xFF00FF00, 0xFF000000, 0xFF00FF00, 0xFF000000);
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString)
            {
                faced = faced && call.fontFamily == L"Casso Apple II" && call.fontSizeDip == 16.0f;
                plain = plain || call.argb == 0xFF00FF00;
            }
        }

        for (const RecordedPaintCall & call : painter.Calls())
        {
            back = back || (call.kind == RecordedPaintKind::FillRect && call.argb == 0xFF000000);
        }

        Assert::IsTrue (faced, L"14 DIP at 120 DPI is 17.5 pixels, rounded to 16, two to a dot");
        Assert::IsTrue (plain, L"The text is in the host's color");
        Assert::IsTrue (back,  L"on the host's background");

        text.Reset();
        painter.Reset();
        view.SelectAll();
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            inverse = inverse || (call.kind == RecordedTextKind::DrawString && call.text == L"HELLO" && call.argb == 0xFF000000);
        }

        for (const RecordedPaintCall & call : painter.Calls())
        {
            selFill = selFill || (call.kind == RecordedPaintKind::FillRect && call.argb == 0xFF00FF00);
        }

        Assert::IsTrue (inverse && selFill, L"A selection is the host's text over its fill");

        //  The zoom applies to the rounded size, so each step changes it.
        text.Reset();
        view.SetZoom (1.1f);
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString)
            {
                Assert::AreEqual (16.0f * 1.1f, call.fontSizeDip, 0.01f, L"110% of the rounded 16 pixels");
            }
        }
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
};
