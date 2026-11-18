#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Drive.h"
#include "Machines/Apple2/Common/Disk2LatchPeek.h"

class Disk2Clock;
class Disk2Lines;
class Disk2Recorder;
class Disk2Signals;
class IDiskPortHook;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2CardSequencer
//
//  The Disk II card's logic: its sixteen switches (phase magnets, motor,
//  drive select, Q6 and Q7), the data path they select, and the motor's
//  spin-up and spin-down. A true bit-stream controller: each drive's
//  Disk2NibbleEngine streams its disk at four CPU cycles a bit. Each access
//  first runs the selected drive on to the CPU's cycle (Disk2Clock).
//
////////////////////////////////////////////////////////////////////////////////

class Disk2CardSequencer
{
public:
    // Real Disk II: writing $C0E8 (motor off) starts a one-second spin-down,
    // so DOS RWTS can toggle the motor off between commands without losing
    // rotational sync (UTAIIe ch. 9, AppleWin SPINNING_CYCLES).
    static constexpr uint32_t  kMotorSpindownCycles = 1'000'000U;

    // The sequencer's stability window. For about $2EC cycles after the
    // motor's off to on edge the Logic State Sequencer hasn't latched a
    // stable nibble, so a read returns $80. Copy protection reads during
    // this window and checks for the absence of sync nibbles. Matches
    // AppleWin's MOTOR_ON_UNTIL_LSS_STABLE_CYCLES (GH#864); the range from
    // card to card is $2EC to $990. The bit cursor advances during the
    // window; only the latch the CPU reads is overridden.
    static constexpr uint32_t  kMotorSpinupCycles = 0x2EC;

    Disk2CardSequencer (Disk2Lines    & lines,
                        Disk2Drives   & drives,
                        Disk2Signals  & signals,
                        Disk2Recorder & recorder,
                        Disk2Clock    & clock);

    Disk2CardSequencer (const Disk2CardSequencer &)             = delete;
    Disk2CardSequencer & operator= (const Disk2CardSequencer &) = delete;

    Byte  Read    (int offset, IDiskPortHook * hook);
    void  Write   (int offset, Byte value, IDiskPortHook * hook);
    bool  TryPeek (int offset, Byte & value);
    void  Tick    (uint32_t cpuCycles);
    void  Reset   ();

    bool      IsMotorAtSpeed           () const;
    uint32_t  GetMotorSpinupRemaining  () const { return m_motorSpinupRemaining; }
    uint32_t  GetMotorSpindownCycles   () const { return m_motorSpindownCycles; }

    // How many times a program has accessed the switches since the card was
    // built. With the count the same and the motor stopped, no disk event
    // happened in between. Not saved: only ever compared on one machine.
    uint64_t  GetSwitchAccessCount     () const { return m_switchAccesses; }

    //  The counters as a saved state has them.
    void      Restore (uint32_t spindownCycles, uint32_t spinupRemaining);

private:
    bool  PollFloppyEnable      (IDiskPortHook * hook);
    void  CatchUpToCpu          ();
    void  HandleSwitch          (int offset);
    void  CommandMotorOff       ();
    void  CommandMotorOn        ();
    void  SelectDrive           (int drive);
    void  SetQ7                 (bool isOn);
    Byte  HandleReadDispatch    ();
    void  StopMotor             ();
    void  UpdateEngineSelection ();
    void  UpdateEngineWriteMode ();

    Disk2Drive  & GetActive();

    Disk2Lines     & m_lines;
    Disk2Drives    & m_drives;
    Disk2Signals   & m_signals;
    Disk2Recorder  & m_recorder;
    Disk2Clock     & m_clock;

    uint32_t         m_motorSpindownCycles  = 0;     // cycles left of the spin-down; the motor stops at 0
    uint32_t         m_motorSpinupRemaining = 0;     // cycles left of the stability window
    uint64_t         m_switchAccesses       = 0;
    Disk2LatchPeek   m_peek;
};
