#include "Pch.h"

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





TEST_CLASS (DxuiViewportTests)
{
public:

    TEST_METHOD (Defaults_LeafWithGenericViewportRole)
    {
        DxuiViewport  vp;

        Assert::IsTrue   (vp.Visible());
        Assert::IsTrue   (vp.Enabled());
        Assert::IsFalse  (vp.Focusable());
        Assert::AreEqual ((size_t) 0, vp.ChildCount());
        Assert::IsTrue   (vp.AccessibleRole() == DxuiAccessibleRole::Viewport);
    }


    TEST_METHOD (OnMouseAndOnKey_DoNotConsume)
    {
        DxuiViewport    vp;
        DxuiMouseEvent  mouse;
        DxuiKeyEvent    key;

        Assert::IsFalse (vp.OnMouse (mouse));
        Assert::IsFalse (vp.OnKey   (key));
    }


    TEST_METHOD (Paint_IsNoOp)
    {
        DxuiViewport          vp;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        vp.Paint (painter, text, theme);

        // The placeholder paints nothing; logs on the recording mocks remain empty.
        Assert::AreEqual ((size_t) 0, painter.Calls().size());
        Assert::AreEqual ((size_t) 0, text.Calls().size());
    }


    TEST_METHOD (Layout_StoresBoundsOnControlBase)
    {
        DxuiViewport   vp;
        DxuiDpiScaler  scaler;
        RECT           bounds = MakeRect (10, 20, 110, 80);
        RECT           stored = {};

        vp.Layout (bounds, scaler);
        stored = vp.Bounds();

        Assert::AreEqual ((LONG) 10, stored.left);
        Assert::AreEqual ((LONG) 20, stored.top);
        Assert::AreEqual ((LONG) 110, stored.right);
        Assert::AreEqual ((LONG) 80, stored.bottom);
    }


    TEST_METHOD (Layout_FiresCallbackOnFirstCall)
    {
        DxuiViewport   vp;
        DxuiDpiScaler  scaler;
        RECT           bounds   = MakeRect (0, 0, 200, 100);
        int            fireCount = 0;
        RECT           seen      = {};

        vp.SetOnBoundsChanged ([&] (const RECT & rc) { ++fireCount; seen = rc; });
        vp.Layout (bounds, scaler);

        Assert::AreEqual (1, fireCount);
        Assert::AreEqual ((LONG) 200, seen.right);
    }


    TEST_METHOD (Layout_DoesNotRefireWhenBoundsUnchanged)
    {
        DxuiViewport   vp;
        DxuiDpiScaler  scaler;
        RECT           bounds   = MakeRect (0, 0, 200, 100);
        int            fireCount = 0;

        vp.SetOnBoundsChanged ([&] (const RECT &) { ++fireCount; });
        vp.Layout (bounds, scaler);
        vp.Layout (bounds, scaler);
        vp.Layout (bounds, scaler);

        Assert::AreEqual (1, fireCount);
    }


    TEST_METHOD (Layout_RefiresWhenBoundsChange)
    {
        DxuiViewport   vp;
        DxuiDpiScaler  scaler;
        RECT           a         = MakeRect (0, 0, 200, 100);
        RECT           b         = MakeRect (0, 0, 300, 150);
        int            fireCount = 0;

        vp.SetOnBoundsChanged ([&] (const RECT &) { ++fireCount; });
        vp.Layout (a, scaler);
        vp.Layout (b, scaler);

        Assert::AreEqual (2, fireCount);
    }


    TEST_METHOD (Layout_NoCallbackRegistered_DoesNotCrash)
    {
        DxuiViewport   vp;
        DxuiDpiScaler  scaler;
        RECT           bounds = MakeRect (0, 0, 50, 50);

        vp.Layout (bounds, scaler);

        Assert::AreEqual ((LONG) 50, vp.Bounds().right);
    }
};
