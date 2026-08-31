#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHudNotice
//
//  A line of text laid over arbitrary content -- video, a 3D scene, a game --
//  and kept legible against all of it.
//
//  Ordinary chrome can assume its own background. A notice over live content
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
//  Cost is one text draw per offset while the notice is up, so this is for the
//  persistent message that has to be readable over anything -- a captured
//  pointer, a mode with a way out -- and not for ordinary chrome.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHudNotice : public IDxuiControl
{
public:
    DxuiHudNotice  () = default;
    ~DxuiHudNotice () override = default;

    // Visibility and bounds are the base control's -- a second copy here
    // would shadow the ones the panel tree consults.
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetFontSizeDip (float sizeDip)             { m_fontSizeDip = sizeDip; }
    void  SetFontFace    (const wchar_t * face)      { m_fontFace = face; }
    void  SetTextColor   (uint32_t argb)             { m_textArgb = argb; }
    void  SetDpi         (UINT dpi)                  { m_dpi = dpi; }

    // The notice spans `boundsDip` and centers its line inside it. The glow
    // reaches kGlowLayers pixels beyond the ink, so the rect wants at least
    // that much slack around the text or the shadow clips against its edges.
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
};
