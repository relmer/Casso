#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteDragCancelTests
//
//  As in Visual Studio, a pane's drag shows a cross and a shade only while
//  the pointer is over the panes: off them, over a menu, a toolbar or a
//  status bar, both go at once and the edge guides stay. Escape cancels a
//  drag the pointer started, and so does the window losing the mouse with
//  the button down; either leaves every pane where it was.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteDragCancelTests
{
    //  The documents' group on the left over the console's, and the
    //  registers tabbed with the stack on the right, in a site whose top
    //  lies `top` below the window's, as a toolbar's height would put it.
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        MockDxuiTheme    theme;
        int              changes = 0;

        explicit Rig (long height = 600, long top = 0)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.AddPane       (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetPaneLayout (layout);
            site.SetOnChanged  ([this] { changes++; });
            site.Layout        (RECT { 0, top, 1000, top + height }, scaler);
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

        size_t CountGuides()
        {
            size_t  guides = 0;

            for (const DxuiDockDragMark & mark : site.GetDragMarks (theme))
            {
                guides += (mark.image != nullptr) ? 1 : 0;
            }

            return guides;
        }

        size_t CountShades()
        {
            return site.GetDragMarks (theme).size() - CountGuides();
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



    static DxuiKeyEvent Key (WPARAM vk, DxuiKeyEventKind kind = DxuiKeyEventKind::Down)
    {
        DxuiKeyEvent  ev;



        ev.kind = kind;
        ev.vk   = vk;
        return ev;
    }



    static POINT Center (const RECT & r)
    {
        return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
    }



    //  A press on the tool windows' title bar dragged past the drag
    //  distance, which drags the registers and the stack together.
    static bool TryDragTheToolWindows (Rig & rig)
    {
        DxuiTabGroup  * tools = rig.GroupOf (rig.regs);
        RECT            title = {};



        if (tools == nullptr)
        {
            return false;
        }

        title = tools->GetTitleRect();

        rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, POINT { title.left + 20, title.top + 8 }));
        rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { title.left + 60, title.top + 8 }));

        return rig.site.HasPointerDrag();
    }



    TEST_CLASS (DxuiDockSiteDragCancelTests)
    {
    public:

        //  The pointer's drag over the console's cross, then Escape: the drag
        //  and everything it showed go, and the release that follows over the
        //  same button docks nothing.
        TEST_METHOD (EscapeCancelsADragAndLeavesTheLayout)
        {
            Rig           rig;
            std::wstring  before  = rig.site.GetPaneLayout().ToText();
            POINT         console = {};



            Assert::IsTrue (TryDragTheToolWindows (rig), L"the title bar's drag is the pointer's");

            console = Center (rig.console.GetBounds());
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, console));
            Assert::IsNotNull (rig.site.GetHoveredZone(), L"the console's center button is under the pointer");

            Assert::IsTrue  (rig.site.OnDragKey (Key (VK_ESCAPE)), L"Escape cancels the drag");
            Assert::IsFalse (rig.site.IsDragging());
            Assert::IsFalse (rig.site.HasDragLayer(),              L"nothing is left to paint");
            Assert::IsTrue  (rig.site.GetDragMarks (rig.theme).empty());

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, console));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   console));

            Assert::IsFalse  (rig.site.IsDragging(),                     L"the moves after it start no drag");
            Assert::AreEqual (before, rig.site.GetPaneLayout().ToText(), L"every pane is where it was");
            Assert::AreEqual (0, rig.changes,                            L"and no change is reported");
        }


        //  A strip the drag hovered had opened a gap for the tab; Escape
        //  closes it.
        TEST_METHOD (EscapeClosesTheGapAHoveredStripOpened)
        {
            Rig             rig;
            DxuiTabGroup  * code = rig.GroupOf (rig.code);
            RECT            tab  = {};



            if (code == nullptr)
            {
                Assert::Fail (L"the rig has a code group");
                return;
            }

            tab = code->GetTabRect (0);

            Assert::IsTrue (TryDragTheToolWindows (rig));

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { tab.left + 4, Center (tab).y }));
            Assert::IsTrue (rig.site.GetStripTargetGroup() >= 0, L"the code's strip opened a gap");

            Assert::IsTrue   (rig.site.OnDragKey (Key (VK_ESCAPE)));
            Assert::AreEqual (-1,       rig.site.GetStripTargetGroup());
            Assert::AreEqual (tab.left, code->GetTabRect (0).left, L"the tab is back where it was");
        }


        //  Only Escape going down cancels, and only a drag the site's own
        //  pointer started: a floating window's drag is ended by its own
        //  move loop.
        TEST_METHOD (EscapeCancelsOnlyThePointersDrag)
        {
            Rig  rig;



            rig.site.BeginDrag (L"stack");
            Assert::IsFalse (rig.site.HasPointerDrag(),            L"a drag begun from elsewhere");
            Assert::IsFalse (rig.site.OnDragKey (Key (VK_ESCAPE)), L"is not the site's to cancel");
            Assert::IsTrue  (rig.site.IsDragging());
            rig.site.CancelDrag();

            Assert::IsTrue  (TryDragTheToolWindows (rig));
            Assert::IsFalse (rig.site.OnDragKey (Key (VK_RETURN)),                       L"another key");
            Assert::IsFalse (rig.site.OnDragKey (Key (VK_ESCAPE, DxuiKeyEventKind::Up)), L"Escape coming up");
            Assert::IsTrue  (rig.site.HasPointerDrag(),                                  L"leave the drag going");
        }


        //  The window losing the mouse with the button still down cancels the
        //  drag, and the release goes to whatever took the mouse, so the title
        //  bar's press is canceled with it. The window releases the capture
        //  itself as the button comes up, with the button already up, which is
        //  not a loss: the drop still lands.
        TEST_METHOD (LosingTheMouseCancelsButAReleaseDoesNot)
        {
            Rig             rig;
            std::wstring    before  = rig.site.GetPaneLayout().ToText();
            POINT           console = {};
            DxuiMouseEvent  hover;



            Assert::IsTrue (TryDragTheToolWindows (rig));

            console = Center (rig.console.GetBounds());
            rig.site.OnMouse         (Mouse (DxuiMouseEventKind::Move, console));
            rig.site.OnDragMouseLost (true);

            Assert::IsFalse  (rig.site.IsDragging(),                     L"a loss with the button down cancels");
            Assert::AreEqual (before, rig.site.GetPaneLayout().ToText());

            hover        = Mouse (DxuiMouseEventKind::Move, console);
            hover.button = DxuiMouseButton::None;

            Assert::IsFalse (rig.site.OnMouse (hover), L"no title bar press is left, so a hover goes to the panes");

            Assert::IsTrue (TryDragTheToolWindows (rig));

            rig.site.OnMouse         (Mouse (DxuiMouseEventKind::Move, console));
            rig.site.OnDragMouseLost (false);
            Assert::IsTrue (rig.site.IsDragging(), L"a release is not a loss");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, console));
            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"console") == std::vector<std::wstring> { L"console", L"regs", L"stack" },
                            L"the release drops the panes");
        }


        //  Off the docked area, over what lies above the site, the cross and
        //  the shade go at once and only the edge guides stay; back over a
        //  pane, its cross shows again.
        TEST_METHOD (LeavingThePanesTakesDownTheCrossAndTheShade)
        {
            Rig    rig (600, 100);
            POINT  code = {};



            Assert::IsTrue (TryDragTheToolWindows (rig));

            code = Center (rig.code.GetBounds());
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, code));

            Assert::IsNotNull (rig.site.GetHoveredZone(),     L"the documents' center button");
            Assert::AreEqual  ((size_t) 5, rig.CountGuides(), L"the edge guides and the documents' cross");
            Assert::IsTrue    (rig.CountShades() > 0,         L"and the shade");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { code.x, 50 }));

            Assert::IsNull   (rig.site.GetHoveredZone(),     L"nothing is targeted over the toolbar");
            Assert::AreEqual ((size_t) 4, rig.CountGuides(), L"the edge guides alone");
            Assert::AreEqual ((size_t) 0, rig.CountShades(), L"and no shade");
            Assert::IsTrue   (rig.site.IsDragging(),         L"the drag goes on");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, code));
            Assert::AreEqual ((size_t) 5, rig.CountGuides(), L"the cross is back");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Leave, code));
            Assert::IsNull   (rig.site.GetHoveredZone(),     L"leaving the window takes them down too");
            Assert::AreEqual ((size_t) 4, rig.CountGuides());
        }


        //  A short site: the documents' large cross reaches above the top of
        //  the site. Its top button there, over what lies above the site, is
        //  not a target, and a release on it docks nothing.
        TEST_METHOD (ACrossPastTheSitesEdgeIsNoTargetThere)
        {
            Rig              rig (160, 100);
            std::wstring     before = rig.site.GetPaneLayout().ToText();
            DxuiTabGroup   * code   = rig.GroupOf (rig.code);
            POINT            origin = {};
            RECT             top    = {};



            if (code == nullptr)
            {
                Assert::Fail (L"the rig has a code group");
                return;
            }

            origin = DxuiDockGuide::GetOrigin (DxuiDockGuideKind::LargeCross, Center (code->GetBounds()), rig.scaler);
            top    = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::DockTop, origin, rig.scaler);

            Assert::IsTrue (Center (top).y < 100, L"the top button reaches above the site");

            Assert::IsTrue (TryDragTheToolWindows (rig));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (code->GetBounds())));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (top)));

            Assert::IsNull   (rig.site.GetHoveredZone(),     L"the button past the site is not hit");
            Assert::AreEqual ((size_t) 4, rig.CountGuides(), L"and the cross is down");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up, Center (top)));
            Assert::AreEqual (before, rig.site.GetPaneLayout().ToText(), L"the release there docks nothing");
        }
    };
}
