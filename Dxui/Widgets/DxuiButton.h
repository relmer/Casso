#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





class DxuiButton : public IDxuiControl
{
public:
    using ClickFn = std::function<void()>;

    DxuiButton() { m_focusable = true; }
    explicit DxuiButton  (const std::wstring & label) { m_focusable = true; SetLabel (label); }
    ~DxuiButton() override = default;

    void  Layout          (const RECT & rect) { SetBounds (rect); }
    void  SetLabel        (const std::wstring & label);
    wchar_t  Accelerator  () const { return m_accelerator; }
    void  SetOnClick        (ClickFn click) { m_click = std::move (click); }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }

    // Visual variant. Default uses the theme's neutral button tokens;
    // Primary fills with the theme accent (darkened to a WCAG 4.5:1
    // contrast against its white label). Link renders as accent-colored
    // text with no fill/border (a clickable hyperlink); the consumer wires
    // SetOnClick to open the URL. A button cannot be given an arbitrary,
    // non-theme color -- every fill/text derives from IDxuiTheme.
    enum class Variant { Default, Primary, Link };
    void  SetVariant      (Variant variant) { m_variant = variant; }
    // Emphasizes a Default button (e.g. a dialog's default action) with a
    // themed accent outline, without promoting it to Primary.
    void  SetEmphasis     (bool on) { m_emphasis = on; }
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
    std::wstring  m_label;
    wchar_t       m_accelerator     = 0;
    ClickFn       m_click;
    bool          m_hover           = false;
    bool          m_pressed         = false;
    bool          m_focused         = false;
    bool          m_enabled         = true;
    bool          m_visible         = true;
    DxuiDpiScaler     m_scaler;
    Variant       m_variant         = Variant::Default;
    bool          m_emphasis        = false;
};
