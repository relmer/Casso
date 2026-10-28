#include "Pch.h"

#include "Ui/Settings/SettingsDriveAudition.h"

#include "Shell/EmulatorShell.h"
#include "Ui/Settings/SettingsSheet.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDriveAudition::Audition
//
//  Pushes the current drive-audio settings to the engine and fires a
//  one-shot test of the given sound, and marks the audition dirty so a
//  cancel restores the mixer to the dialog-open baseline.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDriveAudition::Audition (int drive, int kind, bool centered)
{
    char                     test[16] = {};
    float                    pan0     = 0.0f;
    float                    pan1     = 0.0f;
    const SettingsUiPrefs  & prefs    = m_sheet.m_state.GetPrefs();



    if (m_sheet.m_emuShell == nullptr)
    {
        return;
    }

    pan0 = prefs.driveOnePan;
    pan1 = prefs.driveTwoPan;
    if (centered)
    {
        if (drive == 0) { pan0 = 0.0f; }
        else            { pan1 = 0.0f; }
    }

    Push (prefs.driveMotorVolume,
          prefs.driveHeadVolume,
          prefs.driveDoorVolume,
          pan0,
          pan1,
          prefs.floppyMechanism);

    // The push above changed the live engine mixer; remember to undo it if the
    // dialog is canceled without persisting.
    m_isDirty = true;

    sprintf_s (test, "%d,%d", drive, kind);
    m_sheet.m_emuShell->PostCommand (IDM_AUDIO_DRIVE_TEST, test);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDriveAudition::SnapshotBaseline
//
//  The as-opened values, taken when the sheet opens.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDriveAudition::SnapshotBaseline()
{
    const SettingsUiPrefs &  prefs = m_sheet.m_state.GetPrefs();



    m_baselineMotorVol  = prefs.driveMotorVolume;
    m_baselineHeadVol   = prefs.driveHeadVolume;
    m_baselineDoorVol   = prefs.driveDoorVolume;
    m_baselineOnePan    = prefs.driveOnePan;
    m_baselineTwoPan    = prefs.driveTwoPan;
    m_baselineMechanism = prefs.floppyMechanism;
    m_isDirty           = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDriveAudition::RevertIfDirty
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDriveAudition::RevertIfDirty()
{
    if (!m_isDirty)
    {
        return;
    }

    Push (m_baselineMotorVol,
          m_baselineHeadVol,
          m_baselineDoorVol,
          m_baselineOnePan,
          m_baselineTwoPan,
          m_baselineMechanism);
    m_isDirty = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDriveAudition::Push
//
//  Posts volumes, pan and mechanism to the engine command queue.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDriveAudition::Push (
    float                motor,
    float                head,
    float                door,
    float                pan0,
    float                pan1,
    const std::string  & mechanism)
{
    char  vol[32] = {};
    char  pan[32] = {};



    if (m_sheet.m_emuShell == nullptr)
    {
        return;
    }

    sprintf_s (vol, "%d,%d,%d",
               (int) std::lround (motor * 100.0f),
               (int) std::lround (head  * 100.0f),
               (int) std::lround (door  * 100.0f));
    m_sheet.m_emuShell->PostCommand (IDM_AUDIO_DRIVE_VOLUMES, vol);

    if (mechanism != m_lastMechanism)
    {
        m_sheet.m_emuShell->PostCommand (IDM_AUDIO_DRIVE_MECHANISM, mechanism);
        m_lastMechanism = mechanism;
    }

    sprintf_s (pan, "%d,%d",
               (int) std::lround (pan0 * 100.0f),
               (int) std::lround (pan1 * 100.0f));
    m_sheet.m_emuShell->PostCommand (IDM_AUDIO_DRIVE_PAN, pan);
}
