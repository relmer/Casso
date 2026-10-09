#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "Ui/Chrome/CassoTheme.h"

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



    //  A label other than the focused selection differs from the foreground
    //  and reads at 4.5:1 on the band, and a hovered tab shows on the band.
    static void AssertTabInksRead (const IDxuiTheme & theme, const wchar_t * name)
    {
        constexpr float  kMinContrast = 4.5f;
        uint32_t         band         = theme.PaneBand();
        uint32_t         label        = DxuiTabStrip::GetLabelInk (theme, false);
        uint32_t         hover        = DxuiColor::Composite (DxuiTabStrip::GetTabHoverFill (theme), band);



        Assert::AreNotEqual (theme.Foreground(), label, name);
        Assert::IsTrue      (DxuiColor::ComputeContrastRatio (label, band) >= kMinContrast, name);
        Assert::AreNotEqual (band, hover, name);
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

            Assert::AreEqual ((long) DxuiTabGroup::kTitleDip,             rig.a.GetBounds().top);
            Assert::AreEqual ((long) (300 - DxuiTabGroup::kStripDip),     rig.a.GetBounds().bottom, L"above the line and the strip, kStripDip deep together");
            Assert::AreEqual ((long) (300 - DxuiTabGroup::kStripDip + 1), tab.top, L"the tabs below the line");
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


        //  A title starts at the pane's text inset from its outer edge: the
        //  outline and 8 DIP more, 9, 11 and 14 px at 100%, 125% and 150%,
        //  where Visual Studio starts its titles.
        TEST_METHOD (TitleTextStartsAtTheTextInset)
        {
            constexpr UINT  kDpis[]   = { 96, 120, 144 };
            constexpr long  kInsets[] = { 9, 11, 14 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                UINT                  dpi   = kDpis[i];
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
                Assert::AreEqual ((int) kInsets[i],           DxuiPaneMetrics::GetTextInsetPx (scaler), L"the text inset");
                Assert::AreEqual ((float) (30 + kInsets[i]), x,                                          L"at the text inset");
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


        //  Visual Studio's sizes, 31 px at 125%: a document's tab band, a
        //  tool window's tab band with the line over it, and a tool window's
        //  title bar; and title bar buttons, below the title bar's outline,
        //  as many pixels square, one pitch apart, the close button's right
        //  edge one line inside the pane's.
        TEST_METHOD (TheBandTitleAndTitleButtonsTakeVisualStudiosSizes)
        {
            constexpr UINT  kDpis[]    = { 96, 120, 144 };
            constexpr long  kBands[]   = { 25, 31, 38 };
            constexpr long  kLines[]   = { 1, 1, 2 };
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

                Assert::AreEqual (kBands[i],              strip.bottom - strip.top,       (L"a document's band, " + at).c_str());
                Assert::AreEqual (kBands[i] - kLines[i],  toolBand.bottom - toolBand.top, (L"a tool window's band, " + at).c_str());
                Assert::AreEqual (300 - kBands[i],        tool.GetBodyRect().bottom,      (L"and the line over it, " + at).c_str());
                Assert::AreEqual (kBands[i],              title.bottom - title.top,       (L"a tool window's title bar, " + at).c_str());
                Assert::AreEqual (kButtons[i],            close.right - close.left,       (L"a title bar button's width, " + at).c_str());
                Assert::AreEqual (kButtons[i],            close.bottom - close.top,       (L"and its height, " + at).c_str());
                Assert::AreEqual (kLines[i],              close.top,                      (L"below the title bar's outline, " + at).c_str());
                Assert::AreEqual (kButtons[i],            close.left - pin.left,          (L"the pitch, " + at).c_str());
                Assert::AreEqual (kButtons[i],            pin.left - menu.left,           (L"the pitch, " + at).c_str());
                Assert::AreEqual (kRightPx - kLines[i],   close.right,                    (L"the close button inside the outline, " + at).c_str());
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


        //  A title bar's pin and close glyphs are a document tab's, the pin at
        //  10.67 DIP and the close at 14, so their ink is 16 px across at
        //  150%, and the menu's chevron is 8 DIP, 12 px across at 150%.
        TEST_METHOD (TheTitleBarsGlyphsAreTheTabsGlyphs)
        {
            constexpr UINT   kDpis[]       = { 96, 120, 144 };
            constexpr float  kPinPx[]      = { 10.67f, 13.34f, 16.0f };
            constexpr float  kClosePx[]    = { 14.0f, 17.5f, 21.0f };
            constexpr float  kMenuPx[]     = { 8.0f, 10.0f, 12.0f };
            constexpr float  kToleranceDip = 0.01f;



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiTabGroup          group;
                MockDxuiControl       a;
                MockDxuiControl       b;
                DxuiDpiScaler         scaler;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                size_t                glyphs = 0;
                float                 pin    = 0.0f;
                float                 close  = 0.0f;
                float                 menu   = 0.0f;
                std::wstring          at     = std::format (L"{} dpi", kDpis[i]);

                scaler.SetDpi (kDpis[i]);
                group.SetKind (DxuiTabGroup::Kind::ToolWindow);
                group.AddTab  (L"Registers", &a);
                group.AddTab  (L"Stack",     &b);
                group.Layout  (RECT { 0, 0, 400, 300 }, scaler);
                group.Paint   (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    glyphs += (call.text == s_kpszMdl2Pinned || call.text == s_kpszMdl2Cancel || call.text == s_kpszMdl2ChevronDown) ? 1 : 0;
                    pin     = (call.text == s_kpszMdl2Pinned)      ? call.fontSizeDip : pin;
                    close   = (call.text == s_kpszMdl2Cancel)      ? call.fontSizeDip : close;
                    menu    = (call.text == s_kpszMdl2ChevronDown) ? call.fontSizeDip : menu;
                }

                Assert::AreEqual ((size_t) 3, glyphs, (L"the title bar's three glyphs and no tab's, " + at).c_str());
                Assert::AreEqual (kPinPx[i],   pin,   kToleranceDip, (L"the pin, " + at).c_str());
                Assert::AreEqual (kClosePx[i], close, kToleranceDip, (L"the close button, " + at).c_str());
                Assert::AreEqual (kMenuPx[i],  menu,  kToleranceDip, (L"the menu, " + at).c_str());
            }
        }


        //  Every document tab keeps room for its pin and close button, and a
        //  tool window's tabs have neither, so no tab's width changes as it
        //  is selected or comes under the pointer, in either kind of group.
        TEST_METHOD (NeitherHoverNorSelectionChangesATabsWidth)
        {
            for (DxuiTabGroup::Kind kind : { DxuiTabGroup::Kind::Document, DxuiTabGroup::Kind::ToolWindow })
            {
                Rig                rig;
                std::vector<long>  widths;
                RECT               tab = {};

                rig.group.SetKind          (kind);
                rig.group.SetOnCloseTab    ([] (int) {});
                rig.group.SetOnTitleButton ([] (DxuiTabGroup::TitleButton, int, POINT) {});
                rig.group.Layout           (RECT { 0, 0, 400, 300 }, rig.scaler);

                for (int i = 0; i < 3; i++)
                {
                    widths.push_back (rig.group.GetTabRect (i).right - rig.group.GetTabRect (i).left);
                }

                rig.group.SetActive (2);
                tab = rig.group.GetTabRect (1);
                rig.group.OnMouse   (Mouse (DxuiMouseEventKind::Move, (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2));

                for (int i = 0; i < 3; i++)
                {
                    Assert::AreEqual (widths[(size_t) i], rig.group.GetTabRect (i).right - rig.group.GetTabRect (i).left, L"the same width");
                }
            }
        }


        //  A document tab's pin acts as the title bar's pin does, for the
        //  tab's own pane, without selecting the tab. It is the second 24-px
        //  square from the tab's right end.
        TEST_METHOD (ATabsPinActsAsTheTitleBarsPin)
        {
            Rig                        rig;
            RECT                       tab    = {};
            int                        clicks = 0;
            DxuiTabGroup::TitleButton  which  = DxuiTabGroup::TitleButton::Menu;
            int                        index  = -1;
            long                       x      = 0;
            long                       y      = 0;



            rig.group.SetOnTitleButton ([&] (DxuiTabGroup::TitleButton button, int at, POINT) { clicks++; which = button; index = at; });
            rig.group.Layout           (RECT { 0, 0, 400, 300 }, rig.scaler);

            tab = rig.group.GetTabRect (1);
            x   = tab.right - 1 - 36;
            y   = (tab.top + tab.bottom) / 2;

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, x, y));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, x, y));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   x, y));

            Assert::AreEqual (1, clicks);
            Assert::IsTrue   (which == DxuiTabGroup::TitleButton::Pin);
            Assert::AreEqual (1, index,                  L"for the tab's pane");
            Assert::AreEqual (0, rig.group.GetActive(), L"which stays unselected");
        }


        //  A tool window's tabs show no pin or close button, as Visual
        //  Studio's do not, with both handlers set: nothing is reported over
        //  where a document tab's would be, and a press there selects the
        //  tab without pinning or closing anything.
        TEST_METHOD (AToolWindowsTabsHaveNoPinOrClose)
        {
            Rig                        rig;
            RECT                       tab    = {};
            RECT                       rect   = {};
            int                        clicks = 0;
            int                        closes = 0;
            int                        index  = -1;
            DxuiTabGroup::TitleButton  button = DxuiTabGroup::TitleButton::Menu;
            long                       y      = 0;



            rig.group.SetKind          (DxuiTabGroup::Kind::ToolWindow);
            rig.group.SetOnCloseTab    ([&] (int) { closes++; });
            rig.group.SetOnTitleButton ([&] (DxuiTabGroup::TitleButton, int, POINT) { clicks++; });
            rig.group.Layout           (RECT { 0, 0, 400, 300 }, rig.scaler);

            tab = rig.group.GetTabRect (1);
            y   = (tab.top + tab.bottom) / 2;

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Move, tab.right - 1 - 36, y));

            Assert::IsFalse (rig.group.TryGetTabButtonAt (POINT { tab.right - 1 - 36, y }, button, index, rect), L"no pin on the hovered tab");
            Assert::IsFalse (rig.group.TryGetTabButtonAt (POINT { tab.right - 1 - 12, y }, button, index, rect), L"and no close button");

            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Down, tab.right - 1 - 12, y));
            rig.group.OnMouse (Mouse (DxuiMouseEventKind::Up,   tab.right - 1 - 12, y));

            Assert::AreEqual (0, clicks,                 L"nothing pinned");
            Assert::AreEqual (0, closes,                 L"nothing closed");
            Assert::AreEqual (1, rig.group.GetActive(), L"the press selects the tab");
        }


        //  The selected tab's pin and close button are reported as the title
        //  bar buttons they stand for, so the host shows a title bar button's
        //  tip; the label is no button, and nor is an unshown button.
        TEST_METHOD (ATabsButtonsAreReportedAsTitleBarButtons)
        {
            Rig                        rig;
            RECT                       tab    = {};
            RECT                       rect   = {};
            int                        index  = -1;
            DxuiTabGroup::TitleButton  button = DxuiTabGroup::TitleButton::Menu;
            long                       y      = 0;



            rig.group.SetOnCloseTab    ([] (int) {});
            rig.group.SetOnTitleButton ([] (DxuiTabGroup::TitleButton, int, POINT) {});
            rig.group.Layout           (RECT { 0, 0, 400, 300 }, rig.scaler);

            tab = rig.group.GetTabRect (0);
            y   = (tab.top + tab.bottom) / 2;

            Assert::IsTrue   (rig.group.TryGetTabButtonAt (POINT { tab.right - 1 - 36, y }, button, index, rect));
            Assert::IsTrue   (button == DxuiTabGroup::TitleButton::Pin);
            Assert::AreEqual (0, index);
            Assert::IsTrue   (rig.group.TryGetTabButtonAt (POINT { tab.right - 1 - 12, y }, button, index, rect));
            Assert::IsTrue   (button == DxuiTabGroup::TitleButton::Close);
            Assert::AreEqual (tab.right - 1, rect.right);
            Assert::IsFalse  (rig.group.TryGetTabButtonAt (POINT { tab.left + 12, y }, button, index, rect), L"the label");

            tab = rig.group.GetTabRect (2);
            Assert::IsFalse  (rig.group.TryGetTabButtonAt (POINT { tab.right - 1 - 12, y }, button, index, rect), L"a tab neither selected nor hovered");
        }


        //  A tool window's tab labels start at the pane's text inset from
        //  each tab's left edge, as a document's do.
        TEST_METHOD (AToolWindowsTabLabelsStartAtTheTextInset)
        {
            Rig                   rig;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            float                 x = -1.0f;



            rig.group.SetKind (DxuiTabGroup::Kind::ToolWindow);
            rig.group.Paint   (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                x = (call.text == L"Stack") ? call.x : x;
            }

            Assert::AreEqual ((float) (rig.group.GetTabRect (1).left + DxuiPaneMetrics::GetTextInsetPx (rig.scaler)), x);
        }


        //  In a document group, the selected tab's label and glyphs are in
        //  the full foreground while the group has the focused look, and
        //  every other label, the selected one's too without it, a step toward
        //  the muted foreground.
        TEST_METHOD (TheFocusedGroupsSelectedTabIsInTheFullForeground)
        {
            for (bool focused : { false, true })
            {
                Rig                   rig;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                uint32_t              other    = DxuiTabStrip::GetLabelInk (theme, false);
                uint32_t              selected = focused ? theme.Foreground() : other;
                uint32_t              glyph    = focused ? theme.Foreground() : DxuiTabStrip::GetGlyphInk (theme, false);
                size_t                labels   = 0;
                size_t                glyphs   = 0;

                Assert::AreNotEqual (theme.Foreground(), other, L"the two inks differ");

                rig.group.SetOnCloseTab  ([] (int) {});
                rig.group.Layout         (RECT { 0, 0, 400, 300 }, rig.scaler);
                rig.group.SetFocusedLook (focused);
                rig.group.Paint          (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (call.text == L"Registers")
                    {
                        labels++;
                        Assert::AreEqual (selected, call.argb, L"the selected tab's label");
                    }
                    else if (call.text == L"Stack" || call.text == L"Watch")
                    {
                        labels++;
                        Assert::AreEqual (other, call.argb, L"another tab's label");
                    }
                    else if (call.text == s_kpszMdl2Cancel)
                    {
                        glyphs++;
                        Assert::AreEqual (glyph, call.argb, L"the selected tab's close glyph");
                    }
                }

                Assert::AreEqual ((size_t) 3, labels, L"every tab's label");
                Assert::AreEqual ((size_t) 1, glyphs, L"and the selected tab's close glyph, the one tab with buttons");
            }
        }


        //  A tool window's title and its menu, pin and close glyphs take the
        //  inks of a selected tab: the full foreground while the group has the
        //  focused look, and otherwise a step toward the muted foreground for
        //  the title and a shade under that for the glyphs.
        TEST_METHOD (TheToolWindowsTitleTakesTheSelectedTabsInks)
        {
            for (bool focused : { false, true })
            {
                Rig                   rig;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                RECT                  title  = {};
                uint32_t              label  = DxuiTabStrip::GetLabelInk (theme, focused);
                uint32_t              glyph  = DxuiTabStrip::GetGlyphInk (theme, focused);
                size_t                titles = 0;
                size_t                glyphs = 0;
                std::wstring          look   = focused ? L"focused" : L"unfocused";

                Assert::AreNotEqual (DxuiTabStrip::GetLabelInk (theme, true), DxuiTabStrip::GetLabelInk (theme, false), L"the two label inks differ");
                Assert::AreNotEqual (DxuiTabStrip::GetGlyphInk (theme, true), DxuiTabStrip::GetGlyphInk (theme, false), L"and the two glyph inks");

                rig.group.SetKind        (DxuiTabGroup::Kind::ToolWindow);
                rig.group.Layout         (RECT { 0, 0, 400, 300 }, rig.scaler);
                rig.group.SetFocusedLook (focused);
                rig.group.Paint          (painter, text, theme);

                title = rig.group.GetTitleRect();

                for (const RecordedTextCall & call : text.Calls())
                {
                    bool  inTitle = call.y >= (float) title.top && call.y < (float) title.bottom;
                    bool  isGlyph = call.text == s_kpszMdl2ChevronDown || call.text == s_kpszMdl2Pinned || call.text == s_kpszMdl2Cancel;

                    if (inTitle && call.text == L"Registers")
                    {
                        titles++;
                        Assert::AreEqual (label, call.argb, (L"the title, " + look).c_str());
                    }
                    else if (inTitle && isGlyph)
                    {
                        glyphs++;
                        Assert::AreEqual (glyph, call.argb, (L"a title bar glyph, " + look).c_str());
                    }
                }

                Assert::AreEqual ((size_t) 1, titles, (L"the title, " + look).c_str());
                Assert::AreEqual ((size_t) 3, glyphs, (L"the menu, pin and close glyphs, " + look).c_str());
            }
        }


        //  With Visual Studio's colors -- a foreground of #FFFFFF, muted to
        //  #C5C5C5, and content of #282828, so a band of #262626 -- the inks
        //  are Visual Studio's: other labels #D7D7D7, glyphs #D1D1D1, the
        //  hovered tab's label #D9D9D9 and glyphs #D3D3D3, and a hovered tab
        //  #333333 once laid over the band.
        TEST_METHOD (TheInksReproduceVisualStudio)
        {
            constexpr float  kChannel = 255.0f;
            DxuiTheme        theme    = {};
            uint32_t         hover    = 0;
            float            alpha    = 0.0f;
            long             over     = 0;



            theme.panelBg       = 0xFF282828;
            theme.contentBg     = 0xFF282828;
            theme.bodyText      = 0xFFFFFFFF;
            theme.dropdownAccel = 0xFFC5C5C5;

            hover = DxuiTabStrip::GetTabHoverFill (theme);
            alpha = (float) (hover >> 24) / kChannel;
            over  = std::lround (0x26 + (0xFF - 0x26) * alpha);

            Assert::AreEqual (0xFF262626u, theme.PaneBand());
            Assert::AreEqual (0xFFFFFFFFu, DxuiTabStrip::GetLabelInk (theme, true));
            Assert::AreEqual (0xFFD7D7D7u, DxuiTabStrip::GetLabelInk (theme, false));
            Assert::AreEqual (0xFFFFFFFFu, DxuiTabStrip::GetGlyphInk (theme, true));
            Assert::AreEqual (0xFFD1D1D1u, DxuiTabStrip::GetGlyphInk (theme, false));
            Assert::AreEqual (0xFFD9D9D9u, DxuiTabStrip::GetHoveredLabelInk (theme), L"the hovered tab's label");
            Assert::AreEqual (0xFFD3D3D3u, DxuiTabStrip::GetHoveredGlyphInk (theme), L"and its glyphs");
            Assert::AreEqual (0x33L,       over, L"#333333 over the band");
        }


        //  In every theme the debugger offers, a label other than the focused
        //  selection differs from the foreground and reads at 4.5:1 on the
        //  band, and a hovered tab shows on it.
        TEST_METHOD (EveryThemesTabLabelsReadOnTheBand)
        {
            DxuiDarkTheme   dark;
            DxuiLightTheme  light;



            AssertTabInksRead (dark,                            L"system dark");
            AssertTabInksRead (light,                           L"system light");
            AssertTabInksRead (CassoTheme::MakeSkeuomorphic(),  L"skeuomorphic");
            AssertTabInksRead (CassoTheme::MakeDarkModern(),    L"dark modern");
            AssertTabInksRead (CassoTheme::MakeRetroTerminal(), L"retro terminal");
        }


        //  A pane too small to round, here 23 px deep at 125%, under four
        //  6-px radii, draws its tabs square as its frame is: the selected
        //  tab and the hovered tab are plain fills, and nothing is rounded.
        TEST_METHOD (ATabInAPaneTooSmallToRoundIsSquare)
        {
            DxuiTabGroup          group;
            MockDxuiControl       a;
            MockDxuiControl       b;
            DxuiDpiScaler         scaler;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            RECT                  tab     = {};
            size_t                rounded = 0;
            bool                  body    = false;
            bool                  hovered = false;



            scaler.SetDpi (120);
            group.AddTab  (L"Registers", &a);
            group.AddTab  (L"Stack",     &b);
            group.Layout  (RECT { 0, 0, 400, 23 }, scaler);
            tab = group.GetTabRect (1);
            group.OnMouse (Mouse (DxuiMouseEventKind::Move, (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2));
            group.Paint   (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                rounded += (call.kind == RecordedPaintKind::FillRoundedRect || call.kind == RecordedPaintKind::OutlineRoundedRect) ? 1 : 0;
                body     = body    || (call.kind == RecordedPaintKind::FillRect && call.argb == theme.ContentBackground() && call.x < (float) tab.left);
                hovered  = hovered || (call.kind == RecordedPaintKind::FillRect && call.argb == DxuiTabStrip::GetTabHoverFill (theme));
            }

            Assert::AreEqual ((size_t) 0, rounded, L"nothing rounded");
            Assert::IsTrue   (body,    L"the selected tab, square");
            Assert::IsTrue   (hovered, L"the hovered tab, square");
        }


        //  The joins' fillets are drawn after the tabs, so the selected tab
        //  flares into the line over a hovered neighbor's box, as Visual
        //  Studio draws it.
        TEST_METHOD (TheJoinsAreDrawnOverAHoveredNeighbor)
        {
            Rig                   rig;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            RECT                  tab     = {};
            size_t                box     = SIZE_MAX;
            size_t                fillet  = 0;



            rig.group.SetActive (1);
            tab = rig.group.GetTabRect (2);
            rig.group.OnMouse   (Mouse (DxuiMouseEventKind::Move, (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2));
            rig.group.Paint     (painter, text, theme);

            for (size_t i = 0; i < painter.Calls().size(); i++)
            {
                const RecordedPaintCall  & call = painter.Calls()[i];

                if (box == SIZE_MAX && call.kind == RecordedPaintKind::FillRoundedRect && call.argb == DxuiTabStrip::GetTabHoverFill (theme))
                {
                    box = i;
                }

                if (call.kind == RecordedPaintKind::OutlineRoundedRect && call.argb == theme.ContentBackground() && call.clip.right > tab.left)
                {
                    fillet = i;
                }
            }

            Assert::IsTrue (box != SIZE_MAX, L"the hovered tab's box");
            Assert::IsTrue (fillet > box,    L"the join beside it is drawn after it");
        }
    };
}
