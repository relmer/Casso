#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Checkbox
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

class Checkbox
{
public:
    using ChangeFn = std::function<void (bool checked)>;

    void  SetRect    (const RECT & rect)  { m_rect = rect; }
    void  SetLabel   (const std::wstring & label) { m_label = label; }
    void  SetChecked (bool checked) { m_checked = checked; }
    void  SetEnabled (bool enabled) { m_enabled = enabled; if (!enabled) { m_hover = false; m_pressed = false; } }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetDpi      (UINT dpi) { m_scaler.SetDpi (dpi); }

    const RECT         & Rect     () const { return m_rect;     }
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
    void  Paint           (DxUiPainter & painter, DwriteTextRenderer & text) const;

private:
    void  Toggle ();


    RECT          m_rect    = {};
    std::wstring  m_label;
    ChangeFn      m_change;
    bool          m_checked = false;
    bool          m_enabled = true;
    bool          m_focused = false;
    bool          m_hover   = false;
    bool          m_pressed = false;
    DpiScaler     m_scaler;
};
