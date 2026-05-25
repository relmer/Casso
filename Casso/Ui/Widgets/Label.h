#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Label
//
//  Pure-render widget. Holds a positioned text run and forwards it
//  through DwriteTextRenderer when Paint is invoked. No state, no
//  hit-testing, no focus participation -- callers compose Labels
//  with the interactive widgets they decorate.
//
////////////////////////////////////////////////////////////////////////////////

class Label
{
public:
    void  SetRect       (const RECT & rect) { m_rect = rect; }
    void  SetText       (const std::wstring & text) { m_text = text; }
    void  SetColorArgb  (uint32_t argb) { m_argb = argb; }
    void  SetFontSizeDip (float dip) { m_fontDip = dip; }
    void  SetDpi        (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect      () const { return m_rect; }
    const std::wstring & Text      () const { return m_text; }
    uint32_t             ColorArgb () const { return m_argb; }
    float                FontSizeDip () const { return m_fontDip; }

    void  Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
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
                                                  L"Segoe UI"));
    }

private:
    RECT          m_rect    = {};
    std::wstring  m_text;
    uint32_t      m_argb    = 0xFFFFFFFF;
    float         m_fontDip = 13.0f;
    DpiScaler     m_scaler;
};
