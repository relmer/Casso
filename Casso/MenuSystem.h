#pragma once

#include "Pch.h"
#include "Core/MachineConfig.h"





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

    // FR-001a. Re-evaluate runtime-driven menu state (currently just
    // the Disk II Debug item's enabled flag). Called from
    // EmulatorShell's WM_INITMENUPOPUP handler so a SwitchMachine
    // that swaps the controller in / out takes effect on the next
    // menu open.
    void UpdateDynamicMenuItems (const MachineConfig & config) noexcept;

    SpeedMode GetSpeedMode () const { return m_speedMode; }
    ColorMode GetColorMode () const { return m_colorMode; }

private:
    HMENU       m_menuBar     = nullptr;
    HMENU       m_fileMenu    = nullptr;
    HMENU       m_editMenu    = nullptr;
    HMENU       m_machineMenu = nullptr;
    HMENU       m_diskMenu    = nullptr;
    HMENU       m_viewMenu    = nullptr;
    HMENU       m_helpMenu    = nullptr;

    SpeedMode   m_speedMode = SpeedMode::Authentic;
    ColorMode   m_colorMode = ColorMode::Color;

    HWND        m_hwnd = nullptr;
};



////////////////////////////////////////////////////////////////////////////////
//
//  ShouldEnableDiskIIDebugMenuItem
//
//  FR-001a pure helper. Returns true iff the active MachineConfig
//  wires at least one Disk II controller (any slot). Inline so the
//  headless UnitTest project can exercise the decision without
//  having to compile MenuSystem.cpp (which would drag Win32 menu
//  APIs into the test binary).
//
////////////////////////////////////////////////////////////////////////////////

inline bool ShouldEnableDiskIIDebugMenuItem (const MachineConfig & config) noexcept
{
    for (const SlotConfig & slot : config.slots)
    {
        if (slot.device == "disk-ii")
        {
            return true;
        }
    }

    return false;
}





