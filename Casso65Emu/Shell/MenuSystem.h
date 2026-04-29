#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorMode
//
////////////////////////////////////////////////////////////////////////////////

enum class ColorMode
{
    Color,
    GreenMono,
    AmberMono,
    WhiteMono
};





////////////////////////////////////////////////////////////////////////////////
//
//  SpeedMode
//
////////////////////////////////////////////////////////////////////////////////

enum class SpeedMode
{
    Authentic,  // 1x
    Double,     // 2x
    Maximum     // Unlimited
};





////////////////////////////////////////////////////////////////////////////////
//
//  MenuSystem
//
////////////////////////////////////////////////////////////////////////////////

class MenuSystem
{
public:
    MenuSystem ();

    HRESULT CreateMenuBar (HWND hwnd);

    void SetSpeedMode (SpeedMode mode);
    void SetColorMode (ColorMode mode);
    void SetPaused    (bool paused);

    SpeedMode GetSpeedMode () const { return m_speedMode; }
    ColorMode GetColorMode () const { return m_colorMode; }

private:
    HMENU       m_menuBar;
    HMENU       m_fileMenu;
    HMENU       m_machineMenu;
    HMENU       m_diskMenu;
    HMENU       m_viewMenu;
    HMENU       m_helpMenu;

    SpeedMode   m_speedMode;
    ColorMode   m_colorMode;

    HWND        m_hwnd;
};
