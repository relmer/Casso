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
    using FormatFn      = std::function<std::wstring (float value)>;

    DxuiSlider() { m_focusable = true; }
    ~DxuiSlider() override = default;

    void   SetRect    (const RECT & rect) { SetBounds (rect); }
    void   SetRange   (float minValue, float maxValue);
    void   SetStep    (float step) { m_step = step; }
    //  Granularity a mouse drag snaps to. 0 (default) reuses the keyboard step;
    //  set a finer value so dragging feels smooth on a slider whose keyboard
    //  step is deliberately coarse (e.g. a 0-200 step-10 brightness slider that
    //  should nudge by 10 on arrow keys but glide by 1 under the cursor).
    void   SetDragStep (float step) { m_dragStep = step; }
    void   SetValue   (float value);
    void   SetSuffix  (const std::wstring & suffix) { m_suffix = suffix; m_explicitShowValue = true; m_showValue = true; }
    void   SetShowValue (bool show) { m_explicitShowValue = true; m_showValue = show; }
    void   SetDecimalPlaces (int places) { m_decimalPlaces = places; }
    void   SetValueFormatter (FormatFn fn) { m_formatter = std::move (fn); m_explicitShowValue = true; m_showValue = true; }
    void   SetCenterOriginFill (bool centered) { m_centerOriginFill = centered; }
    void   SetShowTicks (bool show) { m_showTicks = show; }
    //  Spacing between tick marks in value units. 0 (default) draws one tick
    //  per drag step; set a coarser interval when the step is fine (e.g. a
    //  0-100 step-1 volume slider) so the ticks don't become a solid picket.
    void   SetTickInterval (float interval) { m_tickInterval = interval; }
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
    void   PaintInternal  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   ApplyValue         (float v);
    void   ApplyValueWithStep (float v, float step);
    float  DragStep       () const { return m_dragStep > 0.0f ? m_dragStep : m_step; }
    float  ValueFromX     (int x) const;
    ChangeFn       m_change;
    InteractionFn  m_onDragStart;
    InteractionFn  m_onDragEnd;
    InteractionFn  m_onKeyboard;
    FormatFn       m_formatter;
    std::wstring   m_suffix;
    DxuiDpiScaler      m_scaler;
    float          m_min      = 0.0f;
    float          m_max      = 1.0f;
    float          m_step     = 0.01f;
    float          m_dragStep = 0.0f;    // 0 => drag snaps to m_step
    float          m_value    = 0.0f;
    bool           m_enabled  = true;
    bool           m_focused  = false;
    bool           m_hover    = false;
    bool           m_dragging = false;
    bool           m_showTicks = true;
    float          m_tickInterval = 0.0f;   // 0 => one tick per step
    bool           m_showValue = false;
    bool           m_explicitShowValue = false;
    bool           m_centerOriginFill = false;
    int            m_decimalPlaces = 0;
};
