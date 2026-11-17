#include "Pch.h"

#include "Seams/RawDevice/Win32PipeStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::Attach
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PipeStream::Attach (HANDLE pipe)
{
    HRESULT  hr = S_OK;



    Close();

    m_pipe = pipe;
    CBRAEx (pipe != INVALID_HANDLE_VALUE && pipe != nullptr, E_INVALIDARG);

    m_readEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    CWR (m_readEvent != nullptr);

    m_writeEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    CWR (m_writeEvent != nullptr);

    m_closed = false;

Error:
    if (FAILED (hr))
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::Write
//
//  Returns once the pipe has every byte, which for a write longer than the
//  pipe's buffer is once the other end has read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PipeStream::Write (const Byte * data, size_t count)
{
    HRESULT     hr         = S_OK;
    size_t      total      = 0;
    bool        valid      = data != nullptr || count == 0;



    CBRAEx (valid, E_INVALIDARG);
    CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    while (total < count)
    {
        OVERLAPPED  overlapped = {};
        DWORD       chunk      = (DWORD) std::min<size_t> (count - total, 0x100000);
        DWORD       written    = 0;
        DWORD       error      = ERROR_SUCCESS;
        BOOL        ok         = FALSE;

        overlapped.hEvent = m_writeEvent;

        ok = WriteFile (m_pipe, data + total, chunk, nullptr, &overlapped);

        if (!ok && GetLastError() == ERROR_IO_PENDING)
        {
            ok = GetOverlappedResult (m_pipe, &overlapped, &written, TRUE);
        }
        else if (ok)
        {
            ok = GetOverlappedResult (m_pipe, &overlapped, &written, FALSE);
        }

        error = ok ? ERROR_SUCCESS : GetLastError();

        if (!ok && IsBroken (error))
        {
            m_closed = true;
            error    = ERROR_BROKEN_PIPE;
        }

        CBREx (ok, HRESULT_FROM_WIN32 (error));

        total += written;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::Read
//
//  A read that is waiting at the timeout is canceled; bytes that arrived as
//  it was canceled are returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PipeStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
{
    HRESULT     hr         = S_OK;
    OVERLAPPED  overlapped = {};
    DWORD       toRead     = (DWORD) std::min<size_t> (capacity, 0x100000);
    DWORD       got        = 0;
    DWORD       error      = ERROR_SUCCESS;
    BOOL        ok         = FALSE;
    bool        valid      = buffer != nullptr || capacity == 0;



    outRead = 0;

    CBRAEx (valid, E_INVALIDARG);
    BAIL_OUT_IF (m_closed || toRead == 0, S_OK);

    overlapped.hEvent = m_readEvent;

    ok = ReadFile (m_pipe, buffer, toRead, nullptr, &overlapped);

    if (!ok && GetLastError() == ERROR_IO_PENDING)
    {
        if (WaitForSingleObject (m_readEvent, timeoutMs) != WAIT_OBJECT_0)
        {
            CancelIoEx (m_pipe, &overlapped);
        }

        ok = GetOverlappedResult (m_pipe, &overlapped, &got, TRUE);
    }
    else if (ok)
    {
        ok = GetOverlappedResult (m_pipe, &overlapped, &got, FALSE);
    }

    error = ok ? ERROR_SUCCESS : GetLastError();

    if (!ok && IsBroken (error))
    {
        m_closed = true;
    }

    //  A canceled read and a closed pipe both return no bytes, successfully.
    ok = ok || error == ERROR_OPERATION_ABORTED || IsBroken (error);
    CBREx (ok, HRESULT_FROM_WIN32 (error));

    outRead = got;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::GetReadableCount
//
////////////////////////////////////////////////////////////////////////////////

size_t Win32PipeStream::GetReadableCount()
{
    DWORD  available = 0;



    if (m_closed && m_pipe == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    if (!PeekNamedPipe (m_pipe, nullptr, 0, nullptr, &available, nullptr))
    {
        m_closed  = true;
        available = 0;
    }

    return available;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::Close
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeStream::Close()
{
    m_closed = true;

    if (m_pipe != INVALID_HANDLE_VALUE && m_pipe != nullptr)
    {
        CloseHandle (m_pipe);
    }

    if (m_readEvent != nullptr)
    {
        CloseHandle (m_readEvent);
    }

    if (m_writeEvent != nullptr)
    {
        CloseHandle (m_writeEvent);
    }

    m_pipe       = INVALID_HANDLE_VALUE;
    m_readEvent  = nullptr;
    m_writeEvent = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream::IsBroken
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeStream::IsBroken (DWORD error)
{
    return error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_HANDLE_EOF;
}
