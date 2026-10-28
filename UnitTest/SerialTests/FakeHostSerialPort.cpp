#include "Pch.h"

#include "FakeHostSerialPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::FakeHostSerialPort
//
////////////////////////////////////////////////////////////////////////////////

FakeHostSerialPort::FakeHostSerialPort (std::shared_ptr<FakeSerialPortState> state)
    : m_state (std::move (state))
{
    m_state->isOpen = true;
    m_state->opens++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::~FakeHostSerialPort
//
////////////////////////////////////////////////////////////////////////////////

FakeHostSerialPort::~FakeHostSerialPort()
{
    m_state->isOpen = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::CheckPlugged
//
//  An unplugged USB adapter fails every call as Windows fails it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::CheckPlugged() const
{
    return m_state->isUnplugged ? HRESULT_FROM_WIN32 (ERROR_BAD_COMMAND) : S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::GetBaudRates
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::GetBaudRates (
    std::vector<uint32_t>  & outRates,
    bool                   & outIsAnyRate)
{
    outRates     = m_state->rates;
    outIsAnyRate = m_state->isAnyRate;

    return CheckPlugged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::Configure
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::Configure (const SerialLineFormat & format)
{
    HRESULT  hr = CheckPlugged();



    CHR (hr);

    m_state->formats.push_back (format);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::SetControl
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::SetControl (
    bool  dtr,
    bool  rts)
{
    HRESULT  hr = CheckPlugged();



    CHR (hr);

    m_state->dtr = dtr;
    m_state->rts = rts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::SetBreak
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::SetBreak (bool isOn)
{
    HRESULT  hr = CheckPlugged();



    CHR (hr);

    m_state->isBreak = isOn;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::Write (
    std::span<const Byte>   bytes,
    size_t                & outWritten)
{
    HRESULT  hr = CheckPlugged();



    outWritten = 0;
    CHR (hr);

    m_state->written.insert (m_state->written.end(), bytes.begin(), bytes.end());
    outWritten = bytes.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::Read (
    std::span<Byte>      buffer,
    size_t             & outRead,
    SerialPortErrors   & outErrors)
{
    HRESULT  hr    = CheckPlugged();
    size_t   count = 0;



    outRead   = 0;
    outErrors = {};
    CHR (hr);

    count = std::min (buffer.size(), m_state->inbound.size());
    std::copy_n (m_state->inbound.begin(), count, buffer.begin());
    m_state->inbound.erase (m_state->inbound.begin(), m_state->inbound.begin() + count);

    outRead          = count;
    outErrors        = m_state->errors;
    m_state->errors  = {};

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort::GetLines
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPort::GetLines (SerialPortLines & outLines)
{
    outLines = m_state->lines;

    return CheckPlugged();
}
