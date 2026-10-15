#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream
//
//  IByteStream over Winsock, one connection at a time.
//
//  Listening, the stream accepts a client on a thread of its own, and the
//  next one after that one disconnects; writes with no client connected are
//  dropped, as bytes sent down a serial line with nothing on the other end
//  are. Connecting, the stream closes when the other end does.
//
//  A thread receives into a queue that Read takes from, so a read waits on
//  the queue, never on the socket, and Close ends both threads at once.
//
////////////////////////////////////////////////////////////////////////////////

class Win32TcpStream : public IByteStream
{
public:
    ~Win32TcpStream() override;

    HRESULT  Listen  (const char * bindAddress, uint16_t port);
    HRESULT  Connect (const std::string & host, uint16_t port);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override;
    void     Close            () override;

private:
    HRESULT  StartWinsock ();
    void     RunListener  ();
    void     RunReceiver  (SOCKET client);
    void     DropClient   ();

    static constexpr size_t  s_kReceiveChunk = 4096;

    bool                     m_winsock   = false;
    SOCKET                   m_listener  = INVALID_SOCKET;
    SOCKET                   m_client    = INVALID_SOCKET;
    std::thread              m_thread;
    mutable std::mutex       m_lock;
    std::condition_variable  m_arrived;
    std::deque<Byte>         m_received;
    bool                     m_closed    = false;
};
