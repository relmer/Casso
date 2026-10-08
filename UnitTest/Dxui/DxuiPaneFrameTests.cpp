#include "Pch.h"

#include "MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameTests
//
//  A pane's frame as Visual Studio draws it, part by part: the worked checks
//  against Visual Studio at 125% and the owner's capture at 150%, the bend
//  where the selected tab meets the line, which side a tab is flush with or
//  cut off on, and the square frame of a pane too small to round.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPaneFrameTests
{
    using Parts = std::vector<DxuiPaneFramePart>;



    static RECT GetPartRect (const DxuiPaneFramePart & part)
    {
        return RECT { std::lround (part.x), std::lround (part.y), std::lround (part.x + part.width), std::lround (part.y + part.height) };
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



    //  A quarter of the outline: a ring round the circle of radius `ro` at
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
        bool               found = TryFindRing (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content, box, ring);



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
            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content), L"no left join");

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


        //  The owner's capture at 144 dpi: Disk II selected across [166,310)
        //  in a tool window's bottom band ending at row 57.
        TEST_METHOD (ToolWindowWithBottomTabsAt144)
        {
            DxuiDpiScaler      scaler = MakeScaler (144);
            DxuiPaneFrameSpec  spec   = MakeToolWindow (RECT { 0, -200, 700, 57 }, 38, 38, 166, 310, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);
            DxuiPaneFramePart  band;



            Assert::AreEqual (2L, spec.linePx);
            Assert::AreEqual (8L, spec.cornerPx);

            Assert::IsTrue (TryFindBand (parts, band), L"the band");
            Assert::IsTrue (IsSameRect (band.clip, RECT { 0, 19, 700, 57 }), L"band rows 19-56");
            Assert::IsTrue (IsSameRect (GetPartRect (band), RECT { 0, 11, 700, 57 }), L"rounded at the bottom only");

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,   17, 160, 19 }), L"line rows 17-18, left of the join");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 316, 17, 700, 19 }), L"and right of it");

            AssertFillet      (parts, 160, 25, 8,    RECT { 160, 17, 168, 25 });
            AssertQuarterRing (parts, 160, 25, 8, 2, RECT { 160, 17, 168, 25 });
            AssertFillet      (parts, 316, 25, 8,    RECT { 308, 17, 316, 25 });
            AssertQuarterRing (parts, 316, 25, 8, 2, RECT { 308, 17, 316, 25 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 166, 25, 168, 49 }), L"tab left side");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 308, 25, 310, 49 }), L"tab right side");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 174, 55, 302, 57 }), L"tab bottom");

            AssertQuarterRing (parts, 174, 49, 8, 2, RECT { 166, 49, 174, 57 });
            AssertQuarterRing (parts, 302, 49, 8, 2, RECT { 302, 49, 310, 57 });

            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 0,   -192, 2,   19 }), L"left side, square at the line");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 698, -192, 700, 19 }), L"right side, square at the line");
            Assert::IsTrue (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 8,   -200, 692, -198 }), L"top");

            Assert::AreEqual ((size_t) 0, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap), L"no caps: the title and band are the group's own fills");
            Assert::IsTrue (IsSameRect (DxuiPaneFrame::GetBodyRect (spec), RECT { 2, -162, 698, 17 }), L"body");
        }


        //  Each join's ring is tangent to the tab side's own column and to
        //  the line's own row, so the outline bends with no jog.
        TEST_METHOD (BendMeetsTheSideLine)
        {
            for (UINT dpi : { 96u, 120u, 144u, 168u, 192u })
            {
                for (bool toolWindow : { false, true })
                {
                    DxuiDpiScaler      scaler = MakeScaler (dpi);
                    long               t      = DxuiPaneMetrics::GetLinePx (scaler);
                    long               band   = scaler.ToPx (DxuiTabGroup::kStripDip);
                    RECT               pane   = { 40, 30, 840, 630 };
                    long               sl     = 200;
                    long               sr     = 330;
                    DxuiPaneFrameSpec  spec   = toolWindow ? MakeToolWindow (pane, scaler.ToPx (DxuiTabGroup::kTitleDip), band, sl, sr, scaler)
                                                           : MakeDocument (pane, band, sl, sr, scaler);
                    long               lineTop = toolWindow ? pane.bottom - band - t : pane.top + band;
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
        //  next to the line, so nothing of a join reaches a neighbor's pill.
        TEST_METHOD (JoinsStayInTheirQuadrants)
        {
            for (bool toolWindow : { false, true })
            {
                DxuiDpiScaler      scaler  = MakeScaler (144);
                long               t       = DxuiPaneMetrics::GetLinePx (scaler);
                long               ro      = DxuiPaneMetrics::GetCornerPx (scaler);
                long               band    = scaler.ToPx (DxuiTabGroup::kStripDip);
                RECT               pane    = { 0, 0, 900, 500 };
                DxuiPaneFrameSpec  spec    = toolWindow ? MakeToolWindow (pane, 38, band, 300, 420, scaler) : MakeDocument (pane, band, 300, 420, scaler);
                long               lineTop = toolWindow ? pane.bottom - band - t : band;
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



            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content), L"the right join only");
            Assert::IsFalse  (HasLineRunAt (parts, 51, 1, 10), L"no line left of the tab");
            Assert::IsTrue   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 26, 11, 414 }), L"the pane's left side starts at PT + Ro");
            AssertQuarterRing (parts, 16, 26, 6, 1, RECT { 10, 20, 16, 26 });
        }


        TEST_METHOD (LastTabFlushRight)
        {
            DxuiDpiScaler      scaler = MakeScaler (120);
            DxuiPaneFrameSpec  spec   = MakeDocument (RECT { 10, 20, 610, 420 }, 31, 470, 610, scaler);
            Parts              parts  = DxuiPaneFrame::Build (spec);



            Assert::AreEqual ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content), L"the left join only");
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
            Parts              tool     = DxuiPaneFrame::Build (MakeToolWindow (RECT { 10, 20, 610, 420 }, 31, 31, 200, 300, scaler));



            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  51, 11,  414 }), L"document left side from the line");
            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 51, 610, 414 }), L"document right side from the line");
            Assert::IsTrue (HasRect (document, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  51, 195, 52  }), L"the line from the left side");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  26, 11,  389 }), L"tool window left side to the line");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 609, 26, 610, 389 }), L"tool window right side to the line");
            Assert::IsTrue (HasRect (tool,     DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10,  388, 195, 389 }), L"the line from the left side");
        }


        //  A tool window with one pane has no tabs: its outline runs right
        //  round it, with the gap outside its rounded bottom corners.
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
            Assert::AreEqual ((size_t) 0, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content), L"no joins");
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

            Assert::AreEqual  ((size_t) 1, CountParts (parts, DxuiPaneFrameShape::Ring, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content), L"the right join only");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 10, 51, 45,  52 }), L"the line up to the arrow");
            Assert::IsTrue    (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 20, 194, 21 }), L"the tab's top from the arrow");
            Assert::IsFalse   (HasRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 26, 46,  46 }), L"no left side");
            Assert::IsFalse   (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 45, 20, 51, 26 }), L"no rounded corner");
            Assert::IsTrue    (HasRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { 194, 20, 200, 26 }), L"the right corner stays");
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
    };
}
