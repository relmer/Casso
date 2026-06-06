#include "Pch.h"

#include "Widgets/DxuiPopupMenu.h"
#include "Win32/DxuiHostWindow.h"
#include "Win32/DxuiPopupHost.h"

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

    m_boundsDip.left   = left;
    m_boundsDip.top    = top;
    m_boundsDip.right  = left + width;
    m_boundsDip.bottom = top  + height;

    // Opt-in popup hosting (see header). When a host is wired up we
    // acquire a pooled DxuiPopupHost and show the menu via WS_POPUP
    // so it isn't clipped by the owner client area.
    if (m_popupHost != nullptr && m_activePopup == nullptr)
    {
        HRESULT                    hrShow      = S_OK;
        DxuiPopupHost::ShowParams  showParams;

        m_activePopup = m_popupHost->AcquirePopup();
        if (m_activePopup != nullptr)
        {
            showParams.ownerHwnd        = m_popupHost->Hwnd();
            showParams.anchorRectScreen = m_boundsDip;
            showParams.placement        = DxuiPopupPlacement::AtCursor;
            showParams.flipIfOffscreen  = true;
            showParams.dismiss          = DxuiPopupDismiss::OnClickOutside;
            showParams.input            = DxuiPopupInput::Interactive;
            showParams.shadow           = true;
            showParams.sizeDip.cx       = width;
            showParams.sizeDip.cy       = height;
            showParams.content          = std::make_unique<DxuiPanel>();

            hrShow = m_activePopup->Show (std::move (showParams));
            if (FAILED (hrShow))
            {
                m_popupHost->ReleasePopup (m_activePopup);
                m_activePopup = nullptr;
            }
        }
    }
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

    if (m_activePopup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (m_activePopup);
        m_activePopup = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::HitTest (int x, int y) const
{
    return m_visible
        && x >= m_boundsDip.left && x < m_boundsDip.right
        && y >= m_boundsDip.top  && y < m_boundsDip.bottom;
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
    int  relY   = y - (m_boundsDip.top + border);

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
    float     left     = (float) m_boundsDip.left;
    float     top      = (float) m_boundsDip.top;
    float     width    = (float) (m_boundsDip.right  - m_boundsDip.left);
    float     height   = (float) (m_boundsDip.bottom - m_boundsDip.top);


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




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Layout  (IDxuiControl override)
//
//  The popup geometry is computed by Show(); the override only
//  records the panel-supplied bounds for IDxuiControl::Bounds()
//  consumers and updates the DPI scaler.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme == nullptr)
    {
        m_theme = &theme;
    }
    static_cast<const DxuiPopupMenu *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        return false;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}
