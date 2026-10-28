#include "Pch.h"

#include "Seams/Win32HostSerialPort.h"
#include "Devices/Serial/SerialLineFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kBaudBits
//
//  COMMPROP's settable-rate bits and the rate each is. BAUD_134_5 is passed
//  to the driver as 134, the integer the DCB takes.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr std::pair<DWORD, uint32_t>  s_kBaudBits[] =
{
    { BAUD_075,        75 },
    { BAUD_110,       110 },
    { BAUD_134_5,     134 },
    { BAUD_150,       150 },
    { BAUD_300,       300 },
    { BAUD_600,       600 },
    { BAUD_1200,     1200 },
    { BAUD_1800,     1800 },
    { BAUD_2400,     2400 },
    { BAUD_4800,     4800 },
    { BAUD_7200,     7200 },
    { BAUD_9600,     9600 },
    { BAUD_14400,   14400 },
    { BAUD_19200,   19200 },
    { BAUD_38400,   38400 },
    { BAUD_56K,     56000 },
    { BAUD_57600,   57600 },
    { BAUD_115200, 115200 },
    { BAUD_128K,   128000 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::~Win32HostSerialPort
//
////////////////////////////////////////////////////////////////////////////////

Win32HostSerialPort::~Win32HostSerialPort()
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::Prepare
//
//  A read interval of MAXDWORD with no total timeout makes ReadFile return
//  at once with what has arrived. A write waits at most kWriteTimeoutMs; at
//  the 6551's pace the driver's buffer does not fill.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::Prepare (HANDLE handle)
{
    constexpr DWORD  kBufferBytes    = 4096;
    constexpr DWORD  kWriteTimeoutMs = 50;
    HRESULT          hr              = S_OK;
    BOOL             isDone          = FALSE;
    COMMTIMEOUTS     timeouts        = {};



    isDone = SetupComm (handle, kBufferBytes, kBufferBytes);
    CWR (isDone);

    timeouts.ReadIntervalTimeout        = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant  = kWriteTimeoutMs;

    isDone = SetCommTimeouts (handle, &timeouts);
    CWR (isDone);

    isDone = PurgeComm (handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    CWR (isDone);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::GetBaudRates
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::GetBaudRates (
    std::vector<uint32_t>  & outRates,
    bool                   & outIsAnyRate)
{
    HRESULT   hr     = S_OK;
    COMMPROP  props  = {};
    BOOL      isDone = FALSE;



    outRates.clear();

    isDone = GetCommProperties (m_handle, &props);
    CWR (isDone);

    for (const auto & [bit, rate] : s_kBaudBits)
    {
        if (props.dwSettableBaud & bit)
        {
            outRates.push_back (rate);
        }
    }

    outIsAnyRate = (props.dwSettableBaud & BAUD_USER) != 0;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::Configure
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::Configure (const SerialLineFormat & format)
{
    static constexpr BYTE  kParities[] = { NOPARITY, ODDPARITY, EVENPARITY, MARKPARITY, SPACEPARITY };
    static constexpr BYTE  kStopBits[] = { ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS };
    HRESULT                hr          = S_OK;
    DCB                    dcb         = {};
    BOOL                   isDone      = FALSE;



    dcb.DCBlength = sizeof (dcb);

    isDone = GetCommState (m_handle, &dcb);
    CWR (isDone);

    dcb.BaudRate        = (DWORD) format.baud;
    dcb.ByteSize        = (BYTE) format.wordBits;
    dcb.Parity          = kParities[(size_t) format.parity];
    dcb.StopBits        = kStopBits[(size_t) format.stopBits];
    dcb.fBinary         = TRUE;
    dcb.fParity         = format.parity != SerialParity::None;
    dcb.fOutxCtsFlow    = FALSE;
    dcb.fOutxDsrFlow    = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX           = FALSE;
    dcb.fInX            = FALSE;
    dcb.fErrorChar      = FALSE;
    dcb.fNull           = FALSE;
    dcb.fAbortOnError   = FALSE;
    dcb.fDtrControl     = m_dtr ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    dcb.fRtsControl     = m_rts ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;

    isDone = SetCommState (m_handle, &dcb);
    CWR (isDone);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::SetControl
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::SetControl (
    bool  dtr,
    bool  rts)
{
    HRESULT  hr     = S_OK;
    BOOL     isDone = FALSE;



    isDone = EscapeCommFunction (m_handle, dtr ? SETDTR : CLRDTR);
    CWR (isDone);

    isDone = EscapeCommFunction (m_handle, rts ? SETRTS : CLRRTS);
    CWR (isDone);

    m_dtr = dtr;
    m_rts = rts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::SetBreak
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::SetBreak (bool isOn)
{
    HRESULT  hr     = S_OK;
    BOOL     isDone = FALSE;



    isDone = isOn ? SetCommBreak (m_handle) : ClearCommBreak (m_handle);
    CWR (isDone);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::Write (
    std::span<const Byte>   bytes,
    size_t                & outWritten)
{
    HRESULT  hr      = S_OK;
    DWORD    written = 0;
    BOOL     isDone  = FALSE;



    outWritten = 0;

    isDone = WriteFile (m_handle, bytes.data(), (DWORD) bytes.size(), &written, nullptr);
    CWR (isDone);

    outWritten = written;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::Read
//
//  ClearCommError reports, and clears, the errors latched since the last
//  call and the count of bytes waiting; only those are read, so the call
//  never waits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::Read (
    std::span<Byte>      buffer,
    size_t             & outRead,
    SerialPortErrors   & outErrors)
{
    HRESULT  hr      = S_OK;
    DWORD    errors  = 0;
    COMSTAT  status  = {};
    DWORD    wanted  = 0;
    DWORD    read    = 0;
    BOOL     isDone  = FALSE;



    outRead   = 0;
    outErrors = {};

    isDone = ClearCommError (m_handle, &errors, &status);
    CWR (isDone);

    outErrors.isFraming = (errors & CE_FRAME)                   != 0;
    outErrors.isParity  = (errors & CE_RXPARITY)                != 0;
    outErrors.isOverrun = (errors & (CE_OVERRUN | CE_RXOVER))   != 0;
    outErrors.isBreak   = (errors & CE_BREAK)                   != 0;

    wanted = std::min ((DWORD) buffer.size(), status.cbInQue);
    BAIL_OUT_IF (wanted == 0, S_OK);

    isDone = ReadFile (m_handle, buffer.data(), wanted, &read, nullptr);
    CWR (isDone);

    outRead = read;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort::GetLines
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSerialPort::GetLines (SerialPortLines & outLines)
{
    HRESULT  hr     = S_OK;
    DWORD    status = 0;
    BOOL     isDone = FALSE;



    isDone = GetCommModemStatus (m_handle, &status);
    CWR (isDone);

    outLines.cts = (status & MS_CTS_ON)  != 0;
    outLines.dsr = (status & MS_DSR_ON)  != 0;
    outLines.dcd = (status & MS_RLSD_ON) != 0;

Error:
    return hr;
}
