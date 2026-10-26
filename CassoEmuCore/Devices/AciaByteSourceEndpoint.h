#pragma once

#include "Pch.h"

#include "Devices/IAciaEndpoint.h"

class Acia6551;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaByteSourceEndpoint
//
//  A fixed run of bytes sent into a 6551, as if typed at the far end of the
//  cable: the contents of a host file, read by the caller. Start queues them
//  all at once, and the 6551 delivers them one character time apart.
//  Transmitted bytes are discarded.
//
//  Reading the file is the caller's job, so the endpoint itself touches no
//  file system and a test can supply the bytes directly.
//
////////////////////////////////////////////////////////////////////////////////

class AciaByteSourceEndpoint : public IAciaEndpoint
{
public:
    AciaByteSourceEndpoint (Acia6551 * acia, vector<Byte> bytes);

    // Queues the bytes. Call after Acia6551::SetEndpoint, which empties the
    // queue.
    void    Start ();

    void    OnByteTransmitted (Byte value) override;

private:
    Acia6551 *      m_acia = nullptr;
    vector<Byte>    m_bytes;
};
