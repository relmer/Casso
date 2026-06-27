#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiTextInput.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox
//
//  Themed single-line search field. Hosts a chromeless DxuiTextInput for
//  the actual editing (caret, selection, clipboard, blink) and owns the
//  frame plus two affordances:
//
//    * A leading magnifier glyph (Segoe MDL2) with a "Search" placeholder.
//      While the field is empty and unfocused the glyph sits at the left
//      with the placeholder beside it. On focus it slides left and fades
//      out, collapsing the leading inset so the caret rests at the left
//      edge. The slide is time-driven via Tick.
//
//    * A trailing clear (X) glyph, shown only while the field is focused
//      and non-empty. Clicking it empties the field and fires OnChange.
//
//  The widget is host-driven (like DxuiListView): a dialog custom body or
//  panel forwards mouse / key / char / tick events through the public
//  methods. OnChange fires on every edit so the host can filter live.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSearchBox : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (const std::wstring &)>;

    DxuiSearchBox ();
    ~DxuiSearchBox() override = default;

    void  SetDpi         (UINT dpi);
    void  SetTheme       (const IDxuiTheme * theme)     { m_theme = theme; m_input.SetTheme (theme); }
    void  SetRect        (const RECT & rect);
    void  SetHwnd        (HWND hwnd)                    { m_input.SetHwnd (hwnd); }
    void  SetPlaceholder (const std::wstring & text)    { m_input.SetPlaceholder (text); }
    void  SetOnChange    (ChangeFn fn)                  { m_change = std::move (fn); }

    const std::wstring & Text     () const              { return m_input.Text(); }
    void                 SetText  (const std::wstring & text);
    void                 Clear    ();
    bool                 Focused  () const              { return m_focused; }
    void                 SetFocused (bool focused);

    // Host-driven input. The bespoke (x, y) / WPARAM / wchar_t entry points
    // match a dialog custom-body channel; each returns true when consumed.
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    void  OnMouseMove    (int x, int y);
    bool  OnKey          (WPARAM vk);
    bool  OnChar         (wchar_t ch);

    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees and the
    //  shared animation / paint pump. Mouse / keyboard is host-driven
    //  through the bespoke entry points above.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                Tick           (int64_t nowMs) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override { return m_input.Text(); }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::TextInput; }

private:
    void  RelayoutInput   ();
    bool  IsClearVisible  () const;
    RECT  ClearGlyphRect  () const;
    bool  HitTestClear    (int x, int y) const;
    void  FireChange      ();

    DxuiTextInput       m_input;
    const IDxuiTheme  * m_theme        = nullptr;
    ChangeFn            m_change;
    DxuiDpiScaler       m_scaler;
    bool                m_focused      = false;
    bool                m_clearHover   = false;
    bool                m_clearPressed = false;

    // Magnifier presence, 0..1: 1 == fully shown (empty + unfocused),
    // 0 == hidden (focused or non-empty). Animated toward its target in
    // Tick; drives the glyph alpha / slide and the input's leading inset.
    float               m_glyphShown   = 1.0f;
    int64_t             m_lastTickMs   = 0;
};
