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
////////////////////////////////////////////////////////////////////////////////

bool TapeTurboGovernor::ShouldRunAtMaximum (
    bool             isPreferenceOn,
    const TapeDeck & deck,
    uint64_t         nowCycle,
    double           cpuClockHz)
{
    TapeTransport  transport = deck.GetTransport();
    bool           isMoving  = transport == TapeTransport::Playing || transport == TapeTransport::Recording;
    uint64_t       window    = (uint64_t) (cpuClockHz * kAccessWindowSeconds);
    uint64_t       last      = deck.GetLastAccessCycle();
    bool           isRecent  = deck.HasBeenAccessed() && nowCycle >= last && nowCycle - last <= window;



    return isPreferenceOn && isMoving && isRecent;
}
