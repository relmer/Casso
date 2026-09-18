#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiTheme;






class DxuiComboBox : public IDxuiControl
{
public:
    using SelectFn = std::function<void (int index)>;

    DxuiComboBox() { m_focusable = true; }
    ~DxuiComboBox() override = default;

    void  SetRect     (const RECT & rect) { SetBounds (rect); }
    void  SetItems    (const std::vector<std::wstring> & items);

    // An optional Segoe MDL2 Assets glyph drawn before each item, index for
    // index with the items; an empty string, or an item past the end, has
    // none. SetItems clears them, so set them after the items.
    void  SetItemGlyphs (const std::vector<std::wstring> & glyphs);
    void  SetSelected (int index);
    void  SetEnabled  (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_armed = false; if (m_open) { Close(); } } }
    void  SetFocused  (bool focused) { m_focused = focused; if (!focused && m_open) { Close(); } }
    bool  IsFocused   () const { return m_focused; }
    void  SetSelect   (SelectFn select) { m_select = std::move (select); }
    void  SetOnHighlightChange (SelectFn fn) { m_highlightChange = std::move (fn); }
    void  Open           ();
    void  Close          ();
    bool  IsOpen()       const { return m_open; }
    int   GetHighlightIndex () const { return m_highlight; }
    int   GetSelectedIndex  () const { return m_selected; }
    const RECT & GetRect()    const { return m_boundsDip; }

    // What the open menu covers of THIS window, in GetRect()'s space. Empty
    // when closed, and empty when the menu is a real popup window too -- that
    // covers the desktop, not the frame underneath it. Callers that paint
    // over the window (a 3D pass after the panel tree) use this to stay out
    // from under the menu.
    RECT  GetInWindowMenuRect () const;
    const std::vector<std::wstring> & GetItems () const { return m_items; }

    // Most rows the open list shows at once. A longer list scrolls, by the
    // wheel or by moving the highlight past an edge, so a device reporting
    // 128 buttons does not throw a list off the bottom of the screen.
    static constexpr int  kMaxVisibleRows = 12;

    int   GetVisibleRowCount () const;
    int   GetScrollTop       () const { return m_scrollTop; }
    void  ScrollBy           (int rows);
    bool  HitTest       (int x, int y) const;
    int   HitTestItem   (int x, int y) const;
    bool  IsEnabled     () const { return m_enabled; }
    void  SetMouseHover (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    bool  HandleKey     (WPARAM vk);
    void  Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  PaintBase     (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  PaintMenu     (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  SetDpi        (UINT dpi) { m_scaler.SetDpi (dpi); }

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //  IDxuiControl::Paint invokes the existing Paint(painter, text)
    //  which renders PaintBase plus PaintMenu in z-slot. Legacy
    //  consumers that need the menu painted last across siblings
    //  keep calling PaintBase / PaintMenu directly.
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { SetFocused (focused); }
    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Dropdown; }

    //
    //  Opt-in popup hosting. When a host is supplied the dropdown
    //  acquires a DxuiPopupHost from the host's pool on Open() and
    //  releases it on Close(), so the menu portion renders into a
    //  top-level WS_POPUP HWND that is not clipped by the owner
    //  client area (FR-054 / FR-061; satisfies SC-008). Existing
    //  callers that leave the host unset retain the legacy in-
    //  panel rendering path (PaintMenu).
    //
    //  Switching the host (or clearing it via nullptr) while the
    //  dropdown is open closes the menu first so the active pooled
    //  popup is released back to the OLD host. Without this, an
    //  open dropdown whose host is about to be destroyed would leak
    //  the popup AND keep a dangling m_activePopup pointer that
    //  blocks the next Open() from acquiring a fresh popup.
    //
    void  SetPopupHost   (DxuiHwndSource * host)
    {
        if (host != m_popupHost && m_open)
        {
            Close();
        }

        m_popupHost = host;
    }

    DxuiHwndSource *  GetPopupHost () const { return m_popupHost; }
    DxuiPopupHost  *  GetActivePopup () const { return m_activePopup; }

    //
    //  Supplies the active theme so every paint path derives its colors
    //  from IDxuiTheme tokens: the themed IDxuiControl override sets it
    //  automatically, but pages that paint the box and menu separately via
    //  PaintBase / PaintMenu (for cross-widget z-order) must call this
    //  first. The pointer is also read by the popup's RenderPopupMenu hook.
    //
    void  SetTheme       (const IDxuiTheme * theme) const;

private:
    static bool  IsPointInRect (const RECT & rect, int x, int y);

    struct ResolvedColors
    {
        uint32_t  boxIdle;
        uint32_t  boxHover;
        uint32_t  boxPressed;
        uint32_t  boxDisabled;
        uint32_t  menu;
        uint32_t  menuHover;
        uint32_t  text;
        uint32_t  textDisabled;
        uint32_t  edge;
        uint32_t  edgeDisabled;
        uint32_t  focus;
    };

    void            Commit          (int index);
    void            EnsureHighlightVisible ();
    void            RenderPopupMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    float           GetGlyphIndent  (float fontPx) const;
    void            PaintItemGlyph  (IDxuiTextRenderer & text, int index, float x, float top, float height, uint32_t color, float fontPx) const;
    void            OnPopupMove     (POINT localPx);
    void            OnPopupClick    (POINT localPx);
    ResolvedColors  ResolveColors   () const;

    std::vector<std::wstring>    m_items;
    std::vector<std::wstring>    m_glyphs;
    SelectFn                     m_select;
    SelectFn                     m_highlightChange;
    bool                         m_open            = false;
    bool                         m_armed           = false;
    bool                         m_hover           = false;
    int                          m_highlight       = -1;
    int                          m_selected        = -1;
    int                          m_scrollTop       = 0;
    DxuiDpiScaler                m_scaler;
    bool                         m_enabled         = true;
    bool                         m_focused         = false;
    DxuiHwndSource             * m_popupHost       = nullptr;
    DxuiPopupHost              * m_activePopup     = nullptr;
    mutable bool                 m_hasThemeColors  = false;
    mutable ResolvedColors       m_themeColors     = {};
};
