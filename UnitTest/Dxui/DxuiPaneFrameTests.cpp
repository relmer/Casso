#include "Pch.h"

#include "MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameTests
//
//  A pane's frame as Visual Studio draws it, part by part: the worked checks
//  against Visual Studio's documents at 125% and 150% and a tool window at
//  150%, the bend where the selected tab meets the line, the joins drawn
//  after the tabs, which side a tab is flush with or cut off on, that no
//  part reaches past the pane, and the square frame of a pane too small to
//  round.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPaneFrameTests
{
    using Parts = std::vector<DxuiPaneFramePart>;



    static RECT GetPartRect (const DxuiPaneFramePart & part)
    {
        return RECT { std::lround (part.x), std::lround (part.y), std::lround (part.x + part.width), std::lround (part.y + part.height) };
    }



    //  What a part can reach: its rect, cut to its clip when it has one.
    static RECT GetPartReach (const DxuiPaneFramePart & part)
    {
        RECT  r = GetPartRect (part);



        if (part.clipped)
        {
            r = RECT { (std::max) (r.left,  part.clip.left),  (std::max) (r.top,    part.clip.top),
                       (std::min) (r.right, part.clip.right), (std::min) (r.bottom, part.clip.bottom) };
        }

        return r;
    }



    static bool IsSameRect (const RECT & a, const RECT & b)
    {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }



    static std::wstring Describe (const RECT & r)
    {
        return std::format (L"[{}, {}, {}, {})", r.left, r.top, r.right, r.bottom);
    }



    //  The unclipped plain fill over `rect`, in `role` and `phase`.
    static bool HasRect (const Parts & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & rect)
    {
        return std::any_of (parts.begin(), parts.end(), [&] (const DxuiPaneFramePart & part)
        {
            return part.shape == DxuiPaneFrameShape::Rect && part.phase == phase && part.role == role &&
                   !part.clipped && IsSameRect (GetPartRect (part), rect);
        });
    }



    //  The ring clipped to `box`, in `role` and `phase`.
    static bool TryFindRing (const Parts & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & box, DxuiPaneFramePart & ring)
    {
        bool  found = false;



        for (const DxuiPaneFramePart & part : parts)
        {
            bool  matches = part.shape == DxuiPaneFrameShape::Ring && part.phase == phase && part.role == role &&
                            part.clipped && IsSameRect (part.clip, box);

            if (matches && !found)
            {
                ring  = part;
                found = true;
            }
        }

        return found;
    }



    static bool HasRing (const Parts & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & box)
    {
        DxuiPaneFramePart  ring;



        return TryFindRing (parts, phase, role, box, ring);
    }



    //  The band's rounded fill.
    static bool TryFindBand (const Parts & parts, DxuiPaneFramePart & band)
    {
        bool  found = false;



        for (const DxuiPaneFramePart & part : parts)
        {
            if (part.shape == DxuiPaneFrameShape::RoundedFill && part.role == DxuiPaneFrameRole::Band)
            {
                band  = part;
                found = true;
            }
        }

        return found;
    }



    //  A quarter of the outline: a ring around the circle of radius `ro` at
    //  (cx, cy), `t` thick, seen inside `box`.
    static void AssertQuarterRing (const Parts & parts, long cx, long cy, long ro, long t, const RECT & box)
    {
        DxuiPaneFramePart  ring;
        std::wstring       what  = L"a quarter ring in " + Describe (box);
        bool               found = TryFindRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, box, ring);



        Assert::IsTrue   (found, what.c_str());
        Assert::IsTrue   (IsSameRect (GetPartRect (ring), RECT { cx - ro, cy - ro, cx + ro, cy + ro }), (what + L" centered on its corner").c_str());
        Assert::AreEqual ((float) ro, ring.radius,    what.c_str());
        Assert::AreEqual ((float) t,  ring.thickness, what.c_str());
    }



    //  A join's fillet: the content color in `box` outside the circle of
    //  radius `ro` at (cx, cy).
    static void AssertFillet (const Parts & parts, long cx, long cy, long ro, const RECT & box)
    {
        DxuiPaneFramePart  ring;
        std::wstring       what  = L"a fillet in " + Describe (box);
        bool               found = TryFindRing (parts, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content, box, ring);



        Assert::IsTrue   (found, what.c_str());
        Assert::IsTrue   (IsSameRect (GetPartRect (ring), RECT { cx - 2 * ro, cy - 2 * ro, cx + 2 * ro, cy + 2 * ro }), what.c_str());
        Assert::AreEqual ((float) (2 * ro), ring.radius,    what.c_str());
        Assert::AreEqual ((float) ro,       ring.thickness, what.c_str());
    }



    static size_t CountParts (const Parts & parts, DxuiPaneFrameShape shape, DxuiPaneFramePhase phase, DxuiPaneFrameRole role)
    {
        return (size_t) std::count_if (parts.begin(), parts.end(), [&] (const DxuiPaneFramePart & part)
        {
            return part.shape == shape && part.phase == phase && part.role == role;
        });
    }



    //  An outline run on the line row [lineTop, lineTop + t) that touches
    //  column `x`.
    static bool HasLineRunAt (const Parts & parts, long lineTop, long t, long x)
    {
        return std::any_of (parts.begin(), parts.end(), [&] (const DxuiPaneFramePart & part)
        {
            RECT  r = GetPartRect (part);

            return part.shape == DxuiPaneFrameShape::Rect && part.role == DxuiPaneFrameRole::Outline &&
                   r.top == lineTop && r.bottom == lineTop + t && x >= r.left && x < r.right;
        });
    }



    static DxuiPaneFrameSpec MakeDocument (const RECT & pane, long bandPx, long selLeft, long selRight, const DxuiDpiScaler & scaler)
    {
        DxuiPaneFrameSpec  spec;



        spec.pane        = pane;
        spec.bandPx      = bandPx;
        spec.hasSelected = true;
        spec.selLeft     = selLeft;
        spec.selRight    = selRight;
        spec.linePx      = DxuiPaneMetrics::GetLinePx (scaler);
        spec.cornerPx    = DxuiPaneMetrics::GetCornerPx (scaler);
        return spec;
    }



    static DxuiPaneFrameSpec MakeToolWindow (const RECT & pane, long titlePx, long bandPx, long selLeft, long selRight, const DxuiDpiScaler & scaler)
    {
        DxuiPaneFrameSpec  spec = MakeDocument (pane, bandPx, selLeft, selRight, scaler);



        spec.toolWindow  = true;
        spec.titlePx     = titlePx;
        spec.hasSelected = bandPx > 0;
        return spec;
    }



    static DxuiDpiScaler MakeScaler (UINT dpi)
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi (dpi);
        return scaler;
    }



    TEST_CLASS (DxuiPaneFrameTests)
    {
    public:

        //  Visual Studio at 120 dpi: a pane (5,51)-(806,384) with a 31-px
        //  band and its first tab, Disassembly, selected across [5,164).
        TEST_METHOD (Vs125DocumentGeometry)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 5, 51, 806, 384 }, 31, 5, 164, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);
            DxuiPaneFramePart  band;



            Assert::AreEqual (1L, spec.linePx);
            Assert::AreEqual (6L, spec.cornerPx);

            Assert::IsTrue   (TryFindBand (parts, band), L"the band");
            Assert::IsTrue   (band.phase == DxuiPaneFramePhase::Under);
            Assert::IsTrue   (IsSameRect (band.clip, RECT { 5, 51, 806, 82 }), L"band rows 51-81");
            Assert::IsTrue   (IsSameRect (GetPartRect (band), RECT { 5, 51, 806, 88 }), L"rounded at the top only");
            Assert::AreEqual (6.0f, band.radius);

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { 5,   51, 11,  57 }), L"gap outside the top left");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { 800, 51, 806, 57 }), L"gap outside the top right");

            AssertFillet (parts, 169, 77, 6, RECT { 163, 77, 169, 83 });
            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"no left join");

            Assert::IsTrue (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap, RECT { 5,   378, 11,  384 }), L"cap at the bottom left");
            Assert::IsTrue (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap, RECT { 800, 378, 806, 384 }), L"cap at the bottom right");

            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 169, 82,  806, 83  }), L"line from the join to the right side");
            Assert::IsFalse (HasLineRunAt (parts, 82, 1, 5), L"no line run left of the flush tab");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 163, 57,  164, 77  }), L"tab right side");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 805, 82,  806, 378 }), L"pane right side, square at the line");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 5,   57,  6,   378 }), L"pane left side, the tab's own");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 11,  383, 800, 384 }), L"bottom");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 11,  51,  158, 52  }), L"tab top");

            AssertQuarterRing (parts, 11,  378, 6, 1, RECT { 5,   378, 11,  384 });
            AssertQuarterRing (parts, 800, 378, 6, 1, RECT { 800, 378, 806, 384 });
            AssertQuarterRing (parts, 11,  57,  6, 1, RECT { 5,   51,  11,  57  });
            AssertQuarterRing (parts, 158, 57,  6, 1, RECT { 158, 51,  164, 57  });
            AssertQuarterRing (parts, 169, 77,  6, 1, RECT { 163, 77,  169, 83  });

            Assert::IsTrue (IsSameRect (DxuiPaneFrame::GetBodyRect (spec), RECT { 6, 83, 805, 383 }), L"body");
        }


        //  A tool window at 144 dpi with Disk II selected across [166,310)
        //  in its bottom band, which ends at row 57. The line and the band
        //  under it are 38 rows together: the line on rows 19-20 and the
        //  band on 21-56, with an outer radius of 7.
        TEST_METHOD (ToolWindowWithBottomTabsAt144)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, -200, 700, 57 }, 38, 36, 166, 310, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);
            DxuiPaneFramePart  band;



            Assert::AreEqual (2L, spec.linePx);
            Assert::AreEqual (7L, spec.cornerPx);

            Assert::IsTrue (TryFindBand (parts, band), L"the band");
            Assert::IsTrue (IsSameRect (band.clip, RECT { 0, 21, 700, 57 }), L"band rows 21-56");
            Assert::IsTrue (IsSameRect (GetPartRect (band), RECT { 0, 14, 700, 57 }), L"rounded at the bottom only");

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,   19, 161, 21 }), L"line rows 19-20, left of the join");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 315, 19, 700, 21 }), L"and right of it");

            AssertFillet      (parts, 161, 26, 7,    RECT { 161, 19, 168, 26 });
            AssertQuarterRing (parts, 161, 26, 7, 2, RECT { 161, 19, 168, 26 });
            AssertFillet      (parts, 315, 26, 7,    RECT { 308, 19, 315, 26 });
            AssertQuarterRing (parts, 315, 26, 7, 2, RECT { 308, 19, 315, 26 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 166, 26, 168, 50 }), L"tab left side, rows 26-49");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 308, 26, 310, 50 }), L"tab right side");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 173, 55, 303, 57 }), L"tab bottom");

            AssertQuarterRing (parts, 173, 50, 7, 2, RECT { 166, 50, 173, 57 });
            AssertQuarterRing (parts, 303, 50, 7, 2, RECT { 303, 50, 310, 57 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,   -193, 2,   21 }), L"left side, square at the line");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 698, -193, 700, 21 }), L"right side, square at the line");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 7,   -200, 693, -198 }), L"top");

            Assert::AreEqual ((size_t) 0, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap), L"no caps: the title and band are the group's own fills");
            Assert::IsTrue (IsSameRect (DxuiPaneFrame::GetBodyRect (spec), RECT { 2, -162, 698, 19 }), L"body");
        }


        //  Visual Studio at 144 dpi, focused: a document pane from (3,62) to
        //  x 804 with its first tab, Disassembly, selected across [3,196).
        //  The band is rows 62-99 and the line rows 100-101.
        TEST_METHOD (Vs150DocumentGeometry)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 3, 62, 804, 500 }, 38, 3, 196, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::AreEqual  (7L, spec.cornerPx);
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 201, 100, 804, 102 }), L"line rows 100-101 from x 201, square at the right side");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 802, 100, 804, 493 }), L"the right side from the line");
            AssertFillet      (parts, 201, 95, 7,    RECT { 194, 95, 201, 102 });
            AssertQuarterRing (parts, 201, 95, 7, 2, RECT { 194, 95, 201, 102 });
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 194, 69, 196, 95 }), L"tab side x 194-195, rows 69-94");
            AssertQuarterRing (parts, 10,  69, 7, 2, RECT { 3,   62, 10,  69 });
            AssertQuarterRing (parts, 189, 69, 7, 2, RECT { 189, 62, 196, 69 });
        }


        //  Visual Studio at 144 dpi, unfocused: Memory 3 selected across
        //  [202,378) in a pane from (9,47) to x 810, the line on rows 85-86.
        TEST_METHOD (Vs150UnfocusedDocumentGeometry)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 9, 47, 810, 500 }, 38, 202, 378, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 9,   85, 197, 87 }), L"line run [9,197)");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 383, 85, 810, 87 }), L"line run [383,810)");
            AssertQuarterRing (parts, 197, 80, 7, 2, RECT { 197, 80, 204, 87 });
            AssertQuarterRing (parts, 383, 80, 7, 2, RECT { 376, 80, 383, 87 });
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 202, 54, 204, 80 }), L"tab left side, rows 54-79");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 376, 54, 378, 80 }), L"tab right side");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 209, 47, 371, 49 }), L"tab top rows 47-48, x [209,371)");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 9,   85, 11,  493 }), L"the pane's sides from row 85, a square corner");
        }


        //  The fillets that flare the selected tab into the line are drawn
        //  after the tabs, in their own phase, so they lie over a hovered
        //  neighbor's fill; none goes down with the band.
        TEST_METHOD (FilletsAreDrawnAfterTheTabs)
        {
            for (bool toolWindow : { false, true })
            {
                DxuiDpiScaler  scaler  = MakeScaler (120);
                RECT           pane    = { 10, 20, 610, 420 };
                Parts          parts   = DxuiPaneFrame::Build (toolWindow ? MakeToolWindow (pane, 31, 30, 200, 300, scaler) : MakeDocument (pane, 31, 200, 300, scaler));
                size_t         fillets = 0;

                for (const DxuiPaneFramePart & part : parts)
                {
                    if (part.role != DxuiPaneFrameRole::Content || part.shape != DxuiPaneFrameShape::Ring)
                    {
                        continue;
                    }

                    fillets++;
                    Assert::IsTrue (part.phase == DxuiPaneFramePhase::Joins, L"a fillet is drawn after the tabs");
                }

                Assert::AreEqual ((size_t) 2, fillets, L"a fillet each side of the tab");
                Assert::IsTrue   (std::all_of (parts.begin(), parts.end(), [] (const DxuiPaneFramePart & part)
                                  {
                                      return part.phase != DxuiPaneFramePhase::Joins || (part.role == DxuiPaneFrameRole::Content && part.shape == DxuiPaneFrameShape::Ring);
                                  }), L"and nothing else is");
            }
        }


        //  Each join's ring is tangent to the tab side's own column and to
        //  the line's own row, so the outline bends with no jog.
        TEST_METHOD (BendMeetsTheSideLine)
        {
            for (UINT dpi : { 96u, 120u, 144u, 168u, 192u })
            {
                for (bool toolWindow : { false, true })
                {
                    DxuiDpiScaler      scaler  = MakeScaler (dpi);
                    long               t       = DxuiPaneMetrics::GetLinePx (scaler);
                    long               band    = scaler.ToPx (DxuiTabGroup::kStripDip);
                    RECT               pane    = { 40, 30, 840, 630 };
                    long               sl      = 200;
                    long               sr      = 330;
                    DxuiPaneFrameSpec  spec    = toolWindow ? MakeToolWindow (pane, scaler.ToPx (DxuiTabGroup::kTitleDip), band - t, sl, sr, scaler)
                                                            : MakeDocument (pane, band, sl, sr, scaler);
                    long               lineTop = toolWindow ? pane.bottom - band : pane.top + band;
                    Parts              parts   = DxuiPaneFrame::Build (spec);
                    size_t             joins   = 0;
                    std::wstring       where   = std::format (L"{} dpi, {}", dpi, toolWindow ? L"tool window" : L"document");

                    for (const DxuiPaneFramePart & part : parts)
                    {
                        RECT  r      = GetPartRect (part);
                        bool  isJoin = part.shape == DxuiPaneFrameShape::Ring && part.phase == DxuiPaneFramePhase::Over &&
                                       part.role == DxuiPaneFrameRole::Outline && part.clip.top >= lineTop - spec.cornerPx &&
                                       part.clip.bottom <= lineTop + t + spec.cornerPx;

                        if (!isJoin)
                        {
                            continue;
                        }

                        joins++;

                        if (part.clip.right <= (sl + sr) / 2)
                        {
                            Assert::AreEqual (sl + t, r.right, (where + L": left join meets the tab side's column").c_str());
                        }
                        else
                        {
                            Assert::AreEqual (sr - t, r.left, (where + L": right join meets the tab side's column").c_str());
                        }

                        Assert::AreEqual (toolWindow ? lineTop : lineTop + t, toolWindow ? r.top : r.bottom, (where + L": join meets the line's row").c_str());
                    }

                    Assert::AreEqual ((size_t) 2, joins, (where + L": a join each side").c_str());
                }
            }
        }


        //  Every fillet and its ring lie in an Ro x Ro box in the Ro rows
        //  next to the line, so a join reaches no further into a neighbor.
        TEST_METHOD (JoinsStayInTheirQuadrants)
        {
            for (bool toolWindow : { false, true })
            {
                DxuiDpiScaler      scaler  = MakeScaler (144);
                long               t       = DxuiPaneMetrics::GetLinePx (scaler);
                long               ro      = DxuiPaneMetrics::GetCornerPx (scaler);
                long               band    = scaler.ToPx (DxuiTabGroup::kStripDip);
                RECT               pane    = { 0, 0, 900, 500 };
                DxuiPaneFrameSpec  spec    = toolWindow ? MakeToolWindow (pane, 38, band - t, 300, 420, scaler) : MakeDocument (pane, band, 300, 420, scaler);
                long               lineTop = toolWindow ? pane.bottom - band : band;
                long               rowsTop = toolWindow ? lineTop : lineTop + t - ro;
                Parts              parts   = DxuiPaneFrame::Build (spec);
                size_t             fillets = 0;

                for (const DxuiPaneFramePart & part : parts)
                {
                    if (part.shape != DxuiPaneFrameShape::Ring || part.role != DxuiPaneFrameRole::Content)
                    {
                        continue;
                    }

                    fillets++;
                    Assert::IsTrue   (part.clipped);
                    Assert::AreEqual (ro,      part.clip.right  - part.clip.left, L"Ro wide");
                    Assert::AreEqual (ro,      part.clip.bottom - part.clip.top,  L"Ro deep");
                    Assert::AreEqual (rowsTop, part.clip.top,                     L"in the Ro rows next to the line");
                    Assert::IsTrue   (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, part.clip), L"its ring shares its box");
                }

                Assert::AreEqual ((size_t) 2, fillets, L"a fillet each side of a tab in the middle");
            }
        }


        //  A first tab's rounded corner is the pane's own: no left join, no
        //  line left of it, and the pane's left side runs up into the tab.
        TEST_METHOD (FirstTabRoundsThePaneCorner)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 10, 150, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"the right join only");
            Assert::IsFalse  (HasLineRunAt (parts, 51, 1, 10), L"no line left of the tab");
            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 26, 11, 414 }), L"the pane's left side starts at PT + Ro");
            AssertQuarterRing (parts, 16, 26, 6, 1, RECT { 10, 20, 16, 26 });
        }


        TEST_METHOD (LastTabFlushRight)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 470, 610, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"the left join only");
            AssertFillet     (parts, 465, 46, 6, RECT { 465, 46, 471, 52 });
            Assert::IsFalse  (HasLineRunAt (parts, 51, 1, 609), L"no line right of the tab");
            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  51, 465, 52 }), L"the line up to the left join");
            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 26, 610, 414 }), L"the pane's right side starts at PT + Ro");
            AssertQuarterRing (parts, 604, 26, 6, 1, RECT { 604, 20, 610, 26 });
        }


        //  Where the tab is not flush, the line meets the pane's side in a
        //  square corner: the side starts on the line's row (a document) or
        //  ends below it (a tool window).
        TEST_METHOD (LineMeetsTheSideSquare)
        {
            DxuiDpiScaler      scaler   = MakeScaler (120);
            Parts              document = DxuiPaneFrame::Build (MakeDocument (RECT { 10, 20, 610, 420 }, 31, 200, 300, scaler));
            Parts              tool     = DxuiPaneFrame::Build (MakeToolWindow (RECT { 10, 20, 610, 420 }, 31, 30, 200, 300, scaler));



            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  51, 11,  414 }), L"document left side from the line");
            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 51, 610, 414 }), L"document right side from the line");
            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  51, 195, 52  }), L"the line from the left side");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  26, 11,  390 }), L"tool window left side to the line");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 26, 610, 390 }), L"tool window right side to the line");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  389, 195, 390 }), L"the line from the left side, 31 rows above the bottom");
        }


        //  A tool window with one pane has no tabs: its outline runs all the
        //  way around it, with the gap outside its rounded bottom corners.
        TEST_METHOD (NoStripToolWindowIsFullyRounded)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 10, 20, 610, 420 }, 31, 0, 0, 0, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::IsTrue    (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap, RECT { 10,  414, 16,  420 }), L"cap at the bottom left");
            Assert::IsTrue    (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap, RECT { 604, 414, 610, 420 }), L"cap at the bottom right");
            Assert::AreEqual  ((size_t) 4, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline), L"four corners");

            AssertQuarterRing (parts, 16,  26,  6, 1, RECT { 10,  20,  16,  26  });
            AssertQuarterRing (parts, 604, 26,  6, 1, RECT { 604, 20,  610, 26  });
            AssertQuarterRing (parts, 16,  414, 6, 1, RECT { 10,  414, 16,  420 });
            AssertQuarterRing (parts, 604, 414, 6, 1, RECT { 604, 414, 610, 420 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 16,  20,  604, 21  }), L"top");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 16,  419, 604, 420 }), L"bottom");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  26,  11,  414 }), L"left");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 26,  610, 414 }), L"right");
            Assert::IsTrue (IsSameRect (DxuiPaneFrame::GetBodyRect (spec), RECT { 11, 51, 609, 419 }), L"body");
        }


        TEST_METHOD (DocumentWithNoSelectionDrawsTheFullLine)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 0, 0, scaler);
            Parts              parts  = {};



            spec.hasSelected = false;
            parts            = DxuiPaneFrame::Build (spec);

            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 51, 610, 52 }), L"the line across the pane");
            Assert::AreEqual ((size_t) 0, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"no joins");
            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 51, 11, 414 }), L"the left side from the line");
        }


        //  A tab cut off by a scroll arrow has no side, corner or join there;
        //  its top runs on to the arrow, and so does the line.
        TEST_METHOD (OpenSideHasNoSideOrJoin)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 45, 200, scaler);
            Parts              parts  = {};



            spec.openLeft = true;
            parts         = DxuiPaneFrame::Build (spec);

            Assert::AreEqual  ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"the right join only");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 51, 45,  52 }), L"the line up to the arrow");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 20, 194, 21 }), L"the tab's top from the arrow");
            Assert::IsFalse   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 26, 46,  46 }), L"no left side");
            Assert::IsFalse   (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 20, 51, 26 }), L"no rounded corner");
            Assert::IsTrue    (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 194, 20, 200, 26 }), L"the right corner stays");
        }


        //  A pane rounds its corners at 5, 6 and 7 px at 100%, 125% and
        //  150% while it is four radii across both ways, and not below that.
        TEST_METHOD (APaneUnderFourRadiiAcrossIsSquare)
        {
            constexpr UINT  kDpis[]  = { 96, 120, 144 };
            constexpr long  kRadii[] = { 5, 6, 7 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiDpiScaler  scaler = MakeScaler (kDpis[i]);
                long           ro     = DxuiPaneMetrics::GetCornerPx (scaler);
                long           across = 4 * kRadii[i];
                std::wstring   at     = std::format (L"{} dpi", kDpis[i]);

                Assert::AreEqual (kRadii[i], ro, at.c_str());
                Assert::AreEqual (kRadii[i], DxuiPaneFrame::GetCornerPx (RECT { 0, 0, across,     across     }, ro), (L"four radii across, " + at).c_str());
                Assert::AreEqual (0L,        DxuiPaneFrame::GetCornerPx (RECT { 0, 0, across - 1, 400        }, ro), (L"a pixel too narrow, " + at).c_str());
                Assert::AreEqual (0L,        DxuiPaneFrame::GetCornerPx (RECT { 0, 0, 400,        across - 1 }, ro), (L"a pixel too short, " + at).c_str());
            }
        }


        //  A pane too small to round is square: no ring, no rounded fill.
        TEST_METHOD (TinyPaneIsSquare)
        {
            DxuiDpiScaler  scaler = MakeScaler (120);



            for (bool toolWindow : { false, true })
            {
                DxuiPaneFrameSpec  spec  = toolWindow ? MakeToolWindow (RECT { 0, 0, 22, 60 }, 10, 8, 4, 14, scaler) : MakeDocument (RECT { 0, 0, 22, 60 }, 8, 4, 14, scaler);
                Parts              parts = DxuiPaneFrame::Build (spec);
                RECT               body  = DxuiPaneFrame::GetBodyRect (spec);

                Assert::IsFalse (parts.empty());

                for (const DxuiPaneFramePart & part : parts)
                {
                    Assert::IsTrue (part.shape == DxuiPaneFrameShape::Rect, L"square pieces only");
                }

                Assert::IsTrue (body.right >= body.left && body.bottom >= body.top, L"the body is never inverted");
            }
        }


        //  A last tab ending 2 px short of the pane's right side at 120 dpi,
        //  nearer than a join's 5-px reach, is drawn flush with it: no right
        //  join, no line right of it, its top running to the pane's rounded
        //  corner, and the pane's side up to that corner. A tab ending the
        //  whole reach short keeps its join, inside the pane.
        TEST_METHOD (ATabEndingJustShortOfTheSideIsDrawnFlush)
        {
            DxuiDpiScaler      scaler   = MakeScaler (120);
            DxuiPaneFrameSpec  spec     = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 470, 608, scaler);
            Parts              parts    = DxuiPaneFrame::Build (spec);
            Parts              boundary = DxuiPaneFrame::Build (MakeDocument (RECT { 10, 20, 610, 420 }, 31, 470, 605, scaler));



            Assert::AreEqual  (5L, DxuiPaneFrame::GetFlushReachPx (spec.cornerPx, spec.linePx), L"the outer radius less a line");
            Assert::AreEqual  ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"the left join only");
            Assert::IsFalse   (HasLineRunAt (parts, 51, 1, 609), L"no line right of the tab");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 476, 20, 604, 21  }), L"the tab's top runs to the pane's corner");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 26, 610, 414 }), L"the pane's right side starts at PT + Ro");
            AssertQuarterRing (parts, 604, 26, 6, 1, RECT { 604, 20, 610, 26 });

            Assert::AreEqual  ((size_t) 2, CountParts (boundary, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Joins, DxuiPaneFrameRole::Content), L"a join each side");
            AssertFillet      (boundary, 610, 46, 6, RECT { 604, 46, 610, 52 });
        }


        //  However near a side of the pane the selected tab ends, no part of
        //  the frame reaches past the pane, at any scale, in either kind.
        TEST_METHOD (NoPartReachesPastThePane)
        {
            for (UINT dpi : { 96u, 120u, 144u, 168u })
            {
                for (bool toolWindow : { false, true })
                {
                    DxuiDpiScaler  scaler = MakeScaler (dpi);
                    long           ro     = DxuiPaneMetrics::GetCornerPx (scaler);
                    long           band   = scaler.ToPx (DxuiTabGroup::kStripDip);
                    long           title  = scaler.ToPx (DxuiTabGroup::kTitleDip);
                    RECT           pane   = { 40, 30, 840, 630 };
                    size_t         parts  = 0;

                    for (long gap = 0; gap <= 2 * ro; gap++)
                    {
                        std::wstring                    where = std::format (L"{} dpi, {}, {} px from a side", dpi, toolWindow ? L"tool window" : L"document", gap);
                        std::vector<DxuiPaneFrameSpec>  specs = toolWindow ? std::vector<DxuiPaneFrameSpec> { MakeToolWindow (pane, title, band, 500, pane.right - gap, scaler),
                                                                                                              MakeToolWindow (pane, title, band, pane.left + gap, 300, scaler) }
                                                                           : std::vector<DxuiPaneFrameSpec> { MakeDocument (pane, band, 500, pane.right - gap, scaler),
                                                                                                              MakeDocument (pane, band, pane.left + gap, 300, scaler) };

                        for (const DxuiPaneFrameSpec & spec : specs)
                        {
                            for (const DxuiPaneFramePart & part : DxuiPaneFrame::Build (spec))
                            {
                                RECT  reach = GetPartReach (part);

                                if (reach.right <= reach.left || reach.bottom <= reach.top)
                                {
                                    continue;
                                }

                                parts++;
                                Assert::IsTrue (reach.left >= pane.left && reach.top >= pane.top && reach.right <= pane.right && reach.bottom <= pane.bottom,
                                                (where + L": " + Describe (reach) + L" stays in the pane").c_str());
                            }
                        }
                    }

                    Assert::IsTrue (parts > 0, L"parts were checked");
                }
            }
        }


        //  A floating tool window at 144 dpi whose four corners are its
        //  window's, rounded by Windows at 12 px: each corner's ring is 12 px
        //  outside and 10 inside, the title is rounded at 12, the straight
        //  runs stop 12 px from each corner, and no gap color lies outside
        //  them. The body is where it always is.
        TEST_METHOD (AFloatingPaneFollowsTheWindowsCorners)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, 0, 700, 500 }, 38, 0, 0, 0, scaler);
            Parts              parts;
            DxuiPaneFramePart  title;
            size_t             fills  = 0;



            spec.windowCorners  = DxuiPaneFrame::kCornerTopLeft | DxuiPaneFrame::kCornerTopRight | DxuiPaneFrame::kCornerBottomLeft | DxuiPaneFrame::kCornerBottomRight;
            spec.windowCornerPx = scaler.ToPx (DxuiPaneMetrics::kWindowCornerDip);
            parts               = DxuiPaneFrame::Build (spec);

            Assert::AreEqual (12L, spec.windowCornerPx, L"8 DIP, 12 px at 150%");

            AssertQuarterRing (parts, 12,  12,  12, 2, RECT { 0,   0,   12,  12  });
            AssertQuarterRing (parts, 688, 12,  12, 2, RECT { 688, 0,   700, 12  });
            AssertQuarterRing (parts, 12,  488, 12, 2, RECT { 0,   488, 12,  500 });
            AssertQuarterRing (parts, 688, 488, 12, 2, RECT { 688, 488, 700, 500 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 12,  0,   688, 2   }), L"top");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,   12,  2,   488 }), L"left");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 698, 12,  700, 488 }), L"right");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 12,  498, 688, 500 }), L"bottom");

            for (const DxuiPaneFramePart & part : parts)
            {
                Assert::IsTrue (part.role != DxuiPaneFrameRole::Gap, L"no gap color at the window's corners");

                if (part.shape == DxuiPaneFrameShape::RoundedFill && part.role == DxuiPaneFrameRole::Content)
                {
                    title = part;
                    fills++;
                }
            }

            Assert::AreEqual ((size_t) 1, fills, L"one title fill");
            Assert::IsTrue   (IsSameRect (title.clip, RECT { 0, 0, 700, 38 }), L"over the title");
            Assert::IsTrue   (IsSameRect (GetPartRect (title), RECT { 0, 0, 700, 50 }), L"rounded at the top only");
            Assert::AreEqual (12.0f, title.radius, L"at the window's radius");
            Assert::IsTrue   (IsSameRect (DxuiPaneFrame::GetBodyRect (spec), RECT { 2, 38, 698, 498 }), L"body");
        }


        //  With tabs along its bottom and the first one selected, a floating
        //  tool window's bottom corners are the band's, rounded at the
        //  window's radius with no gap color outside them, and the selected
        //  tab's own corner at the window's corner is the window's: its ring
        //  is 12 px outside, the tab's bottom runs from 12 px in, and the
        //  pane's left side runs down to the ring.
        TEST_METHOD (AFloatingPanesFirstTabTakesTheWindowsCorner)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, 0, 700, 500 }, 38, 36, 0, 166, scaler);
            Parts              parts;
            DxuiPaneFramePart  band;



            spec.windowCorners  = DxuiPaneFrame::kCornerTopLeft | DxuiPaneFrame::kCornerTopRight | DxuiPaneFrame::kCornerBottomLeft | DxuiPaneFrame::kCornerBottomRight;
            spec.windowCornerPx = 12;
            parts               = DxuiPaneFrame::Build (spec);

            Assert::IsTrue   (TryFindBand (parts, band), L"the band");
            Assert::IsTrue   (IsSameRect (band.clip, RECT { 0, 464, 700, 500 }), L"band rows 464-499");
            Assert::IsTrue   (IsSameRect (GetPartRect (band), RECT { 0, 452, 700, 500 }), L"rounded at the bottom only");
            Assert::AreEqual (12.0f, band.radius, L"at the window's radius");

            AssertQuarterRing (parts, 12, 488, 12, 2, RECT { 0, 488, 12, 500 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 12, 498, 159, 500 }), L"the tab's bottom from the window's corner");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,  12,  2,   488 }), L"the left side down to the ring");
            Assert::AreEqual ((size_t) 0, CountParts (parts, DxuiPaneFrameShape::Rect, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap), L"no gap boxes");
        }


        //  A pane whose top-left corner alone is its window's rounds that
        //  corner at the window's radius and the others at its own: the
        //  title's fill is drawn in two halves, each rounded at its corner's
        //  radius, and only the top right keeps the gap color outside it.
        TEST_METHOD (APaneWithOneWindowCornerRoundsEachCornerAtItsOwnRadius)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, 0, 700, 500 }, 38, 0, 0, 0, scaler);
            Parts              parts;
            DxuiPaneFramePart  left;
            DxuiPaneFramePart  right;
            size_t             fills  = 0;



            spec.windowCorners  = DxuiPaneFrame::kCornerTopLeft;
            spec.windowCornerPx = 12;
            parts               = DxuiPaneFrame::Build (spec);

            for (const DxuiPaneFramePart & part : parts)
            {
                if (part.shape != DxuiPaneFrameShape::RoundedFill || part.role != DxuiPaneFrameRole::Content)
                {
                    continue;
                }

                fills++;
                left  = (part.clip.left == 0) ? part : left;
                right = (part.clip.left != 0) ? part : right;
            }

            Assert::AreEqual ((size_t) 2, fills, L"the title in two halves");
            Assert::IsTrue   (IsSameRect (left.clip,  RECT { 0,   0, 350, 38 }), L"the left half");
            Assert::AreEqual (12.0f, left.radius,  L"at the window's radius");
            Assert::IsTrue   (IsSameRect (right.clip, RECT { 350, 0, 700, 38 }), L"the right half");
            Assert::AreEqual (7.0f,  right.radius, L"at the pane's own");

            AssertQuarterRing (parts, 12,  12, 12, 2, RECT { 0,   0, 12,  12 });
            AssertQuarterRing (parts, 693, 7,  7,  2, RECT { 693, 0, 700, 7  });

            Assert::IsFalse (HasRect (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { 0,   0, 12,  12 }), L"no gap at the window's corner");
            Assert::IsTrue  (HasRect (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { 693, 0, 700, 7  }), L"the gap at the pane's own");
        }


        //  The window's corner is 8 DIP at every scale: 8, 10 and 12 px at
        //  100%, 125% and 150%, its ring 7, 9 and 10 px inside, one line in.
        TEST_METHOD (TheWindowsCornerScalesWithTheDpi)
        {
            constexpr UINT  kDpis[]   = { 96, 120, 144 };
            constexpr long  kOuters[] = { 8, 10, 12 };
            constexpr long  kInners[] = { 7, 9, 10 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiDpiScaler      scaler = MakeScaler (kDpis[i]);
                DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, 0, 700, 500 }, 38, 0, 0, 0, scaler);
                DxuiPaneFramePart  ring;
                RECT               box    = {};
                std::wstring       at     = std::format (L"{} dpi", kDpis[i]);

                spec.windowCorners  = DxuiPaneFrame::kCornerTopLeft;
                spec.windowCornerPx = scaler.ToPx (DxuiPaneMetrics::kWindowCornerDip);
                box                 = RECT { 0, 0, kOuters[i], kOuters[i] };

                Assert::IsTrue   (TryFindRing (DxuiPaneFrame::Build (spec), DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, box, ring), (L"the corner's ring, " + at).c_str());
                Assert::AreEqual ((float) kOuters[i], ring.radius,                  (L"outside, " + at).c_str());
                Assert::AreEqual ((float) kInners[i], ring.radius - ring.thickness, (L"inside, " + at).c_str());
            }
        }
    };
}
