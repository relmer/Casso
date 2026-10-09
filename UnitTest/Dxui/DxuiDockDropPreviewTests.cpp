#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropPreviewTests
//
//  What a drag shows over the target under the pointer, as Visual Studio
//  shows it at 125%:
//
//  - A tab drop shades the pane up to the line between it and its tabs, and
//    a 125-px tab where the first tab is, over the band and the line.
//  - Any other drop shades the group the pane would form, as the layout lays
//    it out once the pane is there; a window edge's shade also reaches over
//    the margin around the panes.
//  - The shade is the theme's DockPreview, with no outline. It lies over the
//    guide holding the button under the pointer and under every other guide.
//  - An edge guide's box lies 10 DIP in from the docked area's edge, the
//    margin included, centered along it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockDropPreviewTests
{
    static constexpr UINT  s_kDpi125 = 120;
    static constexpr RECT  s_kBounds = { 0, 0, 1000, 600 };



    //  The documents' group above the console's, and the registers tabbed
    //  with the stack to their right, with Visual Studio's gap and margin.
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        DxuiDarkTheme    theme;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            scaler.SetDpi (s_kDpi125);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.AddPane       (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetPaneLayout (layout);
            site.SetPaneGap    (DxuiDockSite::kPaneGapDip, DxuiDockSite::kPaneMarginDip);
            site.Layout        (s_kBounds, scaler);
        }

        DxuiTabGroup * GetGroupOf (MockDxuiControl & pane) const
        {
            DxuiTabGroup  * found = nullptr;

            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                found = (site.GetGroup (i)->IndexOf (&pane) >= 0) ? site.GetGroup (i) : found;
            }

            return found;
        }

        void Move (POINT at)
        {
            DxuiMouseEvent  ev;

            ev.kind        = DxuiMouseEventKind::Move;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;
            (void) site.OnMouse (ev);
        }

        //  The pointer over a group, so its cross shows, then on `button` of
        //  that cross; the zone it hovers, or none.
        const DxuiDockDropZone * HoverCross (const DxuiTabGroup & group, DxuiDockGuideKind kind, DxuiDockGuideButton button)
        {
            RECT   bounds = group.GetBounds();
            POINT  center = { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };
            POINT  origin = DxuiDockGuide::GetOrigin (kind, center, scaler);
            RECT   rect   = DxuiDockGuide::GetButtonRect (kind, button, origin, scaler);

            Move (center);
            Move (POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 });
            return site.GetHoveredZone();
        }

        //  The edge guide shown against one edge of the site, or an empty
        //  rect.
        RECT GetEdgeGuideRect (DxuiDockSide side) const
        {
            long  size  = DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge, scaler).cx;
            RECT  found = {};

            for (const DxuiDockDragMark & mark : site.GetDragMarks (theme))
            {
                RECT  r       = mark.rect;
                bool  isEdge  = mark.image != nullptr && r.right - r.left == size;
                bool  matches = (side == DxuiDockSide::Left   && r.left   < 100)                  ||
                                (side == DxuiDockSide::Right  && r.right  > s_kBounds.right - 100) ||
                                (side == DxuiDockSide::Top    && r.top    < 100)                  ||
                                (side == DxuiDockSide::Bottom && r.bottom > s_kBounds.bottom - 100);

                found = (isEdge && matches) ? r : found;
            }

            return found;
        }

        //  The pointer on the button of the edge guide along `side`.
        const DxuiDockDropZone * HoverEdge (DxuiDockSide side)
        {
            RECT  guide = GetEdgeGuideRect (side);
            RECT  rect  = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::Edge, DxuiDockGuide::GetDockButton (side), POINT { guide.left, guide.top }, scaler);

            Move (POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 });
            return site.GetHoveredZone();
        }
    };



    //  The site edge a guide lies against.
    static DxuiDockSide GetSideOf (const RECT & guide)
    {
        return (guide.left   < 100)                    ? DxuiDockSide::Left
             : (guide.right  > s_kBounds.right - 100)  ? DxuiDockSide::Right
             : (guide.top    < 100)                    ? DxuiDockSide::Top
                                                       : DxuiDockSide::Bottom;
    }



    static void CheckRect (const RECT & expected, const RECT & actual, const wchar_t * what)
    {
        std::wstring  text = std::format (L"{}: expected {{{}, {}, {}, {}}}, got {{{}, {}, {}, {}}}", what,
                                          expected.left, expected.top, expected.right, expected.bottom,
                                          actual.left, actual.top, actual.right, actual.bottom);

        Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, text.c_str());
    }



    TEST_CLASS (DxuiDockDropPreviewTests)
    {
    public:

        //  The registers and the stack share a tool window with its tabs
        //  along its bottom: the shade runs down to the line above the tabs,
        //  and the new tab is 125 px wide, from the line to the bottom.
        TEST_METHOD (ATabDropOnAToolWindowShadesItDownToItsLineAndATab)
        {
            Rig                       rig;
            DxuiTabGroup            * regs  = rig.GetGroupOf (rig.regs);
            const DxuiDockDropZone  * zone  = nullptr;
            RECT                      pane  = {};
            long                      line  = 0;



            if (regs == nullptr)
            {
                Assert::Fail (L"the registers are in a group");
                return;
            }

            pane = regs->GetBounds();
            line = regs->GetStripRect().top - DxuiPaneMetrics::GetLinePx (rig.scaler);

            rig.site.BeginDrag (L"console");
            zone = rig.HoverCross (*regs, DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center);

            if (zone == nullptr || zone->kind != DxuiDockDropZone::Kind::Tab)
            {
                Assert::Fail (L"the cross's center button is under the pointer");
                return;
            }

            CheckRect (RECT { pane.left, pane.top, pane.right,      line        }, zone->preview,    L"the pane, short of its line");
            CheckRect (RECT { pane.left, line,     pane.left + 125, pane.bottom }, zone->previewTab, L"a 125-px tab over the line and the band");
        }


        //  The console's tool window shows no tabs; the tab drop gives it some,
        //  so its shade shows them where a tool window with tabs has them.
        TEST_METHOD (ATabDropOnALoneToolWindowShadesTheTabsItWouldGain)
        {
            Rig                       rig;
            DxuiTabGroup            * regs    = rig.GetGroupOf (rig.regs);
            DxuiTabGroup            * console = rig.GetGroupOf (rig.console);
            const DxuiDockDropZone  * zone    = nullptr;
            RECT                      pane    = {};
            long                      depth   = 0;



            if (regs == nullptr || console == nullptr)
            {
                Assert::Fail (L"the registers and the console are in groups");
                return;
            }

            pane  = console->GetBounds();
            depth = regs->GetBounds().bottom - (regs->GetStripRect().top - DxuiPaneMetrics::GetLinePx (rig.scaler));

            Assert::IsTrue (console->GetStripRect().bottom <= console->GetStripRect().top, L"the console shows no tabs yet");

            rig.site.BeginDrag (L"stack");
            zone = rig.HoverCross (*console, DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center);

            if (zone == nullptr || zone->kind != DxuiDockDropZone::Kind::Tab)
            {
                Assert::Fail (L"the cross's center button is under the pointer");
                return;
            }

            CheckRect (RECT { pane.left, pane.top,            pane.right,      pane.bottom - depth }, zone->preview,    L"the pane, short of the line its tabs would bring");
            CheckRect (RECT { pane.left, pane.bottom - depth, pane.left + 125, pane.bottom          }, zone->previewTab, L"the tab, as deep as a tool window's line and band");
        }


        TEST_METHOD (ATabDropOnADocumentShadesBelowItsTabs)
        {
            Rig                       rig;
            DxuiTabGroup            * code = rig.GetGroupOf (rig.code);
            const DxuiDockDropZone  * zone = nullptr;
            RECT                      pane = {};
            RECT                      body = {};



            if (code == nullptr)
            {
                Assert::Fail (L"the documents are in a group");
                return;
            }

            pane = code->GetBounds();
            body = code->GetBodyRect();

            rig.site.BeginDrag (L"console");
            zone = rig.HoverCross (*code, DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::Center);

            if (zone == nullptr || zone->kind != DxuiDockDropZone::Kind::Tab)
            {
                Assert::Fail (L"the cross's center button is under the pointer");
                return;
            }

            CheckRect (RECT { pane.left, body.top, pane.right,      pane.bottom }, zone->preview,    L"the pane below its tabs and their line");
            CheckRect (RECT { pane.left, pane.top, pane.left + 125, body.top    }, zone->previewTab, L"a 125-px tab over the band and the line");
        }


        //  Every other drop shades exactly where the pane lands, as deep as
        //  the layout makes it, not half the target: a side of a tool window,
        //  a side of the documents (beside the whole well), and a new tab
        //  group split off them.
        TEST_METHOD (EveryOtherDropShadesWhereThePaneLands)
        {
            struct Case
            {
                bool                 onDocuments;
                DxuiDockGuideButton  button;
                const wchar_t      * what;
            };

            const Case  kCases[] =
            {
                { false, DxuiDockGuideButton::DockBottom, L"below the registers"   },
                { false, DxuiDockGuideButton::DockLeft,   L"left of the registers" },
                { true,  DxuiDockGuideButton::DockTop,    L"above the documents"   },
                { true,  DxuiDockGuideButton::SplitRight, L"split off to the right of the documents" },
            };



            for (const Case & test : kCases)
            {
                Rig                       rig;
                DxuiTabGroup            * target  = rig.GetGroupOf (test.onDocuments ? rig.code : rig.regs);
                DxuiTabGroup            * landed  = nullptr;
                const DxuiDockDropZone  * zone    = nullptr;
                RECT                      preview = {};
                POINT                     drop    = {};

                if (target == nullptr)
                {
                    Assert::Fail (test.what);
                    return;
                }

                rig.site.BeginDrag (L"console");
                zone = rig.HoverCross (*target, test.onDocuments ? DxuiDockGuideKind::LargeCross : DxuiDockGuideKind::SmallCross, test.button);

                if (zone == nullptr)
                {
                    Assert::Fail (test.what);
                    return;
                }

                preview = zone->preview;
                drop    = POINT { (zone->target.left + zone->target.right) / 2, (zone->target.top + zone->target.bottom) / 2 };

                Assert::IsTrue (rig.site.EndDrag (drop), test.what);

                landed = rig.GetGroupOf (rig.console);

                if (landed == nullptr)
                {
                    Assert::Fail (test.what);
                    return;
                }

                CheckRect (landed->GetBounds(), preview, test.what);
            }
        }


        //  Docked against a window edge, the pane lands inside the margin, but
        //  the shade reaches over it to the docked area's edges, Visual
        //  Studio's 8 px at 125%.
        TEST_METHOD (AWindowEdgeShadeReachesOverTheMargin)
        {
            Rig                       rig;
            DxuiTabGroup            * landed  = nullptr;
            const DxuiDockDropZone  * zone    = nullptr;
            RECT                      preview = {};
            POINT                     drop    = {};



            rig.site.BeginDrag (L"console");
            zone = rig.HoverEdge (DxuiDockSide::Left);

            if (zone == nullptr || zone->kind != DxuiDockDropZone::Kind::Edge)
            {
                Assert::Fail (L"the left edge guide's button is under the pointer");
                return;
            }

            preview = zone->preview;
            drop    = POINT { (zone->target.left + zone->target.right) / 2, (zone->target.top + zone->target.bottom) / 2 };

            Assert::IsTrue (rig.site.EndDrag (drop));
            landed = rig.GetGroupOf (rig.console);

            if (landed == nullptr)
            {
                Assert::Fail (L"the console docked");
                return;
            }

            Assert::AreEqual (8L, landed->GetBounds().left, L"the pane lands inside the margin");
            CheckRect (RECT { s_kBounds.left, s_kBounds.top, landed->GetBounds().right, s_kBounds.bottom }, preview,
                       L"the shade reaches the docked area's edges on three sides");
        }


        //  The shade is the theme's own, faint, and only a fill.
        TEST_METHOD (TheShadeIsTheThemesDockPreviewWithNoOutline)
        {
            Rig             rig;
            DxuiTabGroup  * regs   = rig.GetGroupOf (rig.regs);
            size_t          shades = 0;



            if (regs == nullptr)
            {
                Assert::Fail (L"the registers are in a group");
                return;
            }

            rig.site.BeginDrag (L"console");
            (void) rig.HoverCross (*regs, DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockTop);

            for (const DxuiDockDragMark & mark : rig.site.GetDragMarks (rig.theme))
            {
                if (mark.image != nullptr)
                {
                    continue;
                }

                shades++;
                Assert::AreEqual (rig.theme.DockPreview(), mark.argb,      L"the theme's DockPreview");
                Assert::AreEqual (0,                       mark.outlinePx, L"a fill, with no outline");
            }

            Assert::AreEqual ((size_t) 1, shades, L"a side drop shades one rect");
            Assert::AreEqual (0x1Eu,      rig.theme.DockPreview() >> 24, L"at Visual Studio's alpha");
        }


        //  The shade lies over the guide holding the button under the pointer
        //  and under the others: that guide comes first, with the shade laid
        //  over its picture where they meet, and the others are untouched.
        TEST_METHOD (TheShadeLiesOverTheHoveredGuideAndUnderTheOthers)
        {
            Rig                            rig;
            DxuiTabGroup                 * regs   = rig.GetGroupOf (rig.regs);
            const DxuiDockDropZone       * zone   = nullptr;
            std::vector<DxuiDockDragMark>  marks;
            std::vector<DxuiDockDragMark>  guides;
            std::vector<RECT>              shades;
            DxuiDockGuideColors            colors;
            DxuiIconImage                  cross;
            DxuiIconImage                  plain;



            if (regs == nullptr)
            {
                Assert::Fail (L"the registers are in a group");
                return;
            }

            colors.border       = rig.theme.DockGuideBorder();
            colors.fill         = rig.theme.DockGuideFill();
            colors.buttonBorder = rig.theme.DockGuideButtonBorder();
            colors.buttonFill   = rig.theme.DockGuideButtonFill();
            colors.glyph        = rig.theme.DockGuideGlyph();
            colors.arrow        = rig.theme.DockGuideArrow();

            rig.site.BeginDrag (L"console");
            zone  = rig.HoverCross (*regs, DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center);
            marks = rig.site.GetDragMarks (rig.theme);

            if (zone == nullptr)
            {
                Assert::Fail (L"the cross's center button is under the pointer");
                return;
            }

            for (const DxuiDockDragMark & mark : marks)
            {
                if (mark.image == nullptr)
                {
                    Assert::IsTrue (guides.empty(), L"every shade comes before every guide");
                    shades.push_back (mark.rect);
                    continue;
                }

                guides.push_back (mark);
            }

            Assert::AreEqual ((size_t) 2, shades.size(), L"the pane and its new tab");
            Assert::AreEqual ((size_t) 5, guides.size(), L"the cross and four edge guides");
            Assert::AreEqual ((long) DxuiDockGuide::GetSizePx (DxuiDockGuideKind::SmallCross, rig.scaler).cx,
                              guides[0].rect.right - guides[0].rect.left, L"the cross holding the button comes first");

            cross = DxuiDockGuide::Render (DxuiDockGuideKind::SmallCross, DxuiDockSide::Left, (int) DxuiDockGuideButton::Center, colors, rig.scaler);
            plain = cross;

            for (const RECT & shade : shades)
            {
                DxuiCoverageRaster::TintCovered (cross, DxuiCoverageRect { (float) (shade.left  - guides[0].rect.left), (float) (shade.top    - guides[0].rect.top),
                                                                           (float) (shade.right - guides[0].rect.left), (float) (shade.bottom - guides[0].rect.top) },
                                                 rig.theme.DockPreview());
            }

            Assert::IsTrue (cross.bgraPremul == guides[0].image->bgraPremul, L"the cross shows the shade over it");
            Assert::IsTrue (plain.bgraPremul != guides[0].image->bgraPremul, L"which changes it");

            for (size_t i = 1; i < guides.size(); i++)
            {
                plain = DxuiDockGuide::Render (DxuiDockGuideKind::Edge, GetSideOf (guides[i].rect), -1, colors, rig.scaler);

                Assert::IsTrue (plain.bgraPremul == guides[i].image->bgraPremul, L"an edge guide over the shade shows none of it");
            }
        }


        //  Each edge guide's box lies 10 DIP, 13 px, in from the docked area's
        //  edge, which takes in the margin but not an auto-hide strip, and is
        //  centered along it, rounding down.
        TEST_METHOD (AnEdgeGuideLiesTenDipInFromTheDockedArea)
        {
            Rig   rig;
            long  docked      = 0;
            RECT  leftGuide   = {};
            RECT  topGuide    = {};
            RECT  rightGuide  = {};
            RECT  bottomGuide = {};



            (void) rig.site.EditPaneLayout().AutoHide (L"stack", DxuiDockSide::Right);
            rig.site.Relayout();

            docked = s_kBounds.right - rig.scaler.ToPx (DxuiDockSite::kEdgeStripDip);

            rig.site.BeginDrag (L"console");
            leftGuide   = rig.GetEdgeGuideRect (DxuiDockSide::Left);
            topGuide    = rig.GetEdgeGuideRect (DxuiDockSide::Top);
            rightGuide  = rig.GetEdgeGuideRect (DxuiDockSide::Right);
            bottomGuide = rig.GetEdgeGuideRect (DxuiDockSide::Bottom);

            Assert::AreEqual (13L,                                     leftGuide.left,                           L"13 px from the left edge, the margin included");
            Assert::AreEqual (13L,                                     topGuide.top,                             L"and from the top");
            Assert::AreEqual (docked - 13,                             rightGuide.right,                         L"from the auto-hide strip on the right");
            Assert::AreEqual (s_kBounds.bottom - 13,                   bottomGuide.bottom,                       L"and from the bottom");
            Assert::AreEqual ((s_kBounds.left + docked) / 2,           (topGuide.left + topGuide.right) / 2,     L"centered along the docked area");
            Assert::AreEqual ((s_kBounds.top + s_kBounds.bottom) / 2, (leftGuide.top + leftGuide.bottom) / 2,   L"and down its side");
        }
    };
}
