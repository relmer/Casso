#pragma once

#include "Pch.h"

class SettingsSheet;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDriveAudition
//
//  The drive-sound audition behind the Machine page's (>) buttons: the
//  staged volumes, pan and mechanism are pushed to the engine and a one-shot
//  test of the sound is posted. The values the dialog opened with are saved
//  first, so a cancel puts the mixer back after an audition that was never
//  committed. OK persists the staged values through the normal apply path.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsDriveAudition
{
public:
    explicit SettingsDriveAudition (SettingsSheet & sheet) : m_sheet (sheet) {}

    void  Audition         (int drive, int kind, bool centered);
    void  SnapshotBaseline ();
    void  RevertIfDirty    ();

private:
    //  The mechanism is reloaded only when it differs from what the engine
    //  last loaded, avoiding a redundant WAV reload.
    void  Push (float motor, float head, float door, float pan0, float pan1, const std::string & mechanism);

    SettingsSheet  & m_sheet;
    std::string      m_lastMechanism;
    bool             m_isDirty           = false;
    float            m_baselineMotorVol  = 0.0f;
    float            m_baselineHeadVol   = 0.0f;
    float            m_baselineDoorVol   = 0.0f;
    float            m_baselineOnePan    = 0.0f;
    float            m_baselineTwoPan    = 0.0f;
    std::string      m_baselineMechanism;
};
