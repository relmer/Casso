#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusManagerTests
//
//  Focus traversal: Tab order, spatial arrow movement, and scope push and pop.
//
//  Tab walks the TREE while arrows walk GEOMETRY, and both are tested because
//  they legitimately disagree -- a tree that reads sensibly can still be laid
//  out in a grid, and each rule is right for its own key.
//
//  Invisible and disabled controls must be SKIPPED rather than focused and
//  passed over, since a focus ring on something the user cannot see or use
//  reads as the UI having hung.
//
//  Scopes get their own coverage because they are what makes a dialog modal to
//  the keyboard: while one is pushed, traversal must not escape it, and Escape
//  pops rather than reaching the window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiFocusManagerTests)
{
public:

    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }

    TEST_METHOD (TabAcrossRowsAndCols_FollowsReadingOrder)
    {
        DxuiPanel          panel;
        MockDxuiControl &  topLeft     = panel.Add<MockDxuiControl>();
        MockDxuiControl &  topMid      = panel.Add<MockDxuiControl>();
        MockDxuiControl &  topRight    = panel.Add<MockDxuiControl>();
        MockDxuiControl &  bottomLeft  = panel.Add<MockDxuiControl>();
        MockDxuiControl &  bottomRight = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        topLeft.SetBounds     (MakeRect (0,   0,   50,  20));
        topMid.SetBounds      (MakeRect (60,  0,   110, 20));
        topRight.SetBounds    (MakeRect (120, 0,   170, 20));
        bottomLeft.SetBounds  (MakeRect (0,   100, 50,  120));
        bottomRight.SetBounds (MakeRect (120, 100, 170, 120));

        focus.SetRowEpsilonDip (32.0f);
        focus.Attach (&panel);

        Assert::AreEqual ((size_t) 5, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&topLeft), static_cast<void *> (focus.GetTabOrderAt (0)));
        Assert::AreEqual (static_cast<void *> (&topMid), static_cast<void *> (focus.GetTabOrderAt (1)));
        Assert::AreEqual (static_cast<void *> (&topRight), static_cast<void *> (focus.GetTabOrderAt (2)));
        Assert::AreEqual (static_cast<void *> (&bottomLeft), static_cast<void *> (focus.GetTabOrderAt (3)));
        Assert::AreEqual (static_cast<void *> (&bottomRight), static_cast<void *> (focus.GetTabOrderAt (4)));
    }


    TEST_METHOD (TabKey_AdvancesFocusForward)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        a.SetBounds (MakeRect (0, 0, 50, 20));
        b.SetBounds (MakeRect (60, 0, 110, 20));
        focus.SetRowEpsilonDip (32.0f);
        focus.Attach (&panel);

        Assert::IsTrue   (focus.HandleKey (DxuiFocusKey::Tab));
        Assert::AreEqual (static_cast<void *> (&a), static_cast<void *> (focus.GetFocusedControl()));

        Assert::IsTrue   (focus.HandleKey (DxuiFocusKey::Tab));
        Assert::AreEqual (static_cast<void *> (&b), static_cast<void *> (focus.GetFocusedControl()));

        // Wraps to a.
        Assert::IsTrue   (focus.HandleKey (DxuiFocusKey::Tab));
        Assert::AreEqual (static_cast<void *> (&a), static_cast<void *> (focus.GetFocusedControl()));
    }


    TEST_METHOD (ExplicitTabIndex_BeatsGeometry)
    {
        DxuiPanel          panel;
        MockDxuiControl &  geomFirst   = panel.Add<MockDxuiControl>();
        MockDxuiControl &  explicitOne = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        geomFirst.SetBounds   (MakeRect (0,   0, 50,  20));
        explicitOne.SetBounds (MakeRect (200, 0, 250, 20));
        explicitOne.SetTabIndex (0);   // explicit index wins over geometry

        focus.Attach (&panel);

        Assert::AreEqual (static_cast<void *> (&explicitOne), static_cast<void *> (focus.GetTabOrderAt (0)));
        Assert::AreEqual (static_cast<void *> (&geomFirst), static_cast<void *> (focus.GetTabOrderAt (1)));
    }


    TEST_METHOD (TabIndexExcluded_RemovedFromTabOrder)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        b.SetTabIndex (IDxuiControl::kTabIndexExcluded);
        focus.Attach (&panel);

        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&a), static_cast<void *> (focus.GetTabOrderAt (0)));
    }


    TEST_METHOD (HiddenDisabledOrNonFocusable_Skipped)
    {
        DxuiPanel          panel;
        MockDxuiControl &  visible      = panel.Add<MockDxuiControl>();
        MockDxuiControl &  hidden       = panel.Add<MockDxuiControl>();
        MockDxuiControl &  disabled     = panel.Add<MockDxuiControl>();
        MockDxuiControl &  nonFocusable = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        hidden.SetVisible       (false);
        disabled.SetEnabled     (false);
        nonFocusable.SetFocusable (false);

        focus.Attach (&panel);

        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&visible), static_cast<void *> (focus.GetTabOrderAt (0)));
    }


    TEST_METHOD (ArrowDown_PicksNearestBelow)
    {
        DxuiPanel           panel;
        MockDxuiControl   & top       = panel.Add<MockDxuiControl>();
        MockDxuiControl   & belowFar  = panel.Add<MockDxuiControl>();
        MockDxuiControl   & belowNear = panel.Add<MockDxuiControl>();
        DxuiFocusManager    focus;


        top.SetBounds       (MakeRect (0, 0,  50, 20));
        belowNear.SetBounds (MakeRect (0, 30, 50, 50));
        belowFar.SetBounds  (MakeRect (0, 80, 50, 100));
        focus.SetRowEpsilonDip (8.0f);
        focus.Attach   (&panel);
        focus.SetFocused (&top);

        Assert::IsTrue   (focus.HandleKey (DxuiFocusKey::ArrowDown));
        Assert::AreEqual (static_cast<void *> (&belowNear), static_cast<void *> (focus.GetFocusedControl()));
    }


    TEST_METHOD (FocusScope_RestrictsTabAndRestoresOnPop)
    {
        DxuiPanel          panel;
        MockDxuiControl &  outer    = panel.Add<MockDxuiControl>();
        DxuiPanel       &  popup    = panel.Add<DxuiPanel>();
        MockDxuiControl &  popupCtl = popup.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        outer.SetBounds    (MakeRect (0, 0,  50, 20));
        popupCtl.SetBounds (MakeRect (0, 50, 50, 70));
        focus.SetRowEpsilonDip (8.0f);
        focus.Attach (&panel);
        focus.SetFocused (&outer);

        focus.PushScope (&popup);
        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&popupCtl), static_cast<void *> (focus.GetTabOrderAt (0)));

        focus.PopScope();
        Assert::AreEqual (static_cast<void *> (&outer), static_cast<void *> (focus.GetFocusedControl()));
    }
};

