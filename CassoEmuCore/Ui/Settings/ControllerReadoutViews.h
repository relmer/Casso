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

    // What to call the two axes. They are the controller's own PDL0 and PDL1
    // until two people play, when a player's paddles are whichever the machine
    // gave their slot -- PDL2 and PDL3 for the second joystick. An empty
    // second label is an axis this player does not drive.
    void  SetAxisLabels (const std::wstring & horizontal, const std::wstring & vertical);

    void  Layout    (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:

    DxuiDpiScaler  m_scaler;
    Byte           m_pdl0       = 127;
    Byte           m_pdl1       = 127;
    bool           m_isActive   = false;
    std::wstring   m_horizontal = L"PDL0";
    std::wstring   m_vertical   = L"PDL1";
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
