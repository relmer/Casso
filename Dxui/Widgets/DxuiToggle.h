#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle
//
//  Win11-style pill toggle. Functionally a DxuiCheckbox but renders as a
//  pill with a circular thumb that slides between off (neutral pill) and on
//  (accent pill). The pill is horizontal by default, the thumb traveling
//  right when it is turned on; SetOnDirection stands it on end, the thumb
//  traveling up or down instead, for a control drawn as a physical switch.
//  The label, when set, paints to the right of the pill.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToggle : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (bool checked)>;

    // The end the thumb travels to when the toggle is turned on.
    enum class OnDirection
    {
        Right,
        Up,
        Down,
    };

    // The pill as painted: the track is the whole pill, its long side along
    // the direction of travel, with end caps of capRadius; the thumb is a
    // circle at one end of it.
    struct TrackAndThumb
    {
        D2D1_RECT_F    track       = {};
        float          capRadius   = 0.0f;
        D2D1_POINT_2F  thumbCenter = {};
        float          thumbRadius = 0.0f;
    };

    static TrackAndThumb  ComputeTrackAndThumb (const D2D1_RECT_F & pill, OnDirection direction, bool checked);

    DxuiToggle() { m_focusable = true; }
    ~DxuiToggle() override = default;

    void  SetRect    (const RECT & rect)  { SetBounds (rect); }
    void  SetLabel   (const std::wstring & label) { m_label = label; }
    void  SetChecked (bool checked) { m_checked = checked; }
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetDpi      (UINT dpi) { m_scaler.SetDpi (dpi); }

    // Which way the thumb travels when turned on; Right by default.
    void         SetOnDirection (OnDirection direction) { m_onDirection = direction; }
    OnDirection  GetOnDirection () const                { return m_onDirection; }

    const RECT         & GetRect   () const { return m_boundsDip;    }
    const std::wstring & GetLabel  () const { return m_label;   }
    bool                 IsChecked () const { return m_checked; }
    bool                 IsEnabled () const { return m_enabled; }
    bool                 IsFocused () const { return m_focused; }
    bool                 IsHovered () const { return m_hover;   }
    bool                 IsPressed () const { return m_pressed; }

    bool  HitTest       (int x, int y) const;
    void  SetMouseHover (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    bool  OnKey         (WPARAM vk);
    void  Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { SetFocused (focused); }
    std::wstring        GetAccessibleName () const override { return m_label; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Checkbox; }

private:
    static constexpr uint32_t  kDefaultAccentArgb = 0xFF2D7CDB;   // "on" pill
    static constexpr uint32_t  kDefaultFocusArgb  = 0xFFAACCFF;   // focus ring
    static constexpr float     kHoverLighten      = 1.15f;        // "on" pill hover brighten

    void  PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb, uint32_t focusArgb) const;
    void  Flip ();
    std::wstring   m_label;
    ChangeFn       m_change;
    bool           m_checked     = false;
    bool           m_enabled     = true;
    bool           m_focused     = false;
    bool           m_hover       = false;
    bool           m_pressed     = false;
    OnDirection    m_onDirection = OnDirection::Right;
    DxuiDpiScaler  m_scaler;
};
