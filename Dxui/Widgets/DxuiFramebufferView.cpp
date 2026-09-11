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
    int     boundsW = m_boundsDip.right - m_boundsDip.left;
    int     boundsH = m_boundsDip.bottom - m_boundsDip.top;
    double  scaleX  = 0.0;
    double  scaleY  = 0.0;
    int     destW   = 0;
    int     destH   = 0;
    int     whole   = 0;
    RECT    dest    = {};



    if (m_width <= 0 || m_height <= 0 || boundsW <= 0 || boundsH <= 0)
    {
        return dest;
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
        destW = m_width  * whole;
        destH = m_height * whole;
    }
    else
    {
        destW = (int) (m_width  * scaleX);
        destH = (int) (m_height * scaleY);
    }

    dest.left   = m_boundsDip.left + (boundsW - destW) / 2;
    dest.top    = m_boundsDip.top  + (boundsH - destH) / 2;
    dest.right  = dest.left + destW;
    dest.bottom = dest.top  + destH;

    return dest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFramebufferView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UNREFERENCED_PARAMETER (scaler);

    SetBounds (boundsDip);
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



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (m_pixels.empty() || dest.right <= dest.left)
    {
        return;
    }

    hr = text.DrawFramebuffer (m_pixels.data(), m_width, m_height,
                               (float) dest.left, (float) dest.top,
                               (float) (dest.right - dest.left), (float) (dest.bottom - dest.top));
    IGNORE_RETURN_VALUE (hr, S_OK);
}
