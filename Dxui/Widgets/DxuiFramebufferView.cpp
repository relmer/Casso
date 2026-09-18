#include "Pch.h"

#include "DxuiFramebufferView.h"

#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::SetFramebuffer
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::SetFramebuffer (const uint32_t * bgra, int width, int height)
{
    size_t  count = 0;



    if (bgra == nullptr || width <= 0 || height <= 0)
    {
        Clear();
        return;
    }

    count = (size_t) width * (size_t) height;

    m_pixels.assign (bgra, bgra + count);
    m_width  = width;
    m_height = height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::Clear()
{
    m_pixels.clear();
    m_width  = 0;
    m_height = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::GetDestinationRect
//
//  The largest whole multiple that fits, when integer scaling is on and 1x
//  fits; otherwise the fit scale, the same on both axes when the aspect is
//  kept. Centered either way.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiFramebufferView::GetDestinationRect() const
{
    int   boundsW = m_boundsDip.right - m_boundsDip.left;
    int   boundsH = m_boundsDip.bottom - m_boundsDip.top;
    int   destW   = 0;
    int   destH   = 0;
    RECT  dest    = {};



    GetScaledSize (destW, destH);

    if (destW <= 0 || destH <= 0)
    {
        return dest;
    }

    dest.left   = m_boundsDip.left + (boundsW - destW) / 2 - m_panX;
    dest.top    = m_boundsDip.top  + (boundsH - destH) / 2 - m_panY;
    dest.right  = dest.left + destW;
    dest.bottom = dest.top  + destH;

    return dest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::GetScaledSize
//
//  The picture's drawn size: the fitted size, times the zoom.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::GetScaledSize (int & outWidth, int & outHeight) const
{
    int     boundsW = m_boundsDip.right - m_boundsDip.left;
    int     boundsH = m_boundsDip.bottom - m_boundsDip.top;
    double  scaleX  = 0.0;
    double  scaleY  = 0.0;
    int     whole   = 0;



    outWidth  = 0;
    outHeight = 0;

    if (m_width <= 0 || m_height <= 0 || boundsW <= 0 || boundsH <= 0)
    {
        return;
    }

    scaleX = (double) boundsW / (double) m_width;
    scaleY = (double) boundsH / (double) m_height;

    if (m_keepAspect)
    {
        scaleX = scaleY = (std::min) (scaleX, scaleY);
    }

    whole = (int) (std::min) (scaleX, scaleY);

    if (m_integer && whole >= 1)
    {
        outWidth  = m_width  * whole;
        outHeight = m_height * whole;
    }
    else
    {
        outWidth  = (int) (m_width  * scaleX);
        outHeight = (int) (m_height * scaleY);
    }

    outWidth  = (int) ((float) outWidth  * m_zoom);
    outHeight = (int) ((float) outHeight * m_zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::CanPan
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFramebufferView::CanPan() const
{
    int  destW = 0;
    int  destH = 0;



    GetScaledSize (destW, destH);

    return destW > (m_boundsDip.right - m_boundsDip.left) || destH > (m_boundsDip.bottom - m_boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::ClampPan
//
//  The pan goes no further than puts an edge of the picture at the matching
//  edge of the bounds, and is zero along an axis the picture fits.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::ClampPan()
{
    int  destW = 0;
    int  destH = 0;
    int  maxX  = 0;
    int  maxY  = 0;



    GetScaledSize (destW, destH);

    maxX = (std::max) (0, (destW - (int) (m_boundsDip.right - m_boundsDip.left)) / 2);
    maxY = (std::max) (0, (destH - (int) (m_boundsDip.bottom - m_boundsDip.top)) / 2);

    m_panX = (std::max) (-maxX, (std::min) (m_panX, maxX));
    m_panY = (std::max) (-maxY, (std::min) (m_panY, maxY));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::SyncScrollbars
//
//  A scrollbar's position runs from zero at the picture's near edge, so the
//  centered pan is half its range.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::SyncScrollbars()
{
    int         barW   = m_scaler.ToPx (s_kScrollbarWidthDip);
    int         destW  = 0;
    int         destH  = 0;
    int         viewW  = m_boundsDip.right - m_boundsDip.left;
    int         viewH  = m_boundsDip.bottom - m_boundsDip.top;
    SCROLLINFO  info   = { sizeof (info) };



    GetScaledSize (destW, destH);

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;

    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, s_kWheelStepDip);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom - barW });

    info.nMax  = (std::max) (destH - 1, 0);
    info.nPage = (UINT) (std::max) (viewH, 0);
    info.nPos  = (std::max) (0, (destH - viewH) / 2 + m_panY);
    m_vertScroll.SetScrollInfo (info);

    m_horzScroll.Configure (DxuiScrollbar::Orientation::Horizontal, barW, barW, s_kWheelStepDip);
    m_horzScroll.SetTrack (RECT { m_boundsDip.left, m_boundsDip.bottom - barW, m_boundsDip.right - barW, m_boundsDip.bottom });

    info.nMax  = (std::max) (destW - 1, 0);
    info.nPage = (UINT) (std::max) (viewW, 0);
    info.nPos  = (std::max) (0, (destW - viewW) / 2 + m_panX);
    m_horzScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::OnMouse  (IDxuiControl override)
//
//  Points are in the same coordinates as the bounds.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFramebufferView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  pt     = ev.positionDip;
    int    destW  = 0;
    int    destH  = 0;
    int    step   = m_scaler.ToPx (s_kWheelStepDip);



    GetScaledSize (destW, destH);
    SyncScrollbars();

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:
        if (ev.button != DxuiMouseButton::Left || !CanPan())
        {
            return false;
        }

        if (destH > (m_boundsDip.bottom - m_boundsDip.top) && m_vertScroll.HitTest (pt.x, pt.y))
        {
            m_vertScroll.OnMouseDown (pt.x, pt.y);
        }
        else if (destW > (m_boundsDip.right - m_boundsDip.left) && m_horzScroll.HitTest (pt.x, pt.y))
        {
            m_horzScroll.OnMouseDown (pt.x, pt.y);
        }
        else
        {
            m_panning  = true;
            m_panFrom  = pt;
            m_panFromX = m_panX;
            m_panFromY = m_panY;
            return true;
        }

        m_panY = m_vertScroll.GetScrollPos() - (destH - (int) (m_boundsDip.bottom - m_boundsDip.top)) / 2;
        m_panX = m_horzScroll.GetScrollPos() - (destW - (int) (m_boundsDip.right - m_boundsDip.left)) / 2;
        ClampPan();
        return true;

    case DxuiMouseEventKind::Move:
        if (m_vertScroll.IsDragging() || m_horzScroll.IsDragging())
        {
            m_vertScroll.OnMouseMove (pt.x, pt.y);
            m_horzScroll.OnMouseMove (pt.x, pt.y);

            m_panY = m_vertScroll.GetScrollPos() - (destH - (int) (m_boundsDip.bottom - m_boundsDip.top)) / 2;
            m_panX = m_horzScroll.GetScrollPos() - (destW - (int) (m_boundsDip.right - m_boundsDip.left)) / 2;
            ClampPan();
            return true;
        }

        if (!m_panning)
        {
            return false;
        }

        m_panX = m_panFromX - (pt.x - m_panFrom.x);
        m_panY = m_panFromY - (pt.y - m_panFrom.y);
        ClampPan();
        return true;

    case DxuiMouseEventKind::Up:
        if (m_vertScroll.IsDragging() || m_horzScroll.IsDragging())
        {
            m_vertScroll.OnMouseUp();
            m_horzScroll.OnMouseUp();
            return true;
        }

        if (!m_panning)
        {
            return false;
        }

        m_panning = false;
        return true;

    case DxuiMouseEventKind::Wheel:
        if (!CanPan())
        {
            return false;
        }

        //  A touchpad's sideways swipe arrives as a horizontal wheel, and
        //  Shift turns the vertical wheel sideways, as in most viewers.
        if (ev.wheelHorizontal || ev.shift)
        {
            m_panX += (int) (ev.wheelDelta * (float) step) * (ev.wheelHorizontal ? 1 : -1);
        }
        else
        {
            m_panY -= (int) (ev.wheelDelta * (float) step);
        }

        ClampPan();
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::GetCursorForPoint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiFramebufferView::GetCursorForPoint (POINT clientPx) const
{
    (void) clientPx;

    return CanPan() ? IDC_HAND : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    ClampPan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     dest = GetDestinationRect();
    HRESULT  hr   = S_OK;



    if (m_pixels.empty() || dest.right <= dest.left)
    {
        return;
    }

    //  A zoomed picture can be larger than the bounds, and is cut off at them.
    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                            (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawFramebuffer (m_pixels.data(), m_width, m_height,
                               (float) dest.left, (float) dest.top,
                               (float) (dest.right - dest.left), (float) (dest.bottom - dest.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (CanPan())
    {
        SyncScrollbars();
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
        m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}
