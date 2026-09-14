#include "Pch.h"

#include "DxuiScrollbar.h"

#include "Render/IDxuiPainter.h"
#include "Core/DxuiSystemSettings.h"





static constexpr uint32_t  s_kRgbMask           = 0x00FFFFFFu;
static constexpr uint32_t  s_kThumbAlpha        = 0xDD000000u;

//  Explorer's scrollbar widens quickly when the pointer arrives and narrows
//  more slowly when it leaves.
static constexpr float     s_kGrowMs            = 100.0f;
static constexpr float     s_kShrinkMs          = 400.0f;
static constexpr uint32_t  s_kArrowAlpha        = 0xA0000000u;
static constexpr uint32_t  s_kArrowHotAlpha     = 0xFF000000u;
static constexpr float     s_kArrowGlyphRatio   = 0.30f;
static constexpr int       s_kArrowGlyphMinPx   = 3;
static constexpr int       s_kArrowGlyphAspect  = 2;
static constexpr int       s_kArrowCount        = 2;
static constexpr int       s_kArrowFitSlackPx   = 2;
static constexpr float     s_kThumbCrossInsetPx = 1.0f;

//  At rest, a Windows 11 scrollbar is a thin rounded thumb with no track, and
//  it widens to the full bar with arrows only while the pointer is over it.
//  Measured in Explorer's navigation pane at 120 dpi: three pixels of #959595,
//  no track. The GRAB BAND is the full strip in both states; only the drawing
//  narrows.
static constexpr int       s_kRestThumbDip      = 3;
static constexpr uint32_t  s_kRestThumbAlpha    = 0x99000000u;





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

    //  The resting puck is a quarter of the strip, and never thinner than
    //  the three pixels Explorer draws at 120 dpi.
    m_restThumbPx = (std::max) (s_kRestThumbDip, (int) std::lround ((double) thicknessPx * 0.25));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::SetScrollInfo
//
//  Honors the SCROLLINFO fMask so callers can push a partial update (e.g.
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
    return std::max (0, GetContentExtent() - m_page);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::IsVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::IsVisible() const
{
    return m_page > 0 && GetContentExtent() > m_page && GetMainTrackLength() > 0 && m_thicknessPx > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetContentExtent
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::GetContentExtent() const
{
    return m_max - m_min;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetMainTrackStart
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::GetMainTrackStart() const
{
    return (m_orientation == Orientation::Vertical) ? m_track.top : m_track.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetMainTrackLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::GetMainTrackLength() const
{
    return (m_orientation == Orientation::Vertical) ? (m_track.bottom - m_track.top)
                                                    : (m_track.right - m_track.left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetArrowExtent
//
//  Arrow buttons are square (one bar thickness) at each end of the track,
//  dropped entirely when the bar is too short to host them plus a thumb.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollbar::GetArrowExtent() const
{
    int  extent = m_thicknessPx;



    if (GetMainTrackLength() < m_thicknessPx * s_kArrowCount + m_minThumbPx + s_kArrowFitSlackPx)
    {
        extent = 0;
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetThumbLength
//
//  Thumb extent along the scroll axis: proportional to page / content,
//  floored at the minimum thumb size and capped at the track length.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiScrollbar::GetThumbLength() const
{
    int    trackLen = GetMainTrackLength() - GetArrowExtent() * s_kArrowCount;
    int    content  = GetContentExtent();
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
//  DxuiScrollbar::GetThumbStart
//
//  Thumb start along the scroll axis (widget-relative px): the track start
//  plus the fraction of the travel corresponding to the current position.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiScrollbar::GetThumbStart() const
{
    int    arrowExt   = GetArrowExtent();
    int    trackStart = GetMainTrackStart() + arrowExt;
    int    trackLen   = GetMainTrackLength() - arrowExt * s_kArrowCount;
    float  thumbLen   = GetThumbLength();
    float  travel     = (float) trackLen - thumbLen;
    int    maxPos     = GetMaxScrollPos();
    float  start      = (float) trackStart;



    //  A dragged puck follows the pointer pixel by pixel. The position it
    //  reports moves in whole units, so the view scrolls only when the puck
    //  has gone far enough for one.
    if (m_dragging)
    {
        start = std::clamp (m_dragThumb, (float) trackStart, (float) trackStart + (std::max) (travel, 0.0f));
    }
    else if (maxPos > 0 && travel > 0.0f)
    {
        start += travel * (float) (m_pos - m_min) / (float) maxPos;
    }

    return start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::GetMainRect
//
//  Builds a widget-relative rect spanning [mainStart, mainStart + extent)
//  along the scroll axis and the full bar thickness across it.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiScrollbar::GetMainRect (int mainStart, int mainExtent) const
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
//  Computes every rect the scrollbar occupies -- bar, track, thumb, and the
//  two arrows -- in one call.
//
//  Painting and hit-testing both read from this, which is the point: computing
//  them separately is how a thumb ends up drawn in one place and clickable in
//  another.
//
//  The geometry is expressed through MainRect and a main-axis start and
//  length, so one set of formulas serves both orientations. A vertical and a
//  horizontal scrollbar differ only in which axis MainRect maps onto.
//
//  Arrow rects are omitted when the arrow extent is zero, since a scrollbar
//  styled without arrows has no such regions to hit-test -- and the track
//  already spans the full length in that case.
//
//  An invisible scrollbar returns a zeroed Metrics with visible false, so
//  callers test one field instead of guarding every use.
//
////////////////////////////////////////////////////////////////////////////////

DxuiScrollbar::Metrics DxuiScrollbar::GetMetrics() const
{
    HRESULT  hr        = S_OK;
    Metrics  m         = {};
    int      arrowExt  = GetArrowExtent();
    int      mainStart = GetMainTrackStart();
    int      mainLen   = GetMainTrackLength();



    BAIL_OUT_IF (!IsVisible(), S_OK);

    m.visible     = true;
    m.bar         = m_track;
    m.track       = GetMainRect (mainStart + arrowExt, mainLen - arrowExt * s_kArrowCount);
    m.thumbStart  = GetThumbStart();
    m.thumbLength = GetThumbLength();

    if (arrowExt > 0)
    {
        m.arrowLess = GetMainRect (mainStart, arrowExt);
        m.arrowMore = GetMainRect (mainStart + mainLen - arrowExt, arrowExt);
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
        m_dragging  = true;
        m_dragGrab  = mainPt - m.thumbStart;
        m_dragThumb = m.thumbStart;
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
//  thumb tracks the cursor without snapping its center.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::OnMouseMove (int xPx, int yPx)
{
    HRESULT  hr         = S_OK;
    bool     handled    = false;
    int      arrowExt   = GetArrowExtent();
    float    mainPt     = (float) ((m_orientation == Orientation::Vertical) ? yPx : xPx);
    float    trackStart = (float) (GetMainTrackStart() + arrowExt);
    float    travel     = (float) (GetMainTrackLength() - arrowExt * s_kArrowCount) - GetThumbLength();
    int      maxPos     = GetMaxScrollPos();
    float    ratio      = 0.0f;
    int      newPos     = 0;



    BAIL_OUT_IF (!m_dragging, S_OK);

    handled     = true;
    m_dragThumb = std::clamp (mainPt - m_dragGrab, trackStart, trackStart + (std::max) (travel, 0.0f));
    ratio       = (travel > 0.0f) ? ((m_dragThumb - trackStart) / travel) : 0.0f;
    newPos      = m_min + (int) std::lround ((double) ratio * (double) maxPos);
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
    float     amount    = m_dragging ? 1.0f : m_hoverAmount;
    float     restA     = (float) (s_kRestThumbAlpha >> 24);
    float     wideA     = (float) (s_kThumbAlpha >> 24);
    uint32_t  thumbArgb = (foregroundArgb & s_kRgbMask) | ((uint32_t) (restA + (wideA - restA) * amount) << 24);
    uint32_t  arrowArgb = (foregroundArgb & s_kRgbMask) | ((uint32_t) ((float) (s_kArrowAlpha >> 24) * amount) << 24);
    uint32_t  arrowHot  = (foregroundArgb & s_kRgbMask) | ((uint32_t) ((float) (s_kArrowHotAlpha >> 24) * amount) << 24);
    bool      vertical  = (m_orientation == Orientation::Vertical);
    float     strip     = 0.0f;
    float     thumbW    = 0.0f;
    float     inset     = 0.0f;



    BAIL_OUT_IF (!m.visible, S_OK);

    //  The puck floats on whatever the view drew, with no track, and grows
    //  from its resting width toward the bar's as the pointer arrives.
    strip  = vertical ? (float) (m.bar.right - m.bar.left) : (float) (m.bar.bottom - m.bar.top);
    thumbW = (float) m_restThumbPx + ((strip - s_kThumbCrossInsetPx * 2.0f) - (float) m_restThumbPx) * amount;
    thumbW = (std::min) (thumbW, strip);
    inset  = (strip - thumbW) * 0.5f;

    if (vertical)
    {
        PaintThumb (painter, (float) m.bar.left + inset, m.thumbStart, thumbW, m.thumbLength, thumbArgb);
    }
    else
    {
        PaintThumb (painter, m.thumbStart, (float) m.bar.top + inset, m.thumbLength, thumbW, thumbArgb);
    }

    //  Arrows fade in with the widening, and the one under the pointer is
    //  brighter, as in Windows.
    if (amount > 0.0f && m.arrowLess.right > m.arrowLess.left && m.arrowLess.bottom > m.arrowLess.top)
    {
        PaintArrow (painter, m.arrowLess, true,  (m_hoverArrow < 0) ? arrowHot : arrowArgb);
        PaintArrow (painter, m.arrowMore, false, (m_hoverArrow > 0) ? arrowHot : arrowArgb);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::PaintThumb
//
//  A rounded puck, drawn as one shape. Windows has drawn these round since
//  11, and a square-ended bar is one of the tells that a window is not native.
//  A rectangle with circles laid over its ends would draw the translucent
//  overlap twice, leaving darker marks where they meet.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollbar::PaintThumb (IDxuiPainter & painter, float x, float y, float w, float h, uint32_t argb) const
{
    bool   vertical = (m_orientation == Orientation::Vertical);
    float  radius   = (vertical ? w : h) * 0.5f;



    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    painter.FillRoundedRect (x, y, w, h, radius, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::Tick
//
//  Moves the hover amount toward the pointer's state: quickly toward wide,
//  slowly back toward rest. With animations off in Windows it goes straight
//  there. Reports whether it moved, so the host knows to repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::Tick (int64_t nowMs)
{
    float    target = (m_expanded || m_dragging) ? 1.0f : 0.0f;
    float    dt     = (m_lastTickMs == 0) ? 0.0f : (float) (nowMs - m_lastTickMs);



    m_lastTickMs = nowMs;

    if (m_hoverAmount == target)
    {
        return false;
    }

    if (!DxuiSystemSettings::Instance().AreAnimationsEnabled())
    {
        m_hoverAmount = target;
        return true;
    }

    m_hoverAmount = (target > m_hoverAmount) ? (std::min) (target, m_hoverAmount + dt / s_kGrowMs)
                                             : (std::max) (target, m_hoverAmount - dt / s_kShrinkMs);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::SetHover
//
//  Starts the bar widening while the pointer is over it, or narrowing when it
//  leaves, and notes which arrow the pointer is on. Reports whether anything
//  changed to repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollbar::SetHover (bool over, POINT pt)
{
    Metrics  m       = GetMetrics();
    int      arrow   = 0;
    bool     changed = false;



    if (over && PtInRect (&m.arrowLess, pt))
    {
        arrow = -1;
    }
    else if (over && PtInRect (&m.arrowMore, pt))
    {
        arrow = 1;
    }

    changed      = (m_expanded != over) || (m_hoverArrow != arrow);
    m_expanded   = over;
    m_hoverArrow = arrow;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar::PaintArrow
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
        float  sliceLen = 0.0f;
        float  mainOff  = 0.0f;
        float  crossOff = 0.0f;

        float  frac     = less ? (float) (i + 1) / (float) depth
                               : (float) (depth - i) / (float) depth;
        sliceLen = (float) width * frac;
        mainOff = (float) ((rectMain - depth) / 2 + i);
        crossOff = ((float) rectCross - sliceLen) / 2.0f;

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
