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



    class RecordingSink : public IDxuiViewportInputSink
    {
    public:
        int             mouseCount  = 0;
        int             keyCount    = 0;
        DxuiMouseEvent  lastMouse   = {};
        DxuiKeyEvent    lastKey     = {};
        bool            consumeMouse = true;
        bool            consumeKey   = true;

        bool  OnViewportMouse (const DxuiMouseEvent & ev) override
        {
            mouseCount++;
            lastMouse = ev;
            return consumeMouse;
        }

        bool  OnViewportKey (const DxuiKeyEvent & ev) override
        {
            keyCount++;
            lastKey = ev;
            return consumeKey;
        }
    };
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


    TEST_METHOD (Defaults_PolicyFillNotConsumingNoSink)
    {
        DxuiViewport  vp;

        Assert::IsTrue   (vp.Policy() == DxuiViewport::SizePolicy::Fill);
        Assert::IsFalse  (vp.ConsumesInput());
        Assert::IsTrue   (vp.InputSink() == nullptr);
        Assert::AreEqual ((LONG) 0, vp.PreferredSizeDip().cx);
        Assert::AreEqual ((LONG) 0, vp.PreferredSizeDip().cy);
    }


    TEST_METHOD (SetConsumesInput_MakesFocusableAndBackAgain)
    {
        DxuiViewport  vp;

        Assert::IsFalse (vp.Focusable());

        vp.SetConsumesInput (true);
        Assert::IsTrue  (vp.ConsumesInput());
        Assert::IsTrue  (vp.Focusable());

        vp.SetConsumesInput (false);
        Assert::IsFalse (vp.ConsumesInput());
        Assert::IsFalse (vp.Focusable());
    }


    TEST_METHOD (SetPreferredSizeDip_RoundTrips)
    {
        DxuiViewport  vp;
        SIZE          pref = { 560, 384 };

        vp.SetPreferredSizeDip (pref);
        vp.SetSizePolicy       (DxuiViewport::SizePolicy::Fixed);

        Assert::AreEqual ((LONG) 560, vp.PreferredSizeDip().cx);
        Assert::AreEqual ((LONG) 384, vp.PreferredSizeDip().cy);
        Assert::IsTrue   (vp.Policy() == DxuiViewport::SizePolicy::Fixed);
    }


    TEST_METHOD (OnMouse_ConsumesInputTrueWithSink_ForwardsToSink)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiMouseEvent  ev;
        bool            consumed = false;

        ev.kind = DxuiMouseEventKind::Down;
        ev.positionDip = POINT{ 5, 7 };

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);
        consumed = vp.OnMouse (ev);

        Assert::IsTrue  (consumed);
        Assert::AreEqual (1, sink.mouseCount);
        Assert::AreEqual ((LONG) 5, sink.lastMouse.positionDip.x);
    }


    TEST_METHOD (OnMouse_ConsumesInputFalse_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiMouseEvent  ev;
        bool            consumed = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (false);
        consumed = vp.OnMouse (ev);

        Assert::IsFalse  (consumed);
        Assert::AreEqual (0, sink.mouseCount);
    }


    TEST_METHOD (OnMouse_ConsumesInputTrueButNoSink_DoesNotConsume)
    {
        DxuiViewport    vp;
        DxuiMouseEvent  ev;
        bool            consumed = true;

        vp.SetConsumesInput (true);
        consumed = vp.OnMouse (ev);

        Assert::IsFalse (consumed);
    }


    TEST_METHOD (OnKey_NonReservedKey_ForwardsToSink)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;
        bool            consumed = false;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = 'A';

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);
        consumed = vp.OnKey (ev);

        Assert::IsTrue   (consumed);
        Assert::AreEqual (1, sink.keyCount);
        Assert::AreEqual ((WPARAM) 'A', sink.lastKey.vk);
    }


    TEST_METHOD (OnKey_ReservedTab_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;
        bool            consumed = true;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_TAB;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);
        consumed = vp.OnKey (ev);

        Assert::IsFalse  (consumed);
        Assert::AreEqual (0, sink.keyCount);
    }


    TEST_METHOD (OnKey_ReservedShiftTab_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;
        bool            consumed = true;

        ev.kind  = DxuiKeyEventKind::Down;
        ev.vk    = VK_TAB;
        ev.shift = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);
        consumed = vp.OnKey (ev);

        Assert::IsFalse  (consumed);
        Assert::AreEqual (0, sink.keyCount);
    }


    TEST_METHOD (OnKey_ReservedEscape_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_ESCAPE;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsFalse  (vp.OnKey (ev));
        Assert::AreEqual (0, sink.keyCount);
    }


    TEST_METHOD (OnKey_ReservedAltAlone_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_MENU;
        ev.alt  = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsFalse  (vp.OnKey (ev));
        Assert::AreEqual (0, sink.keyCount);
    }


    TEST_METHOD (OnKey_ReservedF10_DoesNotForward)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_F10;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsFalse  (vp.OnKey (ev));
        Assert::AreEqual (0, sink.keyCount);
    }


    TEST_METHOD (OnKey_CtrlTab_NotReserved_ForwardsToSink)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_TAB;
        ev.ctrl = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsTrue   (vp.OnKey (ev));
        Assert::AreEqual (1, sink.keyCount);
    }


    TEST_METHOD (OnKey_AltF10_NotReserved_ForwardsToSink)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_F10;
        ev.alt  = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsTrue   (vp.OnKey (ev));
        Assert::AreEqual (1, sink.keyCount);
    }


    TEST_METHOD (OnKey_CtrlC_ForwardsToSink)
    {
        DxuiViewport    vp;
        RecordingSink   sink;
        DxuiKeyEvent    ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = 'C';
        ev.ctrl = true;

        vp.SetInputSink     (&sink);
        vp.SetConsumesInput (true);

        Assert::IsTrue   (vp.OnKey (ev));
        Assert::AreEqual (1, sink.keyCount);
    }


    TEST_METHOD (ClassifyHit_AlwaysClient)
    {
        DxuiViewport  vp;
        POINT         p = { 100, 200 };

        Assert::IsTrue (vp.ClassifyHit (p) == DxuiHitTestKind::Client);
    }
};
