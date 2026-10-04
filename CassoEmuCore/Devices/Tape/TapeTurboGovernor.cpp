#include "Pch.h"

#include "Devices/Tape/TapeTurboGovernor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTurboGovernor::ShouldRunAtMaximum
//
//  Ends by itself at the end of the tape, on stop or eject (the deck stops
//  moving), and once the guest goes a tenth of a second without touching the
//  port.
//
//  EXCEPT OVER A LEADER. The Monitor's READ finds the tape and then waits
//  three and a half seconds without touching it, the head over the leader
//  all the while, before it reads the record. That wait is dead time, so a
//  guest that read the tape within the last few seconds keeps the speed for
//  as long as a leader is playing. Past the last record's data no leader is
//  under the head, so whatever the load ran next starts at its own speed.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeTurboGovernor::ShouldRunAtMaximum (
    bool             isPreferenceOn,
    const TapeDeck & deck,
    uint64_t         nowCycle,
    double           cpuClockHz)
{
    TapeTransport  transport  = deck.GetTransport();
    bool           isMoving   = transport == TapeTransport::Playing || transport == TapeTransport::Recording;
    uint64_t       window     = (uint64_t) (cpuClockHz * kAccessWindowSeconds);
    uint64_t       waitWindow = (uint64_t) (cpuClockHz * kLeaderWaitSeconds);
    uint64_t       last       = deck.GetLastAccessCycle();
    bool           isAfter    = deck.HasBeenAccessed() && nowCycle >= last;
    bool           isRecent   = isAfter && nowCycle - last <= window;
    bool           isWaiting  = isAfter && nowCycle - last <= waitWindow &&
                                transport == TapeTransport::Playing && deck.IsOnLeader (nowCycle);



    return isPreferenceOn && isMoving && (isRecent || isWaiting);
}
