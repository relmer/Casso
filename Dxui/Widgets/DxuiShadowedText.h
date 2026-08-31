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

    // How far the shadow reaches, in PIXELS. Fewer layers is a tighter,
    // harder rim; more is a broader, softer one.
    //
    // WORTH TUNING PER SITE, because what the shadow has to survive is not
    // the same everywhere. Over the dark desk backdrop a wide one is
    // invisible and simply works. Over the monitor's pale case it is the
    // conspicuous part of the drawing, and the eight compass directions
    // stop reading as a halo and start reading as spikes -- the whole
    // STRING is redrawn at each offset, so a short one has too little
    // overlap to hide its own structure. A short reach keeps it a rim.
    void  SetGlowLayers  (int layers)                { m_glowLayers = layers; }

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

    // MatrixRain's glowLayers. Ten rings of eight compass points, ring i at
    // i PIXELS out with alpha 1 - i/10 -- the outermost contributes nothing
    // and the innermost is nearly opaque. Pixels rather than DIPs is the
    // original's own choice and worth keeping: the glow stays a tight edge
    // treatment on a dense display instead of spreading until its eight
    // directions read as spokes.
    static constexpr int    kGlowLayers = 10;
    static constexpr float  kFontDip    = 13.0f;

private:
    std::wstring     m_text;
    const wchar_t *  m_fontFace    = nullptr;   // null = the theme's body face
    uint32_t         m_textArgb    = 0xFFFFFFFF;
    float            m_fontSizeDip = kFontDip;
    UINT             m_dpi         = 96;
    int              m_glowLayers  = kGlowLayers;
    DxuiTextHAlign   m_hAlign      = DxuiTextHAlign::Center;
    DxuiTextVAlign   m_vAlign      = DxuiTextVAlign::Center;
};
