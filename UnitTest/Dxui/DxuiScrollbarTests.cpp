#include "Pch.h"

#include "MockDxuiPainter.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbarTests
//
//  Behavior coverage for the orientation-parameterized scrollbar: model
//  clamping, derived geometry (track / arrows / thumb), hit-testing, the
//  arrow / track / thumb interaction notifications, and painting. All
//  units are widget-relative pixels; no device is required.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiScrollbarTests)
{
public:

    // A vertical bar: 10px-thick strip down x=[100,110), y=[0,200), over
    // 100 rows of content with a 20-row page. maxPos = 80; arrows are 10px
    // squares; the track is y=[10,190); the resting thumb is 36px tall.
    static DxuiScrollbar  MakeVertical()
    {
        DxuiScrollbar   bar;
        DxuiScrollInfo  info;

        bar.Configure (DxuiScrollbar::Orientation::Vertical, 10, 16, 1);
        bar.SetTrack (RECT{ 100, 0, 110, 200 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 100;
        info.nPage = 20;
        info.nPos  = 0;
        bar.SetScrollInfo (info);

        return bar;
    }


    //  Marks are lines across the whole bar, down the track between the
    //  arrows, in the match color and the current one's; with marks the bar
    //  is drawn wide, and once they are cleared it rests narrow again.
    TEST_METHOD (Marks_ALineAcrossTheBarForEachMatch)
    {
        constexpr uint32_t   kMatch   = 0xFF806000u;
        constexpr uint32_t   kCurrent = 0xFFFFFF00u;
        DxuiScrollbar        bar      = MakeVertical();
        MockDxuiPainter      painter;
        std::vector<float>   tops;
        float                restW    = 0.0f;
        float                markedW  = 0.0f;



        bar.Paint (painter, 0xFFFFFFFFu);
        restW = painter.Calls().back().width;

        bar.SetMarks ({ { 0.0f, false }, { 0.5f, true } }, kMatch, kCurrent);
        painter.Reset();
        bar.Paint (painter, 0xFFFFFFFFu);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && (call.argb == kMatch || call.argb == kCurrent))
            {
                Assert::AreEqual (100.0f, call.x,     L"across the bar from its left");
                Assert::AreEqual (10.0f,  call.width, L"the whole bar wide");
                tops.push_back (call.y);
            }
            else if (call.kind == RecordedPaintKind::FillRoundedRect)
            {
                markedW = call.width;
            }
        }

        Assert::AreEqual ((size_t) 2, tops.size(), L"a line for each mark");
        Assert::AreEqual (10.0f, tops[0], 0.01f, L"the first at the track's top, below the arrow");
        Assert::IsTrue   (tops[1] > 95.0f && tops[1] < 100.0f, L"the current one halfway down the track");
        Assert::IsTrue   (markedW > restW, L"the puck is wide while there are marks");

        bar.ClearMarks();
        painter.Reset();
        bar.Paint (painter, 0xFFFFFFFFu);

        Assert::AreEqual (restW, painter.Calls().back().width, L"and narrow again once they are cleared");
    }


    TEST_METHOD (Visibility_HiddenWhenContentFitsPage)
    {
        DxuiScrollbar   bar;
        DxuiScrollInfo  info;

        bar.Configure (DxuiScrollbar::Orientation::Vertical, 10, 16, 1);
        bar.SetTrack (RECT{ 0, 0, 10, 200 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 10;
        info.nPage = 20;
        bar.SetScrollInfo (info);

        Assert::IsFalse (bar.IsVisible());
        Assert::IsFalse (bar.GetMetrics().visible);
    }


    TEST_METHOD (Geometry_VerticalArrowsTrackAndThumb)
    {
        DxuiScrollbar            bar = MakeVertical();
        DxuiScrollbar::Metrics   m   = bar.GetMetrics();

        Assert::IsTrue (m.visible);
        Assert::AreEqual (100L, m.bar.left);
        Assert::AreEqual (200L, m.bar.bottom);

        Assert::AreEqual (0L,   m.arrowLess.top);
        Assert::AreEqual (10L,  m.arrowLess.bottom);
        Assert::AreEqual (190L, m.arrowMore.top);
        Assert::AreEqual (200L, m.arrowMore.bottom);

        Assert::AreEqual (10L,  m.track.top);
        Assert::AreEqual (190L, m.track.bottom);

        Assert::AreEqual (10.0f, m.thumbStart,  0.01f);
        Assert::AreEqual (36.0f, m.thumbLength, 0.01f);
    }


    TEST_METHOD (Geometry_ThumbAtMaxIsFlushWithTrackEnd)
    {
        DxuiScrollbar           bar = MakeVertical();
        DxuiScrollbar::Metrics  m   = {};

        bar.SetScrollPos (80);

        m = bar.GetMetrics();

        Assert::AreEqual (154.0f, m.thumbStart, 0.01f);
        Assert::AreEqual (190.0f, m.thumbStart + m.thumbLength, 0.01f);
    }


    TEST_METHOD (Geometry_ArrowsDroppedWhenTrackTooShort)
    {
        DxuiScrollbar           bar;
        DxuiScrollInfo          info;
        DxuiScrollbar::Metrics  m    = {};

        bar.Configure (DxuiScrollbar::Orientation::Vertical, 10, 16, 1);
        bar.SetTrack (RECT{ 0, 0, 10, 30 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 100;
        info.nPage = 20;
        bar.SetScrollInfo (info);

        m = bar.GetMetrics();

        Assert::IsTrue (m.visible);
        Assert::IsTrue (m.arrowLess.right <= m.arrowLess.left);
        Assert::AreEqual (0L,  m.track.top);
        Assert::AreEqual (30L, m.track.bottom);
    }


    TEST_METHOD (Clamp_PositionStaysWithinRange)
    {
        DxuiScrollbar  bar = MakeVertical();

        bar.SetScrollPos (1000);
        Assert::AreEqual (80, bar.GetScrollPos());

        bar.SetScrollPos (-5);
        Assert::AreEqual (0, bar.GetScrollPos());
    }


    TEST_METHOD (HitTest_OnlyOnTheStrip)
    {
        DxuiScrollbar  bar = MakeVertical();

        Assert::IsTrue  (bar.HitTest (105, 50));
        Assert::IsFalse (bar.HitTest (50,  50));
    }


    TEST_METHOD (Press_ArrowMoreStepsOneLine)
    {
        DxuiScrollbar  bar      = MakeVertical();
        int            lastCode = 0;
        int            lastPos  = -1;

        bar.SetOnScroll ([&] (int code, int pos) { lastCode = code; lastPos = pos; });

        Assert::IsTrue (bar.OnMouseDown (105, 195));
        Assert::AreEqual ((int) SB_LINEDOWN, lastCode);
        Assert::AreEqual (1, lastPos);
        Assert::AreEqual (1, bar.GetScrollPos());
    }


    TEST_METHOD (Press_TrackBelowThumbPagesDown)
    {
        DxuiScrollbar  bar      = MakeVertical();
        int            lastCode = 0;
        int            lastPos  = -1;

        bar.SetOnScroll ([&] (int code, int pos) { lastCode = code; lastPos = pos; });

        Assert::IsTrue (bar.OnMouseDown (105, 180));
        Assert::AreEqual ((int) SB_PAGEDOWN, lastCode);
        Assert::AreEqual (20, lastPos);
    }


    TEST_METHOD (Drag_ThumbTracksCursorThenEnds)
    {
        DxuiScrollbar  bar      = MakeVertical();
        int            lastCode = 0;
        int            lastPos  = -1;

        bar.SetOnScroll ([&] (int code, int pos) { lastCode = code; lastPos = pos; });

        Assert::IsTrue (bar.OnMouseDown (105, 20));
        Assert::IsTrue (bar.IsDragging());

        Assert::IsTrue (bar.OnMouseMove (105, 100));
        Assert::AreEqual ((int) SB_THUMBTRACK, lastCode);
        Assert::AreEqual (44, lastPos);
        Assert::AreEqual (44, bar.GetScrollPos());

        Assert::IsTrue (bar.OnMouseUp());
        Assert::IsFalse (bar.IsDragging());
        Assert::AreEqual ((int) SB_ENDSCROLL, lastCode);
    }


    TEST_METHOD (Press_OutsideStripIsNotConsumed)
    {
        DxuiScrollbar  bar = MakeVertical();

        Assert::IsFalse (bar.OnMouseDown (50, 50));
    }


    TEST_METHOD (Horizontal_ThumbGeometryUsesXAxis)
    {
        DxuiScrollbar           bar;
        DxuiScrollInfo          info;
        DxuiScrollbar::Metrics  m    = {};

        bar.Configure (DxuiScrollbar::Orientation::Horizontal, 10, 16, 32);
        bar.SetTrack (RECT{ 0, 50, 300, 60 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 600;
        info.nPage = 300;
        bar.SetScrollInfo (info);

        m = bar.GetMetrics();

        Assert::AreEqual (0L,   m.arrowLess.left);
        Assert::AreEqual (10L,  m.arrowLess.right);
        Assert::AreEqual (10.0f,  m.thumbStart,  0.01f);
        Assert::AreEqual (140.0f, m.thumbLength, 0.01f);
    }


    TEST_METHOD (Horizontal_DragUsesXAxis)
    {
        DxuiScrollbar   bar;
        DxuiScrollInfo  info;
        int             lastPos = -1;

        bar.Configure (DxuiScrollbar::Orientation::Horizontal, 10, 16, 32);
        bar.SetTrack (RECT{ 0, 50, 300, 60 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 600;
        info.nPage = 300;
        bar.SetScrollInfo (info);

        bar.SetOnScroll ([&] (int, int pos) { lastPos = pos; });

        Assert::IsTrue (bar.OnMouseDown (50, 55));
        Assert::IsTrue (bar.IsDragging());
        Assert::IsTrue (bar.OnMouseMove (150, 55));
        Assert::AreEqual (214, lastPos);
    }


    TEST_METHOD (ScrollInfo_PartialPosUpdate)
    {
        DxuiScrollbar   bar = MakeVertical();
        DxuiScrollInfo  info;

        info.fMask = SIF_POS;
        info.nPos  = 30;
        bar.SetScrollInfo (info);

        Assert::AreEqual (30, bar.GetScrollPos());
    }


    //  Windows draws a rounded puck and never a track. At rest the puck is
    //  thin; under the pointer it widens and the arrows appear. The grab band
    //  does not change with either.
    TEST_METHOD (Paint_AtRestIsAThinPuckAndNoTrack)
    {
        DxuiScrollbar    bar      = MakeVertical();
        MockDxuiPainter  painter;
        int              rects    = 0;
        int              pucks    = 0;
        float            widest   = 0.0f;


        bar.Paint (painter, 0xFFFFFFFFu);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect)
            {
                ++rects;
            }

            if (call.kind == RecordedPaintKind::FillRoundedRect)
            {
                ++pucks;
                widest = (std::max) (widest, call.width);
            }
        }

        Assert::AreEqual (0, rects, L"A bar at rest paints no track");
        Assert::AreEqual (1, pucks, L"and its puck is one rounded shape");
        Assert::IsTrue   (widest <= 4.0f,
            L"a few pixels wide rather than the whole strip");
    }


    TEST_METHOD (Paint_ExpandedWidensThePuckWithoutATrack)
    {
        DxuiScrollbar    bar      = MakeVertical();
        MockDxuiPainter  painter;
        int              rects    = 0;
        float            widest   = 0.0f;


        bar.SetExpanded (true);
        bar.Paint (painter, 0xFFFFFFFFu);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.x == 100.0f && call.width == 10.0f)
            {
                ++rects;
            }

            if (call.kind == RecordedPaintKind::FillRoundedRect)
            {
                widest = (std::max) (widest, call.width);
            }
        }

        Assert::AreEqual (0, rects, L"The pointer brings no track");
        Assert::AreEqual (5.0f, widest, L"and the puck widens to half the strip");
    }


    //  The arrows sit on the strip's center line with the puck, on whole
    //  pixels, whether the strip is an even or an odd number of pixels wide.
    TEST_METHOD (Paint_ArrowsAreCenteredOnWholePixels)
    {
        for (int thickness : { 10, 11 })
        {
            DxuiScrollbar    bar;
            DxuiScrollInfo   info;
            MockDxuiPainter  painter;
            int              slices = 0;

            bar.Configure (DxuiScrollbar::Orientation::Vertical, thickness, 16, 1);
            bar.SetTrack (RECT{ 100, 0, 100 + thickness, 200 });

            info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            info.nMax  = 100;
            info.nPage = 20;
            bar.SetScrollInfo (info);
            bar.SetExpanded (true);
            bar.Paint (painter, 0xFFFFFFFFu);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.height == 1.0f)
                {
                    ++slices;
                    Assert::AreEqual (std::floor (call.x), call.x, L"Each arrow slice starts on a whole pixel");
                    Assert::AreEqual (100.0f + (float) thickness * 0.5f, call.x + call.width * 0.5f, L"and is centered on the strip");
                }
            }

            Assert::IsTrue (slices > 0, L"Arrows are drawn on the widened bar");
        }
    }


    //  A dragged puck follows the pointer one pixel at a time, even though the
    //  position it reports moves in whole rows. Pressing without moving moves
    //  nothing.
    TEST_METHOD (Drag_PuckFollowsThePointerByThePixel)
    {
        DxuiScrollbar  bar = MakeVertical();
        int            y   = 0;


        bar.OnMouseDown (105, 20);
        bar.OnMouseMove (105, 20);

        Assert::AreEqual (10.0f, bar.GetMetrics().thumbStart, L"Pressing leaves the puck where it was");
        Assert::AreEqual (0,     bar.GetScrollPos(),          L"and the position where it was");

        for (y = 21; y <= 30; y++)
        {
            bar.OnMouseMove (105, y);
            Assert::AreEqual ((float) (y - 10), bar.GetMetrics().thumbStart, L"Each pixel moves the puck one pixel");
        }

        bar.OnMouseUp();
    }
};
