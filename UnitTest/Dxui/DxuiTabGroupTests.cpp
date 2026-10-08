#include "Pch.h"

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
            Assert::AreEqual ((long) DxuiTabGroup::kStripDip + 1, rig.a.GetBounds().top, L"below the strip and the line under it");
            Assert::AreEqual ((long) 299, rig.a.GetBounds().bottom, L"inside the outline");
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

            for (const wchar_t * title : { L"Registers", L"Stack", L"Watch" })
            {
                Assert::IsTrue (std::any_of (text.Calls().begin(), text.Calls().end(),
                                             [title] (const RecordedTextCall & call) { return call.text == title; }), title);
            }
        }


        TEST_METHOD (AToolWindowOfOnePaneHasATitleBarAndNoTabs)
        {
            DxuiTabGroup     group;
            MockDxuiControl  a;
            DxuiDpiScaler    scaler;



            group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            group.AddTab  (L"Registers", &a);
            group.Layout  (RECT { 0, 0, 400, 300 }, scaler);

            Assert::AreEqual ((long) DxuiTabGroup::kTitleDip, a.GetBounds().top,    L"below the title bar");
            Assert::AreEqual ((long) 299,                     a.GetBounds().bottom, L"no strip below, inside the outline");
            Assert::AreEqual (-1, group.HitTestTab (POINT { 5, 5 }));
            Assert::IsTrue   (group.IsChromeAt (POINT { 5, 5 }));
            Assert::IsFalse  (group.IsChromeAt (POINT { 5, 290 }));
        }


        TEST_METHOD (AToolWindowOfSeveralPanesHasItsTabsAlongTheBottom)
        {
            Rig   rig;
            RECT  tab = {};



            rig.group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            tab = rig.group.GetTabRect (0);

            Assert::AreEqual ((long) DxuiTabGroup::kTitleDip,            rig.a.GetBounds().top);
            Assert::AreEqual ((long) (300 - DxuiTabGroup::kStripDip - 1), rig.a.GetBounds().bottom, L"above the strip and the line over it");
            Assert::AreEqual ((long) (300 - DxuiTabGroup::kStripDip),     tab.top);
            Assert::AreEqual (1, rig.group.HitTestTab (POINT { rig.group.GetTabRect (1).left + 3, tab.top + 3 }));
        }


        TEST_METHOD (ATitleBarButtonActsOnReleaseOverIt)
        {
            Rig                        rig;
            RECT                       pin    = {};
            int                        clicks = 0;
            DxuiTabGroup::TitleButton  which  = DxuiTabGroup::TitleButton::Menu;
            int                        index  = -1;



            rig.group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            rig.group.SetActive (1);
            rig.group.SetOnTitleButton ([&] (DxuiTabGroup::TitleButton button, int at, POINT) { clicks++; which = button; index = at; });
            pin = rig.group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin);

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, pin.left + 2, pin.top + 2));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   pin.left - 40, pin.top + 2));
            Assert::AreEqual (0, clicks, L"released off the button");

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, pin.left + 2, pin.top + 2));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   pin.left + 2, pin.top + 2));
            Assert::AreEqual (1, clicks);
            Assert::IsTrue   (which == DxuiTabGroup::TitleButton::Pin);
            Assert::AreEqual (1, index, L"for the active pane");
        }


        TEST_METHOD (TheCloseButtonShowsOnlyWhileTheActivePaneCanClose)
        {
            Rig  rig;



            rig.group.SetKind     (DxuiTabGroup::Kind::ToolWindow);
            rig.group.SetCanClose ([] (int index) { return index != 0; });

            Assert::IsTrue  (rig.group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Close).right == 0);

            rig.group.SetActive (2);
            Assert::IsTrue  (rig.group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Close).right > 0);
            Assert::IsTrue  (rig.group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin).right <=
                             rig.group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Close).left, L"the pin moves over for it");
        }


        TEST_METHOD (DraggingATitleBarReportsADragOfTheActivePane)
        {
            Rig  rig;
            int  which = -1;



            rig.group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            rig.group.SetActive (2);
            rig.group.SetOnDragStart ([&] (int index, POINT) { which = index; });

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, 20, 10));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, 60, 60));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   60, 60));
            Assert::AreEqual (2, which);
        }


        TEST_METHOD (ADocumentTabsCloseButtonClosesItsPane)
        {
            Rig   rig;
            RECT  tab    = {};
            int   closed = -1;



            rig.group.SetOnCloseTab ([&] (int index) { closed = index; });
            rig.group.SetCanClose   ([] (int index) { return index != 1; });
            rig.group.Layout (RECT { 0, 0, 400, 300 }, rig.scaler);
            tab = rig.group.GetTabRect (0);

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, tab.right - 12, (tab.top + tab.bottom) / 2));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   tab.right - 12, (tab.top + tab.bottom) / 2));
            Assert::AreEqual (0, closed);
            Assert::AreEqual (0, rig.group.GetActive(), L"a close is not a press on the tab");
        }


        //  A tool window's title bar is the pane's own color, rounded at the
        //  pane's top corners, with no separator under it.
        TEST_METHOD (TitleIsFilledWithTheContentColor)
        {
            DxuiTabGroup          group;
            MockDxuiControl       a;
            DxuiDpiScaler         scaler;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            RECT                  title  = {};
            bool                  filled = false;



            group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            group.AddTab  (L"Registers", &a);
            group.Layout  (RECT { 30, 20, 430, 320 }, scaler);
            group.Paint   (painter, text, theme);
            title = group.GetTitleRect();

            for (const RecordedPaintCall & call : painter.Calls())
            {
                filled = filled || (call.kind == RecordedPaintKind::FillRoundedRect && call.isClipped && call.argb == theme.ContentBackground() &&
                                    call.clip.left == title.left && call.clip.top == title.top && call.clip.right == title.right && call.clip.bottom == title.bottom);
            }

            Assert::IsTrue (filled, L"a rounded fill in the content color, clipped to the title bar");
        }


        //  A title starts at the pane's text inset from its outer edge, the
        //  same at every scale.
        TEST_METHOD (TitleTextStartsAtTheTextInset)
        {
            for (UINT dpi : { 96u, 144u })
            {
                DxuiTabGroup          group;
                MockDxuiControl       a;
                DxuiDpiScaler         scaler;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                bool                  drawn = false;
                float                 x     = 0.0f;

                scaler.SetDpi (dpi);
                group.SetKind (DxuiTabGroup::Kind::ToolWindow);
                group.AddTab  (L"Registers", &a);
                group.Layout  (RECT { 30, 20, 430, 320 }, scaler);
                group.Paint   (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (!drawn && call.kind == RecordedTextKind::DrawString && call.text == L"Registers")
                    {
                        drawn = true;
                        x     = call.x;
                    }
                }

                Assert::IsTrue   (drawn, L"the title is drawn");
                Assert::AreEqual ((float) (30 + DxuiPaneMetrics::GetTextInsetPx (scaler)), x, L"at the text inset");
            }
        }


        //  A document's first tab's label starts where a tool window's title
        //  does, so the two line up down a column of panes: in whole pixels,
        //  so at 106 dpi, where 8 DIP is not a whole pixel, as well.
        TEST_METHOD (TheFirstTabsLabelStartsWhereATitleWould)
        {
            for (UINT dpi : { 96u, 106u, 120u, 144u })
            {
                DxuiTabGroup          group;
                MockDxuiControl       a;
                MockDxuiControl       b;
                DxuiDpiScaler         scaler;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                bool                  drawn = false;
                float                 x     = 0.0f;

                scaler.SetDpi (dpi);
                group.AddTab (L"Registers", &a);
                group.AddTab (L"Stack",     &b);
                group.Layout (RECT { 30, 20, 430, 320 }, scaler);
                group.Paint  (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (!drawn && call.kind == RecordedTextKind::DrawString && call.text == L"Registers")
                    {
                        drawn = true;
                        x     = call.x;
                    }
                }

                Assert::IsTrue   (drawn, L"the first tab's label is drawn");
                Assert::AreEqual ((float) (30 + DxuiPaneMetrics::GetTextInsetPx (scaler)), x, L"where a title starts");
            }
        }


        //  The outline is the frame's, drawn after the siblings: Paint draws
        //  none of it, and PaintFrame draws it in the border color, or in the
        //  focus accent while the group has the focused look.
        TEST_METHOD (PaintDrawsNoOutlinePaintFrameDoes)
        {
            Rig                   rig;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            auto                  countIn = [&] (uint32_t argb)
            {
                return std::count_if (painter.Calls().begin(), painter.Calls().end(),
                                      [argb] (const RecordedPaintCall & call) { return call.argb == argb; });
            };



            Assert::AreNotEqual (theme.Border(), theme.FocusAccent(), L"the two looks differ");

            rig.group.Paint (painter, text, theme);
            Assert::AreEqual ((ptrdiff_t) 0, countIn (theme.Border()),      L"Paint draws no outline");
            Assert::AreEqual ((ptrdiff_t) 0, countIn (theme.FocusAccent()), L"in either color");

            painter.Reset();
            rig.group.PaintFrame (painter, theme);
            Assert::IsTrue   (countIn (theme.Border()) > 0,                 L"PaintFrame draws it in the border color");

            painter.Reset();
            rig.group.SetFocusedLook (true);
            rig.group.PaintFrame (painter, theme);
            Assert::IsTrue   (countIn (theme.FocusAccent()) > 0,            L"and in the focus accent while focused");
            Assert::AreEqual ((ptrdiff_t) 0, countIn (theme.Border()),      L"and not in the border color then");
        }


        //  Visual Studio's sizes, 31 px at 125%: the tab band of either kind
        //  and a tool window's title bar, and title bar buttons as many
        //  pixels square, one pitch apart, the close button's right edge one
        //  line inside the pane's.
        TEST_METHOD (TheBandTitleAndTitleButtonsTakeVisualStudiosSizes)
        {
            constexpr UINT  kDpis[]    = { 96, 120, 144 };
            constexpr long  kBands[]   = { 25, 31, 38 };
            constexpr long  kButtons[] = { 24, 30, 36 };
            constexpr long  kRightPx   = 400;



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiTabGroup     document;
                DxuiTabGroup     tool;
                MockDxuiControl  a;
                MockDxuiControl  b;
                MockDxuiControl  c;
                MockDxuiControl  d;
                DxuiDpiScaler    scaler;
                RECT             strip    = {};
                RECT             toolBand = {};
                RECT             title    = {};
                RECT             close    = {};
                RECT             pin      = {};
                RECT             menu     = {};
                std::wstring     at       = std::format (L"{} dpi", kDpis[i]);

                scaler.SetDpi   (kDpis[i]);
                document.AddTab (L"Registers", &a);
                document.AddTab (L"Stack",     &b);
                document.Layout (RECT { 0, 0, kRightPx, 300 }, scaler);
                tool.SetKind    (DxuiTabGroup::Kind::ToolWindow);
                tool.AddTab     (L"Registers", &c);
                tool.AddTab     (L"Stack",     &d);
                tool.Layout     (RECT { 0, 0, kRightPx, 300 }, scaler);

                strip    = document.GetStripRect();
                toolBand = tool.GetStripRect();
                title    = tool.GetTitleRect();
                close    = tool.GetTitleButtonRect (DxuiTabGroup::TitleButton::Close);
                pin      = tool.GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin);
                menu     = tool.GetTitleButtonRect (DxuiTabGroup::TitleButton::Menu);

                Assert::AreEqual (kBands[i],   strip.bottom - strip.top,       (L"a document's band, " + at).c_str());
                Assert::AreEqual (kBands[i],   toolBand.bottom - toolBand.top, (L"a tool window's band, " + at).c_str());
                Assert::AreEqual (kBands[i],   title.bottom - title.top,       (L"a tool window's title bar, " + at).c_str());
                Assert::AreEqual (kButtons[i], close.right - close.left,       (L"a title bar button's width, " + at).c_str());
                Assert::AreEqual (kButtons[i], close.bottom - close.top,       (L"and its height, " + at).c_str());
                Assert::AreEqual (kButtons[i], close.left - pin.left,          (L"the pitch, " + at).c_str());
                Assert::AreEqual (kButtons[i], pin.left - menu.left,           (L"the pitch, " + at).c_str());
                Assert::AreEqual (kRightPx - DxuiPaneMetrics::GetLinePx (scaler), close.right, (L"the close button inside the outline, " + at).c_str());
            }
        }


        //  A hovered title bar button is washed in a rounded square 3 DIP in
        //  from its edges, with a 4-DIP radius.
        TEST_METHOD (AHoveredTitleButtonIsWashedInARoundedSquare)
        {
            constexpr UINT  kDpis[]   = { 96, 120, 144 };
            constexpr long  kInsets[] = { 3, 4, 5 };
            constexpr long  kRadii[]  = { 4, 5, 6 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiTabGroup          group;
                MockDxuiControl       a;
                MockDxuiControl       b;
                DxuiDpiScaler         scaler;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                RECT                  pin    = {};
                uint32_t              hover  = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
                size_t                washes = 0;
                RecordedPaintCall     wash;
                std::wstring          at     = std::format (L"{} dpi", kDpis[i]);

                scaler.SetDpi  (kDpis[i]);
                group.SetKind  (DxuiTabGroup::Kind::ToolWindow);
                group.AddTab   (L"Registers", &a);
                group.AddTab   (L"Stack",     &b);
                group.Layout   (RECT { 0, 0, 400, 300 }, scaler);
                pin = group.GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin);
                group.OnMouse  (Mouse (DxuiMouseEventKind::Move, (pin.left + pin.right) / 2, (pin.top + pin.bottom) / 2));
                group.Paint    (painter, text, theme);

                for (const RecordedPaintCall & call : painter.Calls())
                {
                    if (call.kind == RecordedPaintKind::FillRoundedRect && call.argb == hover)
                    {
                        wash = call;
                        washes++;
                    }
                }

                Assert::AreEqual ((size_t) 1, washes, (L"one wash, " + at).c_str());
                Assert::AreEqual ((float) (pin.left + kInsets[i]),                    wash.x,      (L"in from the left, " + at).c_str());
                Assert::AreEqual ((float) (pin.top  + kInsets[i]),                    wash.y,      (L"in from the top, " + at).c_str());
                Assert::AreEqual ((float) (pin.right - pin.left - 2 * kInsets[i]),    wash.width,  (L"in from both sides, " + at).c_str());
                Assert::AreEqual ((float) (pin.bottom - pin.top - 2 * kInsets[i]),    wash.height, (L"in from top and bottom, " + at).c_str());
                Assert::AreEqual ((float) kRadii[i],                                  wash.radius, (L"the radius, " + at).c_str());
            }
        }
    };
}
