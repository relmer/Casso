#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle
//
//  Win11-style pill toggle. Functionally a DxuiCheckbox but renders as a
//  horizontal pill with a circular thumb that slides between off
//  (thumb left, neutral pill) and on (thumb right, accent pill).
//  DxuiLabel, when set, paints to the right of the pill.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToggle : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (bool checked)>;

    DxuiToggle() { m_focusable = true; }
    ~DxuiToggle() override = default;

    void  SetRect    (const RECT & rect)  { SetBounds (rect); }
    void  SetLabel   (const std::wstring & label) { m_label = label; }
    void  SetChecked (bool checked) { m_checked = checked; }
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetDpi      (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect    () const { return m_boundsDip;    }
    const std::wstring & Label   () const { return m_label;   }
    bool                 Checked () const { return m_checked; }
    bool                 Enabled () const { return m_enabled; }
    bool                 Focused () const { return m_focused; }
    bool                 Hover   () const { return m_hover;   }
    bool                 Pressed () const { return m_pressed; }

    bool  HitTest       (int x, int y) const;
    void  SetMouseHover (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    bool  OnKey         (WPARAM vk);
    void  Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override { return m_label; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Checkbox; }

private:
    void  PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb, uint32_t focusArgb) const;
    void  Flip ();
    std::wstring  m_label;
    ChangeFn      m_change;
    bool          m_checked = false;
    bool          m_enabled = true;
    bool          m_focused = false;
    bool          m_hover   = false;
    bool          m_pressed = false;
    DxuiDpiScaler     m_scaler;
};
