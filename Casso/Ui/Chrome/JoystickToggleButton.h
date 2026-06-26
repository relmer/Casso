#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "Core/IDxuiControl.h"
#include "LedIndicator.h"
#include "../../UiCommandTypes.h"




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
//  JoystickToggleButton is Casso-specific; its Paint assumes the
//  IDxuiTheme reference is actually a ChromeTheme and `static_cast`s
//  to read the button/link palette fields (a debug `dynamic_cast`
//  guard pins the contract).
//
////////////////////////////////////////////////////////////////////////////////

class JoystickToggleButton : public IDxuiControl
{
public:
    JoystickToggleButton  ();
    ~JoystickToggleButton () override = default;

    //
    //  Inject the text renderer used by Layout to measure the
    //  "Joystick Mode" label so the button frame sizes correctly. The
    //  renderer must outlive any subsequent Layout call. Pre-Initialize
    //  callers may pass nullptr; the layout falls back to a fixed-pitch
    //  estimate until a real renderer is wired.
    //
    void                  SetTextRenderer  (IDxuiTextRenderer * pText) { m_textRenderer = pText; }

    void                  Hide         ()              { m_bounds = {}; }
    void                  SetOn        (bool on)       { m_on      = on; }
    bool                  IsOn         () const        { return m_on; }

    // Tri-state input-mode adapter (ported minimally from master's paddle
    // work): Off shows dark/off, Joystick and Paddle both light the LED.
    // Full "Paddle Mode (ESC to exit)" labelling is a follow-up.
    void                  SetMode      (InputMappingMode mode) { m_on = (mode != InputMappingMode::Off); }
    void                  SetHovered   (bool hovered)  { m_hovered = hovered; }
    bool                  IsHovered    () const        { return m_hovered; }
    void                  SetFocused   (bool focused)  { m_focused = focused; }
    bool                  IsFocused    () const        { return m_focused; }
    void                  SetPressed   (bool pressed)  { m_pressed = pressed; }
    bool                  HitTest      (int x, int y) const;
    RECT                  Bounds       () const        { return m_bounds; }

    void                  Paint        (IDxuiPainter      & painter,
                                        IDxuiTextRenderer & text,
                                        const IDxuiTheme  & theme) override;

    //
    //  IDxuiControl::Layout — centers the button on the center of
    //  boundsDip and sizes the frame to the measured "Joystick Mode"
    //  label plus the LED and internal padding. The text renderer
    //  installed via SetTextRenderer is consulted for measurement;
    //  the button falls back to a fixed-pitch estimate when no
    //  renderer is wired. boundsDip.right / boundsDip.bottom only
    //  contribute the center coordinate; the final SetBounds is
    //  written to the computed frame rect.
    //
    void                  Layout       (const RECT          & boundsDip,
                                        const DxuiDpiScaler & scaler) override;

    static const wchar_t * TooltipText ();

private:
    RECT                  m_bounds        = {};
    LedIndicator          m_led;
    UINT                  m_dpi           = 96;
    bool                  m_on            = false;
    bool                  m_hovered       = false;
    bool                  m_focused       = false;
    bool                  m_pressed       = false;
    IDxuiTextRenderer *   m_textRenderer  = nullptr;
};
