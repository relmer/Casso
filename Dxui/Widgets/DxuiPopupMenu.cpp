#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiPopupMenu.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"

#include "Core/UnicodeSymbols.h"





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
    constexpr int    kMinWidthDip      = 140;



    HRESULT  hr        = S_OK;
    float    fontDip   = 0.0f;
    int      itemH     = 0;
    int      padL      = 0;
    int      padR      = 0;
    int      border    = 0;
    int      minW      = 0;
    int      widestPx  = 0;
    int      width     = 0;
    int      height    = 0;
    int      left      = anchorX;
    int      top       = anchorY;


    m_items   = std::move (items);
    m_hover   = -1;
    m_pressed = -1;
    m_visible = true;

    // Popup DPI follows its host window, folding what used to be an
    // explicit SetDpi push from the consumer into the show path.
    if (m_popupHost != nullptr)
    {
        m_scaler.SetDpi (m_popupHost->GetScaler().GetDpi());
    }

    fontDip = (float) m_scaler.ToPxf (kFontDip);
    itemH   = m_scaler.ToPx (kItemHeightDip);
    padL    = m_scaler.ToPx (kItemPadLeftDip);
    padR    = m_scaler.ToPx (kItemPadRightDip);
    border  = m_scaler.ToPx (kBorderDip);
    minW    = m_scaler.ToPx (kMinWidthDip);

    for (const auto & it : m_items)
    {
        float  w   = 0.0f;
        float  h   = 0.0f;
        int    wpx = 0;
        hr = text.MeasureString (it.label.c_str(), fontDip, DxuiTheme::kBodyFace, w, h);
        if (FAILED (hr))
        {
            w = (float) (it.label.size() * 8);
        }

        wpx = (int) std::ceil (w);
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
        HWND                       owner       = m_popupHost->GetHwnd();
        POINT                      cursor      = { anchorX, anchorY };
        UINT                       dpi         = m_scaler.GetDpi();
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
    int   border    = m_scaler.ToPx (kBorderDip);
    int   itemH     = m_scaler.ToPx (kItemHeightDip);
    int   relY      = y - (m_boundsDip.top + border);
    int   idx       = -1;
    bool  isInItems = false;



    isInItems = HitTest (x, y) && itemH > 0 && relY >= 0;

    if (isInItems)
    {
        idx = relY / itemH;

        if (idx >= (int) m_items.size())
        {
            idx = -1;
        }
    }

    return idx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnMouseMove (int x, int y)
{
    // A live popup owns its own input via the host WndProc.
    if (m_activePopup == nullptr && m_visible)
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
    // Captured before Hide() can clear it: a visible menu consumes the click
    // either way, so the result is about the state on ENTRY, not on exit.
    bool  wasVisible = m_visible;
    bool  isOwnInput = false;



    // A live popup owns its own input via the host WndProc, so the menu
    // consumes the click but does nothing with it.
    isOwnInput = wasVisible && m_activePopup == nullptr;

    if (isOwnInput && !HitTest (x, y))
    {
        Hide();
    }
    else if (isOwnInput)
    {
        m_pressed = HitTestIndex (x, y);
    }

    return wasVisible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Commits a row on a completed click, or dismisses on a click-away.
//
//  Two captures happen before anything can run, and both are lifetime
//  problems. m_visible is read up front because Hide clears it, and the return
//  value must report whether the menu was visible when the click ARRIVED --
//  otherwise the owner cannot tell a consumed click from one to pass along.
//  The callback is likewise copied before Hide, since Hide can tear down state
//  the callback lives in.
//
//  A live HOSTED popup owns its own input through the host WndProc, so this
//  path applies only to the in-window fallback. Handling both would
//  double-commit.
//
//  A commit requires the release to land on the SAME row the press did, so a
//  press-then-drag-off cancels rather than selecting whatever it ended over.
//
//  Hide runs BEFORE the callback fires, so a handler that opens a dialog or
//  another menu does not do it underneath a menu still on screen.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnLButtonUp (int x, int y)
{
    int       idx        = HitTestIndex (x, y);
    SelectFn  cb         = m_onSelect;
    int       commit     = -1;
    // Captured before Hide() can clear it -- see OnLButtonDown.
    bool      wasVisible = m_visible;
    bool      isOwnInput = false;



    // A live popup owns its own input via the host WndProc.
    isOwnInput = wasVisible && m_activePopup == nullptr;

    if (isOwnInput)
    {
        if (idx >= 0 && idx == m_pressed)
        {
            commit = idx;
        }

        m_pressed = -1;

        if (commit >= 0)
        {
            Hide();

            if (cb) { cb (commit); }
        }
        else if (!HitTest (x, y))
        {
            Hide();
        }
    }

    return wasVisible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Keyboard handling for an open menu: navigate, commit, or dismiss.
//
//  A visible menu SWALLOWS EVERY KEY, including ones it does nothing with --
//  that is what the return value reports. A menu is modal in practice, so
//  letting an unhandled key through would type into whatever is behind it. The
//  arrow arms consequently fall through silently on an empty item list rather
//  than reporting unhandled.
//
//  m_visible is captured before anything can Hide, so the answer describes the
//  state when the key arrived.
//
//  Up and Down WRAP, matching the dropdown and every platform menu.
//
//  A hosted popup is explicitly marked dirty on each move: it renders in its
//  own window, outside the owner's paint pass, and would otherwise show a
//  stale highlight while the keyboard walks through it.
//
//  Commit hides before invoking the callback, so a handler that opens another
//  window does not do it beneath a menu still on screen.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (WPARAM vk)
{
    // Captured before Hide() can clear it -- see OnLButtonDown. A visible menu
    // swallows every key, including ones it does nothing with, so the arrow
    // arms below fall through silently on an empty item list.
    bool      wasVisible = m_visible;
    bool      hasItems   = !m_items.empty();
    SelectFn  cb         = nullptr;
    int       commit     = -1;



    if (wasVisible && vk == VK_ESCAPE)
    {
        Hide();
    }
    else if (wasVisible && vk == VK_DOWN && hasItems)
    {
        m_hover = (m_hover + 1) % (int) m_items.size();

        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
    }
    else if (wasVisible && vk == VK_UP && hasItems)
    {
        m_hover = (m_hover <= 0) ? (int) m_items.size() - 1 : m_hover - 1;

        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
    }
    else if (wasVisible && (vk == VK_RETURN || vk == VK_SPACE) &&
             m_hover >= 0 && m_hover < (int) m_items.size())
    {
        cb     = m_onSelect;
        commit = m_hover;
        Hide();

        if (cb) { cb (commit); }
    }

    return wasVisible;
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
    constexpr int    kCheckGlyphOffDip = 10;



    HRESULT   hr       = S_OK;
    int       border   = m_scaler.ToPx (kBorderDip);
    int       itemH    = m_scaler.ToPx (kItemHeightDip);
    int       padL     = m_scaler.ToPx (kItemPadLeftDip);
    int       padR     = m_scaler.ToPx (kItemPadRightDip);
    int       glyphX   = m_scaler.ToPx (kCheckGlyphOffDip);
    float     fontDip  = (float) m_scaler.ToPxf (kFontDip);
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
            hr = text.DrawString (s_kpszCheckMark,
                                  left + (float) glyphX,
                                  iy,
                                  (float) (padL - glyphX),
                                  (float) itemH,
                                  fgArgb, fontDip, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left,
                                  DxuiTextVAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        hr = text.DrawString (m_items[i].label.c_str(),
                              left + (float) padL,
                              iy,
                              width - (float) padL - (float) padR,
                              (float) itemH,
                              fgArgb, fontDip, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
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
    int   border  = m_scaler.ToPx (kBorderDip);
    int   itemH   = m_scaler.ToPx (kItemHeightDip);
    int   relY    = localPx.y - border;
    int   row     = -1;
    bool  isInRow = false;



    isInRow = itemH > 0 && !m_items.empty() && relY >= 0;

    if (isInRow)
    {
        row     = relY / itemH;
        isInRow = row < (int) m_items.size();
    }

    if (isInRow && row != m_hover)
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
    int       border  = m_scaler.ToPx (kBorderDip);
    int       itemH   = m_scaler.ToPx (kItemHeightDip);
    int       relY    = localPx.y - border;
    int       row     = -1;
    SelectFn  cb      = m_onSelect;
    bool      isInRow = false;



    isInRow = itemH > 0 && !m_items.empty() && relY >= 0;

    if (isInRow)
    {
        row     = relY / itemH;
        isInRow = row < (int) m_items.size();
    }

    if (isInRow)
    {
        Hide();

        if (cb)
        {
            cb (row);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Layout  (IDxuiControl override)
//
//  The popup geometry is computed by Show(); the override only
//  records the panel-supplied bounds for IDxuiControl::GetBounds()
//  consumers and updates the DPI scaler.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
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
//  DxuiPopupMenu::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A HOSTED popup never reaches this path -- it lives in its own HWND and
//  delivers input through the callbacks installed when it was shown. This
//  serves the in-window fallback menu only.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}


