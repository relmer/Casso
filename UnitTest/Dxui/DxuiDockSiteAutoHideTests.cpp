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
    };
}
