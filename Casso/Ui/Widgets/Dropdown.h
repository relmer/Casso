#pragma once

#include "Pch.h"

#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





class Dropdown
{
public:
    using SelectFn = std::function<void (int index)>;

    void  SetRect        (const RECT & rect) { m_rect = rect; }
    void  SetItems       (const std::vector<std::wstring> & items);
    void  SetSelected    (int index);
    void  SetEnabled     (bool enabled) { m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; m_open = false; } }
    void  SetFocused     (bool focused) { m_focused = focused; if (!focused) { m_open = false; } }
    bool  Focused        () const { return m_focused; }
    void  SetSelect      (SelectFn select) { m_select = std::move (select); }
    void  SetOnHighlightChange (SelectFn fn) { m_highlightChange = std::move (fn); }
    void  Open           ();
    void  Close()        { m_open = false; }
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
    void  Paint          (DxUiPainter & painter, DwriteTextRenderer & text) const;
    void  PaintBase      (DxUiPainter & painter, DwriteTextRenderer & text) const;
    void  PaintMenu      (DxUiPainter & painter, DwriteTextRenderer & text) const;
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

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
};
