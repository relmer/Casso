#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostEndpoint
//
//  An IPv4 address and port on the PC's side of the emulated network card,
//  both in host byte order.
//
////////////////////////////////////////////////////////////////////////////////

struct HostEndpoint
{
    uint32_t  ipv4 = 0;
    uint16_t  port = 0;

    bool operator== (const HostEndpoint & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostSocketKind
//
////////////////////////////////////////////////////////////////////////////////

enum class HostSocketKind
{
    Tcp,
    Udp,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostSocketReadiness
//
//  The result of one readiness poll. `readable` covers pending data, a
//  pending accept and an orderly close; `writable` covers a finished connect
//  and free send space; `failed` covers a connect that ended in an error,
//  such as no listener at the remote port, and any later socket error.
//
////////////////////////////////////////////////////////////////////////////////

struct HostSocketReadiness
{
    bool  readable = false;
    bool  writable = false;
    bool  failed   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostSocket
//
//  One non-blocking socket on the PC. No call waits: an operation that would
//  block succeeds having done nothing and reports that through its
//  out-parameters, so the emulation thread can poll every socket it owns
//  between instructions. Destroying the object closes the socket.
//
////////////////////////////////////////////////////////////////////////////////

class IHostSocket
{
public:

    virtual ~IHostSocket () = default;

    virtual HRESULT  Bind         (const HostEndpoint & local, bool allowBroadcast)        = 0;
    virtual HRESULT  GetLocal     (HostEndpoint & outLocal) const                          = 0;
    virtual HRESULT  Listen       ()                                                       = 0;
    virtual HRESULT  Accept       (std::unique_ptr<IHostSocket>  & outAccepted,
                                   HostEndpoint                  & outPeer,
                                   bool                          & outWouldBlock)          = 0;

    //  Starts a connect; Poll reports writable when it completes and failed
    //  when it ends in an error.
    virtual HRESULT  Connect      (const HostEndpoint & remote)                            = 0;

    //  `outSent` may be less than the span, including zero when the send
    //  buffer is full.
    virtual HRESULT  Send         (std::span<const Byte> bytes, size_t & outSent)          = 0;

    //  Zero bytes with `outEof` false means nothing is pending.
    virtual HRESULT  Receive      (std::span<Byte>  buffer,
                                   size_t         & outReceived,
                                   bool           & outEof)                                = 0;

    virtual HRESULT  SendTo       (const HostEndpoint & remote, std::span<const Byte> bytes) = 0;

    //  Zero bytes means nothing is pending.
    virtual HRESULT  ReceiveFrom  (std::span<Byte>  buffer,
                                   size_t         & outReceived,
                                   HostEndpoint   & outFrom)                               = 0;

    virtual HRESULT  ShutdownSend ()                                                       = 0;
    virtual HRESULT  Poll         (HostSocketReadiness & outReadiness)                     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostNetwork
//
//  The PC's network stack, as the emulated network card uses it: sockets,
//  and the address of the PC's DNS server for the virtual network's DNS
//  relay. Unit tests substitute a fake in which the test acts as the remote
//  peer.
//
////////////////////////////////////////////////////////////////////////////////

class IHostNetwork
{
public:

    virtual ~IHostNetwork () = default;

    virtual HRESULT  CreateSocket (HostSocketKind kind, std::unique_ptr<IHostSocket> & outSocket) = 0;

    //  E_NOT_SET when the PC has no IPv4 DNS server.
    virtual HRESULT  GetDnsServer (uint32_t & outIpv4)                                           = 0;
};
