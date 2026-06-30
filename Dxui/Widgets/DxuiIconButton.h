#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconButton
//
//  Small square glyph button (Segoe MDL2 Assets). Draws a single icon
//  glyph with a themed hover / pressed background and fires a click
//  callback -- for compact affordances such as the settings sound-preview
//  play buttons. Slots into a DxuiPanel tree as an IDxuiControl.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiIconButton : public IDxuiControl
{
public:
    using ClickFn = std::function<void()>;

    DxuiIconButton();
    ~DxuiIconButton() override = default;

    void  SetGlyph          (const wchar_t * glyph)     { m_glyph = glyph; }
    void  SetClick          (ClickFn click)             { m_click = std::move (click); }
    void  SetDpi            (UINT dpi)                  { m_scaler.SetDpi (dpi); }
    void  SetAccessibleName (const std::wstring & name) { m_a11yName = name; }
    void  SetEnabled        (bool enabled)              { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    bool  Enabled           () const                    { return m_enabled; }
    void  SetVisible        (bool visible)              { IDxuiControl::SetVisible (visible); m_visible = visible; if (!visible) { m_hover = false; m_pressed = false; } }
    bool  Visible           () const                    { return m_visible; }
    bool  HitTest           (int x, int y) const;
    void  Click             ();

    //
    //  IDxuiControl overrides so DxuiIconButton slots into DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { m_focused = focused; }
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring        AccessibleName () const override { return m_a11yName; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    void  SetMouse (int x, int y, bool down);

    const wchar_t  * m_glyph    = L"";
    std::wstring     m_a11yName;
    ClickFn          m_click;
    DxuiDpiScaler    m_scaler;
    bool             m_hover    = false;
    bool             m_pressed  = false;
    bool             m_focused  = false;
    bool             m_enabled  = true;
    bool             m_visible  = true;
};
