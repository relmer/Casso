#pragma once

#include "Devices/Tape/TapeDeck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTurboGovernor
//
//  Whether a tape load should run at Maximum speed right now. Only while the
//  deck is moving AND the guest has touched the cassette port within the last
//  tenth of a second of emulated time. Reading $C060 alone is not enough: on
//  the //e it is also how a game reads a fourth button, and a game polling it
//  with no tape moving must keep its normal speed.
//
////////////////////////////////////////////////////////////////////////////////

class TapeTurboGovernor
{
public:
    static bool  ShouldRunAtMaximum (bool             isPreferenceOn,
                                     const TapeDeck & deck,
                                     uint64_t         nowCycle,
                                     double           cpuClockHz);

    static constexpr double  kAccessWindowSeconds = 0.1;
    static constexpr double  kLeaderWaitSeconds   = 4.0;    // past the Monitor's 3.5 s wait over a leader
};
