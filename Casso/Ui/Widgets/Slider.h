#pragma once

#include "Pch.h"

#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Slider
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

class Slider
{
public:
    using ChangeFn = std::function<void (float value)>;

    void   SetRect    (const RECT & rect) { m_rect = rect; }
    void   SetRange   (float minValue, float maxValue);
    void   SetStep    (float step) { m_step = step; }
    void   SetValue   (float value);
    void   SetEnabled (bool enabled) { m_enabled = enabled; if (!enabled) { m_dragging = false; m_hover = false; } }
    void   SetFocused (bool focused) { m_focused = focused; }
    void   SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    const RECT & Rect      () const { return m_rect;     }
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
    void   Paint          (DxUiPainter & painter, DwriteTextRenderer & text) const;

private:
    void   ApplyValue     (float v);
    float  ValueFromX     (int x) const;


    RECT      m_rect     = {};
    ChangeFn  m_change;
    float     m_min      = 0.0f;
    float     m_max      = 1.0f;
    float     m_step     = 0.01f;
    float     m_value    = 0.0f;
    bool      m_enabled  = true;
    bool      m_focused  = false;
    bool      m_hover    = false;
    bool      m_dragging = false;
};
