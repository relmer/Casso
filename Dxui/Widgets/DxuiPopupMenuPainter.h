#pragma once

#include "Pch.h"

#include "Widgets/DxuiPopupMenuTypes.h"

class DxuiPopupMenu;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter
//
//  A popup menu's paint: its card, rows, icon row and mnemonic
//  underlines, in the colors its theme or its owner sets.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPopupMenuPainter
{
public:
    explicit DxuiPopupMenuPainter (DxuiPopupMenu & menu);

    DxuiPopupMenuPalette  ResolvePalette () const;
    void                  PaintBody      (IDxuiPainter & painter, IDxuiTextRenderer & text, int originLeft, int originTop) const;

private:
    void  PaintRow       (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiPopupMenuPalette & pal,
                          int index, float left, float top, float width, float fontDip) const;
    void  PaintIconRow   (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiPopupMenuPalette & pal,
                          int index, float left, float top) const;
    void  PaintUnderline (IDxuiPainter & painter, IDxuiTextRenderer & text, const std::wstring & stripped,
                          int mnIdx, float labelX, float labelY, float fontDip, uint32_t ink) const;

    DxuiPopupMenu  & m_menu;
};