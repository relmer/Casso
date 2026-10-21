#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IAciaEndpoint
//
//  Whatever is plugged into a 6551's serial connector: a host file, a
//  loopback plug, or, from the Apple II link, a socket or a COM port.
//
//  The 6551 calls these on the emulation thread. An endpoint that forwards
//  to another thread copies what it is passed and returns at once.
//
//  Bytes travel the other way through Acia6551::EnqueueReceivedBytes, and
//  the modem inputs through Acia6551::SetModemInputs; both are safe from any
//  thread.
//
//  Endpoints are caller-owned; the ACIA never deletes its endpoint.
//
////////////////////////////////////////////////////////////////////////////////

class IAciaEndpoint
{
public:
    virtual ~IAciaEndpoint () = default;

    // A character finished shifting out.
    virtual void OnByteTransmitted (Byte value) = 0;

    // DTR or RTS changed; also called once when the endpoint is attached.
    virtual void OnModemControlChanged (bool dtr, bool rts)
    {
        (void) dtr;
        (void) rts;
    }

    // The transmitter started or stopped sending break.
    virtual void OnBreakChanged (bool active)
    {
        (void) active;
    }
};
