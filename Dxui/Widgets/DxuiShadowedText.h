#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadowedText
//
//  A line of text laid over arbitrary content -- video, a 3D scene, a game --
//  and kept legible against all of it.
//
//  Ordinary chrome can assume its own background. Text over live content
//  cannot: light text disappears into a pale frame, dark text into a dark one,
//  and a solid backing plate reads as a dialog nobody dismissed. What works is
//  a SHADOW IN THE SHAPE OF THE TEXT -- the string drawn many times in black
//  underneath itself at small offsets, so what accumulates hugs the glyphs and
//  thins out between and around them.
//
//  The construction is MatrixRain's DrawFeatheredGlow, ported rather than
//  reinvented: rings of eight compass offsets, each ring a pixel further out
//  and fainter than the last. See Paint.
//
//  Cost is one text draw per offset while the text is up -- eighty draws at
//  the default layer count -- so this is for the few strings that have to
//  read over anything, not for ordinary chrome.
//
//  ALIGNMENT IS THE CALLER'S. This draws where it is told; DxuiHudNotice is
//  the centered-in-a-band case, and lives on top of this rather than inside
//  it, so a readout that wants a corner is not fighting a rule written for a
//  notification.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiShadowedText : public IDxuiControl
{
public:
    DxuiShadowedText  () = default;
    ~DxuiShadowedText () override = default;

    // Visibility and bounds are the base control's -- a second copy here
    // would shadow the ones the panel tree consults.
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetFontSizeDip (float sizeDip)             { m_fontSizeDip = sizeDip; }
    void  SetFontFace    (const wchar_t * face)      { m_fontFace = face; }
    void  SetTextColor   (uint32_t argb)             { m_textArgb = argb; }
    void  SetDpi         (UINT dpi)                  { m_dpi = dpi; }

    // How far the shadow reaches, in PIXELS at the size the text is drawn.
    //
    // SCALE IT WITH THE FONT. The default suits body text; a caller drawing
    // at four times that size and scaling the result down -- the desk scene
    // renders its drive label into a texture -- wants four times the reach,
    // or the halo shrinks to a hairline on the way to the screen.
    void  SetGlowReachPx (int reachPx)               { m_reachPx = reachPx; }

    void  SetAlign       (DxuiTextHAlign h, DxuiTextVAlign v)
    {
        m_hAlign = h;
        m_vAlign = v;
    }

    // The text spans `boundsDip` and aligns inside it. The glow reaches
    // kGlowLayers pixels beyond the ink, so the rect wants at least that much
    // slack around the text or the shadow clips against its edges.
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    void  Paint  (IDxuiPainter      & painter,
                  IDxuiTextRenderer & text,
                  const IDxuiTheme  & theme) override;

    // The shadow construction itself, so a caller that is not a control can
    // have it. The desk scene renders the drive label into a TEXTURE -- the
    // name is geometry there, and a halo painted over the scene afterwards
    // would stay flat on the glass while the text it belongs to turned
    // away -- and needs the same rings this paints.
    static void  PaintShadowed (IDxuiTextRenderer & renderer,
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
                                int                 reachPx);

    // MatrixRain's glowLayers: ten rings, ring r at r PIXELS out with alpha
    // 1 - r/10, so the outermost contributes nothing and the innermost is
    // nearly opaque. Pixels rather than DIPs is the original's own choice and
    // worth keeping: the glow stays an edge treatment on a dense display
    // rather than one that grows with the display.
    static constexpr int    kGlowReachPx     = 10;
    static constexpr float  kFontDip         = 13.0f;

    // Samples per ring. EIGHT, flat, which is the count the original drew and
    // the count that holds 60 fps: every sample redraws the whole string, so
    // this multiplies straight into the frame. Scaling it with the ring's
    // circumference tripled the work for a halo nobody could tell apart.
    static constexpr int    kRingSamples     = 8;

private:
    std::wstring     m_text;
    const wchar_t *  m_fontFace    = nullptr;   // null = the theme's body face
    uint32_t         m_textArgb    = 0xFFFFFFFF;
    float            m_fontSizeDip = kFontDip;
    UINT             m_dpi         = 96;
    int              m_reachPx     = kGlowReachPx;
    DxuiTextHAlign   m_hAlign      = DxuiTextHAlign::Center;
    DxuiTextVAlign   m_vAlign      = DxuiTextVAlign::Center;
};
