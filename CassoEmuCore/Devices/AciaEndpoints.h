#pragma once

#include "Pch.h"

#include "IAciaEndpoint.h"

class Acia6551;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaLoopbackEndpoint
//
//  A loopback plug: every transmitted byte is queued back to the ACIA's
//  receiver, RTS is wired to CTS, and DTR to DSR and DCD. Used for comms
//  self-test: the guest reads what it sends. The ACIA is caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaLoopbackEndpoint : public IAciaEndpoint
{
public:
    explicit    AciaLoopbackEndpoint (Acia6551 * acia) : m_acia (acia) {}

    void        OnByteTransmitted     (Byte value) override;
    void        OnModemControlChanged (bool dtr, bool rts) override;

private:
    Acia6551 *    m_acia = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AciaFileEndpoint
//
//  Appends every transmitted byte to a host file — the v1 serial-printing sink
//  (spec 015 routes guest print output here). Transmit-only.
//
////////////////////////////////////////////////////////////////////////////////

class AciaFileEndpoint : public IAciaEndpoint
{
public:
    HRESULT     Open   (const string & path);
    void        Close  ();
    bool        IsOpen () const { return m_file.is_open (); }

    void        OnByteTransmitted (Byte value) override;

private:
    ofstream    m_file;
};
