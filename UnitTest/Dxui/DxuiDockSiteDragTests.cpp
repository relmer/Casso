#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteDragTests
//
//  Dragging panes as Visual Studio does: a tool window's title bar carries
//  its whole group, and a drop on a group's tabs inserts the dragged tab
//  where the strip opened a gap for it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteDragTests
{
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane (L"code",    L"Disassembly", &code);
            site.AddPane (L"regs",    L"Registers",   &regs);
            site.AddPane (L"console", L"Console",     &console);
            site.AddPane (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
        }

        DxuiTabGroup * GroupOf (const MockDxuiControl & content)
        {
            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                if (site.GetGroup (i)->IndexOf (const_cast<MockDxuiControl *> (&content)) >= 0)
                {
                    return site.GetGroup (i);
                }
            }

            return nullptr;
        }
    };



    static DxuiMouseEvent Mouse (DxuiMouseEventKind kind, POINT at)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = at;
        return ev;
    }



    static POINT Center (const RECT & r)
    {
        return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
    }



    TEST_CLASS (DxuiDockSiteDragTests)
    {
    public:

        TEST_METHOD (TabWithAtInsertsAheadOfTheTab)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"a");



            layout.Add (L"b", L"a");
            layout.Add (L"c", L"");

            Assert::IsTrue (layout.TabWithAt (L"c", L"a", 1));
            Assert::IsTrue (layout.GetGroup (L"a") == std::vector<std::wstring> { L"a", L"c", L"b" });
            Assert::IsFalse (layout.IsDocked (L"x"));
        }


        TEST_METHOD (TabWithAtMovesATabWithinItsGroup)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"a");



            layout.Add (L"b", L"a");
            layout.Add (L"c", L"a");

            Assert::IsTrue  (layout.TabWithAt (L"a", L"b", 3));
            Assert::IsTrue  (layout.GetGroup (L"a") == std::vector<std::wstring> { L"b", L"c", L"a" });
            Assert::IsFalse (layout.TabWithAt (L"c", L"b", 2), L"already there");
        }


        //  A tool window's title bar carries every tab of its group, in order,
        //  and the one shown stays shown.
        TEST_METHOD (DraggingATitleBarMovesTheWholeGroup)
        {
            Rig             rig;
            DxuiTabGroup  * tools      = rig.GroupOf (rig.regs);
            RECT            title      = {};
            bool            stackShown = false;



            if (tools == nullptr)
            {
                Assert::Fail (L"the rig has a group of two tool windows");
                return;
            }

            stackShown = rig.stack.IsVisible();
            title      = tools->GetTitleRect();

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, POINT { title.left + 20, title.top + 8 }));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { title.left + 60, title.top + 60 }));
            Assert::IsTrue (rig.site.IsDragging());
            Assert::AreEqual ((size_t) 2, rig.site.GetDraggedPanes().size());

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (rig.console.GetBounds())));
            Assert::IsNotNull (rig.site.GetHoveredZone());
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, Center (rig.console.GetBounds())));

            Assert::IsTrue   (rig.site.GetPaneLayout().GetGroup (L"console") == std::vector<std::wstring> { L"console", L"regs", L"stack" });
            Assert::AreEqual (stackShown, rig.stack.IsVisible(), L"the same tab shown");
            Assert::AreEqual (!stackShown, rig.regs.IsVisible());
        }


        //  A tab dragged out of a group still moves alone.
        TEST_METHOD (DraggingATabMovesThatTabAlone)
        {
            Rig  rig;



            rig.site.BeginDrag (L"stack");
            Assert::AreEqual ((size_t) 1, rig.site.GetDraggedPanes().size());
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, Center (rig.console.GetBounds())));

            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"console") == std::vector<std::wstring> { L"console", L"stack" });
            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"regs")    == std::vector<std::wstring> { L"regs" });
        }


        //  Over a group's tabs the strip opens a gap where the tab will land,
        //  pushing the tabs after it right, and a drop inserts it there.
        TEST_METHOD (ADropOnATabStripInsertsTheTabAtTheGap)
        {
            Rig             rig;
            DxuiTabGroup  * code   = rig.GroupOf (rig.code);
            RECT            tab    = {};
            POINT           at     = {};



            if (code == nullptr)
            {
                Assert::Fail (L"the rig has a code group");
                return;
            }

            tab = code->GetTabRect (0);
            at  = POINT { tab.left + 4, Center (tab).y };

            rig.site.BeginDrag (L"console");
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, at));

            Assert::IsNull   (rig.site.GetHoveredZone());
            Assert::AreEqual (0, rig.site.GetStripTargetIndex());
            Assert::IsTrue   (rig.site.GetStripTargetGroup() >= 0);
            Assert::AreEqual (tab.left + (long) DxuiDockSite::kInsertGapDip, code->GetTabRect (0).left, L"the tab moved right for the gap");
            Assert::AreEqual (tab.left, code->GetInsertGapRect().left);

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, at));

            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"code") == std::vector<std::wstring> { L"console", L"code" });
            Assert::IsTrue (rig.console.IsVisible(), L"the dropped tab is shown");
            Assert::AreEqual (-1, rig.site.GetStripTargetGroup());
        }


        //  A group dropped on a strip goes in whole, after the last tab when
        //  dropped past it.
        TEST_METHOD (AGroupDroppedOnAStripGoesInWhole)
        {
            Rig             rig;
            DxuiTabGroup  * code  = rig.GroupOf (rig.code);
            RECT            tab   = {};
            POINT           at    = {};



            if (code == nullptr)
            {
                Assert::Fail (L"the rig has a code group");
                return;
            }

            tab = code->GetTabRect (0);
            at  = POINT { tab.right + 40, Center (tab).y };

            rig.site.BeginGroupDrag ({ L"regs", L"stack" }, L"regs");
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, at));
            Assert::AreEqual (1, rig.site.GetStripTargetIndex());
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, at));

            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"code") == std::vector<std::wstring> { L"code", L"regs", L"stack" });
            Assert::IsTrue (rig.regs.IsVisible());
        }
    };
}
