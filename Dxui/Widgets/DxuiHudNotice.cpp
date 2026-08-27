#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiHudNotice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHudNotice::Layout
//
//  The whole given rect: the caller decides where the notice sits, and the
//  line centers itself in what it is given.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHudNotice::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_dpi = scaler.Dpi();
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHudNotice::Paint
//
//  The glow, then the line. This is MatrixRain's DrawFeatheredGlow, ported:
//
//      for (int i = glowLayers; i > 0; --i)
//          offset  = i
//          opacity = 1 - i / glowLayers
//          draw the string in black at that offset in the eight compass
//          directions, skipping the center
//
//  Layers run OUTSIDE IN, so each lands on what the wider, fainter ones
//  already put down and the darkness climbs toward the ink rather than
//  stepping at each radius. Every sample is the whole string, which is what
//  makes the result follow the text's own contour -- dense where strokes
//  crowd, thin between words, gone a few pixels out -- instead of sitting
//  behind it as a shape.
//
//  THE OFFSETS ARE PIXELS, NOT DIPS, exactly as in the original: the glyphs
//  scale with the display and the glow does not, so it stays a tight edge
//  treatment on a dense display rather than a halo that grows until its eight
//  directions separate into spokes. Deviating from that is what made a first
//  attempt at this look like a spiked outline.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHudNotice::Paint (IDxuiPainter      & painter,
                           IDxuiTextRenderer & text,
                           const IDxuiTheme  & theme)
{
    const wchar_t *  face   = (m_fontFace != nullptr) ? m_fontFace : DxuiTheme::kBodyFace;
    float            fontPx = 0.0f;
    float            bl     = 0.0f;
    float            bt     = 0.0f;
    float            bw     = 0.0f;
    float            bh     = 0.0f;
    HRESULT          hr     = S_OK;
    RECT             bounds = Bounds();



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (!Visible() || m_text.empty() || bounds.right <= bounds.left)
    {
        return;
    }

    fontPx = m_fontSizeDip * (float) m_dpi / 96.0f;
    bl     = (float) bounds.left;
    bt     = (float) bounds.top;
    bw     = (float) (bounds.right - bounds.left);
    bh     = (float) (bounds.bottom - bounds.top);

    for (int i = kGlowLayers; i > 0; i--)
    {
        float     offset  = (float) i;
        float     opacity = 1.0f - ((float) i / (float) kGlowLayers);
        uint32_t  argb    = ((uint32_t) (opacity * 255.0f + 0.5f) << 24);

        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                hr = text.DrawString (m_text.c_str(),
                                      bl + offset * (float) dx,
                                      bt + offset * (float) dy,
                                      bw, bh, argb, fontPx, face,
                                      DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                                      DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    hr = text.DrawString (m_text.c_str(), bl, bt, bw, bh,
                          m_textArgb, fontPx, face,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
