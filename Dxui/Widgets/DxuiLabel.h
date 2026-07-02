#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/DxuiTheme.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel
//
//  Pure-render widget. Holds a positioned text run and forwards it
//  through DxuiTextRenderer when Paint is invoked. No state, no
//  hit-testing, no focus participation -- callers compose Labels
//  with the interactive widgets they decorate.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiLabel : public IDxuiControl
{
public:
    DxuiLabel  () = default;

    //
    //  Primary constructor. Text is the label's defining property;
    //  role, alignment, and font size are static style that the theme /
    //  DPI resolve at paint. Font size defaults to the sentinel, so the
    //  label draws with the theme's body font size unless overridden.
    //  Color is a SEMANTIC ROLE, not a resolved value -- see DxuiTextRole.
    //
    explicit DxuiLabel  (std::wstring    text,
                         DxuiTextRole    role    = DxuiTextRole::Body,
                         DxuiTextHAlign  hAlign  = DxuiTextHAlign::Left,
                         DxuiTextVAlign  vAlign  = DxuiTextVAlign::Center,
                         float           fontDip = kDxuiDefaultFontSizeDip)
        : m_text (std::move (text))
        , m_fontDip (fontDip)
        , m_hAlign (hAlign)
        , m_vAlign (vAlign)
        , m_role (role)
        , m_useThemeRole (true)
    {
    }

    ~DxuiLabel() override = default;

    void  SetRect        (const RECT & rect) { SetBounds (rect); }
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetTextRole    (DxuiTextRole role) { m_role = role; m_useThemeRole = true; }
    void  SetFontSizeDip (float dip) { m_fontDip = dip; }
    void  SetFontFace    (const std::wstring & face) { m_fontFace = face; }
    void  SetTextAlign   (DxuiTextHAlign h, DxuiTextVAlign v) { m_hAlign = h; m_vAlign = v; }
    void  SetFontWeight  (DxuiFontWeight w) { m_weight = w; }
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    //
    //  Legacy explicit-color setter. Pins a resolved ARGB and opts the
    //  label OUT of theme-role resolution. Retained for consumers not yet
    //  migrated to roles; new code passes a DxuiTextRole instead.
    //
    void  SetColorArgb   (uint32_t argb) { m_argb = argb; m_useThemeRole = false; }

    const RECT         & Rect      () const { return m_boundsDip; }
    const std::wstring & Text      () const { return m_text; }
    float                FontSizeDip () const { return m_fontDip; }

    //
    //  Legacy theme-less paint. Draws with the pinned explicit color
    //  (SetColorArgb) -- role resolution needs a theme, so consumers that
    //  use this overload must set an explicit color. Kept for pre-role
    //  call sites; the tree paints through the theme overload below.
    //
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
    {
        DrawResolved (painter, text, m_argb, (m_fontDip > 0.0f) ? m_fontDip : s_kFallbackFontDip);
    }

    //
    //  IDxuiControl overrides — additive shims so DxuiLabel slots
    //  into DxuiPanel trees alongside other IDxuiControl-derived
    //  widgets.
    //
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler.SetDpi (scaler.Dpi());
    }

    //
    //  Themed paint. Resolves the color from the semantic role (unless an
    //  explicit color was pinned) and the font size from the theme's body
    //  font when left at the sentinel -- so a plain label carries no color
    //  or size of its own and tracks the active theme automatically.
    //
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        uint32_t  argb = m_useThemeRole ? theme.TextColor (m_role) : m_argb;
        float     dip  = (m_fontDip > 0.0f) ? m_fontDip : theme.BodyFont().sizeDip;

        DrawResolved (painter, text, argb, dip);
    }

    std::wstring        AccessibleName () const override { return m_text; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    void  DrawResolved (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t argb, float fontDip) const
    {
        HRESULT  hr = S_OK;

        UNREFERENCED_PARAMETER (painter);

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                                  (float) m_boundsDip.left,
                                                  (float) m_boundsDip.top,
                                                  (float) (m_boundsDip.right  - m_boundsDip.left),
                                                  (float) (m_boundsDip.bottom - m_boundsDip.top),
                                                  argb,
                                                  m_scaler.Pxf (fontDip),
                                                  m_fontFace.c_str(),
                                                  m_hAlign,
                                                  m_vAlign,
                                                  m_weight));
    }


    static constexpr float  s_kFallbackFontDip = 13.0f;

    std::wstring                  m_text;
    std::wstring                  m_fontFace = DxuiTheme::kBodyFace;
    uint32_t                      m_argb     = 0xFFFFFFFF;
    float                         m_fontDip  = s_kFallbackFontDip;
    DxuiTextHAlign    m_hAlign   = DxuiTextHAlign::Left;
    DxuiTextVAlign    m_vAlign   = DxuiTextVAlign::Center;
    DxuiFontWeight                m_weight   = DxuiFontWeight::Normal;
    DxuiTextRole                  m_role     = DxuiTextRole::Body;
    bool                          m_useThemeRole = false;
    DxuiDpiScaler                     m_scaler;
};
