#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "LedIndicator.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../../UiCommandTypes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  JoystickToggleButton
//
//  A cycling tri-state control in the bottom drive bar that mirrors the
//  "Cycle Input Mode" machine command (Off -> Joystick -> Paddle). It is
//  frameless unless hovered, focused, or pressed, and carries a glowing
//  LED (left of the label) that is dark in Off and lit blue in Joystick
//  and Paddle. Off and Joystick share the "Joystick Mode" label (the LED
//  distinguishes them); Paddle widens the frame to show "Paddle Mode (ESC
//  to exit)" so the captured-mouse escape stays visible the whole time.
//  The owner drives state (mode / hovered / focused / pressed) and the
//  actual cycle through the EmulatorShell input-mode path, relaying out
//  on each mode change since the frame width tracks the current label.
//
////////////////////////////////////////////////////////////////////////////////

class JoystickToggleButton
{
public:
    void                  Layout       (int centerXPx,
                                         int centerYPx,
                                         UINT dpi,
                                         DwriteTextRenderer * pText);
    void                  Hide         ()                         { m_bounds = {}; }
    void                  SetMode      (InputMappingMode mode)    { m_mode    = mode; }
    InputMappingMode      GetMode      () const                   { return m_mode; }
    void                  SetHovered   (bool hovered)             { m_hovered = hovered; }
    bool                  IsHovered    () const                   { return m_hovered; }
    void                  SetFocused   (bool focused)             { m_focused = focused; }
    bool                  IsFocused    () const                   { return m_focused; }
    void                  SetPressed   (bool pressed)             { m_pressed = pressed; }
    bool                  HitTest      (int x, int y) const;
    RECT                  Bounds       () const                   { return m_bounds; }
    void                  Paint        (DxUiPainter        & painter,
                                         DwriteTextRenderer & text,
                                         const ChromeTheme  & theme) const;

    static const wchar_t * LabelFor    (InputMappingMode mode);
    static const wchar_t * TooltipText ();

private:
    RECT              m_bounds  = {};
    LedIndicator      m_led;
    UINT              m_dpi     = 96;
    InputMappingMode  m_mode    = InputMappingMode::Off;
    bool              m_hovered = false;
    bool              m_focused = false;
    bool              m_pressed = false;
};
