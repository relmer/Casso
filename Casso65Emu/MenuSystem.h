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
    HMENU       m_menuBar     = nullptr;
    HMENU       m_fileMenu    = nullptr;
    HMENU       m_machineMenu = nullptr;
    HMENU       m_diskMenu    = nullptr;
    HMENU       m_viewMenu    = nullptr;
    HMENU       m_helpMenu    = nullptr;

    SpeedMode   m_speedMode = SpeedMode::Authentic;
    ColorMode   m_colorMode = ColorMode::Color;

    HWND        m_hwnd = nullptr;
};
