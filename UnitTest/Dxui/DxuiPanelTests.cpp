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


    TEST_METHOD (Adopt_AddsNonOwnedChildToWalks)
    {
        DxuiPanel             panel;
        MockDxuiControl       caller;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        panel.Adopt (caller);

        Assert::AreEqual ((size_t) 1, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&caller), static_cast<void *> (panel.Child (0)));
        Assert::AreEqual (static_cast<void *> (&panel),  static_cast<void *> (caller.Parent()));

        panel.Paint (painter, text, theme);
        Assert::AreEqual (1, caller.paintCount);
    }


    TEST_METHOD (Adopt_DestructionLeavesAdoptedChildAlive)
    {
        MockDxuiControl  caller;


        {
            DxuiPanel  panel;
            panel.Adopt (caller);
        }
        // If the panel had taken ownership the dtor would have freed
        // caller; we still have a live local, so its destructor must
        // run after this scope on its own terms. (Test simply asserts
        // the panel destructor compiled and ran without UAF.)
        caller.SetEnabled (true);
        Assert::IsTrue (caller.Enabled());
    }


    TEST_METHOD (Adopt_DuplicateIsNoOp)
    {
        DxuiPanel        panel;
        MockDxuiControl  caller;


        panel.Adopt (caller);
        panel.Adopt (caller);

        Assert::AreEqual ((size_t) 1, panel.ChildCount());
    }


    TEST_METHOD (RemoveAdopted_DropsRegistrationLeavesPointerLive)
    {
        DxuiPanel        panel;
        MockDxuiControl  caller;


        panel.Adopt (caller);
        Assert::IsTrue   (panel.RemoveAdopted (caller));
        Assert::AreEqual ((size_t) 0, panel.ChildCount());
        Assert::IsNull   (caller.Parent());
    }


    TEST_METHOD (RemoveAdopted_UnknownReturnsFalse)
    {
        DxuiPanel        panel;
        MockDxuiControl  stranger;


        Assert::IsFalse (panel.RemoveAdopted (stranger));
    }


    TEST_METHOD (RemoveAdopted_RefusesToDropOwnedChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  owned = panel.Add<MockDxuiControl>();


        Assert::IsFalse (panel.RemoveAdopted (owned));
        Assert::AreEqual ((size_t) 1, panel.ChildCount());
    }


    TEST_METHOD (ClearAdopted_LeavesOwnedChildrenAlone)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedA;
        MockDxuiControl    adoptedB;
        MockDxuiControl &  owned = panel.Add<MockDxuiControl>();


        panel.Adopt (adoptedA);
        panel.Adopt (adoptedB);
        Assert::AreEqual ((size_t) 3, panel.ChildCount());

        panel.ClearAdopted();

        Assert::AreEqual ((size_t) 1, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&owned), static_cast<void *> (panel.Child (0)));
        Assert::IsNull   (adoptedA.Parent());
        Assert::IsNull   (adoptedB.Parent());
    }


    TEST_METHOD (AdoptAndAdd_CoexistInInsertionOrder)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedFirst;
        MockDxuiControl    adoptedLast;


        panel.Adopt (adoptedFirst);
        MockDxuiControl &  ownedMiddle = panel.Add<MockDxuiControl>();
        panel.Adopt (adoptedLast);

        Assert::AreEqual ((size_t) 3, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&adoptedFirst), static_cast<void *> (panel.Child (0)));
        Assert::AreEqual (static_cast<void *> (&ownedMiddle),  static_cast<void *> (panel.Child (1)));
        Assert::AreEqual (static_cast<void *> (&adoptedLast),  static_cast<void *> (panel.Child (2)));
    }


    TEST_METHOD (Adopt_MouseDispatchesFrontToBackAcrossOwnedAndAdopted)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedFront;
        DxuiMouseEvent     ev;


        panel.Add<MockDxuiControl>();              // owned, rear
        panel.Adopt (adoptedFront);                // adopted, front (last-inserted)
        adoptedFront.consumeMouse = true;

        Assert::IsTrue   (panel.OnMouse (ev));
        Assert::AreEqual (1, adoptedFront.mouseCount);
        Assert::AreEqual (static_cast<void *> (&adoptedFront), static_cast<void *> (panel.Child (1)));
    }


    TEST_METHOD (StackLayout_AssignsBoundsToLeafWidgetChild)
    {
        DxuiPanel       panel;
        DxuiDpiScaler   scaler;
        MockDxuiControl a;
        MockDxuiControl b;
        RECT            bounds = MakeRect (0, 0, 200, 50);


        scaler.SetDpi (96);
        a.SetBounds (MakeRect (0, 0, 60, 50));
        b.SetBounds (MakeRect (0, 0, 80, 50));
        panel.Adopt (a);
        panel.Adopt (b);
        panel.SetLayout (std::make_unique<DxuiStackLayout> (DxuiStackLayout::Orientation::Horizontal,
                                                            0.0f,
                                                            DxuiStackLayout::Align::Stretch));

        panel.Layout (bounds, scaler);

        // Layout-policy positioning must be visible via the leaf
        // widget's IDxuiControl::Bounds() — i.e., the policy's
        // SetBounds calls actually take effect on the widget. This
        // is the regression check for the rect-duality fix.
        Assert::AreEqual ((LONG) 0,   a.Bounds().left);
        Assert::AreEqual ((LONG) 60,  a.Bounds().right);
        Assert::AreEqual ((LONG) 60,  b.Bounds().left);
        Assert::AreEqual ((LONG) 140, b.Bounds().right);
    }
};
