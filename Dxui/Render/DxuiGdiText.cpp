#include "Pch.h"

#include "Render/DxuiGdiText.h"
#include "Render/DxuiSvgPath.h"
#include "DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGdiText::DxuiGdiText
//
////////////////////////////////////////////////////////////////////////////////

DxuiGdiText::DxuiGdiText (DxuiTextRenderer & renderer)
    : m_renderer (renderer)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGdiText::~DxuiGdiText
//
////////////////////////////////////////////////////////////////////////////////

DxuiGdiText::~DxuiGdiText()
{
    for (const auto & [key, font] : m_gdiFonts)
    {
        DeleteObject (font);
    }

    if (m_gdiDc != nullptr)
    {
        DeleteDC (m_gdiDc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectGdiFont
//
//  The face at a whole pixel height into the measuring DC; each face and
//  height is made once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiGdiText::SelectGdiFont (float fontSizePx, const wchar_t * fontFamily)
{
    HRESULT                         hr     = S_OK;
    int                             height = -(int) std::lround (fontSizePx);
    std::pair<std::wstring, int>    key    = { (fontFamily != nullptr) ? fontFamily : L"Segoe UI", height };
    HFONT                           font   = nullptr;



    if (m_gdiDc == nullptr)
    {
        m_gdiDc = CreateCompatibleDC (nullptr);
        CWR (m_gdiDc != nullptr);
    }

    if (!m_gdiFonts.contains (key))
    {
        font = CreateFontW (height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, key.first.c_str());
        CWR (font != nullptr);
        m_gdiFonts[key] = font;
    }

    SelectObject (m_gdiDc, m_gdiFonts[key]);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineHeightGdi
//
//  The font's height in GDI's measure, ascent and descent each rounded:
//  25 px for Segoe UI at 150%, where its own metrics give 23.9.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiGdiText::GetLineHeightGdi (float fontSizePx, const wchar_t * fontFamily, float & outHeightPx)
{
    HRESULT     hr      = S_OK;
    TEXTMETRICW metrics = {};
    BOOL        gotIt   = FALSE;



    outHeightPx = 0.0f;

    hr = SelectGdiFont (fontSizePx, fontFamily);
    CHR (hr);

    gotIt = GetTextMetricsW (m_gdiDc, &metrics);
    CWR (gotIt);

    outHeightPx = (float) metrics.tmHeight;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureStringGdi
//
//  The face at a whole pixel height, measured by DrawText with its kerning,
//  as the shell's list view lays out a caption; each face and height is made
//  once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiGdiText::MeasureStringGdi (const wchar_t * text, float fontSizePx, const wchar_t * fontFamily, float & outWidthPx)
{
    HRESULT  hr     = S_OK;
    RECT     box    = {};
    int      drawn  = 0;



    outWidthPx = 0.0f;

    hr = SelectGdiFont (fontSizePx, fontFamily);
    CHR (hr);

    //  DrawText's measure, kerning pairs included, as the shell's own.
    drawn = DrawTextW (m_gdiDc, text, -1, &box, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    CWR (drawn != 0 || text[0] == L'\0');

    outWidthPx = (float) (box.right - box.left);

Error:
    return hr;
}
