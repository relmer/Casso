#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





class Button
{
public:
    using ClickFn = std::function<void()>;

    // Visual role. Both variants derive every color from the active theme;
    // there is intentionally no way to inject an arbitrary color, so a
    // button can never fall out of sync with the theme.
    //   Default  -- neutral themed face (button* tokens).
    //   Primary  -- accent call-to-action (derived from the theme accent,
    //               darkened for legible white text).
    enum class Variant
    {
        Default,
        Primary,
    };

    void  Layout          (const RECT & rect) { m_rect = rect; }
    void  SetLabel        (const std::wstring & label);
    wchar_t  Accelerator  () const { return m_accelerator; }
    void  SetClick        (ClickFn click) { m_click = std::move (click); }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetVariant      (Variant variant) { m_variant = variant; }
    // Draw an extra accent ring (e.g. to mark the default button in a
    // dialog). The ring color is taken from the theme, not the caller.
    void  SetEmphasis     (bool on) { m_emphasis = on; }
    void  SetMouse        (int x, int y, bool down);
    void  SetFocused      (bool focused) { m_focused = focused; }
    bool  Focused         () const { return m_focused; }
    void  SetEnabled      (bool enabled) { m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    bool  Enabled         () const { return m_enabled; }
    void  SetVisible      (bool visible) { m_visible = visible; if (!visible) { m_hover = false; m_pressed = false; m_focused = false; } }
    bool  Visible         () const { return m_visible; }
    bool  HitTest         (int x, int y) const;
    void  Click           ();
    bool  OnKey           (WPARAM vk);
    void  Paint           (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme);

private:
    RECT          m_rect        = {};
    std::wstring  m_label;
    wchar_t       m_accelerator = 0;
    ClickFn       m_click;
    bool          m_hover       = false;
    bool          m_pressed     = false;
    bool          m_focused     = false;
    bool          m_enabled     = true;
    bool          m_visible     = true;
    bool          m_emphasis    = false;
    Variant       m_variant     = Variant::Default;
    DpiScaler     m_scaler;
};
