#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket
//
//  IHostSocket over a Winsock SOCKET placed in non-blocking mode. A call that
//  would block returns S_OK with nothing done; readiness comes from WSAPoll
//  with a zero timeout.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostSocket : public IHostSocket
{
public:

    explicit Win32HostSocket (SOCKET handle);
    ~Win32HostSocket () override;

    static HRESULT  Create (HostSocketKind kind, std::unique_ptr<IHostSocket> & outSocket);

    HRESULT  Bind         (const HostEndpoint & local, bool allowBroadcast)          override;
    HRESULT  GetLocal     (HostEndpoint & outLocal) const                            override;
    HRESULT  Listen       ()                                                         override;
    HRESULT  Accept       (std::unique_ptr<IHostSocket>  & outAccepted,
                           HostEndpoint                  & outPeer,
                           bool                          & outWouldBlock)            override;
    HRESULT  Connect      (const HostEndpoint & remote)                              override;
    HRESULT  Send         (std::span<const Byte> bytes, size_t & outSent)            override;
    HRESULT  Receive      (std::span<Byte>  buffer,
                           size_t         & outReceived,
                           bool           & outEof)                                  override;
    HRESULT  SendTo       (const HostEndpoint & remote, std::span<const Byte> bytes) override;
    HRESULT  ReceiveFrom  (std::span<Byte>  buffer,
                           size_t         & outReceived,
                           HostEndpoint   & outFrom)                                 override;
    HRESULT  ShutdownSend ()                                                         override;
    HRESULT  Poll         (HostSocketReadiness & outReadiness)                       override;

private:

    static sockaddr_in   EndpointToAddress     (const HostEndpoint & endpoint);
    static HostEndpoint  AddressToEndpoint     (const sockaddr_in & address);
    static HRESULT       GetLastSocketError    ();
    static bool          IsWouldBlock          (int wsaError);
    static HRESULT       PrepareSocket         (SOCKET handle, HostSocketKind kind);

    SOCKET  m_handle = INVALID_SOCKET;
};
