#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeStream
//
//  One end of a pipe as a byte stream, for the two processes of
//  RawDeviceProtocol.
//
//  The pipe is opened for overlapped I/O, so a read ends at its timeout. A
//  pipe whose other end has closed is reported by `IsClosed` after the read
//  or write that found it so.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PipeStream : public IByteStream
{
public:
    Win32PipeStream () = default;
    ~Win32PipeStream () override { Close(); }

    Win32PipeStream (const Win32PipeStream &) = delete;
    Win32PipeStream & operator= (const Win32PipeStream &) = delete;

    //  `pipe` is closed by this stream from here on, also when Attach fails.
    //  It must have been opened with FILE_FLAG_OVERLAPPED.
    HRESULT  Attach           (HANDLE pipe);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override { return m_closed; }
    void     Close            () override;

private:
    static bool  IsBroken (DWORD error);

    HANDLE             m_pipe       = INVALID_HANDLE_VALUE;
    HANDLE             m_readEvent  = nullptr;
    HANDLE             m_writeEvent = nullptr;
    std::atomic<bool>  m_closed     { true };
};
