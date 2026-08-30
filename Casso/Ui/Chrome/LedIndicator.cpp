#include "Pch.h"

#include "LedIndicator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LedIndicator
//
////////////////////////////////////////////////////////////////////////////////

LedIndicator::LedIndicator()
{
    m_focusable = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PositionAt
//
//  Places the LED at a point, sizing its core and halo for the DPI.
//
//  The halo is grown OUTWARD from the core rather than the core being inset
//  inside a fixed box, so the caller positions the light itself and the glow
//  extends around it. Positioning the outer box instead would move the visible
//  dot whenever the halo size changed.
//
//  Both dimensions are FLOORED at their 96-DPI values. A sub-scale LED would
//  round down to a few pixels and disappear -- and an indicator that vanishes
//  is worse than one slightly larger than its nominal scale.
//
//  A zero DPI falls back to the base rather than producing a zero-size LED,
//  since callers legitimately position chrome before the window's DPI is
//  known.
//
//  The control's bounds are set to the HALO rect, so hit-testing and
//  invalidation cover the glow rather than clipping it at the core's edge.
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::PositionAt (int x, int y, UINT dpi)
{
    constexpr int  kBaseDpi       = 96;
    constexpr int  kCorePx        = 7;
    constexpr int  kHaloPaddingPx = 2;



    int  effectiveDpi = (dpi == 0) ? kBaseDpi : (int) dpi;
    int  core         = MulDiv (kCorePx, effectiveDpi, kBaseDpi);
    int  haloPadding  = MulDiv (kHaloPaddingPx, effectiveDpi, kBaseDpi);



    if (core < kCorePx)
    {
        core = kCorePx;
    }

    if (haloPadding < kHaloPaddingPx)
    {
        haloPadding = kHaloPaddingPx;
    }

    m_layout.coreRect.left   = x;
    m_layout.coreRect.top    = y;
    m_layout.coreRect.right  = x + core;
    m_layout.coreRect.bottom = y + core;
    m_layout.haloRect.left   = x - haloPadding;
    m_layout.haloRect.top    = y - haloPadding;
    m_layout.haloRect.right  = x + core + haloPadding;
    m_layout.haloRect.bottom = y + core + haloPadding;
    SetBounds (m_layout.haloRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  IDxuiControl override -- treats bounds.left/top as the LED core
//  origin and derives DPI from the scaler. Halo padding is added
//  outside; the resulting SetBounds reflects the halo rect.
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    PositionAt (boundsDip.left, boundsDip.top, scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCoreArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t LedIndicator::GetCoreArgb (const CassoTheme & theme) const
{
    uint32_t  argb = theme.ledIdle;



    if      (m_state == LedState::Active)  { argb = theme.ledActive;  }
    else if (m_state == LedState::Present) { argb = theme.ledPresent; }

    return argb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHaloArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t LedIndicator::GetHaloArgb (const CassoTheme & theme) const
{
    // Only an active LED glows; 0 is fully transparent, so the halo pass
    // draws nothing in the other states.
    return (m_state == LedState::Active) ? theme.ledHalo : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::Paint (IDxuiPainter & painter, IDxuiTextRenderer & /*text*/, const IDxuiTheme & dxuiTheme)
{
    const CassoTheme &  theme = static_cast<const CassoTheme &> (dxuiTheme);
    uint32_t            halo  = 0;
    float               cx    = 0.0f;
    float               cy    = 0.0f;
    float               coreR = 0.0f;
    float               haloR = 0.0f;



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    halo = GetHaloArgb (theme);
    cx = (float) (m_layout.coreRect.left + m_layout.coreRect.right) * 0.5f;
    cy = (float) (m_layout.coreRect.top  + m_layout.coreRect.bottom) * 0.5f;
    coreR = (float) (m_layout.coreRect.right - m_layout.coreRect.left) * 0.5f;
    haloR = (float) (m_layout.haloRect.right - m_layout.haloRect.left) * 0.5f;



    if (halo != 0)
    {
        painter.FillCircleApprox (cx, cy, haloR, halo);
    }

    painter.FillCircleApprox (cx, cy, coreR, GetCoreArgb (theme));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Color-overriding variant used by widgets that need a fixed LED hue
//  independent of the active theme's amber/red/green drive LED -- the
//  joystick-mode toggle, for instance, always glows a realistic LED
//  blue. A zero `haloArgb` suppresses the halo (dark/off state).
//
////////////////////////////////////////////////////////////////////////////////

void LedIndicator::Paint (IDxuiPainter & painter, uint32_t coreArgb, uint32_t haloArgb) const
{
    float  cx    = (float) (m_layout.coreRect.left + m_layout.coreRect.right) * 0.5f;
    float  cy    = (float) (m_layout.coreRect.top  + m_layout.coreRect.bottom) * 0.5f;
    float  coreR = (float) (m_layout.coreRect.right - m_layout.coreRect.left) * 0.5f;



    if (haloArgb != 0)
    {
        // Soft, fuzzy feathered glow: stack concentric circles from a
        // wide / dim outer ring inward to a brighter ring just outside
        // the core. Cumulative SRC_OVER blending of equal-alpha rings
        // gives a smooth quadratic-looking falloff with no visible bands
        // once the ring count is high enough. Glow radius scales with
        // the core so DPI changes look right without retuning.
        constexpr int    s_kGlowRings        = 12;
        constexpr float  s_kGlowRadiusFactor = 1.5f;

        uint8_t  haloA      = (uint8_t) ((haloArgb >> 24) & 0xFF);
        uint32_t haloRgb    = haloArgb & 0x00FFFFFF;
        float    glowOuterR = coreR * s_kGlowRadiusFactor;
        float    glowInnerR = coreR + 1.0f;
        // Per-ring alpha picked so the cumulative SRC_OVER sum at the
        // core edge sits near the source halo alpha. 1 - (1-a)^N = haloA
        // -> a = 1 - (1 - haloA)^(1/N).
        float    perRing    = 1.0f - powf (1.0f - ((float) haloA / 255.0f), 1.0f / (float) s_kGlowRings);
        uint8_t  ringA      = (uint8_t) (perRing * 255.0f + 0.5f);
        uint32_t ringArgb   = ((uint32_t) ringA << 24) | haloRgb;

        for (int i = 0; i < s_kGlowRings; i++)
        {
            // i==0 is the largest (faintest visual contribution at the
            // outer edge); i==N-1 is the smallest (just outside core).
            float  t = (float) i / (float) (s_kGlowRings - 1);
            float  r = glowOuterR + (glowInnerR - glowOuterR) * t;

            painter.FillCircleApprox (cx, cy, r, ringArgb);
        }
    }

    painter.FillCircleApprox (cx, cy, coreR, coreArgb);
}
