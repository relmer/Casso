#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInput
//
//  Single-line themed text input widget. Owns its own selection state,
//  caret position, and clipboard plumbing.
//
//  Mouse contract:
//      LButtonDown over hit rect -> place caret, begin drag selection.
//      LButtonUp                 -> end drag selection.
//      MouseMove while dragging  -> extend selection to current x.
//
//  Keyboard contract (must be focused):
//      Left  / Right                 -> move caret 1 char (Shift extends sel).
//      Home  / End                   -> move caret to start/end.
//      Backspace                     -> delete selection or char before caret.
//      Delete                        -> delete selection or char at caret.
//      Ctrl+A                        -> select all.
//      Ctrl+C                        -> copy selection.
//      Ctrl+X                        -> cut selection.
//      Ctrl+V                        -> paste clipboard at caret / replace.
//      Printable chars (via OnChar)  -> insert at caret / replace.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextInput
{
public:
    using ChangeFn = std::function<void (const std::wstring &)>;

    void  SetRect       (const RECT & rect)           { m_rect = rect; }
    void  SetText       (const std::wstring & text)   { m_text = text; ClampCaret(); }
    void  SetMaxLength  (size_t maxLen)               { m_maxLen = maxLen; }
    void  SetFocused    (bool focused)                { m_focused = focused; if (!focused) { m_dragging = false; } }
    void  SetEnabled    (bool enabled)                { m_enabled = enabled; if (!enabled) { m_focused = false; m_hover = false; m_dragging = false; } }
    void  SetDpi        (UINT dpi)                    { m_scaler.SetDpi (dpi); }
    void  SetTheme      (const IDxuiTheme * theme)    { m_theme = theme; }
    void  SetOnChange   (ChangeFn fn)                 { m_change = std::move (fn); }
    void  SetHwnd       (HWND hwnd)                   { m_hwnd = hwnd; }

    const std::wstring & Text     () const { return m_text;    }
    const RECT         & Rect     () const { return m_rect;    }
    bool                 Focused  () const { return m_focused; }
    bool                 Enabled  () const { return m_enabled; }

    bool  HitTest       (int x, int y) const;
    void  SetMouseHover (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    void  OnMouseMove   (int x, int y);
    bool  OnKey         (WPARAM vk);
    bool  OnChar        (wchar_t ch);

    void  Paint         (DxuiPainter & painter, DxuiTextRenderer & text) const;

private:
    void   ClampCaret ();
    size_t CaretFromX (DxuiTextRenderer & text, int xPx) const;
    void   DeleteSelection ();
    void   InsertText      (const std::wstring & ins);
    void   CopyToClipboard () const;
    void   PasteFromClipboard ();
    void   FireChange ();

    static bool Shift   () { return (GetKeyState (VK_SHIFT)   & 0x8000) != 0; }
    static bool Control () { return (GetKeyState (VK_CONTROL) & 0x8000) != 0; }


    RECT                  m_rect       = {};
    std::wstring          m_text;
    size_t                m_maxLen     = 64;
    size_t                m_caret      = 0;
    size_t                m_anchor     = 0;
    bool                  m_focused    = false;
    bool                  m_enabled    = true;
    bool                  m_hover      = false;
    bool                  m_dragging   = false;
    const IDxuiTheme    * m_theme      = nullptr;
    HWND                  m_hwnd       = nullptr;
    ChangeFn              m_change;
    DxuiDpiScaler             m_scaler;

    // Horizontal scroll offset (pixels) for the rendered text. Paint
    // adjusts this so the caret remains inside the visible inner
    // rect; mouse hit-testing translates xPx by m_scrollPx so clicks
    // land on the character under the cursor regardless of scroll.
    // Mutable because Paint() is const but owns the auto-scroll math.
    mutable float         m_scrollPx   = 0.0f;
};
