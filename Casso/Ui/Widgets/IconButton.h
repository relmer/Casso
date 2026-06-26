#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"




////////////////////////////////////////////////////////////////////////////////
//
//  IconButton
//
//  Small square glyph button (Segoe MDL2 Assets). Draws a single icon
//  glyph with a hover/pressed background and fires a click callback. Used
//  for compact affordances such as the settings panel's sound-preview
//  play buttons.
//
//  Mouse contract:
//      LButtonDown over hit rect -> arm (pressed).
//      LButtonUp   over hit rect -> fire click.
//
////////////////////////////////////////////////////////////////////////////////

class IconButton
{
public:
    using ClickFn = std::function<void ()>;

    void  SetRect    (const RECT & rect)            { m_rect = rect; }
    void  SetGlyph   (const wchar_t * glyph)        { m_glyph = glyph; }
    void  SetDpi     (UINT dpi)                     { m_scaler.SetDpi (dpi); }
    void  SetClick   (ClickFn fn)                   { m_click = std::move (fn); }
    void  SetEnabled (bool enabled)                 { m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    bool  Enabled    () const                       { return m_enabled; }

    bool  HitTest        (int x, int y) const;
    void  SetMouseHover  (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    void  Paint          (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme) const;

private:
    static constexpr wchar_t   s_kpszMdl2Family[] = L"Segoe MDL2 Assets";
    static constexpr float     s_kGlyphFontDp     = 12.0f;
    static constexpr uint32_t  s_kDisabledFg      = 0xFF6A7585;

    RECT                 m_rect    = {};
    const wchar_t      * m_glyph   = L"";
    ClickFn              m_click;
    DpiScaler            m_scaler;
    bool                 m_enabled = true;
    bool                 m_hover   = false;
    bool                 m_pressed = false;
};
