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


    //  The view routes the wheel and a drag over a map picture to its
    //  pictures: the wheel zooms instead of scrolling, a drag pans instead
    //  of selecting, and the map's ping animates.
    TEST_METHOD (WheelAndDragOverAMap_GoToThePictures)
    {
        DxuiTextView  view;

        SetMapRows (view);
        Wheel (view, 6 + 50, 6 + 16 + 50, 4.0f);

        Assert::IsTrue   (view.GetPictures().GetMapZoom() > 1.0f);
        Assert::AreEqual (0, view.GetScroll().GetTopLine(), L"the wheel zooms the map rather than scroll the text");

        Mouse (view, DxuiMouseEventKind::Down, 6 + 50, 6 + 16 + 50);
        Mouse (view, DxuiMouseEventKind::Move, 6 + 40, 6 + 16 + 50);

        Assert::IsTrue  (view.GetPictures().IsPanning());
        Assert::IsFalse (view.GetSelection().HasSelection(), L"a drag on the map doesn't select text");

        Mouse (view, DxuiMouseEventKind::Up, 6 + 40, 6 + 16 + 50);

        Assert::IsFalse (view.GetPictures().IsPanning());
        Assert::IsTrue  (view.GetPictures().IsPinging());
    }


    //  A press and a drag on the text go to the selection.
    TEST_METHOD (Drag_GoesToTheSelection)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        Mouse (view, DxuiMouseEventKind::Down, 6 + 3 * 8, 6 + 4);
        Mouse (view, DxuiMouseEventKind::Move, 6 + 5 * 8, 6 + 16 + 4);
        Mouse (view, DxuiMouseEventKind::Up,   6 + 5 * 8, 6 + 16 + 4);

        Assert::AreEqual (std::wstring (L"one\r\nB\ttw"), view.GetSelection().GetSelectionText());
        Assert::IsFalse  (view.IsInteracting());
    }


    //  Proportional text is laid out once a paint has measured its
    //  characters.
    TEST_METHOD (Proportional_LaysOutOnceAPaintMeasures)
    {
        DxuiTextView          view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        LayOut (view, 400, 400);
        view.SetProportional (true);
        view.SetRows ({ MakeRow ({ L"Format", L"WOZ 2" }), MakeRow ({ L"", L"second line" }) });

        Assert::AreEqual (0, view.GetLayout().GetLineCount(), L"Nothing is laid out until a paint measures the characters");

        view.Paint (painter, text, theme);

        Assert::AreEqual (2, view.GetLayout().GetLineCount());
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
        Assert::AreEqual (std::wstring (L"A\tone\r\nB\ttwo"), view.GetSelection().GetSelectionText());
    }


    TEST_METHOD (NoWrap_KeepsARowOnOneLine_AndScrollsSideways)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"SYSTEM.APPLE 32 3-Sep-85 Datafile" }) });

        Assert::AreEqual (1, view.GetLayout().GetLineCount());
        Assert::IsTrue   (view.GetScroll().IsHorzScrollbarVisible());

        view.GetScroll().ScrollColumns (7);

        Assert::AreEqual (7, view.GetScroll().GetLeftColumn());

        view.GetScroll().ScrollColumns (1000);

        Assert::AreEqual (33 - 16, view.GetScroll().GetLeftColumn(), L"The row's last character stops at the right edge");
    }


    TEST_METHOD (NoWrap_HasNoHorizontalBar_WhenRowsFit)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"SHORT" }) });

        view.GetScroll().ScrollColumns (3);

        Assert::IsFalse  (view.GetScroll().IsHorzScrollbarVisible());
        Assert::AreEqual (0, view.GetScroll().GetLeftColumn());
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

        Assert::IsTrue (view.GetScroll().IsHorzScrollbarVisible());
        Assert::IsTrue (view.GetScroll().IsScrollbarVisible());
    }


    TEST_METHOD (NoWrap_Off_WrapsTheRow_AndResetsTheScroll)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetNoWrap (true);
        view.SetRows ({ MakeRow ({ L"PRINT AAAA BBBB CCCC DDDD" }) });
        view.GetScroll().ScrollColumns (5);
        view.SetNoWrap (false);

        Assert::AreEqual (2, view.GetLayout().GetLineCount());
        Assert::AreEqual (0, view.GetScroll().GetLeftColumn());
        Assert::IsFalse  (view.GetScroll().IsHorzScrollbarVisible());
    }
};
