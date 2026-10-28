#pragma once

#include "Pch.h"

struct SerialLineFormat;





////////////////////////////////////////////////////////////////////////////////
//
//  IAciaEndpoint
//
//  Whatever is plugged into a 6551's serial connector: a host file, a
//  loopback plug, a PC COM port, a TCP connection or a modem.
//
//  The 6551 calls these on the emulation thread. An endpoint that forwards
//  to another thread copies what it is passed and returns at once.
//
//  A byte source sends its bytes through Acia6551::EnqueueReceivedBytes, safe
//  from any thread. A host link (IsHostLink) is polled by the chip on the
//  emulation thread while the machine is live, and sends what it brings in
//  through the chip's host-input methods (Acia6551::ReceiveFromHost,
//  SetHostLines, NoteHostConnection), which journal it. A host link treats
//  OnModemControlChanged, OnBreakChanged and OnFormatChanged as the state to
//  apply on its next poll, so a replay, which is never polled, changes
//  nothing on the PC.
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

    // The baud rate, word length, parity or stop bits changed; also called
    // once when the endpoint is attached.
    virtual void OnFormatChanged (const SerialLineFormat & format)
    {
        (void) format;
    }

    // A plug that returns what is sent to the chip itself. Its bytes stay
    // in the machine, so a muted chip passes them to it.
    virtual bool IsLoopback () const
    {
        return false;
    }

    // An attachment that brings input from outside the machine.
    virtual bool IsHostLink () const
    {
        return false;
    }

    // Emulation thread, live only: take what the PC has for the chip and
    // apply the chip's lines and format to the PC. `cycles` is the chip's
    // count of emulated cycles. True when anything was taken or sent, which
    // shortens the interval to the next poll.
    virtual bool Poll (uint64_t cycles)
    {
        (void) cycles;
        return false;
    }

    // The machine is live again at a point in the past: close any
    // connection, and set the chip's lines and receive queue directly to
    // what a dropped line produces.
    virtual void DropConnection ()
    {
    }
};
