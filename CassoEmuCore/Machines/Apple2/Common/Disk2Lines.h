#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Drive.h"

class Disk2Recorder;
class Disk2Signals;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines
//
//  The lines between a Disk II controller and its drives that both front
//  ends set: the phase magnets, the motor, the drive select, and Q6 and Q7.
//  The //c's IWM has a separate copy of the select, Q6 and Q7 and sets these
//  after each access, so diagnostics and the saved state read one place in
//  either mode.
//
//  A DRIVE'S STEPPER WORKS ONLY WHILE THE DRIVE IS ENABLED: ENABLE' low,
//  which is the motor running, its spin-down included (Sather, Understanding
//  the Apple IIe, chapter 9, "The Drive ENABLE' Input"). The phases are
//  latched either way, so the ones on when the drive is next enabled pull
//  its head then (GH #176).
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Lines
{
public:
    Disk2Lines (Disk2Drives & drives, Disk2Recorder & recorder, Disk2Signals & signals);

    Disk2Lines (const Disk2Lines &)             = delete;
    Disk2Lines & operator= (const Disk2Lines &) = delete;

    void  SetPhase    (int phase, bool isOn);
    void  SetPhases   (uint8_t phases);
    void  PullHead    ();
    void  SelectDrive (int drive)  { m_activeDrive = drive; }
    void  SetMotorOn  (bool isOn)  { m_motorOn = isOn; }
    void  SetQ6       (bool isOn)  { m_q6 = isOn; }
    void  SetQ7       (bool isOn)  { m_q7 = isOn; }
    void  Reset       ();

    uint8_t        GetPhases       () const { return m_phases; }
    int            GetPhase        () const { return m_phase; }
    bool           IsMotorOn       () const { return m_motorOn; }
    int            GetActiveDrive  () const { return m_activeDrive; }
    bool           IsQ6            () const { return m_q6; }
    bool           IsQ7            () const { return m_q7; }
    int            GetQuarterTrack () const { return m_drives[(size_t) m_activeDrive].GetQuarterTrack(); }
    int            GetCurrentTrack () const { return GetQuarterTrack() / kQuarterTracksPerTrack; }

    Disk2Drive        & GetActive()       { return m_drives[(size_t) m_activeDrive]; }
    const Disk2Drive  & GetActive() const { return m_drives[(size_t) m_activeDrive]; }

    //  The fields as a saved state has them, already checked.
    void  Restore (uint8_t phases, int phase, bool isMotorOn, int activeDrive, bool q6, bool q7);

private:
    static constexpr int  kQuarterTracksPerTrack = 4;

    Disk2Drives    & m_drives;
    Disk2Recorder  & m_recorder;
    Disk2Signals   & m_signals;

    uint8_t          m_phases      = 0;
    int              m_phase       = 0;
    bool             m_motorOn     = false;
    int              m_activeDrive = 0;
    bool             m_q6          = false;
    bool             m_q7          = false;
};
