#include "Pch.h"

#include "MockDxuiControl.h"

#include "Widgets/DxuiScrollPanel.h"
#include "Window/DxuiPropertyPage.h"

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


    TEST_METHOD (APress_FocusesWithoutTheRectangle_AndTabMovesOnFromIt)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();
        MockDxuiControl &  c = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        a.SetBounds (MakeRect (0,   0, 50,  20));
        b.SetBounds (MakeRect (60,  0, 110, 20));
        c.SetBounds (MakeRect (120, 0, 170, 20));
        focus.SetRowEpsilonDip (32.0f);
        focus.Attach (&panel);

        Assert::IsFalse  (focus.FocusAtPoint (POINT { 55, 10 }), L"a press between controls focuses nothing");
        Assert::IsTrue   (focus.FocusAtPoint (POINT { 80, 10 }));
        Assert::AreEqual (static_cast<void *> (&b), static_cast<void *> (focus.GetFocusedControl()), L"the control pressed takes focus");
        Assert::IsFalse  (b.IsFocusCueVisible(), L"but draws no focus rectangle");

        Assert::IsTrue   (focus.HandleKey (DxuiFocusKey::Tab));
        Assert::AreEqual (static_cast<void *> (&c), static_cast<void *> (focus.GetFocusedControl()), L"Tab goes to the control after the one pressed");
        Assert::IsTrue   (c.IsFocusCueVisible(), L"and the keyboard brings the rectangle back");
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


    //
    //  Visibility changes AFTER the order was built.
    //
    //  The tab order is a snapshot. A property sheet switching pages shows one
    //  subtree and hides another, and until Rebuild is called again the order
    //  still holds the hidden controls and still lacks the shown ones -- which
    //  put Tab on a control the user could not see and let Space operate it.
    //  Observed as a machine dropdown opening from the Screenshots tab.
    //

    TEST_METHOD (HidingAPage_LeavesTheOrderStaleUntilRebuilt)
    {
        DxuiPanel          panel;
        DxuiPanel       &  pageA = panel.Add<DxuiPanel>();
        DxuiPanel       &  pageB = panel.Add<DxuiPanel>();
        MockDxuiControl &  ctlA  = pageA.Add<MockDxuiControl>();
        MockDxuiControl &  ctlB  = pageB.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        ctlA.SetBounds (MakeRect (0,  0, 50, 20));
        ctlB.SetBounds (MakeRect (0, 30, 50, 50));
        pageB.SetVisible (false);

        focus.Attach (&panel);
        focus.Rebuild();

        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&ctlA), static_cast<void *> (focus.GetTabOrderAt (0)));

        // Switch pages the way a sheet does, WITHOUT rebuilding.
        pageA.SetVisible (false);
        pageB.SetVisible (true);

        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&ctlA), static_cast<void *> (focus.GetTabOrderAt (0)),
            L"the stale order still holds the control on the now-hidden page");
    }


    TEST_METHOD (RebuildAfterAPageSwitch_SwapsTheOrderOver)
    {
        DxuiPanel          panel;
        DxuiPanel       &  pageA = panel.Add<DxuiPanel>();
        DxuiPanel       &  pageB = panel.Add<DxuiPanel>();
        MockDxuiControl &  ctlA  = pageA.Add<MockDxuiControl>();
        MockDxuiControl &  ctlB  = pageB.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        ctlA.SetBounds (MakeRect (0,  0, 50, 20));
        ctlB.SetBounds (MakeRect (0, 30, 50, 50));
        pageB.SetVisible (false);

        focus.Attach (&panel);
        focus.Rebuild();

        pageA.SetVisible (false);
        pageB.SetVisible (true);
        focus.Rebuild();

        Assert::AreEqual ((size_t) 1, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&ctlB), static_cast<void *> (focus.GetTabOrderAt (0)),
            L"only the visible page's control is reachable");
    }


    // Tab must not land on a hidden control even when focus started on one:
    // the rescue is the caller's, but the order must not offer it either.
    TEST_METHOD (AfterRebuild_TabCannotReachAHiddenPage)
    {
        DxuiPanel          panel;
        DxuiPanel       &  pageA = panel.Add<DxuiPanel>();
        DxuiPanel       &  pageB = panel.Add<DxuiPanel>();
        MockDxuiControl &  ctlA  = pageA.Add<MockDxuiControl>();
        MockDxuiControl &  ctlB  = pageB.Add<MockDxuiControl>();
        DxuiFocusManager   focus;


        ctlA.SetBounds (MakeRect (0,  0, 50, 20));
        ctlB.SetBounds (MakeRect (0, 30, 50, 50));
        pageB.SetVisible (false);

        focus.Attach (&panel);
        focus.Rebuild();
        focus.SetFocused (&ctlA);

        pageA.SetVisible (false);
        pageB.SetVisible (true);
        focus.Rebuild();

        focus.HandleKey (DxuiFocusKey::Tab);

        Assert::AreEqual (static_cast<void *> (&ctlB), static_cast<void *> (focus.GetFocusedControl()),
            L"Tab from a now-hidden control lands on the visible page");
    }


    //  A sheet scrolled down shows its page through a viewport between the tab
    //  strip and the button row, and lays the page's controls out past it: one
    //  above the tab strip, a table's row and the page's last control below
    //  the buttons. Tab still takes the page's controls, table row included,
    //  after the tab strip and before the buttons.
    TEST_METHOD (Tab_ReachesAScrolledPageBeforeTheButtonsBelowIt)
    {
        DxuiPanel            root;
        MockDxuiControl    & tabs     = root.Add<MockDxuiControl>();
        DxuiPropertyPage   & page     = root.Add<DxuiPropertyPage> (std::wstring (L"Page"));
        MockDxuiControl    & ok       = root.Add<MockDxuiControl>();
        MockDxuiControl    & first    = page.Add<MockDxuiControl>();
        DxuiScrollPanel    & table    = page.Add<DxuiScrollPanel>();
        MockDxuiControl    & row      = table.Add<MockDxuiControl>();
        MockDxuiControl    & reset    = page.Add<MockDxuiControl>();
        RECT                 viewport = MakeRect (0, 30, 300, 200);
        DxuiFocusManager     focus;



        tabs.SetBounds     (MakeRect (0,   0,   300, 24));
        page.SetViewport   (&viewport);
        page.SetBounds     (MakeRect (0,   -100, 300, 500));
        first.SetBounds    (MakeRect (10,  -90,  100, -70));
        table.SetBounds    (MakeRect (10,  250,  200, 300));
        row.SetBounds      (MakeRect (10,  260,  190, 280));
        reset.SetBounds    (MakeRect (10,  400,  100, 420));
        ok.SetBounds       (MakeRect (200, 210,  290, 234));

        focus.SetRowEpsilonDip (16.0f);
        focus.Attach (&root);

        Assert::AreEqual ((size_t) 5, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&tabs),  static_cast<void *> (focus.GetTabOrderAt (0)), L"the tab strip first");
        Assert::AreEqual (static_cast<void *> (&first), static_cast<void *> (focus.GetTabOrderAt (1)), L"then the page, from its top");
        Assert::AreEqual (static_cast<void *> (&row),   static_cast<void *> (focus.GetTabOrderAt (2)), L"the table's row in the page's order");
        Assert::AreEqual (static_cast<void *> (&reset), static_cast<void *> (focus.GetTabOrderAt (3)), L"the page's last control before the buttons");
        Assert::AreEqual (static_cast<void *> (&ok),    static_cast<void *> (focus.GetTabOrderAt (4)));
    }


    //  A control taken away while it has focus, as a mapping row set to None
    //  is, passes focus to the next control in the tab order, with the focus
    //  rectangle it had, rather than dropping it.
    TEST_METHOD (RemovingTheFocusedControl_MovesFocusToTheNext)
    {
        DxuiPanel          panel;
        MockDxuiControl  & a     = panel.Add<MockDxuiControl>();
        MockDxuiControl  & b     = panel.Add<MockDxuiControl>();
        MockDxuiControl  & c     = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;



        a.SetBounds (MakeRect (0,   0, 50,  20));
        b.SetBounds (MakeRect (60,  0, 110, 20));
        c.SetBounds (MakeRect (120, 0, 170, 20));
        focus.SetRowEpsilonDip (32.0f);
        focus.Attach (&panel);
        focus.SetFocused (&b);

        b.SetVisible (false);
        focus.Rebuild();

        Assert::AreEqual (static_cast<void *> (&c), static_cast<void *> (focus.GetFocusedControl()), L"the next control");
        Assert::IsTrue   (c.IsFocusCueVisible(), L"with the focus rectangle");
        Assert::IsTrue   (c.lastFocused);
    }


    //  Past the end of the order, focus goes back to the nearest control
    //  before it that is still there, rather than around to the first.
    TEST_METHOD (RemovingTheFocusedControlAtTheEnd_MovesFocusToThePrevious)
    {
        DxuiPanel          panel;
        MockDxuiControl  & a     = panel.Add<MockDxuiControl>();
        MockDxuiControl  & b     = panel.Add<MockDxuiControl>();
        MockDxuiControl  & c     = panel.Add<MockDxuiControl>();
        MockDxuiControl  & d     = panel.Add<MockDxuiControl>();
        DxuiFocusManager   focus;



        a.SetBounds (MakeRect (0,   0, 50,  20));
        b.SetBounds (MakeRect (60,  0, 110, 20));
        c.SetBounds (MakeRect (120, 0, 170, 20));
        d.SetBounds (MakeRect (180, 0, 230, 20));
        focus.SetRowEpsilonDip (32.0f);
        focus.Attach (&panel);
        focus.SetFocused (&c);

        c.SetVisible (false);
        d.SetVisible (false);
        focus.Rebuild();

        Assert::AreEqual (static_cast<void *> (&b), static_cast<void *> (focus.GetFocusedControl()), L"the previous control, not the first");
    }
};
