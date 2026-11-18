#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream
//
//  IByteStream over UDP datagrams to one address and port: an original
//  Uthernet's or a LANceGS's network shim, which has no TCP. Each write is
//  sent as datagrams of at most kDatagramMax bytes, a few milliseconds
//  apart, so the card's receive memory doesn't overflow while the Apple
//  copies one out. Each datagram received from that address and port is
//  appended to the bytes Read takes, in order; one lost on the network is
//  a run of lost bytes, as on a serial line.
//
//  Open sends an empty datagram, and the shim sends to its source address
//  and port from then on.
//
////////////////////////////////////////////////////////////////////////////////

class Win32UdpStream : public IByteStream
{
public:
    ~Win32UdpStream() override;

    static constexpr size_t  kDatagramMax = 1024;

    HRESULT  Open (const std::string & host, uint16_t port);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override;
    void     Close            () override;

private:
    HRESULT  StartWinsock ();
    void     RunReceiver  (SOCKET receiving);

    static constexpr DWORD   s_kPaceMs       = 4;

    //  SIO_UDP_CONNRESET, from mstcpip.h.
    static constexpr DWORD   s_kUdpConnReset = _WSAIOW (IOC_VENDOR, 12);

    bool                     m_winsock = false;
    SOCKET                   m_socket  = INVALID_SOCKET;
    std::thread              m_thread;
    mutable std::mutex       m_lock;
    std::condition_variable  m_arrived;
    std::deque<Byte>         m_received;
    bool                     m_closed  = false;
};
