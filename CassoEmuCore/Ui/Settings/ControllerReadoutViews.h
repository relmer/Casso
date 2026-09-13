#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StickPositionView
//
//  Where the game port's joystick is, drawn as a dot inside a circle. The
//  horizontal axis is PDL0 and the vertical PDL1, each labeled with its
//  value, so a user moving a stick sees what the guest will read rather than
//  a pair of numbers to picture.
//
//  0 is left and up and 255 is right and down, the way an Apple II paddle
//  reads. An axis nothing drives rests the dot at center.
//
////////////////////////////////////////////////////////////////////////////////

class StickPositionView : public IDxuiControl
{
public:

    void  SetValues (Byte pdl0, Byte pdl1);
    void  SetActive (bool isActive);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler  m_scaler;
    Byte           m_pdl0     = 127;
    Byte           m_pdl1     = 127;
    bool           m_isActive = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ButtonLightView
//
//  One pushbutton's state: a ring that fills while the guest reads the
//  button pressed.
//
////////////////////////////////////////////////////////////////////////////////

class ButtonLightView : public IDxuiControl
{
public:

    void  SetLit    (bool isLit);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler  m_scaler;
    bool           m_isLit = false;
};
