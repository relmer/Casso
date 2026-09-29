#include "Pch.h"

#include "Widgets/DxuiScrollPanel.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollPanel
//
//  The scrollbar's arrows, track and thumb move the children as the wheel
//  does.
//
////////////////////////////////////////////////////////////////////////////////

DxuiScrollPanel::DxuiScrollPanel()
{
    m_scrollbar.SetOnScroll ([this] (int, int pos) { SetScrollPosPx (pos); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlaceChild
//
//  A child placed again keeps one entry, at its new place.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::PlaceChild (IDxuiControl & child, const RECT & rectPx)
{
    for (Placement & placement : m_placements)
    {
        if (placement.child == &child)
        {
            placement.rectPx = rectPx;
            return;
        }
    }

    m_placements.push_back ({ &child, rectPx });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxScrollPosPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollPanel::GetMaxScrollPosPx() const
{
    int  viewH = m_viewportPx.bottom - m_viewportPx.top;



    return std::max (m_contentPx - viewH, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetScrollPosPx
//
//  Clamped to the range, and ignored while the children fit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::SetScrollPosPx (int posPx)
{
    int  viewH   = m_viewportPx.bottom - m_viewportPx.top;
    int  clamped = ClampScrollPos (posPx, m_contentPx, viewH);



    if (!m_isScrollable || clamped == m_scrollPosPx)
    {
        return;
    }

    m_scrollPosPx = clamped;

    ApplyScrollPos();
    ConfigureScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The panel's bounds are its viewport. The content runs from the viewport's
//  top to the lowest visible child, and as far again below it as the highest
//  one sits below the top, so a list inset from the viewport's top ends with
//  the same inset at its bottom. The scroll position is kept, clamped to the
//  new range.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::Layout (const RECT & viewportPx, const DxuiDpiScaler & scaler)
{
    int  viewH = viewportPx.bottom - viewportPx.top;



    SetBounds (viewportPx);

    m_viewportPx   = viewportPx;
    m_scaler       = scaler;
    m_contentPx    = ComputeContentHeightPx();
    m_insetPx      = ComputeInsetPx();
    m_isScrollable = m_contentPx > viewH;
    m_scrollPosPx  = m_isScrollable ? ClampScrollPos (m_scrollPosPx, m_contentPx, viewH) : 0;

    ApplyScrollPos();
    ConfigureScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeContentHeightPx
//
//  From the viewport's top to the lowest visible child, and the inset again
//  below it; 0 while no placed child is visible.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollPanel::ComputeContentHeightPx() const
{
    int   lowest = 0;
    bool  hasAny = false;



    for (const Placement & placement : m_placements)
    {
        if (placement.child->IsVisible())
        {
            lowest = hasAny ? std::max (lowest, (int) placement.rectPx.bottom) : (int) placement.rectPx.bottom;
            hasAny = true;
        }
    }

    return hasAny ? (lowest - (int) m_viewportPx.top) + ComputeInsetPx() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeInsetPx
//
//  How far the highest visible child sits below the viewport's top, with the
//  panel scrolled to its top; 0 while none is visible.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollPanel::ComputeInsetPx() const
{
    int   highest = 0;
    bool  hasAny  = false;



    for (const Placement & placement : m_placements)
    {
        if (placement.child->IsVisible())
        {
            highest = hasAny ? std::min (highest, (int) placement.rectPx.top) : (int) placement.rectPx.top;
            hasAny  = true;
        }
    }

    return hasAny ? std::max (highest - (int) m_viewportPx.top, 0) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyScrollPos
//
//  Every placed child laid out at its place, moved up by the scroll
//  position.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::ApplyScrollPos()
{
    for (const Placement & placement : m_placements)
    {
        RECT  shifted = placement.rectPx;

        OffsetRect (&shifted, 0, -m_scrollPosPx);
        placement.child->Layout (shifted, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureScrollbar
//
//  Down the viewport's right edge, in pixels of content.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::ConfigureScrollbar()
{
    constexpr int   kMinThumbDip = 16;
    int             barW         = m_scaler.ToPx (kScrollbarWidthDip);
    DxuiScrollInfo  info;



    m_scrollbar.Configure (DxuiScrollbar::Orientation::Vertical, barW, m_scaler.ToPx (kMinThumbDip), m_lineStepPx);
    m_scrollbar.SetTrack  (RECT { m_viewportPx.right - barW, m_viewportPx.top, m_viewportPx.right, m_viewportPx.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = m_isScrollable ? m_contentPx : 0;
    info.nPage = (UINT) (m_viewportPx.bottom - m_viewportPx.top);
    info.nPos  = m_scrollPosPx;
    m_scrollbar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The children clipped to the viewport, within whatever clip a scrolled
//  page around the panel has set, which is put back after; then the
//  scrollbar while the children do not fit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr       = S_OK;
    RECT     prior    = {};
    RECT     clip     = m_viewportPx;
    bool     hasPrior = painter.GetClipRect (prior);



    if (hasPrior)
    {
        clip.left   = std::max (clip.left,   prior.left);
        clip.top    = std::max (clip.top,    prior.top);
        clip.right  = std::min (clip.right,  std::max (prior.right,  clip.left));
        clip.bottom = std::min (clip.bottom, std::max (prior.bottom, clip.top));
    }

    painter.SetClipRect (&clip);

    hr = text.PushClipRect ((float) m_viewportPx.left,
                            (float) m_viewportPx.top,
                            (float) (m_viewportPx.right  - m_viewportPx.left),
                            (float) (m_viewportPx.bottom - m_viewportPx.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    DxuiPanel::Paint (painter, text, theme);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_isScrollable)
    {
        m_scrollbar.Paint (painter, theme.Foreground());
    }

    painter.SetClipRect (hasPrior ? &prior : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  The scrollbar first. A press or a wheel turn outside the viewport reaches
//  no child, since a child there is laid out but not drawn; moves and
//  releases reach them all, so a control pressed inside keeps tracking. A
//  wheel turn no child takes scrolls the panel, and is left for the page
//  around it when the panel is already as far as it goes that way.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollPanel::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isWheel   = ev.kind == DxuiMouseEventKind::Wheel;
    bool  hitsPoint = ev.kind == DxuiMouseEventKind::Down || isWheel;
    bool  inView    = IsInViewport (ev.positionDip);
    bool  consumed  = false;
    int   before    = m_scrollPosPx;



    if (m_isScrollable && m_scrollbar.IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move) { return m_scrollbar.OnMouseMove (ev.positionDip.x, ev.positionDip.y); }
        if (ev.kind == DxuiMouseEventKind::Up)   { return m_scrollbar.OnMouseUp(); }
    }

    if (m_isScrollable && inView && ev.kind == DxuiMouseEventKind::Down && m_scrollbar.HitTest (ev.positionDip.x, ev.positionDip.y))
    {
        return m_scrollbar.OnMouseDown (ev.positionDip.x, ev.positionDip.y);
    }

    if (!hitsPoint || inView)
    {
        consumed = DxuiPanel::OnMouse (ev);
    }

    if (!consumed && inView && isWheel && !ev.wheelHorizontal)
    {
        SetScrollPosPx (m_scrollPosPx - (int) std::lround (ev.wheelDelta * (float) m_lineStepPx));
        consumed = m_scrollPosPx != before;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCursorForPoint
//
//  As OnMouse: outside the viewport a child has no say.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiScrollPanel::GetCursorForPoint (POINT clientPx) const
{
    return IsInViewport (clientPx) ? DxuiPanel::GetCursorForPoint (clientPx) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPointClipped
//
//  A press outside the viewport must not focus a child laid out there.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollPanel::IsPointClipped (POINT clientPx) const
{
    return !IsInViewport (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RevealDescendant
//
//  With the inset the list has at the viewport's top, so a row revealed at
//  either end sits where the first and last rows sit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiScrollPanel::RevealDescendant (const IDxuiControl & descendant)
{
    RECT  target = descendant.GetBounds();



    InflateRect    (&target, 0, m_insetPx);
    SetScrollPosPx (GetScrollPosToReveal (m_scrollPosPx, target, m_viewportPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInViewport
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiScrollPanel::IsInViewport (POINT clientPx) const
{
    return PtInRect (&m_viewportPx, clientPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampScrollPos
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollPanel::ClampScrollPos (int posPx, int contentPx, int viewportPx)
{
    int  maxPos = std::max (contentPx - viewportPx, 0);



    return std::clamp (posPx, 0, maxPos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScrollPosToReveal
//
//  Scrolling by d moves the target up by d, so a target below the viewport
//  needs d = bottom overshoot and one above it d = -(top undershoot). The top
//  edge wins for a target taller than the viewport.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiScrollPanel::GetScrollPosToReveal (int posPx, const RECT & targetPx, const RECT & viewportPx)
{
    int  result = posPx;



    if (targetPx.bottom > viewportPx.bottom)
    {
        result = posPx + (targetPx.bottom - viewportPx.bottom);
    }

    if (targetPx.top - (result - posPx) < viewportPx.top)
    {
        result = posPx - (viewportPx.top - targetPx.top);
    }

    return result;
}





