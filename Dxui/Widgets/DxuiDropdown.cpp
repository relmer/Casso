#include "Pch.h"

#include "DxuiDropdown.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiTheme.h"
#include "Theme/DxuiColor.h"





static constexpr uint32_t  s_kFocusRingArgb   = 0xFFAACCFF;
static constexpr float     s_kFocusRingPx     = 1.5f;
static constexpr float     s_kFocusInsetPx    = -2.0f;
static constexpr int       s_kRowHeightDip     = 28;
static constexpr int       s_kTextInsetDip     = 8;
static constexpr int       s_kChevronWidthDip  = 10;
static constexpr int       s_kChevronHeightDip = 5;
static constexpr int       s_kChevronRightDip  = 10;
static constexpr uint32_t  s_kBoxIdleArgb     = 0xFF263241;
static constexpr uint32_t  s_kBoxHoverArgb    = 0xFF33475C;
static constexpr uint32_t  s_kBoxPressedArgb  = 0xFF1E2733;
static constexpr uint32_t  s_kBoxDisabledArgb = 0xFF1C242F;
static constexpr uint32_t  s_kMenuArgb        = 0xFF202A35;
static constexpr uint32_t  s_kMenuHoverArgb   = 0xFF34475F;
static constexpr uint32_t  s_kTextArgb        = 0xFFE8EEF4;
static constexpr uint32_t  s_kTextDisabledArgb = 0xFF6A7585;
static constexpr uint32_t  s_kEdgeArgb        = 0xFF5C7088;
static constexpr uint32_t  s_kEdgeDisabledArgb = 0xFF364252;
static constexpr float     s_kEdgePx          = 1.0f;
static constexpr float     s_kFontDip         = 13.0f;
static constexpr float     s_kDisabledScale   = 0.7f;   // darken themed box fill for the disabled state
static constexpr const wchar_t * s_kFontFamily    = DxuiTheme::kBodyFace;





////////////////////////////////////////////////////////////////////////////////
//
//  RectContains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::RectContains (const RECT & rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
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
//  Opens the list, preferring a real top-level popup window and falling back
//  to painting inside the owner.
//
//  Popup hosting is OPT-IN: with a host wired up, the menu renders into its
//  own WS_POPUP HWND and can therefore extend past the owner's client area,
//  which is what a dropdown near the bottom of a panel needs (SC-008). With no
//  host, the in-window PaintMenu path still works -- clipped, but functional --
//  so a caller that never wires a host is not broken, merely limited.
//
//  The open STATE is set before any of that and is never rolled back, so a
//  failed popup still yields an open dropdown drawn the fallback way.
//
//  The two coordinate conversions run in opposite directions and are easy to
//  get backwards. m_boundsDip holds physical CLIENT pixels despite the name
//  (the page lays out through DxuiDpiScaler::Px), so the anchor maps straight
//  to screen with ClientToScreen and needs no DPI scaling -- while Show scales
//  sizeDip by the owner's DPI, so the size must be converted BACK to DIPs
//  first.
//
//  The highlight opens on the current selection, or the first row when nothing
//  is selected, so a keyboard user starts somewhere meaningful.
//
//  The `acquired` flag gates the cleanup: the early bails share the exit path
//  and run before any popup exists, so releasing unconditionally would return
//  a popup that was never taken.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Open()
{
    DxuiPopupHost::ShowParams  showParams;
    POINT                      tl       = {};
    POINT                      br       = {};
    HWND                       owner    = nullptr;
    HRESULT                    hr       = S_OK;
    // Gates the cleanup below: the two early bails must not reach a
    // ReleasePopup on a host that may be null or a popup never acquired.
    bool                       acquired = false;



    m_open      = true;
    m_highlight = (m_selected >= 0) ? m_selected : (m_items.empty() ? -1 : 0);

    // Opt-in popup hosting: if a host window is wired up, acquire a
    // pooled popup that renders the menu into its own top-level
    // WS_POPUP HWND (no client-area clipping; delivers SC-008). With
    // no host, the menu falls back to the in-window PaintMenu path.
    BAIL_OUT_IF (m_popupHost == nullptr || m_activePopup != nullptr, S_OK);

    owner         = m_popupHost->Hwnd();
    m_activePopup = m_popupHost->AcquirePopup();
    acquired      = (m_activePopup != nullptr);

    BAIL_OUT_IF (!acquired, S_OK);

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

Error:
    // Show failed — return the pooled popup and fall back to no menu. Gated
    // on `acquired` because the bails above also land here, and they run
    // before (or instead of) any acquisition.
    if (acquired && FAILED (hr))
    {
        m_popupHost->ReleasePopup (m_activePopup);
        m_activePopup = nullptr;
    }
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
    int   rowHeight = m_scaler.Px (s_kRowHeightDip);
    int   row       = -1;
    bool  onRow     = false;



    onRow = rowHeight > 0 && !m_items.empty();

    if (onRow)
    {
        row   = localPx.y / rowHeight;
        onRow = row >= 0 && row < (int) m_items.size();
    }

    if (onRow && row != m_highlight)
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
    int   rowHeight = m_scaler.Px (s_kRowHeightDip);
    int   row       = -1;
    bool  onRow     = false;



    onRow = rowHeight > 0 && !m_items.empty();

    if (onRow)
    {
        row   = localPx.y / rowHeight;
        onRow = row >= 0 && row < (int) m_items.size();
    }

    if (onRow)
    {
        Commit (row);
        Close();
    }
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
//  Which list row a point falls on, for the IN-WINDOW fallback menu only.
//
//  A live popup owns its own hit-testing entirely -- it is a separate HWND
//  with its own coordinate space and its own click callback -- so this returns
//  a miss whenever one is up. Hit-testing both would double-handle every click
//  on a hosted dropdown.
//
//  The fallback menu is laid out immediately below the box, so its rect is
//  derived here rather than stored: nothing else needs it, and deriving keeps
//  it from drifting out of step with the paint.
//
//  A point below the last row is a miss, not the last row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDropdown::ItemHitTest (int x, int y) const
{
    RECT  menuRect  = m_boundsDip;
    int   index     = -1;
    int   rowHeight = m_scaler.Px (s_kRowHeightDip);
    bool  inMenu    = false;



    // A live popup owns its own hit-testing; this is the in-window fallback
    // menu only.
    if (m_activePopup == nullptr)
    {
        menuRect.top    = m_boundsDip.bottom;
        menuRect.bottom = m_boundsDip.bottom + (int) m_items.size() * rowHeight;
        inMenu          = m_open && RectContains (menuRect, x, y);
    }

    if (inMenu)
    {
        index = (y - menuRect.top) / rowHeight;

        if (index >= (int) m_items.size())
        {
            index = -1;
        }
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
    bool  onBox     = m_enabled && HitTest (x, y);
    bool  onItem    = m_enabled && !onBox && ItemHitTest (x, y) >= 0;
    bool  consumed  = onBox || onItem;



    if (onBox)
    {
        m_armed = true;
    }
    else if (m_enabled && !onItem && m_open)
    {
        // A press outside both the box and the open menu dismisses it, but
        // is not consumed -- whatever is underneath still gets the click.
        Close();
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
//  Acts on the release: commit a row, or toggle the list open and shut.
//
//  A box click only counts if the press ARMED it. Pressing elsewhere and
//  releasing over the box does nothing, which is the standard cancel gesture
//  and the reason the armed flag exists at all.
//
//  The armed flag is cleared before any of the branches, so no path can leave
//  it set for the next click to inherit.
//
//  Committing also CLOSES, so selecting a row never leaves the list hanging
//  open over the value it just changed.
//
//  A release on the box while open closes rather than re-opening, which makes
//  the box a toggle instead of a control that cannot be dismissed by clicking
//  the thing that opened it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnLButtonUp (int x, int y)
{
    int   item     = ItemHitTest (x, y);
    bool  wasArmed = m_armed;
    bool  onItem   = m_enabled && item >= 0;
    bool  onBox    = false;
    bool  consumed = false;



    if (m_enabled)
    {
        m_armed = false;
        onBox   = !onItem && wasArmed && HitTest (x, y);
    }

    if (onItem)
    {
        Commit (item);
        Close();
    }
    else if (onBox && m_open)
    {
        Close();
    }
    else if (onBox)
    {
        Open();
    }

    consumed = onItem || onBox;

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
//  Keyboard handling, which is two different control schemes depending on
//  whether the list is open.
//
//  CLOSED, only a focused box responds, and only to the three keys that open
//  it (Enter, Space, Down). Everything else must pass through -- a closed
//  dropdown that swallowed arrow keys would trap keyboard navigation on the
//  page.
//
//  OPEN, the list owns navigation, commit, and dismiss outright, including
//  Escape. Focus is not re-checked, because an open list is modal in practice:
//  it is the thing the user is interacting with.
//
//  Up and Down WRAP. A short list is faster to reach the end of by going the
//  other way, and wrapping is what a dropdown does.
//
//  A hosted popup is explicitly marked dirty on a highlight change. It renders
//  in its own window and is not part of the owner's paint pass, so it would
//  otherwise show a stale highlight while the keyboard moves through it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::HandleKey (WPARAM vk)
{
    HRESULT  hr      = S_OK;
    int      count   = (int) m_items.size();
    bool     handled = false;



    BAIL_OUT_IF (!m_enabled || count <= 0, S_OK);

    // Closed: only a focused box responds, and only to the three keys that
    // open it. Open: the menu owns navigation, commit and dismiss.
    if (!m_open && m_focused && (vk == VK_RETURN || vk == VK_SPACE || vk == VK_DOWN))
    {
        Open();
        handled = true;
    }
    else if (m_open && (vk == VK_DOWN || vk == VK_UP))
    {
        m_highlight = (vk == VK_DOWN) ? ((m_highlight + 1) % count)
                                      : ((m_highlight + count - 1) % count);

        if (m_highlightChange)        { m_highlightChange (m_highlight); }
        if (m_activePopup != nullptr) { m_activePopup->MarkDirty(); }

        handled = true;
    }
    else if (m_open && vk == VK_RETURN)
    {
        Commit (m_highlight);
        Close();
        handled = true;
    }
    else if (m_open && vk == VK_ESCAPE)
    {
        Close();
        handled = true;
    }

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::Commit (int index)
{
    bool  changed  = index != m_selected;
    bool  inRange  = index >= 0 && index < (int) m_items.size();



    if (!inRange)
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
//  Resolves the box / menu / text / edge / focus colors from the active
//  theme (set via SetTheme or the themed Paint override). With no theme
//  set the hardcoded dark defaults stand in, so a dropdown painted before
//  a theme is supplied still renders sensibly.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDropdown::ResolvedColors DxuiDropdown::ResolveColors() const
{
    ResolvedColors  c = { s_kBoxIdleArgb,  s_kBoxHoverArgb,     s_kBoxPressedArgb,
                          s_kBoxDisabledArgb, s_kMenuArgb,      s_kMenuHoverArgb,
                          s_kTextArgb,     s_kTextDisabledArgb, s_kEdgeArgb,
                          s_kEdgeDisabledArgb, s_kFocusRingArgb };


    if (m_hasThemeColors)
    {
        c = m_themeColors;
    }

    return c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDropdown::SetTheme (const IDxuiTheme * theme) const
{
    if (theme == nullptr)
    {
        m_hasThemeColors = false;
        return;
    }

    m_themeColors.boxIdle      = theme->BackgroundElevated();
    m_themeColors.boxHover     = theme->HoverBackground();
    m_themeColors.boxPressed   = theme->PressedBackground();
    m_themeColors.boxDisabled  = DxuiColor::Darken (theme->BackgroundElevated(), s_kDisabledScale);
    m_themeColors.menu         = theme->BackgroundElevated();
    m_themeColors.menuHover    = theme->HoverBackground();
    m_themeColors.text         = theme->Foreground();
    m_themeColors.textDisabled = theme->ForegroundDisabled();
    m_themeColors.edge         = theme->Border();
    m_themeColors.edgeDisabled = theme->Divider();
    m_themeColors.focus        = theme->FocusRing();
    m_hasThemeColors           = true;
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
    HRESULT         hr = S_OK;
    ResolvedColors  c  = ResolveColors();
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
                                              DxuiTextVAlign::Center, DxuiFontWeight::Normal, false));

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
                                                  DxuiTextVAlign::Center, DxuiFontWeight::Normal, false));
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
                                                  DxuiTextVAlign::Center, DxuiFontWeight::Normal, false));
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
//  DxuiDropdown::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the box does not consume moves other widgets want.
//
//  Note that a HOSTED popup never reaches this function at all -- it lives in
//  its own HWND and delivers its moves and clicks through the callbacks
//  installed in Open. This path serves the box, plus the in-window fallback
//  menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
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
//  DxuiDropdown::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDropdown::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = HandleKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDropdown::AccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected item (or empty if no selection).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiDropdown::AccessibleName() const
{
    std::wstring  name;
    bool          hasSelection = m_selected >= 0 && m_selected < (int) m_items.size();



    if (hasSelection)
    {
        name = m_items[(size_t) m_selected];
    }

    return name;
}

