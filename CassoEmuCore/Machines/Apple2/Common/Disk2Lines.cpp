#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Lines.h"
#include "Machines/Apple2/Common/Disk2Recorder.h"
#include "Machines/Apple2/Common/Disk2Signals.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::Disk2Lines
//
////////////////////////////////////////////////////////////////////////////////

Disk2Lines::Disk2Lines (Disk2Drives & drives, Disk2Recorder & recorder, Disk2Signals & signals) :
    m_drives   (drives),
    m_recorder (recorder),
    m_signals  (signals)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::SetPhase
//
//  Latches one phase magnet and, while the motor runs, moves the selected
//  drive's head to where the phases now pull it.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Lines::SetPhase (int phase, bool isOn)
{
    if (isOn)
    {
        m_phases = static_cast<uint8_t> (m_phases | (1 << phase));
        m_phase  = phase;
    }
    else
    {
        m_phases = static_cast<uint8_t> (m_phases & ~(1 << phase));
    }

    if (m_motorOn)
    {
        PullHead();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::SetPhases
//
//  The //c's IWM sets all four magnets of a drive at once: each one that
//  changed is set in turn, lowest first.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Lines::SetPhases (uint8_t phases)
{
    uint8_t  changed = static_cast<uint8_t> (phases ^ m_phases);
    int      phase   = 0;



    for (phase = 0; phase < Disk2Drive::kPhaseCount; phase++)
    {
        if ((changed & (1 << phase)) != 0)
        {
            SetPhase (phase, (phases & (1 << phase)) != 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::PullHead
//
//  Moves the selected drive's head to where the energized phases pull it.
//  A move goes to the address-mark watcher and the signals; the other
//  drive's head stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Lines::PullHead()
{
    Disk2HeadMove  move = GetActive().MoveHead (m_phases);



    if (!move.IsMoved())
    {
        return;
    }

    m_recorder.NoteHeadMoved (move.to);
    m_signals.OnHeadMoved (move);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Lines::Reset()
{
    Restore (0, 0, false, 0, false, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Lines::Restore
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Lines::Restore (uint8_t phases, int phase, bool isMotorOn, int activeDrive, bool q6, bool q7)
{
    m_phases      = phases;
    m_phase       = phase;
    m_motorOn     = isMotorOn;
    m_activeDrive = activeDrive;
    m_q6          = q6;
    m_q7          = q7;
}
