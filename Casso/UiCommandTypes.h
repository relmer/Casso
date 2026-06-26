#pragma once

#include "Pch.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorMode
//
//  Video output color treatment selected by the user. Wired from the
//  View menu / Settings panel into VideoOutput so the framebuffer is
//  re-shaded on the next frame.
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
//  Emulator pacing mode. Authentic (1x) targets the real //e clock;
//  Double (2x) runs at twice the rate for disk imaging; Maximum
//  spins the CPU thread as fast as the host allows.
//
////////////////////////////////////////////////////////////////////////////////

enum class SpeedMode
{
    Authentic,
    Double,
    Maximum
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputMappingMode
//
//  How host pointer / arrow input is mapped onto the emulated game port.
//  Off leaves the keys as ordinary //e keystrokes; Joystick maps the
//  arrow keys (plus Z / X) onto the paddle axes and fire buttons with a
//  spring return to center on release; Paddle captures the mouse and maps
//  relative motion onto the paddle axes, holding the last position (no
//  recenter) the way a real paddle's dial does. Cycled Off -> Joystick ->
//  Paddle -> Off from the drive-bar widget and the Machine menu.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputMappingMode
{
    Off,
    Joystick,
    Paddle
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldEnableDisk2DebugMenuItem
//
//  Pure helper that returns true iff the active MachineConfig wires
//  at least one Disk II controller (any slot). Inline so the headless
//  UnitTest project can exercise the decision without pulling in any
//  Win32 dependencies.
//
////////////////////////////////////////////////////////////////////////////////

inline bool ShouldEnableDisk2DebugMenuItem (const MachineConfig & config) noexcept
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
