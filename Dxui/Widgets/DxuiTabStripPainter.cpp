#include "Pch.h"

#include "Widgets/DxuiTabStripPainter.h"
#include "Theme/DxuiTheme.h"
#include "DxuiTabStrip.h"
#include "DxuiTabDragModel.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiAnimation.h"
#include "Core/DxuiSystemSettings.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::DxuiTabStripPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabStripPainter::DxuiTabStripPainter (DxuiTabStrip & strip)
    : m_strip (strip)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintFloatingTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintFloatingTab (IDxuiTextRenderer & text, int index, float widthPx, float heightPx, uint32_t textArgb) const
{
    HRESULT       hr      = S_OK;
    float         iconPx  = m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kIconDip);
    float         labelX  = m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kLabelInsetDip);
    float         labelW  = (std::max) (widthPx - labelX - m_strip.m_scaler.ToPxf (DxuiTabStrip::kLabelPadXDip), 0.0f);
    float         fontPx  = m_strip.m_scaler.ToPxf (DxuiTabStrip::kExplorerLabelFontDip);
    std::wstring  shown;



    BAIL_OUT_IF (index < 0 || index >= (int) m_strip.m_tabs.size(), S_OK);

    if (HasTabIcon (index))
    {
        const DxuiIconImage  & icon = *m_strip.m_tabs[(size_t) index].icon;

        hr = text.DrawIconBitmap (icon.bgraPremul.data(), icon.width, icon.height,
                                  m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kIconInsetDip), (heightPx - iconPx) * 0.5f, iconPx, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    shown = DxuiTextElide::ToWidth (text, m_strip.m_tabs[(size_t) index].label, fontPx, DxuiTabStrip::GetLabelFace(), labelW, DxuiElide::Tail);

    hr = text.DrawString (shown.c_str(),
                          labelX, 0.0f, labelW, heightPx,
                          textArgb,
                          fontPx,
                          DxuiTabStrip::GetLabelFace(),
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::SemiBold,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintArrow
//
//  A triangle, dimmed when there is nothing further to scroll to, with the
//  hover fill only when it can act.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintArrow (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t hoverArgb, uint32_t textArgb) const
{
    constexpr float  s_kArrowFontDip   = 9.0f;
    constexpr float  s_kArrowIdleScale = 0.35f;
    constexpr float  s_kPressedScale   = 0.82f;



    HRESULT   hr      = S_OK;
    int       arrow   = m_strip.GetArrowWidthPx();
    float     left    = (float) ((direction < 0) ? (int) m_strip.m_boundsDip.left : m_strip.GetViewRight());
    float     top     = (float) m_strip.m_boundsDip.top;
    float     height  = (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top);
    bool      enabled = m_strip.CanScroll (direction);
    uint32_t  color   = enabled ? textArgb : DxuiColor::Scale (textArgb, s_kArrowIdleScale);



    if (enabled && m_strip.m_hoverArrow == direction)
    {
        painter.FillRect (left, top, (float) arrow, height,
                          (m_strip.m_pressedArrow == direction) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
    }

    hr = text.DrawString ((direction < 0) ? s_kpszTriangleLeft : s_kpszTriangleRight,
                          left, top, (float) arrow, height,
                          color,
                          m_strip.m_scaler.ToPxf (s_kArrowFontDip),
                          DxuiTheme::GetUiFace(),
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintNewTab (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const
{
    constexpr float  s_kGlyphDip     = 12.0f;
    constexpr float  s_kHoverInset   = 4.0f;
    constexpr float  s_kPressedScale = 0.82f;



    HRESULT  hr    = S_OK;
    RECT     rc    = m_strip.GetNewTabRect();
    float    inset = m_strip.m_scaler.ToPxf (s_kHoverInset);



    //  Explorer's: a rounded box inside the button's span, and the icon font's
    //  thin Add glyph rather than a heavy text plus.
    if (m_strip.m_hoverNewTab)
    {
        painter.FillRoundedRect ((float) rc.left + inset, (float) rc.top + inset,
                                 (float) (rc.right - rc.left) - inset * 2.0f, (float) (rc.bottom - rc.top) - inset * 2.0f,
                                 m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kCornerDip),
                                 m_strip.m_pressedNewTab ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb);
    }

    hr = text.DrawString (s_kpszMdl2Add,
                          (float) rc.left, (float) rc.top, (float) (rc.right - rc.left), (float) (rc.bottom - rc.top),
                          textArgb,
                          m_strip.m_scaler.ToPxf (s_kGlyphDip),
                          DxuiTextRenderer::IsFontFamilyInstalled (L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets",
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::GetHoverFill
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStripPainter::GetHoverFill (int index, uint32_t hoverArgb) const
{
    constexpr float  s_kPressedScale = 0.82f;   // armed-tab tint, a touch darker than hover



    return (index == m_strip.m_pressed && index == m_strip.m_hover) ? DxuiColor::Darken (hoverArgb, s_kPressedScale) : hoverArgb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::GetBesideFill
//
//  The hover is translucent over the strip, so it is mixed onto it here.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTabStripPainter::GetBesideFill (int index, uint32_t stripArgb, uint32_t hoverArgb) const
{
    uint32_t  hover = 0;



    if (index < 0 || index >= (int) m_strip.m_tabs.size() || index != m_strip.m_hover)
    {
        return stripArgb;
    }

    hover = GetHoverFill (index, hoverArgb);

    return DxuiColor::Mix (stripArgb, hover | 0xFF000000u, (float) (hover >> 24) / 255.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::HasTabIcon
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabStripPainter::HasTabIcon (int index) const
{
    const DxuiTabStrip::Tab  & t = m_strip.m_tabs[(size_t) index];



    return t.icon && !t.icon->bgraPremul.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintTabIcon
//
//  Draws tab `index`'s icon, if it has one, near its left edge and centered
//  on its height.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintTabIcon (IDxuiTextRenderer & text, int index) const
{
    HRESULT  hr     = S_OK;
    RECT     r      = m_strip.GetTabScreenRect (index);
    float    iconPx = m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kIconDip);
    float    height = (float) (r.bottom - r.top);



    if (HasTabIcon (index))
    {
        const DxuiIconImage  & icon = *m_strip.m_tabs[(size_t) index].icon;

        hr = text.DrawIconBitmap (icon.bgraPremul.data(), icon.width, icon.height,
                                  (float) r.left + m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kIconInsetDip), (float) r.top + (height - iconPx) * 0.5f, iconPx, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintTabClose
//
//  Draws tab `index`'s close button: the glyph, over a faint wash of
//  `washArgb` while hovered, a stronger one while pressed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintTabClose (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    int                  index,
    uint32_t             glyphArgb,
    uint32_t             washArgb,
    float                cornerPx) const
{
    constexpr uint32_t  kHoverAlpha   = 0x1F000000u;
    constexpr uint32_t  kPressedAlpha = 0x33000000u;



    HRESULT   hr    = S_OK;
    RECT      close = m_strip.GetCloseRect (index);
    uint32_t  alpha = (m_strip.m_pressedClose == index) ? kPressedAlpha : kHoverAlpha;



    if (m_strip.m_hoverClose == index)
    {
        painter.FillRoundedRect ((float) close.left, (float) close.top,
                                 (float) (close.right - close.left), (float) (close.bottom - close.top), cornerPx,
                                 (washArgb & 0x00FFFFFFu) | alpha);
    }

    hr = text.DrawString (s_kpszMdl2ChromeClose,
                          (float) close.left, (float) close.top,
                          (float) (close.right - close.left), (float) (close.bottom - close.top),
                          glyphArgb,
                          m_strip.m_scaler.ToPxf ((m_strip.m_style == DxuiTabStripStyle::Explorer) ? DxuiTabStrip::s_kExplorerCloseGlyphDip : (float) DxuiTabStrip::s_kCloseGlyphDip),
                          m_iconFace,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintStandard
//
//  The Standard style, with caller-supplied colors so the themed and fallback
//  Paint entry points share one body.
//
//  A connected-tab look: the selected tab shares the page background (no
//  chip fill) and is marked by a thick accent underline flush with the page
//  edge; the rest are unfilled with dimmed labels, and a hovered or armed one
//  gets a subtle fill hint. Each label is centered in its tab, inside the
//  label pad; an icon or a close button takes its room from that box.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintStandard (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    uint32_t             hoverArgb,
    uint32_t             underlineArgb,
    uint32_t             textArgb,
    uint32_t             focusArgb) const
{
    constexpr float  kFocusThickDip  = 1.0f;
    constexpr float  kFocusInsetDip  = 1.0f;
    constexpr float  kPadYDip        = 4.0f;
    constexpr float  kPressedScale   = 0.82f;   // armed-tab tint, a touch darker than hover
    constexpr float  kUnderlineDip   = 3.0f;    // thick selected-tab underline
    constexpr float  kMutedTextScale = 0.62f;   // dim unselected labels



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_strip.m_tabs.size();
    float     focusThick = m_strip.m_scaler.ToPxf (kFocusThickDip);
    float     focusInset = m_strip.m_scaler.ToPxf (kFocusInsetDip);
    float     padX       = m_strip.m_scaler.ToPxf (DxuiTabStrip::kLabelPadXDip);
    float     padY       = m_strip.m_scaler.ToPxf (kPadYDip);
    float     fontDip    = m_strip.m_scaler.ToPxf (DxuiTabStrip::kLabelFontDip);
    float     underline  = m_strip.m_scaler.ToPxf (kUnderlineDip);
    float     corner     = m_strip.m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    uint32_t  mutedText  = DxuiColor::Scale (textArgb, kMutedTextScale);
    int       selected   = m_strip.GetPaintedSelection();



    if (m_strip.IsOverflowing())
    {
        hr = text.PushClipRect ((float) m_strip.GetViewLeft(), (float) m_strip.m_boundsDip.top,
                                (float) (m_strip.GetViewRight() - m_strip.GetViewLeft()), (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (i = 0; i < (int) n; ++i)
    {
        const DxuiTabStrip::Tab  & t       = m_strip.m_tabs[(size_t) i];
        RECT                       r       = m_strip.GetTabScreenRect (i);
        float                      width   = (float) (r.right  - r.left);
        float                      height  = (float) (r.bottom - r.top);
        bool                       isSel   = (i == selected);
        bool                       isHover = (i == m_strip.m_hover);
        bool                       isArmed = (i == m_strip.m_pressed && i == m_strip.m_hover);
        float                      labelX  = (float) r.left + padX;
        float                      labelW  = width - padX * 2.0f;

        if (!m_strip.IsPainted (i))
        {
            continue;
        }

        if (!isSel && (isHover || isArmed))
        {
            painter.FillRoundedRect ((float) r.left, (float) r.top, width, height, corner,
                                     isArmed ? DxuiColor::Darken (hoverArgb, kPressedScale) : hoverArgb);
        }

        if (isSel)
        {
            painter.FillRect ((float) r.left, (float) r.bottom - underline, width, underline, underlineArgb);
        }

        if (m_strip.m_focused && m_strip.m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect ((float) r.left + focusInset, (float) r.top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, focusArgb);
        }

        if (HasTabIcon (i))
        {
            PaintTabIcon (text, i);

            labelX = (float) r.left + m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kLabelInsetDip);
            labelW = (std::max) ((float) r.right - padX - labelX, 0.0f);
        }

        if (m_strip.m_close)
        {
            PaintTabClose (painter, text, i, isSel ? textArgb : mutedText, textArgb, corner);

            labelW = (std::max) ((float) m_strip.GetCloseRect (i).left - labelX, 0.0f);
        }

        hr = text.DrawString (t.label.c_str(),
                              labelX,
                              (float) r.top + padY,
                              labelW,
                              height - padY * 2.0f,
                              isSel ? textArgb : mutedText,
                              fontDip,
                              DxuiTabStrip::GetLabelFace(),
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_strip.IsOverflowing())
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);

        PaintArrow (painter, text, -1, hoverArgb, textArgb);
        PaintArrow (painter, text,  1, hoverArgb, textArgb);
    }

    if (m_strip.GetNewTabWidthPx() > 0)
    {
        PaintNewTab (painter, text, hoverArgb, textArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintExplorer
//
//  The Explorer style, with caller-supplied colors so the themed and
//  fallback Paint entry points share one body.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintExplorer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    uint32_t             stripArgb,
    uint32_t             hoverArgb,
    uint32_t             fillArgb,
    uint32_t             dividerArgb,
    uint32_t             textArgb,
    uint32_t             focusArgb) const
{
    constexpr float  s_kFocusThickDip  = 1.0f;
    constexpr float  s_kFocusInsetDip  = 1.0f;
    constexpr float  s_kDividerDip     = 16.0f;
    constexpr float  s_kMutedTextScale = 0.8f;    // Explorer's #CCCCCC on an unselected tab
    constexpr float  s_kBaseLineDip    = 1.0f;    // Explorer's 2 px at 150%
    constexpr float  s_kBaseLineScale  = 0.9f;    // Explorer's #1D1D1D under #202020



    HRESULT   hr         = S_OK;
    int       i          = 0;
    size_t    n          = m_strip.m_tabs.size();
    float     focusThick = m_strip.m_scaler.ToPxf (s_kFocusThickDip);
    float     focusInset = m_strip.m_scaler.ToPxf (s_kFocusInsetDip);
    float     padX       = m_strip.m_scaler.ToPxf (DxuiTabStrip::kLabelPadXDip);
    float     fontDip    = m_strip.m_scaler.ToPxf (DxuiTabStrip::kExplorerLabelFontDip);
    float     corner     = m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kCornerDip);
    float     dividerH   = m_strip.m_scaler.ToPxf (s_kDividerDip);
    float     baseLine   = (std::max) (1.0f, std::round (m_strip.m_scaler.ToPxf (s_kBaseLineDip)));
    uint32_t  mutedText  = DxuiColor::Scale (textArgb, s_kMutedTextScale);
    int       selected   = m_strip.GetPaintedSelection();



    //  File Explorer's tabs: the selected one is filled with the
    //  row below and joins it, rounded at the top and square at the bottom;
    //  the rest are unfilled, split by short dividers, with dimmer labels.
    if (m_strip.HasBounds())
    {
        hr = text.PushClipRect ((float) m_strip.GetViewLeft(), (float) m_strip.m_boundsDip.top,
                                (float) (m_strip.GetViewRight() - m_strip.GetViewLeft()), (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  Explorer's strip ends in a darker line the selected tab breaks, and
    //  which a hovered tab stops short of.
    //  It is black laid over the strip, which darkens an opaque strip by the
    //  same amount and darkens a backdrop showing through an empty one too,
    //  and it stops at the selected tab, whose translucent fill would show it.
    if (m_strip.HasBounds())
    {
        float     lineTop   = (float) m_strip.m_boundsDip.bottom - baseLine;
        float     viewLeft  = (float) m_strip.GetViewLeft();
        float     viewRight = (float) m_strip.GetViewRight();
        uint32_t  lineArgb  = (uint32_t) std::lround ((1.0f - s_kBaseLineScale) * 255.0f) << 24;

        if (selected >= 0 && selected < (int) n)
        {
            RECT  sel = m_strip.GetTabScreenRect (selected);

            painter.FillRect (viewLeft,         lineTop, (std::max) (0.0f, (float) sel.left - viewLeft),   baseLine, lineArgb);
            painter.FillRect ((float) sel.right, lineTop, (std::max) (0.0f, viewRight - (float) sel.right), baseLine, lineArgb);
        }
        else
        {
            painter.FillRect (viewLeft, lineTop, viewRight - viewLeft, baseLine, lineArgb);
        }
    }

    //  Fills first, so no fill covers another tab's label or close button;
    //  the selected tab last.
    for (i = 0; i < (int) n; ++i)
    {
        RECT  r = m_strip.GetTabScreenRect (i);

        //  Explorer's hovered tab: rounded at the top, square where it meets
        //  the row below, with a 1 px edge halfway between it and the strip
        //  along its top and sides.
        if (i != selected && i == m_strip.m_hover && m_strip.IsPainted (i))
        {
            float     left   = (float) r.left;
            float     top    = (float) r.top;
            float     width  = (float) (r.right - r.left);
            float     height = (float) (r.bottom - r.top) - baseLine;
            uint32_t  fill   = GetBesideFill (i, stripArgb, fillArgb);
            uint32_t  edge   = DxuiColor::Mix (stripArgb, fill, 0.5f);

            painter.FillRoundedRect (left, top, width, height, corner, edge);
            painter.FillRect        (left, top + height - corner, width, corner, edge);
            painter.FillRoundedRect (left + 1.0f, top + 1.0f, width - 2.0f, height - 1.0f, (std::max) (corner - 1.0f, 0.0f), fill);
            painter.FillRect        (left + 1.0f, top + height - corner, width - 2.0f, corner, fill);
        }
    }

    if (selected >= 0 && selected < (int) n)
    {
        RECT   r      = m_strip.GetTabScreenRect (selected);
        float  left   = (float) r.left;
        float  top    = (float) r.top;
        float  width  = (float) (r.right - r.left);
        float  height = (float) (r.bottom - r.top);
        float  bottom = top + height;

        //  The rounded top is clipped short of the square bottom, so a
        //  translucent fill is laid down once everywhere.
        painter.PushClipRect    (left, top, width, height - corner);
        painter.FillRoundedRect (left, top, width, height, corner, fillArgb);
        painter.PopClipRect();
        painter.FillRect        (left, bottom - corner, width, corner, fillArgb);
    }

    for (i = 0; i < (int) n; ++i)
    {
        const DxuiTabStrip::Tab  & t       = m_strip.m_tabs[(size_t) i];
        RECT                       r       = m_strip.GetTabScreenRect (i);
        float                      left    = (float) r.left;
        float                      top     = (float) r.top;
        float                      width   = (float) (r.right - r.left);
        float                      height  = (float) (r.bottom - r.top);
        float                      bottom  = top + height;
        bool                       isSel   = (i == selected);
        bool                       isHover = (i == m_strip.m_hover);
        bool                       isArmed = (i == m_strip.m_pressed && i == m_strip.m_hover);
        bool                       nextLit = (i + 1 == selected) || (i + 1 == m_strip.m_hover);
        float                      labelX  = left + m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kLabelInsetDip);
        float                      labelR  = (float) r.right - padX;
        std::wstring               shown   = ;

        if (!m_strip.IsPainted (i))
        {
            continue;
        }

        if (!isSel && !isHover && !isArmed && !nextLit && i + 1 < (int) n)
        {
            painter.FillRect (left + width - 1.0f, top + (height - dividerH) * 0.5f, 1.0f, dividerH, dividerArgb);
        }

        if (m_strip.m_focused && m_strip.m_focusCueVisible && isSel)
        {
            painter.OutlineRoundedRect (left + focusInset, top + focusInset,
                                        width - focusInset * 2.0f, height - focusInset * 2.0f,
                                        corner, focusThick, focusArgb);
        }

        PaintTabIcon (text, i);

        if (m_strip.m_close)
        {
            PaintTabClose (painter, text, i, isSel ? textArgb : mutedText, textArgb, corner);

            labelR = (float) m_strip.GetCloseRect (i).left;
        }

        //  A label wider than its room is cut off with an ellipsis, never wrapped.
        shown = DxuiTextElide::ToWidth (text, t.label, fontDip, DxuiTabStrip::GetLabelFace(), (std::max) (labelR - labelX, 0.0f), DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(),
                              labelX, top, (std::max) (labelR - labelX, 0.0f), height,
                              isSel ? textArgb : mutedText,
                              fontDip,
                              DxuiTabStrip::GetLabelFace(),
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              isSel ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_strip.HasBounds())
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_strip.GetArrowWidthPx() > 0)
    {
        PaintArrow (painter, text, -1, hoverArgb, textArgb);
        PaintArrow (painter, text,  1, hoverArgb, textArgb);
    }

    if (m_strip.GetNewTabWidthPx() > 0)
    {
        PaintNewTab (painter, text, hoverArgb, textArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintExplorerThemed
//
//  The Explorer style in the theme's colors, with the host's strip and
//  selected fills where it gives them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintExplorerThemed (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    uint32_t  fill  = (m_selectedFill != 0) ? m_selectedFill : theme.BackgroundElevated();
    uint32_t  strip = (m_stripFill    != 0) ? m_stripFill    : theme.Background();
    uint32_t  hover = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;



    //  The strip's own fill, when the host gives one, goes down first.
    if (m_stripFill != 0 && m_strip.HasBounds())
    {
        painter.FillRect ((float) m_strip.m_boundsDip.left, (float) m_strip.m_boundsDip.top,
                          (float) (m_strip.m_boundsDip.right - m_strip.m_boundsDip.left), (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top), m_stripFill);
    }

    //  The selected tab takes the color of the row it joins, which the host
    //  gives; without one it takes the elevated surface. A hovered tab is a
    //  faint wash of the text color, which reads in either theme, as Explorer's
    //  gray does, where the theme's hover is a saturated selection color.
    PaintExplorer (painter, text,
                   strip,
                   hover,
                   fill,
                   theme.Divider(),
                   theme.Foreground(),
                   theme.FocusRing());

    //  Explorer's light theme lines the strip's bottom, except under the
    //  selected tab, which joins the row below.
    if (theme.TabStripEdge() != 0 && m_strip.HasBounds())
    {
        float  line   = (std::max) (1.0f, m_strip.m_scaler.ToPxf (1.0f));
        float  bottom = (float) m_strip.m_boundsDip.bottom - line;
        float  left   = (float) m_strip.m_boundsDip.left;
        float  right  = (float) m_strip.m_boundsDip.right;
        int    sel    = m_strip.GetPaintedSelection();
        RECT   tab    = (sel >= 0 && sel < (int) m_strip.m_tabs.size()) ? m_strip.GetTabScreenRect (sel) : RECT {};

        if (tab.right > tab.left)
        {
            painter.FillRect (left, bottom, (std::max) (0.0f, (float) tab.left - left), line, theme.TabStripEdge());
            painter.FillRect ((float) tab.right, bottom, (std::max) (0.0f, right - (float) tab.right), line, theme.TabStripEdge());
        }
        else
        {
            painter.FillRect (left, bottom, right - left, line, theme.TabStripEdge());
        }
    }
}
