#pragma once

#include "Pch.h"

#include "Devices/Floppy35/Iwm.h"
#include "Machines/Apple2/Common/Disk2Drive.h"
#include "Machines/Apple2/Common/Disk2IwmDriveBus.h"

class Disk2Clock;
class Disk2Lines;
class Disk2Recorder;
class Disk2Signals;
class IDiskPortHook;
class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort
//
//  The //c's disk port, an Integrated Woz Machine in place of a Disk II
//  card: the switches go to an Iwm, whose two drives are the controller's
//  drives (Disk2IwmDriveBus). After each access the IWM's drive select, Q6
//  and Q7 go to the controller's lines, and the motor runs while either
//  drive is enabled.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2IwmPort
{
public:
    Disk2IwmPort (Disk2Lines    & lines,
                  Disk2Drives   & drives,
                  Disk2Signals  & signals,
                  Disk2Recorder & recorder,
                  Disk2Clock    & clock);

    Disk2IwmPort (const Disk2IwmPort &)             = delete;
    Disk2IwmPort & operator= (const Disk2IwmPort &) = delete;

    Byte     Access    (int offset, bool isWrite, Byte value, IDiskPortHook * hook);
    bool     TryPeek   (int offset, const IDiskPortHook * hook, Byte & value) const;
    void     Tick      (uint32_t cpuCycles);
    void     Reset     ();
    void     SaveState (StateWriter & writer) const;
    HRESULT  LoadState (StateReader & reader);

    Iwm        & GetIwm ()       { return m_iwm; }
    const Iwm  & GetIwm () const { return m_iwm; }

private:
    uint64_t  GetNow        ();
    void      SyncMotor     ();
    void      ReportSwitch  (int offset);

    Disk2Lines                       & m_lines;
    Disk2Drives                      & m_drives;
    Disk2Signals                     & m_signals;
    Disk2Recorder                    & m_recorder;
    Disk2Clock                       & m_clock;
    Iwm                                m_iwm;
    std::array<Disk2IwmDriveBus, 2>    m_buses;
    uint64_t                           m_cycles = 0;
};
