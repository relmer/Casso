#pragma once

#include "Pch.h"


class DxuiHostWindow;
class DxuiPopupHost;






class DxuiDropdown
{
public:
    using SelectFn = std::function<void (int index)>;

    void  SetRect        (const RECT & rect) { m_rect = rect; }
    void  SetItems       (const std::vector<std::wstring> & items);
    void  SetSelected    (int index);
    void  SetEnabled     (bool enabled) { m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; if (m_open) { Close(); } } }
    void  SetFocused     (bool focused) { m_focused = focused; if (!focused && m_open) { Close(); } }
    bool  Focused        () const { return m_focused; }
    void  SetSelect      (SelectFn select) { m_select = std::move (select); }
    void  SetOnHighlightChange (SelectFn fn) { m_highlightChange = std::move (fn); }
    void  Open           ();
    void  Close          ();
    bool  IsOpen()       const { return m_open; }
    int   HighlightIndex () const { return m_highlight; }
    int   SelectedIndex  () const { return m_selected; }
    const RECT & Rect()    const { return m_rect; }
    const std::vector<std::wstring> & Items () const { return m_items; }
    bool  HitTest        (int x, int y) const;
    int   ItemHitTest    (int x, int y) const;
    bool  Enabled        () const { return m_enabled; }
    void  SetMouseHover  (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  HandleKey      (WPARAM vk);
    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  PaintBase      (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  PaintMenu      (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    //
    //  Opt-in popup hosting. When a host is supplied the dropdown
    //  acquires a DxuiPopupHost from the host's pool on Open() and
    //  releases it on Close(), so the menu portion renders into a
    //  top-level WS_POPUP HWND that is not clipped by the owner
    //  client area (FR-054 / FR-061; satisfies SC-008). Existing
    //  callers that leave the host unset retain the legacy in-
    //  panel rendering path (PaintMenu).
    //
    void  SetPopupHost   (DxuiHostWindow * host) { m_popupHost = host; }
    DxuiHostWindow *  PopupHost () const { return m_popupHost; }
    DxuiPopupHost  *  ActivePopup () const { return m_activePopup; }

private:
    void  Commit         (int index);

    std::vector<std::wstring>  m_items;
    SelectFn                  m_select;
    SelectFn                  m_highlightChange;
    RECT                      m_rect      = {};
    bool                      m_open      = false;
    bool                      m_pressed   = false;
    bool                      m_hover     = false;
    int                       m_highlight = -1;
    int                       m_selected  = -1;
    DxuiDpiScaler                 m_scaler;
    bool                      m_enabled   = true;
    bool                      m_focused   = false;
    DxuiHostWindow          * m_popupHost   = nullptr;
    DxuiPopupHost           * m_activePopup = nullptr;
};
