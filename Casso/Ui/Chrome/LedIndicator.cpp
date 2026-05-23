#include "Pch.h"

#include "LedIndicator.h"





namespace
{
    constexpr int  s_kBaseDpi       = 96;
    constexpr int  s_kCorePx        = 12;
    constexpr int  s_kHaloPaddingPx = 4;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::Layout (int x, int y, UINT dpi)
{
    int  effectiveDpi = (dpi == 0) ? s_kBaseDpi : (int) dpi;
    int  core         = MulDiv (s_kCorePx, effectiveDpi, s_kBaseDpi);
    int  haloPadding  = MulDiv (s_kHaloPaddingPx, effectiveDpi, s_kBaseDpi);



    if (core < s_kCorePx)
    {
        core = s_kCorePx;
    }

    if (haloPadding < s_kHaloPaddingPx)
    {
        haloPadding = s_kHaloPaddingPx;
    }

    m_layout.coreRect.left   = x;
    m_layout.coreRect.top    = y;
    m_layout.coreRect.right  = x + core;
    m_layout.coreRect.bottom = y + core;
    m_layout.haloRect.left   = x - haloPadding;
    m_layout.haloRect.top    = y - haloPadding;
    m_layout.haloRect.right  = x + core + haloPadding;
    m_layout.haloRect.bottom = y + core + haloPadding;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CoreArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t LedIndicator::CoreArgb (const ChromeTheme & theme) const
{
    if (m_state == LedState::Active)
    {
        return theme.ledActiveArgb;
    }

    if (m_state == LedState::Present)
    {
        return theme.ledPresentArgb;
    }

    return theme.ledIdleArgb;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HaloArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t LedIndicator::HaloArgb (const ChromeTheme & theme) const
{
    if (m_state == LedState::Active)
    {
        return theme.ledHaloArgb;
    }

    return 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::Paint (DxUiPainter & painter, const ChromeTheme & theme) const
{
    uint32_t  halo = HaloArgb (theme);



    if (halo != 0)
    {
        painter.FillRect ((float) m_layout.haloRect.left,
                          (float) m_layout.haloRect.top,
                          (float) (m_layout.haloRect.right - m_layout.haloRect.left),
                          (float) (m_layout.haloRect.bottom - m_layout.haloRect.top),
                          halo);
    }

    painter.FillRect ((float) m_layout.coreRect.left,
                      (float) m_layout.coreRect.top,
                      (float) (m_layout.coreRect.right - m_layout.coreRect.left),
                      (float) (m_layout.coreRect.bottom - m_layout.coreRect.top),
                      CoreArgb (theme));
}
