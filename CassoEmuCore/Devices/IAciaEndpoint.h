#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IAciaEndpoint
//
//  Sink for bytes the 6551 ACIA transmits to the outside world. Concrete
//  endpoints route those bytes somewhere real: a host file (serial printing),
//  a loopback that feeds them straight back into the ACIA's receiver (comms
//  self-test), or, in a later spec, a physical COM port or TCP socket.
//
//  Receiving is push-driven the other way: an endpoint calls
//  Acia6551::ReceiveByte to hand an incoming byte to the guest. The loopback
//  endpoint is the only v1 source; file endpoints are transmit-only.
//
//  Endpoints are caller-owned; the ACIA never deletes its endpoint.
//
////////////////////////////////////////////////////////////////////////////////

class IAciaEndpoint
{
public:
    virtual ~IAciaEndpoint () = default;

    // The ACIA finished transmitting one byte.
    virtual void OnByteTransmitted (Byte value) = 0;

    // An endpoint that shifts each byte out at the line's speed clears the
    // transmitter-empty flag itself, and calls
    // Acia6551::SetTransmitterEmpty once the byte has left. Every other
    // endpoint takes a byte at once and the flag never clears.
    virtual bool PacesTransmit () const { return false; }
};
