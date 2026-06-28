#include "Pch.h"

#include "MockDxuiPainter.h"

#include "Widgets/DxuiScrollbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbarTests
//
//  Behaviour coverage for the orientation-parameterized scrollbar: model
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
        DxuiScrollbar  bar = MakeVertical();

        bar.SetScrollPos (80);

        DxuiScrollbar::Metrics  m = bar.GetMetrics();

        Assert::AreEqual (154.0f, m.thumbStart, 0.01f);
        Assert::AreEqual (190.0f, m.thumbStart + m.thumbLength, 0.01f);
    }


    TEST_METHOD (Geometry_ArrowsDroppedWhenTrackTooShort)
    {
        DxuiScrollbar   bar;
        DxuiScrollInfo  info;

        bar.Configure (DxuiScrollbar::Orientation::Vertical, 10, 16, 1);
        bar.SetTrack (RECT{ 0, 0, 10, 30 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 100;
        info.nPage = 20;
        bar.SetScrollInfo (info);

        DxuiScrollbar::Metrics  m = bar.GetMetrics();

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
        DxuiScrollbar   bar;
        DxuiScrollInfo  info;

        bar.Configure (DxuiScrollbar::Orientation::Horizontal, 10, 16, 32);
        bar.SetTrack (RECT{ 0, 50, 300, 60 });

        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin  = 0;
        info.nMax  = 600;
        info.nPage = 300;
        bar.SetScrollInfo (info);

        DxuiScrollbar::Metrics  m = bar.GetMetrics();

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


    TEST_METHOD (Paint_EmitsTrackAndThumbFills)
    {
        DxuiScrollbar    bar     = MakeVertical();
        MockDxuiPainter  painter;
        bool             sawTrack = false;
        bool             sawThumb = false;

        bar.Paint (painter, 0xFFFFFFFFu);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == 0x18FFFFFFu &&
                call.x == 100.0f && call.width == 10.0f)
            {
                sawTrack = true;
            }

            if (call.kind == RecordedPaintKind::FillRect && call.argb == 0x80FFFFFFu &&
                call.x == 101.0f && call.width == 8.0f)
            {
                sawThumb = true;
            }
        }

        Assert::IsTrue (sawTrack);
        Assert::IsTrue (sawThumb);
    }
};
