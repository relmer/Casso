#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteGapTests
//
//  The room Visual Studio leaves between docked panes and around them: 8 px
//  between neighbors and 5 px at the edges at 125%, painted in the theme's
//  DockGap. The gap itself is the sash.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteGapTests
{
    static constexpr UINT  s_kDpi125 = 120;
    static constexpr UINT  s_kDpi150 = 144;
    static constexpr RECT  s_kBounds = { 0, 0, 1000, 600 };



    //  Code above the console and the registers to their right, as the
    //  debugger lays them out by default.
    struct Rig
    {
        MockDxuiControl       code;
        MockDxuiControl       regs;
        MockDxuiControl       console;
        DxuiDockSite          site;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        Rig (UINT dpi, bool hasGap)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            scaler.SetDpi (dpi);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetPaneLayout (layout);

            if (hasGap)
            {
                site.SetPaneGap (DxuiDockSite::kPaneGapDip, DxuiDockSite::kPaneMarginDip);
            }

            site.Layout (s_kBounds, scaler);
        }

        //  The bounds of the group holding `pane`, or an empty rect.
        RECT GetGroupRect (MockDxuiControl & pane) const
        {
            RECT  rect = {};

            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                rect = (site.GetGroup (i)->IndexOf (&pane) >= 0) ? site.GetGroup (i)->GetBounds() : rect;
            }

            return rect;
        }

        void Press (POINT point, DxuiMouseEventKind kind)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = point;
            (void) site.OnMouse (ev);
        }
    };



    static void AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
    {
        std::wstring  text = std::format (L"{} expected {{{}, {}, {}, {}}} actual {{{}, {}, {}, {}}}", msg,
                                          expected.left, expected.top, expected.right, expected.bottom,
                                          actual.left, actual.top, actual.right, actual.bottom);

        Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, text.c_str());
    }



    static RECT MakeRect (const RecordedPaintCall & call)
    {
        return RECT { std::lround (call.x), std::lround (call.y), std::lround (call.x + call.width), std::lround (call.y + call.height) };
    }



    static bool IsOverlapping (const RECT & a, const RECT & b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }



    static bool IsInside (const RECT & inner, const RECT & outer)
    {
        return inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top && inner.bottom <= outer.bottom;
    }



    static long GetArea (const RECT & r)
    {
        return (r.right - r.left) * (r.bottom - r.top);
    }



    //  Marks the pixels of `r` inside the site's bounds, a row at a time.
    static void MarkCovered (std::vector<uint8_t> & covered, const RECT & r)
    {
        RECT  clipped = {};

        IntersectRect (&clipped, &r, &s_kBounds);

        for (long y = clipped.top; y < clipped.bottom; y++)
        {
            std::fill_n (covered.begin() + (y * s_kBounds.right + clipped.left), clipped.right - clipped.left, (uint8_t) 1);
        }
    }



    TEST_CLASS (DxuiDockSiteGapTests)
    {
    public:

        TEST_METHOD (PanesAreAGapApart)
        {
            Rig   rig (s_kDpi125, true);
            RECT  code    = rig.GetGroupRect (rig.code);
            RECT  regs    = rig.GetGroupRect (rig.regs);
            RECT  console = rig.GetGroupRect (rig.console);



            Assert::AreEqual (8L, regs.left   - code.right,    L"8 px between side-by-side panes at 125%");
            Assert::AreEqual (8L, regs.left   - console.right, L"and beside the console");
            Assert::AreEqual (8L, console.top - code.bottom,   L"and between stacked ones");

            Assert::AreEqual (5L,   code.left,      L"a 5 px margin on the left");
            Assert::AreEqual (5L,   code.top,       L"at the top");
            Assert::AreEqual (5L,   regs.top,       L"at the top on the right");
            Assert::AreEqual (995L, regs.right,     L"on the right");
            Assert::AreEqual (595L, regs.bottom,    L"at the bottom on the right");
            Assert::AreEqual (5L,   console.left,   L"on the left below");
            Assert::AreEqual (595L, console.bottom, L"and at the bottom");
        }


        //  At 150% the gap is 9 px: the left or upper neighbor gives 4 and the
        //  right or lower one 5, so the split stays where the layout puts it.
        TEST_METHOD (TheGapIsExactAtOddSizes)
        {
            Rig                                     rig (s_kDpi150, true);
            RECT                                    code    = rig.GetGroupRect (rig.code);
            RECT                                    regs    = rig.GetGroupRect (rig.regs);
            RECT                                    console = rig.GetGroupRect (rig.console);
            RECT                                    inside  = { 6, 6, 994, 594 };
            std::vector<DxuiPaneLayout::SplitRect>  splits  = rig.site.GetPaneLayout().ArrangeSplits (inside, nullptr, nullptr);
            long                                    across  = 0;
            long                                    down    = 0;



            Assert::AreEqual ((size_t) 2, splits.size(), L"one split each way");

            across = splits[0].horizontal ? splits[0].position : splits[1].position;
            down   = splits[0].horizontal ? splits[1].position : splits[0].position;

            Assert::AreEqual (9L,         regs.left - code.right,    L"9 px at 150%");
            Assert::AreEqual (across - 4, code.right,                L"4 px of it from the left pane");
            Assert::AreEqual (across + 5, regs.left,                 L"5 px from the right");
            Assert::AreEqual (9L,         console.top - code.bottom, L"9 px between stacked panes");
            Assert::AreEqual (down - 4,   code.bottom,               L"4 px from the upper pane");
            Assert::AreEqual (down + 5,   console.top,               L"5 px from the lower");
            Assert::AreEqual (6L,         code.left,                 L"a 6 px margin at 150%");
        }


        TEST_METHOD (TheSashIsTheGap)
        {
            Rig    rig (s_kDpi125, true);
            RECT   code   = rig.GetGroupRect (rig.code);
            RECT   regs   = rig.GetGroupRect (rig.regs);
            long   y      = (code.top + code.bottom) / 2;
            POINT  middle = { (code.right + regs.left) / 2, y };
            POINT  away   = { 10, 10 };



            Assert::IsTrue (rig.site.GetCursorForPoint (middle)                      == IDC_SIZEWE, L"the gap's middle sizes");
            Assert::IsTrue (rig.site.GetCursorForPoint (POINT { code.right, y })     == IDC_SIZEWE, L"so does its first pixel");
            Assert::IsTrue (rig.site.GetCursorForPoint (POINT { regs.left - 1, y })  == IDC_SIZEWE, L"and its last");
            Assert::IsTrue (rig.site.GetCursorForPoint (POINT { code.right - 1, y }) == nullptr,    L"the left pane's last pixel does not");
            Assert::IsTrue (rig.site.GetCursorForPoint (POINT { regs.left, y })      == nullptr,    L"nor the right pane's first");

            rig.Press (middle, DxuiMouseEventKind::Down);
            Assert::IsTrue (rig.site.GetCursorForPoint (away) == IDC_SIZEWE, L"a press in the gap starts a sash drag");
            rig.Press (middle, DxuiMouseEventKind::Up);

            rig.Press (POINT { regs.left, y }, DxuiMouseEventKind::Down);
            Assert::IsTrue (rig.site.GetCursorForPoint (away) == nullptr, L"a press on the pane's first pixel does not");
            rig.Press (POINT { regs.left, y }, DxuiMouseEventKind::Up);
        }


        TEST_METHOD (DraggingKeepsTheGapUnderThePointer)
        {
            Rig    rig (s_kDpi125, true);
            RECT   code   = rig.GetGroupRect (rig.code);
            RECT   regs   = rig.GetGroupRect (rig.regs);
            long   y      = (code.top + code.bottom) / 2;
            POINT  middle = { (code.right + regs.left) / 2, y };
            POINT  target = { 600, y };



            rig.Press (middle, DxuiMouseEventKind::Down);
            rig.Press (target, DxuiMouseEventKind::Move);
            rig.Press (target, DxuiMouseEventKind::Up);

            code = rig.GetGroupRect (rig.code);
            regs = rig.GetGroupRect (rig.regs);

            Assert::AreEqual (target.x - 4, code.right, L"the left pane ends half the gap short of the pointer");
            Assert::AreEqual (target.x + 4, regs.left,  L"and the right one starts half the gap past it");
        }


        //  The margin and the gaps are painted in DockGap, all of them and
        //  nothing under a pane, and no line is drawn down a split any more.
        //  A group's own fills in that color keep to its edges and corners,
        //  clear of its body.
        TEST_METHOD (PaintFillsOnlyTheGaps)
        {
            Rig                   rig (s_kDpi125, true);
            std::vector<RECT>     groups;
            std::vector<RECT>     bodies;
            std::vector<uint8_t>  covered ((size_t) GetArea (s_kBounds), 0);
            long                  painted = 0;
            long                  gaps    = GetArea (s_kBounds);
            size_t                fills   = 0;
            size_t                lines   = 0;



            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                groups.push_back (rig.site.GetGroup (i)->GetBounds());
                bodies.push_back (rig.site.GetGroup (i)->GetBodyRect());
                gaps -= GetArea (groups.back());
            }

            Assert::AreEqual ((size_t) 3, groups.size(), L"three groups");

            rig.site.Paint (rig.painter, rig.text, rig.theme);

            for (const RecordedPaintCall & call : rig.painter.Calls())
            {
                RECT  r         = MakeRect (call);
                bool  inGroup   = false;
                bool  overGroup = false;
                bool  overBody  = false;

                if (call.kind != RecordedPaintKind::FillRect)
                {
                    continue;
                }

                for (size_t i = 0; i < groups.size(); i++)
                {
                    inGroup   = inGroup   || IsInside      (r, groups[i]);
                    overGroup = overGroup || IsOverlapping (r, groups[i]);
                    overBody  = overBody  || IsOverlapping (r, bodies[i]);
                }

                lines += (call.argb == rig.theme.Divider() && !inGroup) ? 1 : 0;

                if (call.argb != rig.theme.DockGap())
                {
                    continue;
                }

                Assert::IsFalse (overBody,              L"no gap fill reaches into a pane's body");
                Assert::IsTrue  (inGroup || !overGroup, L"a gap fill is the site's, clear of every group, or a group's own");

                if (!inGroup)
                {
                    fills++;
                    MarkCovered (covered, r);
                }
            }

            for (uint8_t pixel : covered)
            {
                painted += pixel;
            }

            Assert::IsTrue   (fills > 0,  L"the site paints its gaps");
            Assert::AreEqual (gaps,       painted, L"every pixel between and around the panes, and none under them");
            Assert::AreEqual ((size_t) 0, lines,   L"no line drawn down a split");
        }


        TEST_METHOD (GetInsetForGapKeepsOuterSides)
        {
            RECT  area  = { 0, 0, 100, 100 };
            RECT  half  = { 0, 0, 50, 100 };
            RECT  other = { 50, 0, 100, 100 };
            RECT  inner = { 20, 30, 60, 70 };



            AssertRect (RECT { 0, 0, 46, 100 },   DxuiDockSite::GetInsetForGap (half,  area, 8), L"only the inner side moves, by half the gap");
            AssertRect (RECT { 54, 0, 100, 100 }, DxuiDockSite::GetInsetForGap (other, area, 8), L"from the other side too");
            AssertRect (RECT { 55, 0, 100, 100 }, DxuiDockSite::GetInsetForGap (other, area, 9), L"a left side takes the larger half of an odd gap");
            AssertRect (RECT { 0, 0, 46, 100 },   DxuiDockSite::GetInsetForGap (half,  area, 9), L"a right side the smaller");
            AssertRect (RECT { 24, 34, 56, 66 },  DxuiDockSite::GetInsetForGap (inner, area, 8), L"every inner side moves");
            AssertRect (area,                     DxuiDockSite::GetInsetForGap (area,  area, 8), L"a rect on the area's edges stays");
            AssertRect (inner,                    DxuiDockSite::GetInsetForGap (inner, area, 0), L"no gap, no change");
        }


        TEST_METHOD (NoGapByDefault)
        {
            Rig   rig (s_kDpi125, false);
            RECT  code    = rig.GetGroupRect (rig.code);
            RECT  regs    = rig.GetGroupRect (rig.regs);
            RECT  console = rig.GetGroupRect (rig.console);



            Assert::AreEqual (code.right,  regs.left,      L"side-by-side panes meet");
            Assert::AreEqual (code.bottom, console.top,    L"so do stacked ones");
            Assert::AreEqual (0L,          code.left,      L"against the site's left edge");
            Assert::AreEqual (0L,          code.top,       L"and its top");
            Assert::AreEqual (1000L,       regs.right,     L"and its right");
            Assert::AreEqual (600L,        console.bottom, L"and its bottom");
        }


        //  A slid-out pane covers the panes inside the same margin, so its
        //  outline lines up with theirs.
        TEST_METHOD (TheSlidPaneLinesUpWithThePanes)
        {
            Rig   rig (s_kDpi125, true);
            RECT  slid    = {};
            RECT  code    = {};
            RECT  console = {};



            (void) rig.site.EditPaneLayout().AutoHide (L"regs", DxuiDockSide::Right);
            rig.site.Relayout();
            rig.site.SlideOut (L"regs");

            slid    = rig.site.GetSlidRect();
            code    = rig.GetGroupRect (rig.code);
            console = rig.GetGroupRect (rig.console);

            Assert::IsTrue   (rig.site.GetSlidPane() == L"regs", L"the pane slides out");
            Assert::AreEqual (code.right,     slid.right,  L"its right edge is the panes' right edge");
            Assert::AreEqual (code.top,       slid.top,    L"its top is theirs");
            Assert::AreEqual (console.bottom, slid.bottom, L"and its bottom");
        }
    };
}
