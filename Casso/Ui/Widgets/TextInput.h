#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextInput
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

class TextInput
{
public:
    using ChangeFn = std::function<void (const std::wstring &)>;

    void  SetRect       (const RECT & rect)           { m_rect = rect; }
    void  SetText       (const std::wstring & text)   { m_text = text; ClampCaret(); }
    void  SelectAll     ()                            { m_anchor = 0; m_caret = m_text.size(); }
    void  SetMaxLength  (size_t maxLen)               { m_maxLen = maxLen; }
    void  SetFocused    (bool focused)                { m_focused = focused; if (!focused) { m_dragging = false; } }
    void  SetEnabled    (bool enabled)                { m_enabled = enabled; if (!enabled) { m_focused = false; m_hover = false; m_dragging = false; } }
    void  SetDpi        (UINT dpi)                    { m_scaler.SetDpi (dpi); }
    void  SetTheme      (const ChromeTheme * theme)   { m_theme = theme; }
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

    void  Paint         (DxUiPainter & painter, DwriteTextRenderer & text) const;

    // Copies the entire field contents to the clipboard regardless of the
    // current selection (for host copy affordances such as a copy button).
    void  CopyAllToClipboard () const;

private:
    void   ClampCaret ();
    void   DeleteSelection ();
    void   InsertText      (const std::wstring & ins);
    void   CopyToClipboard () const;
    void   WriteTextToClipboard (const std::wstring & s) const;
    void   PasteFromClipboard ();
    void   FireChange ();
    void   ResetBlink ();

    // Caret index nearest a client x using the glyph offsets cached by the
    // most recent Paint (mouse handlers have no text renderer of their own).
    size_t CaretFromClientX (int xPx) const;

    // Word-boundary navigation from `pos` (caret index): WordLeft lands on
    // the start of the word at/just before pos; WordRight on the start of
    // the next word. Used by Ctrl+Left / Ctrl+Right.
    size_t WordLeft  (size_t pos) const;
    size_t WordRight (size_t pos) const;
    void   SelectWordAt (size_t pos);

    static bool IsWordChar (wchar_t ch) { return iswalnum (ch) != 0 || ch == L'_'; }
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
    const ChromeTheme   * m_theme      = nullptr;
    HWND                  m_hwnd       = nullptr;
    ChangeFn              m_change;
    DpiScaler             m_scaler;

    // Double-click word-select detection: time + caret index of the last
    // mouse-down, so a second click within the system double-click time
    // (on the same caret index) selects the word.
    int64_t               m_lastClickMs    = 0;
    size_t                m_lastClickCaret = 0;

    // Caret blink phase anchor (ms, GetTickCount64). Reset on every caret
    // move / edit so the caret is solid immediately after an action, then
    // blinks. Mutable so Paint() can seed it on first focus.
    mutable int64_t       m_blinkAnchorMs  = 0;

    // Cumulative text-local x (px) of each character boundary, indices
    // 0..size, recomputed each Paint at the current font / DPI. Mouse
    // hit-testing reads this (one frame old at worst -- the popup repaints
    // continuously). Mutable because Paint() is const.
    mutable std::vector<float>  m_glyphX;

    // Horizontal scroll offset (pixels) for the rendered text. Paint
    // adjusts this so the caret remains inside the visible inner
    // rect; mouse hit-testing translates xPx by m_scrollPx so clicks
    // land on the character under the cursor regardless of scroll.
    // Mutable because Paint() is const but owns the auto-scroll math.
    mutable float         m_scrollPx   = 0.0f;
};
