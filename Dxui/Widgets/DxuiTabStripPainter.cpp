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
#include "Core/DxuiPaneMetrics.h"





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
        painter.PushClip    (left, top, width, height - corner);
        painter.FillRoundedRect (left, top, width, height, corner, fillArgb);
        painter.PopClip();
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
        std::wstring               shown;

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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintCompactThemed
//
//  The selected tab takes the color of the pane it opens into, which the
//  host sets; without one it takes the content color. A hovered tab is a
//  faint wash of the text color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintCompactThemed (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    DxuiTabStripPalette  pal;



    //  The strip's own fill, when the host gives one, goes down first.
    if (m_stripFill != 0 && m_strip.HasBounds())
    {
        painter.FillRect ((float) m_strip.m_boundsDip.left, (float) m_strip.m_boundsDip.top,
                          (float) (m_strip.m_boundsDip.right - m_strip.m_boundsDip.left), (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top), m_stripFill);
    }

    pal.hover        = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
    pal.fill         = (m_selectedFill != 0) ? m_selectedFill : theme.ContentBackground();
    pal.text         = theme.Foreground();
    pal.tabHover     = DxuiTabStrip::GetTabHoverFill (theme);
    pal.label        = DxuiTabStrip::GetLabelInk (theme, false);
    pal.labelFocused = DxuiTabStrip::GetLabelInk (theme, true);
    pal.labelHovered = DxuiTabStrip::GetHoveredLabelInk (theme);
    pal.glyph        = DxuiTabStrip::GetGlyphInk (theme, false);
    pal.glyphFocused = DxuiTabStrip::GetGlyphInk (theme, true);
    pal.glyphHovered = DxuiTabStrip::GetHoveredGlyphInk (theme);

    PaintCompact (painter, text, pal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintCompact
//
//  The tabs' fills are clipped to the part of the strip between the arrows,
//  extended one line past the strip's edge along the pane, below a
//  document's tabs and above a tool window's, so the selected tab can fill
//  the line it opens into. The selected tab's body and label are painted
//  last, over its neighbors' washes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintCompact (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiTabStripPalette & pal) const
{
    HRESULT  hr        = S_OK;
    int      n         = (int) m_strip.m_tabs.size();
    int      selected  = m_strip.GetPaintedSelection();
    long     line      = DxuiPaneMetrics::GetLinePx (m_strip.m_scaler);
    long     lineAbove = (m_strip.m_style == DxuiTabStripStyle::ToolWindow) ? line : 0;
    bool     bounded   = m_strip.HasBounds();
    float    viewLeft  = (float) m_strip.GetViewLeft();
    float    viewWidth = (float) (m_strip.GetViewRight() - m_strip.GetViewLeft());
    float    top       = (float) m_strip.m_boundsDip.top;
    float    height    = (float) (m_strip.m_boundsDip.bottom - m_strip.m_boundsDip.top);



    if (bounded)
    {
        hr = text.PushClipRect (viewLeft, top, viewWidth, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.PushClip (viewLeft, top - (float) lineAbove, viewWidth, height + (float) line);
    }

    for (int i = 0; i < n; i++)
    {
        if (i != selected && m_strip.IsPainted (i))
        {
            PaintCompactTab (painter, text, i, pal);
        }
    }

    if (selected >= 0 && selected < n)
    {
        PaintCompactTab (painter, text, selected, pal);
    }

    if (bounded)
    {
        painter.PopClip();

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_strip.GetArrowWidthPx() > 0)
    {
        PaintCompactArrow (painter, text, -1, pal.tabHover, pal.text);
        PaintCompactArrow (painter, text,  1, pal.tabHover, pal.text);
    }

    if (m_strip.GetNewTabWidthPx() > 0)
    {
        PaintCompactNewTab (painter, text, pal.tabHover, pal.text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintCompactTab
//
//  Visual Studio's tabs. The selected tab is filled with its pane's color,
//  rounded at its far corners and open into the pane; the pane's frame draws
//  its outline. The one under the pointer is a faint box the band's depth,
//  its label a step brighter. The rest are plain text on the band.
//
//  Each label starts where a pane's title does, at the pane's text inset
//  from the tab's left edge in whole pixels, so the first tab's label lines
//  up with a title to the pixel. A document tab's label runs to the pin's
//  square, which every document tab keeps whether or not its pin shows, and
//  a tool window tab's to the line at its end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintCompactTab (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, const DxuiTabStripPalette & pal) const
{
    const DxuiTabStrip::Tab  & tab     = m_strip.m_tabs[(size_t) index];
    const DxuiDpiScaler      & scaler  = m_strip.m_scaler;
    RECT                       r       = m_strip.GetTabScreenRect (index);
    RECT                       pin     = m_strip.GetTabButtonRect (index, DxuiTabStrip::TabButton::Pin);
    bool                       isDoc   = m_strip.m_style == DxuiTabStripStyle::Document;
    long                       labelR  = isDoc ? pin.left : r.right - DxuiPaneMetrics::GetLinePx (scaler);
    float                      top     = (float) r.top;
    float                      height  = (float) (r.bottom - r.top);
    float                      fontPx  = scaler.ToPxf ((float) DxuiTabStrip::s_kCompactFontDip);
    float                      markPx  = (float) scaler.ToPx (DxuiTabStrip::s_kCompactMarkDip);
    float                      labelX  = (float) (r.left + DxuiPaneMetrics::GetTextInsetPx (scaler));
    float                      room    = 0.0f;
    bool                       isSel   = index == m_strip.m_selected;
    bool                       isHover = index == m_strip.m_hover;
    bool                       isArmed = index == m_strip.m_pressed && index == m_strip.m_hover;
    uint32_t                   ink     = (isSel && m_strip.m_focusedLook) ? pal.labelFocused : isHover ? pal.labelHovered : pal.label;
    std::wstring               shown;
    HRESULT                    hr      = S_OK;



    if (isSel)
    {
        PaintSelectedBody (painter, r, pal.fill);
    }
    else if (isHover)
    {
        PaintHoverBox (painter, r, isArmed ? DxuiColor::Darken (pal.tabHover, DxuiTabStrip::kPressedScale) : pal.tabHover);
    }

    //  A mark with no face given is drawn in the system face, which no theme
    //  replaces: a theme's body face may have no glyph for it, and the
    //  fallback face's glyph is placed off the label's line.
    if (!tab.mark.empty() && tab.markArgb != 0)
    {
        hr = text.DrawString (tab.mark.c_str(), labelX, top, markPx, height,
                              tab.markArgb, fontPx, tab.markFace.empty() ? DxuiTheme::kSystemFace : tab.markFace.c_str(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!tab.mark.empty())
    {
        labelX += markPx;
    }

    if (m_strip.IsPinShown (index))
    {
        PaintTabButton (painter, text, index, true, pal);
    }

    if (m_strip.IsCloseShown (index))
    {
        PaintTabButton (painter, text, index, false, pal);
    }

    room  = (std::max) ((float) labelR - labelX, 0.0f);
    shown = DxuiTextElide::ToWidth (text, tab.label, fontPx, DxuiTabStrip::GetLabelFace(), room, DxuiElide::Tail);

    hr = text.DrawString (shown.c_str(), labelX, top, room, height,
                          ink, fontPx, DxuiTabStrip::GetLabelFace(),
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintTabButton
//
//  A tab's pin or close button: its glyph centered in its square, in the
//  full foreground on the selected tab of the pane in use and a shade under
//  the tab's label otherwise, washed in a rounded square 3 DIP in from the
//  square's edges while the pointer is on it, as a title bar's button is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintTabButton (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, bool isPin, const DxuiTabStripPalette & pal) const
{
    static constexpr int  kWashInsetDip = 3;
    RECT                  r             = m_strip.GetTabButtonRect (index, isPin ? DxuiTabStrip::TabButton::Pin : DxuiTabStrip::TabButton::Close);
    bool                  hovered       = isPin ? m_strip.m_hoverPin   == index : m_strip.m_hoverClose   == index;
    bool                  pressed       = isPin ? m_strip.m_pressedPin == index : m_strip.m_pressedClose == index;
    bool                  isFocused     = index == m_strip.m_selected && m_strip.m_focusedLook;
    float                 inset         = (float) m_strip.m_scaler.ToPx (kWashInsetDip);
    float                 corner        = (float) m_strip.m_scaler.ToPx (DxuiTabStrip::s_kCompactCornerDip);
    float                 width         = (float) (r.right - r.left);
    float                 height        = (float) (r.bottom - r.top);
    uint32_t              ink           = isFocused ? pal.glyphFocused : (index == m_strip.m_hover) ? pal.glyphHovered : pal.glyph;
    HRESULT               hr            = S_OK;



    if (hovered)
    {
        painter.FillRoundedRect ((float) r.left + inset, (float) r.top + inset, width - 2.0f * inset, height - 2.0f * inset, corner,
                                 pressed ? DxuiColor::Darken (pal.hover, DxuiTabStrip::kPressedScale) : pal.hover);
    }

    hr = text.DrawString (isPin ? s_kpszMdl2Pinned : s_kpszMdl2Cancel, (float) r.left, (float) r.top, width, height,
                          ink, m_strip.m_scaler.ToPxf (isPin ? DxuiTabStrip::kPinGlyphDip : DxuiTabStrip::kCloseGlyphDip), m_iconFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintSelectedBody
//
//  The selected tab's fill, in the color of the pane it opens into, across
//  the span GetSelectedSpan gives: rounded at the pane's outer radius at its
//  two far corners, square along the edge it shares with the pane, and
//  extended one line past that edge so it fills the line it opens into.
//  Nothing is drawn outside that span and that line; the pane's frame draws
//  the outline and the joins. In a pane too small to round, it is square.
//
//  A side a scroll arrow cuts off is square to the arrow, as the frame's
//  outline runs straight on to it there: the fill reaches a radius past the
//  cut, so its rounded corner falls outside what shows.
//
//  A document's tabs sit above their pane, a tool window's below it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintSelectedBody (IDxuiPainter & painter, const RECT & tab, uint32_t fillArgb) const
{
    long   t         = DxuiPaneMetrics::GetLinePx (m_strip.m_scaler);
    long   ro        = m_strip.GetCornerPx();
    bool   below     = m_strip.m_style == DxuiTabStripStyle::ToolWindow;
    long   left      = tab.left;
    long   right     = tab.right;
    bool   openLeft  = false;
    bool   openRight = false;
    bool   shown     = m_strip.GetSelectedSpan (left, right, openLeft, openRight);
    long   fillLeft  = 0;
    long   fillRight = 0;
    RECT   clip      = {};
    float  depth     = (float) (tab.bottom - tab.top + t + ro);
    float  top       = below ? (float) (tab.top - t - ro) : (float) tab.top;



    if (!shown)
    {
        return;
    }

    fillLeft  = openLeft  ? left  - ro : left;
    fillRight = openRight ? right + ro : right;
    clip      = below ? RECT { left, tab.top - t, right, tab.bottom } : RECT { left, tab.top, right, tab.bottom + t };

    painter.PushClip ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top));

    if (ro <= 0 || fillRight - fillLeft < 2 * ro)
    {
        painter.FillRect ((float) clip.left, (float) clip.top, (float) (clip.right - clip.left), (float) (clip.bottom - clip.top), fillArgb);
    }
    else
    {
        painter.FillRoundedRect ((float) fillLeft, top, (float) (fillRight - fillLeft), depth, (float) ro, fillArgb);
    }

    painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintHoverBox
//
//  The tab under the pointer, as Visual Studio draws it: a translucent box
//  as wide as the tab and the band's full depth, rounded at the pane's outer
//  radius at its two far corners and square along the line, with no
//  outline. In a pane too small to round, it is square.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintHoverBox (IDxuiPainter & painter, const RECT & tab, uint32_t argb) const
{
    long   ro     = m_strip.GetCornerPx();
    float  left   = (float) tab.left;
    float  width  = (float) (tab.right - tab.left);
    float  height = (float) (tab.bottom - tab.top);
    float  top    = (m_strip.m_style == DxuiTabStripStyle::ToolWindow) ? (float) (tab.top - ro) : (float) tab.top;



    painter.PushClip (left, (float) tab.top, width, height);

    if (ro <= 0 || tab.right - tab.left < 2 * ro)
    {
        painter.FillRect (left, (float) tab.top, width, height, argb);
    }
    else
    {
        painter.FillRoundedRect (left, top, width, height + (float) ro, (float) ro, argb);
    }

    painter.PopClip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintHoverPill
//
//  The wash under the pointer on a scroll arrow or the + in the compact
//  styles: as wide as what it washes, standing in from the strip's edges by
//  the hover inset, with rounded corners.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintHoverPill (IDxuiPainter & painter, const RECT & rect, uint32_t argb) const
{
    float  inset  = (float) DxuiTabStrip::GetHoverInsetPx (m_strip.m_scaler);
    float  corner = (float) m_strip.m_scaler.ToPx (DxuiTabStrip::s_kCompactCornerDip);
    float  height = (float) (rect.bottom - rect.top) - 2.0f * inset;



    painter.FillRoundedRect ((float) rect.left, (float) rect.top + inset, (float) (rect.right - rect.left), (std::max) (0.0f, height), corner, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintCompactArrow
//
//  As PaintArrow, with the wash a pill standing in from the strip's edges.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintCompactArrow (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t washArgb, uint32_t textArgb) const
{
    constexpr float  kArrowFontDip   = 9.0f;
    constexpr float  kArrowIdleScale = 0.35f;
    HRESULT          hr              = S_OK;
    int              arrow           = m_strip.GetArrowWidthPx();
    long             left            = (direction < 0) ? m_strip.m_boundsDip.left : (long) m_strip.GetViewRight();
    bool             enabled         = m_strip.CanScroll (direction);
    uint32_t         color           = enabled ? textArgb : DxuiColor::Scale (textArgb, kArrowIdleScale);
    uint32_t         wash            = (m_strip.m_pressedArrow == direction) ? DxuiColor::Darken (washArgb, DxuiTabStrip::kPressedScale) : washArgb;
    RECT             area            = { left, m_strip.m_boundsDip.top, left + arrow, m_strip.m_boundsDip.bottom };



    if (enabled && m_strip.m_hoverArrow == direction)
    {
        PaintHoverPill (painter, area, wash);
    }

    hr = text.DrawString ((direction < 0) ? s_kpszTriangleLeft : s_kpszTriangleRight,
                          (float) area.left, (float) area.top, (float) arrow, (float) (area.bottom - area.top),
                          color,
                          m_strip.m_scaler.ToPxf (kArrowFontDip),
                          DxuiTheme::GetUiFace(),
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripPainter::PaintCompactNewTab
//
//  The + set in the tabs' font and size, so it sits on their centerline
//  rather than a larger glyph's.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabStripPainter::PaintCompactNewTab (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t washArgb, uint32_t textArgb) const
{
    HRESULT   hr   = S_OK;
    RECT      rc   = m_strip.GetNewTabRect();
    uint32_t  wash = m_strip.m_pressedNewTab ? DxuiColor::Darken (washArgb, DxuiTabStrip::kPressedScale) : washArgb;



    if (m_strip.m_hoverNewTab)
    {
        PaintHoverPill (painter, rc, wash);
    }

    hr = text.DrawString (L"+",
                          (float) rc.left, (float) rc.top, (float) (rc.right - rc.left), (float) (rc.bottom - rc.top),
                          textArgb,
                          m_strip.m_scaler.ToPxf ((float) DxuiTabStrip::s_kCompactFontDip),
                          DxuiTabStrip::GetLabelFace(),
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
