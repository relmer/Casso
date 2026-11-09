#pragma once

#include "Pch.h"

class DxuiTabStrip;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPalette
//
//  The colors one compact paint uses: `fill` for the selected tab, `hover`
//  for a button's wash and `text` for the arrows and the +; `tabHover`
//  under the pointer, on a tab, an arrow or the +; and each label and glyph
//  ink, the `Focused` ones for the selected tab of the pane the user works
//  in and the `Hovered` ones for the tab under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTabStripPalette
{
    uint32_t  hover        = 0;
    uint32_t  fill         = 0;
    uint32_t  text         = 0;
    uint32_t  tabHover     = 0;
    uint32_t  label        = 0;
    uint32_t  labelFocused = 0;
    uint32_t  labelHovered = 0;
    uint32_t  glyph        = 0;
    uint32_t  glyphFocused = 0;
    uint32_t  glyphHovered = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter
//
//  A tab strip's paint, in its standard style or File Explorer's: the
//  tabs with their icons and close buttons, the scroll arrows, the +
//  button, and a tab drawn alone for a drag.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStripPainter
{
public:
    explicit DxuiTabStripPainter (DxuiTabStrip & strip);

    void  SetSelectedFill (uint32_t argb)        { m_selectedFill = argb; }
    void  SetIconFace     (const wchar_t * face) { m_iconFace     = face; }
    void  SetStripFill    (uint32_t argb)        { m_stripFill    = argb; }

    void  PaintFloatingTab    (IDxuiTextRenderer & text, int index, float widthPx, float heightPx, uint32_t textArgb) const;
    void  PaintStandard       (IDxuiPainter & painter, IDxuiTextRenderer & text,
                               uint32_t hoverArgb, uint32_t underlineArgb,
                               uint32_t textArgb, uint32_t focusArgb) const;
    void  PaintExplorer       (IDxuiPainter & painter, IDxuiTextRenderer & text,
                               uint32_t stripArgb, uint32_t hoverArgb, uint32_t fillArgb, uint32_t dividerArgb,
                               uint32_t textArgb, uint32_t focusArgb) const;
    void  PaintExplorerThemed (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    //  The document and tool-window styles, in a palette or in a theme's colors.
    void  PaintCompact        (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiTabStripPalette & pal) const;
    void  PaintCompactThemed  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

private:
    void  PaintArrow  (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t hoverArgb, uint32_t textArgb) const;
    void  PaintNewTab (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const;

    //  A tab's hover fill, darker while pressed; and what a tab shows: the
    //  strip, or its hover mixed onto the strip.
    uint32_t  GetHoverFill  (int index, uint32_t hoverArgb) const;
    uint32_t  GetBesideFill (int index, uint32_t stripArgb, uint32_t hoverArgb) const;
    bool      HasTabIcon    (int index) const;
    void      PaintTabIcon  (IDxuiTextRenderer & text, int index) const;
    void      PaintTabClose (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, uint32_t glyphArgb, uint32_t washArgb, float cornerPx) const;

    void  PaintCompactTab     (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, const DxuiTabStripPalette & pal) const;
    void  PaintTabButton      (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, bool isPin, const DxuiTabStripPalette & pal) const;
    void  PaintSelectedBody   (IDxuiPainter & painter, const RECT & tab, uint32_t fillArgb) const;
    void  PaintHoverBox       (IDxuiPainter & painter, const RECT & tab, uint32_t argb) const;
    void  PaintHoverPill      (IDxuiPainter & painter, const RECT & rect, uint32_t argb) const;
    void  PaintCompactArrow   (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t washArgb, uint32_t textArgb) const;
    void  PaintCompactNewTab  (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t washArgb, uint32_t textArgb) const;

    DxuiTabStrip  & m_strip;

    uint32_t         m_selectedFill = 0;
    uint32_t         m_stripFill    = 0;
    const wchar_t *  m_iconFace     = L"Segoe MDL2 Assets";
};