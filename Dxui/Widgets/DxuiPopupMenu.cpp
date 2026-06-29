#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiPopupMenu.h"
#include "Window/DxuiHostWindow.h"
#include "Window/DxuiPopupHost.h"

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
        hr = text.MeasureString (it.label.c_str(), fontDip, DxuiTheme::kBodyFace, w, h);
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
    // acquire a pooled DxuiPopupHost and render the menu into a
    // top-level WS_POPUP so it isn't clipped by the owner client area
    // (SC-008). The cursor-anchored popup slides to stay on-monitor.
    if (m_popupHost != nullptr && m_activePopup == nullptr)
    {
        HRESULT                    hrShow      = S_OK;
        DxuiPopupHost::ShowParams  showParams;
        HWND                       owner       = m_popupHost->Hwnd();
        POINT                      cursor      = { anchorX, anchorY };
        UINT                       dpi         = m_scaler.Dpi();
        uint32_t                   bgArgb      = (m_theme != nullptr)
                                                    ? m_theme->BackgroundElevated()
                                                    : DxuiPopupHost::kDefaultMenuBackgroundArgb;

        // Anchor at the cursor in screen physical pixels (anchorX/Y are
        // client px). Size in DIPs (Show scales x dpi once); the menu
        // metrics above are physical px, so convert back to DIPs.
        ClientToScreen (owner, &cursor);

        m_activePopup = m_popupHost->AcquirePopup();
        if (m_activePopup != nullptr)
        {
            showParams.ownerHwnd        = owner;
            showParams.anchorRectScreen = { cursor.x, cursor.y, cursor.x, cursor.y };
            showParams.placement        = DxuiPopupPlacement::AtCursor;
            showParams.flipIfOffscreen  = true;
            showParams.dismiss          = DxuiPopupDismiss::OnClickOutside;
            showParams.input            = DxuiPopupInput::Interactive;
            showParams.shadow           = true;
            showParams.sizeDip.cx       = MulDiv (width,  DxuiDpiScaler::kBaseDpi, (int) dpi);
            showParams.sizeDip.cy       = MulDiv (height, DxuiDpiScaler::kBaseDpi, (int) dpi);
            showParams.backgroundArgb   = bgArgb;
            showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopupMenu (p, t); };
            showParams.onMoveInside     = [this] (POINT localPx) { OnPopupMove  (localPx); };
            showParams.onClickInside    = [this] (POINT localPx) { OnPopupClick (localPx); };
            showParams.onClosed         = [this] () { Hide(); };

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
    DxuiPopupHost *  popup  = m_activePopup;


    // Clear m_activePopup BEFORE releasing so the popup's onClosed
    // callback (which routes back here) is a no-op — no double release.
    m_visible     = false;
    m_hover       = -1;
    m_pressed     = -1;
    m_activePopup = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
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
    if (m_activePopup != nullptr)
    {
        return;
    }

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

    // A live popup owns its own input via the host WndProc.
    if (m_activePopup != nullptr) { return true; }

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

    // A live popup owns its own input via the host WndProc.
    if (m_activePopup != nullptr) { return true; }

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
        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
        return true;
    }

    if (vk == VK_UP)
    {
        if (m_items.empty()) { return true; }
        m_hover = (m_hover <= 0) ? (int) m_items.size() - 1 : m_hover - 1;
        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
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
//  In-window fallback path (no popup host wired up). Draws at the
//  panel-absolute bounds. Suppressed when a real popup is active —
//  the popup renders itself through RenderPopupMenu.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    if (!m_visible || m_activePopup != nullptr || m_theme == nullptr)
    {
        return;
    }

    PaintBody (painter, text, m_boundsDip.left, m_boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintBody
//
//  Shared menu renderer. Draws the background, border, hover row, check
//  glyphs, and labels at the supplied origin. The in-window Paint uses
//  the panel-absolute bounds; the popup render hook passes (0,0).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::PaintBody (IDxuiPainter & painter, IDxuiTextRenderer & text, int originLeft, int originTop) const
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
    float     left     = (float) originLeft;
    float     top      = (float) originTop;
    float     width    = (float) (m_boundsDip.right  - m_boundsDip.left);
    float     height   = (float) (m_boundsDip.bottom - m_boundsDip.top);


    if (m_theme == nullptr)
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
                                                      fgArgb, fontDip, DxuiTheme::kBodyFace,
                                                      DxuiTextHAlign::Left,
                                                      DxuiTextVAlign::Center));
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (m_items[i].label.c_str(),
                                                  left + (float) padL,
                                                  iy,
                                                  width - (float) padL - (float) padR,
                                                  (float) itemH,
                                                  fgArgb, fontDip, DxuiTheme::kBodyFace,
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPopupMenu
//
//  Popup-local render hook (origin 0,0 = popup top-left). The host
//  already cleared the back buffer to the theme background; this draws
//  the border, hover row, and item text on top.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::RenderPopupMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintBody (painter, text, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPopupMove
//
//  Pointer-move inside the popup (popup-local physical pixels). Maps
//  the y to a row and re-renders on change.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnPopupMove (POINT localPx)
{
    int  border = m_scaler.Px (s_kBorderDip);
    int  itemH  = m_scaler.Px (s_kItemHeightDip);
    int  relY   = localPx.y - border;
    int  row    = -1;


    if (itemH <= 0 || m_items.empty() || relY < 0)
    {
        return;
    }

    row = relY / itemH;
    if (row < 0 || row >= (int) m_items.size())
    {
        return;
    }

    if (row != m_hover)
    {
        m_hover = row;
        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPopupClick
//
//  Left-click inside the popup (popup-local physical pixels). Commits
//  the row under the cursor and dismisses.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnPopupClick (POINT localPx)
{
    int       border = m_scaler.Px (s_kBorderDip);
    int       itemH  = m_scaler.Px (s_kItemHeightDip);
    int       relY   = localPx.y - border;
    int       row    = -1;
    SelectFn  cb     = m_onSelect;


    if (itemH <= 0 || m_items.empty() || relY < 0)
    {
        return;
    }

    row = relY / itemH;
    if (row < 0 || row >= (int) m_items.size())
    {
        return;
    }

    Hide();
    if (cb)
    {
        cb (row);
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
