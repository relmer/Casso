#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitterTests
//
//  Position clamping, drag, arrow-key steps, the resize cursor over the sash,
//  and the hover color. Bounds are 400x300 at 96 DPI, so pixels and DIPs
//  agree and the arithmetic reads directly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiSplitterTests)
{
public:

    static void  LayOut (DxuiSplitter & splitter, UINT dpi = 96)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (dpi);
        splitter.Layout (RECT { 0, 0, 400, 300 }, scaler);
    }


    static DxuiMouseEvent  MakeMouse (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };
        return ev;
    }


    static DxuiKeyEvent  MakeKey (WPARAM vk)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = vk;
        return ev;
    }


    TEST_METHOD (SetPosition_ClampsToBothLimits)
    {
        DxuiSplitter  splitter;

        LayOut (splitter);
        splitter.SetLimitsDip (50, 100);

        splitter.SetPositionDip (10);
        Assert::AreEqual (50, splitter.GetPositionDip());

        splitter.SetPositionDip (390);
        Assert::AreEqual (400 - DxuiSplitter::kSashDip - 100, splitter.GetPositionDip());
    }


    TEST_METHOD (SashRect_FollowsOrientation)
    {
        DxuiSplitter  splitter;
        RECT          sash = {};

        LayOut (splitter);
        splitter.SetPositionDip (120);

        sash = splitter.GetSashRect();
        Assert::AreEqual (120L, sash.left);
        Assert::AreEqual (124L, sash.right);
        Assert::AreEqual (300L, sash.bottom);

        splitter.SetOrientation (DxuiSplitter::Orientation::Horizontal);
        sash = splitter.GetSashRect();
        Assert::AreEqual (120L, sash.top);
        Assert::AreEqual (124L, sash.bottom);
        Assert::AreEqual (400L, sash.right);
    }


    TEST_METHOD (Drag_MovesAndReportsWithinLimits)
    {
        DxuiSplitter      splitter;
        std::vector<int>  moves;

        LayOut (splitter);
        splitter.SetLimitsDip (40, 40);
        splitter.SetPositionDip (100);
        splitter.SetOnMoved ([&moves] (int dip) { moves.push_back (dip); });

        Assert::IsTrue (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 101, 50)));
        Assert::IsTrue (splitter.IsDragging());

        Assert::IsTrue (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 201, 50)));
        Assert::AreEqual (200, splitter.GetPositionDip());

        Assert::IsTrue (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 399, 50)));
        Assert::AreEqual (400 - DxuiSplitter::kSashDip - 40, splitter.GetPositionDip());

        Assert::IsTrue  (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Up, 399, 50)));
        Assert::IsFalse (splitter.IsDragging());

        Assert::AreEqual ((size_t) 2, moves.size());
        Assert::AreEqual (200, moves[0]);
    }


    TEST_METHOD (PressOffSash_IsNotConsumed)
    {
        DxuiSplitter  splitter;

        LayOut (splitter);
        splitter.SetPositionDip (100);

        Assert::IsFalse (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 20, 50)));
        Assert::IsFalse (splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 30, 50)));
        Assert::IsFalse (splitter.IsDragging());
    }


    TEST_METHOD (ArrowKeys_StepByEightDips)
    {
        DxuiSplitter  splitter;
        int           reported = -1;

        LayOut (splitter);
        splitter.SetPositionDip (100);
        splitter.SetOnMoved ([&reported] (int dip) { reported = dip; });

        Assert::IsTrue (splitter.OnKey (MakeKey (VK_RIGHT)));
        Assert::AreEqual (108, splitter.GetPositionDip());
        Assert::AreEqual (108, reported);

        Assert::IsTrue  (splitter.OnKey (MakeKey (VK_LEFT)));
        Assert::AreEqual (100, splitter.GetPositionDip());
        Assert::IsFalse (splitter.OnKey (MakeKey (VK_DOWN)));

        splitter.SetOrientation (DxuiSplitter::Orientation::Horizontal);
        Assert::IsTrue (splitter.OnKey (MakeKey (VK_DOWN)));
        Assert::AreEqual (108, splitter.GetPositionDip());
    }


    TEST_METHOD (Cursor_IsResizeOnlyOverSash)
    {
        DxuiSplitter  splitter;

        LayOut (splitter);
        splitter.SetPositionDip (100);

        Assert::IsTrue (splitter.GetCursorForPoint (POINT { 102, 10 }) == IDC_SIZEWE);
        Assert::IsNull (splitter.GetCursorForPoint (POINT { 50, 10 }));

        splitter.SetOrientation (DxuiSplitter::Orientation::Horizontal);
        Assert::IsTrue (splitter.GetCursorForPoint (POINT { 10, 102 }) == IDC_SIZENS);
    }


    TEST_METHOD (Paint_UsesDividerThenHoverColor)
    {
        DxuiSplitter          splitter;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        LayOut (splitter);
        splitter.SetPositionDip (100);

        splitter.Paint (painter, text, theme);
        Assert::AreEqual (theme.Divider(), painter.Calls().back().argb);
        Assert::AreEqual (100.0f, painter.Calls().back().x);
        Assert::AreEqual ((float) DxuiSplitter::kSashDip, painter.Calls().back().width);

        splitter.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 101, 10));
        splitter.Paint (painter, text, theme);
        Assert::AreEqual (theme.HoverBackground(), painter.Calls().back().argb);
    }


    TEST_METHOD (HighDpi_SashIsScaled)
    {
        DxuiSplitter  splitter;
        RECT          sash = {};

        LayOut (splitter, 192);
        splitter.SetPositionDip (50);

        sash = splitter.GetSashRect();
        Assert::AreEqual (100L, sash.left);
        Assert::AreEqual (108L, sash.right);
    }
};
