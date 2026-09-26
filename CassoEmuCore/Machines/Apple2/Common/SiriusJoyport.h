#pragma once

#include "Pch.h"
#include "Controllers/ControllerTypes.h"

class AppleSoftSwitchBank;





////////////////////////////////////////////////////////////////////////////////
//
//  SiriusJoyport
//
//  The Sirius Software Joyport in Atari mode, Controller Select at Center:
//  two Atari joysticks multiplexed onto the three pushbutton inputs. AN0
//  picks the jack (off = left, player 1), AN1 picks the pair of directions
//  (off = left/right, on = up/down); PB0 is fire, PB1 left or up, PB2 right
//  or down. The lines are active low, so an open switch reads bit 7 set.
//
//  Not a bus device. The device that answers $C061-$C063 (the ][/][+ game
//  port, or the //e keyboard) asks TryReadButton first and answers as it
//  always has when that declines: while detached, and for kReleaseCycles
//  after every reset, so the //e firmware's reset-time reads of Open Apple and
//  Closed Apple see the keys rather than the Joyport's idle lines, which read
//  as both held down and would send every reset into the self-test.
//
//  Threads: the UI thread attaches it and writes the jacks; the CPU thread
//  reads. Both cross through atomics.
//
////////////////////////////////////////////////////////////////////////////////

class SiriusJoyport
{
public:
    static constexpr uint64_t  kReleaseCycles = 500'000;

    void  SetAnnunciatorSource (const AppleSoftSwitchBank * bank) { m_annunciatorSource = bank; }
    void  SetCycleSource       (const uint64_t * totalCycles)     { m_cycleSource       = totalCycles; }

    void  SetAttached     (bool attached);
    bool  IsAttached      () const;
    void  SetJackSwitches (size_t jack, JoystickSwitches switches);

    void  OnMachineReset();

    bool  TryReadButton    (int index, Byte & value) const;
    bool  IsDrivingPaddles () const;

private:
    static constexpr int   kAnnunciatorJack      = 0;
    static constexpr int   kAnnunciatorDirection = 1;
    static constexpr int   kButtonFire           = 0;
    static constexpr int   kButtonLeftOrUp       = 1;
    static constexpr int   kButtonCount          = 3;
    static constexpr Byte  kSwitchOpen           = 0x80;
    static constexpr Byte  kSwitchClosed         = 0x00;

    // Each jack's JoystickSwitches bits, as the UI thread last wrote them.
    using JackStore = std::array<atomic<unsigned long>, JoyportJacks::kJackCount>;

    bool            IsInResetWindow   () const;
    bool            IsAnnunciatorOn   (int index) const;
    JoystickSwitch  GetSelectedSwitch (int index) const;

    const AppleSoftSwitchBank  * m_annunciatorSource = nullptr;
    const uint64_t             * m_cycleSource       = nullptr;
    atomic<bool>                 m_isAttached        { false };
    JackStore                    m_jacks             {};
    uint64_t                     m_resetCycle        = 0;
    bool                         m_hasResetStamp     = false;
};
