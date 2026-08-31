#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiShadowedText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadowedText::Layout
//
//  The whole given rect: the caller decides where the text sits, and the line
//  aligns itself in what it is given.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiShadowedText::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_dpi = scaler.GetDpi();
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadowedText::Paint
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

void DxuiShadowedText::Paint (IDxuiPainter      & painter,
                              IDxuiTextRenderer & text,
                              const IDxuiTheme  & theme)
{
    const wchar_t *  face   = (m_fontFace != nullptr) ? m_fontFace : DxuiTheme::kBodyFace;
    RECT             bounds = GetBounds();



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (!IsVisible() || m_text.empty() || bounds.right <= bounds.left)
    {
        return;
    }

    PaintShadowed (text, m_text.c_str(),
                   (float) bounds.left, (float) bounds.top,
                   (float) (bounds.right - bounds.left),
                   (float) (bounds.bottom - bounds.top),
                   m_textArgb, m_fontSizeDip * (float) m_dpi / 96.0f, face,
                   m_hAlign, m_vAlign, m_glowLayers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadowedText::PaintShadowed
//
//  The rings, then the line, through whatever target the renderer is drawing
//  to -- the back buffer for a control, an off-screen texture for the desk
//  scene's drive label.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiShadowedText::PaintShadowed (IDxuiTextRenderer & renderer,
                                      const wchar_t     * text,
                                      float               x,
                                      float               y,
                                      float               width,
                                      float               height,
                                      uint32_t            argb,
                                      float               fontPx,
                                      const wchar_t     * face,
                                      DxuiTextHAlign      hAlign,
                                      DxuiTextVAlign      vAlign,
                                      int                 glowLayers)
{
    const wchar_t *  useFace = (face != nullptr) ? face : DxuiTheme::kBodyFace;
    HRESULT          hr      = S_OK;



    for (int i = glowLayers; i > 0; i--)
    {
        float     offset  = (float) i;
        float     opacity = 1.0f - ((float) i / (float) glowLayers);
        uint32_t  shadow  = ((uint32_t) (opacity * 255.0f + 0.5f) << 24);

        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                hr = renderer.DrawString (text,
                                          x + offset * (float) dx,
                                          y + offset * (float) dy,
                                          width, height, shadow, fontPx, useFace,
                                          hAlign, vAlign,
                                          DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    hr = renderer.DrawString (text, x, y, width, height, argb, fontPx, useFace,
                              hAlign, vAlign, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
