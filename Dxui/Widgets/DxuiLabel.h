#pragma once

#include "Pch.h"






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

class DxuiLabel
{
public:
    void  SetRect        (const RECT & rect) { m_rect = rect; }
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetColorArgb   (uint32_t argb) { m_argb = argb; }
    void  SetFontSizeDip (float dip) { m_fontDip = dip; }
    void  SetFontFace    (const std::wstring & face) { m_fontFace = face; }
    void  SetHAlign      (DxuiTextHAlign a) { m_hAlign = a; }
    void  SetVAlign      (DxuiTextVAlign a) { m_vAlign = a; }
    void  SetFontWeight  (DWRITE_FONT_WEIGHT w) { m_weight = w; }
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect      () const { return m_rect; }
    const std::wstring & Text      () const { return m_text; }
    uint32_t             ColorArgb () const { return m_argb; }
    float                FontSizeDip () const { return m_fontDip; }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
    {
        HRESULT  hr = S_OK;

        UNREFERENCED_PARAMETER (painter);

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                                  (float) m_rect.left,
                                                  (float) m_rect.top,
                                                  (float) (m_rect.right  - m_rect.left),
                                                  (float) (m_rect.bottom - m_rect.top),
                                                  m_argb,
                                                  m_scaler.Pxf (m_fontDip),
                                                  m_fontFace.c_str(),
                                                  m_hAlign,
                                                  m_vAlign,
                                                  m_weight));
    }

private:
    RECT                          m_rect     = {};
    std::wstring                  m_text;
    std::wstring                  m_fontFace = L"Segoe UI";
    uint32_t                      m_argb     = 0xFFFFFFFF;
    float                         m_fontDip  = 13.0f;
    DxuiTextHAlign    m_hAlign   = DxuiTextHAlign::Left;
    DxuiTextVAlign    m_vAlign   = DxuiTextVAlign::Center;
    DWRITE_FONT_WEIGHT            m_weight   = DWRITE_FONT_WEIGHT_NORMAL;
    DxuiDpiScaler                     m_scaler;
};
