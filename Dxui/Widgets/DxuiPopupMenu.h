#pragma once

#include "Pch.h"

#include "ChromeTheme.h"





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
//  All metrics are DPI-scaled. Theming pulls dropdownBg / navHover /
//  dropdownItemText from the active `ChromeTheme`.
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
    void  SetTheme    (const ChromeTheme * th)  { m_theme = th; }
    void  SetOnSelect (SelectFn fn)             { m_onSelect = std::move (fn); }

    bool                       IsVisible () const { return m_visible; }
    const std::vector<Item>  & Items     () const { return m_items;   }
    const RECT               & Rect      () const { return m_rect;    }

    void  Show           (int anchorX,
                          int anchorY,
                          std::vector<Item> items,
                          DxuiTextRenderer & text,
                          const RECT & hostClient);
    void  Hide           ();

    bool  HitTest        (int x, int y) const;
    void  OnMouseMove    (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnKey          (WPARAM vk);
    void  Paint          (DxuiPainter & painter, DxuiTextRenderer & text) const;

private:
    int   HitTestIndex (int x, int y) const;


    std::vector<Item>     m_items;
    SelectFn              m_onSelect;
    const ChromeTheme   * m_theme    = nullptr;
    RECT                  m_rect     = {};
    int                   m_hover    = -1;
    int                   m_pressed  = -1;
    bool                  m_visible  = false;
    DxuiDpiScaler             m_scaler;
};
