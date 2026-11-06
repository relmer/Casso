#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteAutoHideTests
//
//  An auto-hidden pane as Visual Studio shows one: a press on its edge tab
//  slides it out, a hover never does, and the focus leaving it for another
//  pane slides it back. Out, it has a pane's rounded frame, in the focus
//  accent only while it has the focus.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteAutoHideTests
{
    //  Code above the console and the registers to their right, with the
    //  console hidden against the bottom edge.
    struct Rig
    {
        DxuiDockSite          site;
        MockDxuiControl       code;
        MockDxuiControl       regs;
        MockDxuiControl       console;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        static constexpr int64_t  kStartMs = 5000;

        int64_t               nowMs = kStartMs;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetPaneLayout (layout);
            site.Layout        (RECT { 0, 0, 1000, 600 }, scaler);

            (void) site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom);
            site.Relayout();
        }

        void Send (DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;
            (void) site.OnMouse (ev);
        }

        POINT GetTabCenter() const
        {
            RECT  tab = site.GetEdgeTabRect (L"console");

            return POINT { (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2 };
        }

        //  Runs the item of a pane's Dock To menu with the given label.
        bool RunDockTo (const std::wstring & pane, const std::wstring & label)
        {
            for (const DxuiDockSite::MenuItem & item : site.GetDockToMenu (pane))
            {
                if (item.label == label)
                {
                    return item.action();
                }
            }

            return false;
        }

        //  The group whose tabs include a pane's control, or null.
        DxuiTabGroup * GroupOf (MockDxuiControl & content)
        {
            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                if (site.GetGroup (i)->IndexOf (&content) >= 0)
                {
                    return site.GetGroup (i);
                }
            }

            return nullptr;
        }

        //  A clock the test sets, and animations on regardless of the host.
        void UseTestClock()
        {
            site.SetClock             ([this] { return nowMs; });
            site.SetAnimationsEnabled (true);
        }


        //  The slid-out pane's paint calls of one kind in one color.
        size_t CountSlidCalls (RecordedPaintKind kind, uint32_t argb)
        {
            painter.Reset();
            site.PaintSlidOver (painter, text, theme);

            return (size_t) std::count_if (painter.Calls().begin(), painter.Calls().end(), [kind, argb] (const RecordedPaintCall & call)
            {
                return call.kind == kind && call.argb == argb;
            });
        }
    };



    TEST_CLASS (DxuiDockSiteAutoHideTests)
    {
    public:

        //  A new title for a hidden pane sizes its edge tab again, so a
        //  longer title is not clipped to the old tab.
        TEST_METHOD (ANewTitleSizesTheEdgeTabAgain)
        {
            Rig   rig;
            RECT  before = rig.site.GetEdgeTabRect (L"console");
            RECT  after  = {};



            rig.site.SetTitle (L"console", L"Console with a much longer title");
            after = rig.site.GetEdgeTabRect (L"console");

            Assert::IsTrue ((after.right - after.left) + (after.bottom - after.top) > (before.right - before.left) + (before.bottom - before.top),
                            L"the tab grows with its title");
        }


        //  The pointer resting on the tab, or moving about over it, lights
        //  the tab and nothing more.
        TEST_METHOD (AHoverNeverSlidesThePaneOut)
        {
            Rig    rig;
            POINT  tab = rig.GetTabCenter();



            for (long dx = -20; dx <= 20; dx += 10)
            {
                rig.Send (DxuiMouseEventKind::Move, POINT { tab.x + dx, tab.y });
                Assert::IsTrue (rig.site.GetSlidPane().empty(), L"a hover does not slide the pane out");
            }

            Assert::AreEqual (std::wstring (L"console"), rig.site.GetHoveredEdgeTab(), L"it lights the tab");
            Assert::IsFalse  (rig.console.IsVisible(),                                 L"and the pane stays hidden");

            rig.Send (DxuiMouseEventKind::Down, tab);
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane(), L"a press slides it out");
        }


        TEST_METHOD (TheFocusLeavingTheSlidPaneSlidesItBack)
        {
            Rig  rig;



            rig.site.SetFocusedPane (L"code");
            rig.Send (DxuiMouseEventKind::Down, rig.GetTabCenter());
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane(), L"out on the press");

            rig.site.SetFocusedPane (L"console");
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane(), L"the focus coming to it keeps it out");

            rig.site.SetFocusedPane (L"regs");
            Assert::IsTrue  (rig.site.GetSlidPane().empty(), L"the focus going to another pane slides it back");
            Assert::IsFalse (rig.console.IsVisible());
        }


        //  A press on the tab does not move the focus, so a pane slid out while
        //  another pane holds it stays out until the focus moves: to another
        //  pane, or out of the site altogether, as to a floating window.
        TEST_METHOD (APaneSlidOutWhileAnotherHasTheFocusStaysOutUntilTheFocusMoves)
        {
            Rig  rig;



            rig.site.SetFocusedPane (L"code");
            rig.Send (DxuiMouseEventKind::Down, rig.GetTabCenter());

            rig.site.SetFocusedPane (L"code");
            Assert::AreEqual (std::wstring (L"console"), rig.site.GetSlidPane(), L"the focus staying where it was keeps it out");

            rig.site.SetFocusedPane (L"");
            Assert::IsTrue (rig.site.GetSlidPane().empty(), L"the focus leaving the site slides it back");
        }


        //  Out, the pane has the rounded frame every pane has: quarter rings
        //  at its corners, in the border color until it has the focus, and in
        //  the focus accent while it does.
        TEST_METHOD (TheSlidPanesFrameIsAccentedOnlyWhileItHasTheFocus)
        {
            Rig       rig;
            uint32_t  accent = rig.theme.FocusAccent();
            uint32_t  border = rig.theme.Border();



            rig.site.SetFocusedPane (L"code");
            rig.Send (DxuiMouseEventKind::Down, rig.GetTabCenter());

            Assert::AreEqual ((size_t) 0, rig.CountSlidCalls (RecordedPaintKind::FillRect,           accent), L"no accent without the focus");
            Assert::AreEqual ((size_t) 0, rig.CountSlidCalls (RecordedPaintKind::OutlineRoundedRect, accent));
            Assert::IsTrue   (rig.CountSlidCalls (RecordedPaintKind::FillRect,           border) > 0, L"the outline's runs in the border color");
            Assert::IsTrue   (rig.CountSlidCalls (RecordedPaintKind::OutlineRoundedRect, border) > 0, L"and its rounded corners");

            rig.site.SetFocusedPane (L"console");

            Assert::IsTrue   (rig.CountSlidCalls (RecordedPaintKind::FillRect,           accent) > 0, L"with the focus, the runs in the accent");
            Assert::IsTrue   (rig.CountSlidCalls (RecordedPaintKind::OutlineRoundedRect, accent) > 0, L"and the rounded corners");
            Assert::AreEqual ((size_t) 0, rig.CountSlidCalls (RecordedPaintKind::OutlineRoundedRect, border), L"none left in the border color");
        }


        //  Pinning removes the pane from its group at once, and a rect
        //  shrinks from the group into the pane's edge tab, drawn in the drag
        //  layer. One more frame is due as the morph ends, to clear it.
        TEST_METHOD (PinningShrinksARectFromThePaneIntoItsEdgeTab)
        {
            Rig   rig;
            RECT  from = {};
            RECT  to   = {};
            RECT  at   = {};
            bool  drew = false;



            rig.UseTestClock();
            Assert::IsTrue (rig.site.TryGetPaneGroupRect (L"regs", from));
            Assert::IsTrue (rig.RunDockTo (L"regs", L"Auto hide"));

            to = rig.site.GetEdgeTabRect (L"regs");
            Assert::IsTrue (rig.site.GetPaneLayout().IsAutoHidden (L"regs"), L"the layout changes at once");
            Assert::IsTrue (rig.site.IsMorphing());

            at = rig.site.GetMorphRect();
            Assert::AreEqual (from.left,  at.left,  L"the rect starts at the pane's group");
            Assert::AreEqual (from.right, at.right);

            rig.painter.Reset();
            Assert::IsTrue (rig.site.HasDragLayer());
            rig.site.PaintDragLayer (rig.painter, rig.text, rig.theme);

            for (const RecordedPaintCall & call : rig.painter.Calls())
            {
                drew = drew || (call.kind == RecordedPaintKind::FillRect && call.argb == rig.theme.DockPreview());
            }

            Assert::IsTrue (drew, L"in the drop preview's shade");

            rig.nowMs = Rig::kStartMs + DxuiRectMorph::kDurationMs - 1;
            at        = rig.site.GetMorphRect();
            Assert::AreEqual (to.left,   at.left,   L"and ends at the edge tab");
            Assert::AreEqual (to.top,    at.top);
            Assert::AreEqual (to.right,  at.right);
            Assert::AreEqual (to.bottom, at.bottom);
            Assert::IsTrue   (rig.site.TickAnimation(), L"a frame is due mid-morph");

            rig.nowMs = Rig::kStartMs + DxuiRectMorph::kDurationMs;
            Assert::IsFalse  (rig.site.IsMorphing());
            Assert::IsFalse  (rig.site.HasDragLayer());
            Assert::IsTrue   (rig.site.TickAnimation(), L"and one as it ends");
            Assert::IsFalse  (rig.site.TickAnimation(), L"and no more");
        }


        //  Unpinning docks the pane at once, and a rect grows from its edge
        //  tab to its group.
        TEST_METHOD (UnpinningGrowsARectFromTheEdgeTabToThePane)
        {
            Rig   rig;
            RECT  from = rig.site.GetEdgeTabRect (L"console");
            RECT  to   = {};
            RECT  at   = {};



            rig.UseTestClock();
            Assert::IsTrue (rig.RunDockTo (L"console", L"Dock"));
            Assert::IsTrue (rig.site.GetPaneLayout().IsDocked (L"console"), L"the layout changes at once");
            Assert::IsTrue (rig.site.TryGetPaneGroupRect (L"console", to));

            at = rig.site.GetMorphRect();
            Assert::AreEqual (from.left, at.left, L"the rect starts at the edge tab");
            Assert::AreEqual (from.top,  at.top);

            rig.nowMs = Rig::kStartMs + DxuiRectMorph::kDurationMs - 1;
            at        = rig.site.GetMorphRect();
            Assert::AreEqual (to.left,   at.left, L"and ends at the pane's group");
            Assert::AreEqual (to.bottom, at.bottom);
        }


        //  With animations off nothing morphs, and the layout changes as it
        //  does with them on.
        TEST_METHOD (WithAnimationsOffPinningDoesNotMorph)
        {
            Rig  rig;



            rig.site.SetAnimationsEnabled (false);
            Assert::IsTrue  (rig.RunDockTo (L"regs", L"Auto hide"));
            Assert::IsTrue  (rig.site.GetPaneLayout().IsAutoHidden (L"regs"));
            Assert::IsFalse (rig.site.IsMorphing());
            Assert::IsFalse (rig.site.HasDragLayer());
            Assert::IsFalse (rig.site.TickAnimation());
        }


        //  A tab of a group with other tabs, pinned, shrinks from its tab
        //  rather than from the whole group.
        TEST_METHOD (PinningATabOfAGroupStartsFromTheTab)
        {
            Rig             rig;
            DxuiTabGroup  * group = nullptr;
            RECT            tab   = {};
            RECT            at    = {};



            (void) rig.site.EditPaneLayout().TabWithAt (L"regs", L"code", 0);
            rig.site.Relayout();
            rig.UseTestClock();

            group = rig.GroupOf (rig.regs);
            Assert::IsNotNull (group, L"the registers share the code's group");
            tab = group->GetTabRect (group->IndexOf (&rig.regs));

            Assert::IsTrue   (rig.RunDockTo (L"regs", L"Auto hide"));
            at = rig.site.GetMorphRect();
            Assert::AreEqual (tab.left,   at.left,   L"the rect starts at the tab");
            Assert::AreEqual (tab.top,    at.top);
            Assert::AreEqual (tab.right,  at.right);
            Assert::AreEqual (tab.bottom, at.bottom);
        }


        //  Unpinned into a group with other tabs, the rect ends at the tab the
        //  pane takes, and the pane is the active tab with the focus outline
        //  on its group, as after a press on its tab.
        TEST_METHOD (UnpinningIntoAGroupEndsAtTheTabAndFocusesThePane)
        {
            Rig             rig;
            DxuiTabGroup  * group     = nullptr;
            RECT            tab       = {};
            RECT            at        = {};
            std::wstring    requested;



            (void) rig.site.EditPaneLayout().TabWithAt (L"regs", L"code", 0);
            rig.site.Relayout();
            rig.site.SetFocusedPane      (L"code");
            rig.site.SetOnFocusRequested ([&requested] (const std::wstring & pane) { requested = pane; });
            Assert::IsTrue (rig.RunDockTo (L"regs", L"Auto hide"));

            rig.UseTestClock();
            Assert::IsTrue (rig.RunDockTo (L"regs", L"Dock"));

            group = rig.GroupOf (rig.regs);
            Assert::IsNotNull (group);
            tab = group->GetTabRect (group->IndexOf (&rig.regs));

            rig.nowMs = Rig::kStartMs + DxuiRectMorph::kDurationMs - 1;
            at        = rig.site.GetMorphRect();
            Assert::AreEqual (tab.left,   at.left,   L"the rect ends at the tab");
            Assert::AreEqual (tab.right,  at.right);
            Assert::AreEqual (tab.bottom, at.bottom);

            Assert::IsTrue   (group->GetActiveContent() == &rig.regs, L"the pane is the active tab");
            Assert::IsTrue   (group->HasFocusedLook(),                L"its group has the focus outline");
            Assert::AreEqual (std::wstring (L"regs"), requested,      L"and the focus handler is called for it");
        }


        //  A pane slid out when it is unpinned starts from where it lies, not
        //  from its edge tab.
        TEST_METHOD (UnpinningASlidOutPaneStartsFromTheSlidOutPane)
        {
            Rig   rig;
            RECT  slid = {};
            RECT  at   = {};



            rig.Send (DxuiMouseEventKind::Down, rig.GetTabCenter());
            slid = rig.site.GetSlidRect();
            Assert::IsTrue (slid.right > slid.left, L"the console is out");

            rig.UseTestClock();
            Assert::IsTrue (rig.RunDockTo (L"console", L"Dock"));

            at = rig.site.GetMorphRect();
            Assert::AreEqual (slid.left,   at.left,  L"the rect starts at the slid-out pane");
            Assert::AreEqual (slid.top,    at.top);
            Assert::AreEqual (slid.right,  at.right);
            Assert::AreEqual (slid.bottom, at.bottom);
        }


        //  The first tab of a group, pinned and then unpinned, is the first
        //  tab again rather than the last.
        TEST_METHOD (UnpinningPutsTheTabBackInItsPlace)
        {
            Rig  rig;



            (void) rig.site.EditPaneLayout().TabWithAt (L"regs", L"code", 0);
            rig.site.Relayout();
            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"code") == std::vector<std::wstring> { L"regs", L"code" });

            Assert::IsTrue (rig.RunDockTo (L"regs", L"Auto hide"));
            Assert::IsTrue (rig.RunDockTo (L"regs", L"Dock"));

            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"code") == std::vector<std::wstring> { L"regs", L"code" }, L"first again");
        }
    };
}
