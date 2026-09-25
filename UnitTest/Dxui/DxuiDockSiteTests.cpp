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

            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a group of two tabs");
                return;
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

            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a group of two tabs");
                return;
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


        TEST_METHOD (AnEdgeTabIsAButtonNotAHoverTarget)
        {
            Rig   rig;
            RECT  tab = {};



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();
            tab = rig.site.GetEdgeTabRect (L"console");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (tab)));
            Assert::IsTrue (rig.site.GetSlidPane().empty(), L"a hover does not slide it out");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   Center (tab)));
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane());

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            Assert::IsTrue (rig.site.GetSlidPane().empty(), L"a second press slides it back");
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


        TEST_METHOD (AutoHideSendsTheTopOfTheRightColumnRight)
        {
            Rig   rig;
            RECT  regs = {};



            //  Registers alone at the top of the right-hand column, in a short
            //  window: wider than it is tall, and touching the top as well.
            Assert::IsTrue (rig.site.EditPaneLayout().DockToSide (L"stack", L"regs", DxuiDockSide::Bottom));
            rig.site.Layout (RECT { 0, 0, 1000, 300 }, rig.scaler);
            regs = rig.regs.GetBounds();
            Assert::IsTrue (regs.right - regs.left > regs.bottom - regs.top, L"the case the long-side rule sent to the top");

            for (const DxuiDockSite::MenuItem & item : rig.site.GetDockToMenu (L"regs"))
            {
                if (item.label == L"Auto Hide")
                {
                    (void) item.action();
                }
            }

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


        TEST_METHOD (DocumentGroupsHaveTopTabsAndTheRestTitleBars)
        {
            Rig             rig;
            DxuiTabGroup  * console = nullptr;



            rig.site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            rig.site.Relayout();

            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                DxuiTabGroup  * group = rig.site.GetGroup (i);

                Assert::IsTrue (group->GetKind() == ((group->IndexOf (&rig.code) >= 0) ? DxuiTabGroup::Kind::Document : DxuiTabGroup::Kind::ToolWindow));
                console = (group->IndexOf (&rig.console) >= 0) ? group : console;
            }

            if (console == nullptr)
            {
                Assert::Fail (L"the rig has a console group");
                return;
            }

            Assert::AreEqual (console->GetBounds().top + (long) DxuiTabGroup::kTitleDip, rig.console.GetBounds().top, L"below its title bar");
        }


        TEST_METHOD (APinOnATitleBarAutoHidesThePane)
        {
            Rig             rig;
            DxuiTabGroup  * console = nullptr;
            RECT            pin     = {};



            rig.site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            rig.site.Relayout();

            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                console = (rig.site.GetGroup (i)->IndexOf (&rig.console) >= 0) ? rig.site.GetGroup (i) : console;
            }

            if (console == nullptr)
            {
                Assert::Fail (L"the rig has a console group");
                return;
            }

            pin = console->GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin);
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (pin)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   Center (pin)));

            Assert::IsTrue (rig.site.GetPaneLayout().IsAutoHidden (L"console"));
        }


        TEST_METHOD (ASlidOutPaneLiesOverTheOthersBelowATitleBar)
        {
            Rig           rig;
            RECT          before = {};
            std::wstring  heard;



            rig.site.SetOnSlid ([&] (const std::wstring & pane) { heard = pane; });
            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();
            before = rig.code.GetBounds();

            rig.site.SlideOut (L"console");

            Assert::AreEqual (std::wstring (L"console"), heard);
            Assert::AreEqual (before.bottom, rig.code.GetBounds().bottom, L"the docked panes keep their places");
            Assert::AreEqual (rig.site.GetSlidRect().top + (long) DxuiTabGroup::kTitleDip, rig.console.GetBounds().top, L"below its title bar");

            rig.site.SlideIn();
            Assert::IsTrue (heard.empty());
        }


        TEST_METHOD (APinOnASlidOutPaneDocksItBack)
        {
            Rig    rig;
            RECT   slid = {};
            POINT  pin  = {};



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();
            rig.site.SlideOut (L"console");

            //  With no close handler the pin is the title bar's last button.
            slid = rig.site.GetSlidRect();
            pin  = POINT { slid.right - 1 - DxuiTabGroup::kTitleButtonDip / 2, slid.top + DxuiTabGroup::kTitleDip / 2 };

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, pin));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   pin));

            Assert::IsTrue (rig.site.GetPaneLayout().IsDocked (L"console"));
            Assert::IsTrue (rig.site.GetSlidPane().empty());
        }


        TEST_METHOD (DraggingASlidOutPanesTitleBarDocksItOnAZone)
        {
            Rig                        rig;
            RECT                       slid = {};
            const DxuiDockDropZone   * zone = nullptr;



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();
            rig.site.SlideOut (L"console");
            slid = rig.site.GetSlidRect();

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, POINT { slid.left + 20, slid.top + 8 }));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { slid.left + 60, slid.top - 40 }));
            Assert::IsTrue (rig.site.IsDragging());
            Assert::IsTrue (rig.site.GetSlidPane().empty(), L"slid back so the zones show");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (rig.regs.GetBounds())));
            zone = rig.site.GetHoveredZone();
            Assert::IsNotNull (zone);

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, Center (rig.regs.GetBounds())));
            Assert::IsTrue (rig.site.GetPaneLayout().IsDocked (L"console"));
        }


        TEST_METHOD (ASideTabRunsAlongItsEdge)
        {
            Rig   rig;
            RECT  tab = {};



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"regs", DxuiDockSide::Right));
            rig.site.Relayout();
            tab = rig.site.GetEdgeTabRect (L"regs");

            Assert::AreEqual ((long) DxuiTabGroup::kStripDip, tab.right - tab.left, L"one tab high");
            Assert::IsTrue   (tab.bottom - tab.top > tab.right - tab.left, L"its title runs down the edge");
            Assert::AreEqual ((long) 1000, tab.right);
        }


        TEST_METHOD (TheFocusedPanesGroupAloneHasTheFocusedLook)
        {
            Rig  rig;



            rig.site.SetFocusedPane (L"stack");

            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                DxuiTabGroup  * group = rig.site.GetGroup (i);

                Assert::AreEqual (group->IndexOf (&rig.stack) >= 0, group->HasFocusedLook());
            }
        }
    };
}
