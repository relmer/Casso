#pragma once

#include "Pch.h"

class DxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGdiText
//
//  Text measured by GDI's whole-pixel advances: GDI fonts, one a face and
//  pixel height, and the memory DC they are measured in.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiGdiText
{
public:
    explicit DxuiGdiText (DxuiTextRenderer & renderer);
    ~DxuiGdiText ();

    HRESULT  MeasureStringGdi (const wchar_t * text, float fontSizePx, const wchar_t * fontFamily, float & outWidthPx);
    HRESULT  GetLineHeightGdi (float fontSizePx, const wchar_t * fontFamily, float & outHeightPx);

private:
    HRESULT  SelectGdiFont (float fontSizePx, const wchar_t * fontFamily);

    DxuiTextRenderer  & m_renderer;

    HDC                                            m_gdiDc = nullptr;
    std::map<std::pair<std::wstring, int>, HFONT>  m_gdiFonts;
};