#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





class DxuiButton : public IDxuiControl
{
public:
    using ClickFn = std::function<void()>;

    ~DxuiButton() override = default;

    void  Layout          (const RECT & rect) { m_rect = rect; SetBounds (rect); }
    void  SetLabel        (const std::wstring & label);
    wchar_t  Accelerator  () const { return m_accelerator; }
    void  SetClick        (ClickFn click) { m_click = std::move (click); }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetColors       (uint32_t idleArgb, uint32_t hoverArgb, uint32_t pressedArgb)
    {
        m_idleOverride    = idleArgb;
        m_hoverOverride   = hoverArgb;
        m_pressedOverride = pressedArgb;
        m_useOverrides    = true;
    }
    void  SetTextColor    (uint32_t argb) { m_textOverride = argb; m_useTextOverride = true; }
    void  SetOutline      (float thicknessPx, uint32_t argb)
    {
        m_outlineThick = thicknessPx;
        m_outlineArgb  = argb;
    }
    void  SetMouse        (int x, int y, bool down);
    void  SetFocused      (bool focused) { m_focused = focused; }
    bool  Focused         () const { return m_focused; }
    void  SetEnabled      (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    bool  Enabled         () const { return m_enabled; }
    void  SetVisible      (bool visible) { IDxuiControl::SetVisible (visible); m_visible = visible; if (!visible) { m_hover = false; m_pressed = false; m_focused = false; } }
    bool  Visible         () const { return m_visible; }
    bool  HitTest         (int x, int y) const;
    void  Click           ();
    bool  OnKey           (WPARAM vk);
    void  Paint           (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    //
    //  IDxuiControl overrides — additive shims so DxuiButton slots
    //  into DxuiPanel trees. Forward to the existing widget API; the
    //  legacy entry points remain callable for direct consumers.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override { return m_label; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    RECT          m_rect            = {};
    std::wstring  m_label;
    wchar_t       m_accelerator     = 0;
    ClickFn       m_click;
    bool          m_hover           = false;
    bool          m_pressed         = false;
    bool          m_focused         = false;
    bool          m_enabled         = true;
    bool          m_visible         = true;
    DxuiDpiScaler     m_scaler;
    bool          m_useOverrides    = false;
    uint32_t      m_idleOverride    = 0;
    uint32_t      m_hoverOverride   = 0;
    uint32_t      m_pressedOverride = 0;
    bool          m_useTextOverride = false;
    uint32_t      m_textOverride    = 0;
    float         m_outlineThick    = 0.0f;
    uint32_t      m_outlineArgb     = 0;
};
