#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider
//
//  Continuous-value slider. Stores a float value in [min, max] plus
//  an optional discrete step; the slider quantizes to the step on
//  every set. Hit-testing is on the track + thumb rect (track gets a
//  modest vertical extent for easier mouse aim).
//
//  Keyboard contract:
//      Left  / Down     -> -1 step
//      Right / Up       -> +1 step
//      PageDown         -> -10 steps
//      PageUp           -> +10 steps
//      Home             -> min
//      End              -> max
//
//  Mouse contract:
//      LButtonDown on track    -> jump to point + begin drag
//      MouseMove   while drag  -> follow cursor
//      LButtonUp               -> end drag
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSlider : public IDxuiControl
{
public:
    using ChangeFn      = std::function<void (float value)>;
    using InteractionFn = std::function<void ()>;

    ~DxuiSlider() override = default;

    void   SetRect    (const RECT & rect) { SetBounds (rect); }
    void   SetRange   (float minValue, float maxValue);
    void   SetStep    (float step) { m_step = step; }
    void   SetValue   (float value);
    void   SetSuffix  (const std::wstring & suffix) { m_suffix = suffix; m_explicitShowValue = true; m_showValue = true; }
    void   SetShowValue (bool show) { m_explicitShowValue = true; m_showValue = show; }
    void   SetDecimalPlaces (int places) { m_decimalPlaces = places; }
    void   SetShowTicks (bool show) { m_showTicks = show; }
    void   SetDpi     (UINT dpi) { m_scaler.SetDpi (dpi); }
    void   SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; if (!enabled) { m_dragging = false; m_hover = false; } }
    void   SetFocused (bool focused) { m_focused = focused; }
    void   SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void   SetOnDragStart (InteractionFn fn) { m_onDragStart = std::move (fn); }
    void   SetOnDragEnd   (InteractionFn fn) { m_onDragEnd   = std::move (fn); }
    void   SetOnKeyboardChange (InteractionFn fn) { m_onKeyboard = std::move (fn); }

    const RECT & Rect      () const { return m_boundsDip;     }
    float        Min       () const { return m_min;      }
    float        Max       () const { return m_max;      }
    float        Step      () const { return m_step;     }
    float        Value     () const { return m_value;    }
    bool         Enabled   () const { return m_enabled;  }
    bool         Focused   () const { return m_focused;  }
    bool         Hover     () const { return m_hover;    }
    bool         Dragging  () const { return m_dragging; }

    bool   HitTest        (int x, int y) const;
    void   SetMouseHover  (int x, int y);
    bool   OnLButtonDown  (int x, int y);
    bool   OnLButtonUp    (int x, int y);
    bool   OnMouseMove    (int x, int y);
    bool   OnKey          (WPARAM vk);
    void   Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Slider; }

private:
    void   PaintInternal  (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb) const;
    void   ApplyValue     (float v);
    float  ValueFromX     (int x) const;
    ChangeFn       m_change;
    InteractionFn  m_onDragStart;
    InteractionFn  m_onDragEnd;
    InteractionFn  m_onKeyboard;
    std::wstring   m_suffix;
    DxuiDpiScaler      m_scaler;
    float          m_min      = 0.0f;
    float          m_max      = 1.0f;
    float          m_step     = 0.01f;
    float          m_value    = 0.0f;
    bool           m_enabled  = true;
    bool           m_focused  = false;
    bool           m_hover    = false;
    bool           m_dragging = false;
    bool           m_showTicks = true;
    bool           m_showValue = false;
    bool           m_explicitShowValue = false;
    int            m_decimalPlaces = 0;
};
