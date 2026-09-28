#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiTextRenderer.h"
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
    void  SetColor       (DxuiArgb color) { m_argb = color; m_useThemeRole = false; }

    const RECT         & GetRect () const { return m_boundsDip; }
    const std::wstring & GetText () const { return m_text; }
    float                GetFontSizeDip () const { return m_fontDip; }

    // Legacy theme-less paint; draws with the color pinned by SetColor.
    void  Paint (IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims so DxuiLabel slots
    //  into DxuiPanel trees alongside other IDxuiControl-derived
    //  widgets.
    //
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler.SetDpi (scaler.GetDpi());
    }

    // Themed paint; resolves color and font size from the theme.
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    std::wstring        GetAccessibleName () const override { return m_text; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    void  DrawResolved (IDxuiTextRenderer & text, uint32_t argb, float fontDip) const;


    static constexpr float  s_kFallbackFontDip = 13.0f;

    std::wstring    m_text;
    std::wstring    m_fontFace     = DxuiTheme::kBodyFace;
    // Only reachable through SetColor or the theme-less Paint overload; a
    // themed paint resolves the role instead.
    uint32_t        m_argb         = 0xFFFFFFFF;
    float           m_fontDip      = s_kFallbackFontDip;
    DxuiTextHAlign  m_hAlign       = DxuiTextHAlign::Left;
    DxuiTextVAlign  m_vAlign       = DxuiTextVAlign::Center;
    DxuiFontWeight  m_weight       = DxuiFontWeight::Normal;
    DxuiTextRole    m_role         = DxuiTextRole::Body;

    // Theme resolution is the DEFAULT, including for the default-constructed
    // label. It was once opt-in, set only by the text-taking constructor and
    // by SetTextRole, so a label declared as a bare member and given its words
    // through SetText painted with m_argb -- the hard-coded white below. Whole
    // settings pages build their labels that way, and every one of them drew
    // white text on the retro and modern palettes. SetColor still opts out.
    bool            m_useThemeRole = true;
    DxuiDpiScaler   m_scaler;
};
