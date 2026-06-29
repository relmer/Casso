#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCheckbox
//
//  Pure-logic checkable control. Tracks the four user-visible states
//  spec'd for the settings panel:
//      * enabled / disabled
//      * checked / unchecked
//      * hover / no hover
//      * pressed / not pressed
//      * focused / not focused
//
//  Keyboard contract:
//      Space   -> toggle (only when enabled + focused)
//      Enter   -> toggle (same)
//
//  Mouse contract:
//      LButtonDown over hit-rect            -> set pressed
//      LButtonUp   over hit-rect while pressed -> toggle
//      LButtonUp   outside hit-rect            -> cancel press
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCheckbox : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (bool checked)>;

    ~DxuiCheckbox() override = default;

    void  SetRect    (const RECT & rect)  { SetBounds (rect); }
    void  SetLabel   (const std::wstring & label) { m_label = label; }
    void  SetChecked (bool checked) { m_checked = checked; }
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetDpi      (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect     () const { return m_boundsDip;     }
    const std::wstring & Label    () const { return m_label;    }
    bool                 Checked  () const { return m_checked;  }
    bool                 Enabled  () const { return m_enabled;  }
    bool                 Focused  () const { return m_focused;  }
    bool                 Hover    () const { return m_hover;    }
    bool                 Pressed  () const { return m_pressed;  }

    bool  HitTest         (int x, int y) const;
    void  SetMouseHover   (int x, int y);
    bool  OnLButtonDown   (int x, int y);
    bool  OnLButtonUp     (int x, int y);
    bool  OnKey           (WPARAM vk);

    //
    //  IDxuiControl overrides — additive shims so DxuiCheckbox slots
    //  into DxuiPanel trees alongside the rest of the chrome.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override { return m_label; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Checkbox; }

private:
    void  Toggle ();


    std::wstring  m_label;
    ChangeFn      m_change;
    bool          m_checked = false;
    bool          m_enabled = true;
    bool          m_focused = false;
    bool          m_hover   = false;
    bool          m_pressed = false;
    DxuiDpiScaler     m_scaler;
};
