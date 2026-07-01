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
    ~DxuiLabel() override = default;

    void  SetRect        (const RECT & rect) { SetBounds (rect); }
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetColorArgb   (uint32_t argb) { m_argb = argb; }
    void  SetFontSizeDip (float dip) { m_fontDip = dip; }
    void  SetFontFace    (const std::wstring & face) { m_fontFace = face; }
    void  SetHAlign      (DxuiTextHAlign a) { m_hAlign = a; }
    void  SetVAlign      (DxuiTextVAlign a) { m_vAlign = a; }
    void  SetFontWeight  (DxuiFontWeight w) { m_weight = w; }
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect      () const { return m_boundsDip; }
    const std::wstring & Text      () const { return m_text; }
    uint32_t             ColorArgb () const { return m_argb; }
    float                FontSizeDip () const { return m_fontDip; }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
    {
        HRESULT  hr = S_OK;

        UNREFERENCED_PARAMETER (painter);

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                                  (float) m_boundsDip.left,
                                                  (float) m_boundsDip.top,
                                                  (float) (m_boundsDip.right  - m_boundsDip.left),
                                                  (float) (m_boundsDip.bottom - m_boundsDip.top),
                                                  m_argb,
                                                  m_scaler.Pxf (m_fontDip),
                                                  m_fontFace.c_str(),
                                                  m_hAlign,
                                                  m_vAlign,
                                                  m_weight));
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

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        UNREFERENCED_PARAMETER (theme);
        static_cast<const DxuiLabel *> (this)->Paint (painter, text);
    }

    std::wstring        AccessibleName () const override { return m_text; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    std::wstring                  m_text;
    std::wstring                  m_fontFace = DxuiTheme::kBodyFace;
    uint32_t                      m_argb     = 0xFFFFFFFF;
    float                         m_fontDip  = 13.0f;
    DxuiTextHAlign    m_hAlign   = DxuiTextHAlign::Left;
    DxuiTextVAlign    m_vAlign   = DxuiTextVAlign::Center;
    DxuiFontWeight                m_weight   = DxuiFontWeight::Normal;
    DxuiDpiScaler                     m_scaler;
};
