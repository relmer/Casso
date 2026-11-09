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
//  A write blocks while the socket's send buffer is full, unless the stream
//  is set to drop: an emulated serial port writes on the CPU thread, and a
//  client that stops reading must not stop the emulator, any more than a
//  device that stops reading stops a real serial line's sender.
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
    HRESULT  Connect (const std::string & host, uint16_t port, std::stop_token stop = {});

    void     SetDropWhenFull (bool drop) { m_dropWhenFull = drop; }

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

    //  How long a connect waits for an answer, and how often it checks for
    //  a stop while it does.
    static constexpr DWORD   s_kConnectTimeoutMs = 20000;
    static constexpr int     s_kConnectSliceMs   = 100;

    bool                     m_winsock      = false;
    SOCKET                   m_listener     = INVALID_SOCKET;
    SOCKET                   m_client       = INVALID_SOCKET;
    std::thread              m_thread;
    mutable std::mutex       m_lock;
    std::condition_variable  m_arrived;
    std::deque<Byte>         m_received;
    bool                     m_closed       = false;
    bool                     m_dropWhenFull = false;
};
