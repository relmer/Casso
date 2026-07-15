#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoomTests
//
//  The pan/zoom controller is pure logic, so every interaction is exercised
//  headless: feed synthetic DxuiMouseEvent / DxuiKeyEvent, Tick to ease, read
//  the transform. The touchpad-lag regression (sub-notch wheel deltas must
//  accumulate, not truncate away) has its own test below.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    DxuiMouseEvent  Wheel (float delta, bool horizontal = false, bool ctrl = false)
    {
        DxuiMouseEvent  ev;
        ev.kind            = DxuiMouseEventKind::Wheel;
        ev.wheelDelta      = delta;
        ev.wheelHorizontal = horizontal;
        ev.ctrl            = ctrl;
        return ev;
    }



    DxuiMouseEvent  Mouse (DxuiMouseEventKind kind, DxuiMouseButton button, LONG x, LONG y)
    {
        DxuiMouseEvent  ev;
        ev.kind        = kind;
        ev.button      = button;
        ev.positionDip = POINT { x, y };
        return ev;
    }



    DxuiKeyEvent  CtrlKey (WPARAM vk)
    {
        DxuiKeyEvent  ev;
        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = vk;
        ev.ctrl = true;
        return ev;
    }



    // Ease long enough (large dt) that current reaches target for assertions
    // about the settled transform.
    void  Settle (DxuiPanZoom & pz)
    {
        pz.Tick (0.0);
        pz.Tick (10.0);
    }
}




TEST_CLASS (DxuiPanZoomTests)
{
public:

    TEST_METHOD (Defaults_ZoomMinNoPan)
    {
        DxuiPanZoom  pz;

        Assert::AreEqual (1.0f, pz.Zoom (),  0.0001f);
        Assert::AreEqual (0.0f, pz.PanX (),  0.0001f);
        Assert::AreEqual (0.0f, pz.PanY (),  0.0001f);
        Assert::IsFalse  (pz.Zoomed ());
    }



    TEST_METHOD (CtrlWheelUp_ZoomsInAndClampsAtMax)
    {
        DxuiPanZoom  pz;

        pz.OnMouse (Wheel (+1.0f, /*horizontal*/ false, /*ctrl*/ true));
        Assert::AreEqual (1.25f, pz.ZoomTarget (), 0.0001f);
        Assert::IsTrue   (pz.Zoomed ());

        for (int i = 0; i < 20; i++)
        {
            pz.OnMouse (Wheel (+1.0f, false, true));
        }
        Assert::AreEqual (4.0f, pz.ZoomTarget (), 0.0001f);   // clamped at zoomMax
    }



    TEST_METHOD (CtrlWheelDown_ZoomsOutAndClampsAtMin)
    {
        DxuiPanZoom  pz;

        for (int i = 0; i < 5; i++)
        {
            pz.OnMouse (Wheel (+1.0f, false, true));
        }
        Assert::IsTrue (pz.ZoomTarget () > 1.0f);

        for (int i = 0; i < 20; i++)
        {
            pz.OnMouse (Wheel (-1.0f, false, true));
        }
        Assert::AreEqual (1.0f, pz.ZoomTarget (), 0.0001f);   // clamped at zoomMin
    }



    TEST_METHOD (ResetZoom_ReturnsToMin)
    {
        DxuiPanZoom  pz;

        pz.OnMouse (Wheel (+1.0f, false, true));
        pz.OnMouse (Wheel (+1.0f, false, true));
        Assert::IsTrue (pz.ZoomTarget () > 1.0f);

        pz.ResetZoom ();
        Assert::AreEqual (1.0f, pz.ZoomTarget (), 0.0001f);
    }



    TEST_METHOD (VerticalWheel_MovesPanYOppositeToDelta)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanY = 96.0f;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        // Wheel up (+delta) reveals earlier content -> panY target decreases.
        pz.OnMouse (Wheel (+1.0f));
        Settle (pz);
        Assert::AreEqual (-96.0f, pz.PanY (), 0.001f);

        pz.OnMouse (Wheel (-1.0f));
        Settle (pz);
        Assert::AreEqual (0.0f, pz.PanY (), 0.001f);
    }



    TEST_METHOD (HorizontalWheel_MovesPanX)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanX = 50.0f;
        DxuiPanZoom  pz (cfg);
        pz.SetPanXBounds (-500.0f, 500.0f);

        pz.OnMouse (Wheel (+2.0f, /*horizontal*/ true));
        Settle (pz);
        Assert::AreEqual (100.0f, pz.PanX (), 0.001f);
    }



    TEST_METHOD (PanYBounds_ClampTarget)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanY = 96.0f;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-50.0f, 50.0f);

        for (int i = 0; i < 10; i++)
        {
            pz.OnMouse (Wheel (-1.0f));   // push up past the +50 ceiling
        }
        Settle (pz);
        Assert::AreEqual (50.0f, pz.PanY (), 0.001f);
    }



    // The touchpad-lag regression: a slow scroll streams many sub-notch wheel
    // deltas. Truncating each to a whole content unit would drop them; the
    // continuous target must accumulate the fractional motion.
    TEST_METHOD (SubNotchWheelDeltas_Accumulate)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanY = 96.0f;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-10000.0f, 10000.0f);

        for (int i = 0; i < 10; i++)
        {
            pz.OnMouse (Wheel (-0.1f));   // ten tenth-notch deltas == one notch
        }
        Settle (pz);
        Assert::AreEqual (96.0f, pz.PanY (), 0.01f);
    }



    TEST_METHOD (Tick_EasesCurrentTowardTarget)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanY  = 100.0f;
        cfg.easeTauSec = 0.1;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        pz.OnMouse (Wheel (-1.0f));       // target -> +100
        pz.Tick (0.0);                    // prime the clock
        pz.Tick (0.05);                   // half a tau: partway there

        Assert::IsTrue (pz.PanY () > 0.0f);
        Assert::IsTrue (pz.PanY () < 100.0f);
    }



    TEST_METHOD (EaseTauZero_SnapsImmediately)
    {
        DxuiPanZoom::Config  cfg;
        cfg.wheelPanY  = 100.0f;
        cfg.easeTauSec = 0.0;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        pz.OnMouse (Wheel (-1.0f));
        pz.Tick (0.0);
        pz.Tick (0.001);
        Assert::AreEqual (100.0f, pz.PanY (), 0.0001f);
    }



    TEST_METHOD (LeftDrag_PansBothAxesByScale)
    {
        DxuiPanZoom  pz;
        pz.SetPanXBounds (-1000.0f, 1000.0f);
        pz.SetPanYBounds (-1000.0f, 1000.0f);
        pz.SetDragScale (2.0f, 3.0f);

        pz.OnMouse (Mouse (DxuiMouseEventKind::Down, DxuiMouseButton::Left, 100, 100));
        pz.OnMouse (Mouse (DxuiMouseEventKind::Move, DxuiMouseButton::Left, 90, 80));
        pz.OnMouse (Mouse (DxuiMouseEventKind::Up,   DxuiMouseButton::Left, 90, 80));
        Settle (pz);

        // Dragging left 10px / up 20px moves content opposite: +20 x, +60 y.
        Assert::AreEqual (20.0f, pz.PanX (), 0.01f);
        Assert::AreEqual (60.0f, pz.PanY (), 0.01f);
    }



    TEST_METHOD (DragAfterUp_DoesNotPan)
    {
        DxuiPanZoom  pz;
        pz.SetPanYBounds (-1000.0f, 1000.0f);
        pz.SetDragScale (1.0f, 1.0f);

        pz.OnMouse (Mouse (DxuiMouseEventKind::Down, DxuiMouseButton::Left, 0, 0));
        pz.OnMouse (Mouse (DxuiMouseEventKind::Up,   DxuiMouseButton::Left, 0, 0));

        bool consumed = pz.OnMouse (Mouse (DxuiMouseEventKind::Move, DxuiMouseButton::None, 50, 50));
        Settle (pz);
        Assert::IsFalse  (consumed);
        Assert::AreEqual (0.0f, pz.PanY (), 0.0001f);
    }



    TEST_METHOD (CtrlPlusMinusZero_Zoom)
    {
        DxuiPanZoom  pz;

        Assert::IsTrue (pz.OnKey (CtrlKey (VK_OEM_PLUS)));
        Assert::AreEqual (1.25f, pz.ZoomTarget (), 0.0001f);

        Assert::IsTrue (pz.OnKey (CtrlKey (VK_OEM_MINUS)));
        Assert::AreEqual (1.0f, pz.ZoomTarget (), 0.0001f);

        pz.OnKey (CtrlKey (VK_ADD));
        pz.OnKey (CtrlKey (VK_ADD));
        Assert::IsTrue (pz.ZoomTarget () > 1.0f);
        Assert::IsTrue (pz.OnKey (CtrlKey ('0')));
        Assert::AreEqual (1.0f, pz.ZoomTarget (), 0.0001f);
    }



    TEST_METHOD (KeyWithoutCtrl_Ignored)
    {
        DxuiPanZoom   pz;
        DxuiKeyEvent  ev;
        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_OEM_PLUS;
        ev.ctrl = false;

        Assert::IsFalse (pz.OnKey (ev));
        Assert::AreEqual (1.0f, pz.ZoomTarget (), 0.0001f);
    }



    TEST_METHOD (SetPanYTarget_DoesNotFireUserPan_ButWheelDoes)
    {
        DxuiPanZoom  pz;
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        int  userPans = 0;
        pz.SetOnUserPanY ([&] { userPans++; });

        pz.SetPanYTarget (100.0f);            // programmatic (follow mode)
        Assert::AreEqual (0, userPans);

        pz.OnMouse (Wheel (-1.0f));           // genuine user scroll
        Assert::AreEqual (1, userPans);
    }



    TEST_METHOD (SnapPanY_TeleportsWithoutGlide)
    {
        DxuiPanZoom  pz;
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        pz.SnapPanY (250.0f);
        Assert::AreEqual (250.0f, pz.PanY (),       0.0001f);
        Assert::AreEqual (250.0f, pz.PanYTarget (), 0.0001f);
    }



    TEST_METHOD (PanByUser_MovesPanAndFiresUserPan)
    {
        DxuiPanZoom  pz;
        pz.SetPanYBounds (-1000.0f, 1000.0f);
        pz.SetPanXBounds (-1000.0f, 1000.0f);

        int  userPans = 0;
        pz.SetOnUserPanY ([&] { userPans++; });

        pz.PanByUser (10.0f, -48.0f);
        Settle (pz);
        Assert::AreEqual (10.0f,  pz.PanX (), 0.001f);
        Assert::AreEqual (-48.0f, pz.PanY (), 0.001f);
        Assert::AreEqual (1, userPans);
    }



    TEST_METHOD (ZoomEaseTauZero_ZoomsInstantlyWhilePanGlides)
    {
        DxuiPanZoom::Config  cfg;
        cfg.zoomEaseTauSec = 0.0;    // instant zoom
        cfg.easeTauSec     = 0.1;    // smooth pan
        cfg.wheelPanY      = 100.0f;
        DxuiPanZoom  pz (cfg);
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        pz.OnMouse (Wheel (+1.0f, false, true));   // zoom target 1.25
        pz.OnMouse (Wheel (-1.0f));                // pan target +100
        pz.Tick (0.0);
        pz.Tick (0.01);                            // a tiny slice of time

        Assert::AreEqual (1.25f, pz.Zoom (), 0.0001f);   // zoom already there
        Assert::IsTrue   (pz.PanY () > 0.0f);            // pan still gliding
        Assert::IsTrue   (pz.PanY () < 100.0f);
    }



    TEST_METHOD (OnChange_FiresForZoomAndPan)
    {
        DxuiPanZoom  pz;
        pz.SetPanYBounds (-1000.0f, 1000.0f);

        int  changes = 0;
        pz.SetOnChange ([&] { changes++; });

        pz.OnMouse (Wheel (+1.0f, false, true));   // zoom
        pz.OnMouse (Wheel (-1.0f));                // pan
        Assert::IsTrue (changes >= 2);
    }
};
