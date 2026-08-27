#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "HudBanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HudBanner::Layout
//
//  The whole given rect: the caller decides where the notice sits, and the
//  line centers itself in what it is given. The backdrop's feather reaches
//  kFeatherDp outward from the text, so the rect wants that much slack on
//  every side -- which is why this takes one rather than sizing to its string.
//
////////////////////////////////////////////////////////////////////////////////

void HudBanner::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_dpi = scaler.Dpi();
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HudBanner::Paint
//
//  A feathered shadow, then the line.
//
//  The backdrop is built from nested rectangles drawn OUTSIDE IN, each a step
//  narrower than the last and each carrying the same small alpha. Where N of
//  them overlap the accumulated coverage is 1 - (1 - a)^N, so the darkness
//  climbs smoothly from nothing at the outer edge to nearly the full value
//  over the text: a ramp with no step in it, out of N cheap quads and no
//  shader. The per-layer alpha is SOLVED from that identity, so the depth and
//  the darkness can be tuned independently.
//
//  Drawing them the other way round would not work -- the accumulation has to
//  land on what the wider, fainter layers already put down.
//
//  The first try was the other technique from the same source: the string
//  itself redrawn in black in rings around the ink. It reads beautifully at
//  the size that source uses it and badly at this one -- eight or twelve
//  directions leave wedges of daylight between them once the ring is a few
//  pixels out, and a short line at chrome size came out wearing a spiky
//  outline rather than a soft cloud.
//
////////////////////////////////////////////////////////////////////////////////

void HudBanner::Paint (IDxuiPainter      & painter,
                       IDxuiTextRenderer & text,
                       const IDxuiTheme  & theme)
{
    float     fontPx    = 0.0f;
    float     inkW      = 0.0f;
    float     inkH      = 0.0f;
    float     cx        = 0.0f;
    float     cy        = 0.0f;
    float     coreHalfW = 0.0f;
    float     coreHalfH = 0.0f;
    float     feather   = 0.0f;
    float     layerA    = 0.0f;
    uint32_t  layerArgb = 0;
    HRESULT   hr        = S_OK;
    RECT      bounds    = Bounds();



    UNREFERENCED_PARAMETER (theme);

    if (!Visible() || m_text.empty() || bounds.right <= bounds.left)
    {
        return;
    }

    fontPx  = m_fontSizeDip * (float) m_dpi / 96.0f;
    feather = kFeatherDp * (float) m_dpi / 96.0f;
    cx      = (float) (bounds.left + bounds.right) * 0.5f;
    cy      = (float) (bounds.top + bounds.bottom) * 0.5f;

    // The core is the text's own ink plus a little air. MEASURED rather than
    // assumed: a notice whose wording changes should not need its backdrop
    // resized by hand, and a shadow wider than its text stops reading as a
    // shadow of anything.
    hr = text.MeasureString (m_text.c_str(), fontPx, DxuiTheme::kBodyFace, inkW, inkH);

    if (FAILED (hr) || inkW <= 0.0f)
    {
        inkW = (float) (bounds.right - bounds.left) * 0.5f;
        inkH = fontPx * 1.4f;
    }

    coreHalfW = inkW * 0.5f + feather * 0.35f;
    coreHalfH = inkH * 0.5f + feather * 0.15f;

    layerA    = 1.0f - std::pow (1.0f - kMaxOpacity, 1.0f / (float) kFeatherLayers);
    layerArgb = ((uint32_t) (layerA * 255.0f + 0.5f) << 24);

    for (int layer = kFeatherLayers; layer > 0; layer--)
    {
        float  out = feather * (float) layer / (float) kFeatherLayers;

        painter.FillRect (cx - coreHalfW - out, cy - coreHalfH - out,
                          (coreHalfW + out) * 2.0f, (coreHalfH + out) * 2.0f,
                          layerArgb);
    }

    hr = text.DrawString (m_text.c_str(),
                          (float) bounds.left, (float) bounds.top,
                          (float) (bounds.right - bounds.left),
                          (float) (bounds.bottom - bounds.top),
                          0xFFFFFFFF, fontPx, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
