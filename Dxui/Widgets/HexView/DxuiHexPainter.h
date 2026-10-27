#pragma once

#include "Pch.h"

#include "Widgets/HexView/DxuiHexViewTypes.h"

class DxuiHexView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter
//
//  Drawing the rows: each row's bytes read and drawn as hex, values and
//  text in the encoding chosen, a line's row, the cells and their colors.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexPainter
{
public:
    explicit DxuiHexPainter (DxuiHexView & view);

    std::wstring  GetTextFor (uint64_t offset, uint64_t count) const;
    std::wstring  GetHexFor  (uint64_t offset, uint64_t count) const;

    //  One row's bytes and marks, read straight from the source into the
    //  buffers the view reuses frame after frame.
    int   ReadRow (uint64_t row);
    void  PaintRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row, int count);
    void  EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    //  One cell run of the fixed-width face, and the fill behind it.
    static void  DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font);

    //  The addresses' strength as a fraction of the bytes'.
    static constexpr float  s_kAddressStrength  = 0.55f;
    std::wstring  FormatValue           (uint64_t value, int present) const;
    uint32_t      GetByteColor          (const IDxuiTheme & theme, uint8_t mark) const;
    void      PaintLineRow       (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row);
    static void  FillCell (IDxuiTextRenderer & text, const RECT & rect, uint32_t argb);

    static wchar_t  GetHexDigit      (int value);
    wchar_t         GetCharFor       (uint8_t byte) const;
    static wchar_t  GetCodePageChar  (UINT codePage, DWORD flags, uint8_t byte);

    static constexpr UINT     s_kDosCodePage     = 437;
    static constexpr UINT     s_kWindowsCodePage = 1252;
    static constexpr uint8_t  s_kFormFeed        = 0x0C;

private:
    friend class DxuiHexView;

    DxuiHexView  & m_view;

    float                          m_zoom         = 1.0f;
    float                          m_textStrength = 1.0f;
    float                          m_cellFontPx   = 0.0f;   // the font size the cell was measured at, 0 when set by the caller
    std::wstring                   m_cellFontFace;
    DxuiHexViewTypes::MarkColorFn  m_markColor;
    std::vector<uint8_t>           m_rowBytes;
    std::vector<uint8_t>           m_rowMarks;
};
