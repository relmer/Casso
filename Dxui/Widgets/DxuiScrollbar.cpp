#include "Pch.h"

#include "DxuiScrollbar.h"

#include "Render/IDxuiPainter.h"





static constexpr uint32_t  s_kRgbMask           = 0x00FFFFFFu;
static constexpr uint32_t  s_kTrackAlpha        = 0x18000000u;
static constexpr uint32_t  s_kThumbAlpha        = 0x80000000u;
static constexpr uint32_t  s_kArrowAlpha        = 0xC0000000u;
static constexpr float     s_kArrowGlyphRatio   = 0.30f;
static constexpr int       s_kArrowGlyphMinPx   = 3;
static constexpr int       s_kArrowGlyphAspect  = 2;
static constexpr int       s_kArrowCount        = 2;
static constexpr int       s_kArrowFitSlackPx   = 2;
static constexpr float     s_kThumbCrossInsetPx = 1.0f;






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::Configure (Orientation orientation, int thicknessPx, int minThumbPx, int arrowStepPx)
{
    m_orientation = orientation;
    m_thicknessPx = thicknessPx;
    m_minThumbPx  = minThumbPx;
    m_arrowStepPx = arrowStepPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::SetScrollInfo
//
//  Honours the SCROLLINFO fMask so callers can push a partial update (e.g.
//  a new nPos mid-drag) without re-sending the whole model, exactly like
//  the Win32 SetScrollInfo.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::SetScrollInfo (const SCROLLINFO & info)
{
    if ((info.fMask & SIF_RANGE) != 0)
    {
        m_min = info.nMin;
        m_max = info.nMax;
    }

    if ((info.fMask & SIF_PAGE) != 0)
    {
        m_page = (int) info.nPage;
    }

    if ((info.fMask & SIF_POS) != 0)
    {
        m_pos = info.nPos;
    }

    SetScrollPos (m_pos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetScrollInfo
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::GetScrollInfo (SCROLLINFO & info) const
{
    info.nMin  = m_min;
    info.nMax  = m_max;
    info.nPage = (UINT) m_page;
    info.nPos  = m_pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::SetScrollPos
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::SetScrollPos (int pos)
{
    m_pos = std::clamp (pos, m_min, m_min + GetMaxScrollPos());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetMaxScrollPos
//
//  The largest position offset (relative to nMin): the content extent that
//  does not fit in one page. Row-quantized callers feed counts, pixel
//  callers feed pixels; either way it is content minus viewport.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::GetMaxScrollPos() const
{
    return std::max (0, ContentExtent() - m_page);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::IsVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::IsVisible() const
{
    return m_page > 0 && ContentExtent() > m_page && MainTrackLength() > 0 && m_thicknessPx > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::ContentExtent
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::ContentExtent() const
{
    return m_max - m_min;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::MainTrackStart
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::MainTrackStart() const
{
    return (m_orientation == Orientation::Vertical) ? m_track.top : m_track.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::MainTrackLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::MainTrackLength() const
{
    return (m_orientation == Orientation::Vertical) ? (m_track.bottom - m_track.top)
                                                    : (m_track.right - m_track.left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::ArrowExtent
//
//  Arrow buttons are square (one bar thickness) at each end of the track,
//  dropped entirely when the bar is too short to host them plus a thumb.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::ArrowExtent() const
{
    int  extent = m_thicknessPx;



    if (MainTrackLength() < m_thicknessPx * s_kArrowCount + m_minThumbPx + s_kArrowFitSlackPx)
    {
        extent = 0;
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::ThumbLength
//
//  Thumb extent along the scroll axis: proportional to page / content,
//  floored at the minimum thumb size and capped at the track length.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiScrollbar::ThumbLength() const
{
    int    trackLen = MainTrackLength() - ArrowExtent() * s_kArrowCount;
    int    content  = ContentExtent();
    float  length   = 0.0f;



    if (trackLen > 0 && content > 0)
    {
        length = std::max ((float) m_minThumbPx, (float) trackLen * (float) m_page / (float) content);
        length = std::min (length, (float) trackLen);
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::ThumbStart
//
//  Thumb start along the scroll axis (widget-relative px): the track start
//  plus the fraction of the travel corresponding to the current position.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiScrollbar::ThumbStart() const
{
    int    arrowExt   = ArrowExtent();
    int    trackStart = MainTrackStart() + arrowExt;
    int    trackLen   = MainTrackLength() - arrowExt * s_kArrowCount;
    float  thumbLen   = ThumbLength();
    float  travel     = (float) trackLen - thumbLen;
    int    maxPos     = GetMaxScrollPos();
    float  start      = (float) trackStart;



    if (maxPos > 0 && travel > 0.0f)
    {
        start += travel * (float) (m_pos - m_min) / (float) maxPos;
    }

    return start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::MainRect
//
//  Builds a widget-relative rect spanning [mainStart, mainStart + extent)
//  along the scroll axis and the full bar thickness across it.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiScrollbar::MainRect (int mainStart, int mainExtent) const
{
    RECT  r = {};



    if (m_orientation == Orientation::Vertical)
    {
        r = { m_track.left, mainStart, m_track.right, mainStart + mainExtent };
    }
    else
    {
        r = { mainStart, m_track.top, mainStart + mainExtent, m_track.bottom };
    }

    return r;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetMetrics
//
////////////////////////////////////////////////////////////////////////////////

DxuiScrollbar::Metrics DxuiScrollbar::GetMetrics() const
{
    HRESULT  hr        = S_OK;
    Metrics  m         = {};
    int      arrowExt  = ArrowExtent();
    int      mainStart = MainTrackStart();
    int      mainLen   = MainTrackLength();



    BAIL_OUT_IF (!IsVisible(), S_OK);

    m.visible     = true;
    m.bar         = m_track;
    m.track       = MainRect (mainStart + arrowExt, mainLen - arrowExt * s_kArrowCount);
    m.thumbStart  = ThumbStart();
    m.thumbLength = ThumbLength();

    if (arrowExt > 0)
    {
        m.arrowLess = MainRect (mainStart, arrowExt);
        m.arrowMore = MainRect (mainStart + mainLen - arrowExt, arrowExt);
    }

Error:
    return m;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::HitTest
//
//  True when the point lies on the visible bar strip.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::HitTest (int xPx, int yPx) const
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    BAIL_OUT_IF (!IsVisible(), S_OK);

    result = (xPx >= m_track.left && xPx < m_track.right && yPx >= m_track.top && yPx < m_track.bottom);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::OnMouseDown
//
//  Classifies the press: an arrow steps by one line, the thumb begins a
//  drag, a track click pages toward the click. Consumes any press on the
//  visible bar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::OnMouseDown (int xPx, int yPx)
{
    HRESULT  hr      = S_OK;
    bool     handled = false;
    Metrics  m       = GetMetrics();
    POINT    pt      = { xPx, yPx };
    float    mainPt  = (float) ((m_orientation == Orientation::Vertical) ? yPx : xPx);



    BAIL_OUT_IF (!m.visible || !HitTest (xPx, yPx), S_OK);

    handled = true;

    if (PtInRect (&m.arrowLess, pt))
    {
        NotifyPos (SB_LINEUP, m_pos - m_arrowStepPx);
    }
    else if (PtInRect (&m.arrowMore, pt))
    {
        NotifyPos (SB_LINEDOWN, m_pos + m_arrowStepPx);
    }
    else if (mainPt >= m.thumbStart && mainPt < m.thumbStart + m.thumbLength)
    {
        m_dragging = true;
        m_dragGrab = mainPt - m.thumbStart;
    }
    else if (PtInRect (&m.track, pt))
    {
        if (mainPt < m.thumbStart)
        {
            NotifyPos (SB_PAGEUP, m_pos - m_page);
        }
        else
        {
            NotifyPos (SB_PAGEDOWN, m_pos + m_page);
        }
    }

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::OnMouseMove
//
//  Drives an in-progress thumb drag, preserving the grab offset so the
//  thumb tracks the cursor without snapping its centre.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::OnMouseMove (int xPx, int yPx)
{
    HRESULT  hr         = S_OK;
    bool     handled    = false;
    int      arrowExt   = ArrowExtent();
    float    mainPt     = (float) ((m_orientation == Orientation::Vertical) ? yPx : xPx);
    float    trackStart = (float) (MainTrackStart() + arrowExt);
    float    travel     = (float) (MainTrackLength() - arrowExt * s_kArrowCount) - ThumbLength();
    int      maxPos     = GetMaxScrollPos();
    float    ratio      = 0.0f;
    int      newPos     = 0;



    BAIL_OUT_IF (!m_dragging, S_OK);

    handled = true;
    ratio   = (travel > 0.0f) ? ((mainPt - m_dragGrab - trackStart) / travel) : 0.0f;
    newPos  = m_min + (int) std::lround ((double) ratio * (double) maxPos);
    NotifyPos (SB_THUMBTRACK, newPos);

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::OnMouseUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::OnMouseUp()
{
    HRESULT  hr      = S_OK;
    bool     handled = false;



    BAIL_OUT_IF (!m_dragging, S_OK);

    m_dragging = false;
    m_dragGrab = 0.0f;
    handled    = true;

    if (m_onScroll)
    {
        m_onScroll (SB_ENDSCROLL, m_pos);
    }

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::NotifyPos
//
//  Clamps and applies a new position; fires onScroll only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::NotifyPos (int sbCode, int newPos)
{
    int  before = m_pos;



    SetScrollPos (newPos);

    if (m_pos != before && m_onScroll)
    {
        m_onScroll (sbCode, m_pos);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::Paint
//
//  Fills the track strip, the thumb (inset one pixel across the bar), and
//  the two arrow triangles when present.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::Paint (IDxuiPainter & painter, uint32_t foregroundArgb) const
{
    HRESULT   hr        = S_OK;
    Metrics   m         = GetMetrics();
    uint32_t  trackArgb = (foregroundArgb & s_kRgbMask) | s_kTrackAlpha;
    uint32_t  thumbArgb = (foregroundArgb & s_kRgbMask) | s_kThumbAlpha;
    uint32_t  arrowArgb = (foregroundArgb & s_kRgbMask) | s_kArrowAlpha;
    float     thickness = 0.0f;



    BAIL_OUT_IF (!m.visible, S_OK);

    painter.FillRect ((float) m.bar.left, (float) m.bar.top,
                      (float) (m.bar.right - m.bar.left), (float) (m.bar.bottom - m.bar.top), trackArgb);

    if (m_orientation == Orientation::Vertical)
    {
        thickness = (float) (m.bar.right - m.bar.left);
        painter.FillRect ((float) m.bar.left + s_kThumbCrossInsetPx, m.thumbStart,
                          thickness - s_kThumbCrossInsetPx * 2.0f, m.thumbLength, thumbArgb);
    }
    else
    {
        thickness = (float) (m.bar.bottom - m.bar.top);
        painter.FillRect (m.thumbStart, (float) m.bar.top + s_kThumbCrossInsetPx,
                          m.thumbLength, thickness - s_kThumbCrossInsetPx * 2.0f, thumbArgb);
    }

    if (m.arrowLess.right > m.arrowLess.left && m.arrowLess.bottom > m.arrowLess.top)
    {
        PaintArrow (painter, m.arrowLess, true,  arrowArgb);
        PaintArrow (painter, m.arrowMore, false, arrowArgb);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::PaintArrow
//
//  Draws a triangle centred in an arrow-button rect as a stack of 1px
//  slices along the scroll axis: the apex sits at the track-start end when
//  `less` is true (up / left), otherwise at the track-end (down / right).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::PaintArrow (IDxuiPainter & painter, const RECT & rect, bool less, uint32_t argb) const
{
    bool  vertical  = (m_orientation == Orientation::Vertical);
    int   rectMain  = vertical ? (rect.bottom - rect.top) : (rect.right - rect.left);
    int   rectCross = vertical ? (rect.right - rect.left) : (rect.bottom - rect.top);
    int   depth     = std::max (s_kArrowGlyphMinPx, (int) std::lround ((double) rectMain * (double) s_kArrowGlyphRatio));
    int   width     = depth * s_kArrowGlyphAspect;
    int   i         = 0;



    for (i = 0; i < depth; i++)
    {
        float  frac     = less ? (float) (i + 1) / (float) depth
                               : (float) (depth - i) / (float) depth;
        float  sliceLen = (float) width * frac;
        float  mainOff  = (float) ((rectMain - depth) / 2 + i);
        float  crossOff = ((float) rectCross - sliceLen) / 2.0f;

        if (vertical)
        {
            painter.FillRect ((float) rect.left + crossOff, (float) rect.top + mainOff, sliceLen, 1.0f, argb);
        }
        else
        {
            painter.FillRect ((float) rect.left + mainOff, (float) rect.top + crossOff, 1.0f, sliceLen, argb);
        }
    }
}
