#pragma once

#include "Pch.h"
#include "Core/MachineConfig.h"
#include "Devices/Disk/IDiskImage.h"





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
//  recenter) the way a real paddle's dial does. Mouse (mouse-capable
//  machines only — the //c) is NON-capturing: while the host cursor is
//  over the emulator viewport its position maps absolutely onto the guest
//  mouse (host cursor hidden there); leaving the viewport releases to the
//  host. Cycled Off -> Joystick -> Paddle [-> Mouse] -> Off from the
//  drive-bar widget and the Machine menu.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputMappingMode
{
    Off,
    Joystick,
    Paddle,
    Mouse
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





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldEnableWriteProtectMenuItem
//
//  Pure helper that returns true iff the Disk menu's write-protect item
//  should be clickable for a bay: something has to be mounted, and it must
//  not be an image whose stored checksum failed to match its contents.
//
//  A damaged image is excluded because the toggle refuses it. Changing that
//  flag means patching the file and recomputing its header checksum, and that
//  checksum failing to match IS the evidence of damage -- so the one write
//  that is otherwise harmless is the one that would destroy the proof. The
//  refusal explains itself, but an item that always refuses should not be
//  offered in the first place.
//
//  Takes the whole WriteProtectInfo rather than a lone bool so a later cause
//  that also makes the toggle meaningless can join without changing callers,
//  and so the call site reads as a question about write protection.
//
//  Inline so the headless UnitTest project can exercise the decision without
//  pulling in any Win32 dependencies.
//
////////////////////////////////////////////////////////////////////////////////

inline bool ShouldEnableWriteProtectMenuItem (
    bool                      isMounted,
    const WriteProtectInfo &  wp) noexcept
{
    return isMounted && !wp.checksumMismatch;
}
