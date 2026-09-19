#include "Pch.h"

#include "Widgets/DxuiTabGroup.h"
#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroupTests
//
//  Tabs over controls it does not own (FR-038): the active one fills the
//  body, the rest are hidden, a press activates and a drag reports, Ctrl+Tab
//  cycles, and an indicator marks a tab whose content changed out of sight.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTabGroupTests
{
    struct Rig
    {
        DxuiTabGroup     group;
        MockDxuiControl  a;
        MockDxuiControl  b;
        MockDxuiControl  c;
        DxuiDpiScaler    scaler;

        Rig()
        {
            group.AddTab (L"Registers", &a);
            group.AddTab (L"Stack",     &b);
            group.AddTab (L"Watch",     &c);
            group.Layout (RECT { 0, 0, 400, 300 }, scaler);
        }
    };



    static DxuiMouseEvent Mouse (DxuiMouseEventKind kind, long x, long y)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };
        return ev;
    }



    TEST_CLASS (DxuiTabGroupTests)
    {
    public:

        TEST_METHOD (TheFirstTabIsShownInTheBodyAndTheRestAreHidden)
        {
            Rig  rig;



            Assert::AreEqual (0, rig.group.GetActive());
            Assert::IsTrue   (rig.a.IsVisible());
            Assert::IsFalse  (rig.b.IsVisible());
            Assert::AreEqual ((long) DxuiTabGroup::kStripDip, rig.a.GetBounds().top, L"below the strip");
            Assert::AreEqual ((long) 300, rig.a.GetBounds().bottom);
        }


        TEST_METHOD (APressOnATabActivatesIt)
        {
            Rig   rig;
            RECT  tab  = rig.group.GetTabRect (1);
            int   told = -1;



            rig.group.SetOnActivated ([&] (int index) { told = index; });

            Assert::IsTrue   (rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, tab.left + 3, tab.top + 3)));
            Assert::AreEqual (1, rig.group.GetActive());
            Assert::AreEqual (1, told);
            Assert::IsTrue   (rig.b.IsVisible());
            Assert::IsFalse  (rig.a.IsVisible());
        }


        TEST_METHOD (TabsSitSideBySideAndHitTestByTitle)
        {
            Rig  rig;



            Assert::AreEqual (rig.group.GetTabRect (0).right, rig.group.GetTabRect (1).left);
            Assert::IsTrue   (rig.group.GetTabRect (0).right - rig.group.GetTabRect (0).left >
                              rig.group.GetTabRect (1).right - rig.group.GetTabRect (1).left, L"a longer title, a wider tab");
            Assert::AreEqual (-1, rig.group.HitTestTab (POINT { 5, 100 }), L"the body is not a tab");
            Assert::AreEqual (-1, rig.group.HitTestTab (POINT { 399, 5 }), L"the strip past the last tab");
        }


        TEST_METHOD (ADragPastTheDistanceReportsOnce)
        {
            Rig   rig;
            RECT  tab   = rig.group.GetTabRect (2);
            int   drags = 0;
            int   which = -1;



            rig.group.SetOnDragStart ([&] (int index, POINT) { drags++; which = index; });
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, tab.left + 3, tab.top + 3));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, tab.left + 5, tab.top + 3));
            Assert::AreEqual (0, drags, L"within the drag distance");

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, tab.left + 30, tab.top + 40));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, tab.left + 60, tab.top + 80));
            Assert::AreEqual (1, drags);
            Assert::AreEqual (2, which);

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up, tab.left + 60, tab.top + 80));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, tab.left + 90, tab.top + 90));
            Assert::AreEqual (1, drags, L"released");
        }


        TEST_METHOD (CtrlTabCyclesAndShiftGoesBack)
        {
            Rig           rig;
            DxuiKeyEvent  key;



            key.kind = DxuiKeyEventKind::Down;
            key.vk   = VK_TAB;
            key.ctrl = true;

            Assert::IsTrue   (rig.group.OnKey (key));
            Assert::AreEqual (1, rig.group.GetActive());

            key.shift = true;
            rig.group.OnKey (key);
            rig.group.OnKey (key);
            Assert::AreEqual (2, rig.group.GetActive(), L"back past the first wraps to the last");

            key.ctrl = false;
            Assert::IsFalse  (rig.group.OnKey (key), L"plain Tab is focus traversal");
        }


        TEST_METHOD (AnIndicatorMarksAHiddenTabUntilItIsShown)
        {
            Rig  rig;



            rig.group.SetIndicator (&rig.c, true);
            rig.group.SetIndicator (&rig.a, true);

            Assert::IsTrue  (rig.group.HasIndicator (2));
            Assert::IsFalse (rig.group.HasIndicator (0), L"the shown tab never carries one");

            rig.group.SetActive (2);
            Assert::IsFalse (rig.group.HasIndicator (2), L"cleared when shown");
        }


        TEST_METHOD (RemovingTheActiveTabShowsItsNeighbor)
        {
            Rig  rig;



            rig.group.SetActive (2);
            Assert::IsTrue   (rig.group.RemoveTab (&rig.c));
            Assert::AreEqual (1, rig.group.GetActive());
            Assert::IsTrue   (rig.b.IsVisible());
            Assert::IsFalse  (rig.group.RemoveTab (&rig.c));

            rig.group.RemoveTab (&rig.a);
            rig.group.RemoveTab (&rig.b);
            Assert::AreEqual (-1, rig.group.GetActive());
            Assert::IsNull   (rig.group.GetActiveContent());
        }


        TEST_METHOD (PaintDrawsEveryTitle)
        {
            Rig                   rig;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            rig.group.Paint (painter, text, theme);
            Assert::AreEqual ((size_t) 3, text.Calls().size());
        }
    };
}
