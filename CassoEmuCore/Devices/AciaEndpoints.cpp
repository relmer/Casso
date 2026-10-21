#include "Pch.h"

#include "AciaEndpoints.h"
#include "Acia6551.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AciaLoopbackEndpoint::OnByteTransmitted
//
////////////////////////////////////////////////////////////////////////////////

void AciaLoopbackEndpoint::OnByteTransmitted (Byte value)
{
    if (m_acia != nullptr)
    {
        m_acia->EnqueueReceivedBytes (&value, 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaLoopbackEndpoint::OnModemControlChanged
//
//  A loopback plug ties RTS to CTS and DTR to DSR and DCD.
//
////////////////////////////////////////////////////////////////////////////////

void AciaLoopbackEndpoint::OnModemControlChanged (bool dtr, bool rts)
{
    if (m_acia != nullptr)
    {
        m_acia->SetModemInputs (rts, dtr, dtr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaFileEndpoint::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AciaFileEndpoint::Open (const string & path)
{
    HRESULT   hr     = S_OK;
    bool      opened = false;



    m_file.open (path, ios::binary | ios::out | ios::app);

    opened = m_file.is_open();
    CBR (opened);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaFileEndpoint::Close
//
////////////////////////////////////////////////////////////////////////////////

void AciaFileEndpoint::Close()
{
    if (m_file.is_open())
    {
        m_file.close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaFileEndpoint::OnByteTransmitted
//
////////////////////////////////////////////////////////////////////////////////

void AciaFileEndpoint::OnByteTransmitted (Byte value)
{
    if (m_file.is_open())
    {
        m_file.put (static_cast<char> (value));
        m_file.flush();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::OnByteTransmitted
//
//  The ACIA passes each byte as its shift ends. A byte that cannot be
//  written, with nothing at the other end, is dropped, as with an unplugged
//  cable.
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::OnByteTransmitted (Byte value)
{
    HRESULT  hr = S_OK;



    if (m_stream != nullptr)
    {
        hr = m_stream->Write (&value, 1);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_log)
    {
        m_log (true, value, m_cycle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::Tick
//
//  Whatever the stream has received goes to the ACIA's receive queue, which
//  lands it one character time apart. The stream is checked once per
//  character time, since no byte can land sooner and a COM port's check is a
//  call into the system. The queue is topped up to kQueueCap, so a backlog
//  stays in the stream's buffer rather than the ACIA's, as on a real line.
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::Tick (uint32_t cpuCycles)
{
    HRESULT  hr         = S_OK;
    Byte     buffer[64] = {};
    size_t   readable   = 0;
    size_t   got        = 0;
    size_t   i          = 0;



    m_cycle    += cpuCycles;
    m_pollWait -= cpuCycles;

    BAIL_OUT_IF (m_stream == nullptr || m_acia == nullptr, S_OK);
    BAIL_OUT_IF (m_pollWait > 0, S_OK);

    m_pollWait = (std::max) ((int64_t) std::ceil (m_acia->GetTransmitCharacterCycles()), (int64_t) 1);
    readable   = (std::min) (m_stream->GetReadableCount(), GetQueueRoom());

    while (readable > 0)
    {
        hr = m_stream->Read (buffer, (std::min) (readable, sizeof (buffer)), 0, got);
        CHR (hr);
        BAIL_OUT_IF (got == 0, S_OK);

        m_acia->EnqueueReceivedBytes (buffer, got);

        for (i = 0; m_log && i < got; i++)
        {
            m_log (false, buffer[i], m_cycle);
        }

        readable = (std::min) (m_stream->GetReadableCount(), GetQueueRoom());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::GetQueueRoom
//
////////////////////////////////////////////////////////////////////////////////

size_t AciaBridgeEndpoint::GetQueueRoom() const
{
    size_t  queued = m_acia->GetQueuedReceiveCount();



    return (queued < kQueueCap) ? kQueueCap - queued : 0;
}