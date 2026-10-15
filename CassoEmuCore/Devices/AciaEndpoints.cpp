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
        m_acia->ReceiveByte (value);
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
//  With the shift register idle, the byte moves into it at once and the data
//  register is empty again; with it busy, the byte remains in the register.
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::OnByteTransmitted (Byte value)
{
    if (m_shifting)
    {
        m_tdr     = value;
        m_tdrFull = true;
    }
    else
    {
        m_shifting  = true;
        m_shiftByte = value;
        m_txWait    = GetCharacterCycles();

        if (m_acia != nullptr)
        {
            m_acia->SetTransmitterEmpty();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::Tick
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::Tick (uint32_t cpuCycles)
{
    m_cycle += cpuCycles;

    if (m_shifting)
    {
        m_txWait -= cpuCycles;

        if (m_txWait <= 0)
        {
            FinishShifting();
        }
    }

    m_rxWait -= cpuCycles;

    if (m_rxWait <= 0)
    {
        DeliverReceived();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::GetBaudRate
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AciaBridgeEndpoint::GetBaudRate (Byte control)
{
    return s_kBaudRates[control & Acia6551::kControlBaudMask];
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::GetCyclesPerCharacter
//
//  Two stop bits for a set stop-bit flag; the 6551's one-and-a-half for a
//  five-bit word with no parity is counted as two.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AciaBridgeEndpoint::GetCyclesPerCharacter (Byte control, Byte command)
{
    uint32_t  dataBits = 8u - (uint32_t) ((control & Acia6551::kControlWordLenMask) >> Acia6551::kControlWordLenShift);
    uint32_t  parity   = (command & Acia6551::kCommandParityEnable) ? 1u : 0u;
    uint32_t  stopBits = (control & Acia6551::kControlStopBits) ? 2u : 1u;
    uint32_t  bits     = 1u + dataBits + parity + stopBits;



    return (uint32_t) (((uint64_t) kCpuHz * bits + GetBaudRate (control) - 1) / GetBaudRate (control));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::DeliverReceived
//
//  With nothing waiting on the stream the line is idle, and it looks again
//  one character time later, which is as soon as a byte sent now could have
//  arrived.
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::DeliverReceived()
{
    HRESULT  hr    = S_OK;
    Byte     value = 0;
    size_t   got   = 0;



    m_rxWait = std::max<int64_t> (m_rxWait, 0) + GetCharacterCycles();

    BAIL_OUT_IF (m_stream == nullptr || m_acia == nullptr, S_OK);
    BAIL_OUT_IF (m_stream->GetReadableCount() == 0, S_OK);

    hr = m_stream->Read (&value, 1, 0, got);
    CHR (hr);
    BAIL_OUT_IF (got == 0, S_OK);

    m_acia->ReceiveByte (value);

    if (m_log)
    {
        m_log (false, value, m_cycle);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::FinishShifting
//
//  A byte that cannot be written, with nothing at the other end, is dropped,
//  as with an unplugged cable.
//
////////////////////////////////////////////////////////////////////////////////

void AciaBridgeEndpoint::FinishShifting()
{
    HRESULT  hr = S_OK;



    if (m_stream != nullptr)
    {
        hr = m_stream->Write (&m_shiftByte, 1);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_log)
    {
        m_log (true, m_shiftByte, m_cycle);
    }

    m_shifting = false;

    if (m_tdrFull)
    {
        m_tdrFull = false;
        OnByteTransmitted (m_tdr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint::GetCharacterCycles
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AciaBridgeEndpoint::GetCharacterCycles() const
{
    return (m_acia != nullptr) ? GetCyclesPerCharacter (m_acia->GetControl(), m_acia->GetCommand()) : 1u;
}