#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteTests
//
//  The site makes pane controls match its layout (FR-038 to FR-042): groups
//  become tab groups over the panes, splits become sashes, a tab dragged and
//  dropped on a zone docks there, and the Dock To menu and arrow keys are
//  the same operations. Every change is reported so the application can save
//  it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteTests
{
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        int              changes = 0;

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
            site.SetOnChanged ([this] { changes++; });
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
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



    TEST_CLASS (DxuiDockSiteTests)
    {
    public:

        TEST_METHOD (EachGroupIsATabGroupAndPanesFillItsBody)
        {
            Rig  rig;



            Assert::AreEqual ((size_t) 3, rig.site.GetGroupCount());
            Assert::IsTrue   (rig.code.IsVisible());
            Assert::IsTrue   (rig.console.GetBounds().top >= rig.code.GetBounds().bottom, L"console below code");
            Assert::AreEqual ((long) DxuiTabGroup::kStripDip, rig.code.GetBounds().top, L"below its strip");
            Assert::IsTrue   (rig.stack.IsVisible() != rig.regs.IsVisible(), L"tabbed: one shown");
        }


        TEST_METHOD (APressOnATabShowsItAndIsReported)
        {
            Rig            rig;
            DxuiTabGroup * group = nullptr;



            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                group = (rig.site.GetGroup (i)->GetTabCount() == 2) ? rig.site.GetGroup (i) : group;
            }

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (group->GetTabRect (0))));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   Center (group->GetTabRect (0))));

            Assert::IsTrue   (rig.regs.IsVisible());
            Assert::IsFalse  (rig.stack.IsVisible());
            Assert::AreEqual (1, rig.changes);
            Assert::AreEqual (std::wstring (L"regs"), rig.site.GetPaneAt (Center (rig.regs.GetBounds())));
        }


        TEST_METHOD (DraggingASashChangesTheRatio)
        {
            Rig   rig;
            long  before = rig.stack.GetBounds().left;
            POINT sash   = { before, 300 };



            Assert::IsTrue (rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, sash)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { 700, 300 }));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   POINT { 700, 300 }));

            Assert::AreEqual ((long) 700, rig.stack.GetBounds().left, L"the shown tab of the right-hand group");
            Assert::AreEqual (1, rig.changes, L"reported once, when the drag ends");
            //  A cursor id is an integer resource, so compare the pointers.
            Assert::IsTrue (IDC_SIZEWE == rig.site.GetCursorForPoint (POINT { 700, 300 }));
        }


        TEST_METHOD (ATabDraggedOntoAZoneDocksThere)
        {
            Rig                        rig;
            DxuiTabGroup             * group = nullptr;
            RECT                       tab   = {};
            const DxuiDockDropZone   * zone  = nullptr;



            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                group = (rig.site.GetGroup (i)->GetTabCount() == 2) ? rig.site.GetGroup (i) : group;
            }

            tab = group->GetTabRect (1);
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { Center (tab).x + 40, Center (tab).y + 40 }));

            Assert::IsTrue   (rig.site.IsDragging());
            Assert::AreEqual (std::wstring (L"stack"), rig.site.GetDraggedPane());

            //  The compass's tab square on the console group.
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (rig.console.GetBounds())));
            zone = rig.site.GetHoveredZone();
            Assert::IsNotNull (zone);
            Assert::IsTrue    (zone->kind == DxuiDockDropZone::Kind::Tab);

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, Center (rig.console.GetBounds())));

            Assert::IsFalse  (rig.site.IsDragging());
            Assert::AreEqual ((size_t) 2, rig.site.GetPaneLayout().GetGroup (L"console").size());
            Assert::IsTrue   (rig.stack.IsVisible(), L"the dropped pane is the active tab");
            Assert::AreEqual (1, rig.changes);
        }


        TEST_METHOD (ADropOutsideTheSiteAsksToFloat)
        {
            Rig           rig;
            std::wstring  floated;
            POINT         where   = {};



            rig.site.SetOnFloatRequested ([&] (const std::wstring & pane, POINT at) { floated = pane; where = at; });
            rig.site.BeginDrag (L"console");
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { 1400, 200 }));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   POINT { 1400, 200 }));

            Assert::AreEqual (std::wstring (L"console"), floated);
            Assert::AreEqual ((long) 1400, where.x);
            Assert::AreEqual (0, rig.changes, L"floating is the application's to do and report");
        }


        TEST_METHOD (ADropBetweenZonesChangesNothing)
        {
            Rig  rig;



            rig.site.BeginDrag (L"console");
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, POINT { 120, 60 }));

            Assert::AreEqual (0, rig.changes);
            Assert::IsTrue   (rig.site.GetPaneLayout().IsDocked (L"console"));
        }


        TEST_METHOD (TheDockToMenuOffersEdgesAndOtherGroups)
        {
            Rig                                   rig;
            std::vector<DxuiDockSite::MenuItem>   items = rig.site.GetDockToMenu (L"console");
            bool                                  tabbedWithCode = false;



            for (const DxuiDockSite::MenuItem & item : items)
            {
                tabbedWithCode = tabbedWithCode || item.label == L"Tab with Disassembly";
            }

            Assert::IsTrue   (tabbedWithCode);
            Assert::AreEqual (std::wstring (L"Dock Left"), items.at (0).label);
            Assert::IsTrue   (items.at (0).action());
            Assert::AreEqual ((long) 0, rig.console.GetBounds().left);
            Assert::AreEqual (1, rig.changes);
        }


        TEST_METHOD (AnArrowKeyMovesThePaneIntoTheNextGroup)
        {
            Rig  rig;



            Assert::IsTrue   (rig.site.MovePaneByArrow (L"console", DxuiDockSide::Right));
            Assert::AreEqual ((size_t) 3, rig.site.GetPaneLayout().GetGroup (L"regs").size());
            Assert::AreEqual ((size_t) 2, rig.site.GetGroupCount());
        }


        TEST_METHOD (AnAutoHiddenPaneIsAnEdgeTabThatSlidesOut)
        {
            Rig   rig;
            RECT  tab   = {};
            RECT  slid  = {};



            Assert::IsTrue (rig.site.GetDockToMenu (L"console").size() > 0);
            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();

            tab = rig.site.GetEdgeTabRect (L"console");
            Assert::IsFalse  (rig.console.IsVisible(), L"hidden until slid out");
            Assert::AreEqual ((long) 600, tab.bottom, L"a tab on the bottom edge");
            Assert::IsTrue   (rig.code.GetBounds().bottom <= tab.top, L"the docked panes give the strip room");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            slid = rig.site.GetSlidRect();

            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane());
            Assert::IsTrue   (rig.console.IsVisible());
            Assert::AreEqual (slid.bottom, rig.console.GetBounds().bottom);
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetPaneAt (Center (slid)));

            //  A press elsewhere slides it back.
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, POINT { 900, 40 }));
            Assert::IsTrue  (rig.site.GetSlidPane().empty());
            Assert::IsFalse (rig.console.IsVisible());
        }


        TEST_METHOD (TheDockToMenuOffersAutoHideAgainstTheNearestEdge)
        {
            Rig                                   rig;
            std::vector<DxuiDockSite::MenuItem>   items = rig.site.GetDockToMenu (L"regs");
            bool                                  done  = false;



            for (const DxuiDockSite::MenuItem & item : items)
            {
                if (item.label == L"Auto Hide")
                {
                    done = item.action();
                }
            }

            Assert::IsTrue (done);
            Assert::IsTrue (rig.site.GetPaneLayout().IsAutoHidden (L"regs"));
            Assert::IsTrue (rig.site.GetEdgeTabRect (L"regs").left >= 900, L"the right edge");
        }


        TEST_METHOD (AFloatingPaneIsLeftToItsWindow)
        {
            Rig  rig;



            Assert::IsTrue (rig.site.EditPaneLayout().Float (L"console", L"m", RECT { 0, 0, 300, 200 }));
            rig.console.SetVisible (true);
            rig.site.Relayout();

            Assert::IsTrue (rig.console.IsVisible(), L"the floating window shows it, not the site");
        }


        TEST_METHOD (AHiddenPaneIsHiddenAndKeepsItsPlace)
        {
            Rig  rig;



            rig.site.SetShownFn ([] (const std::wstring & pane) { return pane != L"console"; });
            rig.site.Relayout();

            Assert::IsFalse  (rig.console.IsVisible());
            Assert::AreEqual ((long) 600, rig.code.GetBounds().bottom, L"code takes the console's area");
            Assert::IsTrue   (rig.site.GetPaneLayout().IsDocked (L"console"));
        }
    };
}
