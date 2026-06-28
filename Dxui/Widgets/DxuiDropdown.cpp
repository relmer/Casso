#include "Pch.h"

#include "DxuiDropdown.h"
#include "Window/DxuiHostWindow.h"
#include "Window/DxuiPopupHost.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"





namespace
{
    constexpr uint32_t  s_kFocusRingArgb   = 0xFFAACCFF;
    constexpr float     s_kFocusRingPx     = 1.5f;
    constexpr float     s_kFocusInsetPx    = -2.0f;
    constexpr int       s_kRowHeightDip     = 28;
    constexpr int       s_kTextInsetDip     = 8;
    constexpr int       s_kChevronWidthDip  = 10;
    constexpr int       s_kChevronHeightDip = 5;
    constexpr int       s_kChevronRightDip  = 10;
    constexpr uint32_t  s_kBoxIdleArgb     = 0xFF263241;
    constexpr uint32_t  s_kBoxHoverArgb    = 0xFF33475C;
    constexpr uint32_t  s_kBoxPressedArgb  = 0xFF1E2733;
    constexpr uint32_t  s_kBoxDisabledArgb = 0xFF1C242F;
    constexpr uint32_t  s_kMenuArgb        = 0xFF202A35;
    constexpr uint32_t  s_kMenuHoverArgb   = 0xFF34475F;
    constexpr uint32_t  s_kTextArgb        = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabledArgb = 0xFF6A7585;
    constexpr uint32_t  s_kEdgeArgb        = 0xFF5C7088;
    constexpr uint32_t  s_kEdgeDisabledArgb = 0xFF364252;
    constexpr float     s_kEdgePx          = 1.0f;
    constexpr float     s_kFontDip         = 13.0f;
    constexpr float     s_kDisabledScale   = 0.7f;   // darken themed box fill for the disabled state
    constexpr wchar_t   s_kFontFamily[]    = L"Segoe UI";


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetItems
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetItems (const std::vector<std::wstring> & items)
{
    m_items = items;

    if (m_selected >= (int) m_items.size())
    {
        m_selected = m_items.empty() ? -1 : 0;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetSelected (int index)
{
    if (index < 0 || index >= (int) m_items.size())
    {
        m_selected = m_items.empty() ? -1 : 0;
        return;
    }

    m_selected = index;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Open
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Open()
{
    DxuiPopupHost::ShowParams  showParams;
    POINT                      tl     = {};
    POINT                      br     = {};
    HWND                       owner  = nullptr;
    HRESULT                    hr     = S_OK;


    m_open      = true;
    m_highlight = (m_selected >= 0) ? m_selected : (m_items.empty() ? -1 : 0);

    // Opt-in popup hosting: if a host window is wired up, acquire a
    // pooled popup that renders the menu into its own top-level
    // WS_POPUP HWND (no client-area clipping; delivers SC-008). With
    // no host, the menu falls back to the in-window PaintMenu path.
    if (m_popupHost == nullptr || m_activePopup != nullptr)
    {
        return;
    }

    owner         = m_popupHost->Hwnd();
    m_activePopup = m_popupHost->AcquirePopup();
    if (m_activePopup == nullptr)
    {
        return;
    }

    // Anchor in screen physical pixels. m_boundsDip holds physical
    // CLIENT pixels (the page lays out via DxuiDpiScaler::Px), so map
    // straight to screen with ClientToScreen — no extra DPI scaling.
    tl.x = m_boundsDip.left;
    tl.y = m_boundsDip.top;
    br.x = m_boundsDip.right;
    br.y = m_boundsDip.bottom;
    ClientToScreen (owner, &tl);
    ClientToScreen (owner, &br);

    showParams.ownerHwnd        = owner;
    showParams.anchorRectScreen = { tl.x, tl.y, br.x, br.y };
    showParams.placement        = DxuiPopupPlacement::Below;
    showParams.flipIfOffscreen  = true;
    showParams.dismiss          = DxuiPopupDismiss::OnClickOutside;
    showParams.input            = DxuiPopupInput::Interactive;
    showParams.shadow           = true;
    // Show() scales sizeDip by the owner DPI, so feed DIPs: convert the
    // box width (physical px) back to DIPs, and use the DIP row-height
    // constant directly for the column.
    showParams.sizeDip.cx       = MulDiv (m_boundsDip.right - m_boundsDip.left,
                                          DxuiDpiScaler::kBaseDpi,
                                          (int) m_scaler.Dpi());
    showParams.sizeDip.cy       = (int) m_items.size() * s_kRowHeightDip;
    showParams.backgroundArgb   = s_kMenuArgb;
    showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopupMenu (p, t); };
    showParams.onMoveInside     = [this] (POINT localPx) { OnPopupMove  (localPx); };
    showParams.onClickInside    = [this] (POINT localPx) { OnPopupClick (localPx); };
    showParams.onClosed         = [this] () { Close(); };

    hr = m_activePopup->Show (std::move (showParams));
    CHR (hr);

    return;

Error:

    // Show failed — return the pooled popup and fall back to no menu.
    m_popupHost->ReleasePopup (m_activePopup);
    m_activePopup = nullptr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Close()
{
    DxuiPopupHost *  popup  = m_activePopup;


    // Clear m_activePopup BEFORE releasing so the popup's onClosed
    // callback (which routes back here) sees no active popup and is a
    // no-op — preventing a double release / recursion.
    m_open        = false;
    m_activePopup = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnPopupMove
//
//  Pointer-move inside the popup (popup-local physical pixels). Maps
//  the y to a row and, on change, updates the highlight + notifies +
//  re-renders the popup. Cheap enough to run on every move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::OnPopupMove (POINT localPx)
{
    int  rowHeight  = m_scaler.Px (s_kRowHeightDip);
    int  row        = -1;


    if (rowHeight <= 0 || m_items.empty())
    {
        return;
    }

    row = localPx.y / rowHeight;
    if (row < 0 || row >= (int) m_items.size())
    {
        return;
    }

    if (row != m_highlight)
    {
        m_highlight = row;
        if (m_highlightChange)
        {
            m_highlightChange (m_highlight);
        }
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
//  the row under the cursor and closes the menu.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::OnPopupClick (POINT localPx)
{
    int  rowHeight  = m_scaler.Px (s_kRowHeightDip);
    int  row        = -1;


    if (rowHeight <= 0 || m_items.empty())
    {
        return;
    }

    row = localPx.y / rowHeight;
    if (row < 0 || row >= (int) m_items.size())
    {
        return;
    }

    Commit (row);
    Close();
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::HitTest (int x, int y) const
{
    return RectContains (m_boundsDip, x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ItemHitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDropdown::ItemHitTest (int x, int y) const
{
    RECT  menuRect   = m_boundsDip;
    int   index      = -1;
    int   rowHeight  = m_scaler.Px (s_kRowHeightDip);



    if (m_activePopup != nullptr)
    {
        return -1;
    }

    menuRect.top    = m_boundsDip.bottom;
    menuRect.bottom = m_boundsDip.bottom + (int) m_items.size() * rowHeight;

    if (!m_open || !RectContains (menuRect, x, y))
    {
        return -1;
    }

    index = (y - menuRect.top) / rowHeight;
    if (index < 0 || index >= (int) m_items.size())
    {
        return -1;
    }

    return index;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetMouseHover (int x, int y)
{
    int  item = ItemHitTest (x, y);



    if (!m_enabled)
    {
        m_hover = false;
        m_armed = false;
        return;
    }

    m_hover = HitTest (x, y);

    if (item >= 0 && item != m_highlight)
    {
        m_highlight = item;
        if (m_highlightChange)
        {
            m_highlightChange (m_highlight);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnLButtonDown (int x, int y)
{
    if (!m_enabled)
    {
        return false;
    }

    if (HitTest (x, y))
    {
        m_armed = true;
        return true;
    }

    if (ItemHitTest (x, y) >= 0)
    {
        return true;
    }

    if (m_open)
    {
        Close();
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnLButtonUp (int x, int y)
{
    int   item       = ItemHitTest (x, y);
    bool  wasArmed   = m_armed;



    if (!m_enabled)
    {
        return false;
    }

    m_armed = false;

    if (item >= 0)
    {
        Commit (item);
        Close();
        return true;
    }

    if (wasArmed && HitTest (x, y))
    {
        if (m_open)
        {
            Close();
        }
        else
        {
            Open();
        }
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::HandleKey (WPARAM vk)
{
    int  count = (int) m_items.size();



    if (!m_enabled || count <= 0)
    {
        return false;
    }

    if (!m_open)
    {
        if (!m_focused)
        {
            return false;
        }

        if (vk == VK_RETURN || vk == VK_SPACE || vk == VK_DOWN)
        {
            Open();
            return true;
        }

        return false;
    }

    if (vk == VK_DOWN)
    {
        m_highlight = (m_highlight + 1) % count;
        if (m_highlightChange) { m_highlightChange (m_highlight); }
        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
        return true;
    }

    if (vk == VK_UP)
    {
        m_highlight = (m_highlight + count - 1) % count;
        if (m_highlightChange) { m_highlightChange (m_highlight); }
        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }
        return true;
    }

    if (vk == VK_RETURN)
    {
        Commit (m_highlight);
        Close();
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        Close();
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Commit (int index)
{
    bool  changed = index != m_selected;



    if (index < 0 || index >= (int) m_items.size())
    {
        return;
    }

    m_selected = index;

    if (changed && m_select)
    {
        m_select (index);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Convenience that paints both the base box and (if open) the popup
//  menu in a single call. Callers that need correct z-order with
//  multiple dropdowns should use PaintBase / PaintMenu separately.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintBase (painter, text);
    PaintMenu (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveColors
//
//  Resolves the box / menu / text / edge / focus colours from the active
//  theme (set via SetTheme or the themed Paint override). With no theme
//  set the hardcoded dark defaults stand in, so a dropdown painted before
//  a theme is supplied still renders sensibly.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDropdown::ResolvedColors DxuiDropdown::ResolveColors () const
{
    ResolvedColors  c = { s_kBoxIdleArgb,  s_kBoxHoverArgb,     s_kBoxPressedArgb,
                          s_kBoxDisabledArgb, s_kMenuArgb,      s_kMenuHoverArgb,
                          s_kTextArgb,     s_kTextDisabledArgb, s_kEdgeArgb,
                          s_kEdgeDisabledArgb, s_kFocusRingArgb };


    if (m_paintTheme != nullptr)
    {
        c.boxIdle      = m_paintTheme->BackgroundElevated();
        c.boxHover     = m_paintTheme->HoverBackground();
        c.boxPressed   = m_paintTheme->PressedBackground();
        c.boxDisabled  = DxuiColor::Darken (m_paintTheme->BackgroundElevated(), s_kDisabledScale);
        c.menu         = m_paintTheme->BackgroundElevated();
        c.menuHover    = m_paintTheme->HoverBackground();
        c.text         = m_paintTheme->Foreground();
        c.textDisabled = m_paintTheme->ForegroundDisabled();
        c.edge         = m_paintTheme->Border();
        c.edgeDisabled = m_paintTheme->Divider();
        c.focus        = m_paintTheme->FocusRing();
    }

    return c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintBase
//
//  Paints the closed-box portion of the dropdown: background fill,
//  border, selected-item text, and a chevron glyph on the right that
//  signals click-to-open.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::PaintBase (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT      hr            = S_OK;
    ResolvedColors c           = ResolveColors();
    uint32_t     boxColor      = !m_enabled            ? c.boxDisabled
                                 : (m_armed && m_hover) ? c.boxPressed
                                 : (m_open || m_hover)  ? c.boxHover
                                 :                        c.boxIdle;
    uint32_t     textColor     = m_enabled ? c.text : c.textDisabled;
    uint32_t     edgeColor     = m_enabled ? c.edge : c.edgeDisabled;
    std::wstring label;
    float        edgePx        = m_scaler.Pxf (s_kEdgePx);
    float        fontDip       = m_scaler.Pxf (s_kFontDip);
    int          textInset     = m_scaler.Px (s_kTextInsetDip);
    int          chevronW      = m_scaler.Px (s_kChevronWidthDip);
    int          chevronH      = m_scaler.Px (s_kChevronHeightDip);
    int          chevronRight  = m_scaler.Px (s_kChevronRightDip);
    int          chevronX      = m_boundsDip.right - chevronRight - chevronW;
    int          chevronY      = (m_boundsDip.top + m_boundsDip.bottom) / 2 - chevronH / 2;
    int          textWidth     = (m_boundsDip.right - m_boundsDip.left) - textInset - (chevronRight + chevronW);



    if (m_selected >= 0 && m_selected < (int) m_items.size())
    {
        label = m_items[(size_t) m_selected];
    }

    if (textWidth < 0)
    {
        textWidth = 0;
    }

    painter.FillRect    ((float) m_boundsDip.left,
                         (float) m_boundsDip.top,
                         (float) (m_boundsDip.right - m_boundsDip.left),
                         (float) (m_boundsDip.bottom - m_boundsDip.top),
                         boxColor);
    painter.OutlineRect ((float) m_boundsDip.left,
                         (float) m_boundsDip.top,
                         (float) (m_boundsDip.right - m_boundsDip.left),
                         (float) (m_boundsDip.bottom - m_boundsDip.top),
                         edgePx,
                         edgeColor);
    IGNORE_RETURN_VALUE (hr, text.DrawString (label.c_str(),
                                              (float) (m_boundsDip.left + textInset),
                                              (float) m_boundsDip.top,
                                              (float) textWidth,
                                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                                              textColor,
                                              fontDip,
                                              s_kFontFamily,
                                              DxuiTextHAlign::Left,
                                              DxuiTextVAlign::Center));

    // Chevron: stack of horizontal rects forming a downward triangle.
    for (int row = 0; row < chevronH; row++)
    {
        int  inset = (row * chevronW) / (2 * chevronH);
        int  w     = chevronW - inset * 2;

        if (w <= 0) break;

        painter.FillRect ((float) (chevronX + inset),
                          (float) (chevronY + row),
                          (float) w,
                          1.0f,
                          textColor);
    }

    if (m_focused)
    {
        float  focusInset = m_scaler.Pxf (s_kFocusInsetPx);
        float  focusThick = m_scaler.Pxf (s_kFocusRingPx);

        painter.OutlineRect ((float) m_boundsDip.left + focusInset,
                             (float) m_boundsDip.top  + focusInset,
                             (float) (m_boundsDip.right  - m_boundsDip.left) - focusInset * 2.0f,
                             (float) (m_boundsDip.bottom - m_boundsDip.top)  - focusInset * 2.0f,
                             focusThick,
                             c.focus);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintMenu
//
//  Paints the popup list when the dropdown is open. Pages that host
//  several dropdowns should call this AFTER painting every other
//  widget so the open menu draws on top of them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::PaintMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT         hr        = S_OK;
    int             i         = 0;
    int             rowHeight = m_scaler.Px (s_kRowHeightDip);
    int             textInset = m_scaler.Px (s_kTextInsetDip);
    float           fontDip   = m_scaler.Pxf (s_kFontDip);
    ResolvedColors  c         = ResolveColors();


    (void) painter;

    if (!m_open || m_activePopup != nullptr)
    {
        return;
    }

    for (i = 0; i < (int) m_items.size(); i++)
    {
        RECT      row   = { m_boundsDip.left, m_boundsDip.bottom + i * rowHeight, m_boundsDip.right, m_boundsDip.bottom + (i + 1) * rowHeight };
        uint32_t  color = (i == m_highlight) ? c.menuHover : c.menu;

        // D2D fill (not D3D painter) so the menu background composites
        // in submission order with prior text and hides sibling text
        // underneath the open menu.
        IGNORE_RETURN_VALUE (hr, text.FillRect ((float) row.left,
                                                (float) row.top,
                                                (float) (row.right - row.left),
                                                (float) (row.bottom - row.top),
                                                color));
        IGNORE_RETURN_VALUE (hr, text.DrawString (m_items[(size_t) i].c_str(),
                                                  (float) (row.left + textInset),
                                                  (float) row.top,
                                                  (float) (row.right - row.left - textInset),
                                                  (float) (row.bottom - row.top),
                                                  c.text,
                                                  fontDip,
                                                  s_kFontFamily,
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  RenderPopupMenu
//
//  Renders the option list into the popup's own back buffer, in
//  popup-local PHYSICAL pixels (origin 0,0 = popup top-left). Mirrors
//  PaintMenu but DPI-scales coordinates itself (the popup's text
//  renderer is bound at 96 DPI, so its logical units are pixels).
//  Backgrounds use the D2D text path (not the D3D painter) so rows
//  composite in submission order with the per-row label.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::RenderPopupMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT         hr        = S_OK;
    int             i         = 0;
    int             rowHeight = m_scaler.Px (s_kRowHeightDip);
    int             textInset = m_scaler.Px (s_kTextInsetDip);
    int             width     = m_boundsDip.right - m_boundsDip.left;
    float           fontPx    = m_scaler.Pxf (s_kFontDip);
    ResolvedColors  c         = ResolveColors();


    (void) painter;

    for (i = 0; i < (int) m_items.size(); i++)
    {
        RECT      row   = { 0, i * rowHeight, width, (i + 1) * rowHeight };
        uint32_t  color = (i == m_highlight) ? c.menuHover : c.menu;

        IGNORE_RETURN_VALUE (hr, text.FillRect ((float) row.left,
                                                (float) row.top,
                                                (float) (row.right - row.left),
                                                (float) (row.bottom - row.top),
                                                color));
        IGNORE_RETURN_VALUE (hr, text.DrawString (m_items[(size_t) i].c_str(),
                                                  (float) (row.left + textInset),
                                                  (float) row.top,
                                                  (float) (row.right - row.left - textInset),
                                                  (float) (row.bottom - row.top),
                                                  c.text,
                                                  fontPx,
                                                  s_kFontFamily,
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDropdown::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDropdown::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    SetTheme (&theme);
    static_cast<const DxuiDropdown *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDropdown::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
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
//  DxuiDropdown::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return HandleKey (ev.vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDropdown::AccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected item (or empty if no selection).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDropdown::AccessibleName () const
{
    if (m_selected < 0 || m_selected >= (int) m_items.size())
    {
        return L"";
    }

    return m_items[(size_t) m_selected];
}
