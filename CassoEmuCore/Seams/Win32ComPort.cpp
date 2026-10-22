#include "Pch.h"

#include "Seams/Win32ComPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::~Win32ComPort
//
////////////////////////////////////////////////////////////////////////////////

Win32ComPort::~Win32ComPort()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::Open
//
//  Opening a port that another program has open fails here with sharing
//  denied, and the search for a link moves on to the next port.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ComPort::Open (const std::wstring & port, const ComSettings & settings)
{
    HRESULT       hr       = S_OK;
    std::wstring  device   = L"\\\\.\\" + port;
    DCB           dcb      = {};
    BOOL          result   = FALSE;



    Close();

    m_handle = CreateFileW (device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (m_handle != INVALID_HANDLE_VALUE);

    ByteStreamSettings::ToDcb (settings, dcb);

    result = SetCommState (m_handle, &dcb);
    CWR (result);

    hr = SetReadTimeout (0);
    CHR (hr);

    result = PurgeComm (m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    CWR (result);

Error:
    if (FAILED (hr))
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ComPort::Write (const Byte * data, size_t count)
{
    HRESULT  hr      = S_OK;
    DWORD    written = 0;
    BOOL     result  = FALSE;
    bool     closed  = IsClosed();



    CBRAEx (data != nullptr || count == 0, E_INVALIDARG);
    CBREx (!closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    result = WriteFile (m_handle, data, (DWORD) count, &written, nullptr);
    CWR (result);
    CBREx (written == count, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

Error:
    if (FAILED (hr) && hr != E_INVALIDARG)
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::Read
//
//  With the interval and the total multiplier at their maximum, a read
//  returns as soon as one byte has arrived, or after the constant with none.
//  Whatever else is queued is taken in a second read with no wait.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ComPort::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
{
    HRESULT    hr       = S_OK;
    DWORD      got      = 0;
    DWORD      more     = 0;
    size_t     queued   = 0;
    BOOL       result   = FALSE;
    bool       closed   = IsClosed();
    ULONGLONG  deadline = GetTickCount64() + timeoutMs;
    ULONGLONG  now      = 0;



    outRead = 0;

    CBRAEx (buffer != nullptr || capacity == 0, E_INVALIDARG);
    CBREx (!closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));
    BAIL_OUT_IF (capacity == 0, S_OK);

    do
    {
        now = GetTickCount64();

        hr = SetReadTimeout ((DWORD) std::min<ULONGLONG> (s_kReadSliceMs, (deadline > now) ? deadline - now : 0));
        CHR (hr);

        result = ReadFile (m_handle, buffer, 1, &got, nullptr);
        CWR (result);

        closed = IsClosed();
        CBREx (!closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));
    }
    while (got == 0 && GetTickCount64() < deadline);

    outRead = got;
    queued  = GetReadableCount();

    if (got != 0 && queued != 0 && capacity > 1)
    {
        hr = SetReadTimeout (0);
        CHR (hr);

        result = ReadFile (m_handle, buffer + 1, (DWORD) std::min (queued, capacity - 1), &more, nullptr);
        CWR (result);

        outRead += more;
    }

Error:
    if (FAILED (hr) && hr != E_INVALIDARG)
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::GetReadableCount
//
////////////////////////////////////////////////////////////////////////////////

size_t Win32ComPort::GetReadableCount()
{
    COMSTAT  status = {};
    DWORD    errors = 0;
    BOOL     result = FALSE;



    if (IsClosed())
    {
        return 0;
    }

    result = ClearCommError (m_handle, &errors, &status);

    return result ? (size_t) status.cbInQue : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::Close
//
////////////////////////////////////////////////////////////////////////////////

void Win32ComPort::Close()
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }

    m_readTimeout = MAXDWORD;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::SetReadTimeout
//
//  Zero returns at once with whatever is queued: the interval alone at its
//  maximum, with no multiplier or constant.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ComPort::SetReadTimeout (DWORD timeoutMs)
{
    HRESULT       hr       = S_OK;
    COMMTIMEOUTS  timeouts = {};
    BOOL          result   = FALSE;



    BAIL_OUT_IF (timeoutMs == m_readTimeout, S_OK);

    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = (timeoutMs == 0) ? 0 : MAXDWORD;
    timeouts.ReadTotalTimeoutConstant    = timeoutMs;
    timeouts.WriteTotalTimeoutConstant   = s_kWriteTimeoutMs;

    result = SetCommTimeouts (m_handle, &timeouts);
    CWR (result);

    m_readTimeout = timeoutMs;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort::TryGetPeerReady
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ComPort::TryGetPeerReady (bool & outReady)
{
    DWORD  lines = 0;
    BOOL   read  = m_handle != INVALID_HANDLE_VALUE && GetCommModemStatus (m_handle, &lines);



    outReady = read && (lines & MS_DSR_ON) != 0;

    return read != FALSE;
}
