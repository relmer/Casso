#include "Pch.h"

#include "SettingsApplyAdapter.h"

#include "../../EmulatorShell.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsApplyAdapter  -- ISettingsApplySink over the EmulatorShell queue
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplySpeedMode (SettingsSpeedMode mode)
{
    WORD  id = IDM_MACHINE_SPEED_1X;



    switch (mode)
    {
        case SettingsSpeedMode::Authentic: id = IDM_MACHINE_SPEED_1X;  break;
        case SettingsSpeedMode::Double:    id = IDM_MACHINE_SPEED_2X;  break;
        case SettingsSpeedMode::Maximum:   id = IDM_MACHINE_SPEED_MAX; break;
    }
    PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyColorMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyColorMode (SettingsColorMode mode)
{
    WORD  id = IDM_VIEW_COLOR;



    switch (mode)
    {
        case SettingsColorMode::Color: id = IDM_VIEW_COLOR; break;
        case SettingsColorMode::Green: id = IDM_VIEW_GREEN; break;
        case SettingsColorMode::Amber: id = IDM_VIEW_AMBER; break;
        case SettingsColorMode::White: id = IDM_VIEW_WHITE; break;
    }
    PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyFloppySound
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyFloppySound (bool enabled)
{
    m_shell.PostCommand (enabled ? IDM_AUDIO_DRIVE_ENABLE
                                 : IDM_AUDIO_DRIVE_DISABLE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMechanism
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyMechanism (const std::string & mechanism)
{
    m_shell.PostCommand (IDM_AUDIO_DRIVE_MECHANISM, mechanism);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDriveVolumes
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyDriveVolumes (float motor, float head, float door)
{
    char  payload[32] = {};

    sprintf_s (payload, "%d,%d,%d",
               (int) std::lround (motor * 100.0f),
               (int) std::lround (head  * 100.0f),
               (int) std::lround (door  * 100.0f));
    m_shell.PostCommand (IDM_AUDIO_DRIVE_VOLUMES, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDrivePan
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyDrivePan (float driveOnePan, float driveTwoPan)
{
    char  payload[32] = {};

    sprintf_s (payload, "%d,%d",
               (int) std::lround (driveOnePan * 100.0f),
               (int) std::lround (driveTwoPan * 100.0f));
    m_shell.PostCommand (IDM_AUDIO_DRIVE_PAN, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyWriteProtect (int drive, bool wp)
{
    // Route through the CPU-thread command queue (like insert /
    // eject) so the mounted DiskImage's write-protect flag -- read
    // by the controller on that thread -- is mutated there. The
    // command id encodes the drive; the payload carries the bool.
    WORD  id = (drive == 0) ? IDM_DISK_WRITEPROTECT1
                            : IDM_DISK_WRITEPROTECT2;

    if (drive == 0 || drive == 1)
    {
        m_shell.PostCommand (id, wp ? std::string ("1") : std::string ("0"));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyExternalDriveConnected
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyExternalDriveConnected (bool connected)
{
    // //c-only live effect: reveal/hide the second drive-mount widget.
    // Non-//c machines ignore the command (their second drive is fixed
    // hardware). Cheap + idempotent, so pushed on every Apply. Routed
    // via PostMessage(WM_COMMAND) -- NOT the CPU command queue -- so it
    // runs on the UI thread: it relays the chrome (menu bar + drive
    // band), which asserts UI-thread affinity. Mirrors ApplyColorMode.
    WORD  id = connected ? IDM_DRIVE_EXTERNAL_CONNECT
                         : IDM_DRIVE_EXTERNAL_DISCONNECT;
    PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMouseConnected
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyAdapter::ApplyMouseConnected (bool connected)
{
    // //c-only live effect: connect/disconnect the mouse
    // peripheral. UI-thread routed like the external drive.
    WORD  id = connected ? IDM_MOUSE_CONNECT : IDM_MOUSE_DISCONNECT;
    PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
}
