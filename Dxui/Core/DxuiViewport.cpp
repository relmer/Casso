#include "Pch.h"

#include "Core/DxuiViewport.h"
#include "Core/DxuiThread.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::Layout
//
//  Stores the new bounds rectangle on the IDxuiControl base, then
//  fires the bounds-changed callback when the rectangle differs from
//  the last value we notified for. The first call always fires so the
//  external renderer learns the initial geometry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::Layout (
    const RECT          & boundsDip,
    const DxuiDpiScaler & scaler)
{
    bool  changed = false;

    DXUI_ASSERT_UI_THREAD();

    (void) scaler;
    SetBounds (boundsDip);

    changed = !m_hasNotifiedBounds
           || boundsDip.left   != m_lastNotifiedBoundsDip.left
           || boundsDip.top    != m_lastNotifiedBoundsDip.top
           || boundsDip.right  != m_lastNotifiedBoundsDip.right
           || boundsDip.bottom != m_lastNotifiedBoundsDip.bottom;

    if (changed)
    {
        m_lastNotifiedBoundsDip = boundsDip;
        m_hasNotifiedBounds     = true;
        if (m_onBoundsChanged)
        {
            m_onBoundsChanged (boundsDip);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::Paint
//
//  No-op. The external renderer (e.g. the Apple ][ framebuffer
//  D3D pass) draws into the same swap chain at the rectangle reported
//  by `Bounds()`. Chrome above the viewport paints on top through the
//  normal control-tree fanout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    DXUI_ASSERT_UI_THREAD();

    (void) painter;
    (void) text;
    (void) theme;
}
