#include "Pch.h"
#include "FakeTextGeometry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScrollTests
//
//  Scrolling over lines laid out in 8 by 16 pixel cells, in a view whose
//  text is 6 pixels in from its edges.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextScrollTests)
{
public:

    //  `count` rows of one cell each, `width` characters wide, one line each.
    static void  Build (DxuiTextLayout & layout, int count, int width)
    {
        std::vector<DxuiTextLayout::Row>  rows ((size_t) count);

        for (DxuiTextLayout::Row & row : rows)
        {
            row.cells = { std::wstring ((size_t) width, L'X') };
        }

        layout.SetCellSize (8, 16);
        layout.SetNoWrap (true);
        layout.SetRows (std::move (rows));
        layout.BuildColumnCells();
        layout.BuildLines (1000, 1.0f);
    }


    static DxuiMouseEvent  MakeWheel (float notches, bool shift)
    {
        DxuiMouseEvent  ev;

        ev.kind       = DxuiMouseEventKind::Wheel;
        ev.wheelDelta = notches;
        ev.shift      = shift;

        return ev;
    }


    TEST_METHOD (Wheel_ScrollsWhenTheLinesOutnumberTheView)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (400, 12 + 16 * 10);
        DxuiTextScroll    scroll (layout, geometry);

        Build (layout, 100, 2);

        Assert::AreEqual (10, scroll.GetLineCap());
        Assert::IsTrue   (scroll.IsScrollbarVisible());

        scroll.OnWheel (MakeWheel (-1.0f, false));

        Assert::AreEqual (DxuiTextScroll::kWheelLines, scroll.GetTopLine());
    }


    TEST_METHOD (Wheel_WithShift_ScrollsSideways)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (12 + 16 * 8, 400);
        DxuiTextScroll    scroll (layout, geometry);

        Build (layout, 1, 60);
        scroll.SetHorzScrollbarVisible (true);

        scroll.OnWheel (MakeWheel (-1.0f, true));

        Assert::AreEqual (DxuiTextScroll::kWheelColumns, scroll.GetLeftColumn());
    }


    TEST_METHOD (ScrollColumns_StopsWithTheRowsEndAtTheRightEdge)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (12 + 16 * 8, 400);
        DxuiTextScroll    scroll (layout, geometry);

        Build (layout, 1, 33);
        scroll.SetHorzScrollbarVisible (true);

        scroll.ScrollColumns (7);
        Assert::AreEqual (7, scroll.GetLeftColumn());

        scroll.ScrollColumns (1000);
        Assert::AreEqual (33 - 16, scroll.GetLeftColumn());
    }


    TEST_METHOD (SetLeftColumn_StaysAtZero_WithoutTheHorizontalBar)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (12 + 16 * 8, 400);
        DxuiTextScroll    scroll (layout, geometry);

        Build (layout, 1, 33);

        scroll.ScrollColumns (3);

        Assert::AreEqual (0, scroll.GetLeftColumn());
    }


    //  The horizontal bar takes a line's height from the lines in sight.
    TEST_METHOD (HorizontalBar_TakesALineFromTheCap)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (400, 12 + 16 * 10);
        DxuiTextScroll    scroll (layout, geometry);

        Build (layout, 10, 2);

        Assert::IsFalse  (scroll.IsScrollbarVisible(), L"ten lines fit");

        scroll.SetHorzScrollbarVisible (true);

        Assert::AreEqual (9, scroll.GetLineCap());
        Assert::IsTrue   (scroll.IsScrollbarVisible(), L"and with the bar, they don't");
    }


    TEST_METHOD (Keys_EndScrollsTheLastLineIntoView_HomeToTheTop)
    {
        DxuiTextLayout    layout;
        FakeTextGeometry  geometry (400, 12 + 16 * 10);
        DxuiTextScroll    scroll (layout, geometry);
        DxuiKeyEvent      ev;

        Build (layout, 100, 2);

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_END;

        Assert::IsTrue   (scroll.OnKey (ev));
        Assert::AreEqual (90, scroll.GetTopLine());

        ev.vk = VK_HOME;

        Assert::IsTrue   (scroll.OnKey (ev));
        Assert::AreEqual (0, scroll.GetTopLine());

        ev.vk = VK_RIGHT;

        Assert::IsFalse  (scroll.OnKey (ev), L"with no horizontal bar, Right isn't a scroll");
    }
};
