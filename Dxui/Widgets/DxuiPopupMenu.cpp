#include "Pch.h"

#include "Widgets/DxuiPopupMenu.h"

#include "UnicodeSymbols.h"


namespace
{
    constexpr int    s_kItemHeightDip    = 26;
    constexpr int    s_kItemPadLeftDip   = 28;
    constexpr int    s_kItemPadRightDip  = 16;
    constexpr int    s_kCheckGlyphOffDip = 10;
    constexpr int    s_kBorderDip        =  1;
    constexpr int    s_kMinWidthDip      = 140;
    constexpr float  s_kFontDip          = 13.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Sizes the popup to its content (longest label, item count, plus
//  padding for the check glyph). Clamps the popup rect to the host
//  client rectangle so it never paints off-screen.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Show (
    int                   anchorX,
    int                   anchorY,
    std::vector<Item>     items,
    IDxuiTextRenderer & text,
    const RECT          & hostClient)
{
    HRESULT  hr        = S_OK;
    float    fontDip   = (float) m_scaler.Pxf (s_kFontDip);
    int      itemH     = m_scaler.Px (s_kItemHeightDip);
    int      padL      = m_scaler.Px (s_kItemPadLeftDip);
    int      padR      = m_scaler.Px (s_kItemPadRightDip);
    int      border    = m_scaler.Px (s_kBorderDip);
    int      minW      = m_scaler.Px (s_kMinWidthDip);
    int      widestPx  = 0;
    int      width     = 0;
    int      height    = 0;
    int      left      = anchorX;
    int      top       = anchorY;


    m_items   = std::move (items);
    m_hover   = -1;
    m_pressed = -1;
    m_visible = true;

    for (const auto & it : m_items)
    {
        float  w = 0.0f;
        float  h = 0.0f;
        hr = text.MeasureString (it.label.c_str(), fontDip, L"Segoe UI", w, h);
        if (FAILED (hr))
        {
            w = (float) (it.label.size() * 8);
        }
        int wpx = (int) std::ceil (w);
        if (wpx > widestPx) { widestPx = wpx; }
    }

    width  = padL + widestPx + padR;
    if (width < minW) { width = minW; }
    height = (int) m_items.size() * itemH + 2 * border;

    if (left + width  > hostClient.right)  { left = hostClient.right  - width;  }
    if (top  + height > hostClient.bottom) { top  = hostClient.bottom - height; }
    if (left < hostClient.left) { left = hostClient.left; }
    if (top  < hostClient.top)  { top  = hostClient.top;  }

    m_rect.left   = left;
    m_rect.top    = top;
    m_rect.right  = left + width;
    m_rect.bottom = top  + height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Hide()
{
    m_visible = false;
    m_hover   = -1;
    m_pressed = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::HitTest (int x, int y) const
{
    return m_visible
        && x >= m_rect.left && x < m_rect.right
        && y >= m_rect.top  && y < m_rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::HitTestIndex (int x, int y) const
{
    int  border = m_scaler.Px (s_kBorderDip);
    int  itemH  = m_scaler.Px (s_kItemHeightDip);
    int  relY   = y - (m_rect.top + border);

    if (!HitTest (x, y))   { return -1; }
    if (itemH <= 0)        { return -1; }
    if (relY < 0)          { return -1; }

    int  idx = relY / itemH;
    if (idx < 0 || idx >= (int) m_items.size()) { return -1; }
    return idx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnMouseMove (int x, int y)
{
    if (m_visible)
    {
        m_hover = HitTestIndex (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnLButtonDown (int x, int y)
{
    if (!m_visible) { return false; }

    if (!HitTest (x, y))
    {
        Hide();
        return true;
    }

    m_pressed = HitTestIndex (x, y);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnLButtonUp (int x, int y)
{
    int       idx     = HitTestIndex (x, y);
    SelectFn  cb      = m_onSelect;
    int       commit  = -1;


    if (!m_visible) { return false; }

    if (idx >= 0 && idx == m_pressed)
    {
        commit = idx;
    }
    m_pressed = -1;

    if (commit >= 0)
    {
        Hide();
        if (cb) { cb (commit); }
        return true;
    }

    if (!HitTest (x, y))
    {
        Hide();
    }
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (WPARAM vk)
{
    if (!m_visible) { return false; }

    if (vk == VK_ESCAPE)
    {
        Hide();
        return true;
    }

    if (vk == VK_DOWN)
    {
        if (m_items.empty()) { return true; }
        m_hover = (m_hover + 1) % (int) m_items.size();
        return true;
    }

    if (vk == VK_UP)
    {
        if (m_items.empty()) { return true; }
        m_hover = (m_hover <= 0) ? (int) m_items.size() - 1 : m_hover - 1;
        return true;
    }

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        if (m_hover >= 0 && m_hover < (int) m_items.size())
        {
            SelectFn cb     = m_onSelect;
            int      commit = m_hover;
            Hide();
            if (cb) { cb (commit); }
        }
        return true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT   hr       = S_OK;
    int       border   = m_scaler.Px (s_kBorderDip);
    int       itemH    = m_scaler.Px (s_kItemHeightDip);
    int       padL     = m_scaler.Px (s_kItemPadLeftDip);
    int       padR     = m_scaler.Px (s_kItemPadRightDip);
    int       glyphX   = m_scaler.Px (s_kCheckGlyphOffDip);
    float     fontDip  = (float) m_scaler.Pxf (s_kFontDip);
    uint32_t  bgArgb   = 0;
    uint32_t  bgHover  = 0;
    uint32_t  fgArgb   = 0;
    uint32_t  brdrArgb = 0;
    float     left     = (float) m_rect.left;
    float     top      = (float) m_rect.top;
    float     width    = (float) (m_rect.right  - m_rect.left);
    float     height   = (float) (m_rect.bottom - m_rect.top);


    if (!m_visible || m_theme == nullptr)
    {
        return;
    }

    bgArgb   = m_theme->BackgroundElevated();
    bgHover  = m_theme->HoverBackground();
    fgArgb   = m_theme->Foreground();
    brdrArgb = (fgArgb & 0x00FFFFFFu) | 0x60000000u;

    painter.FillRect (left, top, width, height, bgArgb);
    painter.FillRect (left, top,                       width,           (float) border, brdrArgb);
    painter.FillRect (left, top + height - (float) border, width,        (float) border, brdrArgb);
    painter.FillRect (left,                       top, (float) border, height,         brdrArgb);
    painter.FillRect (left + width - (float) border, top, (float) border, height,        brdrArgb);

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        float  iy = top + (float) border + (float) (int) i * (float) itemH;

        if ((int) i == m_hover)
        {
            painter.FillRect (left + (float) border,
                              iy,
                              width  - 2.0f * (float) border,
                              (float) itemH,
                              bgHover);
        }

        if (m_items[i].checked)
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (s_kpszCheckMark,
                                                      left + (float) glyphX,
                                                      iy,
                                                      (float) (padL - glyphX),
                                                      (float) itemH,
                                                      fgArgb, fontDip, L"Segoe UI",
                                                      DxuiTextHAlign::Left,
                                                      DxuiTextVAlign::Center));
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_items[i].label.c_str(),
                                                  left + (float) padL,
                                                  iy,
                                                  width - (float) padL - (float) padR,
                                                  (float) itemH,
                                                  fgArgb, fontDip, L"Segoe UI",
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}
