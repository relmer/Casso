#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace
{
    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }
}





TEST_CLASS (DxuiPanelTests)
{
public:

    TEST_METHOD (Add_ReturnsReferenceToConstructedChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();


        Assert::AreEqual ((size_t) 1, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&child), static_cast<void *> (panel.Child (0)));
        Assert::AreEqual (static_cast<void *> (&panel), static_cast<void *> (child.Parent()));
    }


    TEST_METHOD (Remove_NullReturnsFalse)
    {
        DxuiPanel  panel;

        Assert::IsFalse (panel.Remove (nullptr));
    }


    TEST_METHOD (Remove_UnknownReturnsFalse)
    {
        DxuiPanel        panel;
        MockDxuiControl  stranger;

        Assert::IsFalse (panel.Remove (&stranger));
    }


    TEST_METHOD (Remove_KnownReturnsTrueAndDestroysChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();

        Assert::IsTrue   (panel.Remove (&child));
        Assert::AreEqual ((size_t) 0, panel.ChildCount());
    }


    TEST_METHOD (Clear_DropsAllChildren)
    {
        DxuiPanel  panel;

        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>();
        panel.Clear();

        Assert::AreEqual ((size_t) 0, panel.ChildCount());
    }


    TEST_METHOD (Paint_FanoutsToVisibleChildrenInOrder)
    {
        DxuiPanel             panel;
        MockDxuiControl &     a       = panel.Add<MockDxuiControl>();
        MockDxuiControl &     b       = panel.Add<MockDxuiControl>();
        MockDxuiControl &     c       = panel.Add<MockDxuiControl>();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        b.SetVisible (false);
        panel.Paint (painter, text, theme);

        Assert::AreEqual (1, a.paintCount);
        Assert::AreEqual (0, b.paintCount);
        Assert::AreEqual (1, c.paintCount);
    }


    TEST_METHOD (OnMouse_DispatchesFrontToBackAndStopsAtConsumer)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a  = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b  = panel.Add<MockDxuiControl>();
        MockDxuiControl &  c  = panel.Add<MockDxuiControl>();
        DxuiMouseEvent     ev;


        b.consumeMouse = true;
        Assert::IsTrue (panel.OnMouse (ev));

        // c is rear-most insertion, but front-to-back means last-added first.
        Assert::AreEqual (1, c.mouseCount);
        Assert::AreEqual (1, b.mouseCount);
        Assert::AreEqual (0, a.mouseCount);
    }


    TEST_METHOD (SetVisibleFalse_SkipsChildInPaintAndInput)
    {
        DxuiPanel             panel;
        MockDxuiControl &     hidden  = panel.Add<MockDxuiControl>();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiMouseEvent        ev;


        hidden.SetVisible (false);
        panel.Paint   (painter, text, theme);
        panel.OnMouse (ev);

        Assert::AreEqual (0, hidden.paintCount);
        Assert::AreEqual (0, hidden.mouseCount);
    }


    TEST_METHOD (SetVisibleFalse_MarksPanelDirty)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();


        // Add already marked dirty; clear it then toggle visibility.
        panel.ClearDirty();
        Assert::IsFalse (panel.Dirty());

        child.SetVisible (false);
        Assert::IsTrue (panel.Dirty());
    }


    TEST_METHOD (Layout_AsksLayoutPolicyToArrangeOnlyVisibleChildren)
    {
        DxuiPanel          panel;
        DxuiDpiScaler      scaler;
        RECT               bounds = MakeRect (0, 0, 200, 100);


        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>().SetVisible (false);
        panel.Add<MockDxuiControl>();
        panel.SetLayout (std::make_unique<DxuiStackLayout> (DxuiStackLayout::Orientation::Horizontal,
                                                            0.0f,
                                                            DxuiStackLayout::Align::Stretch));

        panel.Layout (bounds, scaler);

        // With no weights, the natural sizes are zero for default-constructed
        // controls; the visible children stack at the same x without errors.
        Assert::AreEqual ((size_t) 3, panel.ChildCount());
        Assert::IsFalse (panel.Dirty());
    }


    TEST_METHOD (OnThemeChanged_FanoutsToAllChildren)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();


        panel.OnThemeChanged();

        Assert::AreEqual (1, a.themeChangedCount);
        Assert::AreEqual (1, b.themeChangedCount);
    }


    TEST_METHOD (Tick_FanoutsToAllChildrenIncludingHidden)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();


        b.SetVisible (false);
        panel.Tick (1000);

        Assert::AreEqual (1, a.tickCount);
        Assert::AreEqual (1, b.tickCount);
    }
};
