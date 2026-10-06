#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarBandsTests
//
//  The bands docked toolbars stand in: a place's band in its saved text and
//  places saved before bands existed, which band each toolbar gets, how the
//  toolbars of one band share its length, and where a drop goes, into a
//  band or into a new one between two.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarBandsTests)
{
public:

    using  Edge  = DxuiToolbarDock::Edge;
    using  Bands = DxuiToolbarBands;

    static constexpr RECT  kArea    = { 0, 100, 1000, 800 };
    static constexpr int   kMargin  = 8;
    static constexpr int   kTall    = 72;
    static constexpr int   kThin    = 40;
    static constexpr int   kBarLen  = 200;
    static constexpr int   kSplit   = 8;
    static constexpr int   kReach   = 66;


    static void AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
    {
        std::wstring  text = std::format (L"{} expected {{{}, {}, {}, {}}} actual {{{}, {}, {}, {}}}", msg,
                                          expected.left, expected.top, expected.right, expected.bottom,
                                          actual.left, actual.top, actual.right, actual.bottom);

        Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, text.c_str());
    }


    static Bands::Bar MakeBar (Edge edge, int band, int offsetPx, int lengthPx, int thicknessPx, bool fills)
    {
        Bands::Bar  bar;

        bar.edge        = edge;
        bar.band        = band;
        bar.offsetPx    = offsetPx;
        bar.lengthPx    = lengthPx;
        bar.thicknessPx = thicknessPx;
        bar.fills       = fills;
        return bar;
    }


    static DxuiToolbarDock MakeDock (Edge edge, int offsetDip, int band)
    {
        DxuiToolbarDock  dock;

        dock.edge      = edge;
        dock.offsetDip = offsetDip;
        dock.band      = band;
        return dock;
    }


    //
    //  Saved text
    //

    TEST_METHOD (APlaceWithABandRoundTripsThroughText)
    {
        DxuiToolbarDock  dock = MakeDock (Edge::Right, 120, 2);

        Assert::AreEqual (std::wstring (L"right 120 band 2"), dock.ToText());
        Assert::IsTrue   (DxuiToolbarDock::FromText (dock.ToText()) == dock);
    }


    TEST_METHOD (APlaceSavedBeforeBandsLoadsWithNoBandAndSavesAsItWas)
    {
        DxuiToolbarDock  dock = DxuiToolbarDock::FromText (L"bottom 40");

        Assert::IsTrue   (dock.edge == Edge::Bottom);
        Assert::AreEqual (40, dock.offsetDip);
        Assert::IsFalse  (dock.HasBand(), L"old text gives no band");
        Assert::AreEqual (std::wstring (L"bottom 40"), dock.ToText(), L"and a place with none writes the old text");
    }


    TEST_METHOD (ABandThatDoesNotReadGivesTheTop)
    {
        for (const wchar_t * text : { L"top 4 band", L"top 4 band x", L"top 4 band -1", L"top 4 bands 2", L"top 4 band 2x", L"top 4band 2" })
        {
            Assert::IsTrue (DxuiToolbarDock::FromText (text) == DxuiToolbarDock {}, text);
        }
    }


    TEST_METHOD (AFloatingPlaceKeepsNoBandInItsText)
    {
        DxuiToolbarDock  dock = MakeDock (Edge::Top, 0, 1);

        dock.floating = true;
        dock.floatPx  = POINT { 300, 40 };

        Assert::AreEqual (std::wstring (L"float 300 40"), dock.ToText());
    }


    //
    //  Band assignment
    //

    TEST_METHOD (OldPlacesOnOneEdgeEachTakeABandInTheOrderGiven)
    {
        DxuiToolbarDock  strip   = DxuiToolbarDock::FromText (L"top 0");
        DxuiToolbarDock  toolbar = DxuiToolbarDock::FromText (L"top 0");
        DxuiToolbarDock  other   = DxuiToolbarDock::FromText (L"left 30");

        Assert::IsTrue   (Bands::AssignBands ({ &strip, &toolbar, &other }), L"places changed");
        Assert::AreEqual (0, strip.band,   L"the first given stands against the edge, as the timeline did");
        Assert::AreEqual (1, toolbar.band, L"the next inside it, as the command bar did");
        Assert::AreEqual (0, other.band,   L"another edge starts its own bands");
    }


    TEST_METHOD (AnOldPlaceGoesInsideTheBandsItsEdgeHas)
    {
        DxuiToolbarDock  banded = MakeDock (Edge::Top, 0, 0);
        DxuiToolbarDock  old    = DxuiToolbarDock::FromText (L"top 0");

        Bands::AssignBands ({ &old, &banded });

        Assert::AreEqual (0, banded.band, L"a place with a band keeps it");
        Assert::AreEqual (1, old.band,    L"one without goes inside it");
    }


    TEST_METHOD (BandsAreNumberedWithNoGapsInTheirOrder)
    {
        DxuiToolbarDock  outer    = MakeDock (Edge::Bottom, 0, 1);
        DxuiToolbarDock  inner    = MakeDock (Edge::Bottom, 0, 4);
        DxuiToolbarDock  floating = MakeDock (Edge::Bottom, 0, 7);
        DxuiToolbarDock  settled  = MakeDock (Edge::Top, 0, 0);

        floating.floating = true;

        Assert::IsTrue   (Bands::AssignBands ({ &inner, &outer, &settled }));
        Assert::AreEqual (0, outer.band);
        Assert::AreEqual (1, inner.band);
        Assert::AreEqual (0, settled.band);
        Assert::IsFalse  (Bands::AssignBands ({ &inner, &outer, &settled }), L"nothing changes a second time");
    }


    TEST_METHOD (ANewBandMovesTheBandsFromItInward)
    {
        DxuiToolbarDock  a = MakeDock (Edge::Left, 0, 0);
        DxuiToolbarDock  b = MakeDock (Edge::Left, 0, 1);
        DxuiToolbarDock  c = MakeDock (Edge::Right, 0, 1);

        Bands::InsertBand ({ &a, &b, &c }, Edge::Left, 1);

        Assert::AreEqual (0, a.band, L"outside the new band, unmoved");
        Assert::AreEqual (2, b.band, L"from the new band on, one further in");
        Assert::AreEqual (1, c.band, L"another edge, unmoved");
    }


    //
    //  Layout
    //

    TEST_METHOD (TwoBandsOnTheTopStackFromTheEdgeIn)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,   0,       kTall, true),
                                                        MakeBar (Edge::Top, 1, 100, kBarLen, kThin, false) }, kArea, kMargin);

        AssertRect (RECT { 8, 100, 992, 172 }, placement.bars[0], L"the strip takes the whole outer band");
        AssertRect (RECT { 108, 172, 308, 212 }, placement.bars[1], L"the toolbar its own band inside it, at its offset");
        Assert::AreEqual (kTall + kThin, placement.depthPx[(size_t) Edge::Top]);
        AssertRect (RECT { 0, 212, 1000, 800 }, placement.inner, L"the bands come out of the area");
        Assert::AreEqual ((size_t) 2, placement.bands.size());
        Assert::IsTrue   (placement.bands[1].isInnermost && !placement.bands[0].isInnermost);
    }


    TEST_METHOD (ToolbarAndStripShareABandAndTheStripTakesTheRest)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Top, 0, 500, 300,     kTall, true),
                                                        MakeBar (Edge::Top, 0, 0,   kBarLen, kThin, false) }, kArea, kMargin);

        AssertRect (RECT { 8, 116, 208, 156 }, placement.bars[1], L"the toolbar keeps its length, centered across the band");
        AssertRect (RECT { 208, 100, 992, 172 }, placement.bars[0], L"the strip runs from it to the band's end");
        AssertRect (RECT { 0, 172, 1000, 800 }, placement.inner, L"one band, as thick as the strip");
        Assert::IsTrue (placement.bands[0].hasFill);
    }


    TEST_METHOD (TheStripCanStandBeforeTheToolbar)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Bottom, 0, 0,   300,     kTall, true),
                                                        MakeBar (Edge::Bottom, 0, 400, kBarLen, kThin, false) }, kArea, kMargin);

        AssertRect (RECT { 8, 728, 792, 800 }, placement.bars[0], L"the strip first");
        AssertRect (RECT { 792, 744, 992, 784 }, placement.bars[1], L"the toolbar at the band's far end");
    }


    TEST_METHOD (TheStripFollowsTheBandsLength)
    {
        RECT              wide      = { 0, 100, 1600, 800 };
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,   300,     kTall, true),
                                                        MakeBar (Edge::Top, 0, 900, kBarLen, kThin, false) }, wide, kMargin);

        AssertRect (RECT { 8, 100, 1392, 172 }, placement.bars[0], L"the strip grows with the window");
        AssertRect (RECT { 1392, 116, 1592, 156 }, placement.bars[1], L"the toolbar keeps its length");
    }


    TEST_METHOD (TwoStripsInOneBandSplitTheRestEqually)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,   300,     kTall, true),
                                                        MakeBar (Edge::Top, 0, 400, kBarLen, kThin, false),
                                                        MakeBar (Edge::Top, 0, 800, 300,     kTall, true) }, kArea, kMargin);

        Assert::AreEqual (392L, placement.bars[0].right - placement.bars[0].left, L"(984 - 200) / 2");
        Assert::AreEqual (392L, placement.bars[2].right - placement.bars[2].left);
        Assert::AreEqual (placement.bars[0].right, placement.bars[1].left, L"end to end");
        Assert::AreEqual (placement.bars[1].right, placement.bars[2].left);
        Assert::AreEqual (992L, placement.bars[2].right);
    }


    TEST_METHOD (AnOddRestGoesToTheFirstStrip)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,  10, kTall, true),
                                                        MakeBar (Edge::Top, 0, 50, 10, kTall, true) }, RECT { 0, 0, 1017, 500 }, kMargin);

        Assert::AreEqual (501L, placement.bars[0].right - placement.bars[0].left);
        Assert::AreEqual (500L, placement.bars[1].right - placement.bars[1].left);
        Assert::AreEqual (1009L, placement.bars[1].right, L"the band filled exactly");
    }


    TEST_METHOD (ToolbarsWithoutAStripKeepTheirOffsetsAndDoNotOverlap)
    {
        Bands::Placement  spread = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,   kBarLen, kThin, false),
                                                     MakeBar (Edge::Top, 0, 500, kBarLen, kThin, false) }, kArea, kMargin);
        Bands::Placement  pushed = Bands::Arrange ({ MakeBar (Edge::Top, 0, 0,   kBarLen, kThin, false),
                                                     MakeBar (Edge::Top, 0, 50,  kBarLen, kThin, false) }, kArea, kMargin);
        Bands::Placement  atEnd  = Bands::Arrange ({ MakeBar (Edge::Top, 0, 900, kBarLen, kThin, false),
                                                     MakeBar (Edge::Top, 0, 950, kBarLen, kThin, false) }, kArea, kMargin);

        Assert::AreEqual (508L, spread.bars[1].left, L"room at its offset");
        Assert::AreEqual (208L, pushed.bars[1].left, L"pushed along past the one before it");
        Assert::AreEqual (992L, atEnd.bars[1].right, L"the last stops at the band's end");
        Assert::AreEqual (atEnd.bars[1].left, atEnd.bars[0].right, L"and the one before it stops at that");
    }


    TEST_METHOD (TheOrderKeyOrdersTheBand)
    {
        Bands::Bar        first  = MakeBar (Edge::Top, 0, 0,   kBarLen, kThin, false);
        Bands::Bar        second = MakeBar (Edge::Top, 0, 500, 300,     kTall, true);
        Bands::Placement  placement;

        first.hasOrderKey  = true;
        first.orderKeyPx   = 700;
        second.hasOrderKey = true;
        second.orderKeyPx  = 400;

        placement = Bands::Arrange ({ first, second }, kArea, kMargin);

        Assert::IsTrue (placement.bars[1].left < placement.bars[0].left, L"the lower key goes first, whatever the offsets");
    }


    TEST_METHOD (SideBandsRunBetweenTheTopAndBottomBands)
    {
        Bands::Placement  placement = Bands::Arrange ({ MakeBar (Edge::Left,   0, 0, 300,     kTall, true),
                                                        MakeBar (Edge::Top,    0, 0, kBarLen, kThin, false),
                                                        MakeBar (Edge::Bottom, 0, 0, kBarLen, kThin, false),
                                                        MakeBar (Edge::Right,  0, 0, kBarLen, kThin, false),
                                                        MakeBar (Edge::Right,  1, 0, 300,     kTall, true) }, kArea, kMargin);

        AssertRect (RECT { 0, 140, 72, 760 }, placement.bars[0], L"the left strip runs between the top and bottom bands");
        AssertRect (RECT { 960, 140, 1000, 340 }, placement.bars[3], L"the right toolbar against the edge");
        AssertRect (RECT { 888, 140, 960, 760 }, placement.bars[4], L"the right strip in the band inside it");
        AssertRect (RECT { 72, 140, 888, 760 }, placement.inner, L"");
    }


    TEST_METHOD (AFloatingBarTakesNoPlace)
    {
        Bands::Bar        bar       = MakeBar (Edge::Top, 0, 0, kBarLen, kThin, false);
        Bands::Placement  placement;

        bar.floating = true;
        placement    = Bands::Arrange ({ bar }, kArea, kMargin);

        AssertRect (RECT {}, placement.bars[0], L"");
        AssertRect (kArea, placement.inner, L"");
        Assert::IsTrue (placement.bands.empty());
    }


    //
    //  Drops
    //

    TEST_METHOD (ADropOverABandJoinsIt)
    {
        Bands::Target  target = Bands::PickTarget (90, { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);

        Assert::AreEqual (1, target.band);
        Assert::IsFalse  (target.isNewBand);

        target = Bands::PickTarget (36, { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);
        Assert::AreEqual (0, target.band, L"the middle of the outer band");
        Assert::IsFalse  (target.isNewBand);
    }


    TEST_METHOD (ADropNearABoundaryMakesANewBandThere)
    {
        Bands::Target  between = Bands::PickTarget (70,  { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);
        Bands::Target  outside = Bands::PickTarget (-10, { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);
        Bands::Target  edge    = Bands::PickTarget (4,   { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);
        Bands::Target  inside  = Bands::PickTarget (140, { kTall, kThin }, DxuiToolbarDock::kNoBand, false, kSplit);
        Bands::Target  empty   = Bands::PickTarget (20,  {},               DxuiToolbarDock::kNoBand, false, kSplit);

        Assert::IsTrue   (between.isNewBand && between.band == 1, L"between the two bands");
        Assert::IsTrue   (outside.isNewBand && outside.band == 0, L"past the outer side, against the edge");
        Assert::IsTrue   (edge.isNewBand    && edge.band    == 0, L"within the split of the outer side");
        Assert::IsTrue   (inside.isNewBand  && inside.band  == 2, L"inside them all");
        Assert::IsTrue   (empty.isNewBand   && empty.band   == 0, L"an edge with no bands");
    }


    TEST_METHOD (AToolbarAloneInItsBandStaysOverTheBoundariesBesideIt)
    {
        Bands::Target  outer = Bands::PickTarget (74,  { kTall, kThin }, 1, true, kSplit);
        Bands::Target  inner = Bands::PickTarget (110, { kTall, kThin }, 1, true, kSplit);
        Bands::Target  other = Bands::PickTarget (4,   { kTall, kThin }, 1, true, kSplit);
        Bands::Target  share = Bands::PickTarget (74,  { kTall, kThin }, 1, false, kSplit);

        Assert::IsTrue (!outer.isNewBand && outer.band == 1, L"its outer boundary keeps it where it is");
        Assert::IsTrue (!inner.isNewBand && inner.band == 1, L"so does its inner one");
        Assert::IsTrue (other.isNewBand  && other.band == 0, L"a boundary away from it still makes a band");
        Assert::IsTrue (share.isNewBand  && share.band == 1, L"one sharing its band leaves it for a band of its own");
    }


    TEST_METHOD (APointerInsideAnEdgesBandsPicksThatEdge)
    {
        int    depth[Bands::kEdgeCount] = { kTall + kThin, 0, 0, 0 };
        Edge   edge                     = Edge::Left;

        Assert::IsTrue  (Bands::TryPickEdge (POINT { 500, 200 }, kArea, depth, kReach, edge), L"inside the top bands");
        Assert::IsTrue  (edge == Edge::Top);

        Assert::IsTrue  (Bands::TryPickEdge (POINT { 500, 270 }, kArea, depth, kReach, edge), L"within reach of their inner side");
        Assert::IsTrue  (edge == Edge::Top);

        Assert::IsFalse (Bands::TryPickEdge (POINT { 500, 300 }, kArea, depth, kReach, edge), L"past reach of every edge");
        Assert::IsFalse (Bands::TryPickEdge (POINT { 500, 90 },  kArea, depth, kReach, edge), L"outside the area");

        Assert::IsTrue  (Bands::TryPickEdge (POINT { 10, 200 }, kArea, depth, kReach, edge), L"over the top bands, near the left side");
        Assert::IsTrue  (edge == Edge::Top, L"the bands it is over win");

        Assert::IsTrue  (Bands::TryPickEdge (POINT { 10, 300 }, kArea, depth, kReach, edge), L"below them, near the left side");
        Assert::IsTrue  (edge == Edge::Left);
    }


    TEST_METHOD (WithNoBandsThePickIsTheNearestEdge)
    {
        int    depth[Bands::kEdgeCount] = {};
        Edge   edge                     = Edge::Top;

        Assert::IsTrue (Bands::TryPickEdge (POINT { 980, 400 }, kArea, depth, kReach, edge));
        Assert::IsTrue (edge == Edge::Right);
        Assert::IsTrue (Bands::TryPickEdge (POINT { 400, 790 }, kArea, depth, kReach, edge));
        Assert::IsTrue (edge == Edge::Bottom);
    }


    TEST_METHOD (TheSidesOffsetsRunBetweenTheTopAndBottomBands)
    {
        int   depth[Bands::kEdgeCount] = { 112, 40, 0, 0 };

        AssertRect (RECT { 0, 212, 1000, 760 }, Bands::GetEdgeArea (Edge::Left, kArea, depth), L"");
        AssertRect (kArea, Bands::GetEdgeArea (Edge::Top, kArea, depth), L"");
        Assert::AreEqual (-5, Bands::GetAcrossPx (POINT { 1005, 300 }, Edge::Right, kArea));
        Assert::AreEqual (40, Bands::GetAcrossPx (POINT { 300, 140 },  Edge::Top,   kArea));
    }


    TEST_METHOD (APlaceOnAGivenEdgeKeepsTheGrabUnderThePointer)
    {
        DxuiToolbarDock  dock = DxuiToolbarDock::PickForDropOn (Edge::Left, POINT { 30, 400 }, POINT { 0, 20 }, kArea, 96);

        Assert::IsTrue   (dock.edge == Edge::Left);
        Assert::AreEqual (280, dock.offsetDip, L"400 - 100 - 20 down the side");
        Assert::IsFalse  (dock.floating);
    }
};
