#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "LedIndicator.h"




////////////////////////////////////////////////////////////////////////////////
//
//  JoystickToggleButton
//
//  A checkbox-style toggle in the bottom drive bar that mirrors the
//  "Map Arrows to Joystick" machine command. It is frameless unless
//  hovered, focused, or pressed, and carries a blue glowing LED
//  (left of the label) that lights when the mode is enabled. The
//  owner drives state (on / hovered / focused / pressed) and the
//  actual toggle through the existing SetMapArrowsToJoystick path.
//
////////////////////////////////////////////////////////////////////////////////

class JoystickToggleButton
{
public:
    void                  Layout       (int centerXPx,
                                         int centerYPx,
                                         UINT dpi,
                                         DxuiTextRenderer * pText);
    void                  Hide         ()              { m_bounds = {}; }
    void                  SetOn        (bool on)       { m_on      = on; }
    bool                  IsOn         () const        { return m_on; }
    void                  SetHovered   (bool hovered)  { m_hovered = hovered; }
    bool                  IsHovered    () const        { return m_hovered; }
    void                  SetFocused   (bool focused)  { m_focused = focused; }
    bool                  IsFocused    () const        { return m_focused; }
    void                  SetPressed   (bool pressed)  { m_pressed = pressed; }
    bool                  HitTest      (int x, int y) const;
    RECT                  Bounds       () const        { return m_bounds; }
    void                  Paint        (DxuiPainter        & painter,
                                         DxuiTextRenderer & text,
                                         const ChromeTheme  & theme) const;

    static const wchar_t * TooltipText ();

private:
    RECT          m_bounds  = {};
    LedIndicator  m_led;
    UINT          m_dpi     = 96;
    bool          m_on      = false;
    bool          m_hovered = false;
    bool          m_focused = false;
    bool          m_pressed = false;
};
