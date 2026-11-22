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


    //  A picture row is as many lines as the picture is tall, and the picture
    //  is drawn once, a pixel a DIP.
    TEST_METHOD (Picture_TakesTheLinesOfItsHeightAndIsDrawnOnce)
    {
        DxuiTextView          view;
        DxuiTextView::Row     row     = MakeRow ({ L"" });
        auto                  picture = std::make_shared<DxuiTextView::Picture>();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   drawn   = 0;

        picture->bgra.assign (10 * 40, 0xFF336699);
        picture->width  = 10;
        picture->height = 40;
        row.picture     = picture;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"above" }), row, MakeRow ({ L"below" }) });

        Assert::AreEqual (5, view.GetLineCount(), L"a line above, three for 40 pixels of 16-pixel lines, and one below");

        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            drawn += (call.kind == RecordedTextKind::DrawFramebuffer) ? 1 : 0;
        }

        Assert::AreEqual (1,  drawn);
        Assert::AreEqual (10, text.GetLastFramebufferWidth());
    }


    //  A point on a picture is its row and the picture's pixel there; a
    //  point on a line of text isn't a picture.
    TEST_METHOD (HitTestPicture_FindsThePixelUnderThePoint)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row     = MakeRow ({ L"" });
        auto               picture = std::make_shared<DxuiTextView::Picture>();
        int                hitRow  = 0;
        int                x       = 0;
        int                y       = 0;

        picture->bgra.assign (10 * 40, 0xFF336699);
        picture->width  = 10;
        picture->height = 40;
        row.picture     = picture;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"above" }), row, MakeRow ({ L"below" }) });

        Assert::IsTrue   (view.HitTestPicture (POINT { 6 + 3, 6 + 16 + 5 }, hitRow, x, y), L"The picture starts on the second line");
        Assert::AreEqual (1, hitRow);
        Assert::AreEqual (3, x);
        Assert::AreEqual (5, y);
        Assert::IsFalse  (view.HitTestPicture (POINT { 6 + 3, 6 + 4 }, hitRow, x, y), L"The first line is text");
        Assert::IsFalse  (view.HitTestPicture (POINT { 6 + 30, 6 + 16 + 5 }, hitRow, x, y), L"Past the picture's right edge");
    }


    //  A zoomable picture with a ping, 100 pixels square, on the second line.
    static void  SetMapRows (DxuiTextView & view)
    {
        DxuiTextView::Row  row     = MakeRow ({ L"" });
        auto               picture = std::make_shared<DxuiTextView::Picture>();

        picture->bgra.assign (100 * 100, 0xFF336699);
        picture->width  = 100;
        picture->height = 100;
        picture->pingX  = 50;
        picture->pingY  = 50;
        picture->zoom   = true;
        row.picture     = picture;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"above" }), row });
    }


    static void  Wheel (DxuiTextView & view, int x, int y, float notches)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.positionDip = POINT { x, y };
        ev.wheelDelta  = notches;

        view.OnMouse (ev);
    }


    //  The wheel over a zoomable picture zooms it around the pointer: the
    //  pixel under the pointer stays put, and the others close in on it.
    TEST_METHOD (MapZoom_KeepsThePixelUnderThePointer)
    {
        DxuiTextView  view;
        int           hitRow = 0;
        int           x      = 0;
        int           y      = 0;

        SetMapRows (view);
        Wheel (view, 6 + 50, 6 + 16 + 50, 1.0f);

        Assert::AreEqual (1.25f, view.GetMapZoom());
        Assert::AreEqual (0, view.GetTopLine(), L"the wheel zooms the map rather than scroll the text");
        Assert::IsTrue   (view.HitTestPicture (POINT { 6 + 50, 6 + 16 + 50 }, hitRow, x, y));
        Assert::AreEqual (50, x);
        Assert::IsTrue   (view.HitTestPicture (POINT { 6 + 10, 6 + 16 + 10 }, hitRow, x, y));
        Assert::AreEqual (18, x, L"10 pixels left of the pointer at 1.25 times is 8 picture pixels left of 50, from an origin of 10");

        Wheel (view, 6 + 50, 6 + 16 + 50, -5.0f);

        Assert::AreEqual (1.0f, view.GetMapZoom(), L"no smaller than the whole picture");
    }


    //  A left drag on a zoomed picture pans it, and the picture stays in view.
    TEST_METHOD (MapPan_FollowsTheDragInsideThePicture)
    {
        DxuiTextView  view;

        SetMapRows (view);
        Wheel (view, 6 + 50, 6 + 16 + 50, 4.0f);
        Mouse (view, DxuiMouseEventKind::Down, 6 + 50, 6 + 16 + 50);
        Mouse (view, DxuiMouseEventKind::Move, 6 + 40, 6 + 16 + 50);

        Assert::IsTrue (view.GetMapOrigin().x > 0, L"dragged left, the view moves right across the picture");
        Assert::IsFalse (view.HasSelection(), L"a drag on the map doesn't select text");

        Mouse (view, DxuiMouseEventKind::Move, 6 + 400, 6 + 16 + 400);
        Mouse (view, DxuiMouseEventKind::Up,   6 + 400, 6 + 16 + 400);

        Assert::AreEqual (0L, view.GetMapOrigin().x, L"dragged far right and down, the view stops at the picture's left");
        Assert::AreEqual (0L, view.GetMapOrigin().y, L"and its top");
    }


    //  The ping animates for the first part of every period, with one more
    //  frame as it ends so its last ring is erased, and then rests.
    TEST_METHOD (MapPing_RunsThenRestsEachPeriod)
    {
        DxuiTextView  view;

        SetMapRows (view);

        Assert::IsTrue  (view.IsPinging());
        Assert::IsTrue  (view.TickPing (10000),        L"the ping starts on the first tick");
        Assert::IsTrue  (view.TickPing (10000 + 1000), L"its rings are spreading");
        Assert::IsTrue  (view.TickPing (10000 + 1600), L"one frame to erase the last ring");
        Assert::IsFalse (view.TickPing (10000 + 2000), L"at rest until the next period");
        Assert::IsTrue  (view.TickPing (10000 + 3100), L"the next ping");

        view.SetRows ({ MakeRow ({ L"text only" }) });

        Assert::IsFalse (view.IsPinging());
    }

    //  A table fits when each row's cells fit at their natural widths; a
    //  narrower view would wrap the last cell, so it doesn't fit.
    TEST_METHOD (DoColumnsFit_OnlyWhenNoCellWraps)
    {
        DxuiTextView          view;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        view.SetProportional (true);
        view.SetRows ({ MakeRow ({ L"Name", L"Weapon" }), MakeRow ({ L"IOLO", L"Dagger and sling" }) });

        LayOut (view, 2000, 400);
        view.Measure (text, theme);
        Assert::IsTrue  (view.DoColumnsFit(), L"Wide enough");

        LayOut (view, 60, 400);
        view.Measure (text, theme);
        Assert::IsFalse (view.DoColumnsFit(), L"Too narrow: the last cell would wrap");
    }


    //  A row's wrap indent starts each wrapped line that many characters past    //  the last cell's first, as source code wraps under its operand.
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


    //  An inverse span is filled with its color, and its characters are drawn
    //  in the background color over the fill.
    TEST_METHOD (Span_Inverse_FillsItsColorAndDrawsInTheBackground)
    {
        DxuiTextView           view;
        DxuiTextView::Row      row     = MakeRow ({ L"10", L"PRINT \"^DX\"" });
        DxuiTextView::Span     span;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        bool                   filled  = false;
        bool                   drawn   = false;

        span.cell    = 1;
        span.start   = 7;
        span.length  = 2;
        span.argb    = 0xFF112233;
        span.inverse = true;
        row.spans.push_back (span);

        LayOut (view, 400, 400);
        view.SetRows ({ row });
        view.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            filled = filled || (call.kind == RecordedPaintKind::FillRect && call.argb == 0xFF112233 && call.width == 16.0f && call.height == 16.0f);
        }

        for (const RecordedTextCall & call : text.Calls())
        {
            drawn = drawn || (call.text == L"^D" && call.argb == theme.ContentBackground());
        }

        Assert::IsTrue (filled, L"The span's two cells are filled with its color");
        Assert::IsTrue (drawn,  L"and its characters are drawn in the background color");
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
