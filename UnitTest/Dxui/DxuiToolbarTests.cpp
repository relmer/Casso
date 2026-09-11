#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarTests
//
//  The strip driven headlessly through its host-forwarded handlers, with no
//  popup host, so the drop-down is the in-window path and every rect is
//  arithmetic on the mock's fixed glyph width.
//
//  Five commands in two groups. At 96 dpi every entry costs 10 + 15 + 10 =
//  35 px collapsed and 35 + 7 + 7 * label px labeled; the strip pads 10 px a
//  side, 4 px between neighbors and 18 px across the one group change. The
//  widths the collapse tests use are computed from those numbers rather than
//  typed in, so a change to a metric moves the test with it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarTests)
{
public:

    static constexpr int  s_kGlyphPx   = 7;     // MockDxuiTextRenderer's fixed advance
    static constexpr int  s_kPadPx     = 10;
    static constexpr int  s_kIconPx    = 15;
    static constexpr int  s_kIconGapPx = 7;
    static constexpr int  s_kBarPadPx  = 10;
    static constexpr int  s_kBtnGapPx  = 4;
    static constexpr int  s_kGroupGap  = 18;
    static constexpr int  s_kBandPx    = 42;

    static int  CollapsedPx()                     { return s_kPadPx * 2 + s_kIconPx; }
    static int  LabeledPx   (const wchar_t * s)   { return CollapsedPx() + s_kIconGapPx + s_kGlyphPx * (int) wcslen (s); }


    //
    //  A stub custom entry that records what it was asked and answers with
    //  fixed numbers, so the widget's delegation is visible from outside.
    //
    struct StubEntry : public IDxuiToolbarCustomEntry
    {
        int   widthCalls   = 0;
        int   layoutCalls  = 0;
        int   paintCalls   = 0;
        int   tipCalls     = 0;
        int   clickCalls   = 0;
        bool  lastLabeled  = true;
        bool  consumeClick = true;
        RECT  lastRc       = {};

        int  GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const override
        {
            (void) scaler; (void) text;
            const_cast<StubEntry *> (this)->widthCalls++;
            return labeled ? 90 : 35;
        }

        void  Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler) override
        {
            (void) scaler;
            layoutCalls++;
            lastRc      = rc;
            lastLabeled = labeled;
        }

        void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme,
                     bool hovered, bool pressed, bool labeled) override
        {
            (void) painter; (void) text; (void) theme; (void) hovered; (void) pressed; (void) labeled;
            paintCalls++;
        }

        const wchar_t *  GetTooltipAt (int x, int y, RECT & anchor) const override
        {
            (void) x; (void) y; (void) anchor;
            const_cast<StubEntry *> (this)->tipCalls++;
            return nullptr;
        }

        bool  OnClick (int x, int y) override
        {
            (void) x; (void) y;
            clickCalls++;
            return consumeClick;
        }
    };


    //
    //  Five commands: three in group 0, two in group 1. Every dispatch counts
    //  and remembers who fired; `delta` is the toggle's state and `enabled`
    //  the fourth command's availability.
    //
    struct Fixture
    {
        DxuiCommand           alpha;
        DxuiCommand           beta;
        DxuiCommand           gamma;
        DxuiCommand           delta;
        DxuiCommand           eps;
        DxuiCommand           rowA;
        DxuiCommand           rowB;
        DxuiCommand           rowC;
        int                   dispatched     = 0;
        int                   lastDispatched = -1;
        bool                  deltaChecked   = false;
        bool                  gammaEnabled   = true;
        int                   checkedRow     = 1;
        std::vector<int>      previews;
        std::vector<int>      commits;
        uint64_t              nowMs          = 10000;
        DxuiToolbar           bar;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;

        Fixture()
        {
            DxuiCommand *  cmds[5] = { &alpha, &beta, &gamma, &delta, &eps };
            const wchar_t *  labels[5] = { L"Alpha", L"Bb", L"Gamma", L"Delta", L"E" };

            for (int i = 0; i < 5; i++)
            {
                cmds[i]->id       = i + 1;
                cmds[i]->label    = labels[i];
                cmds[i]->glyph    = L"x";
                cmds[i]->dispatch = [this, i] () { dispatched++; lastDispatched = i + 1; };
            }

            alpha.tip        = L"Alpha does a thing";
            gamma.isEnabled  = [this] () { return gammaEnabled; };
            delta.isChecked  = [this] () { return deltaChecked; };
            delta.dispatch   = [this] () { deltaChecked = !deltaChecked; dispatched++; lastDispatched = 4; };

            rowA.id = 10; rowA.label = L"Row A"; rowA.isChecked = [this] () { return checkedRow == 0; };
            rowB.id = 11; rowB.label = L"Row B"; rowB.isChecked = [this] () { return checkedRow == 1; };
            rowC.id = 12; rowC.label = L"Row C"; rowC.isChecked = [this] () { return checkedRow == 2; };

            bar.SetTextRenderer (&text);
            bar.SetClock ([this] () { return nowMs; });
        }

        //  alpha (Command), beta (DropDown), gamma (Command) | delta (Toggle), eps (Flyout)
        void  Build (IDxuiToolbarCustomEntry * custom = nullptr, DxuiToolbar::Kind customKind = DxuiToolbar::Kind::Command)
        {
            std::vector<DxuiToolbar::Entry>  entries (5);
            std::vector<DxuiPopupMenuItem>   rows;

            entries[0].command = &alpha;  entries[0].group = 0;
            entries[1].command = &beta;   entries[1].group = 0;  entries[1].kind = DxuiToolbar::Kind::DropDown;
            entries[2].command = &gamma;  entries[2].group = 0;
            entries[3].command = &delta;  entries[3].group = 1;  entries[3].kind = DxuiToolbar::Kind::Toggle;
            entries[4].command = &eps;    entries[4].group = 1;  entries[4].kind = DxuiToolbar::Kind::Flyout;

            if (custom != nullptr)
            {
                entries[2].custom = custom;
                entries[2].kind   = customKind;
            }

            rows.push_back (DxuiPopupMenuItem::ForCommand (&rowA));
            rows.push_back (DxuiPopupMenuItem::ForCommand (&rowB));
            rows.push_back (DxuiPopupMenuItem::ForCommand (&rowC));

            bar.SetEntries      (std::move (entries));
            bar.SetDropDownItems (2, std::move (rows));
            bar.SetDropDownSinks (2,
                                  [this] (int i) { previews.push_back (i); },
                                  [this] (int i) { commits.push_back (i); checkedRow = i; });
        }

        int  FullWidth() const
        {
            return s_kBarPadPx * 2 +
                   LabeledPx (L"Alpha") + s_kBtnGapPx +
                   LabeledPx (L"Bb")    + s_kBtnGapPx +
                   LabeledPx (L"Gamma") + s_kGroupGap +
                   LabeledPx (L"Delta") + s_kBtnGapPx +
                   LabeledPx (L"E");
        }

        void  LayoutAt (int width)
        {
            bar.Layout (RECT { 0, 0, width, s_kBandPx }, scaler);
        }

        //  The center of entry `index`, read back from the layout by hovering
        //  along the strip: the first x whose tooltip anchor is a new rect.
        //  Simpler: entries are laid out left to right at known widths, so
        //  the center is computed the same way the widget computes it.
        POINT  Center (int index, int labeledCount) const
        {
            const wchar_t *  labels[5] = { L"Alpha", L"Bb", L"Gamma", L"Delta", L"E" };
            int              x         = s_kBarPadPx;

            for (int i = 0; i < 5; i++)
            {
                int  w = (i < labeledCount) ? LabeledPx (labels[i]) : CollapsedPx();

                if (i == index)
                {
                    return POINT { x + w / 2, s_kBandPx / 2 };
                }

                x += w + ((i == 2) ? s_kGroupGap : s_kBtnGapPx);
            }

            return POINT { -1, -1 };
        }

        void  Click (POINT p)
        {
            bar.OnToolbarMouseMove   (p.x, p.y);
            bar.OnToolbarLButtonDown (p.x, p.y);
            bar.OnToolbarLButtonUp   (p.x, p.y);
        }
    };


    TEST_METHOD (PlanForWidth_AllLabeledAtFittingWidth)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (f.FullWidth());

        for (int id = 1; id <= 5; id++)
        {
            Assert::IsTrue (f.bar.IsLabeled (id));
        }

        Assert::AreEqual (s_kBandPx, f.bar.GetBandDp());
    }


    TEST_METHOD (PlanForWidth_OnlyRightmostCollapsesOneStepNarrower)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (f.FullWidth() - 1);

        Assert::IsTrue  (f.bar.IsLabeled (1));
        Assert::IsTrue  (f.bar.IsLabeled (2));
        Assert::IsTrue  (f.bar.IsLabeled (3));
        Assert::IsTrue  (f.bar.IsLabeled (4));
        Assert::IsFalse (f.bar.IsLabeled (5));
    }


    TEST_METHOD (PlanForWidth_NoneLabeledFarNarrower)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (s_kBarPadPx * 2 + CollapsedPx() * 5 + s_kBtnGapPx * 3 + s_kGroupGap);

        for (int id = 1; id <= 5; id++)
        {
            Assert::IsFalse (f.bar.IsLabeled (id));
        }
    }


    TEST_METHOD (GetTooltipAt_TipInEveryForm_NameOnlyWhenCollapsed_NullInGap)
    {
        Fixture  f;
        RECT     anchor = {};
        POINT    pAlpha = {};
        POINT    pBeta  = {};
        int      gapX   = 0;


        f.Build();
        f.LayoutAt (f.FullWidth());

        pAlpha = f.Center (0, 5);
        pBeta  = f.Center (1, 5);

        Assert::AreEqual (L"Alpha does a thing", f.bar.GetTooltipAt (pAlpha.x, pAlpha.y, anchor));
        Assert::AreEqual ((LONG) s_kBarPadPx, anchor.left);
        Assert::IsNull   (f.bar.GetTooltipAt (pBeta.x, pBeta.y, anchor));     // labeled, no tip

        gapX = s_kBarPadPx + LabeledPx (L"Alpha") + s_kBtnGapPx / 2;
        Assert::IsNull (f.bar.GetTooltipAt (gapX, s_kBandPx / 2, anchor));

        f.LayoutAt (f.FullWidth() - 1);                                        // eps collapses
        Assert::AreEqual (L"E", f.bar.GetTooltipAt (f.Center (4, 4).x, s_kBandPx / 2, anchor));
    }


    TEST_METHOD (Dispatch_FiresOnceOnDownAndUpOnOneEntry)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (f.FullWidth());

        f.Click (f.Center (0, 5));

        Assert::AreEqual (1, f.dispatched);
        Assert::AreEqual (1, f.lastDispatched);
    }


    TEST_METHOD (Dispatch_DoesNotFireAcrossEntries)
    {
        Fixture  f;
        POINT    a = {};
        POINT    g = {};


        f.Build();
        f.LayoutAt (f.FullWidth());

        a = f.Center (0, 5);
        g = f.Center (2, 5);

        f.bar.OnToolbarMouseMove   (a.x, a.y);
        f.bar.OnToolbarLButtonDown (a.x, a.y);
        f.bar.OnToolbarMouseMove   (g.x, g.y);
        f.bar.OnToolbarLButtonUp   (g.x, g.y);

        Assert::AreEqual (0, f.dispatched);
    }


    TEST_METHOD (Dispatch_DoesNotFireWhenDisabled)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (f.FullWidth());

        f.gammaEnabled = false;
        f.Click (f.Center (2, 5));

        Assert::AreEqual (0, f.dispatched);
    }


    TEST_METHOD (Toggle_DrawsPressedFromIsChecked)
    {
        Fixture  f;
        int      fills = 0;


        f.Build();
        f.LayoutAt (f.FullWidth());

        f.bar.Paint (f.painter, f.text, f.theme);
        fills = (int) f.painter.Calls().size();

        f.deltaChecked = true;
        f.painter.Reset();
        f.bar.Paint (f.painter, f.text, f.theme);

        // The checked toggle adds its pressed fill and border, and nothing
        // else on the idle strip changed.
        Assert::AreEqual (fills + 2, (int) f.painter.Calls().size());

        {
            bool  sawPressed = false;

            for (const RecordedPaintCall & c : f.painter.Calls())
            {
                if (c.kind == RecordedPaintKind::FillRect && c.argb == MockDxuiTheme::s_kButtonPressed)
                {
                    sawPressed = true;
                }
            }

            Assert::IsTrue (sawPressed);
        }
    }


    TEST_METHOD (DropDown_PreviewOnHighlight_SnapBackOnEscape_OneCommitOnEnter)
    {
        Fixture  f;
        POINT    b = {};


        f.Build();
        f.LayoutAt (f.FullWidth());
        b = f.Center (1, 5);

        f.Click (b);
        Assert::IsTrue (f.bar.IsMenuOpen());

        Assert::IsTrue (f.bar.HandleKey (VK_DOWN));                  // row A
        Assert::IsTrue (f.bar.HandleKey (VK_DOWN));                  // row B
        Assert::IsTrue (f.bar.HandleKey (VK_DOWN));                  // row C
        Assert::AreEqual ((size_t) 3, f.previews.size());
        Assert::AreEqual (2, f.previews.back());

        Assert::IsTrue (f.bar.HandleKey (VK_ESCAPE));
        Assert::IsFalse (f.bar.IsMenuOpen());
        Assert::AreEqual ((size_t) 4, f.previews.size());
        Assert::AreEqual (1, f.previews.back());                     // snapped back to the checked row
        Assert::AreEqual ((size_t) 0, f.commits.size());

        // Past the reopen guard, the same button opens the menu again.
        f.nowMs += 1000;
        f.Click (b);
        Assert::IsTrue (f.bar.IsMenuOpen());
        f.bar.HandleKey (VK_DOWN);
        f.bar.HandleKey (VK_DOWN);
        f.bar.HandleKey (VK_DOWN);
        Assert::IsTrue (f.bar.HandleKey (VK_RETURN));
        Assert::IsFalse (f.bar.IsMenuOpen());

        Assert::AreEqual ((size_t) 1, f.commits.size());
        Assert::AreEqual (2, f.commits.back());
        Assert::AreEqual (2, f.checkedRow);
        Assert::AreEqual (2, f.previews.back());                     // no snap-back after a commit
    }


    TEST_METHOD (Flyout_OpensOnDwell_ClosesOnLeave)
    {
        Fixture     f;
        DxuiSlider  slider;
        POINT       e = {};


        f.Build();
        f.bar.SetFlyoutControl (5, &slider, SIZE { 56, 154 });
        f.LayoutAt (f.FullWidth());
        e = f.Center (4, 5);

        Assert::IsFalse (f.bar.IsFlyoutOpen (5));

        f.bar.OnToolbarMouseMove (e.x, e.y);
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));

        // Straight down from the entry stays in the corridor.
        f.bar.OnToolbarMouseMove (e.x, s_kBandPx + 20);
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));

        f.bar.OnToolbarMouseMove (0, s_kBandPx + 20);
        Assert::IsFalse (f.bar.IsFlyoutOpen (5));

        f.bar.OnToolbarMouseMove (e.x, e.y);
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));
        f.bar.OnToolbarMouseLeave();
        Assert::IsFalse (f.bar.IsFlyoutOpen (5));
    }


    TEST_METHOD (Flyout_CollapseClosesIt_NextDwellReopens)
    {
        Fixture     f;
        DxuiSlider  slider;
        POINT       e = {};


        f.Build();
        f.bar.SetFlyoutControl (5, &slider, SIZE { 56, 154 });
        f.LayoutAt (f.FullWidth());
        e = f.Center (4, 5);

        f.bar.OnToolbarMouseMove (e.x, e.y);
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));

        f.LayoutAt (f.FullWidth() - 1);
        Assert::IsFalse (f.bar.IsLabeled (5));
        Assert::IsFalse (f.bar.IsFlyoutOpen (5));

        e = f.Center (4, 4);
        f.bar.OnToolbarMouseMove (e.x, e.y);
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));
    }


    TEST_METHOD (CustomEntry_ReceivesWidthLayoutPaintTooltipAndClick)
    {
        Fixture    f;
        StubEntry  stub;
        RECT       anchor = {};
        POINT      g      = {};


        f.Build (&stub);
        f.LayoutAt (f.FullWidth() + 100);                            // the stub is wider than Gamma

        Assert::IsTrue   (stub.widthCalls > 0);
        Assert::AreEqual (1, stub.layoutCalls);
        Assert::IsTrue   (stub.lastLabeled);
        Assert::AreEqual (90L, stub.lastRc.right - stub.lastRc.left);

        f.bar.Paint (f.painter, f.text, f.theme);
        Assert::AreEqual (1, stub.paintCalls);

        g = POINT { stub.lastRc.left + 45, s_kBandPx / 2 };
        f.bar.GetTooltipAt (g.x, g.y, anchor);
        Assert::IsTrue (stub.tipCalls > 0);

        // Expanded, the entry itself takes no press; collapsed, it is a button.
        f.Click (g);
        Assert::AreEqual (0, stub.clickCalls);

        f.LayoutAt (s_kBarPadPx * 2 + CollapsedPx() * 5 + s_kBtnGapPx * 3 + s_kGroupGap);
        Assert::IsFalse (stub.lastLabeled);

        g = POINT { stub.lastRc.left + 17, s_kBandPx / 2 };
        f.Click (g);
        Assert::AreEqual (1, stub.clickCalls);
        Assert::AreEqual (0, f.dispatched);                         // consumed, so the kind did not fire
    }


    //
    //  A hosted control that records focus and the keys it was handed.
    //
    struct StubPanel : public IDxuiControl
    {
        int     focusOn  = 0;
        int     focusOff = 0;
        int     keys     = 0;
        WPARAM  lastVk   = 0;

        void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override { (void) scaler; SetBounds (boundsDip); }
        void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override { (void) painter; (void) text; (void) theme; }
        bool  OnKey  (const DxuiKeyEvent & ev) override { keys++; lastVk = ev.vk; return true; }
        void  OnFocusChanged (bool focused) override { if (focused) { focusOn++; } else { focusOff++; } }
    };


    TEST_METHOD (Keyboard_EnterOnFocusedCommandDispatches_OutlinePaintsFocus)
    {
        Fixture  f;
        int      rings = 0;


        f.Build();
        f.LayoutAt (f.FullWidth());

        f.bar.SetFocusIndex (0);
        Assert::AreEqual (5, f.bar.GetEntryCount());
        Assert::AreEqual (0, f.bar.GetFocusIndex());
        Assert::IsFalse  (f.bar.OwnsKeyboard());

        f.bar.ActivateFocused();
        Assert::AreEqual (1, f.dispatched);
        Assert::AreEqual (1, f.lastDispatched);

        f.bar.Paint (f.painter, f.text, f.theme);

        for (const RecordedPaintCall & c : f.painter.Calls())
        {
            if (c.kind == RecordedPaintKind::OutlineRect && c.argb == MockDxuiTheme::s_kFocusRing) { rings++; }
        }

        Assert::AreEqual (1, rings);

        f.bar.SetFocusIndex (-1);
        f.painter.Reset();
        f.bar.Paint (f.painter, f.text, f.theme);

        for (const RecordedPaintCall & c : f.painter.Calls())
        {
            Assert::IsFalse (c.kind == RecordedPaintKind::OutlineRect && c.argb == MockDxuiTheme::s_kFocusRing);
        }
    }


    TEST_METHOD (Keyboard_EnterOnDropDownOpensItsList)
    {
        Fixture  f;


        f.Build();
        f.LayoutAt (f.FullWidth());

        f.bar.SetFocusIndex (1);
        f.bar.ActivateFocused();

        Assert::IsTrue (f.bar.IsMenuOpen());
        Assert::IsTrue (f.bar.OwnsKeyboard());
        Assert::IsTrue (f.bar.HandleKey (VK_ESCAPE));
        Assert::IsFalse (f.bar.OwnsKeyboard());
        Assert::AreEqual (1, f.bar.GetFocusIndex());
    }


    TEST_METHOD (Keyboard_EnterOnFlyoutOpensPanel_KeysReachIt_EscapeCloses)
    {
        Fixture    f;
        StubPanel  panel;


        f.Build();
        f.bar.SetFlyoutControl (5, &panel, SIZE { 56, 154 });
        f.LayoutAt (f.FullWidth());

        f.bar.SetFocusIndex (4);
        f.bar.ActivateFocused();

        Assert::IsTrue   (f.bar.IsFlyoutOpen (5));
        Assert::IsTrue   (f.bar.OwnsKeyboard());
        Assert::AreEqual (1, panel.focusOn);
        Assert::AreEqual (0, f.dispatched);                          // Enter opened, it did not toggle

        Assert::IsTrue   (f.bar.HandleKey (VK_UP));
        Assert::IsTrue   (f.bar.HandleKey (VK_HOME));
        Assert::AreEqual (2, panel.keys);
        Assert::AreEqual ((WPARAM) VK_HOME, panel.lastVk);

        // The pointer leaving cannot close a keyboard-opened flyout.
        f.bar.OnToolbarMouseMove (0, 400);
        f.bar.OnToolbarMouseLeave();
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));

        Assert::IsTrue   (f.bar.HandleKey (VK_ESCAPE));
        Assert::IsFalse  (f.bar.IsFlyoutOpen (5));
        Assert::IsFalse  (f.bar.OwnsKeyboard());
        Assert::AreEqual (1, panel.focusOff);
        Assert::AreEqual (4, f.bar.GetFocusIndex());                 // the entry keeps focus

        // Focus moving off the entry closes a flyout it opened.
        f.bar.ActivateFocused();
        Assert::IsTrue (f.bar.IsFlyoutOpen (5));
        f.bar.SetFocusIndex (3);
        Assert::IsFalse (f.bar.IsFlyoutOpen (5));
        Assert::AreEqual (2, panel.focusOff);
    }


    TEST_METHOD (CustomEntry_ClickNotConsumedOnDropDownOpensItsList)
    {
        Fixture    f;
        StubEntry  stub;
        POINT      g = {};


        stub.consumeClick = false;

        f.Build (&stub, DxuiToolbar::Kind::DropDown);
        f.bar.SetDropDownItems (3, { DxuiPopupMenuItem::ForCommand (&f.rowA), DxuiPopupMenuItem::ForCommand (&f.rowB) });
        f.LayoutAt (s_kBarPadPx * 2 + CollapsedPx() * 5 + s_kBtnGapPx * 3 + s_kGroupGap);

        g = POINT { stub.lastRc.left + 17, s_kBandPx / 2 };
        f.Click (g);

        Assert::AreEqual (1, stub.clickCalls);
        Assert::IsTrue   (f.bar.IsMenuOpen());
    }
};
