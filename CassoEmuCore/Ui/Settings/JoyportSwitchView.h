#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportSwitchView
//
//  One Atari joystick's five switches as the Joyport reads them: a D-pad cross
//  whose arms fill while their direction is closed, and a fire button beside
//  it that fills while fire is closed. The shape says which is which, so the
//  readout needs no key.
//
////////////////////////////////////////////////////////////////////////////////

class JoyportSwitchView : public IDxuiControl
{
public:

    void  SetSwitches (JoystickSwitches switches) { m_switches = switches; }
    void  SetActive   (bool isActive)             { m_isActive = isActive; }

    void  Layout      (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    void  PaintCell   (IDxuiTextRenderer & text, const IDxuiTheme & theme, float x, float y, float size, bool isLit) const;
    bool  IsClosed    (JoystickSwitch sw) const { return m_switches.test (static_cast<size_t> (sw)); }

    DxuiDpiScaler     m_scaler;
    JoystickSwitches  m_switches;
    bool              m_isActive = false;
};
