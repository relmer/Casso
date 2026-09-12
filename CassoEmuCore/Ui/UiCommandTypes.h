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
//  should be clickable for a bay: something has to be mounted, it must not be
//  an image whose stored checksum failed to match its contents, and the file
//  must not be closed to writes for a reason the command cannot change.
//
//  Two of the five causes disable it:
//
//    checksumMismatch  changing the WOZ flag rewrites the header checksum,
//                      and that checksum failing to match IS the evidence of
//                      damage -- so the one write that is otherwise harmless
//                      is the one that would destroy the proof.
//    noPermission      an ACL denial or an exclusive lock; the command
//                      changes neither, so every write it tries would fail.
//
//  The other three do not. The image flag and the read-only attribute are
//  the two things the command toggles, so either one being set is exactly
//  when a user reaches for it -- disabling on the attribute once left a disk
//  the command had just write-protected with no way back. The drive
//  preference in Settings is a separate control.
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
    return isMounted && !wp.checksumMismatch && !wp.noPermission;
}
