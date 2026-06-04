#pragma once

#include "Pch.h"


class DxuiHostWindow;
class DxuiPopupHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu
//
//  Lightweight DX-rendered popup. Each item carries a label and an
//  optional checked flag (rendered as a leading check glyph). The
//  owning panel calls `Show()` from a right-click handler and routes
//  mouse / keyboard events to the popup as long as `IsVisible()` is
//  true; once a click selects an item or lands outside the panel
//  the popup hides and the panel resumes normal input.
//
//  All metrics are DPI-scaled. Theming reads BackgroundElevated /
//  HoverBackground / Foreground from the active `IDxuiTheme`.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPopupMenu
{
public:
    struct Item
    {
        std::wstring  label;
        bool          checked = false;
    };

    using SelectFn = std::function<void (int index)>;

    void  SetDpi      (UINT dpi)                { m_scaler.SetDpi (dpi); }
    void  SetTheme    (const IDxuiTheme * th)   { m_theme = th; }
    void  SetOnSelect (SelectFn fn)             { m_onSelect = std::move (fn); }

    //
    //  Opt-in popup hosting (FR-054 / FR-061). When a host window is
    //  supplied the popup acquires a pooled DxuiPopupHost on Show()
    //  and releases it on Hide(), so the menu renders into a top-
    //  level WS_POPUP HWND not clipped by the owner's client area.
    //  Cascading submenus link through DxuiPopupHost::SetParentPopup
    //  so click-outside dismiss walks the chain.
    //
    void  SetPopupHost  (DxuiHostWindow * host) { m_popupHost = host; }
    DxuiHostWindow *  PopupHost   () const { return m_popupHost;   }
    DxuiPopupHost  *  ActivePopup () const { return m_activePopup; }

    bool                       IsVisible () const { return m_visible; }
    const std::vector<Item>  & Items     () const { return m_items;   }
    const RECT               & Rect      () const { return m_rect;    }

    void  Show           (int anchorX,
                          int anchorY,
                          std::vector<Item> items,
                          IDxuiTextRenderer & text,
                          const RECT & hostClient);
    void  Hide           ();

    bool  HitTest        (int x, int y) const;
    void  OnMouseMove    (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnKey          (WPARAM vk);
    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

private:
    int   HitTestIndex (int x, int y) const;


    std::vector<Item>     m_items;
    SelectFn              m_onSelect;
    const IDxuiTheme    * m_theme    = nullptr;
    RECT                  m_rect     = {};
    int                   m_hover    = -1;
    int                   m_pressed  = -1;
    bool                  m_visible  = false;
    DxuiDpiScaler             m_scaler;
    DxuiHostWindow      * m_popupHost     = nullptr;
    DxuiPopupHost       * m_activePopup   = nullptr;
};
