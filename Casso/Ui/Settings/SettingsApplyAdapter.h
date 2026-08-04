#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"


class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsApplyAdapter
//
//  Bridges the pure-logic ISettingsApplySink contract into the
//  EmulatorShell command queue. Live-effect fields post commands so the
//  audio mixer / CRT pipeline picks them up on the next CPU tick;
//  QueueMachineReset is recorded rather than applied, and consumed by the
//  modal confirm path in SettingsPanel.
//
//  Routing is not uniform, and the split matters: audio and disk state go
//  through PostCommand (CPU thread, where the mixer and the mounted
//  DiskImage live), while anything that relays chrome goes through
//  PostMessage(WM_COMMAND) so it runs on the UI thread that asserts
//  affinity. Each method below says which and why.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsApplyAdapter : public ISettingsApplySink
{
public:
    explicit SettingsApplyAdapter (EmulatorShell & shell)
        : m_shell (shell)
    {
    }

    void ApplySpeedMode              (SettingsSpeedMode mode)                override;
    void ApplyColorMode              (SettingsColorMode mode)                override;
    void ApplyFloppySound            (bool enabled)                          override;
    void ApplyMechanism              (const std::string & mechanism)         override;
    void ApplyDriveVolumes           (float motor, float head, float door)   override;
    void ApplyDrivePan               (float driveOnePan, float driveTwoPan)  override;
    void ApplyWriteProtect           (int drive, bool wp)                    override;
    void ApplyExternalDriveConnected (bool connected)                        override;
    void ApplyMouseConnected         (bool connected)                        override;
    void QueueMachineReset           ()                                      override { m_resetQueued = true; }

    bool ResetQueued () const { return m_resetQueued; }

private:
    EmulatorShell & m_shell;
    bool            m_resetQueued = false;
};
