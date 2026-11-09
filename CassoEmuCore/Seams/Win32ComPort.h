#pragma once

#include "Pch.h"

#include "Seams/ByteStreamSettings.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ComPort
//
//  IByteStream over a COM port, opened exclusively. Reads and writes come
//  from one thread at a time, as the link's session and the emulator's bridge
//  each use a stream; the handle is not overlapped.
//
//  A port that fails a read or a write, as a USB adapter pulled out does, is
//  closed.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ComPort : public IByteStream
{
public:
    ~Win32ComPort() override;

    HRESULT  Open (const std::wstring & port, const ComSettings & settings);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override { return m_handle == INVALID_HANDLE_VALUE; }
    bool     TryGetPeerReady  (bool & outReady) override;
    void     Close            () override;

private:
    HRESULT  SetReadTimeout (DWORD timeoutMs);

    static constexpr DWORD  s_kWriteTimeoutMs = 2000;

    //  The longest timeout of one ReadFile. The handle is synchronous, so
    //  Windows runs its reads and writes one at a time: a write from another
    //  thread, such as a cancel, starts only when the read under way ends.
    //  A long read is made of these short ones.
    static constexpr DWORD  s_kReadSliceMs    = 100;

    HANDLE  m_handle      = INVALID_HANDLE_VALUE;
    DWORD   m_readTimeout = MAXDWORD;
    bool    m_handshake   = false;
};
