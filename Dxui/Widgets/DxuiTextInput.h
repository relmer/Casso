#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





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

class DxuiTextInput : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (const std::wstring &)>;

    ~DxuiTextInput() override = default;

    void  SetRect       (const RECT & rect)           { SetBounds (rect); }
    void  SetText       (const std::wstring & text)   { m_text = text; ClampCaret(); }
    void  SetMaxLength  (size_t maxLen)               { m_maxLen = maxLen; }
    void  SetFocused    (bool focused)                { m_focused = focused; if (!focused) { m_dragging = false; } ResetBlink(); }
    void  SetEnabled    (bool enabled)                { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_focused = false; m_hover = false; m_dragging = false; } }
    void  SetDpi        (UINT dpi)                    { m_scaler.SetDpi (dpi); }
    void  SetTheme      (const IDxuiTheme * theme)    { m_theme = theme; }
    void  SetOnChange   (ChangeFn fn)                 { m_change = std::move (fn); }
    void  SetHwnd       (HWND hwnd)                   { m_hwnd = hwnd; }

    // Chromeless mode: skip the self-drawn background fill and border so a
    // host widget (e.g. DxuiSearchBox) can own the frame and draw the
    // input inside it. The text, selection, caret, and placeholder still
    // paint normally.
    void  SetChromeless (bool chromeless)             { m_chromeless = chromeless; }

    // Muted prompt text drawn in place of the value while the field is
    // empty (e.g. "Search"). Empty by default.
    void  SetPlaceholder (const std::wstring & text)  { m_placeholder = text; }

    const std::wstring & Text     () const { return m_text;    }
    const RECT         & Rect     () const { return m_boundsDip;    }
    bool                 Focused  () const { return m_focused; }
    bool                 Enabled  () const { return m_enabled; }

    bool  HitTest       (int x, int y) const;
    void  SetMouseHover (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    void  OnMouseMove   (int x, int y);
    bool  OnKey         (WPARAM vk);
    bool  OnChar        (wchar_t ch);

    void  Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override { return m_text; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::TextInput; }

private:
    void   ClampCaret ();
    size_t CaretFromX (IDxuiTextRenderer & text, int xPx) const;
    void   DeleteSelection ();
    void   InsertText      (const std::wstring & ins);
    void   CopyToClipboard () const;
    void   PasteFromClipboard ();
    void   FireChange ();
    void   ResetBlink () const { m_blinkAnchorMs = 0; }

    static bool Shift   () { return (GetKeyState (VK_SHIFT)   & 0x8000) != 0; }
    static bool Control () { return (GetKeyState (VK_CONTROL) & 0x8000) != 0; }
    std::wstring          m_text;
    std::wstring          m_placeholder;
    size_t                m_maxLen     = 64;
    size_t                m_caret      = 0;
    size_t                m_anchor     = 0;
    bool                  m_focused    = false;
    bool                  m_enabled    = true;
    bool                  m_hover      = false;
    bool                  m_dragging   = false;
    bool                  m_chromeless = false;
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

    // Caret-blink phase anchor (ms, GetTickCount64 base). Seeded on the
    // first paint after focus and reset to 0 on every edit / caret move
    // so the caret shows solid immediately after interaction. Mutable
    // because the seed-on-paint happens inside the const Paint().
    mutable int64_t       m_blinkAnchorMs = 0;
};
